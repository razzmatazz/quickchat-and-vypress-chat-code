/**
 * libqcproto: Vypress/QChat protocol interface library
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

/*
 * QCS: qChat 1.6/VypressChat link interface
 *
 * (c) Saulius Menkevicius 2001-2004
 */

#ifndef QCPROTO_H__
#define QCPROTO_H__

/* API constants
 */
enum qcs_proto {
	QCS_PROTO_QCHAT,
	QCS_PROTO_VYPRESS,
	QCS_PROTO_NUM
};
enum qcs_proto_opt {
	QCS_PROTO_OPT_MULTICAST = 0x01
};

enum qcs_umode {
	QCS_UMODE_INVALID,
	QCS_UMODE_NORMAL,
	QCS_UMODE_DND,
	QCS_UMODE_AWAY,
	QCS_UMODE_OFFLINE
};

enum qcs_msgid {
	QCS_MSG_INVALID,

		/* MSG_REFRESH_REQUEST:
		 *   QCS_SRC	requestor
		 */
	QCS_MSG_REFRESH_REQUEST,

		/* MSG_REFRESH_ACK:
		 *   QCS_SRC	ACK sender
		 *   QCS_DST	ACK receiver
		 *   
		 *   umode	sender mode
		 *   uactive	non-0 if the user is active
		 */
	QCS_MSG_REFRESH_ACK,

		/* MSG_CHANNEL_BROADCAST:
		 *   QCS_SRC	sender
		 *   QCS_TEXT	text that goes to the channel
		 *   QCS_CHAN	channel name (#chan format)
		 */
	QCS_MSG_CHANNEL_BROADCAST,

		/* MSG_CHANNEL_JOIN:
		 *   QCS_SRC	user
		 *   QCS_CHAN	channel name (#chan)
		 *
		 *   umode	user mode
		 */
	QCS_MSG_CHANNEL_JOIN,

		/* MSG_CHANNEL_LEAVE:
		 *   QCS_SRC	user
		 *   QCS_CHAN	channel name
		 */
	QCS_MSG_CHANNEL_LEAVE,

		/* MSG_CHANNEL_ME
		 *   QCS_SRC	user
		 *   QCS_TEXT	text
		 *   QCS_CHAN	channel name
		 */
	QCS_MSG_CHANNEL_ME,

		/* MSG_MESSAGE_SEND:
		 *   QCS_SRC	sender
		 *   QCS_DST	receiver
		 *   QCS_TEXT	msg text	
		 */
	QCS_MSG_MESSAGE_SEND,

		/* MSG_MESSAGE_MASS:
		 *   QCS_SRC	sender
		 *   QCS_DST	receiver (each on the net will get this text)
		 *   QCS_TEXT	msg text
		 */
	QCS_MSG_MESSAGE_MASS,

		/* MSG_MESSAGE_ACK:
		 *   QCS_SRC	sender of ACK
		 *   QCS_DST	receiver of ACK
		 *   QCS_TEXT	ack text
		 *   
		 *   umode	mode of the sender
		 */
	QCS_MSG_MESSAGE_ACK,

		/* MSG_RENAME:
		 *   QCS_SRC	previous nickname
		 *   QCS_TEXT	new nickname
		 */
	QCS_MSG_RENAME,

		/* MSG_MODE_CHANGE:
		 *   QCS_SRC	user
		 *
		 *   umode	new mode
		 */
	QCS_MSG_MODE_CHANGE,

		/* MSG_ACTIVE_CHANGE:
		 *   QCS_SRC			user
		 *
		 *   uactive	non-0 if the user became active, 0 otherwise
		 */
	QCS_MSG_ACTIVE_CHANGE,

		/* MSG_TOPIC_CHANGE:
		 *   QCS_TEXT	new topic
		 *   QCS_CHAN	channel name
		 */
	QCS_MSG_TOPIC_CHANGE,

		/* MSG_TOPIC_REPLY:
		 *   QCS_DST	receiver of topic text
		 *   QCS_TEXT	current topic
		 */
	QCS_MSG_TOPIC_REPLY,

		/* MSG_INFO_REQUEST:
		 *   QCS_SRC	sender of request-for-info
		 *   QCS_DST	receiver of request
		 */
	QCS_MSG_INFO_REQUEST,

		/* MSG_INFO_REPLY:
		 *   QCS_SRC	info about this user
		 *   QCS_DST	receiver of info
		 *   QCS_TEXT	(login name??)
		 *   QCS_CHAN	channels the user is on
		 *   QCS_SUPP	user's motd
		 */
	QCS_MSG_INFO_REPLY,

		/* MSG_CHANMEMBER_REQUEST:
		 *   QCS_SRC	requestor for replies of channel members
		 *   			(where to send replies)
		 */
	QCS_MSG_CHANMEMBER_REQUEST,

		/* MSG_CHANMEMBER_REPLY:
		 *   QCS_SRC	name of sender
		 *   QCS_DST	name of receiver of replies
		 *   QCS_CHAN	channels the sender is on (#..#..#)
		 */
	QCS_MSG_CHANMEMBER_REPLY,

		/* MSG_CHANLIST_REQUEST:
		 *   QCS_SRC	name of requestor
		 */
	QCS_MSG_CHANLIST_REQUEST,

		/* MSG_CHANLIST_REPLY:
		 *   QCS_DST	requestor
		 *   QCS_CHAN	channels the replier is on
		 */
	QCS_MSG_CHANLIST_REPLY,

		/* MSG_BEEP_SEND:
		 *   QCS_SRC	sender of beep
		 *   QCS_DST	receiver of beep
		 */
	QCS_MSG_BEEP_SEND,

		/* MSG_BEEP_ACK:
		 *   QCS_SRC	sender of ack (receiver of beep)
		 *   QCS_DST	receiver of ack
		 */
	QCS_MSG_BEEP_ACK,

		/* MSG_PRIVATE_OPEN:
		 *   QCS_SRC	sender of private open request
		 *   QCS_DST	receiver of request
		 */
	QCS_MSG_PRIVATE_OPEN,

		/* MSG_PRIVATE_CLOSE:
		 *   QCS_SRC	sender of private-close request
		 *   QCS_DST	another party of private
		 */
	QCS_MSG_PRIVATE_CLOSE,

		/* MSG_PRIVATE_TEXT:
		 *   QCS_SRC	sender
		 *   QCS_DST	receiver
		 *   QCS_TEXT	text
		 *
		 */
	QCS_MSG_PRIVATE_TEXT,

		/* MSG_PRIVATE_ME:
		 *   QCS_SRC	sender
		 *   QCS_DST	receiver
		 *   QCS_TEXT	text
		 */
	QCS_MSG_PRIVATE_ME
};

enum qcs_textid {
	QCS_SRC,
	QCS_DST,
	QCS_TEXT,
	QCS_SUPP,
	QCS_CHAN
};

/* qcs_msg:
 *	protocol message type */
typedef struct _qcs_msg {
	enum qcs_msgid msg;	/* message ID */
	enum qcs_umode umode;	/* user mode: offline/dnd, etc | watch */
	unsigned long src_ip;	/* IP address of message source */
	int uactive;		/* used to notify if the user is active */
	char * src;	/* src nickname */
	char * dst;	/* dst nickname */
	char * text;
	char * supp;	/* supplementary text */
	char * chan;	/* channels (in '#Main#...#' form) */
} qcs_msg;

#ifdef __cplusplus
extern "C" {
#endif

typedef void* qcs_link;

/* qcs_open
 *	initializes network link	*/
qcs_link qcs_open( 
	enum qcs_proto proto,
	enum qcs_proto_opt,
	unsigned long broadcast_addr,	 /* can be multicast addr */
	unsigned short port);

/* qcs_close
 *	close specified link 		*/
int qcs_close(qcs_link link);

/* qcs_rxsocket
 *	return RX socket identifier	*/
int qcs_rxsocket(
	qcs_link link,
	int * p_rxsocket );	/* pointer to id store */

/* qcs_waitinput
 *	for RX input, or return in non-blocking mode (if timeout==0)
 * returns:
 *	0 if timed-out
 *	>0 if input pending
 *	<0, if error (see errno)
 */
int qcs_waitinput(
	qcs_link link,
	int timeout_ms );	/* msecs to wait before timeout */

/* qcs_send
 *	sends message to the link	*/
int qcs_send(
	qcs_link link,
	const qcs_msg * msg );

/* qcs_recv
 *	retrieves message from the link		*/
int qcs_recv(
	qcs_link link,
	qcs_msg * msg );

/* qcs_newmsg
 * qcs_deletemsg
 *	(de)allocates & initializes new message struct	*/
qcs_msg * qcs_newmsg();

void qcs_deletemsg(
	qcs_msg * msg  );	/* msg to delete	*/

/* qcs_msgset
 *	sets message text parameter
 */
int qcs_msgset(
	qcs_msg * msg,
	enum qcs_textid which,
	const char * new_text );

#ifdef __cplusplus
}
#endif

#endif		/* #ifdef QCPROTO_H__ */
