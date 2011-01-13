/*
 * gui_ulist.c: user list widget implementation
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
 * $Id: gui_ulist.c,v 1.32 2004/12/23 19:40:00 bobas Exp $
 */

#define IN_GUI		1

#include <string.h>
#include <time.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "main.h"
#include "prefs.h"
#include "user.h"
#include "sess.h"
#include "net.h"
#include "gui.h"
#include "gui_ulist.h"
#include "gui_misc.h"
#include "util.h"

#define TIP_MAX_WIDTH	300
#define TIP_BORDER	4
#define TIP_OFFSET_X	8
#define TIP_OFFSET_Y	8
#define TIP_MAX_CHANNELS 4

#define ULIST_MODE(user)	(user ? user_mode_of(user): prefs_int(PREFS_MAIN_MODE))
#define ULIST_NAME(user)	(user ? user_name_of(user): prefs_str(PREFS_MAIN_NICKNAME))
#define ULIST_ACTIVE(user)	(user ? user_is_active(user): gui_is_active())

enum user_list_column_enum {
	COLUMN_ICON,
	COLUMN_NICKNAME,
	COLUMN_USER_PTR,
	COLUMN_NUM
};

/** static vars
  **********************************************/
static GtkListStore	* ulist_model;

/* user info tip */
static gint		info_tip_timeout;
static GtkWidget	* info_tip_window;
static PangoLayout	* info_tip_layout;
static gpointer		info_tip_user;
static gboolean		info_tip_have_rect;
static gboolean		info_tip_waiting_reply;
static gchar *		info_tip_content_markup;
static GdkRectangle	info_tip_rect;
static GdkPoint		info_tip_pos;

/** static routines
  **********************************************/

static void
user_menu_chat_cb(GtkMenuItem * item, GtkMenuShell * menu)
{
	gpointer user = user_by_name(g_object_get_data(G_OBJECT(menu), "username"));
	if(user!=NULL)
		raise_event(EVENT_IFACE_USER_OPEN_REQ, user, 0);
}

static void
user_menu_send_cb(GtkMenuItem * item, GtkMenuShell * menu)
{
	gpointer user = user_by_name(g_object_get_data(G_OBJECT(menu), "username"));
	if(user!=NULL)
		raise_event(EVENT_IFACE_USER_MESSAGE_REQ, user, 0);
}

static void
user_menu_beep_cb(GtkMenuItem * item, GtkMenuShell * menu)
{
	gpointer user = user_by_name(g_object_get_data(G_OBJECT(menu), "username"));
	if(user != NULL)
		raise_event(EVENT_IFACE_USER_BEEP_REQ, user, 0);
}

static void
user_menu_ignore_cb(GtkMenuItem * item, GtkMenuShell * menu)
{
	prefs_list_add_unique(PREFS_NET_IGNORED_USERS, g_object_get_data(G_OBJECT(menu), "username"));
}

static void
user_menu_unignore_cb(GtkMenuItem * item, GtkMenuShell * menu)
{
	prefs_list_remove(PREFS_NET_IGNORED_USERS, g_object_get_data(G_OBJECT(menu), "username"));
}

static void
user_menu_remove_from_list_cb(GtkMenuItem * item, GtkMenuShell * menu)
{
	gpointer user = user_by_name(g_object_get_data(G_OBJECT(menu), "username"));
	if(user!=NULL)
		raise_event(EVENT_IFACE_USER_REMOVE_REQ, user, 0);
}

static void
user_menu_remove_all_unreplied_cb(GtkMenuItem * item, gpointer dummy)
{
	GList * entry, * list = user_list(FALSE);

	for(entry = list; entry; entry = entry->next)
		if(user_mode_of(entry->data)==UMODE_DEAD) {
			raise_event(EVENT_IFACE_USER_REMOVE_REQ, entry->data, 0);
		}

	g_list_free(list);
}

static void
user_menu_preferences_cb(GtkMenuItem * item, gpointer dummy)
{
	raise_event(EVENT_IFACE_SHOW_CONFIGURE_DLG, NULL, 0);
}

static void
populate_user_menu(GtkMenuShell * menu, gpointer user)
{
	GtkWidget * item_w;

	g_assert(user);

	/* bind current user name to the menu object as user might go offline
	 * until menu entry gets activated
	 */
	g_object_set_data_full(G_OBJECT(menu),
		"username", g_strdup(user_name_of(user)), (GDestroyNotify)g_free);

	/* "Open chat" */
	if(!sess_find(SESSTYPE_PRIVATE, user_name_of(user))) {
		item_w = util_image_menu_item(
			GUI_STOCK_OPEN_CHAT, _("Open chat"),
			G_CALLBACK(user_menu_chat_cb), menu);
		gtk_menu_shell_append(menu, item_w);
	}

	/* "Send message" */
	item_w = util_image_menu_item(
		GUI_STOCK_SEND_MESSAGE, _("<b>Send message</b>"),
		G_CALLBACK(user_menu_send_cb), menu);
	gtk_menu_shell_append(menu, item_w);

	/* "Beep" */
	item_w = util_image_menu_item(NULL, _("Beep"), G_CALLBACK(user_menu_beep_cb), menu);
	gtk_menu_shell_append(menu, item_w);

	/* "Ignore/Unignore" */
	if(!prefs_list_contains(PREFS_NET_IGNORED_USERS, user_name_of(user))) {
		item_w = util_image_menu_item(
			GUI_STOCK_IGNORE, _("Ignore"),
			G_CALLBACK(user_menu_ignore_cb), menu);
		gtk_menu_shell_append(menu, item_w);
	} else {
		item_w = util_image_menu_item(
			NULL, _("Unignore"),
			G_CALLBACK(user_menu_unignore_cb), menu);
		gtk_menu_shell_append(menu, item_w);
	}

	/* "Remove from list", if dead */
	if(user_mode_of(user)==UMODE_DEAD) {
		item_w = util_image_menu_item(
			GTK_STOCK_REMOVE, _("Remove unreplied"),
			G_CALLBACK(user_menu_remove_from_list_cb), menu);
		gtk_menu_shell_append(menu, item_w);
	}
}

static void
list_menu_refresh_cb(GtkMenuItem * item, gpointer dummy)
{
	raise_event(EVENT_IFACE_USER_LIST_REFRESH_REQ, NULL, 0);
}

static void
ulist_popup_list_menu(
	gboolean on_user, gpointer user,
	guint button, guint32 activate_time)
{
	GtkWidget * menu_w, * item_w;
	GList * list, * entry;
	gint num_dead_users;
	gboolean item_before = FALSE;

	menu_w = gtk_menu_new();

	if(on_user) {
		if(user!=NULL) {
			/* populate menu with entries for this user */
			populate_user_menu(GTK_MENU_SHELL(menu_w), user);
		} else {
			/* add the 'Preferences..' item for the 'myself' user */
			item_w = util_image_menu_item(
				GTK_STOCK_PREFERENCES, _("<b>Preferences..</b>"),
				G_CALLBACK(user_menu_preferences_cb), NULL);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);
		}

		item_before = TRUE;
	}

	/* "Remove all unreplied"
	 * check if there are any dead users on the list so we can remove them
	 */
	num_dead_users = 0;
	list = user_list(FALSE);
	for(entry = list; entry; entry = entry->next)
		if(user_mode_of(entry->data)==UMODE_DEAD)
			if(++num_dead_users > 1) break;	/* need to know if 0, 1 or >=2 */

	g_list_free(list);

	if(num_dead_users) {
		/* "Remove all unreplied" */
		if((user && num_dead_users==1 && user_mode_of(user)!=UMODE_DEAD)
				|| (!user && num_dead_users==1)
				|| (num_dead_users>1))
		{
			if(item_before)
				gtk_menu_shell_append(GTK_MENU_SHELL(menu_w),
					gtk_separator_menu_item_new());
			
			item_w = util_image_menu_item(
				GTK_STOCK_CLEAR, _("Remove all unreplied"),
				G_CALLBACK(user_menu_remove_all_unreplied_cb), NULL);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);

			item_before = TRUE;
		}
	}

	/* the "Refresh" item */
	if(item_before)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), gtk_separator_menu_item_new());

	item_w = util_image_menu_item(
		GTK_STOCK_REFRESH, _("Refresh list"),
		G_CALLBACK(list_menu_refresh_cb), NULL);
	gtk_widget_set_sensitive(item_w, net_connected());
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);

	/* show the menu */
	gtk_widget_show_all(menu_w);
	gtk_menu_popup(GTK_MENU(menu_w), NULL, NULL, NULL, NULL, button, activate_time);
}

void ulist_row_activated_cb(
	GtkTreeView * treeview_w,
	GtkTreePath * path,
	GtkTreeViewColumn * col,
	gpointer user_data)
{
	GtkTreeModel * model;
	GtkTreeIter iter;
	gpointer user;

	model = gtk_tree_view_get_model(treeview_w);
	if(gtk_tree_model_get_iter(model, &iter, path)) {
		gtk_tree_model_get(model, &iter, COLUMN_USER_PTR, &user, -1);
		if(user!=NULL)
			raise_event(EVENT_IFACE_USER_MESSAGE_REQ, user, 0);
		else
			raise_event(EVENT_IFACE_SHOW_CONFIGURE_DLG, NULL, 0);
	}
}

static gboolean
ulist_popup_menu_cb(
	GtkWidget * treeview_w,
	gpointer user_data)
{
	GtkTreePath * path;
	gboolean on_user = FALSE;
	gpointer user;

	/* get user for selected menu item (if any) */
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(treeview_w), &path, NULL);
	if(path) {
		GtkTreeModel * model;
		GtkTreeIter iter;

		model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview_w));
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_path_free(path);
		gtk_tree_model_get(model, &iter, COLUMN_USER_PTR, &user, -1);

		on_user = TRUE;
	}

	ulist_popup_list_menu(on_user, user, 0, 0);
	return TRUE;
}

static gboolean
ulist_button_press_cb(
	GtkWidget * treeview_w,
	GdkEventButton * event,
	gpointer user_data)
{
	GtkTreePath * path;

	if(event->type==GDK_BUTTON_PRESS && event->button==3) {
		gboolean on_user = FALSE;
		gpointer user;
		
		/* get user for the item selected (if any) */
		if(gtk_tree_view_get_path_at_pos(
				GTK_TREE_VIEW(treeview_w),
				event->x, event->y, &path, NULL, NULL, NULL))
		{
			GtkTreeSelection * selection;
			GtkTreeModel * model;
			GtkTreeIter iter;
		       
			/* select the path we clicked on */
			selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview_w));
			gtk_tree_selection_unselect_all(selection);
			gtk_tree_selection_select_path(selection, path);

			/* popup user menu */
			model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview_w));
			gtk_tree_model_get_iter(model, &iter, path);
			gtk_tree_path_free(path);

			gtk_tree_model_get(model, &iter, COLUMN_USER_PTR, &user, -1);
			on_user = TRUE;
		}

		ulist_popup_list_menu(on_user, user, event->button, event->time);
		
		return TRUE; /* we handled this */
	}
	return FALSE;
}

/* info_tip_expose_cb:
 *	paints info tip
 */
static gboolean
info_tip_expose_cb(
	GtkWidget * window_w,
	GdkEventExpose * event, gpointer dummy)
{
	GtkStyle * style;

	g_assert(info_tip_layout);

	style = info_tip_window->style;

	gtk_paint_flat_box(
		style, info_tip_window->window,
		GTK_STATE_NORMAL, GTK_SHADOW_OUT, NULL, info_tip_window,
		"tooltip", 0, 0, -1, -1);

	gtk_paint_layout(
		style, info_tip_window->window,
		GTK_STATE_NORMAL, TRUE, NULL, info_tip_window,
		"tooltip", TIP_BORDER, TIP_BORDER, info_tip_layout);

	return TRUE;
}

static gchar *
info_tip_time_format(time_t time)
{
	guint days = time / (3600 * 24),
	      hours = (time % (3600 * 24)) / 3600,
	      minutes = (time % 3600) / 60,
	      seconds = time % 60;
	gchar * str,
		* days_name = days==1 ? _("day"): _("days"),
		* hours_name = hours==1 ? _("hour"): _("hours"),
		* minutes_name = minutes==1 ? _("minute"): _("minutes"),
		* seconds_name = seconds==1 ? _("second"): _("seconds");

	if(days) {
		str = g_strdup_printf("%u %s, %u %s", days, days_name, hours, hours_name);
	}
	else if(hours) {
		str = g_strdup_printf("%u %s, %u %s", hours, hours_name, minutes, minutes_name);
	}
	else if(minutes) {
		str = g_strdup_printf("%u %s, %u %s",
			minutes, minutes_name, seconds, seconds_name);
	}
	else {
		str = g_strdup_printf("%u %s", seconds, seconds_name);
	}

	return str;
}

/* info_tip_set_content_for_me
 *	create info tip content for ourselves
 */
static void
info_tip_set_content_for_me()
{
	GString * markup = g_string_new(NULL);
	GList * channels, * chan;
	guint nchannels;
	gchar * escaped;

	/* add user name */
	escaped = g_markup_escape_text(my_nickname(), -1);
	g_string_printf(markup, "<big><b><u>%s</u></b></big>", escaped);
	g_free(escaped);

	/* show my channels */
	g_string_append_printf(markup, _("\n<b>On channels:</b> "));

	nchannels = 0;
	channels = chan = my_channels();
	while(chan && nchannels++ < TIP_MAX_CHANNELS) {
		escaped = g_markup_escape_text((const gchar*)chan->data, -1);
		g_string_append_printf(
			markup, "<span foreground=\"#800000\">#%s</span>", escaped);
		g_free(escaped);

		if(chan->next && nchannels < TIP_MAX_CHANNELS) {
			g_string_append_printf(markup, ", ");
		}

		chan = chan->next;
	}
	if(chan)
		g_string_append(markup, ", ...");
	g_list_free(channels);
	
	/* assign info tip content markup to the new one*/
	if(info_tip_content_markup)
		g_free(info_tip_content_markup);
	info_tip_content_markup = g_string_free(markup, FALSE);
}

/* info_tip_set_content_for_info_reply
 *	create info tip content markup for specified info reply
 */
static void
info_tip_set_content_for_info_reply(
	gpointer user_id, const gchar * hostname,
	GList * chan, const gchar * motd,
	guint32 src_ip)
{
	GString * markup = g_string_new(NULL);
	gchar * escaped;
	time_t last_time_active;

	/* add user name */
	escaped = g_markup_escape_text(user_name_of(user_id), -1);
	g_string_printf(markup, "<big><b><u>%s</u></b></big>", escaped);
	g_free(escaped);

	/* add hostname */
	if(g_utf8_strlen(hostname, -1)) {
		escaped = g_markup_escape_text(hostname, -1);
		g_string_append_printf(markup, _("\n<b>Hostname:</b> %s"), escaped);
		g_free(escaped);
	}

	/* add list-time-active */
	last_time_active = user_last_time_active(user_id);
	if(last_time_active) {
		gchar * formatted = info_tip_time_format(time(NULL) - last_time_active);
		escaped = g_markup_escape_text(formatted, -1);
		g_free(formatted);

		g_string_append_printf(markup, _("\n<b>Idle for:</b> %s"), escaped);
		g_free(escaped);
	}

	/* add host IP address */
	if(src_ip) {
		g_string_append_printf(markup, _("\n<b>IP address:</b> %d.%d.%d.%d"),
			(int)(src_ip >> 24) & 0xff, (int)(src_ip >> 16) & 0xff,
			(int)(src_ip >> 8) & 0xff, (int)src_ip & 0xff);
	}

	if(chan) {
		gint nchannels = 0;
		g_string_append_printf(markup, _("\n<b>On channels:</b> "));

		while(chan && nchannels++ < TIP_MAX_CHANNELS) {
			escaped = g_markup_escape_text((const gchar*)chan->data, -1);
			g_string_append_printf(
				markup, "<span foreground=\"#800000\">#%s</span>", escaped);
			g_free(escaped);

			if(chan->next && nchannels < TIP_MAX_CHANNELS) {
				g_string_append_printf(markup, ", ");
			}

			chan = chan->next;
		}
		if(chan) {
			g_string_append(markup, ", ...");
		}
	}
	if(g_utf8_strlen(motd, -1)) {
		escaped = g_markup_escape_text(motd, -1);
		g_string_append_printf(markup,
			"\n<span foreground=\"#000060\"><i>%s</i></span>", escaped);
		g_free(escaped);
	}

	/* assign info tip content markup to the new one*/
	if(info_tip_content_markup)
		g_free(info_tip_content_markup);
	info_tip_content_markup = g_string_free(markup, FALSE);
}

/* info_tip_set_content_for_unsuccessfull_request
 *	create info tip content markup for info unsuccessfull request
 */
static void
info_tip_set_content_for_unsuccessfull_request(gpointer user_id)
{
	GString * markup = g_string_new(NULL);
	gchar * escaped;

	/* add user name */
	escaped = g_markup_escape_text(user_name_of(user_id), -1);
	g_string_printf(markup, "<big><b><u>%s</u></b></big>", escaped);
	g_free(escaped);

	g_string_append(markup,
		_("\n<span foreground=\"#902020\">"
			"<i>User is not responding.</i>"
		"</span>"));

	/* assign info tip content markup to the new one*/
	if(info_tip_content_markup)
		g_free(info_tip_content_markup);
	info_tip_content_markup = g_string_free(markup, FALSE);
}

/* info_tip_show:
 *	creates and shows info tip about the user focused
 */
static void
info_tip_show()
{
	gint x, y, width, height, scr_width;

	g_assert(info_tip_window==NULL);
	g_assert(info_tip_content_markup);

	/* create the tip window */
	info_tip_window = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_widget_set_app_paintable(info_tip_window, TRUE);
	gtk_window_set_resizable(GTK_WINDOW(info_tip_window), FALSE);
	gtk_widget_set_name(info_tip_window, "gtk-tooltips");
	gtk_widget_ensure_style(info_tip_window);
	g_signal_connect(
		G_OBJECT(info_tip_window), "expose-event",
		G_CALLBACK(info_tip_expose_cb), NULL);

	/* build content from markup */
	info_tip_layout = gtk_widget_create_pango_layout(info_tip_window, NULL);
	pango_layout_set_wrap(info_tip_layout, PANGO_WRAP_WORD);
	pango_layout_set_width(info_tip_layout, TIP_MAX_WIDTH*1000);
	pango_layout_set_markup(info_tip_layout, info_tip_content_markup, -1);

	/* set window size */
	pango_layout_get_size(info_tip_layout, &width, &height);
	width = PANGO_PIXELS(width) + TIP_BORDER*2;
	height = PANGO_PIXELS(height) + TIP_BORDER*2;
	gtk_widget_set_size_request(info_tip_window, width, height);

	/* set window position so it wont go past screen boundaries
	 */
	gdk_window_get_pointer(NULL, &x, &y, NULL);

	y += TIP_OFFSET_Y;

	scr_width = gdk_screen_width();
	if(x + width > scr_width) {
		x = scr_width - width;
	} else {
		x += TIP_OFFSET_X;
	}
	gtk_window_move(GTK_WINDOW(info_tip_window), x, y);

	/* present tip to the user
	 */
	gtk_widget_show(info_tip_window);
}

/* info_tip_destroy:
 *	deletes the timeout source/dialog/info request
 *	for active user info request tip
 */
static void
info_tip_destroy()
{
	/* destroy tooltip window */
	if(info_tip_window) {
		gtk_widget_destroy(info_tip_window);
		info_tip_window = NULL;

		g_object_unref(G_OBJECT(info_tip_layout));
		info_tip_layout = NULL;
	}

	/* delete info_tip_timeout, if active */
	if(info_tip_timeout) {
		g_source_remove(info_tip_timeout);
		info_tip_timeout = 0;
	}

	/* delete content, if any */
	if(info_tip_content_markup) {
		g_free(info_tip_content_markup);
		info_tip_content_markup = NULL;
	}

	info_tip_waiting_reply = FALSE;
	info_tip_have_rect = FALSE;
}


/* info_tip_net_info_reply:
 *	handles net message from network
 */
static void
info_tip_net_info_reply(
	gpointer user_id, const gchar * login, GList * channels,
	const gchar * motd, guint32 src_ip)
{
	if(info_tip_waiting_reply && user_id==info_tip_user) {
		/* show info tip about this user  */
		info_tip_set_content_for_info_reply(user_id, login, channels, motd, src_ip);

		/* we need no more info replies */
		info_tip_waiting_reply = FALSE;
	}
}


/* info_tip_show_timeout_cb:
 *	invoked by info_tip_timeout to show a tip
 */
static gboolean
info_tip_show_timeout_cb(gpointer _unused)
{
	if(!info_tip_content_markup)
		info_tip_set_content_for_unsuccessfull_request(info_tip_user);

	info_tip_show();

	g_free(info_tip_content_markup);
	info_tip_content_markup = NULL;

	return FALSE;	/* remove this timeout source */
}

/* ulist_motion_notify_cb:
 *	invoked when the user moves mouse over user list;
 *	pops up and hides user info tooltip
 */
static void
ulist_motion_notify_cb(
	GtkWidget * treeview_w,
	GdkEventMotion * event, gpointer user_data)
{
	gboolean cell_found;
	GtkTreeIter cell_iter;
	GtkTreePath * cell_path;

	if(info_tip_have_rect) {
		/* have a user (with the cell rectangle)
		 */
		if(event->y < info_tip_rect.y
				|| event->y > info_tip_rect.y + info_tip_rect.height) {
			/* the pointer went outside of the rectangle of the selected user
			 */
			info_tip_destroy();
		} else {
			/* the pointer was kept inside rectangle
			 */
			if(!info_tip_window) {
				/* .. and we don't have a tip shown yet */
				info_tip_pos.x = event->x;
				info_tip_pos.y = event->y;
			}
		}
	} else {
		/* no user rectangle is focused:
		 * find the user we've focused
		 */
		cell_found = gtk_tree_view_get_path_at_pos(
			GTK_TREE_VIEW(treeview_w), event->x, event->y,
			&cell_path, NULL, NULL, NULL);

		if(cell_found) {
			/* get user of the focused cell */
			gtk_tree_model_get_iter(GTK_TREE_MODEL(ulist_model), &cell_iter, cell_path);
			gtk_tree_model_get(
				GTK_TREE_MODEL(ulist_model), &cell_iter,
				COLUMN_USER_PTR, &info_tip_user, -1);

			/* save rectangle of the focused cell */
			gtk_tree_view_get_cell_area(
				GTK_TREE_VIEW(treeview_w), cell_path, NULL, &info_tip_rect);
			gtk_tree_path_free(cell_path);
			info_tip_have_rect = TRUE;

			if(info_tip_user!=NULL) {
				/* send user info request */
				info_tip_waiting_reply = TRUE;
				raise_event(EVENT_IFACE_USER_INFO_REQ, info_tip_user, 0);
			} else {
				/* that was us - do not request any info */
				info_tip_set_content_for_me();
			}
		
			/* activate timeout to show the tip */
			info_tip_timeout = g_timeout_add(
				prefs_int(PREFS_MAIN_POPUP_TIMEOUT),
				info_tip_show_timeout_cb, NULL);
		}
	}
}

static void
ulist_leave_notify_cb(
	GtkTreeView * treeview_w, GdkEventCrossing * event,
       	gpointer user_data)
{
	info_tip_destroy();
}

/* where_to_insert:
 *	returns valid iterator to empty list entry
 *	where new user/icon should be put into
 */
static void 
ulist_where_to_insert(
	const char * user_name,
	enum user_mode_enum mode,
	GtkTreeIter * where)
{
	gboolean valid;
	GValue v = {0, };
	GtkTreeIter i;

	g_assert(user_name && where);

	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ulist_model), &i);
	while(valid) {
		gtk_tree_model_get_value(GTK_TREE_MODEL(ulist_model), &i, COLUMN_NICKNAME, &v);

		if(util_utf8_strcasecmp(g_value_get_string(&v), user_name) > 0 ) {
			g_value_unset(&v);
			gtk_list_store_insert_before(ulist_model, where, &i);
			return;
		}

		g_value_unset(&v);

		/* skip to next one */
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(ulist_model), &i);
	}

	/* insert at the end */
	gtk_list_store_append(ulist_model, where);
}

static void
ulist_add(gpointer user)
{
	GtkTreeIter iter;

	ulist_where_to_insert(ULIST_NAME(user), ULIST_MODE(user), &iter);
	gtk_list_store_set(
		ulist_model, &iter,
		COLUMN_ICON, util_user_state_stock(ULIST_MODE(user), ULIST_ACTIVE(user)),
		COLUMN_NICKNAME, ULIST_NAME(user),
		COLUMN_USER_PTR, user,
		-1);
}

static gboolean
ulist_iter_by_user(gpointer user, GtkTreeIter * iter)
{
	gpointer iter_user;

	g_assert(iter);
	
	if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ulist_model), iter)) {
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(ulist_model), iter,
					COLUMN_USER_PTR, &iter_user, -1);
			if(iter_user==user)
				return TRUE;
		} while(gtk_tree_model_iter_next(GTK_TREE_MODEL(ulist_model), iter));
	}
	return FALSE;
}

static void
ulist_remove(gpointer user)
{
	GtkTreeIter iter;

	if(ulist_iter_by_user(user, &iter))
		gtk_list_store_remove(ulist_model, &iter);
}

static void
ulist_update_icon_for(gpointer user)
{
	GtkTreeIter iter;

	if(ulist_iter_by_user(user, &iter)) {
		gtk_list_store_set(
			ulist_model, &iter,
			COLUMN_ICON, util_user_state_stock(ULIST_MODE(user), ULIST_ACTIVE(user)),
		       	-1);
	}
}

static void
ulist_prefs_main_nickname_changed_cb(const gchar * prefs_name)
{
	if(net_connected()) {
		ulist_remove(NULL);
		ulist_add(NULL);
	}
}

static void
ulist_prefs_main_mode_changed_cb(const gchar * prefs_name)
{
	ulist_update_icon_for(NULL);
}

static void
ulist_event_cb(enum app_event_enum e, gpointer p, gint i)
{
	switch(e) {
	case EVENT_MAIN_REGISTER_PREFS:
		prefs_add_notifier(PREFS_MAIN_NICKNAME,
				(GHookFunc)ulist_prefs_main_nickname_changed_cb);
		prefs_add_notifier(PREFS_MAIN_MODE,
				(GHookFunc)ulist_prefs_main_mode_changed_cb);
		break;
	case EVENT_USER_NEW:
		ulist_add(EVENT_V(p, 0));
		break;
	case EVENT_USER_REMOVED:
		ulist_remove(p);
		break;

	case EVENT_USER_ACTIVE_CHANGE:
	case EVENT_USER_MODE_CHANGE:
		ulist_update_icon_for(p);
		break;

	case EVENT_USER_RENAME:
		ulist_remove(EVENT_V(p, 0));
		ulist_add(EVENT_V(p, 0));
		break;

	case EVENT_IFACE_ACTIVE_CHANGE:
		ulist_update_icon_for(NULL);
		break;

	case EVENT_NET_CONNECTED:
		/* add myself to the list */
		ulist_add(NULL);
		break;

	case EVENT_NET_DISCONNECTED:
		/* remove myself from the list */
		ulist_remove(NULL);
		break;

	case EVENT_NET_MSG_INFO_REPLY:
		info_tip_net_info_reply(
			EVENT_V(p, 0), EVENT_V(p, 1),
			EVENT_V(p, 2), EVENT_V(p, 3),
			GPOINTER_TO_INT(EVENT_V(p, 4)));
		break;
	default:
		break;
	}
}

/* exported routines
 ******************************************/

/* gui_ulist_create:
 *	creates & setups new userlist widget
 *	and static variables
 */
GtkWidget *
gui_ulist_create()
{
	GtkWidget * treeview_w, * scrolled_w;
	GtkCellRenderer * icon_renderer;
	
	/* initialize misc variables
	 */
	info_tip_timeout = 0;
	info_tip_have_rect = TRUE;
	info_tip_waiting_reply = FALSE;
	info_tip_content_markup = NULL;
	info_tip_window = NULL;

	/* create list store */
	ulist_model = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	
	/* create scrolled window & tree view widget */
	scrolled_w = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW(scrolled_w),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	treeview_w = gtk_tree_view_new();
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_w), FALSE);
	gtk_widget_show(treeview_w);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_w), treeview_w);

	/* link model with view */
	gtk_tree_view_set_model(GTK_TREE_VIEW(treeview_w), GTK_TREE_MODEL(ulist_model));

	/* setup view columns */
	icon_renderer = gtk_cell_renderer_pixbuf_new();
	g_object_set(G_OBJECT(icon_renderer), "stock-size", GTK_ICON_SIZE_MENU, NULL);
	gtk_tree_view_insert_column_with_attributes(
		GTK_TREE_VIEW(treeview_w), -1, NULL,
		icon_renderer, "stock-id", COLUMN_ICON, NULL);

	gtk_tree_view_insert_column_with_attributes(
		GTK_TREE_VIEW(treeview_w), -1, NULL,
		gtk_cell_renderer_text_new(), "text", COLUMN_NICKNAME, NULL);

	/* connect signals */
	g_signal_connect(G_OBJECT(treeview_w),
			"row-activated", G_CALLBACK(ulist_row_activated_cb), NULL);
	g_signal_connect(G_OBJECT(treeview_w),
			"button-press-event", G_CALLBACK(ulist_button_press_cb), NULL);
	g_signal_connect(G_OBJECT(treeview_w),
			"motion-notify-event", G_CALLBACK(ulist_motion_notify_cb), NULL);
	g_signal_connect(G_OBJECT(treeview_w),
			"leave-notify-event", G_CALLBACK(ulist_leave_notify_cb), NULL);
	g_signal_connect(G_OBJECT(treeview_w),
			"popup-menu", G_CALLBACK(ulist_popup_menu_cb), NULL);

	return scrolled_w;
}

/* gui_ulist_free:
 *	frees any static data unreferenced in GtkWidget
 */
void gui_ulist_free()
{
}

/* gui_ulist_register:
 *	registers this module to handle external events
 */
void gui_ulist_register()
{
	register_event_cb(
		ulist_event_cb,
		EVENT_MAIN | EVENT_USER | EVENT_NET | EVENT_IFACE);
}

