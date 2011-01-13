/*
 * gui_channel.c: implements channel dialog
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
 * $Id: gui_channel.c,v 1.21 2004/12/22 04:33:39 bobas Exp $
 */

#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "main.h"
#include "prefs.h"
#include "sess.h"
#include "gui.h"
#include "gui_misc.h"
#include "gui_channel.h"
#include "util.h"

/* widget geometries
 *------------------------------------*/
#define GEOM_ENTRY_LENGTH	128
#define GEOM_LIST_HEIGHT	160

/* struct defs & enumerations
 *------------------------------------*/
struct channel_dlg {
	GtkWidget * window_w, * entry_w, * list_w, * join_w, * update_w;
	GtkListStore * list_model;
};

enum list_column_enum {
	CHANNEL_COLUMN_ICON,
	CHANNEL_COLUMN_NAME,
	CHANNEL_COLUMN_PERSISTENT,
	CHANNEL_COLUMN_IMPORTANT,
	CHANNEL_COLUMN_NUM
};

/* static vars
 */
static struct channel_dlg * the_dlg;

/* static routines
 */

static void destroy_channel_dialog(struct channel_dlg **);

static void
dlg_add_channel(struct channel_dlg * dlg, const gchar * name)
{
	GtkTreeIter iter;
	gboolean found_channel;

	g_assert(dlg && name);

	/* find out if the channel already exists in the list
	 */
	found_channel = FALSE;
	if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(dlg->list_model), &iter)) {
		do {
			gint cmp;
			gchar * chan_name;

			gtk_tree_model_get(
				GTK_TREE_MODEL(dlg->list_model), &iter,
				CHANNEL_COLUMN_NAME, &chan_name, -1);
			cmp = g_utf8_collate(chan_name, name);
			g_free(chan_name);

			if(!cmp) {
				found_channel = TRUE;
				break;
			}
		}
		while(gtk_tree_model_iter_next(GTK_TREE_MODEL(dlg->list_model), &iter));
	}

	/* add this channel to the list, if not present already */
	if(!found_channel)
		gtk_list_store_append(dlg->list_model, &iter);

	gtk_list_store_set(
		dlg->list_model, &iter,
		CHANNEL_COLUMN_ICON, sess_find(SESSTYPE_CHANNEL, name) ? GUI_STOCK_CHANNEL: GUI_STOCK_CHANNEL_INACTIVE,
		CHANNEL_COLUMN_NAME, name,
		CHANNEL_COLUMN_PERSISTENT, prefs_list_contains(PREFS_MAIN_PERSISTENT_CHANNELS, name),
		CHANNEL_COLUMN_IMPORTANT, prefs_list_contains(PREFS_SESS_IMPORTANT_CHANNELS, name),
		-1);

}

static void
dlg_remove_channel(struct channel_dlg * dlg, const gchar * name)
{
	GtkTreeIter iter;
	gint cmp;
	gchar * chan_name;
	gboolean chan_persistent;

	if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(dlg->list_model), &iter)) {
		do {
			gtk_tree_model_get(
				GTK_TREE_MODEL(dlg->list_model), &iter,
				CHANNEL_COLUMN_NAME, &chan_name,
				CHANNEL_COLUMN_PERSISTENT, &chan_persistent,
				-1);
			cmp = g_utf8_collate(chan_name, name);
			g_free(chan_name);

			if(cmp==0) {
				/* found the channel we need to remove:
				 *	check if it's not persistent
				 */
				if(!chan_persistent) {
					/* ok, the channel is not persistent, we can remove it safely */
					gtk_list_store_remove(dlg->list_model, &iter);
				} else {
					/* the channel is persistent, mark it as inactive */
					gtk_list_store_set(
						GTK_LIST_STORE(dlg->list_model), &iter,
						CHANNEL_COLUMN_ICON, GUI_STOCK_CHANNEL_INACTIVE, -1);
				}
				break;
			}
		}
		while(gtk_tree_model_iter_next(GTK_TREE_MODEL(dlg->list_model), &iter));
	}
}

static void
active_channel_enum_cb(
	sess_id session,
	enum session_type type,
	const gchar * name,
	gpointer uid, gpointer dlg)
{
	if(type==SESSTYPE_CHANNEL)
		dlg_add_channel((struct channel_dlg*)dlg, name);
}

static void
dlg_add_persistent_and_important_channels(struct channel_dlg * dlg)
{
	GList * persistent = prefs_list(PREFS_MAIN_PERSISTENT_CHANNELS);
	GList * important = prefs_list(PREFS_SESS_IMPORTANT_CHANNELS);

	for(; persistent; persistent = persistent->next) {
		const gchar * channel = (const gchar*)persistent->data;
		
		if(g_utf8_strlen(channel, -1))
			dlg_add_channel(dlg, channel);
	}

	for(; important; important = important->next) {
		const gchar * channel = (const gchar*)important->data;

		if(g_utf8_strlen(channel, -1))
			dlg_add_channel(dlg, channel);
	}
}

static void
dlg_update_channel_list(struct channel_dlg * dlg)
{
	g_assert(dlg);

	/* clear current list */
	gtk_list_store_clear(dlg->list_model);

	/* fetch channels that we are in */
	sess_enumerate(active_channel_enum_cb, (void*)dlg);

	/* fetch persistent channels */
	dlg_add_persistent_and_important_channels(dlg);

	/* request peers on the network for their channel list
	 * (we'll parse them at dlg_handle_net_channel_update())
	 */
	raise_event(EVENT_IFACE_REQUEST_NET_CHANNELS, NULL, 0);
}

static void
dlg_handle_net_channel_update(
	struct channel_dlg * dlg,
	GList * channels)
{
	g_assert(dlg && channels);

	for(; channels; channels = channels->next)
		dlg_add_channel(dlg, (const char*)channels->data);
}

static void
channel_dlg_button_click_cb(
	GtkWidget * btn_w,
	struct channel_dlg * dlg)
{
	if(btn_w==dlg->join_w && g_utf8_strlen(gtk_entry_get_text(GTK_ENTRY(dlg->entry_w)), -1)) {
		/* click on "Join" button */
		raise_event(
			EVENT_IFACE_JOIN_CHANNEL,
			(gpointer)gtk_entry_get_text(GTK_ENTRY(dlg->entry_w)), 0);
	}
	else if(btn_w==dlg->update_w) {
		/* click on "Update" button */
		dlg_update_channel_list(dlg);
	}

	/* return focus to entry widget */
	gtk_widget_grab_focus(dlg->entry_w);
	gtk_entry_select_region(GTK_ENTRY(dlg->entry_w), 0, -1);
}

static gboolean
channel_dlg_focus_out_event(
	GtkWindow * window_w,
	GdkEventFocus * event,
	struct channel_dlg * dlg)
{
	/* entry widget always retains focus */
	gtk_widget_grab_focus(dlg->entry_w);
	
	return FALSE;	/* propagate the event futher */
}

static gboolean
channel_dlg_key_press_event(
	GtkWidget * window_w,
	GdkEventKey * event,
	struct channel_dlg * dlg)
{
	if(event->keyval==GDK_Escape) {
		gtk_dialog_response(GTK_DIALOG(the_dlg->window_w), GTK_RESPONSE_CANCEL);
		return TRUE;
	}
	return FALSE;
}

static void
channel_list_select_cb(
	GtkTreeView * treeview_w,
	struct channel_dlg * dlg)
{
	GtkTreeSelection * tsel;
	GtkTreeIter iter;
	GValue val = {0,};

	/* check if there's a selected entry in list */
	tsel = gtk_tree_view_get_selection(treeview_w);
	if (gtk_tree_selection_get_selected(tsel, NULL, &iter))
	{
		/* set entry with the channel's name */
		gtk_tree_model_get_value(
			GTK_TREE_MODEL(dlg->list_model), &iter,
			CHANNEL_COLUMN_NAME, &val);

		if(g_value_get_string(&val)) {
			gtk_entry_set_text(GTK_ENTRY(dlg->entry_w), g_value_get_string(&val));
			gtk_entry_select_region(GTK_ENTRY(dlg->entry_w), 0, -1);
		}

		g_value_unset(&val);
		
		/* enable join button */
		gtk_widget_set_sensitive(dlg->join_w, TRUE);
	}
}

static gboolean
channel_list_button_press_event(
	GtkWidget * list_w,
	GdkEventButton * event,
	struct channel_dlg * dlg)
{
	GtkTreeSelection * sel;
	GtkTreeIter iter;
	GValue val = {0,};

	if(event->type==GDK_2BUTTON_PRESS && event->button==1) {
		/* join the selected channel on doubleclick
		 * with left mouse button..
		 */
		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(dlg->list_w));
		if(gtk_tree_selection_get_selected(sel, NULL, &iter)) {
			/* get channel name */
			gtk_tree_model_get_value(
				GTK_TREE_MODEL(dlg->list_model), &iter,
				CHANNEL_COLUMN_NAME, &val);

			if(g_value_get_string(&val)) {
				raise_event(
					EVENT_IFACE_JOIN_CHANNEL,
					(gpointer)g_value_get_string(&val), 0);
			}
			g_value_unset(&val);
		}
	}
	return FALSE;
}

static void
channel_entry_changed_event(
	GtkEntry * entry_w,
	struct channel_dlg * dlg)
{
	gtk_widget_set_sensitive(
		dlg->join_w,
		g_utf8_strlen(gtk_entry_get_text(entry_w), -1) != 0
	);
}

static void
channel_entry_activate_event(
	GtkEntry * entry_w,
	struct channel_dlg * dlg)
{
	if(strlen(gtk_entry_get_text(entry_w))!=0) {
		raise_event(EVENT_IFACE_JOIN_CHANNEL, (gpointer)gtk_entry_get_text(entry_w), 0 );
		destroy_channel_dialog(&the_dlg);
	}
}

static void
channel_dlg_response_handler(
	GtkDialog * dialog_w,
	gint response,
	struct channel_dlg * dlg)
{
	GtkTreeIter iter;
	gchar * channel;
	gboolean persistent, important;
	GList * persistent_channels, * important_channels;

	switch(response) {
	case GTK_RESPONSE_OK:
		/* store the list of persistent channels in PREFS_MAIN_PERSISTENT_CHANNELS
		 * and the list of important channels to PREFS_SESS_IMPORTANT_CHANNELS
		 */

		persistent_channels = important_channels = NULL;
		if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(dlg->list_model), &iter)) {
			do {
				gtk_tree_model_get(GTK_TREE_MODEL(dlg->list_model), &iter,
					CHANNEL_COLUMN_NAME, &channel,
					CHANNEL_COLUMN_PERSISTENT, &persistent,
					CHANNEL_COLUMN_IMPORTANT, &important, -1);

				if(persistent)
					persistent_channels = g_list_append(
						persistent_channels, g_strdup(channel));

				if(important)
					important_channels = g_list_append(
						important_channels, g_strdup(channel));

				g_free(channel);
			} while(gtk_tree_model_iter_next(GTK_TREE_MODEL(dlg->list_model), &iter));
		}
		prefs_set(PREFS_MAIN_PERSISTENT_CHANNELS, persistent_channels);
		util_list_free_with_data(persistent_channels, g_free);

		prefs_set(PREFS_SESS_IMPORTANT_CHANNELS, important_channels);
		util_list_free_with_data(important_channels, g_free);

		/* FALLTROUGH */
	case GTK_RESPONSE_CANCEL:
	case GTK_RESPONSE_DELETE_EVENT:
		/* don't delete the dialog, just hide it */
		destroy_channel_dialog(&the_dlg);
		break;
	}
}

static void
channel_list_persistent_toggled_cb(
	GtkCellRendererToggle * renderer,
	gchar * path_str,
	struct channel_dlg * dlg)
{
	GtkTreeIter iter;
	gboolean persistent;

	gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(dlg->list_model), &iter, path_str);

	/* toggle the value of CHANNEL_COLUMN_PERSISTENT */
	gtk_tree_model_get(
		GTK_TREE_MODEL(dlg->list_model), &iter,
		CHANNEL_COLUMN_PERSISTENT, &persistent, -1);

	gtk_list_store_set(dlg->list_model, &iter, CHANNEL_COLUMN_PERSISTENT, !persistent, -1);
}

static void
channel_list_important_toggled_cb(
	GtkCellRendererToggle * renderer,
	gchar * path_str,
	struct channel_dlg * dlg)
{
	GtkTreeIter iter;
	gboolean important;

	gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(dlg->list_model), &iter, path_str);

	/* toggle the value of CHANNEL_COLUMN_IMPORTANT */
	gtk_tree_model_get(
		GTK_TREE_MODEL(dlg->list_model), &iter,
		CHANNEL_COLUMN_IMPORTANT, &important, -1);

	gtk_list_store_set(dlg->list_model, &iter, CHANNEL_COLUMN_IMPORTANT, !important, -1);
}

static void
show_channel_dialog(struct channel_dlg ** pdlg)
{
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	GtkWidget * frame, * vbox, * hbox, * w, * scrolled;
	struct channel_dlg * dlg;

	/* check if the dialog is already shown */
	if (*pdlg) return;

	*pdlg = dlg = g_malloc(sizeof(struct channel_dlg));

	/* create main window */
	dlg->window_w = gtk_dialog_new_with_buttons(
		_("Channels"), gui_get_main_window(),
		GTK_DIALOG_NO_SEPARATOR,
		GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	
	g_signal_connect(G_OBJECT(dlg->window_w), "response",
		G_CALLBACK(channel_dlg_response_handler), (gpointer)dlg);
	g_signal_connect(G_OBJECT(dlg->window_w), "focus-out-event",
		G_CALLBACK(channel_dlg_focus_out_event), (gpointer)dlg);
	g_signal_connect(G_OBJECT(dlg->window_w), "key-press-event",
		G_CALLBACK(channel_dlg_key_press_event), (gpointer)dlg);
	g_signal_connect(G_OBJECT(dlg->window_w), "delete-event",
		G_CALLBACK(gtk_true), NULL);

	/* make large frame */
	frame = gtk_frame_new("Channels");
	gtk_widget_show(frame);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dlg->window_w)->vbox), frame);

	/* insert main vbox */
	vbox = gtk_vbox_new(FALSE, 2);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	/* add entry/button box */
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	/* add '#' label */
	w = gtk_label_new("#");
	gtk_widget_show(w);
	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 0);

	/* add channel entry */
	dlg->entry_w = gtk_entry_new();
	gtk_widget_set_size_request(
		GTK_WIDGET(dlg->entry_w),
		GEOM_ENTRY_LENGTH, -1);
	gtk_widget_show(dlg->entry_w);
	gtk_entry_set_max_length(GTK_ENTRY(dlg->entry_w), MAX_CHAN_LENGTH);
	g_signal_connect(G_OBJECT(dlg->entry_w), "changed",
		G_CALLBACK(channel_entry_changed_event), (gpointer)dlg);
	g_signal_connect(G_OBJECT(dlg->entry_w), "activate",
		G_CALLBACK(channel_entry_activate_event), (gpointer)dlg);
	gtk_box_pack_start(GTK_BOX(hbox), dlg->entry_w, TRUE, TRUE, 0);

	dlg->update_w = misc_pix_button(
		_("_Update"), GTK_STOCK_REFRESH,
		G_CALLBACK(channel_dlg_button_click_cb), (gpointer)dlg);
	gtk_box_pack_end(GTK_BOX(hbox), dlg->update_w, FALSE, FALSE, 0);

	dlg->join_w = misc_pix_button(
		_("_Join"), GUI_STOCK_CHANNEL,
		G_CALLBACK(channel_dlg_button_click_cb), (gpointer)dlg);
	gtk_widget_set_sensitive(dlg->join_w, FALSE);
	gtk_box_pack_end(GTK_BOX(hbox), dlg->join_w, FALSE, FALSE, 0);

	/* add list box */
	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(
		GTK_CONTAINER(scrolled), 4);
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW(scrolled),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(
		GTK_SCROLLED_WINDOW(scrolled),
		GTK_SHADOW_IN);
	gtk_widget_show(scrolled);
	gtk_box_pack_end(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
	
	dlg->list_w = gtk_tree_view_new();
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(dlg->list_w), TRUE);
	gtk_widget_set_size_request(dlg->list_w, -1, GEOM_LIST_HEIGHT);
	gtk_widget_show(dlg->list_w);
	gtk_container_add(GTK_CONTAINER(scrolled), dlg->list_w);
	g_signal_connect(G_OBJECT(dlg->list_w), "cursor-changed",
			G_CALLBACK(channel_list_select_cb), (gpointer)dlg);
	g_signal_connect(G_OBJECT(dlg->list_w), "button-press-event",
			G_CALLBACK(channel_list_button_press_event),
			(gpointer)dlg);

	/* create sorted list model */
	dlg->list_model = gtk_list_store_new(
			CHANNEL_COLUMN_NUM,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, -1);
	gtk_tree_sortable_set_sort_column_id(
		GTK_TREE_SORTABLE(dlg->list_model),
		CHANNEL_COLUMN_NAME, GTK_SORT_ASCENDING );

	/* link this model to tree view */
	gtk_tree_view_set_model(
		GTK_TREE_VIEW(dlg->list_w),
		GTK_TREE_MODEL(dlg->list_model));

	/* channel icon renderer */
	renderer = gtk_cell_renderer_pixbuf_new();
	g_object_set(G_OBJECT(renderer), "stock-size", GTK_ICON_SIZE_MENU, NULL);
	gtk_tree_view_insert_column_with_attributes(
		GTK_TREE_VIEW(dlg->list_w), -1, NULL,
		renderer,
		"stock-id", CHANNEL_COLUMN_ICON, NULL);

	/* channel name renderer */
	gtk_tree_view_insert_column_with_attributes(
		GTK_TREE_VIEW(dlg->list_w), -1, _("Channel name"),
		gtk_cell_renderer_text_new(),
		"text", CHANNEL_COLUMN_NAME, NULL);
	column = gtk_tree_view_get_column(GTK_TREE_VIEW(dlg->list_w), 1);
	g_object_set(G_OBJECT(column), "resizable", TRUE, "expand", TRUE, NULL);
	
	/* "persistent" toggle renderer */
	renderer = gtk_cell_renderer_toggle_new();
	g_signal_connect(G_OBJECT(renderer),
		"toggled", G_CALLBACK(channel_list_persistent_toggled_cb), dlg);
	gtk_tree_view_insert_column_with_attributes(
		GTK_TREE_VIEW(dlg->list_w), -1, _("Persistent"),
		renderer, "active", CHANNEL_COLUMN_PERSISTENT, NULL);

	/* "important" toggle renderer */
	renderer = gtk_cell_renderer_toggle_new();
	g_signal_connect(G_OBJECT(renderer),
		"toggled", G_CALLBACK(channel_list_important_toggled_cb), dlg);
	gtk_tree_view_insert_column_with_attributes(
		GTK_TREE_VIEW(dlg->list_w), -1, _("Important"),
		renderer, "active", CHANNEL_COLUMN_IMPORTANT, NULL);

	/* present it to the user */
	gtk_window_present(GTK_WINDOW(dlg->window_w));
	gtk_widget_grab_focus(dlg->entry_w);
	dlg_update_channel_list(dlg);
}

static void
destroy_channel_dialog(struct channel_dlg ** pdlg)
{
	g_assert(pdlg);

	if(*pdlg) {
		/* destroy window */
		gtk_widget_destroy((*pdlg)->window_w);

		/* delete channel GtkListStore (which is a GObject) */
		g_object_unref(G_OBJECT((*pdlg)->list_model));

		/* delete the struct */
		g_free(*pdlg);
		*pdlg = NULL;
	}
}

static void
gui_channel_prefs_channel_list_changed(const gchar * pref_name)
{
	if(the_dlg)
		dlg_update_channel_list(the_dlg);
}

static void
gui_channel_event_cb(
	enum app_event_enum e,
	gpointer p, gint i)
{
	switch(e) {
	case EVENT_MAIN_INIT:
		the_dlg = NULL;
		break;

	case EVENT_MAIN_PRESET_PREFS:
		prefs_add_notifier(
			PREFS_MAIN_PERSISTENT_CHANNELS,
			(GHookFunc)gui_channel_prefs_channel_list_changed);
		prefs_add_notifier(
			PREFS_SESS_IMPORTANT_CHANNELS,
			(GHookFunc)gui_channel_prefs_channel_list_changed);
		break;

	case EVENT_MAIN_CLOSE:
		destroy_channel_dialog(&the_dlg);
		break;
	case EVENT_IFACE_SHOW_CHANNEL_DLG:
		show_channel_dialog(&the_dlg);
		break;
	case EVENT_NET_MSG_CHANNEL_LIST:
		if(the_dlg)
			dlg_handle_net_channel_update(the_dlg, (GList*)p);
		break;
	case EVENT_SESSION_OPENED:
		if(the_dlg && ((enum session_type)i)==SESSTYPE_CHANNEL)
			dlg_add_channel(the_dlg, sess_name((sess_id)p));
		break;
	case EVENT_SESSION_CLOSED:
		if(the_dlg && ((enum session_type)i)==SESSTYPE_CHANNEL)
			dlg_remove_channel(the_dlg, sess_name((sess_id)p));
		break;
	case EVENT_NET_DISCONNECTED:
		if(the_dlg)
			destroy_channel_dialog(&the_dlg);
		break;

	default:break;
	}
}

/* exported routines
 *------------------------------------*/

/** gui_channel_register:
  *	registers event callback handler for this module
  */
void
gui_channel_register()
{
	register_event_cb(
		gui_channel_event_cb,
		EVENT_MAIN|EVENT_NET|EVENT_IFACE|EVENT_SESSION|EVENT_NET);
}

