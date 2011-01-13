/**
 * qcRouter links several QuickChat & VypressChat nets through internet
 *
 *   This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*	qcrouter project
 *		localnet.c
 *			manages local vypress/quick chat net connections
 *
 *	(c) Saulius Menkevicius 2002,2003
 */

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "qcproto/qcs_link.h"

#include "common.h"
#include "msg.h"
#include "net.h"
#include "localconn.h"
#include "usercache.h"

#define MAIN_CHANNEL	"#Main"

#define QCROUTER_NICK	"qcRouter_msg"

#define VALID_USER(uid) (!is_null_user(uid))

/** structures
 *************************************/

struct local_net_data {
	msgq_id		delayed_queue;
	qcs_msg *	delayed_qmsg;

	qcs_link	link_id;
	unsigned short	next_user_id;

	timer_id	tm_refresh;
};

struct ref_cb_data {
	qcs_link link_id;
	const char * local_src;
};

struct qmsg_parse_entry {
	enum qcs_msgid msg;
	int (* proc)(qnet *, qnet_msg *, const qcs_msg *);
};

/** forward references
 */
static int handle_recv_chanlist_req(qnet *, qnet_msg *, const qcs_msg *);
static int handle_recv_refresh_ack(qnet *, qnet_msg *, const qcs_msg *);
static int handle_recv_refresh_req(qnet *, qnet_msg *, const qcs_msg *);
static int handle_recv_channel(qnet *, qnet_msg *, const qcs_msg *);
static int handle_recv_private(qnet *, qnet_msg *, const qcs_msg *);
static int handle_recv_message(qnet *, qnet_msg *, const qcs_msg *);
static int handle_recv_rename(qnet *, qnet_msg *, const qcs_msg *);
static int handle_recv_mode_change(qnet *, qnet_msg *, const qcs_msg *);
static int handle_recv_topic_change(qnet *, qnet_msg *, const qcs_msg *);
static int handle_recv_info_request(qnet *, qnet_msg *, const qcs_msg *);
static int handle_recv_beep(qnet *, qnet_msg *, const qcs_msg *);

static void handle_refresh_timeout(timer_id, int, void *);

/** static data
 ***************************************/
static struct qmsg_parse_entry entries[] = {
	{ QCS_MSG_REFRESH_REQUEST,	handle_recv_refresh_req },
	{ QCS_MSG_CHANLIST_REQUEST,	handle_recv_chanlist_req },
	{ QCS_MSG_REFRESH_ACK,		handle_recv_refresh_ack	},
	{ QCS_MSG_CHANNEL_BROADCAST,	handle_recv_channel	},
	{ QCS_MSG_CHANNEL_ME,		handle_recv_channel	},
	{ QCS_MSG_CHANNEL_JOIN,		handle_recv_channel	},
	{ QCS_MSG_CHANNEL_LEAVE,	handle_recv_channel	},
	{ QCS_MSG_PRIVATE_OPEN,		handle_recv_private	},
	{ QCS_MSG_PRIVATE_CLOSE,	handle_recv_private	},
	{ QCS_MSG_PRIVATE_TEXT,		handle_recv_private	},
	{ QCS_MSG_PRIVATE_ME,		handle_recv_private	},
	{ QCS_MSG_MESSAGE_SEND,		handle_recv_message	},
	{ QCS_MSG_MESSAGE_MASS,		handle_recv_message	},
	{ QCS_MSG_MESSAGE_ACK,		handle_recv_message	},
	{ QCS_MSG_RENAME,		handle_recv_rename	},
	{ QCS_MSG_MODE_CHANGE,		handle_recv_mode_change	},
	{ QCS_MSG_TOPIC_CHANGE,		handle_recv_topic_change},
	{ QCS_MSG_INFO_REQUEST,		handle_recv_info_request},
	{ QCS_MSG_BEEP_SEND,		handle_recv_beep	},
	{ QCS_MSG_BEEP_ACK,		handle_recv_beep	},
	{ QCS_MSG_INVALID,		NULL			}
};

static unsigned refresh_timeout_sec;

/** static routines
 *************************************/

#define NETCONN	((struct local_net_data*)net->conn)

/** umode_to_qcs:
 * 	translates umode
 */
static int umode_to_qcs(enum net_umode umode)
{
	switch(umode)
	{
	case UMODE_NORMAL:
		return QCS_UMODE_NORMAL;
	case UMODE_DND:
		return QCS_UMODE_DND;
	case UMODE_AWAY:
		return QCS_UMODE_AWAY;
	case UMODE_OFFLINE:
		return QCS_UMODE_OFFLINE;
	default:break;
	}
	return QCS_UMODE_INVALID;
}

static enum net_umode
	qcs_to_umode(int qcs_mode)
{
	/* ignore WATCH: we don't handle this thing */

	switch(qcs_mode & ~QCS_UMODE_WATCH)
	{
	case QCS_UMODE_NORMAL:
		return UMODE_NORMAL;
	case QCS_UMODE_AWAY:
		return UMODE_AWAY;
	case QCS_UMODE_DND:
		return UMODE_DND;
	case QCS_UMODE_OFFLINE:
		return UMODE_OFFLINE;
	default:break;
	}
	return UMODE_INVALID;
}

/** handle_recv_chanlist_req:
 * 	replies known chanlist
 */
static int handle_recv_chanlist_req(
	qnet * net, qnet_msg * nmsg,
	const qcs_msg * qmsg)
{
	qcs_msg * reply_qmsg = qcs_newmsg();
	chanlist_t chanlist;

	assert(net && nmsg && qmsg);

	usercache_known_channels(&chanlist);
	strcat(chanlist, "#");

	reply_qmsg->msg = QCS_MSG_CHANLIST_REPLY;
	qcs_msgset(reply_qmsg, QCS_DST, qmsg->src);
	qcs_msgset(reply_qmsg, QCS_CHAN, chanlist);

	qcs_send(NETCONN->link_id, reply_qmsg);

	qcs_deletemsg(reply_qmsg);

	return 0;	/* don't make NETMSG */
}

/** handle_recv_refresh_req:
 * 	sends acks as if we are users on the other nets
 * 	(i.e. masquerades refresh acks)
 *
 * 	refresh_req_cb(): callback for enumeration
 */
static void refresh_req_cb(
		void * data, const user_id * uid,
		enum net_umode umode,
		const char * nickname,
		const char * chanlist )
{
#define REF_DATA ((struct ref_cb_data *)data)

	qcs_msg * qmsg = qcs_newmsg();

	/* setup REFRESH_ACK msg */
	qmsg->msg = QCS_MSG_REFRESH_ACK;
	qcs_msgset(qmsg, QCS_SRC, nickname);
	qcs_msgset(qmsg, QCS_DST, REF_DATA->local_src);
	qmsg->mode = umode_to_qcs(umode);

	/* send the msg */
	qcs_send(REF_DATA->link_id, qmsg);
	qcs_deletemsg(qmsg);

#undef REF_DATA
}

static int handle_recv_refresh_req(
	qnet * net, qnet_msg * nmsg,
	const qcs_msg * qmsg )
{
	struct ref_cb_data cb_data;

	cb_data.link_id = ((struct local_net_data*)net->conn)->link_id;
	cb_data.local_src = qmsg->src;

	/* do enumeration of users
	 * altogether with ack replies
	 */
	usercache_enum_except_net(
		&net->id, refresh_req_cb,
		(void *) &cb_data
	);
	return 0;
}

static int handle_recv_refresh_ack(
		qnet * net, qnet_msg * nmsg,
		const qcs_msg * qmsg)
{
	user_id uid;
	enum net_umode umode;

	if(!eq_nickname(qmsg->dst, QCROUTER_NICK)) {
		/* not destined at us: ignore */
		return 0;
	}

	uid = usercache_uid_of(qmsg->src);

	/* mark the user as `alive' */
	usercache_tag_alive(uid);

	/* check if the mode has changed */
	umode = qcs_to_umode(qmsg->mode);
	if(umode == usercache_umode_of(&uid))
		return 0; /* ignore: the same mode */

	/* update the mode in user cache */
	usercache_add(&uid, &umode, NULL, NULL);

	/* and emit mode change msg */
	nmsg->type = MSGTYPE_USER_MODECHANGE;
	nmsg->src.net = net->id;
	msg_set_broadcast(nmsg);

	nmsg->d_user = uid;
	nmsg->d_umode = umode;

	return 1;
}

static int handle_recv_message(
	qnet * net, qnet_msg * nmsg,
	const qcs_msg * qmsg)
{
	assert(net && nmsg && qmsg);

	nmsg->src = usercache_uid_of(qmsg->src);
	nmsg->dst = usercache_uid_of(qmsg->dst);

	if(net->id==nmsg->dst.net
		|| !VALID_USER(nmsg->dst) || !VALID_USER(nmsg->src) )
	{
		/* the message is not destined to users on
		 * outer nets: ignore */
		return 0;
	}

	/* fill in `qnet_msg' fields */
	NETMSG_SET_TEXT(nmsg, qmsg->text);

	switch(qmsg->msg)
	{
	case QCS_MSG_MESSAGE_ACK:
		nmsg->type = MSGTYPE_PMSG_ACK;
		nmsg->d_umode = qcs_to_umode(qmsg->mode);
		break;
	case QCS_MSG_MESSAGE_SEND:
	case QCS_MSG_MESSAGE_MASS:
		nmsg->type =MSGTYPE_PMSG_SEND;
		break;
	default:break;
	}

	return 1;
}

static int handle_recv_channel(
	qnet * net, qnet_msg * nmsg,
	const qcs_msg * qmsg )
{
	qnet_msg * delayed;

	/* check if there's delayed QCS_MSG_CHANNEL_LEAVE:
	 * 	MSGTYPE_USER_LOST: pending
	 */
	nmsg->src = usercache_uid_of(qmsg->src);
	msg_set_broadcast(nmsg);

	if(!VALID_USER(nmsg->src)) return 0;

	NETMSG_SET_CHANLIST(nmsg, qmsg->chan);

	switch(qmsg->msg)
	{
	case QCS_MSG_CHANNEL_BROADCAST:
	case QCS_MSG_CHANNEL_ME:
		nmsg->type = MSGTYPE_CHANNEL_TEXT;

		NETMSG_SET_TEXT(nmsg, qmsg->text);
		nmsg->d_me_text = qmsg->msg==QCS_MSG_CHANNEL_ME;
		break;

	case QCS_MSG_CHANNEL_JOIN:
		nmsg->type = MSGTYPE_CHANNEL_JOIN;
		nmsg->d_umode = qcs_to_umode(qmsg->mode);
		break;

	case QCS_MSG_CHANNEL_LEAVE:
		nmsg->type = MSGTYPE_CHANNEL_LEAVE;

		/* reply with MSGTYPE_USER_LEAVE
		 * if qmsg->chan=="main"
		 */
		if(!strcasecmp(qmsg->chan, "main"))
		{
			/* make USER_LOST msg */
			delayed = msg_new();
			delayed->type = MSGTYPE_USER_LOST;
			delayed->src.net = net->id;
			msg_set_broadcast(delayed);
			delayed->d_user = nmsg->src;

			/* delay it */
			msgq_push(NETCONN->delayed_queue, delayed);
		}
		break;
	default:break;
	}

	return 1;
}

static int handle_recv_info_request(
	qnet * net, qnet_msg * nmsg,
	const qcs_msg * qmsg )
{
	qcs_msg * reply_qmsg;
	user_id	dst_id;
	chanlist_t chanlist;

	dst_id = usercache_uid_of(qmsg->dst);

	if( dst_id.net==net->id || !VALID_USER(dst_id)) {
		/* the info requested is not
		 * about one of masqueraded users */
		return 0;
	}
	
	/* do reply about the user requested */
	reply_qmsg = qcs_newmsg();

	/* make chanlist */
	strcpy(chanlist, usercache_chanlist_of(&dst_id));
	strcat(chanlist, "#");

	reply_qmsg->msg = QCS_MSG_INFO_REPLY;
	qcs_msgset(reply_qmsg, QCS_SRC, qmsg->dst);
	qcs_msgset(reply_qmsg, QCS_DST, qmsg->src);
	qcs_msgset(reply_qmsg, QCS_TEXT, user_id_dump(&dst_id));
	qcs_msgset(reply_qmsg, QCS_CHAN, chanlist);
	qcs_msgset(reply_qmsg, QCS_SUPP, "[masqueraded over qcRouter net]");
	
	qcs_send(NETCONN->link_id, reply_qmsg);
	qcs_deletemsg(reply_qmsg);

	return 1;
}

static int handle_recv_beep(
	qnet * net, qnet_msg * nmsg,
	const qcs_msg * qmsg)
{
	assert(net && nmsg && qmsg);

	nmsg->src = usercache_uid_of(qmsg->src);
	nmsg->dst = usercache_uid_of(qmsg->dst);

	if(net->id==nmsg->dst.net
		|| !VALID_USER(nmsg->src) || !VALID_USER(nmsg->dst))
	{
		/* not directed outside: ignore */
		return 0;
	}

	nmsg->type = qmsg->msg==QCS_MSG_BEEP_SEND
			? MSGTYPE_BEEP
			: MSGTYPE_BEEP_ACK;
	return 1;
}

static int handle_recv_private(
	qnet * net, qnet_msg * nmsg,
	const qcs_msg * qmsg)
{
	nmsg->src = usercache_uid_of(qmsg->src);
	nmsg->dst = usercache_uid_of(qmsg->dst);

	if(net->id==nmsg->dst.net
		|| !VALID_USER(nmsg->src) || !VALID_USER(nmsg->dst))
	{
		/* not directed outside this local net: ignore */
		return 0;
	}

	switch(qmsg->msg)
	{
	case QCS_MSG_PRIVATE_OPEN:
		nmsg->type = MSGTYPE_PRIVATE_OPEN;
		break;

	case QCS_MSG_PRIVATE_CLOSE:
		nmsg->type = MSGTYPE_PRIVATE_CLOSE;
		break;

	case QCS_MSG_PRIVATE_TEXT:
	case QCS_MSG_PRIVATE_ME:
		nmsg->type = MSGTYPE_PRIVATE_TEXT;

		NETMSG_SET_TEXT(nmsg, qmsg->text);
		nmsg->d_me_text = qmsg->msg==QCS_MSG_PRIVATE_ME;
		break;
	default:break;
	}

	return 1;
}

static int handle_new_user(
	qnet * net, qnet_msg * nmsg,
	const qcs_msg * qmsg )
{
	enum net_umode	umode;
	chanlist_t	chanlist;

	if(qmsg->msg==QCS_MSG_CHANNEL_LEAVE) {
		/* ignore CHANNEL_LEAVE:
		 * 	these might go AFTER #Main leave
		 * 	i.e. after we send MSGTYPE_USER_LOST:
		 */
		return 0;
	}

	if(qmsg->src==NULL || usercache_known(qmsg->src)
		|| qmsg->msg==QCS_MSG_RENAME
		)
		return 0; /* the source is known or is not applicable */

	/* the source user is unknown: add it to usercache and
	 * return MSGTYPE_USER_NEW */

	/* postpone this qmsg
	 */
	assert(NETCONN->delayed_qmsg==NULL);
	NETCONN->delayed_qmsg = (qcs_msg*)qmsg;

	/* build MSGTYPE_USER_NEW msg
	 */

	/* make umode & chanlist */
	umode = qmsg->mode==QCS_UMODE_INVALID
			? UMODE_NORMAL
			: qcs_to_umode(qmsg->mode);
	strcpy(chanlist, MAIN_CHANNEL);

	nmsg->type = MSGTYPE_USER_NEW;
	nmsg->src.net = net->id;
	msg_set_broadcast(nmsg);

	nmsg->d_user.net = net->id;
	nmsg->d_user.num = ++ NETCONN->next_user_id;

	nmsg->d_umode = umode;
	NETMSG_SET_NICKNAME(nmsg, qmsg->src);
	NETMSG_SET_CHANLIST(nmsg, chanlist);

	return 1;
}

static int handle_recv_topic_change(
	qnet * net, qnet_msg * nmsg,
	const qcs_msg * qmsg )
{
	/* ignore if it's the same as previous one
	 * or the channel is unknown (1 or -1) */
	if(usercache_same_topic(qmsg->chan, qmsg->text) & 0x1) {
		return 0;	/* ignore msg */
	}

	/* setup msg */
	nmsg->src.net = net->id;
	msg_set_broadcast(nmsg);

	nmsg->type = MSGTYPE_TOPIC_CHANGE;

	NETMSG_SET_TEXT(nmsg, qmsg->text);
	NETMSG_SET_CHANLIST(nmsg, qmsg->chan);

	return 1;
}

static int handle_recv_mode_change(
	qnet * net, qnet_msg * nmsg,
	const qcs_msg * qmsg )
{
	/* handle mode change */
	nmsg->src.net = net->id;
	msg_set_broadcast(nmsg);

	nmsg->type = MSGTYPE_USER_MODECHANGE;

	nmsg->d_user = usercache_uid_of(qmsg->src);
	nmsg->d_umode = qcs_to_umode(qmsg->mode);

	return 1;
}

static int handle_recv_rename(
	qnet * net, qnet_msg * nmsg,
	const qcs_msg * qmsg )
{
	user_id uid;

	/* check that we know the source:
	 * 	handle_new_user will not process
	 * 	QCS_MSG_RENAME, thus we might get null_user_id
	 */
	uid = usercache_uid_of(qmsg->src);
	if(is_null_user(uid)) {
		return 0;	/* skip this msg */
	}
	
	/* handle nickname-rename */
	nmsg->src.net = net->id;
	msg_set_broadcast(nmsg);

	nmsg->type = MSGTYPE_USER_NICKCHANGE;

	nmsg->d_user = uid;
	NETMSG_SET_NICKNAME(nmsg, qmsg->text);	/* new nickname	*/
	NETMSG_SET_TEXT(nmsg, qmsg->src);	/* previous nick */

	return 1;
}

/** switch_qmsg:
 * 	dispatches qcs_msg to the handler
 * 	judging by its `msg' field
 * returns
 * 	1: if more msg left,
 * 	0 otherwise
 */
static void switch_qmsg(
	qnet * net, qnet_msg * nmsg,
	const qcs_msg * qmsg)
{
	struct qmsg_parse_entry * qpe;

	assert(net && nmsg && qmsg);

	/* find msg processing routine */
	for(qpe = entries; qpe->proc!=NULL; qpe ++) {
		if(qpe->msg==qmsg->msg) {
			break;
		}
	}
	if(qpe->proc==NULL) {
		/* no such msg handler found: ignore */
		nmsg->type = MSGTYPE_NULL;
	} else {
		/* invoke msg processing routine */
		if(!qpe->proc(net, nmsg, qmsg)) {
			/* invalidate msg */
			nmsg->type = MSGTYPE_NULL;
		}
	}
}

static void local_reply_handshake(
		qnet * net)
{
	qnet_msg * delayed = msg_new();

	assert(net);

	delayed->type = MSGTYPE_HANDSHAKE;

	/* generate local net id */
	NETMSG_SET_TEXT(delayed, "LOCAL_NET");
	delayed->d_net = common_next_id();

	/* insert into queue for reply in next recv() */
	msgq_push(NETCONN->delayed_queue, delayed);
}

void handle_refresh_timeout(
	timer_id tm,
	int shot_nr,
	void * net)
{
	qcs_msg * qmsg = qcs_newmsg();
	user_id * dead;
	unsigned dead_count, i;
	qnet_msg * msg;

#define QNET ((qnet*)net)
	assert(QNET);

	debug("handle_refresh_timeout: processing any dead users..");

	/* remove any who didn't care to reply us
	 * on refresh */
	dead = usercache_dead_users_from(QNET->id, &dead_count);
	if(dead) {
		for(i = 0; i < dead_count; i++) {
			/* broadcast msg on funerals */
			msg = msg_new();
			msg_set_broadcast(msg);
			msg->type = MSGTYPE_USER_LOST;
			msg->src.net = QNET->id;
			msg->d_user = dead[i];
			
			msgq_push(((struct local_net_data*)QNET->conn)
					->delayed_queue, msg);

			/* log the event of death */
			log_a("net:\tlocal user \"");
			log_a(usercache_nickname_of(dead+i));
			log("\" went dead: no reply to refresh request");
		}
		xfree(dead);
	}

	/* send REFRESH_REQUEST */
	qmsg->msg = QCS_MSG_REFRESH_REQUEST;
	qcs_msgset(qmsg, QCS_SRC, QCROUTER_NICK);
	qcs_send(((struct local_net_data*)QNET->conn)->link_id, qmsg);

	qcs_deletemsg(qmsg);

	/* and mark those users dead again ;) */
	usercache_tag_dead_from(QNET->id);
	
#undef QNET
}

/** exported routines
 *************************************/

/** local_destroy:
 * 	destroys qchat/vchat connection
 */
static void local_destroy(qnet * net)
{
	assert(net);

	/* destroy delayed msg, if there is one */
	if(NETCONN->delayed_qmsg) {
		qcs_deletemsg(NETCONN->delayed_qmsg);
	}

	/* delete timer */
	timer_stop(NETCONN->tm_refresh);

	/* terminate connection */
	qcs_close(NETCONN->link_id);

	/* delete msg queue */
	msgq_delete(NETCONN->delayed_queue);

	/* free struct */
	xfree(net->conn);
	xfree(net);

	return;
}

/** local_recv
 * 	handles messages from the qcs net
 * 	and translates to qnet_msg
 */
static qnet_msg * local_recv(
	qnet * net,
	int * p_more_msg_left )
		/* (set if caller should call for another msg) */
{
	user_id uid;
	qnet_msg * nmsg;
	qcs_msg * qmsg;
	int succ;

	assert(net && p_more_msg_left);

	/* check if there's delayed qnet_msg
	 */
	if(!msgq_empty(NETCONN->delayed_queue)) {
		nmsg = msgq_pop(NETCONN->delayed_queue);

		*p_more_msg_left = NETCONN->delayed_qmsg!=NULL
				|| !msgq_empty(NETCONN->delayed_queue);

		return nmsg;
	}

	/* we may assume here that no more msg has left to recv
	 *	as the delayed_queue is empty and 
	 *	NETCONN->delayed_qmsg will be handled next */
	*p_more_msg_left = 0;

	/* try to recv the qcs_msg:
	 * 	either delayed or from the link */
	if(NETCONN->delayed_qmsg) {
		qmsg = NETCONN->delayed_qmsg;
		NETCONN->delayed_qmsg = NULL;
	} else {
		/* get msg from the link */
		qmsg = qcs_newmsg();

		succ = qcs_recv(NETCONN->link_id, qmsg);
		if(!succ && errno!=ENOMSG) {
			/* link failure */
			qcs_deletemsg(qmsg);
	
			log_a("net:\tlink receive failure for net ");
			log_a(net_id_dump(&net->id));
			log_a(": "); log(strerror(errno));
			return NULL;
		}
	}

	/* ignore the msg if it came from ourselves
	 * or one of masqueraded users (not from this net)
	 * e.g. REFRESH_REQUEST
	 */
	nmsg = msg_new();

	if(qmsg->src != NULL) {
		uid = usercache_uid_of(qmsg->src);

		if(eq_nickname(qmsg->src, QCROUTER_NICK)
			|| (!is_null_net(uid.net) && uid.net!=net->id)
			)
		{
			qcs_deletemsg(qmsg);
			nmsg->type = MSGTYPE_NULL;
			return nmsg;
		}
	}

	/* check if the qmsg->src is unknown:
	 *	this might be a new user.
	 *	if it is, postpone this msg and return MSGTYPE_USER_NEW
	 */
	if(!handle_new_user(net, nmsg, qmsg)) {
		/* this is not a msg from new user:
		 *	handle in common maneer */
		switch_qmsg(net, nmsg, qmsg);
	}

	*p_more_msg_left = NETCONN->delayed_qmsg!=NULL
		|| !msgq_empty(NETCONN->delayed_queue);

	/* we should not delete the msg if it has
	 * 	been registered as delayed one
	 */
	if(NETCONN->delayed_qmsg!=qmsg) {
		qcs_deletemsg(qmsg);
	}

	return nmsg;
}

/** local_send:
 * 	converts qnet_msg to qcs_msg
 * 	and sends over to the net
 */
static void local_send(
		qnet * net, const qnet_msg * nmsg)
{
	qnet_msg * delayed;
	qcs_msg * qmsg = qcs_newmsg();

#define SRC_NICK  usercache_nickname_of(&nmsg->src)
#define DST_NICK  usercache_nickname_of(&nmsg->dst)
#define USER_NICK usercache_nickname_of(&nmsg->d_user)

	switch(nmsg->type)
	{
	case MSGTYPE_NET_ENUM_ENDS:
		delayed = msg_new();
		delayed->type = MSGTYPE_NET_ENUM_ENDS;
		msgq_push(NETCONN->delayed_queue, delayed);

		qcs_deletemsg(qmsg);
		return;
			
	case MSGTYPE_USER_ENUM_REQUEST:
		qmsg->msg = QCS_MSG_REFRESH_REQUEST;
		qcs_msgset(qmsg, QCS_SRC, QCROUTER_NICK);
		break;

	case MSGTYPE_HANDSHAKE:
		local_reply_handshake(net);
		/* don't emit anything onto the net */
		qcs_deletemsg(qmsg);
		return;
	
	case MSGTYPE_USER_NICKCHANGE:
		qmsg->msg = QCS_MSG_RENAME;
		qcs_msgset(qmsg, QCS_SRC, nmsg->d_text);
		qcs_msgset(qmsg, QCS_TEXT, nmsg->d_nickname);
		break;
	
	case MSGTYPE_USER_MODECHANGE:
		qmsg->msg = QCS_MSG_MODE_CHANGE;
		qcs_msgset(qmsg, QCS_SRC, USER_NICK);
		qmsg->mode = umode_to_qcs(nmsg->d_umode);
		break;

	case MSGTYPE_CHANNEL_JOIN:
		qmsg->msg = QCS_MSG_CHANNEL_JOIN;
		qcs_msgset(qmsg, QCS_SRC, SRC_NICK);
		qcs_msgset(qmsg, QCS_CHAN, nmsg->d_chanlist);
		break;

	case MSGTYPE_CHANNEL_LEAVE:
		qmsg->msg = QCS_MSG_CHANNEL_LEAVE;
		qcs_msgset(qmsg, QCS_SRC, SRC_NICK);
		qcs_msgset(qmsg, QCS_CHAN, nmsg->d_chanlist);
		break;

	case MSGTYPE_CHANNEL_TEXT:
		qmsg->msg = nmsg->d_me_text
			? QCS_MSG_CHANNEL_ME
			: QCS_MSG_CHANNEL_BROADCAST;

		qcs_msgset(qmsg, QCS_SRC, SRC_NICK);
		qcs_msgset(qmsg, QCS_CHAN, nmsg->d_chanlist);
		qcs_msgset(qmsg, QCS_TEXT, nmsg->d_text);
		break;

	case MSGTYPE_PRIVATE_CLOSE:
	case MSGTYPE_PRIVATE_OPEN:
		qmsg->msg = nmsg->type==MSGTYPE_PRIVATE_OPEN
				? QCS_MSG_PRIVATE_OPEN
				: QCS_MSG_PRIVATE_CLOSE;

		qcs_msgset(qmsg, QCS_SRC, SRC_NICK);
		qcs_msgset(qmsg, QCS_DST, DST_NICK);
		break;

	case MSGTYPE_PRIVATE_TEXT:
		qmsg->msg = nmsg->d_me_text
			? QCS_MSG_PRIVATE_ME
			: QCS_MSG_PRIVATE_TEXT;
	
		qcs_msgset(qmsg, QCS_SRC, SRC_NICK);
		qcs_msgset(qmsg, QCS_DST, DST_NICK);
		qcs_msgset(qmsg, QCS_TEXT, nmsg->d_text);
		break;

	case MSGTYPE_TOPIC_CHANGE:
		qmsg->msg = QCS_MSG_TOPIC_CHANGE;
		qcs_msgset(qmsg, QCS_TEXT, nmsg->d_text);
		qcs_msgset(qmsg, QCS_CHAN, nmsg->d_chanlist);
		break;

	case MSGTYPE_PMSG_SEND:
		qmsg->msg = QCS_MSG_MESSAGE_SEND;
		qcs_msgset(qmsg, QCS_SRC, SRC_NICK);
		qcs_msgset(qmsg, QCS_DST, DST_NICK);
		qcs_msgset(qmsg, QCS_TEXT, nmsg->d_text);
		break;

	case MSGTYPE_PMSG_ACK:
		qmsg->msg = QCS_MSG_MESSAGE_ACK;
		qcs_msgset(qmsg, QCS_SRC, SRC_NICK);
		qcs_msgset(qmsg, QCS_DST, DST_NICK);
		qcs_msgset(qmsg, QCS_TEXT, nmsg->d_text);
		qmsg->mode = umode_to_qcs(nmsg->d_umode);
		break;

	case MSGTYPE_BEEP:
	case MSGTYPE_BEEP_ACK:
		qmsg->msg = nmsg->type==MSGTYPE_BEEP
			? QCS_MSG_BEEP_SEND
			: QCS_MSG_BEEP_ACK;
	
		qcs_msgset(qmsg, QCS_SRC, SRC_NICK);
		qcs_msgset(qmsg, QCS_DST, DST_NICK);
		break;
	default:
		qcs_deletemsg(qmsg);
		return;
	}

	/* send & release msg */
	qcs_send(NETCONN->link_id, qmsg);
	
	qcs_deletemsg(qmsg);
}

static int local_get_prop(
		qnet * net,
		enum qnet_property property)
{
	int sock;
	switch(property)
	{
	case QNETPROP_ONLINE:
		return 1;	/* we're always online */
	
	case QNETPROP_RX_SOCKET:
		qcs_rxsocket(NETCONN->link_id, &sock);
		return sock;

	case QNETPROP_DAMAGED:
		return 0;	/* XXX: really ?? */
	}
	return 0;
}

static int local_set_prop(
		qnet * net,
		enum qnet_property property, int value)
{
	/* no properties to set.. */
	return 0;
}

/** local_connect:
 * 	creates connection to 'local net'
 */
qnet * local_connect(
	const unsigned long * broadcast_addr,
	unsigned short port,
	enum qnet_type type)
{
	static const unsigned long default_broadcast[] = { 0xffffffffUL, 0 };
	qnet * net;
	qcs_link link_id;
	unsigned long * addr;
	char * logstr = xalloc(512);

	assert(type==QNETTYPE_VYPRESS_CHAT || type==QNETTYPE_QUICK_CHAT);

	/* set broadcast to 255.255.255.255, if not specified */
	if(broadcast_addr==NULL) {
		broadcast_addr = default_broadcast;
	}

	/* dump broadcast addresses */
	log("net:\tUDP/IP broadcast addresses for this connection:");
	log_a("net:\t\t");
	for(addr=(unsigned long*)broadcast_addr; *addr; addr++) {
		sprintf(logstr, " 0x%08lx", *addr);
		log_a(logstr);
	}
	log(".");
	xfree(logstr);

	/* setup connection */	
	link_id = qcs_open(
		type==QNETTYPE_QUICK_CHAT
			? QCS_PROTO_QCHAT:
			QCS_PROTO_VYPRESS ,
		broadcast_addr, port
	);
	if(link_id==NULL) {
		/* failed.. */
		log_a("net:\tlocal net connection failed: ");
		log(strerror(errno));

		return NULL;
	}

	net = xalloc(sizeof(qnet));
	
	/* setup qnet struct */
	net->conn = xalloc(sizeof(struct local_net_data));
	NETCONN->link_id = link_id;
	NETCONN->delayed_qmsg = NULL;
	NETCONN->next_user_id = 0;
	NETCONN->delayed_queue = msgq_new();

	net->type = type;

	/* setup action handlers */
	net->destroy = local_destroy;
	net->send = local_send;
	net->recv = local_recv;
	net->get_prop = local_get_prop;
	net->set_prop = local_set_prop;

	/* setup refresh timer for this net */
	NETCONN->tm_refresh = timer_start(
		refresh_timeout_sec, 0,
		handle_refresh_timeout, (void*)net);

	return net;
}

void localconn_init(unsigned refresh_timeout)
{
#ifndef NDEBUG
	char dbg[128];
	sprintf(dbg, "local_refresh_timeout = %dsecs", refresh_timeout);
	debug(dbg);
#endif

	refresh_timeout_sec =
		refresh_timeout ? refresh_timeout: 1;
}

void localconn_exit()
{
}

