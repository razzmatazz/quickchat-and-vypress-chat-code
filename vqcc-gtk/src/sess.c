/*
 * sess.c: session management
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
 * $Id: sess.c,v 1.30 2004/12/24 03:16:04 bobas Exp $
 */

#include <stdarg.h>
#include <string.h>
#include <glib.h>

#include "main.h"
#include "prefs.h"
#include "sess.h"
#include "gui_page.h"
#include "net.h"
#include "user.h"

/* session_t:
 *	describes opened session
 *******************************/

typedef struct session_struct {
	gboolean closeable;
	char * name;
	char * topic;
	gboolean topic_readonly;
	enum session_type type;
	gboolean hilited;
	gpointer uid;
} session_t;
#define PSESSION_T(s) ((session_t*)s)

GList * sess_list;

#define CURRENT_SESSION() PSESSION_T(gui_page_current())

#define SESSION_TOPIC_READONLY(s) ((s)->type==SESSTYPE_STATUS || (s)->type==SESSTYPE_PRIVATE)

/** static routines
 *************************************************/

static void
delete_all_sessions()
{
	while(sess_list) {
		sess_delete((sess_id)sess_list->data);
	}
}

static void
delete_all_sessions_enum_cb(
	sess_id sess, enum session_type type,
	const char * name, void * uid,
	void * type_to_delete)
{
	if((enum session_type)GPOINTER_TO_INT(type_to_delete)==type) {
		sess_delete(sess);
	}
}

static void
delete_all_sessions_of_type(enum session_type type)
{
	sess_enumerate(delete_all_sessions_enum_cb, GINT_TO_POINTER(type));
}

static gboolean
sess_set_topic(session_t * session, const gchar * topic)
{
	gpointer v[2];

	g_assert(session && session->topic && topic);

	if(!strcmp(session->topic, topic)) {
		/* not changed */
		return FALSE;
	}

	/* update session topic text */
	g_free(session->topic);
	session->topic = g_strdup(topic);

	if(session->type==SESSTYPE_CHANNEL) {
		/* write the new topic onto the channel page */
		sess_write(session, SESSTEXT_TOPIC, _("Topic: \"%s\""), topic);
	}

	/* notify session topic change */
	EVENT_V(v, 0) = (gpointer)session;
	EVENT_V(v, 1) = (gpointer)topic;
	raise_event(EVENT_SESSION_TOPIC_CHANGED, v, SESSION_TOPIC_READONLY(session));

	return TRUE;	/* topic was changed */
}

/**
 * set_topic_for_all_channels:
 *	sets the same topic for all channels
 *	(used on Quickchat network, where there's a single topic for all channels)
 */
static void
set_topic_for_all_channels(const gchar * topic)
{
	GList * l;

	g_assert(topic);

	for(l=sess_list; l; l=l->next) {
		if(PSESSION_T(l->data)->type==SESSTYPE_CHANNEL) {
			sess_set_topic(PSESSION_T(l->data), topic);
		}
	}
}

/** emit_user_rename:
  *	write `xxx has renamed in yyy' message to all channels
  */
static void
emit_user_rename_find_cb(
	sess_id sid, enum session_type type,
	const gchar * name, gpointer user, gpointer cbdata)
{
#define D_UID(d) ((gpointer*)d)[0]
#define D_PREV_NICK(d) ((const char *)((gpointer*)d)[1])

	if(type==SESSTYPE_CHANNEL) {
		/* emit info to session page */
		sess_write(sid, SESSTEXT_NOTIFY, "%s has renamed in %s",
				D_PREV_NICK(cbdata), user_name_of(D_UID(cbdata)));
	}

#undef D_UID
#undef D_NEW_NICK
}

static void
emit_user_rename(gpointer user, const char * previous_nickname)
{
	gpointer data[] = { user, (gpointer)previous_nickname };

	if(prefs_bool(PREFS_SESS_SUPPRESS_RENAME_TEXT))
		return;

	sess_enumerate(emit_user_rename_find_cb, data);
}

/*
 * updates SESSTYPE_PRIVATE session name for specified user,
 * if any; emits rename info to all the channels
 */
static void
handle_user_rename(
	gpointer user_id, gpointer prev_nickname)
{
	gpointer v[2];
	session_t * session;

	/* find private channel with the user */
	session = (session_t*) sess_find(SESSTYPE_PRIVATE, user_name_of(user_id));
	if(session) {
		/* rename session */
		g_free(session->name);
		session->name = g_strdup(user_name_of(user_id));

		/* raise event about session name change */
		EVENT_V(v, 0) = (gpointer)session;
		EVENT_V(v, 1) = (gpointer)user_name_of(user_id);
		raise_event(EVENT_SESSION_RENAMED, v, 0);
	}
	
	/* emit 'xx has renamed in yy' to all the channels */
	emit_user_rename(user_id, prev_nickname);
}

/** handle_user_mode_change:
  *	write `xxx has changed mode to mmm' message to all channels
  * and private with xxx/yyy, if present
  */
static void
emit_user_mode_change_find_cb(
	sess_id sess, enum session_type type,
	const gchar * name, gpointer sess_user, gpointer user)
{
	if(type==SESSTYPE_CHANNEL || (type==SESSTYPE_PRIVATE && sess_user==user)) {
		/* emit info to session page */
		sess_write(sess, SESSTEXT_NOTIFY,
			_("%s has changed mode to %s"),
			user_name_of(user), user_mode_name(user_mode_of(user)));
	}
}

static void
handle_user_mode_change(gpointer user, enum user_mode_enum prev_mode)
{
	if(prefs_bool(PREFS_SESS_SUPPRESS_MODE_TEXT))
		return;

	if(prev_mode!=UMODE_DEAD && user_mode_of(user)!=UMODE_DEAD) {
		sess_enumerate(emit_user_mode_change_find_cb, user);
	}
}

static void
sess_handle_my_text(const char * text, int me_text)
{
	session_t * s;

	g_assert(text);

	s = CURRENT_SESSION();

	/* check that it's channel or private */
	if(s->type!=SESSTYPE_CHANNEL
		&& s->type!=SESSTYPE_PRIVATE) return;

	/* write this thing out */
	if(!me_text) {
		sess_write(s, SESSTEXT_MY_TEXT, "<%n> %s", my_nickname(), text);
	} else {
		sess_write(s, SESSTEXT_MY_ME, "%n %s", my_nickname(), text);
	}
}

static void
sess_private_open_req(gpointer user)
{
	session_t * s;

	g_return_if_fail(user);

	/* check if the session is not already open */
	s = sess_new(SESSTYPE_PRIVATE, user_name_of(user), TRUE, user);
	if(!s) {
		/* seems we have such a session opened already */
		s = PSESSION_T( sess_find(SESSTYPE_PRIVATE, user_name_of(user)) );
	}

	/* switch to that one */
	sess_switch_to(s);
}

/* sess_net_event:
 *	handles EVENT_NET_* messages
 */
static void
sess_net_event(enum app_event_enum e, gpointer p, gint i)
{
	session_t * s;

	switch(e) {
	case EVENT_NET_PREDISCONNECT:
		/* we're going to be disconnected:
		 *	close all the channels & chat pages
		 */
		delete_all_sessions_of_type(SESSTYPE_CHANNEL);
		delete_all_sessions_of_type(SESSTYPE_PRIVATE);
		break;
		
	case EVENT_NET_MSG_TOPIC_CHANGE_4:
		/* topic change for specific channel */
		if((s = sess_find(SESSTYPE_CHANNEL, EVENT_V(p, 0))) != NULL) {
			sess_set_topic(s, EVENT_V(p, 1));
		}
		break;

	case EVENT_NET_MSG_TOPIC_CHANGE:
		/*
		 * handles topic change for all channels
		 * (theres a single topic for all channels on quickchat network)
		 */
		set_topic_for_all_channels((const gchar*)p);
		break;

	case EVENT_NET_MSG_CHANNEL_JOIN:
	case EVENT_NET_MSG_CHANNEL_LEAVE:
		if(prefs_bool(PREFS_SESS_SUPPRESS_JOIN_LEAVE_TEXT))
			break;

		s = sess_find(SESSTYPE_CHANNEL, EVENT_V(p, 1));
		if(s) {
			sess_write(s, SESSTEXT_NOTIFY,
				e==EVENT_NET_MSG_CHANNEL_JOIN
					? _("%s joins %h"): _("%s left %h"),
				user_name_of(EVENT_V(p, 0)), EVENT_V(p, 1));
		}
		break;

	case EVENT_NET_MSG_CHANNEL_TEXT:
		s = sess_find(SESSTYPE_CHANNEL, EVENT_V(p, 1));
		if(s) {
			if(i) {
				sess_write(s, SESSTEXT_THEIR_ME, "%n %s",
					user_name_of(EVENT_V(p, 0)), (const gchar*)EVENT_V(p, 2));
			} else {
				sess_write(s, SESSTEXT_THEIR_TEXT, "<%n> %s",
					user_name_of(EVENT_V(p, 0)), (const gchar*)EVENT_V(p, 2));
			}
		}
		break;

	case EVENT_NET_MSG_PRIVATE_OPEN:
		s = sess_new(SESSTYPE_PRIVATE, user_name_of(p), TRUE,p);
		if(!s) {
			s = sess_find(SESSTYPE_PRIVATE, user_name_of(p));
		}
		sess_write(s, SESSTEXT_JOIN, _("%s has opened private chat"), user_name_of(p));
		break;

	case EVENT_NET_MSG_PRIVATE_CLOSE:
		s = sess_find(SESSTYPE_PRIVATE, user_name_of(p));
		if(s) {
			sess_write(s, SESSTEXT_LEAVE, _("%s has closed private chat"), user_name_of(p));
		}
		break;

	case EVENT_NET_MSG_PRIVATE_TEXT:
		s = sess_new(SESSTYPE_PRIVATE, user_name_of(EVENT_V(p, 0)), TRUE, EVENT_V(p, 0));
		if(!s) {
			s = sess_find(SESSTYPE_PRIVATE, user_name_of(EVENT_V(p, 0)));
		}

		if(i) {
			sess_write(s, SESSTEXT_THEIR_ME, "%n %s",
				user_name_of(EVENT_V(p, 0)), (const gchar*)EVENT_V(p, 1));
		} else {
			sess_write(s, SESSTEXT_THEIR_TEXT, "<%n> %s",
				user_name_of(EVENT_V(p, 0)), (const gchar*)EVENT_V(p, 1));
		}
		break;

	default:
		break;
	}
}

static void
sess_event_cb(enum app_event_enum event, gpointer p, int i)
{
	session_t *sid;
	gpointer event_v[2];

	if(event & EVENT_NET) {
		sess_net_event(event, p, i);
		return;
	}

	switch(event) {
	case EVENT_MAIN_INIT:
		sess_list = NULL;
		break;

	case EVENT_MAIN_REGISTER_PREFS:
		prefs_register(
			PREFS_SESS_STAMP_CHANNEL, PREFS_TYPE_BOOL,
			_("Time-stamp channel text"), NULL, NULL);
		prefs_register(
			PREFS_SESS_STAMP_PRIVATE, PREFS_TYPE_BOOL,
			_("Time-stamp private text"), NULL, NULL);
		prefs_register(
			PREFS_SESS_STAMP_STATUS, PREFS_TYPE_BOOL,
			_("Time-stamp status text"), NULL, NULL);
		prefs_register(
			PREFS_SESS_IMPORTANT_CHANNELS, PREFS_TYPE_LIST,
			_("List of channels that trigger popup on new text"), NULL, NULL);
		prefs_register(
			PREFS_SESS_SUPPRESS_RENAME_TEXT, PREFS_TYPE_BOOL,
			_("Suppress rename messages"), NULL, NULL);
		prefs_register(
			PREFS_SESS_SUPPRESS_MODE_TEXT, PREFS_TYPE_BOOL,
			_("Suppress mode change messages"), NULL, NULL);
		prefs_register(
			PREFS_SESS_SUPPRESS_JOIN_LEAVE_TEXT, PREFS_TYPE_BOOL,
			_("Suppress join/leave messages"), NULL, NULL);
		break;

	case EVENT_MAIN_PRESET_PREFS:
		prefs_set(PREFS_SESS_STAMP_CHANNEL, TRUE);
		prefs_set(PREFS_SESS_STAMP_PRIVATE, TRUE);
		prefs_set(PREFS_SESS_STAMP_STATUS, TRUE);
		break;

	case EVENT_MAIN_PRECLOSE:
		delete_all_sessions();
		break;
	case EVENT_IFACE_PAGE_SWITCH:
		/* un-hilite page if hilited */
		if(PSESSION_T(p)->hilited) {
			gui_page_hilite(p, FALSE);
			PSESSION_T(p)->hilited = FALSE;
		}
		break;
	case EVENT_IFACE_PAGE_CLOSE:
		sid = p ? (sess_id)p: sess_current();
		if(PSESSION_T(sid)->closeable) {
			/*
			 * Delete session and it's page
			 * only if it's closeable (by the user).
			 */
			sess_delete(sid);
		}
		break;
	case EVENT_IFACE_USER_OPEN_REQ:
		sess_private_open_req(p);
		break;
	case EVENT_IFACE_TOPIC_ENTER:
		sid = EVENT_V(p, 0);
		if(!SESSION_TOPIC_READONLY(sid)) {
			/* here we don't care to set topic for ALL the channels in case of QChat,
			 * as net.c will forcefully raise EVENT_NET_MSG_TOPIC_CHANGE
			 */
			if(sess_set_topic(sid, (const gchar*)EVENT_V(p, 1))) {
				EVENT_V(event_v, 0) = sid;
				EVENT_V(event_v, 1) = EVENT_V(p, 1);
				raise_event(EVENT_SESSION_TOPIC_ENTER, event_v, 0);
			}
		}
		break;
	case EVENT_IFACE_JOIN_CHANNEL:
		sid = sess_find(SESSTYPE_CHANNEL, (const gchar *)p);
		if(!sid)
			sid = sess_new(SESSTYPE_CHANNEL, (const gchar *)p, TRUE, NULL);

		sess_switch_to(sid);
		break;

	case EVENT_CMDPROC_SESSION_TEXT:
		sess_handle_my_text((const char *)p, i);
		break;
	case EVENT_USER_RENAME:
		handle_user_rename(EVENT_V(p, 0), EVENT_V(p, 1));
		break;
	case EVENT_USER_MODE_CHANGE:
		handle_user_mode_change(p, (enum user_mode_enum)i);
		break;
	default:
		break;
	}
}

/** exported routines
 *************************************************/

void sess_register()
{
	register_event_cb(
		sess_event_cb,
		EVENT_MAIN | EVENT_IFACE | EVENT_NET | EVENT_CMDPROC | EVENT_USER);
}

sess_id
sess_new(
	enum session_type type,
	const gchar * name,
	gboolean closeable,
	gpointer user_id)
{
	session_t * s;
	gboolean first_sess;

	g_assert(name!=NULL && strlen(name));

	/* check if we have such a session already */
	if(sess_find(type, name)) return NULL;

	/* alloc and fill in session_t */
	s = g_malloc(sizeof(session_t));
	s->name = g_strdup(name);
	s->topic = g_strdup("");
	s->topic_readonly = type!=SESSTYPE_CHANNEL;
	s->type = type;
	s->hilited = FALSE;
	s->uid = type==SESSTYPE_PRIVATE ? user_id: NULL;
	s->closeable = closeable;

	/* insert into the list */
	first_sess = sess_list==NULL;
	sess_list = g_list_append(sess_list, (gpointer)s);

	/* create gui session */
	gui_page_new(
		type, name, NULL, closeable,
		DEFAULT_SESSION_SIZE, (void*)s, user_id);

	/* write out the text */
	switch(type) {
	case SESSTYPE_PRIVATE:
		sess_write(s, SESSTEXT_NOTIFY, _("Chat with %n has been opened"), name);
		break;
	case SESSTYPE_CHANNEL:
		sess_write(s, SESSTEXT_NOTIFY, _("Joined channel %h"), name);
		break;
	default:
		break;
	}

	/* inform about changes */
	raise_event(EVENT_SESSION_OPENED, (void*)s, (int)type);

	return (sess_id)s;
}

void sess_delete(sess_id s)
{
	g_assert(PSESSION_T(s));

	/* inform about delete */
	raise_event(EVENT_SESSION_CLOSED, (void*)s, (int)PSESSION_T(s)->type);

	/* delete from gui */
	gui_page_delete((void*)s);

	/* delete from list */
	sess_list = g_list_remove(sess_list, (gpointer)s);

	/* cleanup the struct */
	g_free(PSESSION_T(s)->name);
	g_free(PSESSION_T(s)->topic);
	g_free(PSESSION_T(s));
}

gpointer
sess_find(enum session_type type, const char * name)
{
	GList * l;
	for(l = sess_list; l; l = l->next) {
		g_assert(PSESSION_T(l->data)->name);
	
		if(PSESSION_T(l->data)->type==type
			&& ((type==SESSTYPE_CHANNEL
					&& !channel_cmp(PSESSION_T(l->data)->name, name))
				|| (type==SESSTYPE_PRIVATE
					&& !nickname_cmp(PSESSION_T(l->data)->name, name))
				|| type==SESSTYPE_STATUS)
			)
			return PSESSION_T(l->data);
	}
	return NULL;
}

/* sess_write:
 *	writes formatted text line onto the session page
 * args:
 *	@session	- session to write text to;
 *	@text_type	- type of the text (error msg/private text/etc)
 *	@text_fmt	- format of the text:
 *				%s - null terminated string;
 *				%c - single char;
 *				%h - channel name;
 *				%n - user nickname;
 */
void sess_write(
	sess_id session,
	enum session_text_type text_type,
	const gchar * text_fmt, ...)
{
	enum text_attr default_attr;
	gboolean prepend_stamp;
	GString * text, * entire_line;
	gchar * gentext;
	const gchar * fmt;
	gpointer event_v[2];
	va_list vargs;

	va_start(vargs, text_fmt);

	g_assert(session && text_fmt);

	entire_line = g_string_sized_new(64);

	/*
	 * check if we need to prepend a timestamp
	 */
	switch(PSESSION_T(session)->type) {
	case SESSTYPE_STATUS:
		prepend_stamp = prefs_bool(PREFS_SESS_STAMP_STATUS); break;
	case SESSTYPE_CHANNEL:
		prepend_stamp = prefs_bool(PREFS_SESS_STAMP_CHANNEL); break;
	case SESSTYPE_PRIVATE:
		prepend_stamp = prefs_bool(PREFS_SESS_STAMP_PRIVATE); break;
	}
	if(prepend_stamp) {
		gentext = generate_timestamp();
		gui_page_write(session, ATTR_TIME, gentext, FALSE);
		g_string_append(entire_line, gentext);
		g_free(gentext);
	}

	/*
	 * select default text attribute by @text_type
	 */
	switch(text_type) {
	case SESSTEXT_NORMAL:	default_attr = ATTR_NORM;	break;
	case SESSTEXT_NOTIFY:	default_attr = ATTR_NOTIFY;	break;
	case SESSTEXT_ERROR:	default_attr = ATTR_ERR;	break;
	case SESSTEXT_JOIN:	default_attr = ATTR_NOTIFY;	break;
	case SESSTEXT_LEAVE:	default_attr = ATTR_NOTIFY;	break;
	case SESSTEXT_MY_TEXT:	default_attr = ATTR_MY_TEXT;	break;
	case SESSTEXT_THEIR_TEXT:default_attr = ATTR_THEIR_TEXT;break;
	case SESSTEXT_MY_ME:	default_attr = ATTR_ME_TEXT;	break;
	case SESSTEXT_THEIR_ME:	default_attr = ATTR_ME_TEXT;	break;
	case SESSTEXT_TOPIC:	default_attr = ATTR_TOPIC;	break;
	}

	/*
	 * parse format text
	 */
	fmt = text_fmt;
	text = g_string_sized_new(128);
	while(*fmt) {
		if(*fmt=='%') {
			fmt ++;

			switch(*fmt) {
			case '%': g_string_append_c(text, '%');			break;
			case 's': g_string_append(text, va_arg(vargs, char*));	break;
			case 'c': g_string_append_c(text, (char)va_arg(vargs, int)); break;
			
			case 'h': /* FALLTROUGH */
			case 'n':
				/* special tags: need to flush the text first */
				gui_page_write(session, default_attr, text->str, FALSE);
				g_string_append(entire_line, text->str);
				g_string_assign(text, "");

				switch(*fmt) {
				case 'h':
					gentext = g_strdup_printf("#%s", va_arg(vargs, char *));

					gui_page_write(session, ATTR_CHAN, gentext, FALSE);
					g_string_append(entire_line, gentext);

					g_free(gentext);
					break;
				case 'n':
					gentext = va_arg(vargs, char *);
					gui_page_write(session, ATTR_USER, gentext, FALSE);
					g_string_append(entire_line, gentext);
					break;
				}
				break;
			default:
				/* unknown tag; emit both % and the char that succeeds it */
				g_string_append_c(text, '%');
				g_string_append_c(text, *fmt);
				break;
			}
		} else {
			/* append this char to the string */
			g_string_append_c(text, *fmt);
		}
		
		fmt ++;
	}

	/* flush the text accumulated onto the page */
	gui_page_write(session, default_attr, text->str, TRUE);
	g_string_append(entire_line, text->str);

	g_string_free(text, TRUE);

	/* emit event about this line */
	EVENT_V(event_v, 0) = session;
	EVENT_V(event_v, 1) = entire_line->str;
	raise_event(EVENT_SESSION_TEXT, event_v, (int)text_type);
	g_string_free(entire_line, TRUE);

	if(PSESSION_T(session)->type == SESSTYPE_CHANNEL
		&& prefs_list_contains(PREFS_SESS_IMPORTANT_CHANNELS, PSESSION_T(session)->name))
	{
		/* 
		 * present the main window and switch to the channel,
		 * if it is an "important" one
		 */
		raise_event(EVENT_IFACE_REQ_PRESENT, NULL, 0);
		sess_switch_to(session);
	} else {
		/*
		 * highlight page tab if not selected
		 */
		if(gui_page_current()!=session && !PSESSION_T(session)->hilited) {
			gui_page_hilite(session, TRUE);
			PSESSION_T(session)->hilited = TRUE;
		}
	}


	va_end(vargs);
}

sess_id sess_current()
{
	return (sess_id)CURRENT_SESSION();
}

enum session_type
sess_type(gpointer s)
{
	return PSESSION_T(s)->type;
}

const gchar *
sess_topic(gpointer s)
{
	return PSESSION_T(s)->topic;
}

gboolean sess_is_closeable(gpointer session)
{
	return PSESSION_T(session)->closeable;
}

gboolean 
sess_topic_readonly(gpointer session)
{
	return PSESSION_T(session)->topic_readonly;
}

const gchar * sess_name(gpointer s)
{
	return PSESSION_T(s)->name;
}

void sess_switch_to(sess_id s)
{
	g_assert(s);

	if(gui_page_current()==s) {
		/* already current */
		return;
	}

	gui_page_switch(s);
}

/** sess_enumerate
  *	enumerates all known sessions
  *	(you can delete the session in subject, in callback)
  */
void sess_enumerate(sess_enum_cb enum_cb, void * user_data)
{
	GList * list;
	GPtrArray * sess_array;
	int nsess;

	g_assert(enum_cb);

	/* collect session id's (pointers, really) */
	sess_array = g_ptr_array_sized_new(4);

	for(list = sess_list; list; list = list->next) {
		g_ptr_array_add(sess_array, (gpointer)list);
	}

	/* iterate over the array and invoke the callback */
	for(nsess=0; nsess < sess_array->len; nsess++) {
		list = (GList *)sess_array->pdata[nsess];
		enum_cb((sess_id)list->data,
			PSESSION_T(list->data)->type,
			PSESSION_T(list->data)->name,
			PSESSION_T(list->data)->uid, user_data);
	}

	g_ptr_array_free(sess_array, TRUE);
}

/** sess_make_channel_list:
  *	generates #chan1#chan2#...#chanN# list of channels
  *
  *	you should free the string returned
  */
gchar *
sess_make_channel_list()
{
	GList * l;
	GString * s = g_string_new(NULL);

	for(l = sess_list; l; l = l->next) {
		if(PSESSION_T(l->data)->type==SESSTYPE_CHANNEL) {
			g_assert(PSESSION_T(l->data)->name);

			g_string_append_c(s, '#');
			g_string_append(s, PSESSION_T(l->data)->name);
		}
	}
	g_string_append_c(s, '#');

	return g_string_free(s, FALSE);
}

