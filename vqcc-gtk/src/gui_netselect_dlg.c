/*
 * gui_netdetect_dlg.c: implements network autodetection dialog
 * Copyright (C) 2004 Saulius Menkevicius
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
 * $Id: gui_netselect_dlg.c,v 1.14 2004/12/29 15:48:18 bobas Exp $
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>

#include "main.h"
#include "net.h"
#include "prefs.h"
#include "gui.h"
#include "gui_misc.h"
#include "gui_netselect_dlg.h"
#include "util.h"

#include "libqcproto/qcproto.h"

#define HAVE_EXPANDER_WIDGET \
	((GTK_MAJOR_VERSION > 2) || ((GTK_MAJOR_VERSION==2) && (GTK_MINOR_VERSION>=4)))

#define LIST_MIN_HEIGHT	128

#define DEFAULT_DETECT_RANGE_BEGIN	8167
#define DEFAULT_DETECT_RANGE_END	8169

#define DEFAULT_NET_TYPE		NET_TYPE_VYPRESS

#define DETECT_DURATION	500	/* how much to wait for a response in network */
#define DETECT_PORT_STEP 5	/* number of ports to scan in each detection step */
#define DETECT_NICKNAME	"vqcc-net-detect"

struct netselect_dlg {
	GtkWidget * dlg_w,
		* tree_w,
#if(HAVE_EXPANDER_WIDGET)
		* detect_expander_w,
#endif
		* detect_begin_spin,
		* detect_end_spin,
		* detect_broadcast_mask_w,
		* detect_multicast_addr_w,
		* detect_btn,
		* detect_stop_btn,
		* detect_progress,
		* net_type_option,
		* net_port_spin,
		* net_use_multicast_w;
	
	guint detect_timeout_id;
	guint range_begin, range_end,
		range_current_begin, range_current_end;

	enum net_type_enum detect_net_type;
	gboolean detect_use_multicast;
	guint32 detect_broadcast_mask;
	guint32 detect_multicast_addr;
	guint detect_num_steps, detect_step;

	qcs_link detect_links[DETECT_PORT_STEP];

	GtkListStore * list;
};

enum network_list_column {
	NETLISTCOL_TYPE_STR,
	NETLISTCOL_PORT_STR,
	NETLISTCOL_USE_MULTICAST_STR,
	NETLISTCOL_TYPE,
	NETLISTCOL_PORT,
	NETLISTCOL_USE_MULTICAST,
	NETLISTCOL_NUM
};

static struct netselect_dlg * main_netselect_dlg;

/* forward ref's */
static void dlg_stop_detection(struct netselect_dlg * dlg, gboolean);
static void netselect_dlg_destroy(struct netselect_dlg ** dlg);

static void
detect_range_spin_value_changed(GtkSpinButton * spin_w, struct netselect_dlg * dlg)
{
	gint spin_begin = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dlg->detect_begin_spin)),
		spin_end = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dlg->detect_end_spin));

	if((GtkWidget*)spin_w==dlg->detect_begin_spin) {
		/* adjust range end so it doesnt get lower than range begin */
		if(spin_begin > spin_end)
			gtk_spin_button_set_value(
				GTK_SPIN_BUTTON(dlg->detect_end_spin), spin_begin);
	} else {
		/* adjust begining so it doesnt get higher than range end */
		if(spin_end < spin_begin)
			gtk_spin_button_set_value(
				GTK_SPIN_BUTTON(dlg->detect_begin_spin), spin_end);
	}
}

static void
dlg_search_mode(struct netselect_dlg * dlg, gboolean search)
{
	gtk_widget_set_sensitive(dlg->detect_begin_spin, !search);
	gtk_widget_set_sensitive(dlg->detect_end_spin, !search);
	gtk_widget_set_sensitive(dlg->detect_btn, !search);
	gtk_widget_set_sensitive(dlg->detect_stop_btn, search);
}

static void
dlg_add_detected_network(
	struct netselect_dlg * dlg,
	enum net_type_enum type,
	gushort port,
	gboolean use_multicast)
{
	GtkTreeIter iter;
	gchar * port_str;

	gtk_list_store_append(dlg->list, &iter);

	port_str = g_strdup_printf("%u", (guint)port);
	gtk_list_store_set(dlg->list, &iter,
		NETLISTCOL_TYPE_STR, net_name_of_type(type),
		NETLISTCOL_PORT_STR, port_str,
		NETLISTCOL_USE_MULTICAST_STR, use_multicast ? "Yes": "No",
		NETLISTCOL_TYPE, (guint)type,
		NETLISTCOL_PORT, (guint)port,
		NETLISTCOL_USE_MULTICAST, use_multicast,
		-1);
	g_free(port_str);
}

/* dlg_advance_detect_step:
 *	returns:
 *		FALSE, if no more steps needed
 */
gboolean
dlg_advance_detect_step(struct netselect_dlg * dlg)
{
	if(dlg->detect_use_multicast==FALSE
			&& dlg->detect_net_type==NET_TYPE_VYPRESS) {
		/* try with multicast (if net_type is NET_TYPE_VYPRESS) */
		dlg->detect_use_multicast = TRUE;
	}
	else if(dlg->detect_net_type < NET_TYPE_LAST) {
		/* try next network type */
		dlg->detect_net_type ++;
		dlg->detect_use_multicast = FALSE;
	}
	else {
		if(dlg->range_current_end < dlg->range_end) {
			/* skip to next port range */
			dlg->detect_net_type = NET_TYPE_FIRST;
			dlg->detect_use_multicast = FALSE;
			dlg->range_current_begin += DETECT_PORT_STEP;
			dlg->range_current_end = MIN(
				dlg->range_end,
				dlg->range_current_begin + DETECT_PORT_STEP - 1);
		} else {
			/* no more ports to test: we're finished */
			return FALSE;
		}
	}

	dlg->detect_step ++;
	return TRUE;
}

/* dlg_init_detect_links:
 *	returns:
 *		FALSE, if detection has not started
 */
static void
dlg_init_detect_links(struct netselect_dlg * dlg)
{
	guint16 port;
	qcs_msg * msg;

	/* setup network query msg */
	msg = qcs_newmsg();
	msg->msg = QCS_MSG_REFRESH_REQUEST;
	qcs_msgset(msg, QCS_SRC, DETECT_NICKNAME);

	/* open link and send query msg for each link opened */
	for(port = dlg->range_current_begin; port <= dlg->range_current_end; port++) {
		int num = port - dlg->range_current_begin;

		if(net_connected() && prefs_int(PREFS_NET_TYPE)==dlg->detect_net_type
				&& prefs_int(PREFS_NET_PORT)==port) {
			/*
			 * this is the network we're currently connected to;
			 * don't try to connect to it as the listening socket
			 * is already taken by the application
			 */
			dlg->detect_links[num] = NULL;
		} else {
			dlg->detect_links[num] = qcs_open(
				dlg->detect_net_type==NET_TYPE_VYPRESS
					? QCS_PROTO_VYPRESS: QCS_PROTO_QCHAT,
				dlg->detect_use_multicast ? QCS_PROTO_OPT_MULTICAST: 0,
				dlg->detect_use_multicast
					? dlg->detect_multicast_addr
					: dlg->detect_broadcast_mask,
				port);

			if(dlg->detect_links[num])
				qcs_send(dlg->detect_links[num], msg);
		}
	}

	qcs_deletemsg(msg);
}

static void
cleanup_detect_links(struct netselect_dlg * dlg)
{
	gint i;
	for(i = 0; i < DETECT_PORT_STEP; i++)
		if(dlg->detect_links[i]) {
			qcs_close(dlg->detect_links[i]);
			dlg->detect_links[i] = NULL;
		}
}

static gboolean
link_got_refresh_ack(qcs_link link)
{
	qcs_msg * msg = qcs_newmsg();
	gboolean refresh_ack_found = FALSE;
	
	while(qcs_waitinput(link, 0) > 0)
		if(qcs_recv(link, msg)) {
			if(msg->msg==QCS_MSG_REFRESH_ACK
					&& !nickname_cmp(msg->dst, DETECT_NICKNAME)) {
				/* found QCS_MSG_REFRESH_ACK msg for out nickname */
				refresh_ack_found = TRUE;
				break;
			}
		}

	qcs_deletemsg(msg);
	return refresh_ack_found;
}

static gboolean
detection_timeout(struct netselect_dlg * dlg)
{
	gint i;
	
	/* check if any of the links setup in last step had returned with success */
	for(i=0; i < DETECT_PORT_STEP; i ++) {
		if(dlg->detect_links[i] && link_got_refresh_ack(dlg->detect_links[i])) {
			/* ok, found at least one refresh ack to our msg on this network */
			dlg_add_detected_network(
				dlg,
				dlg->detect_net_type,
				dlg->range_current_begin + i,
				dlg->detect_use_multicast);
		}
	}
	cleanup_detect_links(dlg);

	if(!dlg_advance_detect_step(dlg)) {
		/* ok, we're finished */
		dlg_stop_detection(dlg, TRUE);
		return FALSE;
	}

	gtk_progress_bar_set_fraction(
		GTK_PROGRESS_BAR(dlg->detect_progress),
		(gfloat)dlg->detect_step / (gfloat)dlg->detect_num_steps);

	dlg_init_detect_links(dlg);

	return TRUE;	/* do not remove me! */
}

static void
dlg_start_detection(struct netselect_dlg * dlg)
{
	gint range_begin, range_end;

	range_begin = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dlg->detect_begin_spin));
	range_end = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dlg->detect_end_spin));

	if(!util_parse_ipv4(
		gtk_entry_get_text(GTK_ENTRY(dlg->detect_broadcast_mask_w)),
		&dlg->detect_broadcast_mask))
	{
#if(HAVE_EXPANDER_WIDGET)
		gtk_expander_set_expanded(GTK_EXPANDER(dlg->detect_expander_w), TRUE);
#endif
		gtk_widget_grab_focus(dlg->detect_broadcast_mask_w);
		gtk_editable_select_region(GTK_EDITABLE(dlg->detect_broadcast_mask_w), 0, -1);
		return;
	}
	
	if(!util_parse_ipv4(
		gtk_entry_get_text(GTK_ENTRY(dlg->detect_multicast_addr_w)),
		&dlg->detect_multicast_addr))
	{
#if(HAVE_EXPANDER_WIDGET)
		gtk_expander_set_expanded(GTK_EXPANDER(dlg->detect_expander_w), TRUE);
#endif
		gtk_widget_grab_focus(dlg->detect_multicast_addr_w);
		gtk_editable_select_region(GTK_EDITABLE(dlg->detect_multicast_addr_w), 0, -1);
		return;
	}

	/* put dialog in search mode (disable some of the controls) */
	dlg_search_mode(dlg, TRUE);

	/* cleanup current list */
	gtk_list_store_clear(dlg->list);

	if(net_connected()
		&& prefs_int(PREFS_NET_PORT) >= range_begin
		&& prefs_int(PREFS_NET_PORT) <= range_end)
	{
		/* add current network to the list as it won't get detected
		 * due to the socket already held by the application
		 */
		dlg_add_detected_network(
			dlg, prefs_int(PREFS_NET_TYPE), prefs_int(PREFS_NET_PORT),
			prefs_int(PREFS_NET_TYPE)==NET_TYPE_VYPRESS
				? prefs_bool(PREFS_NET_USE_MULTICAST) : FALSE
		);
	}

	/* init progress bar */
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dlg->detect_progress), 0.0);

	/* start it */
	dlg->range_begin = range_begin;
	dlg->range_end = range_end;
	dlg->range_current_begin = range_begin;
	dlg->range_current_end = MIN(
		dlg->range_end,
		dlg->range_current_begin + DETECT_PORT_STEP - 1);
	dlg->detect_net_type = NET_TYPE_FIRST;
	dlg->detect_use_multicast = FALSE;
	dlg->detect_num_steps =
		((dlg->range_end - dlg->range_begin + 1 + DETECT_PORT_STEP - 1) / DETECT_PORT_STEP)
			* (NET_TYPE_NUM + 1);
	dlg->detect_step = 0;
	memset(&dlg->detect_links, 0, sizeof(qcs_link) * DETECT_PORT_STEP);

	dlg_init_detect_links(dlg);

	dlg->detect_timeout_id = g_timeout_add(
		DETECT_DURATION, (GSourceFunc)detection_timeout, (gpointer)dlg);
}

static void
dlg_stop_detection(
	struct netselect_dlg * dlg,
	gboolean select_first)
{
	if(!dlg->detect_timeout_id)
		return;	
	
	/* remove timeout source, if active */
	g_source_remove(dlg->detect_timeout_id);
	dlg->detect_timeout_id = 0;

	/* close any detection qcs_links which left dangling
	 *  if we stoped detection in progress */
	cleanup_detect_links(dlg);
	
	/* put dialog in search mode (disable some controls) */
	dlg_search_mode(dlg, FALSE);

	/* clean progress bar */
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dlg->detect_progress), 0.0);

	if(select_first) {
		GtkTreeIter iter;

		/* select the first entry in the list to be active */
		if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(dlg->list), &iter)) {
			GtkTreePath * path;

			/* there's at least a single entry in the list, activate the first one */
			path = gtk_tree_model_get_path(GTK_TREE_MODEL(dlg->list), &iter);
			gtk_tree_view_set_cursor(GTK_TREE_VIEW(dlg->tree_w), path, NULL, FALSE);
			gtk_tree_path_free(path);
		}
	}
}

static void
list_row_activated(
	GtkTreeView * treeview_w,
	GtkTreePath * path, GtkTreeViewColumn * column,
	struct netselect_dlg * dlg)
{
	/* send response GTK_RESPONSE_OK,
	 * which will activate the selected network config in dlg_response() */
	gtk_dialog_response(GTK_DIALOG(dlg->dlg_w), GTK_RESPONSE_OK);
}

static void
list_cursor_changed(
	GtkTreeView * treeview_w,
	struct netselect_dlg * dlg)
{
	GtkTreePath * path;

	gtk_tree_view_get_cursor(treeview_w, &path, NULL);
	if(path) {
		GtkTreeIter iter;
		guint net_type, net_port;
		gboolean net_use_multicast;

		/* get iter to list entry */
		gtk_tree_model_get_iter(GTK_TREE_MODEL(dlg->list), &iter, path);
		gtk_tree_path_free(path);

		/* update manual entry controls */
		gtk_tree_model_get(
			GTK_TREE_MODEL(dlg->list), &iter,
			NETLISTCOL_TYPE, &net_type,
			NETLISTCOL_PORT, &net_port, 
			NETLISTCOL_USE_MULTICAST, &net_use_multicast, -1);

		gtk_option_menu_set_history(GTK_OPTION_MENU(dlg->net_type_option), net_type);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(dlg->net_port_spin), net_port);
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(dlg->net_use_multicast_w),
			net_use_multicast);
	}
}

static void
detect_button_clicked(GtkButton * button_w, struct netselect_dlg * dlg)
{
	if((GtkWidget*)button_w==dlg->detect_btn)
		dlg_start_detection(dlg);
	else
		dlg_stop_detection(dlg, FALSE);
}

static void
dlg_net_type_option_changed(gint new_net_type, struct netselect_dlg * dlg)
{
	gtk_widget_set_sensitive(dlg->net_use_multicast_w, new_net_type==NET_TYPE_VYPRESS);
	if(new_net_type!=NET_TYPE_VYPRESS)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dlg->net_use_multicast_w), FALSE);
}

static void
dlg_response(GtkDialog * dlg_w, gint response, struct netselect_dlg * dlg)
{
	switch(response) {
	case GTK_RESPONSE_OK:
		/* set network & port */
		prefs_set(PREFS_NET_TYPE, gtk_option_menu_get_history(
			GTK_OPTION_MENU(dlg->net_type_option)));
		prefs_set(PREFS_NET_PORT, gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(dlg->net_port_spin)));
		prefs_set(PREFS_NET_USE_MULTICAST,
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dlg->net_use_multicast_w)));
		prefs_set(PREFS_NET_MULTICAST_ADDR, dlg->detect_multicast_addr);
		prefs_set(PREFS_NET_BROADCAST_MASK, dlg->detect_broadcast_mask);

		prefs_set(PREFS_NET_IS_CONFIGURED, TRUE);

		/* hide the dialog */
		netselect_dlg_destroy(&main_netselect_dlg);
		break;
	case GTK_RESPONSE_CANCEL:
	case GTK_RESPONSE_DELETE_EVENT:
		if(!prefs_bool(PREFS_NET_IS_CONFIGURED)) {
			/* one cannot get away without network configured :) */
			raise_event(EVENT_IFACE_EXIT, NULL, 0);
		} else {
			/* network is already configured, destroy the dialog and do nothing */
			netselect_dlg_destroy(&main_netselect_dlg);
		}
		break;
	}
}

static void
netselect_dlg_show()
{
	struct netselect_dlg * dlg;
	GtkWidget * main_vbox, * scrolled,
		* button_box, * manual_hbox,
		* hbox, * vbox, * frame, * settings_vbox;
	GtkTreeViewColumn * column;
	gchar * text;

	if(main_netselect_dlg) {
		/* dialog is shown already */
		gtk_window_present(GTK_WINDOW(dlg->dlg_w));
		return;
	}
	
	dlg = main_netselect_dlg = g_new(struct netselect_dlg, 1);
	dlg->detect_timeout_id = 0;

	dlg->dlg_w = gtk_dialog_new_with_buttons(
		_("Network detection and configuration"), gui_get_main_window(), 0,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);
	g_signal_connect(G_OBJECT(dlg->dlg_w), "response",
		G_CALLBACK(dlg_response), (gpointer)dlg);
	g_signal_connect(G_OBJECT(dlg->dlg_w), "delete-event", G_CALLBACK(gtk_true), NULL);

	/* upper vertical box for list and port range controls
	 */
	main_vbox = gtk_vbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg->dlg_w)->vbox), main_vbox, TRUE, TRUE, 0);

	frame = gtk_frame_new(_("Detected networks"));
	gtk_container_set_border_width(GTK_CONTAINER(frame), 2);
	gtk_box_pack_start(GTK_BOX(main_vbox), frame, TRUE, TRUE, 0);

	dlg->list = gtk_list_store_new(
		NETLISTCOL_NUM,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
		G_TYPE_UINT, G_TYPE_UINT, G_TYPE_BOOLEAN);

	dlg->tree_w = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dlg->list));
	g_signal_connect(G_OBJECT(dlg->tree_w), "row-activated",
		G_CALLBACK(list_row_activated), (gpointer)dlg);
	g_signal_connect(G_OBJECT(dlg->tree_w), "cursor-changed",
		G_CALLBACK(list_cursor_changed), (gpointer)dlg);
	
	gtk_tree_view_insert_column_with_attributes(
		GTK_TREE_VIEW(dlg->tree_w), -1, prefs_description(PREFS_NET_TYPE),
		gtk_cell_renderer_text_new(), "text", NETLISTCOL_TYPE_STR, NULL);
	column = gtk_tree_view_get_column(GTK_TREE_VIEW(dlg->tree_w), 0);
	gtk_tree_view_column_set_resizable(column, TRUE);
		
	gtk_tree_view_insert_column_with_attributes(
		GTK_TREE_VIEW(dlg->tree_w), -1, prefs_description(PREFS_NET_PORT),
		gtk_cell_renderer_text_new(), "text", NETLISTCOL_PORT_STR, NULL);
	column = gtk_tree_view_get_column(GTK_TREE_VIEW(dlg->tree_w), 1);
	gtk_tree_view_column_set_resizable(column, TRUE);

	gtk_tree_view_insert_column_with_attributes(
		GTK_TREE_VIEW(dlg->tree_w), -1, prefs_description(PREFS_NET_USE_MULTICAST),
		gtk_cell_renderer_text_new(), "text", NETLISTCOL_USE_MULTICAST_STR, NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	
	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scrolled, -1, LIST_MIN_HEIGHT);
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled), dlg->tree_w);
	gtk_container_add(GTK_CONTAINER(frame), scrolled);

	/* Make hbox for lower control frames */
	manual_hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(main_vbox), manual_hbox, FALSE, FALSE, 0);

	/*
	 * "Automatic scan" frame
	 */
	frame = gtk_frame_new(_("Automatic scan"));
	gtk_container_set_border_width(GTK_CONTAINER(frame), 2);
	gtk_box_pack_start(GTK_BOX(manual_hbox), frame, TRUE, TRUE, 0);

	/* vbox inside the frame */
	vbox = gtk_vbox_new(FALSE, 4);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	/* progress bar
	 */
	dlg->detect_progress = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(vbox), dlg->detect_progress, FALSE, FALSE, 0);

	/* the 'reset' and 'refresh' buttons
	 */
	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button_box = gtk_hbutton_box_new();
	gtk_box_pack_end(GTK_BOX(hbox), button_box, FALSE, FALSE, 0);

	dlg->detect_btn = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
	g_signal_connect(G_OBJECT(dlg->detect_btn), "clicked",
		G_CALLBACK(detect_button_clicked), (gpointer)dlg);
	gtk_box_pack_start(GTK_BOX(button_box), dlg->detect_btn, FALSE, FALSE, 0);

	dlg->detect_stop_btn = gtk_button_new_from_stock(GTK_STOCK_STOP);
	g_signal_connect(G_OBJECT(dlg->detect_stop_btn), "clicked",
		G_CALLBACK(detect_button_clicked), (gpointer)dlg);
	gtk_box_pack_start(GTK_BOX(button_box), dlg->detect_stop_btn, FALSE, FALSE, 0);

	/* settings vbox */
	settings_vbox = gtk_vbox_new(FALSE, 4);

	/* port range start spin
	 */
	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(settings_vbox), hbox, FALSE, FALSE, 0);
	dlg->detect_begin_spin = gtk_spin_button_new_with_range(1024, 65535, 1);
	gtk_spin_button_set_value(
		GTK_SPIN_BUTTON(dlg->detect_begin_spin), DEFAULT_DETECT_RANGE_BEGIN);
	gtk_box_pack_end(GTK_BOX(hbox), dlg->detect_begin_spin, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), gtk_label_new(_("Scan UDP ports from")), FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(dlg->detect_begin_spin), "value-changed",
		G_CALLBACK(detect_range_spin_value_changed), (gpointer)dlg);

	/* port range end spin
	 */
	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(settings_vbox), hbox, FALSE, FALSE, 0);
	dlg->detect_end_spin = gtk_spin_button_new_with_range(1024, 65535, 1);
	gtk_spin_button_set_value(
		GTK_SPIN_BUTTON(dlg->detect_end_spin), DEFAULT_DETECT_RANGE_END);
	gtk_box_pack_end(GTK_BOX(hbox), dlg->detect_end_spin, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), gtk_label_new(_("to (including)")), FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(dlg->detect_end_spin), "value-changed",
		G_CALLBACK(detect_range_spin_value_changed), (gpointer)dlg);

	/* broadcast mask */
	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(settings_vbox), hbox, FALSE, FALSE, 0);
	dlg->detect_broadcast_mask_w = gtk_entry_new();
	gtk_entry_set_text(
		GTK_ENTRY(dlg->detect_broadcast_mask_w),
		text = util_inet_ntoa(prefs_int(PREFS_NET_BROADCAST_MASK)));
	g_free(text);
	gtk_box_pack_end(GTK_BOX(hbox), dlg->detect_broadcast_mask_w, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox),
		gtk_label_new(prefs_description(PREFS_NET_BROADCAST_MASK)), FALSE, FALSE, 0);

	/* multicast mask */
	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(settings_vbox), hbox, FALSE, FALSE, 0);
	dlg->detect_multicast_addr_w = gtk_entry_new();
	gtk_entry_set_text(
		GTK_ENTRY(dlg->detect_multicast_addr_w),
		text = util_inet_ntoa(prefs_int(PREFS_NET_MULTICAST_ADDR)));
	g_free(text);
	gtk_box_pack_end(GTK_BOX(hbox), dlg->detect_multicast_addr_w, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox),
		gtk_label_new(prefs_description(PREFS_NET_MULTICAST_ADDR)), FALSE, FALSE, 0);

	/* pack settings vbox in to the window (through the expander widget) */
#if(HAVE_EXPANDER_WIDGET)
	dlg->detect_expander_w = gtk_expander_new_with_mnemonic(_("Detection settings"));
	gtk_container_add(GTK_CONTAINER(dlg->detect_expander_w), settings_vbox);
	gtk_box_pack_start(GTK_BOX(vbox), dlg->detect_expander_w, FALSE, FALSE, 0);
#else
	gtk_box_pack_start(GTK_BOX(vbox), gtk_hseparator_new(), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), settings_vbox, FALSE, FALSE, 0);
#endif

	/*
	 * Manual port/network entry frame
	 */
	frame = gtk_frame_new(_("Network settings"));
	gtk_container_set_border_width(GTK_CONTAINER(frame), 2);
	gtk_box_pack_start(GTK_BOX(manual_hbox), frame, TRUE, TRUE, 0);

	/* vbox inside the frame */
	vbox = gtk_vbox_new(FALSE, 4);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	/* use multicast toggle */
	dlg->net_use_multicast_w = gtk_check_button_new_with_label(
		prefs_description(PREFS_NET_USE_MULTICAST));
	gtk_widget_set_sensitive(
		dlg->net_use_multicast_w, prefs_int(PREFS_NET_TYPE)==NET_TYPE_VYPRESS);
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON(dlg->net_use_multicast_w), 
		prefs_int(PREFS_NET_TYPE)==NET_TYPE_VYPRESS
			? prefs_bool(PREFS_NET_USE_MULTICAST): FALSE);

	/* network type option */
	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	dlg->net_type_option = util_net_type_option(
		(util_option_changed_cb)dlg_net_type_option_changed, (gpointer)dlg);
	gtk_box_pack_end(GTK_BOX(hbox), dlg->net_type_option, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox),
		gtk_label_new(prefs_description(PREFS_NET_TYPE)), FALSE, FALSE, 0);
	gtk_option_menu_set_history(
		GTK_OPTION_MENU(dlg->net_type_option), prefs_int(PREFS_NET_TYPE));

	/* network port number */
	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	dlg->net_port_spin = gtk_spin_button_new_with_range(1024, 65535, 1);
	gtk_box_pack_end(GTK_BOX(hbox), dlg->net_port_spin, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox),
		gtk_label_new(prefs_description(PREFS_NET_PORT)), FALSE, FALSE, 0);
	gtk_spin_button_set_value(
		GTK_SPIN_BUTTON(dlg->net_port_spin),
		prefs_int(PREFS_NET_PORT));

	/* append the multicast toggle */
	gtk_box_pack_start(GTK_BOX(vbox), dlg->net_use_multicast_w, FALSE, FALSE, 0);

	/* show dialog to the user */
	dlg_search_mode(dlg, FALSE);

	gtk_widget_show_all(dlg->dlg_w);
	gtk_window_present(GTK_WINDOW(dlg->dlg_w));
}

static void
netselect_dlg_destroy(struct netselect_dlg ** pdlg)
{
	/* stop any search in progress */
	dlg_stop_detection(*pdlg, FALSE);

	/* destroy dialog window */
	gtk_widget_destroy((*pdlg)->dlg_w);
	(*pdlg)->dlg_w = NULL;

	/* and free any structs ref'd */
	g_object_unref(G_OBJECT((*pdlg)->list));
	(*pdlg)->list = NULL;

	g_free(*pdlg);
	*pdlg = NULL;
}

void netselect_dlg_event(
	enum app_event_enum event,
	gpointer v, gint i)
{
	switch(event) {
	case EVENT_MAIN_INIT:
		main_netselect_dlg = NULL;
		break;

	case EVENT_MAIN_START:
		if(!prefs_bool(PREFS_NET_IS_CONFIGURED)) {
			g_assert(!main_netselect_dlg);

			netselect_dlg_show();
			dlg_start_detection(main_netselect_dlg);
		}
		break;

	case EVENT_MAIN_PRECLOSE:
		if(main_netselect_dlg)
			netselect_dlg_destroy(&main_netselect_dlg);
		break;

	case EVENT_IFACE_SHOW_NETDETECT_DLG:
		if(!main_netselect_dlg) {
			netselect_dlg_show();
			dlg_start_detection(main_netselect_dlg);
		}
		break;

	default: break;
	}
}

void gui_netselect_dlg_register()
{
	register_event_cb(netselect_dlg_event, EVENT_MAIN|EVENT_NET|EVENT_IFACE);
}

