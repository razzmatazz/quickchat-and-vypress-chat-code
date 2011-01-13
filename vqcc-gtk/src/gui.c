/*
 * gui.c: implements the main window
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
 * $Id: gui.c,v 1.37 2005/01/07 15:11:01 bobas Exp $
 */

#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "main.h"
#include "net.h"
#include "prefs.h"
#include "user.h"
#include "sess.h"
#include "gui.h"
#include "gui_page.h"
#include "gui_ulist.h"
#include "gui_msg.h"
#include "gui_channel.h"
#include "gui_misc.h"
#include "gui_tray.h"
#include "gui_ignore.h"
#include "gui_config.h"
#include "gui_topic_dlg.h"
#include "gui_netselect_dlg.h"
#include "gui_about_dlg.h"

/** GUI GEOMETRIES  &  OTHER SETTINGS
  *********************************************/
#define GEOM_NICKNAME_LEN	112

/** enums & structs
  *********************************************/

struct iface_main_wnd {
	GtkWidget
		* widget,
		* menu_bar,
			* menu_chat_w,
			* menu_channel_w,
			* menu_channel_close_w,
			* menu_show_menu_w,
			* menu_show_topic_w,
		* topic_entry,
		* text_entry,
		* hbox1,
		* config_btn, * nickname_entry_w, * usermode_opt,
		* vsep1,
		* notebook,
		* userlist, * userlist_frame_w;

	gpointer userlist_menu_current_data;
		/* here we save user_data of activated user list item */

	gboolean topic_modified;
	gboolean has_focus;
	gboolean is_obscured;
};

/* Static variables
 ******************************/
static struct iface_main_wnd * main_wnd;
static int *tmp_p_argc;
static char *** tmp_p_argv;

/* static routines
 ******************************/

static void main_wnd_update_for_session(sess_id session);

static void
usermode_selected_cb(gint new_mode, gpointer data)
{
	if(new_mode >= 0)
		prefs_set(PREFS_MAIN_MODE, new_mode);
}

/** update_userlist_count:
 *	updates user count on userlist frame label (including ourselves)
 */
static void
update_userlist_count()
{
	gchar * label;

	g_assert(main_wnd && main_wnd->userlist_frame_w);

	label = g_strdup_printf(_("Network users: %d"), user_count()+1);
	gtk_frame_set_label(GTK_FRAME(main_wnd->userlist_frame_w),label);
	g_free(label);
}

static void
menu_channel_set_topic_activate_cb(GtkMenuItem * item_w, gpointer user_data)
{
	raise_event(EVENT_IFACE_SHOW_TOPIC_DLG, sess_current(), 0);
}

static void
menu_close_page_activate_cb(GtkMenuItem * item_w, gpointer user_data)
{
	raise_event(EVENT_IFACE_PAGE_CLOSE, NULL, 0);
}

static void
menu_show_menu_toggled_cb(GtkCheckMenuItem * item_w, gpointer user_data)
{
	prefs_set(PREFS_GUI_MENU_BAR, gtk_check_menu_item_get_active(item_w));
}

static void
menu_show_topic_toggled_cb(GtkCheckMenuItem * item_w, gpointer user_data)
{
	prefs_set(PREFS_GUI_TOPIC_BAR, gtk_check_menu_item_get_active(item_w));
}

static void
menu_settings_channels_activate_cb(GtkCheckMenuItem * item_w, gpointer user_data)
{
	raise_event(EVENT_IFACE_SHOW_CHANNEL_DLG, NULL, 0);
}

static void
menu_settings_ignore_list_activate_cb(GtkCheckMenuItem * item_w, gpointer user_data)
{
	raise_event(EVENT_IFACE_SHOW_IGNORE_DLG, NULL, 0);
}

static void
menu_settings_preferences_activate_cb(GtkMenuItem * item_w, gpointer user_data)
{
	raise_event(EVENT_IFACE_SHOW_CONFIGURE_DLG, NULL, 0);
}

static void
menu_help_about_activate_cb(GtkMenuItem * item_w, gpointer user_data)
{
	raise_event(EVENT_IFACE_SHOW_ABOUT_DLG, NULL, 0);
}

static GtkWidget * main_wnd_new_menu_bar(struct iface_main_wnd * mw)
{
	GtkWidget * menubar, * submenu, * item;
       
	menubar = gtk_menu_bar_new();

	/* the "Chat" submenu
	 */
	mw->menu_chat_w = gtk_menu_item_new_with_mnemonic(_("_Chat"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), mw->menu_chat_w);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(mw->menu_chat_w), submenu = gtk_menu_new());

	item = util_image_menu_item(GTK_STOCK_CLOSE, _("_Close"),
		G_CALLBACK(menu_close_page_activate_cb), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);

	/* the "Channel" submenu
	 */
	mw->menu_channel_w = gtk_menu_item_new_with_mnemonic(_("_Channel"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), mw->menu_channel_w);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(mw->menu_channel_w), submenu = gtk_menu_new());

	item = util_image_menu_item(GUI_STOCK_SET_TOPIC, _("Set _topic"),
		G_CALLBACK(menu_channel_set_topic_activate_cb), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);

	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());
	
	mw->menu_channel_close_w = util_image_menu_item(GTK_STOCK_CLOSE, _("_Close"),
		G_CALLBACK(menu_close_page_activate_cb), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), mw->menu_channel_close_w);

	/* the "View" submenu
	 */
	item = gtk_menu_item_new_with_mnemonic(_("_View"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), item);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu = gtk_menu_new());

	mw->menu_show_menu_w = gtk_check_menu_item_new_with_mnemonic(_("Show _menu bar"));
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), mw->menu_show_menu_w);
	g_signal_connect(G_OBJECT(mw->menu_show_menu_w), "toggled",
		G_CALLBACK(menu_show_menu_toggled_cb), NULL);

	mw->menu_show_topic_w = gtk_check_menu_item_new_with_mnemonic(_("Show _topic bar"));
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), mw->menu_show_topic_w);
	g_signal_connect(G_OBJECT(mw->menu_show_topic_w), "toggled",
		G_CALLBACK(menu_show_topic_toggled_cb), NULL);

	/* the "Settings" submenu
	 */
	item = gtk_menu_item_new_with_mnemonic(_("_Settings"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), item);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu = gtk_menu_new());

	item = util_image_menu_item(GUI_STOCK_CHANNEL, _("_Channels.."),
		G_CALLBACK(menu_settings_channels_activate_cb), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
	item = util_image_menu_item(GUI_STOCK_IGNORE, _("_Ignore list.."),
		G_CALLBACK(menu_settings_ignore_list_activate_cb), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
		
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), gtk_separator_menu_item_new());

	item = util_image_menu_item(GTK_STOCK_PREFERENCES, _("_Preferences.."),
		G_CALLBACK(menu_settings_preferences_activate_cb), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);

	/* the "Help" submenu
	 */
	item = gtk_menu_item_new_with_mnemonic(_("_Help"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), item);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu = gtk_menu_new());

	item = util_image_menu_item(NULL, _("_About vqcc-gtk"),
		G_CALLBACK(menu_help_about_activate_cb), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);

	return menubar;
}

/**
 * main_wnd_delete_event_cb:
 *	handles window close event
 */
static gint
main_wnd_delete_event_cb(GtkWidget * widget, GdkEvent * event, gpointer data)
{
	if(gui_tray_is_embedded()) {
		/* hide the window, if we have our icon on the tray */
		gui_set_visible(FALSE);
	} else {
		raise_event(EVENT_IFACE_EXIT, NULL, 0);
	}

	return TRUE;	/* don't destroy window */
}

/**
 * main_wnd_focus_out_cb:
 *	make text entry a focused widget
 *	each time leave focus
 */
static gboolean
main_wnd_focus_out_cb(
	GtkWidget * window, GdkEventFocus * event, gpointer user_data)
{
	gtk_widget_grab_focus(main_wnd->text_entry);
	return FALSE;
}

/**
 * main_wnd_key_press_cb:
 *	handles key presses
 *	(switches pages on alt-'-', alt-'=', ctrl-pgup/down
 */
static gboolean
main_wnd_key_press_cb(GtkWidget * wnd_w, GdkEventKey * key_e, gpointer user_data)
{
	/* get keyboard modifiers */
	gpointer page;
	gboolean alt	= (key_e->state & GDK_MOD1_MASK)!=0,
		ctrl	= (key_e->state & GDK_CONTROL_MASK)!=0;

	if(key_e->keyval==GDK_Escape && gui_tray_is_embedded()) {
		gui_set_visible(FALSE);
		return TRUE;
	}
	else if((alt && key_e->keyval==GDK_minus) || (ctrl && key_e->keyval==GDK_Page_Up)) {
		page = gui_page_prev(gui_page_current());
		if(page)
			gui_page_switch(page);
	}
	else if((alt && key_e->keyval==GDK_equal) || (ctrl && key_e->keyval==GDK_Page_Down)) {
		page = gui_page_next(gui_page_current());
		if(page)
			gui_page_switch(page);
	}
	else if(key_e->keyval==GDK_Page_Up || key_e->keyval==GDK_Page_Down) {
		/* scrool the page up/down
		 */
		page = gui_page_current();
		if(page) {
			gui_page_scroll(
				page,
				key_e->keyval==GDK_Page_Up
					? SCROLL_PAGE_UP: SCROLL_PAGE_DOWN);
		}
	}
	else if(ctrl && (key_e->keyval==GDK_w || key_e->keyval==GDK_W)) {
		/* close active page on CTRL-W */
		raise_event(EVENT_IFACE_PAGE_CLOSE, NULL, 0);
	}
	else if(ctrl && (key_e->keyval==GDK_t || key_e->keyval==GDK_T)) {
		/* popup topic change dialog for the current session */
		if(!sess_topic_readonly(sess_current())) {
			if(prefs_bool(PREFS_GUI_TOPIC_BAR)) {
				/* activate and select-all the topic entry */
				gtk_editable_select_region(
					GTK_EDITABLE(main_wnd->topic_entry), 0, -1);
				gtk_editable_set_position(
					GTK_EDITABLE(main_wnd->topic_entry), -1);

				gtk_widget_grab_focus(main_wnd->topic_entry);
				return TRUE;	/* don't let the text_entry regrab focus */
			} else {
				/* show topic edit dialog as there's no
				 * topic entry at the top of the window */
				raise_event(EVENT_IFACE_SHOW_TOPIC_DLG, sess_current(), 0);
			}
		}
	}
	else {
		/* we're not interested: pass the event down the chain */
		return FALSE;
	}

	/* return focus to edit box */
	gtk_widget_grab_focus(main_wnd->text_entry);
	return TRUE;
}

/**
 * main_wnd_focus_change_cb:
 *	invoked when the main windows gets/looses the kbd focus
 */
static gboolean
main_wnd_focus_change_cb(
	GtkWidget * widget, GdkEventFocus * focus_event, gpointer dummy)
{
	gboolean focus_state = focus_event->in!=0;

	if(focus_state != main_wnd->has_focus) {
		main_wnd->has_focus = focus_state;

		raise_event(EVENT_IFACE_ACTIVE_CHANGE, 0, focus_state);
	}

	return FALSE;
}

static gboolean
main_wnd_visibility_notify_cb(
	GtkWidget * widget, GdkEventVisibility * event, gpointer dummy)
{
	main_wnd->is_obscured = (event->state==GDK_VISIBILITY_FULLY_OBSCURED);
	return FALSE;
}

static gboolean
entry_activate_cb(GtkWidget * widget, gpointer user)
{
	const gchar * new_nick;

	g_assert(main_wnd);
	g_assert(main_wnd->text_entry);

	if(widget==main_wnd->text_entry) {
		raise_event(
			EVENT_IFACE_TEXT_ENTER,
			(gpointer)gtk_entry_get_text(GTK_ENTRY(main_wnd->text_entry)),
			0);
		return TRUE;
	}
	else if(widget==main_wnd->nickname_entry_w) {
		new_nick = gtk_entry_get_text(GTK_ENTRY(main_wnd->nickname_entry_w));

		if(nickname_valid(new_nick)) {
			raise_event(EVENT_IFACE_NICKNAME_ENTERED, (gpointer)new_nick, 0);
		}

		gtk_widget_grab_focus(main_wnd->text_entry);
	}
	return FALSE;
}

static gboolean
topic_entry_activate_cb(GtkEntry * entry, gpointer userdata)
{
	gpointer event[2];
	
	/* change the topic */
	event[0] = sess_current();
	event[1] = (gpointer)gtk_entry_get_text(GTK_ENTRY(main_wnd->topic_entry));
	raise_event(EVENT_IFACE_TOPIC_ENTER, event, 0);

	/* return focus to text entry */
	gtk_widget_grab_focus(main_wnd->text_entry);

	return TRUE;
}

static gboolean
topic_entry_focus_out_event_cb(GtkWidget * widget, gpointer userdata)
{
	/* restore the topic */
	main_wnd_update_for_session(sess_current());

	return FALSE;	/* we MUST return FALSE for GtkEntry to work properly */
}

/**
 * nickname_focus_out_cb:
 *	restores nickname entry if/when it loses focus
 */
static gboolean
nickname_focus_out_cb(GtkWidget * entry_w, GdkEventFocus * event, gpointer data)
{
	if(app_status()==APP_RUNNING)
		gtk_entry_set_text(GTK_ENTRY(main_wnd->nickname_entry_w), my_nickname());

	return FALSE;	/* we MUST return FALSE for GtkEntry to work properly */
}

static struct iface_main_wnd *
new_main_wnd()
{
	GtkWidget * vbox, * w, * b, * hpaned;
	struct iface_main_wnd * mw;

	mw = g_new(struct iface_main_wnd, 1);
	mw->topic_modified = FALSE;
	mw->has_focus = FALSE;
	mw->is_obscured = FALSE;

	mw->widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gui_misc_set_icon_for(GTK_WINDOW(mw->widget));
	gtk_window_set_title(GTK_WINDOW(mw->widget), PACKAGE);
	gtk_container_set_border_width(GTK_CONTAINER(mw->widget), 2);
	g_signal_connect(G_OBJECT(mw->widget), "delete-event",
		G_CALLBACK(main_wnd_delete_event_cb), NULL);
	g_signal_connect(G_OBJECT(mw->widget), "focus-out-event",
		G_CALLBACK(main_wnd_focus_out_cb), NULL);
	g_signal_connect(G_OBJECT(mw->widget), "key-press-event",
		G_CALLBACK(main_wnd_key_press_cb), NULL);
	g_signal_connect(G_OBJECT(mw->widget), "focus-in-event",
		G_CALLBACK(main_wnd_focus_change_cb), NULL);
	g_signal_connect(G_OBJECT(mw->widget), "focus-out-event",
		G_CALLBACK(main_wnd_focus_change_cb), NULL);
	g_signal_connect(G_OBJECT(mw->widget), "visibility-notify-event",
		G_CALLBACK(main_wnd_visibility_notify_cb), NULL);

	/* create vbox for holding the menu bar and the bottom paned */
	vbox = gtk_vbox_new(FALSE, 2);
	gtk_container_add(GTK_CONTAINER(mw->widget), vbox);

	/* create menu bar */
	mw->menu_bar = main_wnd_new_menu_bar(mw);
	gtk_box_pack_start(GTK_BOX(vbox), mw->menu_bar, FALSE, FALSE, 0);

	/* create horizontal paned */
	hpaned = gtk_hpaned_new();
	gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);

	w = gtk_vbox_new(FALSE, 2);
	gtk_paned_pack1(GTK_PANED(hpaned), w, TRUE, TRUE);

	/* topic entry */
	mw->topic_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(mw->topic_entry), MAX_TOPIC_LENGTH);
	gtk_box_pack_start(GTK_BOX(w), mw->topic_entry, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(mw->topic_entry), "activate",
		G_CALLBACK(topic_entry_activate_cb), NULL);
	g_signal_connect(G_OBJECT(mw->topic_entry), "focus-out-event",
		G_CALLBACK(topic_entry_focus_out_event_cb), NULL);
	
	/* notebook */
	mw->notebook = (GtkWidget *) gui_page_alloc_notebook();
	gtk_box_pack_start(GTK_BOX(w), mw->notebook, TRUE, TRUE, 0);

	/* text edit */
	mw->text_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(mw->text_entry), MAX_TEXT_LENGTH);
	gtk_box_pack_start(GTK_BOX(w), mw->text_entry, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(mw->text_entry), "activate",
		G_CALLBACK(entry_activate_cb), NULL);

	/* nickname btn, mode menu */
	w = gtk_vbox_new(FALSE, 2);

	/* nickname and usermode selection widgets */
	b = gtk_vbox_new(FALSE, 2);
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_box_pack_start(GTK_BOX(w), b, FALSE, FALSE, 0);

	/* nickname entry */
	mw->nickname_entry_w = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(mw->nickname_entry_w), MAX_NICK_LENGTH);
	gtk_widget_set_usize(GTK_WIDGET(mw->nickname_entry_w), GEOM_NICKNAME_LEN, 0);
	gtk_box_pack_start(GTK_BOX(b), mw->nickname_entry_w, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(mw->nickname_entry_w), "activate",
			G_CALLBACK(entry_activate_cb), NULL);
	g_signal_connect(G_OBJECT(mw->nickname_entry_w), "focus-out-event",
			G_CALLBACK(nickname_focus_out_cb), NULL);

	/* user mode selection menu */
	mw->usermode_opt = util_user_mode_option(usermode_selected_cb, NULL);
	gtk_box_pack_start(GTK_BOX(b), mw->usermode_opt, FALSE, FALSE, 0);

	gtk_paned_pack2(GTK_PANED(hpaned), w, FALSE, FALSE);

	/* user list */
	mw->userlist_frame_w = gtk_frame_new(_("Network Users"));
	b = gtk_vbox_new(FALSE, 2);
	gtk_container_set_border_width(GTK_CONTAINER(b), 3);
	gtk_container_add(GTK_CONTAINER(mw->userlist_frame_w), b);
	mw->userlist = gui_ulist_create();
	gtk_box_pack_start(GTK_BOX(b), mw->userlist, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(w), mw->userlist_frame_w, TRUE, TRUE, 0);

	gtk_widget_show_all(vbox);

	return mw;
}

static void
delete_main_wnd(struct iface_main_wnd * mw)
{
	if(!mw) return;

	gtk_widget_destroy(mw->widget);

	gui_ulist_free();

	g_free(mw);
}

/**
 * tray_toggle_visible:
 *	invoked when user clicks systray (docklet) icon
 */
static void
tray_toggle_visible()
{
	if(GTK_WIDGET_VISIBLE(main_wnd->widget)) {
		if(gdk_window_get_state(
				GTK_WIDGET(main_wnd->widget)->window) & GDK_WINDOW_STATE_ICONIFIED
			|| main_wnd->is_obscured)
		{
			gtk_window_present(gui_get_main_window());
		} else {
			gui_set_visible(FALSE);
		}
	} else {
		gui_set_visible(TRUE);
	}
}

/* save_window_size:
 *	stores window size in `PREFS_WND_SIZE'
 */
static void
save_window_size(struct iface_main_wnd * mw)
{
	gint width, height;
	gchar * size;

	g_assert(mw && mw->widget);

	gtk_window_get_size(GTK_WINDOW(mw->widget), &width, &height);
	size = g_strdup_printf("%dx%d", width, height);
	prefs_set(PREFS_GUI_SIZE, size);
	g_free(size);
}

/* restore_window_size:
 *	restores window size from `PREFS_WND_SIZE'
 */
static void
restore_window_size(struct iface_main_wnd * mw)
{
	int width, height;
	const gchar * size;

	g_assert(mw && mw->widget);

	/* try to parse the string in PREFS_GUI_SIZE */
	size = prefs_str(PREFS_GUI_SIZE);
	if(size) {
		if(sscanf(size, "%dx%d", &width, &height)==2) {
			if(width>0 && height>0 && !(width & ~0xffff) && !(height & ~0xffff)) {
				gtk_window_resize(GTK_WINDOW(mw->widget), width, height);
			}
		}
	}
}

/* main_wnd_prepare_topic
 *	freely formated topic string (may include newlines and tabs)
 * returns:
 *	a string with those entries removed,
 *	and truncated to specified num of chars
 */
static gchar *
main_wnd_prepare_topic(const gchar * topic, guint chop_at)
{
	GString * chopped = g_string_new(NULL);
	guint len;

	for(len = 0; *topic && len < chop_at; len ++, topic = g_utf8_next_char(topic)) {
		switch(*topic) {
		case '\t':
			/* FALLTHROUGH */
		case '\n':
			/* replace newlines with spaces */
			g_string_append_c(chopped, ' ');
			break;
		case '\r':
			break;
		default:
			g_string_append_unichar(chopped, g_utf8_get_char(topic));
			break;
		}
	}
	if(*topic) {
		/* string was truncated: append "..." on the end */
		g_string_append(chopped, "...");
	}

	return g_string_free(chopped, FALSE);
}

static void
main_wnd_update_for_session(sess_id session)
{
	GString * title;

	/* build window title */
	title = g_string_new(PACKAGE" [");

	switch(sess_type(session)) {
	case SESSTYPE_CHANNEL:
		/* add topic at the beginning */
		g_string_append_printf(title, "#%s", sess_name(session));

		if(*sess_topic(session)
				&& !prefs_bool(PREFS_GUI_TOPIC_BAR)) {
			/* add the topic to window title string if no topic
			 * entry is available to the user
			 */
			gchar * topic = main_wnd_prepare_topic(
				sess_topic(session),
				prefs_int(PREFS_GUI_TITLE_MAX_LEN));
			g_string_append_printf(title, ": %s", topic);
			g_free(topic);
		}
		break;
	case SESSTYPE_PRIVATE:
		g_string_append_printf(title, _("Private chat with %s"), sess_name(session));
		break;
	case SESSTYPE_STATUS:
		g_string_append(title, _("Status"));
		break;
	}
	g_string_append_c(title, ']');

	gtk_window_set_title(GTK_WINDOW(main_wnd->widget), title->str);
	g_string_free(title, TRUE);

	/* set topic entry to the topic of this session, if it wants to be shown */
	if(sess_type(session)==SESSTYPE_CHANNEL && prefs_bool(PREFS_GUI_TOPIC_BAR)) {
		gtk_entry_set_text(GTK_ENTRY(main_wnd->topic_entry), sess_topic(session));
		gtk_widget_show(main_wnd->topic_entry);
	} else {
		gtk_widget_hide(main_wnd->topic_entry);
	}
	
	/* update menu bar for this session */
	g_object_set(G_OBJECT(main_wnd->menu_chat_w), "visible",
		sess_type(session)==SESSTYPE_PRIVATE, NULL);
	g_object_set(G_OBJECT(main_wnd->menu_channel_w), "visible",
		sess_type(session)==SESSTYPE_CHANNEL, NULL);

	if(sess_type(session)==SESSTYPE_CHANNEL)
		gtk_widget_set_sensitive(
			main_wnd->menu_channel_close_w, sess_is_closeable(session));

	gtk_widget_set_sensitive(
		main_wnd->menu_show_topic_w, sess_type(session)==SESSTYPE_CHANNEL);
}

static void
gui_prefs_nickname_changed_cb(const gchar * prefs_name)
{
	gtk_entry_set_text(GTK_ENTRY(main_wnd->nickname_entry_w), prefs_str(prefs_name));
}

static void
gui_prefs_mode_changed_cb(const gchar * prefs_name)
{
	gtk_option_menu_set_history(
		GTK_OPTION_MENU(main_wnd->usermode_opt), prefs_int(prefs_name));
}

static void
gui_prefs_topic_bar_changed_cb(const gchar * prefs_name)
{
	gpointer session = sess_current();
	if(session)
		main_wnd_update_for_session(session);

	/* update menu toggle state */
	gtk_check_menu_item_set_active(
		GTK_CHECK_MENU_ITEM(main_wnd->menu_show_topic_w),
		prefs_bool(PREFS_GUI_TOPIC_BAR));
}

static void
gui_prefs_menu_bar_changed_cb(const gchar * prefs_name)
{
	if(prefs_bool(prefs_name)) {
		gtk_widget_show(main_wnd->menu_bar);
		
		/* update menu toggle state */
		gtk_check_menu_item_set_active(
			GTK_CHECK_MENU_ITEM(main_wnd->menu_show_menu_w), TRUE);
	}
	else
		gtk_widget_hide(main_wnd->menu_bar);
}

static void
gui_register_prefs()
{
	/* register configuration switches */
	prefs_register(PREFS_GUI_KEEP_SIZE,	PREFS_TYPE_BOOL,
		_("Keep window size"), NULL, NULL);
	prefs_register(PREFS_GUI_SIZE,		PREFS_TYPE_STR,
		_("Window size"), NULL, NULL);
	prefs_register(PREFS_GUI_PRESENT_ON_PRIVATE, PREFS_TYPE_BOOL,
		_("Present main window when opening private"), NULL, NULL);
	prefs_register(PREFS_GUI_TITLE_MAX_LEN,	PREFS_TYPE_UINT,
		_("Maximum length of the window title"), NULL, NULL);
	prefs_register(PREFS_GUI_TOPIC_BAR,	PREFS_TYPE_BOOL, _("Show topic bar"), NULL, NULL);
	prefs_register(PREFS_GUI_MENU_BAR,	PREFS_TYPE_BOOL, _("Show menu bar"), NULL, NULL);

	/* register change notifiers */
	prefs_add_notifier(PREFS_MAIN_NICKNAME, (GHookFunc)gui_prefs_nickname_changed_cb);
	prefs_add_notifier(PREFS_MAIN_MODE, (GHookFunc)gui_prefs_mode_changed_cb);
	prefs_add_notifier(PREFS_GUI_TOPIC_BAR, (GHookFunc)gui_prefs_topic_bar_changed_cb);
	prefs_add_notifier(PREFS_GUI_MENU_BAR, (GHookFunc)gui_prefs_menu_bar_changed_cb);
}

static void
gui_preset_prefs()
{
	/* set default values */
	prefs_set(PREFS_GUI_PRESENT_ON_PRIVATE, TRUE);
	prefs_set(PREFS_GUI_TITLE_MAX_LEN, 64);
	prefs_set(PREFS_GUI_TOPIC_BAR, TRUE);
	prefs_set(PREFS_GUI_MENU_BAR, TRUE);
	prefs_set(PREFS_GUI_KEEP_SIZE, TRUE);
}

static void
gui_event_cb(enum app_event_enum event, void * p, int i)
{
	static gboolean already_got_embedded = FALSE;
	
	switch(event) {
	case EVENT_MAIN_INIT:
		g_assert(tmp_p_argc && tmp_p_argv);

		gtk_init(tmp_p_argc, tmp_p_argv);

		gui_misc_init();

		main_wnd = new_main_wnd();
		break;

	case EVENT_MAIN_REGISTER_PREFS:
		gui_register_prefs();
		break;

	case EVENT_MAIN_PRESET_PREFS:
		gui_preset_prefs();
		break;

	case EVENT_MAIN_START:
		if(prefs_bool(PREFS_GUI_KEEP_SIZE)) {
			/* restore window size */
			restore_window_size(main_wnd);
		}

		gui_set_visible(TRUE);
		break;

	case EVENT_MAIN_PRECLOSE:
		if(prefs_bool(PREFS_GUI_KEEP_SIZE))
			save_window_size(main_wnd);
		break;

	case EVENT_MAIN_CLOSE:
		delete_main_wnd(main_wnd);
		main_wnd = NULL;

		gui_misc_destroy();
		break;

	case EVENT_CMDPROC_SET_TEXT:
		gtk_entry_set_text(GTK_ENTRY(main_wnd->text_entry), (const char*)p);
		break;

	case EVENT_USER_NEW:
	case EVENT_USER_REMOVED:
		update_userlist_count();
		break;

	case EVENT_SESSION_OPENED:
		if(prefs_bool(PREFS_GUI_PRESENT_ON_PRIVATE) && app_status()==APP_RUNNING) {
			/* popup the main window and select page
			 * of the new chat
			 */
			gui_set_visible(TRUE);
			sess_switch_to((sess_id)p);
		}
		break;

	case EVENT_SESSION_RENAMED:
	case EVENT_SESSION_TOPIC_CHANGED:
		if(EVENT_V(p, 0)==sess_current())
			main_wnd_update_for_session(EVENT_V(p, 0));
		break;

	case EVENT_IFACE_PAGE_SWITCH:
		main_wnd_update_for_session(sess_current());
		break;

	case EVENT_IFACE_PAGE_RELEASE_FOCUS:
		gtk_widget_grab_focus(main_wnd->text_entry);
		break;

	case EVENT_IFACE_TRAY_CLICK:
		tray_toggle_visible();
		break;

	case EVENT_IFACE_TRAY_EMBEDDED:
		/* hide the main window if the tray got embedded
		 * (usualy on startup.. do this once, as the system tray area
		 * may reappear multiple times during our runtime)
		 */
		if(prefs_bool(PREFS_GUI_TRAY_HIDE_WND_ON_STARTUP)
				&& prefs_bool(PREFS_NET_IS_CONFIGURED)
				&& !already_got_embedded) {
			already_got_embedded = TRUE;
			gui_set_visible(FALSE);
		}
		break;

	case EVENT_IFACE_TRAY_REMOVED:
		/* we've lost the tray: show the main window */
		gui_set_visible(TRUE);
		break;

	case EVENT_IFACE_REQ_PRESENT:
		gui_set_visible(TRUE);
		break;

	default:
		break;
	}
}

/* Exported routines
 ******************************/

void gui_register(int * p_argc, char *** p_argv)
{	
	tmp_p_argc = p_argc;
	tmp_p_argv = p_argv;

	register_event_cb(
		gui_event_cb,
		EVENT_MAIN|EVENT_CMDPROC|EVENT_USER|EVENT_SESSION|EVENT_IFACE);

	/* register neighbour callbacks */
	gui_page_register();
	gui_msg_dlg_register();
	gui_channel_register();
	gui_tray_register();
	gui_ulist_register();
	gui_topic_dlg_register();
	gui_netselect_dlg_register();
	gui_about_dlg_register();
	gui_ignore_dlg_register();
	gui_config_dlg_register();
}

void gui_run()
{
	gtk_main();
}

void gui_shutdown()
{
	gtk_main_quit();
}

gboolean gui_is_visible()
{
	return GTK_WIDGET_VISIBLE(main_wnd->widget);
}

gboolean gui_is_active()
{
	return main_wnd->has_focus;
}

/* gui_set_visible:
 *	shows/hides main window
 *
 *	presents the window (moves it to top of the desktop window stack) if
 *	the window is already shown and @visible==TRUE
 */
void gui_set_visible(gboolean visible)
{
	/* TODO: hide dialogs if visible==FALSE
	 */

	/* save window pos during hide/show */
	static gint prev_x = 0, prev_y = 0;

	if(!GTK_WIDGET_VISIBLE(main_wnd->widget) == !visible) {
		/* already shown/hidden */
		if(visible) {
			/* present */
			gtk_window_present(GTK_WINDOW(main_wnd->widget));
		}
		return;
	}

	if(visible) {
		gtk_widget_show(main_wnd->widget);
		gtk_window_move((GtkWindow*)main_wnd->widget, prev_x, prev_y);
		gtk_widget_grab_focus(main_wnd->text_entry);
		gtk_window_present(GTK_WINDOW(main_wnd->widget));

		raise_event(EVENT_IFACE_MAINWND_SHOWN, 0, 0);
	} else {
		gtk_window_get_position((GtkWindow*)main_wnd->widget, &prev_x, &prev_y);
		gtk_widget_hide(main_wnd->widget);

		raise_event(EVENT_IFACE_MAINWND_HIDDEN, 0, 0);
	}
}

gpointer
gui_get_main_window()
{
	return main_wnd->widget;
}

