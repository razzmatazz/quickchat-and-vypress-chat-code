/*
 * main.h: application initialization, misc funcs
 * Copyright (C) 2002,2004 Saulius Menkevicius
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
 * $Id: main.h,v 1.36 2005/01/03 01:20:22 bobas Exp $
 */

#ifndef MAIN_H__
#define MAIN_H__

#ifdef HAVE_CONFIG_H
 #include <config.h>
#endif

#ifdef ENABLE_NLS
 #include <libintl.h>

 #define _(text) gettext(text)
 #define N_(text) (text)
#else
 #define _(text) (text)
 #define N_(text) (text)
#endif

#define MAX_TOPIC_LENGTH	1023
#define MAX_TEXT_LENGTH		1023
#define MAX_NICK_LENGTH		64
#define MAX_CHAN_LENGTH		64

#define DEFAULT_SESSION_SIZE	256

/* preferences registered in main module
 */
#define PREFS_MAIN_NICKNAME	"main/nickname"
#define PREFS_MAIN_MODE		"main/mode"
#define PREFS_MAIN_LOG_GLOBAL	"main/log_global"
#define PREFS_MAIN_PERSISTENT_CHANNELS "main/persistent_channels"
#define PREFS_MAIN_POPUP_TIMEOUT "main/popup_timeout"

enum app_status_enum {
	APP_INIT,
	APP_START,
	APP_RUNNING,
	APP_PRECLOSE,
	APP_CLOSE
};
enum app_status_enum app_status();

#define EVENT_V(p, i) (((gpointer*)p)[i])
#define APP_EVENT_V(p, i) EVENT_V(p, i)	/* XXX: deprecated */

enum app_event_enum {
	EVENT_NULL,
	EVENT_MAIN	= 0x00100,
	EVENT_MAIN_INIT,
	EVENT_MAIN_REGISTER_PREFS,
	EVENT_MAIN_PRESET_PREFS,
	EVENT_MAIN_LOAD_PREFS,
	EVENT_MAIN_START,
	EVENT_MAIN_PRECLOSE,
	EVENT_MAIN_CLOSE,

	EVENT_IFACE	= 0x00200,
	EVENT_IFACE_EXIT,
	EVENT_IFACE_SHOW_CONFIGURE_DLG,
	EVENT_IFACE_SHOW_IGNORE_DLG,
	EVENT_IFACE_SHOW_TOPIC_DLG,	/* p = sess_id of session to set topic of */
	EVENT_IFACE_SHOW_NETDETECT_DLG,
	EVENT_IFACE_SHOW_CHANNEL_DLG,
	EVENT_IFACE_SHOW_ABOUT_DLG,
	EVENT_IFACE_TOPIC_ENTER,	/* p = {sess_id, text} */
	EVENT_IFACE_TEXT_ENTER,
	EVENT_IFACE_HISTORY,
	EVENT_IFACE_NICKNAME_ENTERED,
	EVENT_IFACE_PAGE_SWITCH,
	EVENT_IFACE_PAGE_CLOSE,		/* p = page_id(eq sess_id); (or NULL, if active page) */
	EVENT_IFACE_PAGE_SCROLL,
	EVENT_IFACE_USER_INFO_REQ,	/* p = user_id */
	EVENT_IFACE_USER_MESSAGE_REQ,
	EVENT_IFACE_USER_BEEP_REQ,
	EVENT_IFACE_USER_OPEN_REQ,
	EVENT_IFACE_USER_CLOSE_REQ,
	EVENT_IFACE_USER_IGNORE_REQ,
	EVENT_IFACE_USER_UNIGNORE_REQ,
	EVENT_IFACE_USER_REMOVE_REQ,
	EVENT_IFACE_USER_LIST_REFRESH_REQ,
	EVENT_IFACE_IGNORE_ADD,		/* p=(const char *)nickname */
	EVENT_IFACE_IGNORE_REMOVE,	/* p=(const char *)nickname */
	EVENT_IFACE_JOIN_CHANNEL,	/* p=(const char *)channel */
	EVENT_IFACE_REQUEST_NET_CHANNELS,
	EVENT_IFACE_RELOAD_CONFIG,
	EVENT_IFACE_STORE_CONFIG,
	EVENT_IFACE_MAINWND_SHOWN,
	EVENT_IFACE_MAINWND_HIDDEN,
	EVENT_IFACE_ACTIVE_CHANGE,	/* i="is_active" */
	EVENT_IFACE_PAGE_RELEASE_FOCUS,	/* invoked when page widget wants to release focus */
	EVENT_IFACE_TRAY_CLICK,
	EVENT_IFACE_TRAY_UMODE,		/* p=(enum user_mode_enum) */
	EVENT_IFACE_TRAY_EMBEDDED,
	EVENT_IFACE_TRAY_REMOVED,
	/* requests to the i-iface */
	EVENT_IFACE_REQ_PRESENT,	/* present the main window */

	EVENT_NET	= 0x00400,
	EVENT_NET_PRECONNECT,
	EVENT_NET_CONNECTED,
	EVENT_NET_PREDISCONNECT,
	EVENT_NET_DISCONNECTED,
	EVENT_NET_ERROR,		/* i(errno) */
	EVENT_NET_NEW_USER,		/* p[]={(char*)username, usermode, found_via_pong} */
	EVENT_NET_MSG_REFRESH_REQ,	/* (uid)p */
	EVENT_NET_MSG_REFRESH_ACK,	/* p[]={uid,(int)is_active}, (usermode)i */
	EVENT_NET_MSG_TOPIC_CHANGE,	/* p (topic): topic of all channels */
	EVENT_NET_MSG_TOPIC_CHANGE_4,	/* p[]={channel, topic} */
	EVENT_NET_MSG_MESSAGE,		/* p[]={uid, msg, is_mass_message} */
	EVENT_NET_MSG_MESSAGE_ACK,	/* p[]={uid, ack_msg}, i(mode)*/
	EVENT_NET_MSG_RENAME,		/* p[]={uid, new_name} */
	EVENT_NET_MSG_MODE_CHANGE,	/* (uid)p; (mode)i */
	EVENT_NET_MSG_ACTIVE_CHANGE,	/* (uid)p; i="is_active" */
	EVENT_NET_MSG_CHANNEL_JOIN,	/* p[]={uid, chan} */
	EVENT_NET_MSG_CHANNEL_LEAVE,	/* p[]={uid, chan} */
	EVENT_NET_MSG_CHANNEL_TEXT,	/* p[]={uid, chan, text}, i(is /me) */
	EVENT_NET_MSG_PRIVATE_OPEN,	/* (uid)p */
	EVENT_NET_MSG_PRIVATE_CLOSE,	/* (uid)p */
	EVENT_NET_MSG_PRIVATE_TEXT,	/* p[]={uid, text}, i(is /me) */
	EVENT_NET_MSG_INFO_REPLY,	/* p[]={uid; login; (GList*)channels; motd; src_ip} */
	EVENT_NET_MSG_BEEP_SEND,	/* (uid)p */
	EVENT_NET_MSG_BEEP_ACK,		/* (uid)p */
	EVENT_NET_MSG_CHANNEL_LIST,	/* p = (GList*)channels */

	EVENT_USER	= 0x00800,
	EVENT_USER_NEW,			/* (enum user_mode)i,
					   p[]={"user_id", nickname, (int)is_active} */
	EVENT_USER_REMOVED,		/* p="user_id" */
	EVENT_USER_MODE_CHANGE,		/* (enum user_mode)i="prev_mode", (user_id)p */
	EVENT_USER_ACTIVE_CHANGE,	/* i="is_active", (user_id)p */
	EVENT_USER_RENAME,		/* p[]={uid, previous_nick} */
	EVENT_USER_NET_UPDATE_REQ,

	EVENT_SESSION	= 0x01000,
	EVENT_SESSION_OPENED,		/* (enum session_type)i, (sess_id)p */
	EVENT_SESSION_CLOSED,		/* (enum session_type)i, (sess_id)p */
	EVENT_SESSION_RENAMED,		/* p[]={sess_id, new_name} */
	EVENT_SESSION_TOPIC_CHANGED,	/* p[]={sess_id, new_topic}; i = (gboolean): TRUE if readonly */
	EVENT_SESSION_TOPIC_ENTER,	/* p[]={sess_id, new_topic}; */
	EVENT_SESSION_TEXT,		/* p[]=(sess_id, text); i = (session_text_type) */

	EVENT_PREFS = 0x02000,
	EVENT_PREFS_CHANGED,		/* (const gchar * prefs_name)p */
	EVENT_PREFS_SAVED,

	EVENT_CMDPROC	= 0x04000,
	EVENT_CMDPROC_SET_TEXT,
	EVENT_CMDPROC_SESSION_TEXT,	/* (boolean)int = me-text	*/

	EVENT_MESSAGE	= 0x08000,
	EVENT_MESSAGE_SEND,		/* p={uid, text}		*/

	EVENT_IDLE	= 0x10000,
	EVENT_IDLE_AUTO_AWAY,
	EVENT_IDLE_AUTO_OFFLINE,
	EVENT_IDLE_NOT
};

typedef void (*app_event_cb)(enum app_event_enum, void *, int);

void register_event_cb(app_event_cb, enum app_event_enum mask);
void unregister_event_cb(app_event_cb);
void raise_event(enum app_event_enum, void *, int);

void log_event_withfree(const gchar *, gchar *, int is_error);
void log_event_nofree(const gchar *, const gchar *, int is_error);

/* !!!! log_f* funcs free the char * suplied !!!! */
#define log_ferror(d,s)	log_event_withfree(d,s,1)
#define log_fevent(d,s)	log_event_withfree(d,s,0)
#define log_error(d,s)	log_event_nofree(d,s,1)
#define log_event(d,s)	log_event_nofree(d,s,0)

#define my_nickname()	prefs_str(PREFS_MAIN_NICKNAME)
#define set_nickname(s)	prefs_set(PREFS_MAIN_NICKNAME, s)
#define my_mode()	(enum user_mode_enum)prefs_int(PREFS_MAIN_MODE)
#define set_mode(m)	prefs_set(PREFS_MODE, (int)m)
#define IS_MESSAGEABLE_MODE(m) (m==UMODE_NORMAL || m==UMODE_AWAY || m==UMODE_DND)
GList * my_channels();

/**
 * channel and nickname compare/validation routines (utf8)
 */
#define nickname_cmp(n1, n2) g_utf8_collate((n1), (n2))
int nickname_valid(const char *);

int channel_cmp(const char *, const char *);
int channel_valid(const char *);

/* you have to g_free this string after use */
gchar * generate_timestamp();

#endif /* #ifndef MAIN_H__ */

