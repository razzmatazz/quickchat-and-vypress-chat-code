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
 *		msg.h
 *			manages net_msg'es and msg id caches
 *
 *	(c) Saulius Menkevicius 2002
 */

#ifndef MSG_H__
#define MSG_H__

#define QNET_MSG_TEXT_LEN	511

enum net_msg_type {
	MSGTYPE_INVALID,
	MSGTYPE_NULL,		/* carries no info, dummy */
	
		/* HANDSHAKE:
		 * 	d_net	net id
		 * 	d_text	?? version ??
		 */
	MSGTYPE_HANDSHAKE,	/* version & netid check, etc after connect */

	MSGTYPE_NET_NEW,	/* new net found in src net */
	MSGTYPE_NET_LOST,
	MSGTYPE_NET_ENUM_ENDS,

	MSGTYPE_PING,		/* calculates which route packets go faster */
	MSGTYPE_PONG,

		/* USER_NEW:
		 * 	d_user = user id
		 * 	d_umode = user mode;
		 * 	d_nickname = user's nickname
		 * 	d_chanlist = user's chanlist */
	MSGTYPE_USER_NEW,

		/* USER_NICKCHANGE:
		 * 	d_user = user id
		 * 	d_text = previous nickname
		 * 	d_nickname = new nickname */
	MSGTYPE_USER_NICKCHANGE,

		/* USER_MODECHANGE:
		 * 	d_user = user id
		 * 	d_umode	= new user mode */
	MSGTYPE_USER_MODECHANGE,

		/* USER_LOST:
		 * 	d_user = user id */
	MSGTYPE_USER_LOST,

	MSGTYPE_USER_ENUM_REQUEST,

		/* CHANNEL_JOIN:
		 * 	src = user_id
		 * 	d_chanlist = channel name
		 * 	d_umode = user mode */
	MSGTYPE_CHANNEL_JOIN,

		/* CHANNEL_LEAVE:
		 * 	src = user_id
		 * 	d_chanlist = channel name */
	MSGTYPE_CHANNEL_LEAVE,

		/* CHANNEL_TEXT:
		 * 	src = user_id
		 * 	d_chanlist = destination channel
		 * 	d_text = text
		 * 	d_me_text = is this text /me text */
	MSGTYPE_CHANNEL_TEXT,

		/* PRIVATE_OPEN
		 * 	src = from
		 * 	dst = to
		 */
	MSGTYPE_PRIVATE_OPEN,

		/* PRIVATE_CLOSE
		 * 	src = from
		 * 	dst = to
		 */
	MSGTYPE_PRIVATE_CLOSE,

		/* PRIVATE_TEXT
		 * 	src = from
		 * 	dst = to
		 * 	d_text = text
		 * 	d_me_text = is this /me text ?
		 */
	MSGTYPE_PRIVATE_TEXT,

		/* PMSG_SEND
		 * 	src = from
		 * 	dst = to
		 * 	d_text = msg
		 */
	MSGTYPE_PMSG_SEND,

		/* PMSG_ACK
		 * 	src = from (who's received the msg)
		 * 	dst = to
		 * 	text = ack text (why, how, etc)
		 *
		 * 	umode = mode of receiver
		 */
	MSGTYPE_PMSG_ACK,

		/* BEEP
		 * 	src = from
		 * 	dst = to
		 */
	MSGTYPE_BEEP,

		/* BEEP_ACK
		 *	src = from
		 *	dst = to
		 */
	MSGTYPE_BEEP_ACK,

		/* TOPIC_CHANGE
		 *	d_chanlist = channel
		 *	d_text = new_topic
		 */
	MSGTYPE_TOPIC_CHANGE
};

typedef char msg_text_t[QNET_MSG_TEXT_LEN+1];
typedef unsigned long qnet_msg_id;

typedef struct qnet_msg_struct {
	/* local info about msg (not transmitted) */
	void * src_qnet;
	
	/* msg header */
	qnet_msg_id id;		/* identifies msg as globally-unique one */
	enum net_msg_type type;	/* type (purpose) of the message */
	user_id src, dst;	/* user ignored, if trans-net msg */

	/* data payload */
	net_id		d_net;
	user_id		d_user;
	unsigned short	d_me_text;
	enum net_umode	d_umode;
	nickname_t	d_nickname;
	chanlist_t	d_chanlist;
	msg_text_t	d_text;

} qnet_msg;

#define NETMSG_SET_NICKNAME(nmsg, src) \
		strncpy((nmsg)->d_nickname, (src), NICKNAME_LEN_MAX)
#define NETMSG_SET_CHANLIST(nmsg, src) \
		strncpy((nmsg)->d_chanlist, (src), CHANLIST_LEN_MAX)
#define NETMSG_SET_TEXT(nmsg, src) \
		strncpy((nmsg)->d_text, (src), CHANLIST_LEN_MAX)


/* msg_new: ALSO ASSIGNS AN ID: no need to call idcache_assign_id */
qnet_msg * msg_new(void);
void msg_delete(qnet_msg *);
void msg_set_broadcast(qnet_msg *);
int msg_is_broadcast(const qnet_msg *);

/** idcache:
 * 	defines interface for registering
 * 	and checking msg id's
 */
#define IDCACHE_MAX_SIZE	0x40

typedef struct idcache_struct {
	qnet_msg_id * ids;
	int head;
} idcache_t;

idcache_t * idcache_new();
void idcache_delete(idcache_t *);

int idcache_is_known(idcache_t *, const qnet_msg *);
void idcache_register(idcache_t *, const qnet_msg *);
void idcache_assign_id(idcache_t *, qnet_msg *);

/** msg_queue:
 *	implements simple message queueing list
 */
typedef void * msgq_id;
struct qnet_struct;
msgq_id msgq_new();
void msgq_delete(msgq_id);
void msgq_push(msgq_id, qnet_msg *);
qnet_msg * msgq_pop(msgq_id);
int msgq_empty(msgq_id);

#endif /* #ifndef MSG_H__ */

