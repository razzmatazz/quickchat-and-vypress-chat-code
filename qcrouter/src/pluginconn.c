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
 *		pluginconn.c
 *			implements Perl interface for writing plugins
 *		(for bots et al)
 *
 *	(c) Saulius Menkevicius 2002,2003
 */

#include <assert.h>

#include "common.h"
#include "msg.h"
#include "net.h"
#include "pluginconn.h"

#include <EXTERN.h>
#include <XSUB.h>
#include <perl.h>

struct plugin_data {
	PerlInterpreter * interpreter;
	msgq_id msg_queue;
	int rx_socket, initializing;
	int next_user_num;
};
#define PPLUGIN_DATA(p) ((struct plugin_data*)p)

/* static data */
static qnet * active_net;

#define ACTIVE_PLUGIN PPLUGIN_DATA(active_net->conn)

/* forward references */
static void plugin_destroy(qnet *);
static void plugin_send(qnet *, const qnet_msg *);
static qnet_msg * plugin_recv(qnet *, int *);
static int plugin_set_prop(qnet *, enum qnet_property, int);
static int plugin_get_prop(qnet *, enum qnet_property);

static void reply_to_handshake(qnet *);

extern void boot_DynaLoader _((CV * cv));

/** QCRouter::setRXSocket
 *	int socket
 */
static XS(XS_QCRouter_setRXSocket)
{
	dXSARGS;

	if(items>=1 && SvIOK(ST(0)) && ACTIVE_PLUGIN->initializing) {
		ACTIVE_PLUGIN->rx_socket = SvIV(ST(0));
	}

	XSRETURN_EMPTY;
}

/** QCRouter::newUser(
 *	int user_net
 *	int user_num
 * 	string nickname;
 *	string mode (i.e. "normal", "dnd", "away", "offline");
 *	string chanlist; (e.g. "#chan#chan2....#chanlast")
 */
static XS(XS_QCRouter_newUser)
{
	qnet_msg * msg;
	dXSARGS;

	if(items>=5 && SvIOK(ST(0)) && SvIOK(ST(1))
		&& SvPOK(ST(2)) && SvPOK(ST(3)) && SvPOK(ST(4)))
	{
		msg = msg_new();
		msg_set_broadcast(msg);
		msg->type = MSGTYPE_USER_NEW;
		msg->src.net = active_net->id;
		msg->d_user.net = SvIV(ST(0));
		msg->d_user.num = SvIV(ST(1));
		msg->d_umode = common_umode_by_name(SvPV_nolen(ST(3)));
		if(msg->d_umode==UMODE_INVALID) {
			msg->d_umode = UMODE_NORMAL;
		}
		NETMSG_SET_NICKNAME(msg, SvPV_nolen(ST(2)));
		NETMSG_SET_CHANLIST(msg, SvPV_nolen(ST(4)));

		/* check that the nickname, chanlist, user_id is valid */
		if(is_valid_nickname(msg->d_nickname)
			&& is_valid_chanlist(msg->d_chanlist)
			&& !usercache_known(msg->d_nickname)
			&& !usercache_exists(&msg->d_user))
		{
			msgq_push(ACTIVE_PLUGIN->msg_queue, msg);
		} else {
			msg_delete(msg);
		}
	}

	XSRETURN_EMPTY;
}

/** QCRouter::lostUser
 *	int user_net
 *	int user_num
 */
static XS(XS_QCRouter_lostUser)
{
	qnet_msg * msg;
	dXSARGS;

	if(items>=2 && SvIOK(ST(0)) && SvIOK(ST(1))) {
		msg = msg_new();
		msg_set_broadcast(msg);
		msg->src.net = active_net->id;
		msg->type = MSGTYPE_USER_LOST;
		msg->d_user.net = SvIV(ST(0));
		msg->d_user.num = SvIV(ST(1));

		if(usercache_exists(&msg->d_user)) {
			msgq_push(ACTIVE_PLUGIN->msg_queue, msg);
		} else {
			msg_delete(msg);
		}
	}

	XSRETURN_EMPTY;
}

/** QCRouter::userNickChange
 *	int user_net
 *	int user_num
 *	string new_nickname
 */
static XS(XS_QCRouter_userNickChange)
{
	qnet_msg * msg;
	dXSARGS;

	if(items>=3 && SvIOK(ST(0)) && SvIOK(ST(1)) && SvPOK(ST(2))) {
		msg = msg_new();
		msg_set_broadcast(msg);
		msg->src.net = active_net->id;
		msg->type = MSGTYPE_USER_NICKCHANGE;
		msg->d_user.net = SvIV(ST(0));
		msg->d_user.num = SvIV(ST(1));

		if(user_exists(&msg->d_user)
			&& is_valid_nickname(SvPV_nolen(ST(2))))
		{
			NETMSG_SET_TEXT(msg, user_nickname_of(&msg->d_user));
			NETMSG_SET_NICKNAME(msg, SvPV_nolen(ST(2)));

			msgq_push(ACTIVE_PLUGIN->msg_queue, msg);
		} else {
			msg_delete(msg);
		}
	}

	XSRETURN_EMPTY;
}

/** QCRouter::userModeChange
 *	int user_net
 *	int user_num
 *	string new_mode ("normal", ... )
 */
static XS(XS_QCRouter_userModeChange)
{
	qnet_msg * msg;
	dXSARGS;

	if(items>=3 && SvIOK(ST(0)) && SvIOK(ST(1)) && SvPOK(ST(2))) {
		msg = msg_new();
		msg_set_broadcast(msg);
		msg->src.net = active_net->id;
		msg->d_user.net = SvIV(ST(0));
		msg->d_user.num = SvIV(ST(1));
		msg->type = MSGTYPE_USER_MODECHANGE;

		msg->d_umode = common_umode_by_name(SvPV_nolen(ST(2)));
		if(msg->d_umode==UMODE_INVALID)
			msg->d_umode = UMODE_NORMAL;

		if(user_exists(&msg->d_user)) {
			msgq_push(ACTIVE_PLUGIN->msg_queue, msg);
		} else {
			msg_delete(msg);
		}
	}

	XSRETURN_EMPTY;
}

/** QCRouter::joinChannel
 *	int user_net
 *	int user_num
 *	string channel name
 */
static XS(XS_QCRouter_joinChannel)
{
	qnet_msg * msg;
	dXSARGS;

	if(items>=3 && SvIOK(ST(0)) && SvIOK(ST(1)) && SvPOK(ST(2))) {
		msg = msg_new();
		msg_set_broadcast(msg);
		msg->src.net = SvIV(ST(0));
		msg->src.num = SvIV(ST(1));
		msg->type = MSGTYPE_CHANNEL_JOIN;
		NETMSG_SET_CHANLIST(msg, SvPV_nolen(ST(2)));

		if(user_exists(&msg->src)) {
			msgq_push(ACTIVE_PLUGIN->msg_queue, msg);
		} else {
			msg_delete(msg);
		}
	}

	XSRETURN_EMPTY;
}

/** QCRouter::leaveChannel
 *	int user_net
 *	int user_num
 *	string channel name
 */
static XS(XS_QCRouter_leaveChannel)
{
	qnet_msg * msg;
	dXSARGS;

	if(items>=3 && SvIOK(0) && SvIOK(1) && SvPOK(2)) {
		msg = msg_new();
		msg_set_broadcast(msg);
		msg->src.net = SvIV(ST(0));
		msg->src.num = SvIV(ST(1));
		msg->type = MSGTYPE_CHANNEL_LEAVE;
		NETMSG_SET_CHANLIST(msg, SvPV_nolen(ST(2)));

		if(user_exists(&msg->src)) {
			msgq_push(ACTIVE_PLUGIN->msg_queue, msg);
		} else {
			msg_delete(msg);
		}
	}

	XSRETURN_EMPTY;
}

/** QCRouter::channelText
 *	int user_net
 *	int user_num
 *	string channel name
 *	string text
 *	[int is_me_text] = 0
 */
static XS(XS_QCRouter_channelText)
{
	qnet_msg * msg;
	dXSARGS;

	if(items>=4 && SvIOK(ST(0)) && SvIOK(ST(1)) && SvPOK(ST(2))
		&& SvPOK(ST(3)) && is_valid_channel(SvPV_nolen(ST(2))))
	{
		msg = msg_new();
		msg_set_broadcast(msg);
		msg->src.net = SvIV(ST(0));
		msg->src.num = SvIV(ST(1));
		msg->type = MSGTYPE_CHANNEL_TEXT;
		NETMSG_SET_CHANLIST(msg, SvPV_nolen(ST(2)));
		NETMSG_SET_TEXT(msg, SvPV_nolen(ST(3)));

		/* is this '/me' text ? */
		if(items>=5 && SvIOK(4)) {
			msg->d_me_text = SvIV(ST(4))!=0;
		} else {
			msg->d_me_text = 0;
		}

		if(user_exists(&msg->src)) {
			msgq_push(ACTIVE_PLUGIN->msg_queue, msg);
		} else {
			msg_delete(msg);
		}
	}

	XSRETURN_EMPTY;
}

/** QCRouter::privateOpen
 *	int src_net
 *	int dst_num
 *	int dst_net
 *	int dst_num
 */
static XS(XS_QCRouter_privateOpen)
{
	qnet_msg * msg;
	dXSARGS;

	if(items>=4 && SvIOK(ST(0)) && SvIOK(ST(1))
			&& SvIOK(ST(2)) && SvIOK(ST(3)))
	{
		msg = msg_new();
		msg->src.net = SvIV(ST(0));
		msg->src.num = SvIV(ST(1));
		msg->dst.net = SvIV(ST(2));
		msg->dst.num = SvIV(ST(3));
		msg->type = MSGTYPE_PRIVATE_OPEN;

		if(user_exists(&msg->src) && user_exists(&msg->dst)) {
			msgq_push(ACTIVE_PLUGIN->msg_queue, msg);
		} else {
			msg_delete(msg);
		}
	}

	XSRETURN_EMPTY;
}

/** QCRouter::privateClose
 *	int src_net
 *	int src_num
 *	int dst_net
 *	int dst_num
 */
static XS(XS_QCRouter_privateClose)
{
	qnet_msg * msg;
	dXSARGS;

	if(items>=4 && SvIOK(ST(0)) && SvIOK(1) && SvIOK(2) && SvIOK(3)) {
		msg = msg_new();
		msg->src.net = SvIV(ST(0));
		msg->src.num = SvIV(ST(1));
		msg->dst.net = SvIV(ST(2));
		msg->dst.num = SvIV(ST(3));
		msg->type = MSGTYPE_PRIVATE_CLOSE;

		if(user_exists(&msg->src) && user_exists(&msg->dst)) {
			msgq_push(ACTIVE_PLUGIN->msg_queue, msg);
		} else {
			msg_delete(msg);
		}
	}

	XSRETURN_EMPTY;
}

/** QCRouter::privateText
 *	int src_net
 *	int src_num
 *	int dst_net
 *	int dst_num
 *	string text
 *	[int is_me_text] = 0
 */
static XS(XS_QCRouter_privateText)
{
	qnet_msg * msg;
	dXSARGS;

	if(items>=5 && SvIOK(ST(0)) && SvIOK(ST(1)) && SvIOK(ST(2))
			&& SvIOK(ST(3)) && SvPOK(ST(4)))
	{
		msg = msg_new();
		msg->src.net = SvIV(ST(0));
		msg->src.num = SvIV(ST(1));
		msg->dst.net = SvIV(ST(2));
		msg->dst.num = SvIV(ST(3));
		msg->type = MSGTYPE_PRIVATE_TEXT;
		NETMSG_SET_TEXT(msg, SvPV_nolen(ST(4)));

		/* is this '/me' text ? */
		if(items>=6 && SvIOK(5)) {
			msg->d_me_text = SvIV(ST(5))!=0;
		} else {
			msg->d_me_text = 0;
		}

		if(user_exists(&msg->src) && user_exists(&msg->dst)) {
			msgq_push(ACTIVE_PLUGIN->msg_queue, msg);
		} else {
			msg_delete(msg);
		}
	}

	XSRETURN_EMPTY;
}

/* QCRouter::messageSend
 *	int src_net
 *	int src_num
 *	int dst_net
 *	int dst_num
 *	string text
 */
static XS(XS_QCRouter_messageSend)
{
	qnet_msg * msg;
	dXSARGS;

	if(items>=5 && SvIOK(ST(0)) && SvIOK(ST(1)) && SvIOK(ST(2))
				&& SvIOK(ST(3)) && SvPOK(ST(4)))
	{
		msg = msg_new();
		msg->src.net = SvIV(ST(0));
		msg->src.num = SvIV(ST(1));
		msg->dst.num = SvIV(ST(2));
		msg->dst.net = SvIV(ST(3));
		msg->type = MSGTYPE_PMSG_SEND;
		NETMSG_SET_TEXT(msg, SvPV_nolen(ST(4)));

		if(user_exists(&msg->src) && user_exists(&msg->dst)) {
			msgq_push(ACTIVE_PLUGIN->msg_queue, msg);
		} else {
			msg_delete(msg);
		}
	}

	XSRETURN_EMPTY;
}

/* QCRouter::messageAck
 *	int src_net
 *	int src_num
 *	int dst_net
 *	int dst_num
 *	[string text (why, etc)] = ""
 *	[string umode ("offline", "normal"...)] = "normal"
 */
static XS(XS_QCRouter_messageAck)
{
	qnet_msg * msg;
	dXSARGS;

	if(items>=4 && SvIOK(ST(0)) && SvIOK(ST(1))
				&& SvIOK(ST(2)) && SvIOK(ST(3)))
	{
		msg = msg_new();
		msg->src.net = SvIV(ST(0));
		msg->src.num = SvIV(ST(1));
		msg->dst.net = SvIV(ST(2));
		msg->dst.num = SvIV(ST(3));
		msg->type = MSGTYPE_PMSG_ACK;
		NETMSG_SET_TEXT(msg, (items>=5 && SvPOK(ST(4)))
						? SvPV_nolen(ST(4)): "");
		if(items>=6 && SvPOK(ST(5))) {
			msg->d_mode = common_umode_by_name(SvPV_nolen(ST(5)));
			if(msg->d_mode==UMODE_INVALID) {
				msg->d_mode = UMODE_NORMAL;
			}
		} else {
			msg->d_mode = UMODE_NORMAL;
		}

		if(user_exists(&msg->src) && user_exists(&msg->dst)) {
			msgq_push(ACTIVE_PLUGIN->msg_queue, msg);
		} else {
			msg_delete(msg);
		}
	}

	XSRETURN_EMPTY;
}

/* QCRouter::beep
 *	int src_net
 *	int src_num
 *	int dst_net
 *	int dst_num
 */
static XS(XS_QCRouter_beep)
{
	qnet_msg * msg;
	dXSARGS;

	if(items>=4 && SvIOK(ST(0)) && SvIOK(ST(1))
				&& SvIOK(ST(2)) && SvIOK(ST(3)))
	{
		msg = msg_new();
		msg->type = MSGTYPE_BEEP;
		msg->src.net = SvIV(ST(0));
		msg->src.num = SvIV(ST(1));
		msg->dst.net = SvIV(ST(2));
		msg->dst.num = SvIV(ST(3));

		if(user_exists(&msg->src) && user_exists(&msg->dst)) {
			msgq_push(active_plugin->msg_queue, msg);
		} else {
			msg_delete(msg);
		}
	}

	XSRETURN_EMPTY;
}

/* QCRouter::beepAck
 *	int src_net
 *	int src_num
 *	int dst_net
 *	int dst_num
 */
static XS(XS_QCRouter_beepAck)
{
	qnet_msg * msg;
	dXSARGS;

	if(items>=4 && SvIOK(ST(0)) && SvIOK(ST(1))
				&& SvIOK(ST(2)) && SvIOK(ST(3)))
	{
		msg = msg_new();
		msg->type = MSGTYPE_BEEP_ACK;
		msg->src.net = SvIV(ST(0));
		msg->src.num = SvIV(ST(1));
		msg->dst.net = SvIV(ST(2));
		msg->dst.num = SvIV(ST(3));

		if(user_exists(&msg->src) && user_exists(&msg->dst)) {
			msgq_push(active_plugin->msg_queue, msg);
		} else {
			msg_delete(msg);
		}
	}

	XSRETURN_EMPTY;
}

/* QCRouter::topicChange
 *	string channel
 *	string text
 */
static XS(XS_QCRouter_topicChange)
{
	qnet_msg * msg;
	dXSARGS;

	if(items>=2 && SvPOK(ST(0)) && SvPOK(ST(1))
			&& is_valid_channel(SvPV_nolen(ST(0))))
	{
		msg = msg_new();
		msg_set_broadcast(msg);
		msg->type = MSGTYPE_TOPIC_CHANGE;
		NETMSG_SET_CHANLIST(msg, SvPV_nolen(ST(0)));
		NETMSG_SET_TEXT(msg, SvPV_nolen(ST(1)));
		msgq_push(active_plugin->msg_queue, msg);
	}

	XSRETURN_EMPTY;

}

static XS(XS_QCRouter_userList)
{

}

static XS(XS_QCRouter_networkList)
{

}

/* static routines  */

static void
xs_init(pTHX)
{
	char * file = __FILE__;

	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);

	/* register exported routines.. */
	newXS("QCRouter::setRXSocket", XS_QCRouter_setRXSocket, "QCRouter");
}

qnet * plugin_open(const char * filename)
{
	qnet * net;
	PerlInterpreter * pi;
	char * args[] = { "", filename };

	/* setup the interpreter and parse the file */
	pi = perl_alloc();
	perl_construct(pi);
	perl_parse(pi, xs_init, filename, 2, args, (char**)0);

	/* setup network struct */
	net = xalloc(sizeof(qnet));
	net->conn = xalloc(sizeof(struct plugin_data));
	PPLUGIN_DATA(net->conn)->interpreter = pi;
	PPLUGIN_DATA(net->conn)->msg_queue = msgq_new();
	PPLUGIN_DATA(net->conn)->rx_socket = -1;
	PPLUGIN_DATA(net->conn)->initializing = 1;
	PPLUGIN_DATA(net->conn)->next_user_num = 0;
	net->type = NETTYPE_PLUGIN;
	net->destroy = plugin_destroy;
	net->send = plugin_send;
	net->recv = plugin_recv;
	net->get_prop = plugin_get_prop;
	net->set_prop = plugin_set_prop;

	/* initialize script */
	active_net = net;
	perl_run(pi);

	PPLUGIN_DATA(net->conn)->initializing = 0;

	return net;	
}

static void
plugin_destroy(
	qnet * net)
{
	perl_destruct(PPLUGIN_DATA(net->conn)->interpreter);
	perl_free(PPLUGIN_DATA(net->conn)->interpreter);

	msgq_delete(PPLUGIN_DATA(net->conn)->msg_queue);

	xfree(net->conn);
	xfree(net);
}

static void
plugin_send(
	qnet * net, const qnet_msg * qmsg)
{
	qnet_msg * reply;

	switch(qmsg->type) {
	case MSGTYPE_HANDSHAKE:
		/* make hanshake message */
		reply = msg_new();
		reply->type = MSGTYPE_HANDSHAKE;
		NETMSG_SET_TEXT(reply, "PERL_PLUGIN");
		reply->d_net = common_next_id();

		/* enqueue handshake reply for retrieval via plugin_recv() */
		msgq_push(PPLUGIN_DATA(reply->conn)->msg_queue, reply);
		break;

	case MSGTYPE_NET_ENUM_ENDS:
		reply = msg_new();
		reply->type = MSGTYPE_NET_ENUM_ENDS;
		msgq_push(PPLUGIN_DATA(reply->conn)->msg_queue, reply);
		break;

	default:
		active_net = net;

		/* XXX: invoke perl handler */
		break;
	}
}

static qnet_msg *
plugin_recv(qnet * net, int * p_more_left)
{
	qnet_msg * qmsg = msgq_pop(PPLUGIN_DATA(net->conn)->msq_queue);
	if(p_more_left) {
		*p_more_left = !msgq_empty(PPLUGIN_DATA(net->conn)->msg_queue);
	}
	return qmsg;
}

static int
plugin_set_prop(
	qnet * net,
	enum qnet_property property, int value)
{
	/* no properties to set currently */
	return 0;
}

static int
plugin_get_prop(
	qnet * net, enum qnet_property property)
{
	switch(property) {
	case QNETPROP_ONLINE:
		/* plugin is always online */
		return 1;
	case QNETPROP_RX_SOCKET:
		/* plugin can QCRouter::setRXSocket during initialization */
		return PPLUGIN_DATA(net->conn)->rx_socket;
	case QNETPROP_RX_PENDING:
		/* core checks this property if plugin has no RX socket
		 * and thus cannot be poll()'ed
		 */
		return !msgq_empty(PPLUGIN_DATA(net->conn)->msg_queue);
	case QNETPROP_DAMAGED:
		/* supposedly never damaged */
		return 0;
	}
	return 0;
}

static void
reply_to_handshake(qnet * net)
{
	qnet_msg * qmsg = msg_new();

	/* make handshake msg */
	qmsg->type = MSGTYPE_HANDSHAKE;
	NETMSG_SET_TEXT(qmsg, "PERL_PLUGIN");
	qmsg->d_net = common_next_id();

	/* enqueue handshake reply for retrieval via plugin_recv() */
	msgq_push(PPLUGIN_DATA(net->conn)->msg_queue, qmsg);
}

