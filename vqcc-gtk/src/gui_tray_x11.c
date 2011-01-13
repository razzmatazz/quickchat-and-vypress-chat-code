/*
 * gui_tray_x11.c: support for freedesktop.org compatible system tray (GNOME/KDE)
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
 * $Id: gui_tray_x11.c,v 1.5 2004/12/14 01:22:18 bobas Exp $
 */

#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "misc/eggtrayicon.h"

#include "user.h"
#include "gui.h"
#include "gui_misc.h"

#define GUI_TRAY_IMPL
#include "gui_tray.h"

#include "../pixmaps/traypixbufs.h"

/* forward references
 */
static gboolean x11_create_tray_widget();

static gboolean	x11_tray_create();
static void	x11_tray_destroy();
static void	x11_tray_set_icon(gboolean, enum user_mode_enum);
static void	x11_tray_set_tooltip(const gchar *);
static void	x11_tray_set_embedded_notifer(tray_embedded_notifier *);
static void	x11_tray_set_removed_notifier(tray_removed_notifier *);
static void	x11_tray_set_clicked_notifier(tray_clicked_notifier *);

/* local (static) structs
 */
static struct tray_impl_ops 
impl_ops = {
	x11_tray_create,
	x11_tray_destroy,
	x11_tray_set_icon,
	x11_tray_set_tooltip,

	x11_tray_set_embedded_notifer,
	x11_tray_set_removed_notifier,
	x11_tray_set_clicked_notifier
};

#define N_TRAY_PIXBUFS	(2 * (UMODE_NUM_VALID))

static GtkWidget * tray_widget;
static GtkWidget * tray_icon_image;
static GdkPixbuf * tray_pixbufs[N_TRAY_PIXBUFS];
static GtkTooltips * tray_tooltips;

static tray_embedded_notifier * notifier_embedded;
static tray_removed_notifier * notifier_removed;
static tray_clicked_notifier * notifier_clicked;

/* tray implementation routines
 */

static gboolean
x11_tray_clicked_cb(
	GtkWidget * tray_w,
	GdkEventButton * event,
	gpointer user_data)
{
	if(notifier_clicked && event->type==GDK_BUTTON_PRESS) {
		notifier_clicked(event->button, event->time);
		return TRUE;
	}
	return FALSE;
}

static void
x11_tray_embedded_cb(GtkWidget * tray_w, gpointer user_data)
{
	if(notifier_embedded)
		notifier_embedded();
}

static void
x11_tray_destroyed_cb(GtkWidget * tray_w, gpointer user_data)
{
	tray_widget = NULL;

	/* recreate the widget, so it will get shown, once systray area reappears */
	g_idle_add(x11_create_tray_widget, NULL);

	/* invoke the "removed" notifier */
	if(notifier_removed)
		notifier_removed();
}

static gboolean
x11_create_tray_widget()
{
	GtkWidget * event_box;

	if(tray_widget) {
		gtk_widget_destroy(tray_widget);
		tray_widget = NULL;
	}

	tray_widget = GTK_WIDGET(egg_tray_icon_new("VQCC-gtk"));
	g_object_ref(G_OBJECT(tray_widget));

	event_box = gtk_event_box_new();
	tray_icon_image = gtk_image_new();
	g_signal_connect(G_OBJECT(event_box), "button-press-event", G_CALLBACK(x11_tray_clicked_cb), NULL);
	g_signal_connect(G_OBJECT(tray_widget), "embedded", G_CALLBACK(x11_tray_embedded_cb), NULL);
	g_signal_connect(G_OBJECT(tray_widget), "destroy", G_CALLBACK(x11_tray_destroyed_cb), NULL);
	
	gtk_container_add(GTK_CONTAINER(event_box), tray_icon_image);
	gtk_container_add(GTK_CONTAINER(tray_widget), event_box);
	gtk_widget_show_all(GTK_WIDGET(tray_widget));

	return FALSE;	/* if we're called as g_idle_add() callback (from x11_tray_destroyed_cb())*/
}

static gboolean
x11_tray_create()
{
	tray_tooltips = gtk_tooltips_new();
	g_object_ref(G_OBJECT(tray_tooltips));

	x11_create_tray_widget();

	return FALSE;
}

static void
x11_tray_destroy()
{
	gint i;

	g_object_unref(tray_tooltips);

	if(tray_widget) {
		/* remove "destroy" signal handler, so tray_destroyed_cb wouldn't recreate
		 * tray widget again
		 */
		g_signal_handlers_disconnect_by_func(
			G_OBJECT(tray_widget), G_CALLBACK(x11_tray_destroyed_cb), NULL);

		gtk_widget_destroy(tray_widget);
		tray_widget = NULL;

		/* invoke the tray_removed notifier */
		if(notifier_removed)
			notifier_removed();
	}

	/* unref all used tray icon pixbufs */
	for(i=0; i < N_TRAY_PIXBUFS; i++) {
		if(tray_pixbufs[i])
			g_object_unref(G_OBJECT(tray_pixbufs[i]));

		tray_pixbufs[i] = NULL;
	}

	/* cleanup notifier ptrs */
	notifier_embedded = NULL;
	notifier_removed = NULL;
	notifier_clicked = NULL;
}

static GdkPixbuf *
x11_tray_make_state_pixbuf(gboolean active, enum user_mode_enum user_mode)
{
	GdkPixbuf * pixbuf, * tag;
	guint pixbuf_num = (active ? 1: 0) * UMODE_NUM_VALID + (guint)user_mode;

	pixbuf = tray_pixbufs[pixbuf_num];
	if(pixbuf==NULL) {
		/* create pixbuf without a state tag */
		pixbuf = gdk_pixbuf_new_from_inline(
			-1, active ? pixbuf_logo_24_act: pixbuf_logo_24,
			TRUE, NULL);

		/* apply state tag */
		tag = util_user_mode_tag_pixbuf(user_mode);
		if(tag) {
			gint tag_x = gdk_pixbuf_get_width(pixbuf) - gdk_pixbuf_get_width(tag),
				tag_y = gdk_pixbuf_get_height(pixbuf) - gdk_pixbuf_get_height(tag);

			gdk_pixbuf_composite(
				tag, pixbuf,
				tag_x, tag_y,
				gdk_pixbuf_get_width(tag), gdk_pixbuf_get_height(tag),
				tag_x - 1, tag_y - 1, 1.0, 1.0,
				GDK_INTERP_NEAREST, 255);

			g_object_unref(G_OBJECT(tag));
		}

		/* store the pixbuf */
		tray_pixbufs[pixbuf_num] = pixbuf;
	}

	return pixbuf;
}

static void
x11_tray_set_icon(gboolean active, enum user_mode_enum user_mode)
{
	gtk_image_set_from_pixbuf(
		GTK_IMAGE(tray_icon_image),
		x11_tray_make_state_pixbuf(active, user_mode));
}

static void
x11_tray_set_tooltip(const gchar * tooltip_text)
{
	if(tooltip_text) {
		/* XXX: check that gtk_tooltips_enable is not called multiple times */
		gtk_tooltips_enable(tray_tooltips);

		gtk_tooltips_set_tip(tray_tooltips, tray_widget, tooltip_text, NULL);
	} else {
		gtk_tooltips_disable(tray_tooltips);
	}
}

static void
x11_tray_set_embedded_notifer(tray_embedded_notifier * notifier)
{
	notifier_embedded = notifier;
}

static void
x11_tray_set_removed_notifier(tray_removed_notifier * notifier)
{
	notifier_removed = notifier;
}

static void
x11_tray_set_clicked_notifier(tray_clicked_notifier * notifier)
{
	notifier_clicked = notifier;
}

/* exported methods
 */
const struct tray_impl_ops *
tray_impl_init()
{
	notifier_embedded = NULL;
	notifier_removed = NULL;
	notifier_clicked = NULL;

	/* clean pixbuf ptrs */
	memset((gpointer)tray_pixbufs, 0, sizeof(gpointer)*N_TRAY_PIXBUFS);

	return &impl_ops;
}

