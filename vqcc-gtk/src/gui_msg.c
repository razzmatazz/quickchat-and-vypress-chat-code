/*
 * gui_msg.c: implements message dialog
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
 * $Id: gui_msg.c,v 1.33 2005/01/06 10:16:08 bobas Exp $
 */

#include <string.h>
#include <stdio.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkkeysyms.h>

#include "main.h"
#include "gui.h"
#include "gui_misc.h"
#include "gui_msg.h"
#include "prefs.h"
#include "user.h"
#include "util.h"
#include "sess.h"

#define HAVE_EXPANDER_WIDGET \
	((GTK_MAJOR_VERSION > 2) || ((GTK_MAJOR_VERSION==2) && (GTK_MINOR_VERSION>=4)))

#define DEFAULT_MESSAGE_WIDTH	300
#define DEFAULT_MESSAGE_HEIGHT	200

struct message_info {
	gpointer remote_user_id;
	gchar * remote_user_name;
	gboolean my, mass_message;

	GtkWidget	* window_w;
	GtkWidget	* text_w;
	GtkWidget	* send_w, * reply_w, * quote_w;
#if(HAVE_EXPANDER_WIDGET)
	GtkWidget	* option_expander_w,
			* option_save_size_w, * option_ctrl_enter_w;
#endif

	GtkTextBuffer	* buffer;
};
#define PMESSAGE_INFO(p) ((struct message_info*)p)

/** static variables
  *****************************************/
static GList * msg_list;
static int msg_count;

/** static routines
  ***************************************/

/* message_init:
 *	initializes message list
 */
static void
message_init()
{
	msg_list = NULL;
	msg_count = 0;
}

/* message_delete:
 *	deletes the message and it's window
 *	(if it has any)
 */
static void
message_delete(struct message_info * mi, gboolean destroy_widget)
{
	g_assert(mi && mi->remote_user_name);

	if(destroy_widget) {
		g_assert(mi->window_w);
		gtk_widget_destroy(mi->window_w);
	}

	g_free(mi->remote_user_name);
	g_free(mi);

	/* remove message struct from the list */
	msg_list = g_list_remove(msg_list, mi);
	--msg_count;
}

/**
 * quotes text in received message text (UTF-8)
 * returns:
 *	an allocated GString with quoted message text (UTF-8)
 */
static GString *
message_quote_orig_text(const gchar * orig)
{
	const gchar * prev_c;
	GString * quoted = g_string_new("");

	if(*orig=='\0') {
		return quoted;	/* empty string, -- do nothing */
	}

	g_string_append_c(quoted, '>');

	while(*orig) {
		g_string_append_unichar(quoted, g_utf8_get_char(orig));
		if(*orig=='\n') {
			if(g_utf8_next_char(orig)!='\0') {
				g_string_append_c(quoted, '>');
			}
		}
		prev_c = orig;
		orig = g_utf8_next_char(orig);
	}

	if(*prev_c != '\n') {
		g_string_append_c(quoted, '\n');
	}

	return quoted;
}

static void
message_send(struct message_info * mi)
{
	GtkTextIter si, ei;
	gpointer event_v[2];

	if(mi->remote_user_id) {
		/* "send" */
		gtk_text_buffer_get_start_iter(mi->buffer, &si);
		gtk_text_buffer_get_end_iter(mi->buffer, &ei);

		/* send net message to the user */
		EVENT_V(event_v,0) = mi->remote_user_id;
		EVENT_V(event_v,1) = gtk_text_buffer_get_text(mi->buffer, &si, &ei, TRUE);
		raise_event(EVENT_MESSAGE_SEND, event_v, 0);
		g_free(EVENT_V(event_v,1));

		/* delete this message */
		message_delete(mi, TRUE);
	}
}

/* message_turn_into_my:
 *	changes the message so the user can
 *	edit the text and send it back
 */
static void
message_turn_into_my(struct message_info * mi, gboolean do_quote)
{
	gchar * title;
	gchar * orig;
	GString * quoted = NULL;
	GtkTextIter start, end;

	g_assert(mi->my==FALSE);
	
	mi->my = TRUE;

	if(do_quote) {
		gtk_text_buffer_get_bounds(mi->buffer, &start, &end);
		orig = gtk_text_buffer_get_text(mi->buffer, &start, &end, FALSE);

		quoted = message_quote_orig_text(orig);

		g_free(orig);
	}

	/* preset text of buffer */
	gtk_text_view_set_editable(GTK_TEXT_VIEW(mi->text_w), mi->my);
	gtk_text_buffer_set_text(mi->buffer, quoted ? quoted->str: "", -1);
	if(quoted)
		g_string_free(quoted, TRUE);

	/* hide reply/quote buttons and emerge send one */
	gtk_widget_hide(mi->reply_w);
	gtk_widget_hide(mi->quote_w);
	gtk_widget_show(mi->send_w);

	/* change title of the message dialog */
	title = g_strdup_printf(_("Message to %s"), mi->remote_user_name);
	gtk_window_set_title(GTK_WINDOW(mi->window_w), title);
	g_free(title);

	/* activate text entry as it might loose cursor when user clicks
	 * on any of dialog buttons */
	gtk_widget_grab_focus(mi->text_w);
}

/* message_cb_key_press_event:
 *	handles dialog key press
 *
 * `Esc' - closes the message
 */
static gboolean
message_cb_key_press_event(
	GtkWidget * window_w,
	GdkEventKey * event,
	struct message_info * mi)
{
	g_assert(mi);

	if(event->keyval==GDK_Escape) {
		/* close this msg */
		message_delete(mi, TRUE);
	}
	else if(event->keyval==GDK_Return && (event->state & GDK_CONTROL_MASK) && mi->my) {
		if(prefs_bool(PREFS_GUI_MSG_CTRL_ENTER))
			message_send(mi);
	}
	else {
		return FALSE;
	}
	return TRUE;
}

/* message_cb_delete_event:
 *	gets notified if the user tries to close the message dialog
 */
static gint
message_cb_delete_event(
	GtkWidget * w, GdkEvent * e,
	struct message_info * mi)
{
	message_delete(mi, FALSE);
	return FALSE;	/* destroy the widget */
}

/* message_cb_response:
 *	handles user response to the dialog:
 *	(cancel/send/reply/close)
 */
static void 
message_cb_response(
	GtkDialog * dialog, gint response,
	struct message_info * mi)
{
	gchar * sizestr;
	gint dlg_width, dlg_height;
	
	switch(response) {
	case GUI_RESPONSE_QUOTE:
		message_turn_into_my(mi, TRUE);
		break;

	case GUI_RESPONSE_REPLY:
		message_turn_into_my(mi, FALSE);
		break;

	case GUI_RESPONSE_SEND:
#if(HAVE_EXPANDER_WIDGET)
		/* save window size, if requested to */
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mi->option_save_size_w))) {
			gtk_window_get_size(GTK_WINDOW(mi->window_w), &dlg_width, &dlg_height);

			sizestr = g_strdup_printf("%dx%d", dlg_width, dlg_height);
			prefs_set(PREFS_GUI_MSG_SUGGESTED_SIZE, sizestr);
			g_free(sizestr);
		}
#endif
		message_send(mi);
		break;

	case GTK_RESPONSE_CANCEL:
		message_delete(mi, TRUE);
		break;
	}
}

/* message_option_ctrl_enter_toggled_cb:
 *	invoked when user toggled the ctrl-send checkbox
 */
static void
message_option_ctrl_enter_toggled_cb(
	GtkToggleButton * button_w,
	struct message_info * msg)
{
	prefs_set(
		PREFS_GUI_MSG_CTRL_ENTER,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(msg->option_ctrl_enter_w)));
}

/* message_title_for:
 *	allocates and fills up title for specified message
 *	(you should free the string returned)
 */
static char *
message_title_for(struct message_info * mi)
{
	char * title;
	
	if(mi->my) {
		if(mi->mass_message)
			title = g_strdup(_("Mass message"));
		else
			title = g_strdup_printf(
				_("Message to %s"), mi->remote_user_name);
	} else {
		gchar * time_str = util_time_stamp();
		title = g_strdup_printf(
			mi->mass_message
				? _("Mass message from %s [%s]")
				: _("Message from %s [%s]"),
			mi->remote_user_name, time_str
		);
		g_free(time_str);
	}

	return title;
}

/*
 * message_get_suggested_size:
 *	returns recommended size for the message dialog
 * returns:
 *	true, if we have suggested size
 */
static gboolean
message_get_window_size(gint * p_width, gint * p_height)
{
	/* try to parse message width */
	if(sscanf(prefs_str(PREFS_GUI_MSG_SUGGESTED_SIZE), "%dx%d", p_width, p_height)==2) {
		/* check if the values are about to be valid */
		if(*p_width > 0 && *p_height > 0 && *p_width < 65536 && *p_height < 65536)
			return TRUE;
	}
	return FALSE;
}

/* message_make_window:
 *	build up and show GTK+ dialog for the message specified (mi)
 */
static void
message_make_window(struct message_info * mi, const gchar * preset_text)
{
	GtkWidget * b, * w, * option_vbox;
	gchar * title;
	gint dlg_width, dlg_height;

	g_assert(mi && mi->remote_user_id && mi->remote_user_name);

	/* make message dialog
	 */
	title = message_title_for(mi);
	mi->window_w = gtk_dialog_new_with_buttons(
		title, NULL,
#if(HAVE_EXPANDER_WIDGET)
		0,
#else
		GTK_DIALOG_NO_SEPARATOR,
#endif
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
	g_free(title);
	gui_misc_set_icon_for(GTK_WINDOW(mi->window_w));
	gtk_widget_set_size_request(mi->window_w, DEFAULT_MESSAGE_WIDTH, DEFAULT_MESSAGE_HEIGHT);
	if(message_get_window_size(&dlg_width, &dlg_height)) {
		gtk_window_set_default_size(GTK_WINDOW(mi->window_w), dlg_width, dlg_height);
	}

	/* add send/quote/reply buttons */
	mi->quote_w = gtk_dialog_add_button(
		GTK_DIALOG(mi->window_w),
		GUI_STOCK_QUOTE_MESSAGE, GUI_RESPONSE_QUOTE);
	mi->reply_w = gtk_dialog_add_button(
		GTK_DIALOG(mi->window_w),
		GUI_STOCK_REPLY_MESSAGE, GUI_RESPONSE_REPLY);
	mi->send_w = gtk_dialog_add_button(
		GTK_DIALOG(mi->window_w),
		GUI_STOCK_SEND_MESSAGE, GUI_RESPONSE_SEND);

	/* register signal source */
	g_signal_connect(
		G_OBJECT(mi->window_w), "delete-event",
		G_CALLBACK(message_cb_delete_event), (gpointer)mi);
	g_signal_connect(
		G_OBJECT(mi->window_w), "key-press-event",
		G_CALLBACK(message_cb_key_press_event), (gpointer)mi);
	g_signal_connect(
		G_OBJECT(mi->window_w), "response",
		G_CALLBACK(message_cb_response), (gpointer)mi);	

	b = gtk_vbox_new(FALSE, 2);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(mi->window_w)->vbox), b);

	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(w), 4);
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW(w),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(
		GTK_SCROLLED_WINDOW(w),
		GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(b), w, TRUE, TRUE, 2);

	/* add text widget */
	mi->text_w = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(mi->text_w), GTK_WRAP_WORD);
	mi->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mi->text_w));
	gtk_text_view_set_border_window_size(
		GTK_TEXT_VIEW(mi->text_w), GTK_TEXT_WINDOW_TOP, 1);
	gtk_text_view_set_border_window_size(
		GTK_TEXT_VIEW(mi->text_w), GTK_TEXT_WINDOW_LEFT, 1);
	gtk_container_add(GTK_CONTAINER(w), mi->text_w);

#if(HAVE_EXPANDER_WIDGET)
	/* add option expander */
	mi->option_expander_w = gtk_expander_new_with_mnemonic(_("_Message options"));
	gtk_box_pack_start(GTK_BOX(b), mi->option_expander_w, FALSE, FALSE, 2);

	option_vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(mi->option_expander_w), option_vbox);

	/* add option to send message on ctrl-enter */
	mi->option_ctrl_enter_w = gtk_check_button_new_with_mnemonic(
		prefs_description(PREFS_GUI_MSG_CTRL_ENTER));
	gtk_box_pack_start(GTK_BOX(option_vbox), mi->option_ctrl_enter_w, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON(mi->option_ctrl_enter_w),
		prefs_bool(PREFS_GUI_MSG_CTRL_ENTER));
	g_signal_connect(
		G_OBJECT(mi->option_ctrl_enter_w), "toggled",
		G_CALLBACK(message_option_ctrl_enter_toggled_cb), (gpointer)mi);
	
	/* add option to save message dialog window size */
	mi->option_save_size_w = gtk_check_button_new_with_mnemonic(_("Save message dialog size"));
	gtk_box_pack_start(GTK_BOX(option_vbox), mi->option_save_size_w, FALSE, FALSE, 0);
#endif

	/*
	 * Disable `Send' button so the user cant send message if the receiver
	 * cannot or will refuse to receive the message.
	 */
	gtk_widget_set_sensitive(
		mi->send_w,
		IS_MESSAGEABLE_MODE(user_mode_of(mi->remote_user_id)));

	/* preset text of buffer */
	gtk_text_view_set_editable(GTK_TEXT_VIEW(mi->text_w), mi->my);
	if(preset_text!=NULL) {
		gtk_text_buffer_set_text(mi->buffer, preset_text, -1);
	}

	/* show it */
	gtk_widget_show_all(GTK_DIALOG(mi->window_w)->vbox);
	if(mi->my) {
		gtk_widget_hide(mi->reply_w);
		gtk_widget_hide(mi->quote_w);
	} else {
		gtk_widget_hide(mi->send_w);
	}
	gtk_window_present(GTK_WINDOW(mi->window_w));
}

/* message_new:
 *	creates a new message and message popup window
 * args:
 *	@user_id	user id of the src/dst of the message
 *	@my		TRUE, if the message is compose one (has 'send' button)
 *	@mass_message	true, if the message is mass message
 *	@text		initial text of the message, or NULL
 */
static struct message_info *
message_new(
	gpointer user_id, gboolean my,
	gboolean mass_message, const gchar * text)
{
	struct message_info * mi;
	g_assert(user_id);

	/* fillup message struct */
	mi = g_malloc(sizeof(struct message_info));
	mi->remote_user_id = user_id;
	mi->remote_user_name = g_strdup(user_name_of(user_id));
	mi->my = my;
	mi->mass_message = mass_message;

	/* add this message to the list */
	msg_list = g_list_prepend(msg_list, mi);
	++msg_count;

	/* make and show up message window */
	message_make_window(mi, text);

	return mi;
}

/* message_cleanup:
 *	deletes all the messages and all the allocated
 *	structs
 */
static void
message_cleanup()
{
	while(msg_list)
		message_delete(PMESSAGE_INFO(msg_list->data), TRUE);
	msg_count = 0;
}

/* message_net_message_ack:
 *	handles remote end confirmation that it has received the message
 */
static void
message_net_message_ack(
	gpointer user,
	const gchar * text,
	enum user_mode_enum mode)
{
	gpointer session;
	
	g_assert(user && text);

	session = util_session_for_notifies_from(user);
	if(session) {
		if(*text) {
			sess_write(
				session, SESSTEXT_NOTIFY,
				_("%s has received your message (%s)"),
				user_name_of(user), text);
		} else {
			sess_write(
				session, SESSTEXT_NOTIFY,
				_("%s has received your message"),
				user_name_of(user));
		}
	}
}

/* message_net_msg:
 *	shows the message we've received from someone
 */
static void
message_net_msg(gpointer user, const gchar * text, gboolean mass_message)
{
	struct message_info * mi;

	/* check if we should discard messages in offline/dnd modes */
	if(!IS_MESSAGEABLE_MODE(my_mode())
			&& !prefs_bool(PREFS_GUI_MSG_SHOW_IN_OFFLINE_DND))
		return;

	if(!prefs_bool(PREFS_GUI_MSG_IN_WINDOW)
		|| msg_count >= prefs_int(PREFS_GUI_MSG_MAX_WINDOWS))
	{
		/* show message on status or user session */
		gpointer session = util_session_for_notifies_from(user);
		if(session)
			sess_write(session, SESSTEXT_NOTIFY,
				_("Message from %s: %s"),
				user_name_of(user), text);
	} else {
		/* create message window */
		mi = message_new(user, FALSE, mass_message, text);
	}
}

/* message_user_mode_change:
 *	disables all messages for the specified user,
 *	as he/she gets unreachable
 * args:
 *	@uid		user who changed the mode or became unavailable
 *	@removed	true, if the user became unavailable (logged out)
 */
static void
message_user_mode_change(
	gpointer uid,
	gboolean removed)
{
	GList * l;
	struct message_info * mi;

	g_assert(uid);

	for(l = msg_list; l; l = l->next) {
		mi = PMESSAGE_INFO(l->data);

		if(mi->remote_user_id == uid) {
			if(removed) {
				gtk_widget_set_sensitive(mi->send_w, FALSE);
				mi->remote_user_id = NULL;
			} else {
				gtk_widget_set_sensitive(
					mi->send_w, IS_MESSAGEABLE_MODE(user_mode_of(uid)));
			}
		}
	}
}

/* message_user_new:
 *	"revives" some disabled messages if the user became available
 *	after being "dead" for some time
 */
static void
message_user_new(const gchar * user_name, gpointer new_uid, enum user_mode_enum umode)
{
	GList * l;
	struct message_info * mi;

	g_assert(user_name && new_uid);

	for(l = msg_list; l; l = l->next) {
		mi = PMESSAGE_INFO(l->data);

		if(!mi->remote_user_id && !nickname_cmp(mi->remote_user_name, user_name)) {
			/* ok, msg found - revive it */
			mi->remote_user_id = new_uid;

			/* enable send/reply button if the user
			 * is in messageable mode
			 */
			if(mi->send_w)
				gtk_widget_set_sensitive(mi->send_w, IS_MESSAGEABLE_MODE(umode));
		}
	}
}

/* message_user_rename:
 *	modifies message title, as user changes it's nickname
 */
static void
message_user_rename(gpointer uid, const gchar * new_name)
{
	gchar * title;
	struct message_info * mi;
	GList * l;

	g_assert(uid && new_name);

	for(l = msg_list; l; l = l->next) {
		mi = PMESSAGE_INFO(l->data);
		if(mi->remote_user_id==uid) {
			/* store new nickname */
			g_free(mi->remote_user_name);
			mi->remote_user_name = g_strdup(new_name);

			/* change message dialog title */
			title = message_title_for(mi);
			gtk_window_set_title(GTK_WINDOW(mi->window_w), title);
			g_free(title);
		}
	}
}

/* gui_msg_event_cb:
 *	handles app events
 */
static void
gui_msg_event_cb(enum app_event_enum e, void * v, int i)
{
	switch(e) {
	case EVENT_MAIN_INIT:
		message_init();
		break;

	case EVENT_MAIN_REGISTER_PREFS:
		prefs_register(PREFS_GUI_MSG_IN_WINDOW, PREFS_TYPE_BOOL,
			_("Show messages in windows"), NULL, NULL);
		prefs_register(PREFS_GUI_MSG_MAX_WINDOWS, PREFS_TYPE_UINT,
			_("Maximum message windows"), NULL, NULL);
		prefs_register(PREFS_GUI_MSG_CTRL_ENTER, PREFS_TYPE_BOOL,
			_("CTRL-Enter sends message"), NULL, NULL);
		prefs_register(PREFS_GUI_MSG_SUGGESTED_SIZE, PREFS_TYPE_STR,
			_("Message dialog size"), NULL, NULL);
		prefs_register(PREFS_GUI_MSG_SHOW_IN_OFFLINE_DND, PREFS_TYPE_BOOL,
			_("Receive message when in Offline or DND mode"), NULL, NULL);
		break;

	case EVENT_MAIN_PRESET_PREFS:
		prefs_set(PREFS_GUI_MSG_IN_WINDOW, TRUE);
		prefs_set(PREFS_GUI_MSG_MAX_WINDOWS, 8);
		prefs_set(PREFS_GUI_MSG_CTRL_ENTER, TRUE);
		break;

	case EVENT_MAIN_PRECLOSE:
		message_cleanup();
		break;
	case EVENT_USER_NEW:
		message_user_new(EVENT_V(v, 1), EVENT_V(v, 0), (enum user_mode_enum)i);
		break;
	case EVENT_USER_REMOVED:
		/* disable the message, as there's no user to send messages to */
		message_user_mode_change(v, TRUE);
		break;
	case EVENT_USER_MODE_CHANGE:
		message_user_mode_change(v, FALSE);
		break;
	case EVENT_USER_RENAME:
		message_user_rename(EVENT_V(v, 0), user_name_of(EVENT_V(v, 0)));
		break;
	case EVENT_IFACE_USER_MESSAGE_REQ:
		message_new(v, TRUE, FALSE, NULL);
		break;
	case EVENT_NET_MSG_MESSAGE:
		message_net_msg(
			EVENT_V(v, 0), (const char *)EVENT_V(v, 1),
			(gboolean)EVENT_V(v, 2));
		break;
	case EVENT_NET_MSG_MESSAGE_ACK:
		message_net_message_ack(
			EVENT_V(v, 0),
			(const char*)EVENT_V(v, 1),
			(enum user_mode_enum)i);
		break;
	default:
		break;	/* nothing */
	}
}

/** exported routines
  *****************************************/
void gui_msg_dlg_register()
{
	register_event_cb(
		gui_msg_event_cb,
		EVENT_MAIN|EVENT_IFACE|EVENT_USER|EVENT_NET);
}

