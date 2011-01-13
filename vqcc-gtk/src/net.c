/*
 * net.c: network routines
 * Copyright (C) 2002-2004 Saulius Menkevicius
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: net.c,v 1.36 2005/01/06 10:16:09 bobas Exp $
 */

#include <glib.h>
#include <errno.h>
#include <string.h>

#include "libqcproto/qcproto.h"

#include "main.h"
#include "net.h"
#include "prefs.h"
#include "sess.h"
#include "user.h"
#include "gui.h"

#define NET_DEFAULT_CHARSET "ISO-8859-1"

/** static vars
  *************************/
static qcs_link net;
static enum net_type_enum net_type;
static guint net_event_source;
static GString * old_nickname;

#define LOG_NET		N_("Network")

#define ERRSTR_ENCODING	N_("Charset encoding/decoding error; \
please check \"Network charset\" under the \"Network\" on the Preferences dialog")

/** local routines
  *************************/
static gboolean net_connect();
static void net_disconnect();

static enum user_mode_enum
net_local_mode(int qcs_umode)
{
	enum user_mode_enum m;

	switch(qcs_umode) {
	case QCS_UMODE_NORMAL:	m = UMODE_NORMAL;	break;
	case QCS_UMODE_OFFLINE:	m = UMODE_OFFLINE;	break;
	case QCS_UMODE_DND:	m = UMODE_DND;		break;
	case QCS_UMODE_AWAY:	m = UMODE_AWAY;		break;
	case QCS_UMODE_INVALID:	m = UMODE_NULL;		break;
	}
	return m;
}

static int
net_qcs_mode(enum user_mode_enum local_mode)
{
	int m;

	switch(local_mode) {
	case UMODE_NORMAL:
	case UMODE_INVISIBLE:
		m = QCS_UMODE_NORMAL;
		break;
	case UMODE_DND:		m = QCS_UMODE_DND;	break;
	case UMODE_AWAY:	m = QCS_UMODE_AWAY;	break;
	case UMODE_OFFLINE:	m = QCS_UMODE_OFFLINE;	break;
	default:		m = QCS_UMODE_INVALID;	break;
	}
	return m;
}

static void
delete_converted_msg(qcs_msg * m)
{
	if(m->src) g_free(m->src);
	if(m->dst) g_free(m->dst);
	if(m->text) g_free(m->text);
	if(m->supp) g_free(m->supp);
	if(m->chan) g_free(m->chan);
	g_free(m);
}

static qcs_msg *
convert_msg_charset(const qcs_msg * m, const gchar * to, const gchar * from)
{
	qcs_msg * t = g_new0(qcs_msg, 1);

	t->msg = m->msg;
	t->umode = m->umode;
	t->uactive = m->uactive;
	t->src_ip = m->src_ip;
	t->src = !m->src ? NULL: g_convert_with_fallback(m->src, -1, to, from, "?", NULL, NULL, NULL);
	t->dst = !m->dst ? NULL: g_convert_with_fallback(m->dst, -1, to, from, "?", NULL, NULL, NULL);
	t->text = !m->text ? NULL: g_convert_with_fallback(m->text, -1, to, from, "?", NULL, NULL, NULL);
	t->supp = !m->supp ? NULL: g_convert_with_fallback(m->supp, -1, to, from, "?", NULL, NULL, NULL);
	t->chan = !m->chan ? NULL: g_convert_with_fallback(m->chan, -1, to, from, "?", NULL, NULL, NULL);

	/* if any of the conversions have failed.. */
	if((m->src && !t->src) || (m->dst && !t->dst) || (m->text && !t->text)
	    || (m->supp && !t->supp) || (m->chan && !t->chan)) {
		delete_converted_msg(t);
		return NULL;
	}

	return t;
}

static GList *
decode_chanlist(const char * chanstring)
{
	const char * p, * next;
	char * name;
	GList * chanlist = NULL;
	int len;

	g_assert(chanstring);

	len = strlen(chanstring);

	/* check that the chanstring is valid */
	if (len<3 || chanstring[0]!='#' || chanstring[len-1]!='#') {
		return NULL;
	}

	/* parse the chanstring */
	p = chanstring + 1;
	do {
		/* extract name */
		next = strchr(p, '#');
		g_assert(next);

		/* make sure that the channel name is valid */
		len = next - p;
		if (len>0 && len<=MAX_CHAN_LENGTH) {
			/* malloc a copy */
			name = g_malloc(len+1);
			memcpy(name, p, len);
			name[len] = '\0';

			/* insert this into list */
			chanlist = g_list_append(chanlist, name);
		}

		/* seek for next channel */
		p = next + 1;
	} while (*p);

	return chanlist;
}

static void
free_chanlist(GList * chanlist)
{
	while(chanlist) {
		g_free(chanlist->data);
		chanlist = g_list_delete_link(chanlist, chanlist);
	}
}

static void
net_errno(int errno_value)
{
	const gchar * error_str = _(strerror(errno_value));

	log_ferror(_(LOG_NET), g_strdup_printf("ERROR (%s)", error_str));
	raise_event(EVENT_NET_ERROR, (gpointer)error_str, errno_value);
}

/*
 * net_send:
 *	sends a message to network
 */
static void
net_send(const qcs_msg * m)
{
	g_assert(m);

	if(net) {
		qcs_msg * cmsg = convert_msg_charset(m, prefs_str(PREFS_NET_ENCODING), "UTF-8");
		if(cmsg) {
			if(!qcs_send(net, cmsg)) {
				net_errno(errno);
			}
			delete_converted_msg(cmsg);
		} else {
			log_error(_(LOG_NET), _(ERRSTR_ENCODING));
		}
	}
}

/**
 * reply_on_net_join:
 *	sends topic info & our existence to new user 
 */
static void
net_reply_on_net_join(const char * nick)
{
	qcs_msg * m;

	g_assert(nick && strlen(nick));

	m = qcs_newmsg();

	/* send topic reply 
	 *	XXX: don't know what to
	 *	reply to vypresschat clients
	 */

	/*
	m->msg = QCS_MSG_TOPIC_REPLY;
	qcs_msgset(m, QCS_DST, nick);
	qcs_msgset(m, QCS_TEXT, "");
	nett_send(m);
	*/

	/* send refresh reply */
	m->msg = QCS_MSG_REFRESH_ACK;
	m->umode = net_qcs_mode(my_mode());
	m->uactive = gui_is_active();
	qcs_msgset(m, QCS_SRC, my_nickname());
	qcs_msgset(m, QCS_DST, nick);

	net_send(m);

	qcs_deletemsg(m);
}

/** net_reply_to_info:
  *	replies to info requests
  */
static void
net_reply_to_info_request(const char * src)
{

	g_assert(src);

	/* do the logging */
	if(prefs_bool(PREFS_NET_VERBOSE))
		log_fevent(_(LOG_NET), g_strdup_printf(_("host info request from %s"),src));

	/* reply, if configured to */
	if(prefs_bool(PREFS_NET_REPLY_INFO_REQ) && my_mode()!=UMODE_INVISIBLE) {
		gchar * chanlist;
		qcs_msg * m = qcs_newmsg();

		m->msg = QCS_MSG_INFO_REPLY;
		qcs_msgset(m, QCS_SRC, my_nickname());
		qcs_msgset(m, QCS_DST, src);
		qcs_msgset(m, QCS_TEXT, my_nickname());
		qcs_msgset(m, QCS_SUPP, my_motd());

		qcs_msgset(m, QCS_CHAN, chanlist = sess_make_channel_list());
		g_free(chanlist);

		net_send(m);
		qcs_deletemsg(m);

		if(prefs_bool(PREFS_NET_VERBOSE))
			log_event(_(LOG_NET), _("..replied to info request"));
	}
}

/* net_reply_to_chanlist_request:
 *	sends reply to the QCS_MSG_CHANLIST_REQUEST message
 */
static void
net_reply_to_chanlist_request(gpointer user)
{
	if(prefs_bool(PREFS_NET_REPLY_INFO_REQ) && my_mode()!=UMODE_INVISIBLE) {
		gchar * chanlist;
		qcs_msg * m = qcs_newmsg();
		m->msg = QCS_MSG_CHANLIST_REPLY;
		qcs_msgset(m, QCS_DST, user_name_of(user));
		qcs_msgset(m, QCS_CHAN, chanlist = sess_make_channel_list());
		g_free(chanlist);

		net_send(m);
		qcs_deletemsg(m);
	}
}

static void
net_send_refresh_ack(const gchar * name)
{
	g_assert(name && g_utf8_strlen(name, -1));

	if(my_mode() != UMODE_INVISIBLE) {
		qcs_msg * m = qcs_newmsg();
		m->msg = QCS_MSG_REFRESH_ACK;
		m->umode = net_qcs_mode(my_mode());
		m->uactive = gui_is_active();
		qcs_msgset(m, QCS_SRC, my_nickname());
		qcs_msgset(m, QCS_DST, name);

		net_send(m);
		qcs_deletemsg(m);
	}
}

static void
net_beep_sent_from(gpointer user)
{
	/* send beep ack (if not invisible) */
	if(my_mode() != UMODE_INVISIBLE) {	
		qcs_msg * m = qcs_newmsg();
		m->msg = QCS_MSG_BEEP_ACK;
		qcs_msgset(m, QCS_SRC, my_nickname());
		qcs_msgset(m, QCS_DST, user_name_of(user));
		net_send(m);
		qcs_deletemsg(m);
	}

	/* inform the world about beep message from 'user' */
	raise_event(EVENT_NET_MSG_BEEP_SEND, user, 0);
}

static void
net_message_from(
	const gchar * nickname, const gchar * text,
	gboolean mass_message)
{
	qcs_msg * m;
	gpointer event_v[3];
	gpointer user;

	g_assert(nickname && text);
	
	/* check if we can receive the message in this mode
	 */
	if(!IS_MESSAGEABLE_MODE(my_mode())) {
		log_fevent(_(LOG_NET), g_strdup_printf(
			mass_message
				? _("%s has sent a mass message however you are in %s mode")
				: _("%s has sent you a message however you are in %s mode"),
			nickname, user_mode_name(my_mode())
			)
		);
		return;
	}

	/* check if this message is a mass message, and if we block them
	 */
	if(mass_message && prefs_bool(PREFS_NET_IGNORE_MASS_MSG)) {
		log_fevent(_(LOG_NET), g_strdup_printf(
			_("%s has sent a mass message however you have blocked them"),
			nickname)
		);
		return;
	}

	if((user = user_by_name(nickname))!=0) {
		event_v[0] = user;
		event_v[1] = (gpointer)text;
		event_v[2] = (gpointer)mass_message;
		raise_event(EVENT_NET_MSG_MESSAGE, event_v, 0);

		/* send confirmation that we've received his/her message */
		m = qcs_newmsg();
		m->msg = QCS_MSG_MESSAGE_ACK;
		m->umode = net_qcs_mode(my_mode());
		qcs_msgset(m, QCS_SRC, my_nickname());
		qcs_msgset(m, QCS_DST, user_name_of(user));
		qcs_msgset(m, QCS_TEXT, my_motd());
		net_send(m);
		qcs_deletemsg(m);
	}
}
	
/**
 * net_handle_netmsg:
 *	Parses a message from the network.
 *	Here we assume that m->src contains NULL if m->msg
 *	has nothing to do with m->src
 */
static void
net_handle_netmsg(const qcs_msg * raw_msg)
{
	gpointer event_v[5];
	qcs_msg * m;

	g_assert(raw_msg);

	m = convert_msg_charset(raw_msg, "UTF-8", prefs_str(PREFS_NET_ENCODING));
	if(!m) {
		/* conversion failed */
		log_error(_(LOG_NET), _(ERRSTR_ENCODING));
		return;
	}

	/* check if the msg is invalid or one from ourselves */
	if( m->msg==QCS_MSG_INVALID
		|| (m->dst && nickname_cmp(m->dst, my_nickname()))
		|| (m->src && !nickname_cmp(m->src, my_nickname())) )
	{
		goto delete_and_exit;
	}

	if(m->src && m->msg!=QCS_MSG_CHANNEL_LEAVE
		&& m->msg!=QCS_MSG_RENAME
		&& m->msg!=QCS_MSG_REFRESH_REQUEST
		&& m->msg!=QCS_MSG_PRIVATE_CLOSE)
	{
		/* check if the user is not ignored */
		if(prefs_list_contains(PREFS_NET_IGNORED_USERS, m->src))
			goto delete_and_exit;
		
		if(!user_exists(m->src)) {
			event_v[0] = m->src;
			event_v[1] = GINT_TO_POINTER((m->msg==QCS_MSG_CHANNEL_JOIN
				|| m->msg==QCS_MSG_MODE_CHANGE
				|| m->msg==QCS_MSG_REFRESH_ACK)
					? net_local_mode(m->umode) : UMODE_NORMAL);
			event_v[2] = GINT_TO_POINTER(m->msg==QCS_MSG_REFRESH_ACK);

			/* new user found on the network */
			raise_event(EVENT_NET_NEW_USER, event_v, 0);
		}
	}

	switch(m->msg) {
	case QCS_MSG_TOPIC_REPLY:
	case QCS_MSG_TOPIC_CHANGE:
		if(net_type==NET_TYPE_QCHAT) {
			/* topic is the same for all channels in QChat */
			raise_event(EVENT_NET_MSG_TOPIC_CHANGE, m->text, 0);
		} else {
			/* topic is different for each channel in VyChat*/
			event_v[0] = m->chan;
			event_v[1] = m->text;
			raise_event(EVENT_NET_MSG_TOPIC_CHANGE_4, event_v, 0);
		}
		break;

	case QCS_MSG_REFRESH_REQUEST:
		net_send_refresh_ack(m->src);
		break;

	case QCS_MSG_REFRESH_ACK:
		event_v[0] = user_by_name(m->src);
		event_v[1] = GINT_TO_POINTER(m->uactive);
		raise_event(
			EVENT_NET_MSG_REFRESH_ACK,
			event_v, net_local_mode(m->umode));
		break;

	case QCS_MSG_INFO_REQUEST:
		net_reply_to_info_request(m->src);
		break;

	case QCS_MSG_MESSAGE_MASS:
	case QCS_MSG_MESSAGE_SEND:
		net_message_from(m->src, m->text, m->msg==QCS_MSG_MESSAGE_MASS);
		break;

	case QCS_MSG_MESSAGE_ACK:
		event_v[0] = user_by_name(m->src);
		event_v[1] = m->text;
		if(event_v[0])
			raise_event(
				EVENT_NET_MSG_MESSAGE_ACK,
				event_v, net_local_mode(m->umode));
		break;

	case QCS_MSG_RENAME:
		event_v[0] = user_by_name(m->src);
		event_v[1] = m->text;
		if(event_v[0])
			raise_event(EVENT_NET_MSG_RENAME, event_v, 0);
		break;

	case QCS_MSG_MODE_CHANGE:
		raise_event(
			EVENT_NET_MSG_MODE_CHANGE,
			user_by_name(m->src), net_local_mode(m->umode));
		break;

	case QCS_MSG_ACTIVE_CHANGE:
		raise_event(
			EVENT_NET_MSG_ACTIVE_CHANGE,
			user_by_name(m->src), m->uactive);
		break;

	case QCS_MSG_CHANNEL_JOIN:
		if(!channel_cmp(m->chan, "Main")) {
			/* reply topic & our presence */
			net_reply_on_net_join(m->src);
		}
		event_v[0] = user_by_name(m->src);
		event_v[1] = m->chan;
		raise_event(EVENT_NET_MSG_CHANNEL_JOIN, event_v, 0);
		break;

	case QCS_MSG_CHANNEL_LEAVE:
		event_v[0] = user_by_name(m->src);
		event_v[1] = m->chan;
		if(event_v[0])
			raise_event(EVENT_NET_MSG_CHANNEL_LEAVE, event_v, 0);
		break;

	case QCS_MSG_CHANNEL_BROADCAST:
	case QCS_MSG_CHANNEL_ME:
		event_v[0] = user_by_name(m->src);
		event_v[1] = m->chan;
		event_v[2] = m->text;
		raise_event(
			EVENT_NET_MSG_CHANNEL_TEXT,
			event_v, m->msg==QCS_MSG_CHANNEL_ME);
		break;

	case QCS_MSG_PRIVATE_OPEN:
		raise_event(EVENT_NET_MSG_PRIVATE_OPEN, user_by_name(m->src), 0);
		break;

	case QCS_MSG_PRIVATE_CLOSE:
		event_v[0] = user_by_name(m->src);
		if(event_v[0])
			raise_event(EVENT_NET_MSG_PRIVATE_CLOSE, event_v[0], 0);
		break;

	case QCS_MSG_PRIVATE_TEXT:
	case QCS_MSG_PRIVATE_ME:
		event_v[0] = user_by_name(m->src);
		event_v[1] = m->text;
		raise_event(EVENT_NET_MSG_PRIVATE_TEXT,
			event_v, m->msg==QCS_MSG_PRIVATE_ME);
		break;
		
	case QCS_MSG_INFO_REPLY:
		event_v[0] = user_by_name(m->src);
		event_v[2] = (gpointer)decode_chanlist(m->chan);
		if(event_v[2]) {
			event_v[1] = m->text;
			event_v[3] = m->supp;
			event_v[4] = GINT_TO_POINTER(m->src_ip);
			raise_event(EVENT_NET_MSG_INFO_REPLY, event_v, 0);
			free_chanlist((GList*)event_v[2]);
		}
		break;

	case QCS_MSG_BEEP_SEND:
		net_beep_sent_from(user_by_name(m->src));
		break;

	case QCS_MSG_BEEP_ACK:
		raise_event(EVENT_NET_MSG_BEEP_ACK, user_by_name(m->src), 0);
		break;
		
	case QCS_MSG_CHANLIST_REQUEST:
		net_reply_to_chanlist_request(user_by_name(m->src));
		break;
		
	case QCS_MSG_CHANLIST_REPLY:
		event_v[0] = (gpointer)decode_chanlist(m->chan);
		if (event_v[0]!=NULL) {
			raise_event(EVENT_NET_MSG_CHANNEL_LIST, event_v[0], 0);
			free_chanlist( (GList*)event_v[0] );
		}
		break;
	default:
		break;	/* NOTHING */
	}
	
delete_and_exit:
	delete_converted_msg(m);
}

static gboolean
net_io_func(
	GIOChannel * s,
	GIOCondition c,
	gpointer data)
{
	qcs_msg * m;

	switch(c) {
	case G_IO_IN:
		m = qcs_newmsg();
		qcs_recv(net, m);
		net_handle_netmsg(m);
		qcs_deletemsg(m);
		break;
	case G_IO_ERR:
	case G_IO_HUP:
	case G_IO_NVAL:
		net_errno(0);
		break;
	default:
		break; /* nothing else */
	}

	return TRUE;	/* leave handler untouched */
}

static void
net_handle_my_text(
	const char * text,
	int me_text)		/* for "/me <text>" expressions */
{
	sess_id s;
	enum session_type type;
	qcs_msg * m;

	/* get current session & check that it's channel or private */
	s = sess_current();
	g_assert(s);
	type = sess_type(s);
	if(type!=SESSTYPE_CHANNEL && type!=SESSTYPE_PRIVATE)
		return;

	/* make proto message */
	g_assert(text);

	m = qcs_newmsg();
	qcs_msgset(m, QCS_SRC, my_nickname());
	qcs_msgset(m, QCS_TEXT, text);

	/* set msg type & destination */
	if(type==SESSTYPE_CHANNEL) {
		m->msg = me_text
			? QCS_MSG_CHANNEL_ME : QCS_MSG_CHANNEL_BROADCAST;
		qcs_msgset(m, QCS_CHAN, sess_name(s));
	} else {
		m->msg = me_text
			? QCS_MSG_PRIVATE_ME : QCS_MSG_PRIVATE_TEXT;
		qcs_msgset(m, QCS_DST, sess_name(s));
	}

	/* send the msg */
	net_send(m);

	qcs_deletemsg(m);
}

static void
net_handle_session_open_close(
	sess_id s,
	int open)	/* open(1) or close(0) */
{
	qcs_msg * m;

	/* don't notify members of the channel if
	 * the `PREFS_CHANNEL_GREET' config option is not set,
	 * or we are "invisible"
	 */
 	if(sess_type(s)==SESSTYPE_CHANNEL
			&& (!prefs_bool(PREFS_NET_CHANNEL_NOTIFY) || my_mode()==UMODE_INVISIBLE)
		) return;

	m = qcs_newmsg();

	switch(sess_type(s)) {
	case SESSTYPE_PRIVATE:
		m->msg = open ? QCS_MSG_PRIVATE_OPEN: QCS_MSG_PRIVATE_CLOSE;
		qcs_msgset(m, QCS_SRC, my_nickname());
		qcs_msgset(m, QCS_DST, sess_name(s));
		break;
	case SESSTYPE_CHANNEL:
		m->msg = open ? QCS_MSG_CHANNEL_JOIN: QCS_MSG_CHANNEL_LEAVE;
		m->umode = net_qcs_mode(my_mode());
		qcs_msgset(m, QCS_SRC, my_nickname());
		qcs_msgset(m, QCS_CHAN, sess_name(s));
		break;
	default:
		/* ignore status & etc open/close */
		break;
	}
	if(m->msg!=QCS_MSG_INVALID) {
		net_send(m);
	}

	qcs_deletemsg(m);
}

static void
handle_channel_topic_enter(
	sess_id session,
	const char * new_topic)
{
	qcs_msg * m;

	g_assert(session && new_topic);

	/* send message */
	m = qcs_newmsg();
	m->msg = QCS_MSG_TOPIC_CHANGE;
	qcs_msgset(m, QCS_CHAN, net_type==NET_TYPE_VYPRESS ? sess_name(session): "Main");
	qcs_msgset(m, QCS_TEXT, new_topic);
	net_send(m);
	qcs_deletemsg(m);

	/* update topics for all the channels, if we are on qchat network */
	if(net_type==NET_TYPE_QCHAT) {
		raise_event(EVENT_NET_MSG_TOPIC_CHANGE, (void*)new_topic, 0);
	}
}

static void
net_send_message_to(gpointer dst_user, const char * text)
{
	qcs_msg * m;

	g_assert(dst_user && text);

	m = qcs_newmsg();
	m->msg = QCS_MSG_MESSAGE_SEND;
	qcs_msgset(m, QCS_SRC, my_nickname());
	qcs_msgset(m, QCS_DST, user_name_of(dst_user));
	qcs_msgset(m, QCS_TEXT, text);

	net_send(m);
	qcs_deletemsg(m);
}

/** net_send_chan_req
  *	supp func to send USER_CHANLIST_REQ
  */
static void
net_send_chan_req()
{
	qcs_msg * m;

	m = qcs_newmsg();
	m->msg = QCS_MSG_CHANLIST_REQUEST;
	qcs_msgset(m, QCS_SRC, my_nickname());
	net_send(m);
	qcs_deletemsg(m);
}

/** net_send_info_req:
  *	supp funct to send USER_INFO_REQ
  */
static void
net_send_info_req(gpointer dst_user)
{
	qcs_msg * m;
	
	g_assert(dst_user);

	m = qcs_newmsg();
	m->msg = QCS_MSG_INFO_REQUEST;
	qcs_msgset(m, QCS_SRC, my_nickname());
	qcs_msgset(m, QCS_DST, user_name_of(dst_user));

	net_send(m);
	qcs_deletemsg(m);
}

/* net_send_beep:
 *	sends BEEP to the specified user
 */
static void
net_send_beep(gpointer dst_user)
{
	qcs_msg * m = qcs_newmsg();
	m->msg = QCS_MSG_BEEP_SEND;
	qcs_msgset(m, QCS_SRC, my_nickname());
	qcs_msgset(m, QCS_DST, user_name_of(dst_user));

	net_send(m);
	qcs_deletemsg(m);
}


/** net_send_refresh_req:
  *	supp func to send REFRESH_REQ
  */
static void
net_send_refresh_req()
{
	qcs_msg * m = qcs_newmsg();
	m->msg = QCS_MSG_REFRESH_REQUEST;
	qcs_msgset(m, QCS_SRC, my_nickname());

	net_send(m);
	qcs_deletemsg(m);
}

/** net_send_active_change
 *	notify network if our client gui is not active
 *	(minimized, not-on-top, hidden, etc)
 */
static void
net_send_active_change(gboolean is_active)
{
	qcs_msg * m = qcs_newmsg();
	m->msg = QCS_MSG_ACTIVE_CHANGE;
	m->uactive = is_active;
	qcs_msgset(m, QCS_SRC, my_nickname());

	net_send(m);
	qcs_deletemsg(m);
}

static gboolean
net_prefs_type_validator(const gchar * prefs_name, gpointer user_data)
{
	return prefs_int(prefs_name) < NET_TYPE_NUM;
}

static gboolean
net_prefs_port_validator(const gchar * prefs_name, gpointer user_data)
{
	return prefs_int(prefs_name) <= G_MAXUINT16;
}

static gboolean
net_prefs_encoding_validator(const gchar * prefs_name, gpointer user_data)
{
	gchar * enc_test;
	const gchar * new_encoding = prefs_str(prefs_name);

	enc_test = g_convert("a", -1, "UTF-8", new_encoding, NULL, NULL, NULL);
	if(!enc_test) {
		log_ferror(NULL, g_strdup_printf(
			_("Charset encoding \"%s\" is invalid or not supported on this system"),
			new_encoding));
		return FALSE;
	}
	g_free(enc_test);
	return TRUE;
}

static void
net_prefs_main_nickname_changed_cb(const gchar * prefs_name)
{
	qcs_msg * m;

	if(my_mode()!=UMODE_INVISIBLE) {
		/* we've been started & online: send MSG_RENAME */
		m = qcs_newmsg();
		m->msg = QCS_MSG_RENAME;
		qcs_msgset(m, QCS_SRC, old_nickname->str);
		qcs_msgset(m, QCS_TEXT, my_nickname());
		net_send(m);
		qcs_deletemsg(m);
	}

	/* preserve old nickname */
	g_string_assign(old_nickname, my_nickname());
}

static void
net_prefs_main_mode_changed_cb(const gchar * prefs_name)
{
	qcs_msg * m;

	/* send MSG_MODE_CHANGE to network */
	if(my_mode()!=UMODE_INVISIBLE) {
		m = qcs_newmsg();
		m->msg = QCS_MSG_MODE_CHANGE;
		m->umode = net_qcs_mode(my_mode());
		qcs_msgset(m, QCS_SRC, my_nickname());
		net_send(m);
		qcs_deletemsg(m);
	}
}

static void
net_prefs_net_settings_changed_cb(const gchar * prefs_name)
{
	if(app_status()==APP_RUNNING) {
		gboolean do_reconnect;

		if(!strcmp(prefs_name, PREFS_NET_TYPE))
			do_reconnect = 1;
		else if(!strcmp(prefs_name, PREFS_NET_PORT))
			do_reconnect = 1;
		else if(!strcmp(prefs_name, PREFS_NET_USE_MULTICAST))
			do_reconnect = prefs_int(PREFS_NET_TYPE)==NET_TYPE_VYPRESS;
		else if(!strcmp(prefs_name, PREFS_NET_MULTICAST_ADDR))
			do_reconnect =
				prefs_int(PREFS_NET_TYPE)==NET_TYPE_VYPRESS
				&& prefs_bool(PREFS_NET_USE_MULTICAST);
		else if(!strcmp(prefs_name, PREFS_NET_BROADCAST_MASK))
			do_reconnect =
				prefs_int(PREFS_NET_TYPE)!=NET_TYPE_VYPRESS
				|| !prefs_bool(PREFS_NET_USE_MULTICAST);
		else
			do_reconnect = 0;
					
		if(do_reconnect) {
			net_disconnect();
			net_connect();
		}
	}
}

static void
net_prefs_net_is_configured_changed_cb(const gchar * prefs_name)
{
	if(prefs_bool(PREFS_NET_IS_CONFIGURED)
			&& !net_connected()
			&& app_status()==APP_RUNNING)
		net_connect();
}

static void
net_register_prefs()
{
	prefs_register(PREFS_NET_TYPE,		PREFS_TYPE_UINT,
		_("Network type"), net_prefs_type_validator, NULL);
	prefs_register(PREFS_NET_PORT,		PREFS_TYPE_UINT,
		_("Network port"), net_prefs_port_validator, NULL);
	prefs_register(PREFS_NET_IS_CONFIGURED,	PREFS_TYPE_BOOL,
		_("Network is configured"), NULL, NULL);
	prefs_register(PREFS_NET_BROADCAST_MASK, PREFS_TYPE_UINT,
		_("Network broadcast mask"), NULL, NULL);
	prefs_register(PREFS_NET_USE_MULTICAST, PREFS_TYPE_BOOL,
		_("Use multicast networking"), NULL, NULL);
	prefs_register(PREFS_NET_MULTICAST_ADDR, PREFS_TYPE_UINT,
		_("Network multicast address"), NULL, NULL);
	prefs_register(PREFS_NET_VERBOSE,	PREFS_TYPE_BOOL, 
		_("Verbosely report network events"), NULL, NULL);
	prefs_register(PREFS_NET_CHANNEL_NOTIFY,PREFS_TYPE_BOOL,
		_("Notify channel users on join/leave"), NULL, NULL);
	prefs_register(PREFS_NET_IGNORE_MASS_MSG,PREFS_TYPE_BOOL,
		_("Ignore mass messages"), NULL, NULL);
	prefs_register(PREFS_NET_ENCODING,	PREFS_TYPE_STR,
		_("Network charset encoding"), net_prefs_encoding_validator, NULL);
	prefs_register(PREFS_NET_REPLY_INFO_REQ,PREFS_TYPE_BOOL, 
		_("Reply to user information requests"), NULL, NULL);
	prefs_register(PREFS_NET_MOTD,		PREFS_TYPE_STR, 
		_("User information text"), NULL, NULL);
	prefs_register(PREFS_NET_MOTD_WHEN_AWAY,PREFS_TYPE_STR,
		_("User information text when away"), NULL, NULL);
	prefs_register(PREFS_NET_IGNORED_USERS, PREFS_TYPE_LIST,
		_("Ignored users"), NULL, NULL);

	prefs_add_notifier(PREFS_MAIN_NICKNAME, (GHookFunc)net_prefs_main_nickname_changed_cb);
	prefs_add_notifier(PREFS_MAIN_MODE, (GHookFunc)net_prefs_main_mode_changed_cb);

	prefs_add_notifier(PREFS_NET_PORT, (GHookFunc)net_prefs_net_settings_changed_cb);
	prefs_add_notifier(PREFS_NET_TYPE, (GHookFunc)net_prefs_net_settings_changed_cb);
	prefs_add_notifier(PREFS_NET_BROADCAST_MASK, (GHookFunc)net_prefs_net_settings_changed_cb);
	prefs_add_notifier(PREFS_NET_MULTICAST_ADDR, (GHookFunc)net_prefs_net_settings_changed_cb);
	prefs_add_notifier(PREFS_NET_USE_MULTICAST, (GHookFunc)net_prefs_net_settings_changed_cb);

	prefs_add_notifier(PREFS_NET_IS_CONFIGURED, (GHookFunc)net_prefs_net_is_configured_changed_cb);
}

static void
net_preset_prefs()
{
	/* set default values */
	prefs_set(PREFS_NET_CHANNEL_NOTIFY, TRUE);
	prefs_set(PREFS_NET_ENCODING, NET_DEFAULT_CHARSET);
	prefs_set(PREFS_NET_REPLY_INFO_REQ, TRUE);
	prefs_set(PREFS_NET_MOTD_WHEN_AWAY, _("User is away"));
	prefs_set(PREFS_NET_BROADCAST_MASK, 0xffffffff);	/* 255.255.255.255 */
	prefs_set(PREFS_NET_MULTICAST_ADDR, 0xe3000002);	/* 227.0.0.2 */
}

static void
net_event_cb(enum app_event_enum e, gpointer p, int i)
{
	switch(e) {
	case EVENT_MAIN_INIT:
		net = NULL;
		net_event_source = 0;
		old_nickname = g_string_new(NULL);
		break;

	case EVENT_MAIN_REGISTER_PREFS:
		net_register_prefs();
		break;

	case EVENT_MAIN_PRESET_PREFS:
		net_preset_prefs();
		break;

	case EVENT_MAIN_START:
		/* gui_netselect_dlg should popup it's dialog
		 * if the network is not configured
		 */
		if(prefs_bool(PREFS_NET_IS_CONFIGURED))
			net_connect();
		break;

	case EVENT_MAIN_PRECLOSE:
		net_disconnect();
		break;

	case EVENT_MAIN_CLOSE:
		g_string_free(old_nickname, TRUE);
		old_nickname = NULL;
		break;

	case EVENT_SESSION_OPENED:
	case EVENT_SESSION_CLOSED:
		net_handle_session_open_close(
			(sess_id)p,
			e==EVENT_SESSION_OPENED);
		break;

	case EVENT_SESSION_TOPIC_ENTER:
		if(sess_type(EVENT_V(p, 0))==SESSTYPE_CHANNEL)
			handle_channel_topic_enter(EVENT_V(p,0), EVENT_V(p,1));
		break;

	case EVENT_CMDPROC_SESSION_TEXT:
		net_handle_my_text((const char *)p, i);
		break;
	case EVENT_MESSAGE_SEND:
		net_send_message_to(EVENT_V(p,0), (const char *)EVENT_V(p,1));
		break;
	case EVENT_USER_NET_UPDATE_REQ:
		net_send_refresh_req();
		break;
	case EVENT_IFACE_USER_INFO_REQ:
		net_send_info_req(p);
		break;
	case EVENT_IFACE_USER_BEEP_REQ:
		net_send_beep(p);
		break;
	case EVENT_IFACE_REQUEST_NET_CHANNELS:
		net_send_chan_req();
		break;
	case EVENT_IFACE_ACTIVE_CHANGE:
		net_send_active_change((gboolean)i);
		break;
	default:
		break;	/* nothing else */
	}
}

static gboolean
net_connect()
{
	GIOChannel * channel;
	int rx_socket;
	enum net_type_enum type = prefs_int(PREFS_NET_TYPE);
	unsigned short port = prefs_int(PREFS_NET_PORT);

	g_assert(net==NULL);

	/* check network type num */
	if(type>=NET_TYPE_NUM) {
		log_error(_(LOG_NET), ("invalid network type specified: falling back to QuickChat"));
		type = NET_TYPE_QCHAT;
	}

	/* notify everyone that we're about to connect to the network */
	raise_event(EVENT_NET_PRECONNECT, 0, 0);

	net_type = type;

	/* do the real connecting */
	if(type==QCS_PROTO_VYPRESS) {
		int use_multicast = prefs_bool(PREFS_NET_USE_MULTICAST);
		net = qcs_open(
			QCS_PROTO_VYPRESS,
			use_multicast ? QCS_PROTO_OPT_MULTICAST: 0,
			use_multicast
				? prefs_int(PREFS_NET_MULTICAST_ADDR)
				: prefs_int(PREFS_NET_BROADCAST_MASK),
			port);
	} else {
		net = qcs_open(QCS_PROTO_QCHAT, 0, prefs_int(PREFS_NET_BROADCAST_MASK), port);
	}

	/* check for errors */
	if(!net) {
		log_ferror(_(LOG_NET), g_strdup_printf(
			_("failed to connect to %s network on port %hd: %s"),
				net_name_of_type(type), port, strerror(errno)));
		log_error(_(LOG_NET),
			_("check if there are any programs running on the same port"
				" and your firewall settings"));
		return FALSE;
	}

	/* install glib event handler on socket */
	qcs_rxsocket(net, &rx_socket);
	channel = g_io_channel_unix_new(rx_socket);
	net_event_source = g_io_add_watch(
			channel,
			G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
			net_io_func, 0);
	g_io_channel_unref(channel);

	log_fevent(_(LOG_NET), g_strdup_printf(
		_("connected to %s network on port %hd"), net_name_of_type(type), port));

	/* notify that we've connected */
	raise_event(EVENT_NET_CONNECTED, 0,0);

	return TRUE;
}

static void
net_disconnect()
{
	if(net) {
		/* notify everyone of our evil intentions */
		raise_event(EVENT_NET_PREDISCONNECT, 0,0);

		/* remove io_chan */
		g_source_remove(net_event_source);
		net_event_source = 0;

		/* close link */
		qcs_close(net);
		net = NULL;

		raise_event(EVENT_NET_DISCONNECTED, 0,0);
	}
}

/** exported routines
  *************************/
void net_register()
{
	register_event_cb(
		net_event_cb,
		EVENT_MAIN | EVENT_CMDPROC | EVENT_IFACE
		| EVENT_SESSION | EVENT_MESSAGE | EVENT_USER);
}

gboolean net_connected()
{
	return net!=NULL;
}

const gchar *
net_name_of_type(enum net_type_enum t)
{
	switch(t) {
	case NET_TYPE_QCHAT:	return "quickChat";
	case NET_TYPE_VYPRESS:	return "Vypress Chat";
	default:
		break;	/* nothing else */
	}
	return "(invalid network type)";
}
