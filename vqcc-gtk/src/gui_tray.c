/*
 * gui_tray.c: support for system tray
 * Copyright (C) 2003-2004 Saulius Menkevicius
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
 * $Id: gui_tray.c,v 1.32 2004/12/30 19:15:22 bobas Exp $
 */

#include <glib.h>
#include <gtk/gtk.h>

#include "main.h"
#include "prefs.h"
#include "sess.h"
#include "user.h"
#include "util.h"
#include "gui_misc.h"
#include "gui.h"

#define GUI_TRAY_IMPL
#include "gui_tray.h"

#define ICON_BLINK_TIMEOUT_MS	500
#define TOOLTIP_MAX_LEN		64	/* max chars in one tooltip line */

/** forward references
 */
static void update_tray_icon();

/** static variables
 */
static gboolean	tray_created,
		tray_embedded;

static guint	tray_blink_timeout;
static gboolean	tray_blink_state;
static guint	tray_tooltip_strings_num;
static GList	* tray_tooltip_strings;

static const struct tray_impl_ops * tray_impl;

/* static routines
 */
static gboolean
tray_blink_cb(gpointer data)
{
	update_tray_icon();
	tray_blink_state = !tray_blink_state;
	
	return TRUE;	/* ie. do not remove this event source */
}

static void
tray_start_blinking()
{
	g_assert(tray_created);

	if(tray_blink_timeout)
		return;

	tray_blink_state = TRUE;	/* show the "faded" icon first time it changes */
	tray_blink_timeout = g_timeout_add(ICON_BLINK_TIMEOUT_MS, &tray_blink_cb, NULL);

	/* force icon update */
	tray_blink_cb(NULL);
}

static void
tray_stop_blinking()
{
	g_assert(tray_created);

	if(!tray_blink_timeout)
		return;
	
	g_source_remove(tray_blink_timeout);
	tray_blink_timeout = 0;

	/* set the icon to "normal" state
	 */
	tray_blink_state = FALSE;
	update_tray_icon();
}

static void
tray_popup_mode_cb(GtkMenuItem * item_w, gpointer new_mode)
{
	raise_event(EVENT_IFACE_TRAY_UMODE, new_mode, 0);
}

static GtkWidget *
tray_popup_menu_mode_submenu()
{
	GtkWidget * submenu_w, * item_w;
	enum user_mode_enum usermode;

	submenu_w = gtk_menu_new();

	for(usermode = UMODE_FIRST_VALID; usermode < UMODE_NUM_VALID; usermode++) {
		item_w = util_image_menu_item(
			util_user_state_stock(usermode, TRUE), user_mode_name(usermode),
			G_CALLBACK(tray_popup_mode_cb), GINT_TO_POINTER(usermode));
		if(prefs_int(PREFS_MAIN_MODE)==usermode)
			gtk_widget_set_sensitive(item_w, FALSE);

		gtk_menu_shell_append(GTK_MENU_SHELL(submenu_w), item_w);
	}

	return submenu_w;
}

static void
tray_popup_message_cb(GtkMenuItem * item_w, gpointer dummy)
{
	gpointer user = user_by_name(g_object_get_data(G_OBJECT(item_w), "username"));
	if(user)
		raise_event(EVENT_IFACE_USER_MESSAGE_REQ, user, 0);
}

static GtkWidget *
tray_popup_menu_message_submenu(gint * n_users)
{
	GtkWidget * submenu_w, * item_w;
	GList * list, * entry;

	g_assert(n_users);

	submenu_w = gtk_menu_new();

	list = user_list(TRUE);
	for(entry = list, *n_users = 0; entry; entry = entry->next, (*n_users) ++) {
		enum user_mode_enum usermode = user_mode_of(entry->data);

		item_w = util_image_menu_item(
			util_user_state_stock(usermode, TRUE), NULL,
			G_CALLBACK(tray_popup_message_cb), NULL);
		gtk_label_set_text(GTK_LABEL(GTK_BIN(item_w)->child), user_name_of(entry->data));
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu_w), item_w);

		if(IS_MESSAGEABLE_MODE(usermode)) {
			g_object_set_data_full(
				G_OBJECT(item_w), "username",
				g_strdup(user_name_of(entry->data)), (GDestroyNotify)g_free);
		} else {
			gtk_widget_set_sensitive(item_w, FALSE);
		}
	}
	
	g_list_free(list);

	return submenu_w;
}

static void
tray_popup_bare_event_cb(GtkMenuItem * item_w, gpointer event)
{
	raise_event(GPOINTER_TO_INT(event), NULL, 0);
}

static void
tray_popup_menu(guint event_button, guint32 event_time)
{
	GtkWidget * menu_w, * submenu_w, * item_w;
	gint user_count;

	menu_w = gtk_menu_new();

	/* "Send message to >" submenu */
	item_w = util_image_menu_item(NULL, _("Message to"), NULL, NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);

	submenu_w = tray_popup_menu_message_submenu(&user_count);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_w), submenu_w);
	if(!user_count)
		gtk_widget_set_sensitive(item_w, FALSE);

	/* "Change mode to >" submenu */
	item_w = gtk_menu_item_new_with_label(_("Set mode"));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_w), tray_popup_menu_mode_submenu());
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);

	/* "Preferences.." */
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), gtk_separator_menu_item_new());

	item_w = util_image_menu_item(
		GTK_STOCK_PREFERENCES, _("Preferences.."),
		G_CALLBACK(tray_popup_bare_event_cb),
		GINT_TO_POINTER(EVENT_IFACE_SHOW_CONFIGURE_DLG));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);

	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), gtk_separator_menu_item_new());

	/* "Quit" */
	item_w = util_image_menu_item(
		GTK_STOCK_QUIT, _("Quit"),
		G_CALLBACK(tray_popup_bare_event_cb), GINT_TO_POINTER(EVENT_IFACE_EXIT));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);

	/* show menu */
	gtk_widget_show_all(menu_w);
	gtk_menu_popup(GTK_MENU(menu_w), NULL, NULL, NULL, NULL, event_button, event_time);
}

/**
 * update_tray_icon:
 *	updates tray icon to current state
 */
static void
update_tray_icon()
{
	g_assert(tray_created);

	tray_impl->set_icon(tray_blink_state, my_mode());
}

static void
create_tray()
{
	/* tooltip strings */
	tray_tooltip_strings_num = 0;
	tray_tooltip_strings = NULL;
	tray_blink_timeout = 0;

	/* create and embed the widget itself */
	tray_embedded = FALSE;
	tray_impl->create(PACKAGE);
	tray_created = TRUE;

	/* update tray icon contents */
	tray_blink_state = FALSE;
	update_tray_icon();
}

static void
destroy_tray()
{
	if(tray_blink_timeout)
		tray_stop_blinking();

	tray_impl->destroy();

	/* delete tooltip structs */
	util_list_free_with_data(tray_tooltip_strings, (GDestroyNotify)g_free);
	tray_tooltip_strings = NULL;
	tray_tooltip_strings_num = 0;
}

/**
 * tray_blink_trigger:
 *	triggers blinking of tray icon on specific session text events
 */
static void
tray_blink_trigger(
	sess_id session,
	const char * text,
	enum session_text_type text_type)
{
	gboolean blink;

	g_assert(tray_created);

	switch(sess_type(session)) {
	case SESSTYPE_STATUS:
		blink = prefs_bool(PREFS_GUI_TRAY_TRIGGERS_STATUS);
		break;
	case SESSTYPE_PRIVATE:
		blink = (prefs_bool(PREFS_GUI_TRAY_TRIGGERS_PRIVATE)
				&& (text_type==SESSTEXT_THEIR_TEXT || text_type==SESSTEXT_THEIR_ME))
			
			|| (prefs_bool(PREFS_GUI_TRAY_TRIGGERS_JOIN_LEAVE)
				&& (text_type==SESSTEXT_JOIN || text_type==SESSTEXT_LEAVE));
		break;
	case SESSTYPE_CHANNEL:
		blink = (prefs_bool(PREFS_GUI_TRAY_TRIGGERS_TOPIC)
				&& text_type==SESSTEXT_TOPIC)
			
			|| (prefs_bool(PREFS_GUI_TRAY_TRIGGERS_JOIN_LEAVE)
				&& (text_type==SESSTEXT_JOIN || text_type==SESSTEXT_LEAVE))

			|| (prefs_bool(PREFS_GUI_TRAY_TRIGGERS_CHANNEL)
				&& (text_type==SESSTEXT_THEIR_TEXT ||text_type==SESSTEXT_THEIR_ME));
		break;
	}

	if(blink)
		tray_start_blinking();
}

/**
 * tray_update_tooltip
 *	appends new line to tray icon popup
 */
static void
tray_update_tooltip(gpointer session, const gchar * text)
{
	GList * entry;
	GString * tooltip_text;

	g_assert(tray_created);
	g_assert(session && text);

	/* check if we can set the tooltip */
	if(! tray_impl->set_tooltip)
		return;

	/* don't write the same line twice
	 *  if 'duplicate status messages on the active tab' is enabled
	 */
	if(prefs_bool(PREFS_MAIN_LOG_GLOBAL)
			&& sess_type(session)==SESSTYPE_STATUS
			&& sess_type(sess_current())!=SESSTYPE_STATUS) {
		return;	
	}

	/* remove old entries so we keep inside specified limit of `PREFS_TRAY_TOOLTIP_LINES' */
	while(tray_tooltip_strings
			&& tray_tooltip_strings_num > (prefs_int(
					PREFS_GUI_TRAY_TOOLTIP_LINE_NUM)-1)) {
		g_free(tray_tooltip_strings->data);
		tray_tooltip_strings = g_list_delete_link(tray_tooltip_strings, tray_tooltip_strings);
		tray_tooltip_strings_num --;
	}

	if(prefs_int(PREFS_GUI_TRAY_TOOLTIP_LINE_NUM)) {
		/* append new line */
		tray_tooltip_strings = g_list_append(tray_tooltip_strings, g_strdup(text));
		tray_tooltip_strings_num ++;

		tooltip_text = g_string_new(NULL);

		for(entry = tray_tooltip_strings; entry; entry = entry->next) {
			/* ensure that tooltip line is not too long */
			gint line_len = g_utf8_strlen((const gchar*)entry->data, -1);
			if(line_len > TOOLTIP_MAX_LEN) {
				/* line is too long, append the beginning of the line only */
				g_string_append_len(
					tooltip_text, (const gchar*)entry->data,
					g_utf8_offset_to_pointer(
						(const gchar*)entry->data, TOOLTIP_MAX_LEN - 2)
					- (const gchar*)entry->data
				);
				g_string_append(tooltip_text, "..");
			} else {			
				g_string_append(tooltip_text, (const gchar*)entry->data);
			}
			if(entry->next)
				g_string_append_c(tooltip_text, '\n');
		}

		/* update tooltip text */
		tray_impl->set_tooltip(tooltip_text->str);

		g_string_free(tooltip_text, TRUE);
	} else {
		/* disable the tooltip as there are no lines do display
		 */
		tray_impl->set_tooltip(NULL);
	}
}

static void
tray_prefs_gui_tray_enable_changed_cb(const gchar * prefs_name)
{
	if(app_status() >= APP_START) {
		if(prefs_bool(PREFS_GUI_TRAY_ENABLE))
			create_tray();
		else	destroy_tray();
	}
}

static void
tray_register_prefs()
{
	prefs_register(PREFS_GUI_TRAY_ENABLE, PREFS_TYPE_BOOL,
		_("Enable system tray icon"), NULL, NULL);
	prefs_register(PREFS_GUI_TRAY_TRIGGERS_JOIN_LEAVE, PREFS_TYPE_BOOL,
		_("Tray blinks when someone joins/leaves a channel"), NULL, NULL);
	prefs_register(PREFS_GUI_TRAY_TRIGGERS_CHANNEL, PREFS_TYPE_BOOL,
		_("Tray blinks on new channel text"), NULL, NULL);
	prefs_register(PREFS_GUI_TRAY_TRIGGERS_PRIVATE, PREFS_TYPE_BOOL,
		_("Tray blinks on new private text"), NULL, NULL);
	prefs_register(PREFS_GUI_TRAY_TRIGGERS_STATUS, PREFS_TYPE_BOOL,
		_("Tray blinks on new status text"), NULL, NULL);
	prefs_register(PREFS_GUI_TRAY_TRIGGERS_TOPIC, PREFS_TYPE_BOOL,
		_("Tray blinks on channel topic change"), NULL, NULL);
	prefs_register(PREFS_GUI_TRAY_HIDE_WND_ON_STARTUP, PREFS_TYPE_BOOL,
		_("Hide main window on startup"), NULL, NULL);
	prefs_register(PREFS_GUI_TRAY_TOOLTIP_LINE_NUM, PREFS_TYPE_UINT,
		_("Number of lines on tray icon tooltip"), NULL, NULL);
}

static void
tray_embedded_cb()
{
	tray_embedded = TRUE;
	raise_event(EVENT_IFACE_TRAY_EMBEDDED, NULL, 0);
}

static void
tray_removed_cb()
{
	tray_embedded = FALSE;
	raise_event(EVENT_IFACE_TRAY_REMOVED, NULL, 0);
}

static void
tray_clicked_cb(guint button, guint32 time)
{
	g_assert(tray_created && tray_embedded);

	if(button==1) {
		/* show hide on left click */
		raise_event(EVENT_IFACE_TRAY_CLICK, NULL, 0);
	}
	else if(button==3) {
		tray_popup_menu(button, time);
	}
}

static void
tray_prefs_main_mode_changed_cb(const gchar * prefs_name)
{
	if(tray_created)
		update_tray_icon();
}

/**
 * tray_event_cb:
 *	handles application events
 */
static void
tray_event_cb(enum app_event_enum e, gpointer p, int i)
{
	switch(e) {
	case EVENT_MAIN_INIT:
		tray_created = tray_embedded = FALSE;

		tray_impl = tray_impl_init();
		tray_impl->set_embedded_notifier(tray_embedded_cb);
		tray_impl->set_removed_notifier(tray_removed_cb);
		tray_impl->set_clicked_notifier(tray_clicked_cb);
		break;

	case EVENT_MAIN_REGISTER_PREFS:
		tray_register_prefs();
		break;

	case EVENT_MAIN_PRESET_PREFS:
		prefs_add_notifier(PREFS_GUI_TRAY_ENABLE,
			(GHookFunc)tray_prefs_gui_tray_enable_changed_cb);
		prefs_add_notifier(PREFS_MAIN_MODE, (GHookFunc)tray_prefs_main_mode_changed_cb);

		prefs_set(PREFS_GUI_TRAY_ENABLE, TRUE);
		prefs_set(PREFS_GUI_TRAY_TOOLTIP_LINE_NUM, 8);
		prefs_set(PREFS_GUI_TRAY_TRIGGERS_JOIN_LEAVE, TRUE);
		prefs_set(PREFS_GUI_TRAY_TRIGGERS_CHANNEL, TRUE);
		prefs_set(PREFS_GUI_TRAY_TRIGGERS_PRIVATE, TRUE);
		prefs_set(PREFS_GUI_TRAY_TRIGGERS_STATUS, TRUE);
		prefs_set(PREFS_GUI_TRAY_TRIGGERS_TOPIC, TRUE);
		break;

	case EVENT_MAIN_START:
		if(prefs_bool(PREFS_GUI_TRAY_ENABLE))
			create_tray();
		break;
		
	case EVENT_MAIN_PRECLOSE:
		tray_impl->destroy();
		break;

	case EVENT_SESSION_TEXT:
		if(tray_embedded) {
			tray_update_tooltip(EVENT_V(p, 0), EVENT_V(p, 1));

			if(!gui_is_active())
				tray_blink_trigger(EVENT_V(p, 0), EVENT_V(p, 1), (enum session_text_type)i);
		}
		break;
	case EVENT_IFACE_ACTIVE_CHANGE:
		if(tray_embedded && i)
			tray_stop_blinking();
		break;
	case EVENT_IFACE_TRAY_UMODE:
		prefs_set(PREFS_MAIN_MODE, GPOINTER_TO_INT(p));
		break;
	default:
		break;
	}
}

/**
 * gui_tray_is_embedded:
 *	returns TRUE, if we have icon on system tray visible to the user
 */
gboolean gui_tray_is_embedded()
{
	return tray_embedded;
}

/**
 * gui_tray_register:
 *	registers gui_tray module for some events
 */
void gui_tray_register()
{
	register_event_cb(tray_event_cb, EVENT_MAIN|EVENT_IFACE|EVENT_SESSION);
}
