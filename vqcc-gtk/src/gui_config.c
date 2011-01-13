/*
 * gui_config.c: implements configuration dialog
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
 * $Id: gui_config.c,v 1.43 2005/01/06 10:16:08 bobas Exp $
 */

#include <stdio.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkkeysyms.h>

#include "main.h"
#include "gui.h"
#include "prefs.h"
#include "net.h"
#include "idle.h"
#include "gui_config.h"
#include "gui_misc.h"
#include "gui_msg.h"
#include "gui_tray.h"
#include "gui_page.h"
#include "util.h"

#define HAVE_ENCODING_COMBO_WIDGET \
	((GTK_MAJOR_VERSION > 2) || ((GTK_MAJOR_VERSION==2) && (GTK_MINOR_VERSION>=4)))

/** geometries
  ************************************/
#define GEOM_DLG_PAGE_BORDER	6

/** structure definitions
  ************************************/
struct config_dlg {
	gboolean is_changed, gathering;

	GtkTreeStore * prefs_tree_store;
	GtkWidget * dlg_w, * prefs_tree_w, * notebook_w,

		/* Network widgets */
		* net_type_w,
		* net_port_w,
		* net_broadcast_label_w, * net_broadcast_w,
		* net_use_multicast_w,
		* net_multicast_label_w, * net_multicast_w,

		* net_reply_w,
		* net_log_w,
		* net_channel_greet_w,
		* net_charset_w,
#if(HAVE_ENCODING_COMBO_WIDGET)
		* net_charset_combo_w,
#endif

		/* User list */
		* userlist_refresh_w,
		* userlist_keep_unreplied_users_w,

		/* Idle frame widgets */
		* idle_enable_w,
		* auto_away_w,
		* auto_offline_w,

		/* Pages */
		* pages_show_topic_entry,
		* pages_present_on_private,
		* pages_log_current_w,
		* date_channel_w,
		* date_private_w,
		* date_status_w,
		* pages_notebook_pos_w,
		* pages_suppress_join_leave_w,
		* pages_suppress_rename_w,
		* pages_suppress_mode_change_w,

		/* Messages */
		* msg_max_w,
		* msg_max_label_w,
		* msg_show_window_w,
		* msg_ignore_mass_w,
		* msg_ctrl_return_w,
		* msg_show_messages_in_offline_dnd_w,

		/* Tray icon */
		* tray_enable_w,
		* tray_hide_on_startup_w,
		* tray_trigger_joinleave_w,
		* tray_trigger_channel_w,
		* tray_trigger_private_w,
		* tray_trigger_status_w,
		* tray_trigger_topic_w,
		* tray_tooltip_lines_w,
		* tray_tooltip_lines_label_w,

		/* Misc */
		* save_on_exit_w,
		* keep_window_size_w,
		* motd_w,
		* motd_away_w,

		/* action_area buttons */
		* save_w,
		* apply_w;
};
#define PCONFIG_DLG(p) ((struct config_dlg*)p)

enum config_tree_model_enum {
	CONFIG_TREE_NAME,
	CONFIG_TREE_INDEX,
	CONFIG_TREE_NUM
};

/** static variables
 */
static struct config_dlg * the_dlg; 

#if(HAVE_ENCODING_COMBO_WIDGET)
const gchar *
supported_encodings[] = {
	"KOI8-R", "CP1251",	/* cyrillic */
	"ISO-8859-2",		/* central europe */
	"ISO-8859-13",		/* baltic */
	"ISO-8859-7",		/* greek */
	"ISO-8859-8",		/* hebrew */
	"ISO-8859-9",		/* turkish */
	"ISO-8859-15",		/* western europe */
	"ISO-2022-JP", "SJIS",	/* japanese */
	"CP1256",		/* arabic */
	"GB18030",		/* chinese */
	"UTF-8",
	NULL
};
#endif

/** static rountines
 */
static void set_config_dlg_modified(struct config_dlg *, gboolean);
static void sync_dlg_from_config(struct config_dlg *);
static void set_config_from_dlg(struct config_dlg *);
static void destroy_config_dlg(struct config_dlg **);

static void
set_ipv4_entry(GtkWidget * entry_w, guint32 addr)
{
	char * str = util_inet_ntoa(addr);
	gtk_entry_set_text(GTK_ENTRY(entry_w), str);
	g_free(str);
}

static void
prefs_dlg_check_button_toggled_cb(GtkToggleButton * togglebutton_w, gpointer dlg)
{
	/* update dialog state */
	set_config_dlg_modified(PCONFIG_DLG(dlg), TRUE);
}

static GtkWidget *
add_check_button(
	struct config_dlg * dlg,
	GtkWidget * b, const gchar * prefs_name,
	GCallback toggled_cb, gpointer toggled_cb_user_data)
{
	GtkWidget * w;

	w = gtk_check_button_new_with_mnemonic(prefs_description(prefs_name));
	gtk_box_pack_start(GTK_BOX(b), w, FALSE, FALSE, 0);

	g_signal_connect(
		G_OBJECT(w), "toggled",
		G_CALLBACK(prefs_dlg_check_button_toggled_cb), (gpointer)dlg);

	if(toggled_cb)
		g_signal_connect(G_OBJECT(w), "toggled", toggled_cb, toggled_cb_user_data);

	return w;
}

static void
prefs_dlg_spin_button_changed_cb(GtkSpinButton * spinbutton_w, gpointer dlg)
{
	/* update dialog state */
	set_config_dlg_modified(PCONFIG_DLG(dlg), TRUE);
}

static GtkWidget *
add_spin_button(
	struct config_dlg * dlg,
	GtkWidget * b,
	const gchar * prefs_name,
	int min, int max,
	GtkWidget ** p_label_w,
	GCallback value_changed_cb,
	gpointer value_changed_cb_data)
{
	GtkWidget * w, * hb, * label_w;

	g_assert(dlg && b && min<max);

	hb = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(b), hb, FALSE, FALSE, 0);

	label_w = gtk_label_new_with_mnemonic(prefs_description(prefs_name));
	gtk_box_pack_start(GTK_BOX(hb), label_w, FALSE, FALSE, 0);
	if(p_label_w)
		*p_label_w = label_w;

	w = gtk_spin_button_new_with_range(min, max, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label_w), w);
	gtk_box_pack_start(GTK_BOX(hb), w, FALSE, FALSE, 0);

	g_signal_connect(
		G_OBJECT(w), "value-changed",
		G_CALLBACK(prefs_dlg_spin_button_changed_cb), (gpointer)dlg);

	if(value_changed_cb)
		g_signal_connect(
			G_OBJECT(w), "value-changed",
			G_CALLBACK(value_changed_cb), value_changed_cb_data);

	return w;
}

static void
prefs_dlg_option_changed_cb(GtkOptionMenu * o, struct config_dlg * dlg)
{
	set_config_dlg_modified(dlg, TRUE);
}

static GtkWidget *
add_option(
	struct config_dlg * dlg,
	GtkWidget * b,
	const gchar * prefs_name,
	GList * entries,
	GtkWidget ** p_label_w)
{
	GtkWidget * o, * hb, * menu_w, * label_w;

	g_assert(b && entries);

	hb = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(b), hb, FALSE, FALSE, 0);

	label_w = gtk_label_new_with_mnemonic(prefs_description(prefs_name));
	gtk_box_pack_start(GTK_BOX(hb), label_w, FALSE, FALSE, 0);
	if(p_label_w) *p_label_w = label_w;

	/* make option menu */
	o = gtk_option_menu_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label_w), o);
	menu_w = gtk_menu_new();
	for(; entries; entries = entries->next) {
		gtk_menu_shell_append(
			GTK_MENU_SHELL(menu_w),
			gtk_menu_item_new_with_mnemonic((gchar*)entries->data));
	}
	gtk_option_menu_set_menu(GTK_OPTION_MENU(o), menu_w);
	gtk_box_pack_start(GTK_BOX(hb), o, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(o), "changed", G_CALLBACK(prefs_dlg_option_changed_cb), dlg);

	return o;
}

static void
prefs_dlg_entry_changed_cb(GtkEntry * w, struct config_dlg * dlg)
{
	set_config_dlg_modified(dlg, TRUE);
}

static GtkWidget *
add_entry(
	struct config_dlg * dlg,
	GtkWidget * b,
	const gchar * prefs_name,
	GtkWidget ** p_label_w)
{
	GtkWidget * hb, * w, * label_w;

	hb = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(b), hb, FALSE, FALSE, 0);

	/* make label */
	label_w = gtk_label_new_with_mnemonic(prefs_description(prefs_name));
	gtk_box_pack_start(GTK_BOX(hb), label_w, FALSE, FALSE, 0);
	if(p_label_w) *p_label_w = label_w;

	/* make edit entry */
	w = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label_w), w);
	gtk_box_pack_start(GTK_BOX(hb), w, TRUE, TRUE, 0);
	g_signal_connect(
		G_OBJECT(w), "changed",
		G_CALLBACK(prefs_dlg_entry_changed_cb), (gpointer)dlg);

	return w;
}

static GtkWidget *
add_widget(
	struct config_dlg * dlg,
	GtkWidget * frame_vbox,
	const gchar * label_text, GtkWidget * widget)
{
	GtkWidget * hbox = gtk_hbox_new(FALSE, 2);

	if(label_text) {
		gtk_box_pack_start(
			GTK_BOX(hbox), gtk_label_new_with_mnemonic(label_text),
			FALSE, FALSE, 0);
	}
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(frame_vbox), hbox, FALSE, FALSE, 0);

	return widget;
}

/* gtk 2.4+ has support for encoding combo widget
 */
#if(HAVE_ENCODING_COMBO_WIDGET)

static GtkWidget *
net_frame_encoding_combo(
	struct config_dlg * dlg,
	const gchar * label_text)
{
	GtkWidget * combo_w;
	const gchar ** encoding;
	gboolean have_current_encoding;

	combo_w = gtk_combo_box_entry_new_text();

	/* add encodings to the combo box list */
	have_current_encoding = FALSE;
	for(encoding = supported_encodings; *encoding; encoding++) {
		/* check encoding is supported on this system/glib */
		gchar *enc_test = g_convert("a", 1, "UTF-8", *encoding, NULL,NULL,NULL);
		if(!enc_test)
			continue;
		g_free(enc_test);

		gtk_combo_box_append_text(GTK_COMBO_BOX(combo_w), *encoding);

		/* check if this is the current encoding */
		if(!g_utf8_collate(*encoding, prefs_str(PREFS_NET_ENCODING)))
			have_current_encoding = TRUE;
	}
	if(!have_current_encoding) {
		/* add the default encoding as it was not found in the list */
		gtk_combo_box_prepend_text(GTK_COMBO_BOX(combo_w), prefs_str(PREFS_NET_ENCODING));
	}

	return combo_w;
}
#endif

static void
net_detect_btn_clicked(GtkButton * button_w, struct config_dlg * dlg)
{
	raise_event(EVENT_IFACE_SHOW_NETDETECT_DLG, NULL, 0);
}

static void
net_prefs_type_changed_cb(gint mode, gpointer userdata)
{
	struct config_dlg * dlg = (struct config_dlg *)userdata;

	set_config_dlg_modified(dlg, TRUE);

	if(mode==NET_TYPE_QCHAT) {
		/* there's no option for multicast in quickchat mode */
		gtk_widget_set_sensitive(dlg->net_use_multicast_w, FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dlg->net_use_multicast_w), FALSE);
	} else {
		gtk_widget_set_sensitive(dlg->net_use_multicast_w, TRUE);
	}
}

static void
net_prefs_use_multicast_changed_cb(GtkToggleButton * button_w, struct config_dlg * dlg)
{
	gboolean set = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_w));
	
	gtk_widget_set_sensitive(dlg->net_broadcast_label_w, !set);
	gtk_widget_set_sensitive(dlg->net_broadcast_w, !set);

	gtk_widget_set_sensitive(dlg->net_multicast_label_w, set);
	gtk_widget_set_sensitive(dlg->net_multicast_w, set);
}

static GtkWidget *
make_net_frame(struct config_dlg * dlg)
{
	GtkWidget * b, * detect_btn;

	b = gtk_vbox_new(FALSE, 2);
	gtk_container_set_border_width(GTK_CONTAINER(b), GEOM_DLG_PAGE_BORDER);

	/* network type option menu */
	dlg->net_type_w = add_widget(
		dlg, b, prefs_description(PREFS_NET_TYPE),
		util_net_type_option(net_prefs_type_changed_cb, (gpointer)dlg));

	/* network port numer */
	dlg->net_port_w = add_spin_button(dlg, b, PREFS_NET_PORT, 0, 65535, NULL, NULL, NULL);

	/* brodcast & multicast addresses */
	dlg->net_use_multicast_w = add_check_button(
		dlg, b, PREFS_NET_USE_MULTICAST,
		G_CALLBACK(net_prefs_use_multicast_changed_cb), (gpointer)dlg);
	dlg->net_broadcast_w = add_entry(dlg, b, PREFS_NET_BROADCAST_MASK, &dlg->net_broadcast_label_w);
	dlg->net_multicast_w = add_entry(dlg, b, PREFS_NET_MULTICAST_ADDR, &dlg->net_multicast_label_w);
	net_prefs_use_multicast_changed_cb(GTK_TOGGLE_BUTTON(dlg->net_use_multicast_w), dlg);
	net_prefs_type_changed_cb(
		gtk_option_menu_get_history(GTK_OPTION_MENU(dlg->net_type_w)), dlg);

	/* net charset selection widget */
#if(HAVE_ENCODING_COMBO_WIDGET)
	dlg->net_charset_combo_w = add_widget(
		dlg, b, prefs_description(PREFS_NET_ENCODING),
		net_frame_encoding_combo(dlg, prefs_description(PREFS_NET_ENCODING)));

	dlg->net_charset_w = GTK_BIN(dlg->net_charset_combo_w)->child;
	g_signal_connect(G_OBJECT(dlg->net_charset_w),
		"changed", G_CALLBACK(prefs_dlg_entry_changed_cb), (gpointer) dlg);
#else
	dlg->net_charset_w = add_entry(dlg, b, PREFS_NET_ENCODING, NULL);
#endif

	/* automatic detection button */
	detect_btn = gtk_button_new_with_mnemonic(_("Detect available networks.."));
	g_signal_connect(G_OBJECT(detect_btn), "clicked",
		G_CALLBACK(net_detect_btn_clicked), (gpointer)dlg);
	add_widget(dlg, b, NULL, detect_btn);

	/* separator for miscelanious settings */
	gtk_box_pack_start(GTK_BOX(b), gtk_hseparator_new(), FALSE, FALSE, 8);

	/* whether to notify buddy/channel of our joins/leaves */
	dlg->net_channel_greet_w = add_check_button(dlg, b, PREFS_NET_CHANNEL_NOTIFY, NULL, NULL);

	/* whether to reply to info requests */
	dlg->net_reply_w = add_check_button(dlg, b, PREFS_NET_REPLY_INFO_REQ, NULL, NULL);

	/* whether to log network events verbosely */
	dlg->net_log_w = add_check_button(dlg, b, PREFS_NET_VERBOSE, NULL, NULL);

	gtk_widget_show_all(b);
	return b;
}

static GtkWidget *
make_user_list_frame(struct config_dlg * dlg)
{
	GtkWidget * b;

	b = gtk_vbox_new(FALSE, 2);
	gtk_container_set_border_width(GTK_CONTAINER(b), GEOM_DLG_PAGE_BORDER);

	/* update interval selection */
	dlg->userlist_refresh_w
		= add_spin_button(dlg, b, PREFS_USER_LIST_REFRESH, 1, 1000, NULL, NULL, NULL);

	/* whether to remove dead users from the list */
	dlg->userlist_keep_unreplied_users_w
		= add_check_button(dlg, b, PREFS_USER_KEEP_UNREPLIED, NULL, NULL);

	gtk_widget_show_all(b);
	return b;
}

static void
idle_prefs_idle_auto_away_changed_cb(GtkSpinButton * spinbutton_w, gpointer dlg)
{
	gint away, offl;

	offl = (gint)gtk_spin_button_get_value(GTK_SPIN_BUTTON(PCONFIG_DLG(dlg)->auto_offline_w));
	away = (gint)gtk_spin_button_get_value(GTK_SPIN_BUTTON(PCONFIG_DLG(dlg)->auto_away_w));

	/* make sure away timeout is not larger than that of offline */
	if(away > offl)
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(PCONFIG_DLG(dlg)->auto_offline_w), away);
}

static void
idle_prefs_idle_auto_offline_changed_cb(GtkSpinButton * spinbutton_w, gpointer dlg)
{
	gint away, offl;

	offl = (gint)gtk_spin_button_get_value(GTK_SPIN_BUTTON(PCONFIG_DLG(dlg)->auto_offline_w));
	away = (gint)gtk_spin_button_get_value(GTK_SPIN_BUTTON(PCONFIG_DLG(dlg)->auto_away_w));

	/* make sure offline timeout is larger away's */
	if(offl < away)
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(PCONFIG_DLG(dlg)->auto_away_w), offl);
}

static void
idle_prefs_idle_enabled_changed_cb(GtkToggleButton * togglebutton_w, gpointer dlg)
{
	gboolean set = gtk_toggle_button_get_active(togglebutton_w);

	gtk_widget_set_sensitive(PCONFIG_DLG(dlg)->auto_away_w, set);
	gtk_widget_set_sensitive(PCONFIG_DLG(dlg)->auto_offline_w, set);
}

static GtkWidget *
make_idle_frame(struct config_dlg * dlg)
{
	GtkWidget * box;

	box = gtk_vbox_new(FALSE, 2);
	gtk_container_set_border_width(GTK_CONTAINER(box), GEOM_DLG_PAGE_BORDER);

	/* insert enabler/disabler */
	dlg->idle_enable_w = add_check_button(
		dlg, box, PREFS_IDLE_ENABLE,
		G_CALLBACK(idle_prefs_idle_enabled_changed_cb), (gpointer)dlg);

	/* insert auto-away & auto-offline */
	dlg->auto_away_w = add_spin_button(
		dlg, box, PREFS_IDLE_AUTO_AWAY, 1, 240, NULL,
		G_CALLBACK(idle_prefs_idle_auto_away_changed_cb), (gpointer)dlg);
	dlg->auto_offline_w = add_spin_button(
		dlg, box, PREFS_IDLE_AUTO_OFFLINE, 1, 240, NULL,
		G_CALLBACK(idle_prefs_idle_auto_offline_changed_cb), (gpointer)dlg);

	gtk_widget_show_all(box);
	return box;
}

static void
messages_pref_in_window_toggled_cb(GtkToggleButton * button_w, gpointer dlg)
{
	gboolean set = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_w));

	gtk_widget_set_sensitive(PCONFIG_DLG(dlg)->msg_max_label_w, set);
	gtk_widget_set_sensitive(PCONFIG_DLG(dlg)->msg_max_w, set);
}

static GtkWidget *
make_messages_frame(struct config_dlg * dlg)
{
	GtkWidget * b;

	b = gtk_vbox_new(FALSE, 2);
	gtk_container_set_border_width(GTK_CONTAINER(b), GEOM_DLG_PAGE_BORDER);

	dlg->msg_ignore_mass_w = add_check_button(dlg, b, PREFS_NET_IGNORE_MASS_MSG, NULL, NULL);
	dlg->msg_ctrl_return_w = add_check_button(dlg, b, PREFS_GUI_MSG_CTRL_ENTER, NULL, NULL);
	dlg->msg_show_messages_in_offline_dnd_w =
		add_check_button(dlg, b, PREFS_GUI_MSG_SHOW_IN_OFFLINE_DND, NULL, NULL);

	gtk_box_pack_start(GTK_BOX(b), gtk_hseparator_new(), FALSE, FALSE, 6);

	dlg->msg_show_window_w = add_check_button(
		dlg, b, PREFS_GUI_MSG_IN_WINDOW,
		G_CALLBACK(messages_pref_in_window_toggled_cb), (gpointer)dlg);

	dlg->msg_max_w = add_spin_button(
		dlg, b, PREFS_GUI_MSG_MAX_WINDOWS,
		1, 64, &dlg->msg_max_label_w, NULL, NULL);

	gtk_widget_show_all(b);
	return b;
}

static GtkWidget *
make_pages_frame(struct config_dlg * dlg)
{
	GList * pos_list;
	GtkWidget * b;

	b = gtk_vbox_new(FALSE, 2);
	gtk_container_set_border_width(GTK_CONTAINER(b), GEOM_DLG_PAGE_BORDER);

	dlg->pages_show_topic_entry = add_check_button(dlg, b, PREFS_GUI_TOPIC_BAR, NULL, NULL);

	/* whether to present (show on top) main window when someone
	 * opens private chat with us */
	dlg->pages_present_on_private = add_check_button(
		dlg, b, PREFS_GUI_PRESENT_ON_PRIVATE, NULL, NULL);

	/* whether to duplicate new "Status" tab messages
	 * in active sessions */
	dlg->pages_log_current_w = add_check_button(dlg, b, PREFS_MAIN_LOG_GLOBAL, NULL, NULL);

	/* add page notebook tab position option
	 * (the order should be the same as in `enum GtkPositionType')
	 */
	pos_list = g_list_append(NULL, _("Left"));
	pos_list = g_list_append(pos_list, _("Right"));
	pos_list = g_list_append(pos_list, _("Top"));
	pos_list = g_list_append(pos_list, _("Bottom"));
	dlg->pages_notebook_pos_w = add_option(dlg, b, PREFS_GUI_PAGE_TAB_POS, pos_list, NULL);
	g_list_free(pos_list);

	/* message suppress settings */
	gtk_box_pack_start(GTK_BOX(b), gtk_hseparator_new(), FALSE, FALSE, 6);
	dlg->pages_suppress_join_leave_w = add_check_button(
		dlg, b, PREFS_SESS_SUPPRESS_JOIN_LEAVE_TEXT, NULL, NULL);
	dlg->pages_suppress_rename_w = add_check_button(
		dlg, b, PREFS_SESS_SUPPRESS_RENAME_TEXT, NULL, NULL);
	dlg->pages_suppress_mode_change_w = add_check_button(
		dlg, b, PREFS_SESS_SUPPRESS_MODE_TEXT, NULL, NULL);

	/* time stamp settings */
	gtk_box_pack_start(GTK_BOX(b), gtk_hseparator_new(), FALSE, FALSE, 6);

	dlg->date_channel_w = add_check_button(dlg, b, PREFS_SESS_STAMP_CHANNEL, NULL, NULL);
	dlg->date_private_w = add_check_button(dlg, b, PREFS_SESS_STAMP_PRIVATE, NULL, NULL);
	dlg->date_status_w = add_check_button(dlg, b, PREFS_SESS_STAMP_STATUS, NULL, NULL);

	gtk_widget_show_all(b);
	return b;
}

static void
systray_pref_gui_tray_enable_toggled_cb(GtkToggleButton * button_w, gpointer dlg)
{
	gboolean set = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_w));

	gtk_widget_set_sensitive(PCONFIG_DLG(dlg)->tray_hide_on_startup_w, set);
	gtk_widget_set_sensitive(PCONFIG_DLG(dlg)->tray_trigger_joinleave_w, set);
	gtk_widget_set_sensitive(PCONFIG_DLG(dlg)->tray_trigger_topic_w, set);
	gtk_widget_set_sensitive(PCONFIG_DLG(dlg)->tray_trigger_status_w, set);
	gtk_widget_set_sensitive(PCONFIG_DLG(dlg)->tray_trigger_private_w, set);
	gtk_widget_set_sensitive(PCONFIG_DLG(dlg)->tray_trigger_channel_w, set);
	gtk_widget_set_sensitive(PCONFIG_DLG(dlg)->tray_tooltip_lines_w, set);
	gtk_widget_set_sensitive(PCONFIG_DLG(dlg)->tray_tooltip_lines_label_w, set);
}

static GtkWidget *
make_systray_frame(struct config_dlg * dlg)
{
	GtkWidget * b, * w;
	b = gtk_vbox_new(FALSE, 2);
	gtk_container_set_border_width(GTK_CONTAINER(b), GEOM_DLG_PAGE_BORDER);

	dlg->tray_enable_w = add_check_button(
		dlg, b, PREFS_GUI_TRAY_ENABLE,
		G_CALLBACK(systray_pref_gui_tray_enable_toggled_cb), (gpointer)dlg);

	dlg->tray_hide_on_startup_w
		= add_check_button(dlg, b, PREFS_GUI_TRAY_HIDE_WND_ON_STARTUP, NULL, NULL);

	w = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(b), w, FALSE, FALSE, 8);

	dlg->tray_trigger_joinleave_w = add_check_button(
		dlg, b, PREFS_GUI_TRAY_TRIGGERS_JOIN_LEAVE, NULL, NULL);
	dlg->tray_trigger_channel_w = add_check_button(
		dlg, b, PREFS_GUI_TRAY_TRIGGERS_CHANNEL, NULL, NULL);
	dlg->tray_trigger_private_w = add_check_button(
		dlg, b, PREFS_GUI_TRAY_TRIGGERS_PRIVATE, NULL, NULL);
	dlg->tray_trigger_status_w = add_check_button(
		dlg, b, PREFS_GUI_TRAY_TRIGGERS_STATUS, NULL, NULL);
	dlg->tray_trigger_topic_w = add_check_button(
		dlg, b, PREFS_GUI_TRAY_TRIGGERS_TOPIC, NULL, NULL);

	w = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(b), w, FALSE, FALSE, 8);

	dlg->tray_tooltip_lines_w = add_spin_button(
		dlg, b, PREFS_GUI_TRAY_TOOLTIP_LINE_NUM,
		0, 32, & dlg->tray_tooltip_lines_label_w, NULL, NULL);
		
	gtk_widget_show_all(b);
	return b;
}

static GtkWidget *
make_settings_frame(struct config_dlg * dlg)
{
	GtkWidget * b, * w;

	b = gtk_vbox_new(FALSE, 2);
	gtk_container_set_border_width(GTK_CONTAINER(b), GEOM_DLG_PAGE_BORDER);

	/* add other settings
	 */
	dlg->keep_window_size_w = add_check_button(dlg, b, PREFS_GUI_KEEP_SIZE, NULL, NULL);
	dlg->save_on_exit_w = add_check_button(dlg, b, PREFS_PREFS_AUTO_SAVE, NULL, NULL);

	w = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(b), w, FALSE, FALSE, 0);

	dlg->motd_w = add_entry(dlg, b, PREFS_NET_MOTD, NULL);
	dlg->motd_away_w = add_entry(dlg, b, PREFS_NET_MOTD_WHEN_AWAY, NULL);

	gtk_widget_show_all(b);
	return b;
}

static gboolean
config_dlg_key_cb(
	GtkWidget * dlg_w, GdkEventKey * event,
	struct config_dlg * dlg)
{
	if(event->keyval==GDK_Escape) {
		destroy_config_dlg(&the_dlg);
		return TRUE;
	}
	return FALSE;
}

/**
 * config_dlg_response_handler:
 *	handles user's response to the dialog
 */
static void
config_dlg_response_handler(
	GtkDialog * dialog,
	gint response,
	struct config_dlg * dlg)
{
	switch(response) {
	case GTK_RESPONSE_OK:
		if(dlg->is_changed)
			set_config_from_dlg(dlg);
		/* FALLTHROUGH */

	case GTK_RESPONSE_DELETE_EVENT:
	case GTK_RESPONSE_CANCEL:
		destroy_config_dlg(&the_dlg);
		break;

	case GTK_RESPONSE_APPLY:
		set_config_from_dlg(dlg);
		sync_dlg_from_config(dlg);
		break;

	case GUI_RESPONSE_SAVE:
		set_config_from_dlg(dlg);
		sync_dlg_from_config(dlg);
		raise_event(EVENT_IFACE_STORE_CONFIG, NULL, 0);
		break;
	}	
}

/*
 * stores values from the dialog to the configuration
 */
static void
set_config_from_dlg(struct config_dlg * dlg)
{
	/* signal that we are currently gathering
	 *	configuration and dialog should not be updated
	 *	(scattered) during this time
	 */
	dlg->gathering = TRUE;

#define READ_TOGGLE_PREFS(p, t) \
	prefs_set((p), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON((t))))
#define READ_SPIN_PREFS(p, s) \
	prefs_set((p), (guint)gtk_spin_button_get_value(GTK_SPIN_BUTTON(s)))
#define READ_OPTION_PREFS(p, o) \
	prefs_set((p), (guint)gtk_option_menu_get_history(GTK_OPTION_MENU(o)))
#define READ_ENTRY_PREFS(p, e) \
	prefs_set((p), gtk_entry_get_text(GTK_ENTRY((e))))
#define READ_IPV4_ENTRY(p, e) do { \
		guint32 ip; \
		if(util_parse_ipv4(gtk_entry_get_text(GTK_ENTRY(e)), &ip)) \
			prefs_set((p), ip); \
		set_ipv4_entry((e), prefs_int(p)); \
	} while(0)

	/* Tray icon */
	READ_TOGGLE_PREFS(PREFS_GUI_TRAY_ENABLE, dlg->tray_enable_w);
	READ_TOGGLE_PREFS(PREFS_GUI_TRAY_HIDE_WND_ON_STARTUP, dlg->tray_hide_on_startup_w);
	READ_TOGGLE_PREFS(PREFS_GUI_TRAY_TRIGGERS_JOIN_LEAVE, dlg->tray_trigger_joinleave_w);
	READ_TOGGLE_PREFS(PREFS_GUI_TRAY_TRIGGERS_CHANNEL, dlg->tray_trigger_channel_w);
	READ_TOGGLE_PREFS(PREFS_GUI_TRAY_TRIGGERS_PRIVATE, dlg->tray_trigger_private_w);
	READ_TOGGLE_PREFS(PREFS_GUI_TRAY_TRIGGERS_STATUS, dlg->tray_trigger_status_w);
	READ_TOGGLE_PREFS(PREFS_GUI_TRAY_TRIGGERS_TOPIC, dlg->tray_trigger_topic_w);
	READ_SPIN_PREFS(PREFS_GUI_TRAY_TOOLTIP_LINE_NUM, dlg->tray_tooltip_lines_w);

	/* Idle */
	READ_TOGGLE_PREFS(PREFS_IDLE_ENABLE, dlg->idle_enable_w);
	READ_SPIN_PREFS(PREFS_IDLE_AUTO_AWAY, dlg->auto_away_w);
	READ_SPIN_PREFS(PREFS_IDLE_AUTO_OFFLINE, dlg->auto_offline_w);

	/* Pages */
	READ_TOGGLE_PREFS(PREFS_GUI_TOPIC_BAR, dlg->pages_show_topic_entry);
	READ_TOGGLE_PREFS(PREFS_GUI_PRESENT_ON_PRIVATE, dlg->pages_present_on_private);
	READ_TOGGLE_PREFS(PREFS_MAIN_LOG_GLOBAL, dlg->pages_log_current_w);
	READ_TOGGLE_PREFS(PREFS_SESS_SUPPRESS_JOIN_LEAVE_TEXT, dlg->pages_suppress_join_leave_w);
	READ_TOGGLE_PREFS(PREFS_SESS_SUPPRESS_RENAME_TEXT, dlg->pages_suppress_rename_w);
	READ_TOGGLE_PREFS(PREFS_SESS_SUPPRESS_MODE_TEXT, dlg->pages_suppress_mode_change_w);
	READ_TOGGLE_PREFS(PREFS_SESS_STAMP_PRIVATE, dlg->date_private_w);
	READ_TOGGLE_PREFS(PREFS_SESS_STAMP_CHANNEL, dlg->date_channel_w);
	READ_TOGGLE_PREFS(PREFS_SESS_STAMP_STATUS, dlg->date_status_w);
	READ_OPTION_PREFS(PREFS_GUI_PAGE_TAB_POS, dlg->pages_notebook_pos_w);

	/* Network */
	READ_OPTION_PREFS(PREFS_NET_TYPE, dlg->net_type_w);
	READ_SPIN_PREFS(PREFS_NET_PORT, dlg->net_port_w);
	READ_ENTRY_PREFS(PREFS_NET_ENCODING, dlg->net_charset_w);
	READ_SPIN_PREFS(PREFS_USER_LIST_REFRESH, dlg->userlist_refresh_w);
	READ_TOGGLE_PREFS(PREFS_NET_REPLY_INFO_REQ, dlg->net_reply_w);
	READ_TOGGLE_PREFS(PREFS_NET_VERBOSE, dlg->net_log_w);
	READ_TOGGLE_PREFS(PREFS_USER_KEEP_UNREPLIED, dlg->userlist_keep_unreplied_users_w);
	READ_TOGGLE_PREFS(PREFS_NET_CHANNEL_NOTIFY, dlg->net_channel_greet_w);
	READ_TOGGLE_PREFS(PREFS_NET_USE_MULTICAST, dlg->net_use_multicast_w);
	READ_IPV4_ENTRY(PREFS_NET_BROADCAST_MASK, dlg->net_broadcast_w);
	READ_IPV4_ENTRY(PREFS_NET_MULTICAST_ADDR, dlg->net_multicast_w);

	/* Messages */
	READ_TOGGLE_PREFS(PREFS_NET_IGNORE_MASS_MSG, dlg->msg_ignore_mass_w);
	READ_TOGGLE_PREFS(PREFS_GUI_MSG_CTRL_ENTER, dlg->msg_ctrl_return_w);
	READ_TOGGLE_PREFS(
		PREFS_GUI_MSG_SHOW_IN_OFFLINE_DND, dlg->msg_show_messages_in_offline_dnd_w);
	READ_TOGGLE_PREFS(PREFS_GUI_MSG_IN_WINDOW, dlg->msg_show_window_w);
	READ_SPIN_PREFS(PREFS_GUI_MSG_MAX_WINDOWS, dlg->msg_max_w);

	/* Misc */
	READ_TOGGLE_PREFS(PREFS_GUI_KEEP_SIZE, dlg->keep_window_size_w);
	READ_TOGGLE_PREFS(PREFS_PREFS_AUTO_SAVE, dlg->save_on_exit_w);
	READ_ENTRY_PREFS(PREFS_NET_MOTD, dlg->motd_w);
	READ_ENTRY_PREFS(PREFS_NET_MOTD_WHEN_AWAY, dlg->motd_away_w);

	/* change the apply button, to notify that the the dialog is now in sync with config
	 * (however the config was not saved)
	 */
	set_config_dlg_modified(dlg, FALSE);

	/* enable scattering of dialog */
	dlg->gathering = FALSE;
}

/**
 * syncs dialog with the current configuration values
 */
static void
sync_dlg_from_config(struct config_dlg * dlg)
{
#define SET_SPIN_FROM_PREF(o, p) \
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(o), prefs_int(p))
#define SET_TOGGLE_FROM_PREF(o, p) \
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(o), prefs_bool(p))
#define SET_OPTION_FROM_PREF(o, p) \
	gtk_option_menu_set_history(GTK_OPTION_MENU(o), prefs_int(p))
#define SET_ENTRY_FROM_PREF(o, p) \
	gtk_entry_set_text(GTK_ENTRY(o), prefs_str(p))
#define SET_IPV4_ENTRY_FROM_PREF(e, p) \
	set_ipv4_entry((e), prefs_int(p))

	/* Idle */
	SET_TOGGLE_FROM_PREF(dlg->idle_enable_w, PREFS_IDLE_ENABLE);
 	SET_SPIN_FROM_PREF(dlg->auto_away_w, PREFS_IDLE_AUTO_AWAY);
	SET_SPIN_FROM_PREF(dlg->auto_offline_w, PREFS_IDLE_AUTO_OFFLINE);
	
	/* Pages */
	SET_TOGGLE_FROM_PREF(dlg->pages_show_topic_entry, PREFS_GUI_TOPIC_BAR);
	SET_TOGGLE_FROM_PREF(dlg->pages_present_on_private, PREFS_GUI_PRESENT_ON_PRIVATE);
	SET_TOGGLE_FROM_PREF(dlg->pages_log_current_w, PREFS_MAIN_LOG_GLOBAL);
	SET_TOGGLE_FROM_PREF(dlg->pages_suppress_join_leave_w, PREFS_SESS_SUPPRESS_JOIN_LEAVE_TEXT);
	SET_TOGGLE_FROM_PREF(dlg->pages_suppress_rename_w, PREFS_SESS_SUPPRESS_RENAME_TEXT);
	SET_TOGGLE_FROM_PREF(dlg->pages_suppress_mode_change_w, PREFS_SESS_SUPPRESS_MODE_TEXT);
	SET_TOGGLE_FROM_PREF(dlg->date_channel_w, PREFS_SESS_STAMP_CHANNEL);
	SET_TOGGLE_FROM_PREF(dlg->date_private_w, PREFS_SESS_STAMP_PRIVATE);
	SET_TOGGLE_FROM_PREF(dlg->date_status_w, PREFS_SESS_STAMP_STATUS);
	SET_OPTION_FROM_PREF(dlg->pages_notebook_pos_w, PREFS_GUI_PAGE_TAB_POS);

	/* Misc */
	SET_TOGGLE_FROM_PREF(dlg->keep_window_size_w, PREFS_GUI_KEEP_SIZE);
	SET_TOGGLE_FROM_PREF(dlg->save_on_exit_w, PREFS_PREFS_AUTO_SAVE);
	SET_ENTRY_FROM_PREF(dlg->motd_w, PREFS_NET_MOTD);
	SET_ENTRY_FROM_PREF(dlg->motd_away_w, PREFS_NET_MOTD_WHEN_AWAY);

	/* Tray icon */
	SET_TOGGLE_FROM_PREF(dlg->tray_enable_w, PREFS_GUI_TRAY_ENABLE);
	systray_pref_gui_tray_enable_toggled_cb(
		GTK_TOGGLE_BUTTON(dlg->tray_enable_w), (gpointer)dlg);
	SET_TOGGLE_FROM_PREF(dlg->tray_hide_on_startup_w, PREFS_GUI_TRAY_HIDE_WND_ON_STARTUP);
	SET_TOGGLE_FROM_PREF(dlg->tray_trigger_joinleave_w, PREFS_GUI_TRAY_TRIGGERS_JOIN_LEAVE);
	SET_TOGGLE_FROM_PREF(dlg->tray_trigger_channel_w, PREFS_GUI_TRAY_TRIGGERS_CHANNEL);
	SET_TOGGLE_FROM_PREF(dlg->tray_trigger_private_w, PREFS_GUI_TRAY_TRIGGERS_PRIVATE);
	SET_TOGGLE_FROM_PREF(dlg->tray_trigger_status_w, PREFS_GUI_TRAY_TRIGGERS_STATUS);
	SET_TOGGLE_FROM_PREF(dlg->tray_trigger_topic_w, PREFS_GUI_TRAY_TRIGGERS_TOPIC);
	SET_SPIN_FROM_PREF(dlg->tray_tooltip_lines_w, PREFS_GUI_TRAY_TOOLTIP_LINE_NUM);

	/* Network */
	SET_OPTION_FROM_PREF(dlg->net_type_w, PREFS_NET_TYPE);
	SET_SPIN_FROM_PREF(dlg->net_port_w, PREFS_NET_PORT);
	SET_ENTRY_FROM_PREF(dlg->net_charset_w, PREFS_NET_ENCODING);
	SET_TOGGLE_FROM_PREF(dlg->net_reply_w, PREFS_NET_REPLY_INFO_REQ);
	SET_TOGGLE_FROM_PREF(dlg->net_log_w, PREFS_NET_VERBOSE);
	SET_TOGGLE_FROM_PREF(dlg->net_channel_greet_w, PREFS_NET_CHANNEL_NOTIFY);
	SET_TOGGLE_FROM_PREF(dlg->net_use_multicast_w, PREFS_NET_USE_MULTICAST);
	SET_IPV4_ENTRY_FROM_PREF(dlg->net_broadcast_w, PREFS_NET_BROADCAST_MASK);
	SET_IPV4_ENTRY_FROM_PREF(dlg->net_multicast_w, PREFS_NET_MULTICAST_ADDR);

	/* User list settings */
	SET_TOGGLE_FROM_PREF(dlg->userlist_keep_unreplied_users_w, PREFS_USER_KEEP_UNREPLIED);
	SET_SPIN_FROM_PREF(dlg->userlist_refresh_w, PREFS_USER_LIST_REFRESH);

	/* Message settings */
	SET_TOGGLE_FROM_PREF(dlg->msg_ignore_mass_w, PREFS_NET_IGNORE_MASS_MSG);
	SET_TOGGLE_FROM_PREF(dlg->msg_ctrl_return_w, PREFS_GUI_MSG_CTRL_ENTER);
	SET_TOGGLE_FROM_PREF(
		dlg->msg_show_messages_in_offline_dnd_w, PREFS_GUI_MSG_SHOW_IN_OFFLINE_DND);
	SET_TOGGLE_FROM_PREF(dlg->msg_show_window_w, PREFS_GUI_MSG_IN_WINDOW);
	SET_SPIN_FROM_PREF(dlg->msg_max_w, PREFS_GUI_MSG_MAX_WINDOWS);
	messages_pref_in_window_toggled_cb(
		GTK_TOGGLE_BUTTON(dlg->msg_show_window_w), (gpointer)dlg);
	
	/* these settings are 'current' - disable the `Save' button */
	set_config_dlg_modified(dlg, FALSE);
}

static void
config_dlg_prefs_tree_selection_changed_cb(
	GtkTreeSelection * selection, struct config_dlg * dlg)
{
	GtkTreeIter iter;
	GtkTreeModel * model;
	gint index;

	if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_tree_model_get(model, &iter, CONFIG_TREE_INDEX, &index, -1);

		if(index >= 0)
			gtk_notebook_set_current_page(GTK_NOTEBOOK(dlg->notebook_w), index);
	}
}

static void 
show_config_dlg(struct config_dlg ** pdlg)
{
	struct config_dlg * dlg;
	GtkWidget * hbox, * scrolled;
	GtkCellRenderer * cell;
	GtkTreeViewColumn * column;
	GtkTreeSelection * selection;
	GtkTreeIter iter;

	if(*pdlg) return;
	
	*pdlg = dlg = g_malloc(sizeof(struct config_dlg));

	dlg->is_changed = FALSE;
	dlg->gathering = FALSE;

	/* create config dlg and setup signals */
	dlg->dlg_w = gtk_dialog_new_with_buttons(_("Preferences"), gui_get_main_window(), 0, NULL);

	gtk_dialog_add_button(GTK_DIALOG(dlg->dlg_w), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	dlg->save_w = gtk_dialog_add_button(
			GTK_DIALOG(dlg->dlg_w), GTK_STOCK_SAVE, GUI_RESPONSE_SAVE);
	gtk_button_box_set_child_secondary(
			GTK_BUTTON_BOX(GTK_DIALOG(dlg->dlg_w)->action_area), dlg->save_w, TRUE);
	dlg->apply_w = gtk_dialog_add_button(
			GTK_DIALOG(dlg->dlg_w), GTK_STOCK_APPLY, GTK_RESPONSE_APPLY);
	gtk_dialog_add_button(GTK_DIALOG(dlg->dlg_w), GTK_STOCK_OK, GTK_RESPONSE_OK);

	g_signal_connect(G_OBJECT(dlg->dlg_w), "response",
		G_CALLBACK(config_dlg_response_handler), (gpointer)dlg);
	g_signal_connect(G_OBJECT(dlg->dlg_w), "delete-event",
		G_CALLBACK(gtk_true), NULL);
	g_signal_connect(G_OBJECT(dlg->dlg_w), "key-press-event",
		G_CALLBACK(config_dlg_key_cb), (gpointer)dlg);

	/* make hbox for a tree and the notebook */
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg->dlg_w)->vbox), hbox, TRUE, TRUE, 0);

	/* setup prefs tree and insert into the dialog */
	dlg->prefs_tree_store = gtk_tree_store_new(CONFIG_TREE_NUM, G_TYPE_STRING, G_TYPE_INT);

	dlg->prefs_tree_w = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dlg->prefs_tree_store));
	gtk_widget_set_size_request(dlg->prefs_tree_w, 150, -1);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(dlg->prefs_tree_w), FALSE);

	/* add pref name renderer */
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			"Preference", cell,
			"text", CONFIG_TREE_NAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(dlg->prefs_tree_w), column);

	/* connect signals */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dlg->prefs_tree_w));
	g_signal_connect(
		G_OBJECT(selection), "changed",
		G_CALLBACK(config_dlg_prefs_tree_selection_changed_cb), dlg);

	scrolled = gtk_scrolled_window_new(FALSE, FALSE);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(hbox), scrolled, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(scrolled), dlg->prefs_tree_w);

	/* setup notebook and insert it into the dialog */
	dlg->notebook_w = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(dlg->notebook_w), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(dlg->notebook_w), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), dlg->notebook_w, TRUE, TRUE, 0);

	gtk_widget_show_all(hbox);

	/* add configuration pages */
#define ADD_PAGE(name, frame) do { \
		gint index = gtk_notebook_append_page( \
			GTK_NOTEBOOK(dlg->notebook_w), (frame), NULL); \
		gtk_tree_store_append(dlg->prefs_tree_store, &iter, NULL); \
		gtk_tree_store_set( \
			dlg->prefs_tree_store, &iter, \
			CONFIG_TREE_NAME, (name), CONFIG_TREE_INDEX, index, -1); \
	} while(0)

	ADD_PAGE(_("Network settings"), make_net_frame(dlg));
	ADD_PAGE(_("User list"), make_user_list_frame(dlg));
	ADD_PAGE(_("Pages"), make_pages_frame(dlg));
	ADD_PAGE(_("Messages"), make_messages_frame(dlg));
	ADD_PAGE(_("Tray icon"), make_systray_frame(dlg));
	ADD_PAGE(_("Idle"), make_idle_frame(dlg));
	ADD_PAGE(_("Miscellaneous"), make_settings_frame(dlg));

	/* show the window */
	gtk_window_present(GTK_WINDOW(dlg->dlg_w));
	gtk_widget_set_sensitive(dlg->save_w, !prefs_in_sync());
}

static void
set_config_dlg_modified(struct config_dlg * dlg, gboolean is_changed)
{
	gtk_widget_set_sensitive(dlg->apply_w, is_changed);
	if(is_changed)
		gtk_widget_set_sensitive(dlg->save_w, is_changed);
			
	dlg->is_changed = is_changed;
}

static void
destroy_config_dlg(struct config_dlg ** pdlg)
{
	if(*pdlg) {
		g_object_unref((*pdlg)->prefs_tree_store);
		gtk_widget_destroy((*pdlg)->dlg_w);

		g_free(*pdlg);
		*pdlg = NULL;
	}
}

static void
gui_config_event_cb(enum app_event_enum e, gpointer p, gint i)
{
	switch(e) {
	case EVENT_MAIN_INIT:
		the_dlg = NULL;
		break;

	case EVENT_MAIN_PRECLOSE:
		destroy_config_dlg(&the_dlg);
		break;
		
	case EVENT_PREFS_CHANGED:
		/* update dialog, if not currently gathering */
		if(the_dlg && !the_dlg->gathering)
			sync_dlg_from_config(the_dlg);
		break;

	case EVENT_PREFS_SAVED:
		/* disable 'Save' button, as the configuration now is in sync
		 */
		if(the_dlg)
			gtk_widget_set_sensitive(the_dlg->save_w, FALSE);
		break;
	case EVENT_IFACE_SHOW_CONFIGURE_DLG:
		/* make & show dialog */
		show_config_dlg(&the_dlg);
		sync_dlg_from_config(the_dlg);
		break;
	default:
		break;	/* nothing */
	}
}

/** exported routines
  *****************************************/
void gui_config_dlg_register()
{
	register_event_cb(
		gui_config_event_cb,
		EVENT_MAIN|EVENT_PREFS|EVENT_IFACE);
}

