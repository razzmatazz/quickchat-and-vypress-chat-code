/*
 * gui_ignore.c: ignore dialog
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
 * $Id: gui_ignore.c,v 1.9 2004/12/22 04:33:39 bobas Exp $
 */

#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkkeysyms.h>

#include "main.h"
#include "net.h"
#include "gui.h"
#include "user.h"
#include "prefs.h"
#include "gui_ignore.h"
#include "gui_misc.h"

/** widget geometries
  ************************************/
#define GEOM_LIST_LENGTH	240
#define GEOM_LIST_HEIGHT	160

/** structure definitions
  ************************************/
struct ignore_dlg {
	GtkWidget * window_w,
		* entry_w,
		* add_w, * remove_w, * remove_all_w,
		* list_w,
		* scrolled_w, * treeview_w;

	GtkListStore	* list_model;
};
#define PIGNORE_DLG(p)	((struct ignore_dlg*)p)

enum dlg_button_enum {
	BTN_ADD, BTN_REMOVE,
	BTN_REMOVE_ALL
};

enum list_column_enum {
	COLUMN_ICON,
	COLUMN_NICKNAME,
	COLUMN_NUM
};

struct find_iterator_data {
	const char * name_to_find;
	GtkTreeIter * p_iter;
	gboolean found_one;
};

/** static variables
  ************************************/
static struct ignore_dlg * the_dlg;

/** static routines
  ************************************/

static void show_ignore_dialog(struct ignore_dlg **);
static void destroy_ignore_dialog(struct ignore_dlg **);

static gboolean
find_iterator_for_cb(
	GtkTreeModel * model,
	GtkTreePath * path,
	GtkTreeIter * iter,
	struct find_iterator_data *p_data)
{
	GValue v = {0, };
	gtk_tree_model_get_value(model, iter, COLUMN_NICKNAME, &v);

	if(!nickname_cmp(g_value_get_string(&v), p_data->name_to_find)) {
		*p_data->p_iter = *iter;
		p_data->found_one = TRUE;
	}
	g_value_unset(&v);
	return p_data->found_one;
}

static gboolean
find_iterator_for(
	GtkListStore * model,
	const char * nickname,
	GtkTreeIter * p_iter)
{
	struct find_iterator_data data;

	data.name_to_find = nickname;
	data.p_iter = p_iter;
	data.found_one = FALSE;
	
	gtk_tree_model_foreach(
		GTK_TREE_MODEL(model),
		(GtkTreeModelForeachFunc)find_iterator_for_cb,
		(gpointer)&data);

	return data.found_one;
}

static void
update_ignore_list_icon_for(GtkListStore * model, gpointer user)
{
	GtkTreeIter where;
	const gchar * icon;
	
	if(find_iterator_for(model, user_name_of(user), &where)) {
		if(user)
			icon = util_user_state_stock(user_mode_of(user), user_is_active(user));
		else	icon = util_user_state_stock(UMODE_DEAD, FALSE);

		gtk_list_store_set(model, &where, 0, icon, -1);
	}
}

static void
ignore_list_insert(
	GtkListStore * list_model,
	const char * nickname)
{
	GtkTreeIter where;
	const gchar * icon;
	gpointer user;

	g_assert(list_model && nickname);

	user = user_by_name(nickname);
	if(user)
		icon = util_user_state_stock(user_mode_of(user), user_is_active(user));
	else	icon = util_user_state_stock(UMODE_DEAD, FALSE);

	gtk_list_store_append(list_model, &where);
	gtk_list_store_set(
		list_model, &where,
		COLUMN_ICON, icon,
		COLUMN_NICKNAME, nickname, -1);
}

static void
update_ignore_list(GtkListStore * list_model)
{
	GList * ignore_list, *l;

	g_assert(list_model);

	/* clean up current list - we didn't track changes
	 * while we were hidden
	 */
	gtk_list_store_clear(list_model);

	ignore_list = prefs_list(PREFS_NET_IGNORED_USERS);

	/* insert each entry of the list
	 *	into the store
	 */
	for(l=ignore_list; l; l=l->next)
		ignore_list_insert(list_model, (const char*)l->data);
}

static void
dlg_button_click_cb(
	GtkButton * btn_w,
	gpointer btn_num)
{
	const gchar * nickname = gtk_entry_get_text(GTK_ENTRY(the_dlg->entry_w));

	switch((enum dlg_button_enum)GPOINTER_TO_INT(btn_num)) {
	case BTN_ADD:
		prefs_list_add_unique(PREFS_NET_IGNORED_USERS, nickname);
		break;
	case BTN_REMOVE:
		prefs_list_remove(PREFS_NET_IGNORED_USERS, nickname);
		break;
	case BTN_REMOVE_ALL:
		prefs_list_clear(PREFS_NET_IGNORED_USERS);
		break;
	}

	/* mark the entire inserted string as selected */
	gtk_editable_select_region(GTK_EDITABLE(the_dlg->entry_w), 0, -1);

	/* return focus to entry widget */
	gtk_widget_grab_focus(the_dlg->entry_w);
}

/** dlg_entry_activate_cb:
  *	invoked on Enter keypress on edit widget
  */
static void
dlg_entry_activate_cb(
	GtkEntry * entry_w,
	struct ignore_dlg * dlg)
{
	if(g_utf8_strlen(gtk_entry_get_text(entry_w), -1)!=0)
		prefs_list_add_unique(PREFS_NET_IGNORED_USERS, gtk_entry_get_text(entry_w));
}

/** dlg_entry_changed_cb
  *	maintains state of ADD button
  *	depending on whether there's text in GtkEntry
  */
static void
dlg_entry_changed_cb(
	GtkEntry * entry_w,
	struct ignore_dlg * dlg)
{
	gboolean non_null = strlen(gtk_entry_get_text(entry_w))!=0;

	gtk_widget_set_sensitive(dlg->add_w, non_null);
	gtk_widget_set_sensitive(dlg->remove_w, non_null);
}

/** dlg_window_focus_out_cb:
 *	maintains focus on entry widget
 */
static gboolean
dlg_window_focus_out_cb(
	GtkWindow * wnd_w,
	GdkEventFocus * event,
	gpointer data)
{
	gtk_widget_grab_focus(PIGNORE_DLG(data)->entry_w);
	return FALSE;	/* propagate this event futher */
}

/**
 * dlg_window_key_press_cb:
 *	destroys dialog on `esc' keypress
 */
static gboolean
dlg_window_key_press_cb(
	GtkWidget * w, GdkEventKey * e,
	struct ignore_dlg * dlg)
{
	if(e->keyval==GDK_Escape) {
		gtk_dialog_response(GTK_DIALOG(the_dlg->window_w), GTK_RESPONSE_CANCEL);
		return TRUE;
	}
	return FALSE;
}

/**
 * dlg_window_response_handler
 *	handles dialog response from user
 */
static void
dlg_window_response_handler(
	GtkDialog * dialog,
	gint response,
	struct ignore_dlg * dlg)
{
	switch(response) {
	case GTK_RESPONSE_OK:
	case GTK_RESPONSE_CANCEL:
	case GTK_RESPONSE_DELETE_EVENT:
		destroy_ignore_dialog(&the_dlg);
	default:
		break;
	}
}

/** list_cursor_changed_cb:
  *	invoked on change of selected entry
  *	on the list
  */
static void
list_cursor_changed_cb(
	GtkTreeView * treeview_w,
	struct ignore_dlg * dlg)
{
	GtkTreePath * current_path;
	GtkTreeIter current_iter;
	GValue val = {0,};

	/* get current path */
	gtk_tree_view_get_cursor(treeview_w, &current_path, NULL);

	/* we can remove entries only when selected */
	gtk_widget_set_sensitive(dlg->remove_w, current_path!=NULL);

	/* update active iterator */
	if(current_path!=NULL) {
		/* build new iterator */
		gtk_tree_model_get_iter(
			GTK_TREE_MODEL(dlg->list_model),
			&current_iter, current_path);
		gtk_tree_path_free(current_path);

		/* get nickname @ selected entry */
		gtk_tree_model_get_value(
			GTK_TREE_MODEL(dlg->list_model), &current_iter,
			COLUMN_NICKNAME, &val);

		/* set entry with this name */
		gtk_entry_set_text(
			GTK_ENTRY(dlg->entry_w),
			g_value_get_string(&val));

		/* select all string */
		gtk_editable_select_region(GTK_EDITABLE(dlg->entry_w), 0, -1);
	
		g_value_unset(&val);
	}
}

static void
show_ignore_dialog(struct ignore_dlg ** pdlg)
{
	GtkWidget * f, * hb, * vb;
	struct ignore_dlg * dlg;
	GtkCellRenderer * icon_renderer;

	if(*pdlg) return;

	/* setup dlg struct */
	*pdlg = dlg = g_malloc(sizeof(struct ignore_dlg));

	/* setup widgets */
	dlg->window_w = gtk_dialog_new_with_buttons(
		_("Ignore list"), gui_get_main_window(),
		GTK_DIALOG_NO_SEPARATOR,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);

	g_signal_connect(G_OBJECT(dlg->window_w), "response",
		G_CALLBACK(dlg_window_response_handler), (gpointer)dlg);
	g_signal_connect(G_OBJECT(dlg->window_w), "focus-out-event",
		G_CALLBACK(dlg_window_focus_out_cb), (gpointer)dlg);
	g_signal_connect(G_OBJECT(dlg->window_w), "key-press-event",
		G_CALLBACK(dlg_window_key_press_cb), (gpointer)dlg);
	g_signal_connect(G_OBJECT(dlg->window_w), "delete-event",
		G_CALLBACK(gtk_true), FALSE);

	f = gtk_frame_new(_("Ignore list"));
	gtk_widget_show(f);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dlg->window_w)->vbox), f);

	vb = gtk_vbox_new(FALSE, 4);
	gtk_container_set_border_width(GTK_CONTAINER(vb), 4);
	gtk_widget_show(vb);
	gtk_container_add(GTK_CONTAINER(f), vb);

	dlg->entry_w = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(dlg->entry_w), MAX_NICK_LENGTH);
	g_signal_connect(G_OBJECT(dlg->entry_w), "changed",
			G_CALLBACK(dlg_entry_changed_cb), (gpointer)dlg);
	g_signal_connect(G_OBJECT(dlg->entry_w), "activate",
			G_CALLBACK(dlg_entry_activate_cb), (gpointer)dlg);
	gtk_widget_show(dlg->entry_w);
	gtk_box_pack_start(GTK_BOX(vb), dlg->entry_w, FALSE, FALSE, 0);

	/* add control button box */
	hb = gtk_hbox_new(FALSE, 2);
	gtk_container_set_border_width(GTK_CONTAINER(hb), 0);
	gtk_widget_show(hb);
	gtk_box_pack_start(GTK_BOX(vb), hb, FALSE, FALSE, 0);

	dlg->add_w = misc_pix_button(
		_("_Add"), GTK_STOCK_ADD,
		G_CALLBACK(dlg_button_click_cb), GINT_TO_POINTER(BTN_ADD));
	gtk_widget_set_sensitive(dlg->add_w, FALSE);
	gtk_box_pack_start(GTK_BOX(hb), dlg->add_w, FALSE, FALSE, 0);

	dlg->remove_w = misc_pix_button(
		_("_Remove"), GTK_STOCK_REMOVE,
		G_CALLBACK(dlg_button_click_cb), GINT_TO_POINTER(BTN_REMOVE));
	gtk_widget_set_sensitive(dlg->remove_w, FALSE);
	gtk_box_pack_start(GTK_BOX(hb), dlg->remove_w, FALSE, FALSE, 0);

	dlg->remove_all_w = misc_pix_button(
		_("_Clear list"), GTK_STOCK_CLEAR,
		G_CALLBACK(dlg_button_click_cb),
		GINT_TO_POINTER(BTN_REMOVE_ALL));
	gtk_box_pack_end(GTK_BOX(hb), dlg->remove_all_w, FALSE, FALSE, 0);

	/* make tree view */
	dlg->list_model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	dlg->scrolled_w = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_usize(dlg->scrolled_w, GEOM_LIST_LENGTH, GEOM_LIST_HEIGHT);
	gtk_widget_show(dlg->scrolled_w);
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW(dlg->scrolled_w),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vb), dlg->scrolled_w, TRUE, TRUE, 0);

	dlg->treeview_w = gtk_tree_view_new();
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(dlg->treeview_w),FALSE);
	gtk_tree_view_set_model(
		GTK_TREE_VIEW(dlg->treeview_w),
		GTK_TREE_MODEL(dlg->list_model));

	icon_renderer = gtk_cell_renderer_pixbuf_new();
	g_object_set(G_OBJECT(icon_renderer), "stock-size", GTK_ICON_SIZE_MENU, NULL);
	gtk_tree_view_insert_column_with_attributes(
		GTK_TREE_VIEW(dlg->treeview_w), -1, NULL,
		icon_renderer, "stock-id", COLUMN_ICON, NULL);
	gtk_tree_view_insert_column_with_attributes(
		GTK_TREE_VIEW(dlg->treeview_w),
		-1, NULL, gtk_cell_renderer_text_new(),
		"text", COLUMN_NICKNAME, NULL);
	gtk_tree_sortable_set_sort_column_id(
		GTK_TREE_SORTABLE(dlg->list_model),
		COLUMN_NICKNAME, GTK_SORT_ASCENDING
	);
	g_signal_connect(G_OBJECT(dlg->treeview_w), "cursor-changed",
			G_CALLBACK(list_cursor_changed_cb), (gpointer)dlg);

	gtk_widget_show(dlg->treeview_w);
	gtk_scrolled_window_add_with_viewport(
		GTK_SCROLLED_WINDOW(dlg->scrolled_w),
		dlg->treeview_w);

	/* show up the dialog */
	update_ignore_list(dlg->list_model);
	gtk_window_present(GTK_WINDOW(dlg->window_w));
}

static void
destroy_ignore_dialog(struct ignore_dlg ** pdlg)
{
	g_assert(pdlg);

	if(*pdlg) {
		gtk_widget_destroy(GTK_WIDGET((*pdlg)->window_w));
		g_free(*pdlg);
		*pdlg = NULL;
	}
}

static void
ignore_prefs_ignore_list_changed_cb(const gchar * pref_name)
{
	if(the_dlg)
		update_ignore_list(the_dlg->list_model);
}

static void
ignore_event_handler(
	enum app_event_enum event,
	gpointer p, gint i)
{
	switch(event) {
	case EVENT_MAIN_INIT:
		the_dlg = NULL;
		break;
	case EVENT_MAIN_START:
		prefs_add_notifier(
			PREFS_NET_IGNORED_USERS,
			(GHookFunc)ignore_prefs_ignore_list_changed_cb);
		break;
	case EVENT_MAIN_PRECLOSE:
		break;
	case EVENT_MAIN_CLOSE:
		destroy_ignore_dialog(&the_dlg);
		break;
	case EVENT_IFACE_SHOW_IGNORE_DLG:
		show_ignore_dialog(&the_dlg);
		break;
	case EVENT_IFACE_USER_IGNORE_REQ:
		prefs_list_add_unique(PREFS_NET_IGNORED_USERS, user_name_of(p));
		break;
	case EVENT_USER_NEW:
	case EVENT_USER_RENAME:
		if(the_dlg)
			update_ignore_list_icon_for(the_dlg->list_model, EVENT_V(p, 0));
		break;
	case EVENT_USER_REMOVED:
	case EVENT_USER_MODE_CHANGE:
		if(the_dlg)
			update_ignore_list_icon_for(the_dlg->list_model, p);
		break;
	default:
		break;	/* nothing else */
	}
}

/** exported routines
  *****************************************/
void
gui_ignore_dlg_register(void)
{
	register_event_cb(ignore_event_handler, EVENT_MAIN|EVENT_IFACE|EVENT_USER);
}

