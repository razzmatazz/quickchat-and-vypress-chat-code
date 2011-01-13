/*
 * gui_topic_dlg.c: implements topic change dialog
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
 * $Id: gui_topic_dlg.c,v 1.5 2005/01/03 01:20:22 bobas Exp $
 */

#include <glib.h>
#include <gtk/gtk.h>

#include "main.h"
#include "sess.h"
#include "gui.h"
#include "gui_misc.h"

#define DEFAULT_DIALOG_WIDTH	400

struct topic_dlg {
	GtkWidget * dialog_w, * entry_w;
	gpointer session;
};

/* there's always a single topic change dialog */
static struct topic_dlg * main_topic_dlg;

/* topic_dlg_destroy():
 *	destroys the topic dialog
 */
static void
topic_dlg_destroy()
{
	if(main_topic_dlg) {
		gtk_widget_destroy(main_topic_dlg->dialog_w);
		g_free(main_topic_dlg);

		main_topic_dlg = NULL;
	}
}

/*
 * Handles topic entry activation (Enter keypress)
 */
static void
topic_dlg_entry_activate_cb(GtkEntry * entry, struct topic_dlg * dlg)
{
	gtk_dialog_response(GTK_DIALOG(dlg->dialog_w), GTK_RESPONSE_OK);
}

/*
 * Handles window delete (close) event -- hides the dialog
 */
static gboolean
topic_dlg_delete_cb(GtkDialog * dialog_w, GdkEvent * e, struct topic_dlg * dlg)
{
	gtk_dialog_response(GTK_DIALOG(dlg->dialog_w), GTK_RESPONSE_CANCEL);
	return TRUE;	/* don't destroy the dialog widget */
}

/*
 * Handles topic dialog response (OK/CANCEL) events
 */
static void
topic_dlg_response_cb(GtkDialog * dialog_w, gint response, struct topic_dlg * dlg)
{
	gpointer v[2];

	switch(response) {
	case GTK_RESPONSE_OK:
		EVENT_V(v, 0) = dlg->session;
		EVENT_V(v, 1) = (gpointer)gtk_entry_get_text(GTK_ENTRY(dlg->entry_w));
		raise_event(EVENT_IFACE_TOPIC_ENTER, v, 0);
		/* FALLTHROUGH */

	case GTK_RESPONSE_CANCEL:
		topic_dlg_destroy();
		break;

	default:
		break;
	}
}

/*
 * Builds new topic change dialog
 */
static struct topic_dlg *
topic_dlg_make()
{
	GtkWidget * hbox, * label;
	struct topic_dlg * dlg;
	
	dlg = g_new0(struct topic_dlg, 1);
	dlg->session = NULL;

	dlg->dialog_w = gtk_dialog_new_with_buttons(
		NULL, gui_get_main_window(), 0,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

	g_signal_connect(G_OBJECT(dlg->dialog_w), "response",
		G_CALLBACK(topic_dlg_response_cb), (gpointer)dlg);
	g_signal_connect(G_OBJECT(dlg->dialog_w), "delete-event",
		G_CALLBACK(topic_dlg_delete_cb), (gpointer)dlg);

	gtk_widget_set_size_request(dlg->dialog_w, DEFAULT_DIALOG_WIDTH, -1);

	/* add hbox for label & entry widgets */
	hbox = gtk_hbox_new(FALSE, 4);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 8);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg->dialog_w)->vbox), hbox, TRUE, TRUE, 0);

	label = gtk_label_new(_("Topic:"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	
	dlg->entry_w = gtk_entry_new_with_max_length(MAX_TOPIC_LENGTH);
	g_signal_connect(G_OBJECT(dlg->entry_w), "activate",
		G_CALLBACK(topic_dlg_entry_activate_cb), (gpointer)dlg);
	gtk_widget_show(dlg->entry_w);
	gtk_box_pack_start(GTK_BOX(hbox), dlg->entry_w, TRUE, TRUE, 0);

	return dlg;
}

/*
 * Shows topic change dialog for specified session.
 * In case there's already dialog shown for another session,
 *  it gets retargeted to specified session.
 */
static void
topic_dlg_show_for_session(gpointer session)
{
	gchar * title;

	g_assert(session);

	/* build the dialog it was not used before */
	if(!main_topic_dlg) {
		main_topic_dlg = topic_dlg_make();
	}
	else if(main_topic_dlg->session==session) {
		/* we already have topic change dialog for this session */
		return;
	}

	/* setup/update session dialog */
	main_topic_dlg->session = session;
	
	gtk_entry_set_text(GTK_ENTRY(main_topic_dlg->entry_w), sess_topic(main_topic_dlg->session));
	gtk_editable_select_region(GTK_EDITABLE(main_topic_dlg->entry_w), 0, -1);

	title = g_strdup_printf(_("Topic for #%s"), sess_name(main_topic_dlg->session));
	gtk_window_set_title(GTK_WINDOW(main_topic_dlg->dialog_w), title);
	g_free(title);

	/* present the dialog */
	gtk_window_present(GTK_WINDOW(main_topic_dlg->dialog_w));
}

/*
 * topic_dlg_event_cb:
 *	handles app events
 */
static void
topic_dlg_event_cb(enum app_event_enum event, gpointer p, gint i)
{
	switch(event) {
	case EVENT_MAIN_INIT:
		main_topic_dlg = NULL;
		break;
	case EVENT_MAIN_START:
		break;
	case EVENT_MAIN_PRECLOSE:
		topic_dlg_destroy();
		break;
	case EVENT_MAIN_CLOSE:
		break;
	case EVENT_IFACE_SHOW_TOPIC_DLG:
		topic_dlg_show_for_session(p);
		break;
	case EVENT_SESSION_CLOSED:
		if(main_topic_dlg && main_topic_dlg->session==p) {
			topic_dlg_destroy();
		}
		break;
	default:
		break;
	}
}

/*
 * exported routines
 */
void gui_topic_dlg_register()
{
	register_event_cb(topic_dlg_event_cb, EVENT_MAIN|EVENT_IFACE|EVENT_SESSION);
}

