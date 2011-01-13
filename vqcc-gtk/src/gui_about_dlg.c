/*
 * gui_about_dlg.c: implements the about dialog
 * Copyright (C) Saulius Menkevicius
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
 * $Id: gui_about_dlg.c,v 1.4 2005/01/04 16:13:02 bobas Exp $
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "main.h"
#include "gui.h"
#include "gui_misc.h"

#define ABOUT_DIALOG_WIDTH 300
#define ABOUT_DIALOG_HEIGHT 350

/* inline pixbuf include */
#include "../pixmaps/vqcc-gtk-large-pixbuf.h"

/* struct defs */
struct about_dlg_struct {
	GtkWidget * dialog_w;
};

struct author_info {
	const gchar * name, * email, * contributed_with;
};
struct translator_info {
	const gchar * language, * name, * email;
};

/* private vars */
static struct about_dlg_struct
	* the_about_dlg;

static struct author_info
authors_list[] = {
	{ "Saulius Menkevicius", "bob@nulis.lt", NULL },
	{ "Bryan Holty", "bryan_holty@xiotech.com", "support for multicast" },
	{ NULL }
};

static struct translator_info
translator_list[] = {
	{ N_("Lithuanian"), "Saulius Menkevicius", "bob@nulis.lt" },
	{ N_("Russian"), "Paul Petruk", "inet@tut.by" },
	{ NULL }
};

/* forward references */
static void about_dlg_destroy(struct about_dlg_struct ** p_dlg);

/* static funcs */
static void
about_dlg_response_cb(
	GtkDialog * dialog_w, gint response,
	struct about_dlg_struct * dlg)
{
	if(response==GTK_RESPONSE_OK)
		about_dlg_destroy(&the_about_dlg);
}

static gboolean
about_dlg_delete_cb(
	GtkWidget * dialog_w, GdkEvent * delete_event,
	struct about_dlg_struct * dlg)
{
	gtk_dialog_response(GTK_DIALOG(dialog_w), GTK_RESPONSE_OK);
	return FALSE;
}

static gboolean
about_dlg_key_press_cb(
	GtkWidget * dialog_w, GdkEventKey * event,
	struct about_dlg_struct * dlg)
{
	if(event->keyval==GDK_Escape) {
		gtk_dialog_response(GTK_DIALOG(dialog_w), GTK_RESPONSE_OK);
		return FALSE;
	}
	return TRUE;
}

/* about_dlg_show
 *	shows the dialog (or presents it to the user if already shown)
 */
static void
about_dlg_show(struct about_dlg_struct ** p_dlg)
{
	struct about_dlg_struct * dlg;
	GtkWidget * vbox, * label, * image, * scrolled, * textview;
	GdkPixbuf * pixbuf;
	GtkTextBuffer * textbuf;
	GtkTextIter iter;
	GtkTextTag * text_tag, * email_tag, * language_tag;
	struct author_info * author;
	struct translator_info * translator;
	
	if(*p_dlg) {
		gtk_window_present(GTK_WINDOW((*p_dlg)->dialog_w));
		return;
	}
	
	dlg = g_malloc(sizeof(struct about_dlg_struct));
	dlg->dialog_w = gtk_dialog_new_with_buttons(
		_("About vqcc-gtk"), gui_get_main_window(), 0,
		GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	
	g_signal_connect(G_OBJECT(dlg->dialog_w), "response",
		G_CALLBACK(about_dlg_response_cb), (gpointer)dlg);
	g_signal_connect(G_OBJECT(dlg->dialog_w), "delete-event",
		G_CALLBACK(about_dlg_delete_cb), (gpointer)dlg);
	g_signal_connect(G_OBJECT(dlg->dialog_w), "key-press-event",
		G_CALLBACK(about_dlg_key_press_cb), (gpointer)dlg);

	gtk_widget_set_size_request(dlg->dialog_w, ABOUT_DIALOG_WIDTH, ABOUT_DIALOG_HEIGHT);
	gtk_window_set_role(GTK_WINDOW(dlg->dialog_w), "about");

	vbox = gtk_vbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg->dialog_w)->vbox), vbox, TRUE, TRUE, 0);

	/* add image */
	pixbuf = gdk_pixbuf_new_from_inline(-1, vqcc_gtk_large, FALSE, NULL);
	image = gtk_image_new_from_pixbuf(pixbuf);
	g_object_unref(G_OBJECT(pixbuf));
	gtk_box_pack_start(GTK_BOX(vbox), image, FALSE, FALSE, 0);
	

	/* add label */
	label = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_label_set_markup(
		GTK_LABEL(label),
		"<span weight=\"bold\" size=\"large\">" PACKAGE " " VERSION "</span>"
	);

	/* add info text box inside scrolled window */
	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(scrolled), 10);
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW(scrolled),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

	textview = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);
	gtk_container_add(GTK_CONTAINER(scrolled), textview);

	textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

	text_tag = gtk_text_buffer_create_tag(
		textbuf, "text_tag",
		"wrap-mode", GTK_WRAP_WORD, NULL);
	
	email_tag = gtk_text_buffer_create_tag(
		textbuf, "email_tag",
		"foreground", "blue",
		"underline", PANGO_UNDERLINE_SINGLE, NULL);

	language_tag = gtk_text_buffer_create_tag(
		textbuf, "language_tag",
		"weight", PANGO_WEIGHT_BOLD, NULL);

#define APPEND(s, t) do { \
		gtk_text_buffer_get_end_iter(textbuf, &iter); \
		gtk_text_buffer_insert_with_tags(textbuf, &iter, (s), -1, (t), NULL); \
	} while(0)

	/* add about text */
	APPEND(_("vqcc-gtk is a chat application written in C for the GTK+"
		" toolkit, primarily used in small LAN's. Based on quickChat/"
		"Vypress Chat (TM) for Windows (from Vypress Research) and is"
		" licensed under the GPL.\n\n"),
		text_tag);

	/* add authors */
	APPEND(_("Authors:\n"), text_tag);

	for(author = authors_list; author->name; author++) {
		APPEND("    ", text_tag);
		APPEND(author->name, text_tag);
		APPEND(" <", text_tag);
		APPEND(author->email, email_tag);
		APPEND(">", text_tag);
		if(author->contributed_with) {
			APPEND(" (", text_tag);
			APPEND(_(author->contributed_with), text_tag);
			APPEND(")", text_tag);
		}
		APPEND("\n", text_tag);
	}

	/* add translator info */
	APPEND(_("\nTranslators:\n"), text_tag);

	for(translator = translator_list; translator->language; translator++) {
		APPEND("    ", text_tag);
		APPEND(_(translator->language), language_tag);
		APPEND(" ", text_tag);
		APPEND(translator->name, text_tag);
		APPEND(" <", text_tag);
		APPEND(translator->email, email_tag);
		APPEND(">\n", text_tag);
	}

	/* show the dialog */
	gtk_widget_show_all(vbox);

	*p_dlg = dlg;
	gtk_window_present(GTK_WINDOW(dlg->dialog_w));
}

/* about_dlg_destroy
 *	hides (destroys) the bout dialog
 */
static void
about_dlg_destroy(struct about_dlg_struct ** p_dlg)
{
	if(! *p_dlg)
		return;

	gtk_widget_destroy((*p_dlg)->dialog_w);

	g_free(*p_dlg);
	*p_dlg = NULL;
}

/*
 * about_dlg_event_cb:
 *	handles app events
 */
static void
about_dlg_event_cb(enum app_event_enum event, gpointer p, gint i)
{
	switch(event) {
	case EVENT_MAIN_INIT:
		the_about_dlg = NULL;
		break;

	case EVENT_MAIN_PRECLOSE:
		about_dlg_destroy(&the_about_dlg);
		break;

	case EVENT_IFACE_SHOW_ABOUT_DLG:
		about_dlg_show(&the_about_dlg);
		break;
		
	default:
		break;
	}
}

/*
 * exported routines
 */
void gui_about_dlg_register()
{
	register_event_cb(about_dlg_event_cb, EVENT_MAIN|EVENT_IFACE);
}

