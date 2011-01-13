/*
 * gui_misc.c: misc gui routines
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
 * $Id: gui_misc.c,v 1.17 2004/12/29 15:58:24 bobas Exp $
 */

#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "main.h"
#include "net.h"
#include "gui.h"
#include "gui_misc.h"

#include "../pixmaps/stockpixbufs.h"

static struct stock_pixbuf_map {
	const gchar * stock_id;
	const guint8 * pixbuf, * pixbuf1;
	const guint8 * button_pixbuf, * button_pixbuf1;
}
stock_pixbuf_maps[] = {
	{ GUI_STOCK_CHANNEL,
		pixbuf_icon_channel,		NULL,
		NULL, NULL },
	{ GUI_STOCK_CHANNEL_INACTIVE,
		pixbuf_icon_channel_inactive,	NULL,
		NULL, NULL },

	{ GUI_STOCK_STATUS,
		pixbuf_icon_status,		NULL,
		NULL, NULL },
	{ GUI_STOCK_STATUS_INACTIVE,
		pixbuf_icon_status_inactive,	NULL,
		NULL, NULL },

	{ GUI_STOCK_USER_DEAD,
		pixbuf_icon_user_dead,		NULL,
		NULL, NULL },
	{ GUI_STOCK_USER_INVISIBLE,
		pixbuf_icon_user_invisible, 	NULL,
		NULL,	NULL },

	{ GUI_STOCK_USER_NORMAL,
		pixbuf_icon_user_normal,	NULL,
		NULL, NULL },
	{ GUI_STOCK_USER_NORMAL_AWAY,
		pixbuf_icon_user_normal,	pixbuf_icon_user_tag_away,
		NULL,	NULL },
	{ GUI_STOCK_USER_NORMAL_DND,	
		pixbuf_icon_user_normal,	pixbuf_icon_user_tag_dnd,
		NULL,	NULL },
	{ GUI_STOCK_USER_NORMAL_OFFLINE,
		pixbuf_icon_user_normal,	pixbuf_icon_user_tag_offline,
		NULL,	NULL },

	{ GUI_STOCK_USER_INACTIVE,
		pixbuf_icon_user_inactive,	NULL,
		NULL, NULL },
	{ GUI_STOCK_USER_INACTIVE_AWAY,
		pixbuf_icon_user_inactive,	pixbuf_icon_user_tag_away,
		NULL,	NULL },
	{ GUI_STOCK_USER_INACTIVE_DND,	
		pixbuf_icon_user_inactive,	pixbuf_icon_user_tag_dnd,
		NULL,	NULL },
	{ GUI_STOCK_USER_INACTIVE_OFFLINE,
		pixbuf_icon_user_inactive,	pixbuf_icon_user_tag_offline,
		NULL,	NULL },

	{ GUI_STOCK_SEND_MESSAGE,
		pixbuf_icon_send,		NULL,
		pixbuf_icon_send_button, 	NULL },
	{ GUI_STOCK_REPLY_MESSAGE,
		pixbuf_icon_reply,		NULL,
		pixbuf_icon_reply_button,	NULL },
	{ GUI_STOCK_QUOTE_MESSAGE,
		pixbuf_icon_quote,		NULL,
		pixbuf_icon_quote_button,	NULL },
	{ NULL, }
};

static GtkStockItem
stock_items[] = {
	{ GUI_STOCK_SEND_MESSAGE, N_("_Send"),	0, 0, GETTEXT_PACKAGE },
	{ GUI_STOCK_REPLY_MESSAGE,N_("_Reply"),	0, 0, GETTEXT_PACKAGE },
	{ GUI_STOCK_QUOTE_MESSAGE,N_("_Quote"),	0, 0, GETTEXT_PACKAGE },
	{ NULL, }
};

static GtkIconFactory * icon_factory = 0;

/** private routines
 */

/** make_menu_item:
  *	 Makes GtkMenuItem containing specified label
  *	and (optionally) a PIX.
  *	 Special case when name=="---": separator is produced.
  */
static GtkWidget *
make_menu_item(
	const char * name,
	const gchar * stock_id,
	gboolean sensitive)
{
	GtkWidget * hbox_w, * image_w, * label_w, * item_w;

	g_assert(name);

	/* make menu item */
	item_w = gtk_menu_item_new();

	if(!strcmp(name, "---")) {
		/* this is separator */
		label_w = gtk_hseparator_new();
		gtk_container_add(GTK_CONTAINER(item_w), label_w);
	} else {
		/* usual item */
		label_w = gtk_label_new_with_mnemonic(name);
		
		if(stock_id) {
			/* image_w + label_w */
			hbox_w = gtk_hbox_new(FALSE, 4);

			image_w = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_MENU);

			gtk_box_pack_start(GTK_BOX(hbox_w), image_w, FALSE, FALSE, 0);
			gtk_box_pack_start(GTK_BOX(hbox_w), label_w, FALSE, FALSE, 0);

			gtk_container_add(GTK_CONTAINER(item_w), hbox_w);
		} else {
			gtk_container_add(GTK_CONTAINER(item_w), label_w);
		}
	}

	/* make it insensitive, if needed */
	if(!sensitive) {
		gtk_widget_set_sensitive(item_w, FALSE);
	}

	return item_w;
}

/** E X P O R T E D   R O U T I N E S
  ******************************************/

/** gui_misc_init:
  *	initializes the module
  */
void gui_misc_init()
{
	GtkIconSource * source;
	GtkIconSet * set;
	struct stock_pixbuf_map * map;
	gint num_items;

	/* setup icon factory and all the icons it contains
	 */
	if(icon_factory==0) {
		/* setup icon factory and all the icons it contains */
		icon_factory = gtk_icon_factory_new();
		gtk_icon_factory_add_default(icon_factory);

		/* register icons */
		for(map = stock_pixbuf_maps; map->stock_id; map ++) {
			GdkPixbuf * pixbuf, * composite;

			g_assert(map->pixbuf);

			set = gtk_icon_set_new();

			/* create icon source for a small 16x16 icon */
			source = gtk_icon_source_new();
			gtk_icon_source_set_size(source, GTK_ICON_SIZE_MENU);

			pixbuf = gdk_pixbuf_new_from_inline(-1, map->pixbuf, TRUE, NULL);
			if(map->pixbuf1) {
				composite = gdk_pixbuf_new_from_inline(
						-1, map->pixbuf1, FALSE, NULL);

				/* add composite pixbuf */
				gdk_pixbuf_composite(
					composite, pixbuf, 0, 0,
					gdk_pixbuf_get_width(pixbuf),
					gdk_pixbuf_get_height(pixbuf),
					0.0, 0.0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);

				/* release the composite pixbuf */
				g_object_unref(G_OBJECT(composite));
			}
			gtk_icon_source_set_pixbuf(source, pixbuf);

			gtk_icon_set_add_source(set, source);
			gtk_icon_source_free(source);	/* this will g_object_unref `pixbuf, too */

			/* create icon source for a "button" 24x24 icon (if any) */
			if(map->button_pixbuf) {
				source = gtk_icon_source_new();
				gtk_icon_source_set_size(source, GTK_ICON_SIZE_BUTTON);

				pixbuf = gdk_pixbuf_new_from_inline(
						-1, map->button_pixbuf, TRUE, NULL);
				if(map->button_pixbuf1) {
					composite = gdk_pixbuf_new_from_inline(
							-1, map->button_pixbuf1, FALSE, NULL);

					gdk_pixbuf_composite(
						composite, pixbuf, 0, 0,
						gdk_pixbuf_get_width(pixbuf),
						gdk_pixbuf_get_height(pixbuf),
						0.0, 0.0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);

					g_object_unref(G_OBJECT(composite));
				}
				gtk_icon_source_set_pixbuf(source, pixbuf);

				gtk_icon_set_add_source(set, source);
				gtk_icon_source_free(source);	/* this will free the pixbuf, too */
			}

			/* add icon set to icon_factory & release it */
			gtk_icon_factory_add(icon_factory, map->stock_id, set);
			gtk_icon_set_unref(set);
		}
	}

	/* register stock items
	 */
	for(num_items=0; stock_items[num_items].stock_id!=NULL; num_items++) ;
	gtk_stock_add_static(stock_items, num_items);
}

/** gui_misc_destroy:
  *	cleanups data allocated in module
  */
void gui_misc_destroy()
{
	if(icon_factory) {
		/* unreference and remove the icon factory */
		gtk_icon_factory_remove_default(icon_factory);
		g_object_unref(G_OBJECT(icon_factory));
	}
}

/**
 * gui_misc_set_icon_for:
 *	sets vqcc-gtk icon (list) for specified window
 */
void
gui_misc_set_icon_for(GtkWindow * window)
{
	GList * icon_list = NULL;

	/* make icon list */
	icon_list = g_list_prepend(
			icon_list, gdk_pixbuf_new_from_inline(-1, pixbuf_logo_24, FALSE, NULL));
	icon_list = g_list_prepend(
			icon_list, gdk_pixbuf_new_from_inline(-1, pixbuf_logo_32, FALSE, NULL));
	icon_list = g_list_prepend(
			icon_list, gdk_pixbuf_new_from_inline(-1, pixbuf_logo_48, FALSE, NULL));

	/* set window icons */
	gtk_window_set_icon_list(window, icon_list);

	/* unref icons & free the list */
	while(icon_list) {
		g_object_unref(G_OBJECT(icon_list->data));
		icon_list = g_list_delete_link(icon_list, icon_list);
	}
}

const gchar *
util_user_state_stock(enum user_mode_enum mode, gboolean active)
{
	const gchar * icon;

	switch(mode) {
	case UMODE_AWAY:
		icon = active ? GUI_STOCK_USER_NORMAL_AWAY: GUI_STOCK_USER_INACTIVE_AWAY;
		break;
	case UMODE_DND:
		icon = active ? GUI_STOCK_USER_NORMAL_DND: GUI_STOCK_USER_INACTIVE_DND;
		break;
	case UMODE_OFFLINE:
		icon = active ? GUI_STOCK_USER_NORMAL_OFFLINE: GUI_STOCK_USER_INACTIVE_OFFLINE;
		break;
	case UMODE_INVISIBLE:
		icon = GUI_STOCK_USER_INVISIBLE;
		break;
	case UMODE_DEAD:
		icon = GUI_STOCK_USER_DEAD;
		break;
	default:
		icon = active ? GUI_STOCK_USER_NORMAL: GUI_STOCK_USER_INACTIVE;
		break;
	}
	return icon;
}

GdkPixbuf * util_user_mode_tag_pixbuf(enum user_mode_enum user_mode)
{
	GdkPixbuf * pixbuf;

	switch(user_mode) {
	case UMODE_AWAY:
		pixbuf = gdk_pixbuf_new_from_inline(-1, pixbuf_icon_user_tag_away, FALSE, NULL);
		break;
	case UMODE_DND:
		pixbuf = gdk_pixbuf_new_from_inline(-1, pixbuf_icon_user_tag_dnd, FALSE, NULL);
		break;
	case UMODE_OFFLINE:
		pixbuf = gdk_pixbuf_new_from_inline(-1, pixbuf_icon_user_tag_offline, FALSE, NULL);
		break;
	default:
		pixbuf = NULL;
		break;
	}
	return pixbuf;
}

static void
util_option_fwd_cb(GtkOptionMenu * opt_menu, gpointer callback)
{
	((util_option_changed_cb)callback)(
		gtk_option_menu_get_history(GTK_OPTION_MENU(opt_menu)),
		g_object_get_data(G_OBJECT(opt_menu), "changed_userdata")
	);
}

GtkWidget *
util_net_type_option(util_option_changed_cb changed_cb, gpointer userdata)
{
	GtkWidget * menu, * opt_menu;
	int i;
	
	menu = gtk_menu_new();
	for (i = 0; i < NET_TYPE_NUM; i++)
		gtk_menu_shell_append(
			GTK_MENU_SHELL(menu),
			make_menu_item(net_name_of_type(i), NULL, TRUE));

	opt_menu = gtk_option_menu_new();
	g_object_set_data(G_OBJECT(opt_menu), "changed_userdata", userdata);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(opt_menu), menu);
	if(changed_cb)
		g_signal_connect(G_OBJECT(opt_menu), "changed",
			G_CALLBACK(util_option_fwd_cb), changed_cb);

	return opt_menu;
}

GtkWidget *
util_user_mode_option(util_option_changed_cb changed_cb, gpointer userdata)
{
	GtkWidget * menu, * opt_menu;
	int mode;

	menu = gtk_menu_new();
	for (mode = UMODE_FIRST_VALID; mode < UMODE_NUM_VALID; mode++)
		gtk_menu_shell_append(
			GTK_MENU_SHELL(menu),
			make_menu_item(user_mode_name(mode),
				util_user_state_stock(mode, TRUE), TRUE) );

	opt_menu = gtk_option_menu_new();
	g_object_set_data(G_OBJECT(opt_menu), "changed_userdata", userdata);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(opt_menu), (gpointer)menu);
	if(changed_cb)
		g_signal_connect(G_OBJECT(opt_menu), "changed",
			G_CALLBACK(util_option_fwd_cb), changed_cb);

	return opt_menu;
}

GtkWidget *
util_image_menu_item(
	const gchar * icon_stock_id,
	const gchar * label_text,
	GCallback activate_cb,
	gpointer activate_user_data)
{
	GtkWidget * item_w;

	if(icon_stock_id) {
		item_w = gtk_image_menu_item_new_with_label("");
		gtk_image_menu_item_set_image(
			GTK_IMAGE_MENU_ITEM(item_w),
			gtk_image_new_from_stock(icon_stock_id, GTK_ICON_SIZE_MENU));
	} else {
		item_w = gtk_menu_item_new_with_label("");
	}

	gtk_label_set_markup_with_mnemonic(GTK_LABEL(GTK_BIN(item_w)->child), label_text);

	/* hook up signal callback */
	if(activate_cb)
		g_signal_connect(G_OBJECT(item_w), "activate", activate_cb, activate_user_data);

	return item_w;
}


/** misc_pix_button:
 *	makes button with pix & label
 */
GtkWidget *
misc_pix_button(
	const char * label,
	const gchar * pix_stock_id,
	GCallback click_cb,
	gpointer click_cb_param)
{
	GtkWidget * hbox_w, * btn_w, * w;

	/* make button */
	btn_w = gtk_button_new();
	gtk_widget_show(btn_w);

	/* make insides of button */
	hbox_w = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox_w);
	gtk_container_add(GTK_CONTAINER(btn_w), hbox_w);

	w = gtk_image_new_from_stock(pix_stock_id, GTK_ICON_SIZE_MENU);
	gtk_widget_show(w);
	gtk_box_pack_start(GTK_BOX(hbox_w), w, FALSE, FALSE, 2);
	
	w = gtk_label_new_with_mnemonic(label);
	gtk_widget_show(w);
	gtk_box_pack_end(GTK_BOX(hbox_w), w, FALSE, FALSE, 2);

	/* connect "click" signal */
	if(click_cb) {
		g_signal_connect(G_OBJECT(btn_w), "clicked",
				click_cb, click_cb_param);
	}

	return btn_w;
}


