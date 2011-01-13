/**
 * qcs_link: Vypress/QChat protocol interface library
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
 * (c) Saulius Menkevicius 2001,2002
 */

#ifndef QCS_LINK_H
#define QCS_LINK_H

/* API constants
 */
#define QCS_PROTO_QCHAT		0x0
#define QCS_PROTO_VYPRESS	0x1

#define QCS_UMODE_NORMAL	0x01
#define QCS_UMODE_DND		0x02
#define QCS_UMODE_AWAY		0x03
#define QCS_UMODE_OFFLINE	0x04

#define QCS_UMODE_WATCH		0x80
#define QCS_UMODE_INVALID	0x00

enum qcs_msgid {
	QCS_MSG_INVALID = 0,

		/* MSG_REFRESH_REQUEST:
		 *   QCS_SRC	requestor
		 */
	QCS_MSG_REFRESH_REQUEST,

		/* MSG_REFRESH_ACK:
		 *   QCS_SRC	ACK sender
		 *   QCS_DST	ACK receiver
		 *   
		 *   umode	mode
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
		 *   umode	mode of the receiver of the message
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

		/* MSG_WATCH_CHANGE:
		 *   QCS_SRC			user
		 *   (umode & QCS_UMODE_WATCH)	new watch-mode
		 */
	QCS_MSG_WATCH_CHANGE,

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
	QCS_SRC,	QCS_DST,
	QCS_TEXT,	QCS_SUPP,
	QCS_CHAN
};

/* qcs_msg:
 *	protocol message type */
typedef struct _qcs_msg {
	enum qcs_msgid msg;	/* message ID */
	int mode;	/* user mode: offline/dnd, etc | watch */
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
	int	proto_mode,	/* QCS_PROTO_QCHAT/QCS_PROTO_VYPRESS	*/
	const unsigned long * broadcasts,
		/* 0UL terminated list of bcst addresses */
	unsigned short port );	/* port to bind to (if 0, uses def.)	*/

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

#ifdef __cplusplus
/**
 * QcsLink:
 * 	C++ interface
 */

class QcsLink;

class QcsMsg {
	qcs_msg * m_msg;
public:
	QcsMsg() { m_msg = qcs_newmsg(); }
	~QcsMsg() { qcs_deletemsg(m_msg); }

	qcs_msgid msg() { return m_msg->msg; }
	int umode() { return m_msg->mode; }
	const char * src() { return m_msg->src ? m_msg->src: ""; }
	const char * dst() { return m_msg->dst ? m_msg->dst: ""; }
	const char * text() { return m_msg->text ? m_msg->text: ""; }
	const char * supp() { return m_msg->supp ? m_msg->supp: ""; }
	const char * chan() { return m_msg->chan ? m_msg->chan: ""; }

	void asRefreshRequest(const char * src, const char * dst) {
		m_msg->msg = QCS_MSG_REFRESH_REQUEST;
		qcs_msgset(m_msg, QCS_SRC, src);
		qcs_msgset(m_msg, QCS_DST, dst);
	}
	void asRefreshAck(const char * src, const char * dst) {
		m_msg->msg = QCS_MSG_REFRESH_ACK;
		qcs_msgset(m_msg, QCS_SRC, src);
		qcs_msgset(m_msg, QCS_DST, dst);
	}
	void asChannelText(
		const char * src, const char * chan,
		const char * text, bool me)
	{
		m_msg->msg = me ? QCS_MSG_CHANNEL_ME: QCS_MSG_CHANNEL_BROADCAST;
		qcs_msgset(m_msg, QCS_SRC, src);
		qcs_msgset(m_msg, QCS_CHAN, chan);
		qcs_msgset(m_msg, QCS_TEXT, text);
	}
	void asChannelJoin(const char * src, const char * chan, int umode) {
		m_msg->msg = QCS_MSG_CHANNEL_JOIN;
		m_msg->mode = m_msg->mode;
		qcs_msgset(m_msg, QCS_SRC, src);
		qcs_msgset(m_msg, QCS_CHAN, chan);
	}
	void asChannelLeave(const char * src, const char * chan) {
		m_msg->msg = QCS_MSG_CHANNEL_LEAVE;
		qcs_msgset(m_msg, QCS_SRC, src);
		qcs_msgset(m_msg, QCS_CHAN, chan);
	}
	void asMessageSend(
		const char * src, const char * dst,
		const char * text, bool mass=false)
	{
		m_msg->msg = mass ? QCS_MSG_MESSAGE_MASS: QCS_MSG_MESSAGE_SEND;
		qcs_msgset(m_msg, QCS_SRC, src);
		qcs_msgset(m_msg, QCS_DST, dst);
		qcs_msgset(m_msg, QCS_TEXT, text);
	}
	void asMessageAck(
		const char * src, const char * dst,
		const char * text, int umode )
	{
		m_msg->msg = QCS_MSG_MESSAGE_ACK;
		qcs_msgset(m_msg, QCS_SRC, src);
		qcs_msgset(m_msg, QCS_DST, dst);
		qcs_msgset(m_msg, QCS_TEXT, text);
	}
	void asRename(const char * src, const char * to) {
		m_msg->msg = QCS_MSG_RENAME;
		qcs_msgset(m_msg, QCS_SRC, src);
		qcs_msgset(m_msg, QCS_DST, to);
	}
	void asUmodeChange(const char * src, int umode) {
		m_msg->msg = QCS_MSG_MODE_CHANGE;
		m_msg->mode = umode;
		qcs_msgset(m_msg, QCS_SRC, src);
	}
	void asWatchChange(const char * src, int watch) {
		m_msg->msg = QCS_MSG_WATCH_CHANGE;
		m_msg->mode = watch;
		qcs_msgset(m_msg, QCS_SRC, src);
	}
	void asTopicChange(const char * text) {
		m_msg->msg = QCS_MSG_TOPIC_CHANGE;
		qcs_msgset(m_msg, QCS_TEXT, text);
	}
	void asTopicReply(const char * dst, const char * text) {
		m_msg->msg = QCS_MSG_TOPIC_REPLY;
		qcs_msgset(m_msg, QCS_DST, dst);
		qcs_msgset(m_msg, QCS_TEXT, text);
	}
	void asInfoRequest(const char * src, const char * dst) {
		m_msg->msg = QCS_MSG_INFO_REQUEST;
		qcs_msgset(m_msg, QCS_SRC, src);
		qcs_msgset(m_msg, QCS_DST, dst);
	}
	void asInfoReply(
		const char * src, const char * dst,
		const char * text, const char * chan,
		const char * supp )
	{
		m_msg->msg = QCS_MSG_INFO_REPLY;
		qcs_msgset(m_msg, QCS_SRC, src);
		qcs_msgset(m_msg, QCS_DST, dst);
		qcs_msgset(m_msg, QCS_TEXT, text);
		qcs_msgset(m_msg, QCS_SUPP, supp);
	}

	/* XXX: implement more asXXX */

	friend QcsLink;
};

class QcsLink {
	qcs_link mLink;
	int mProto;
public:
	QcsLink(int proto=QCS_PROTO_QCHAT) {
		mLink = NULL;
		mProto = proto;
	}
	~QcsLink() { if(mLink!=NULL) close(); }

	bool open(const unsigned long *bcasts , unsigned short port) {
		mLink = qcs_open(mProto, bcasts, port);
		return mLink!=NULL;
	}
	bool close() {
		bool succ = qcs_close(mLink);
		mLink = NULL;
		return succ;
	}
	bool isOpen() { return mLink!=NULL; }
	int rxSocket() {
		int sock;
		qcs_rxsocket(mLink, &sock);
		return sock;
	}
	bool wait(int ms) {
		return qcs_waitinput(mLink, ms)!=0;
	}
	bool send(const QcsMsg &msg) {
		return qcs_send(mLink, msg.m_msg)!=0;
	}
	bool recv(QcsMsg &msg) {
		return qcs_recv(mLink, msg.m_msg)!=0;
	}
};

#endif	/* #ifdef __cplusplus */

#endif	/* QCS_LINK_H */
