/*
 * gui_misc.h: misc gui routines
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
 * $Id: gui_misc.h,v 1.14 2004/12/29 15:58:24 bobas Exp $
 */

#ifndef GUI_MISC_H__
#define GUI_MISC_H__

#include "user.h"

#define GUI_STOCK_LOGO		"vqcc-logo"
#define GUI_STOCK_LOGO_NO_Q	"vqcc-logo-no-q"
#define GUI_STOCK_STATUS	"vqcc-status"
#define GUI_STOCK_STATUS_INACTIVE "vqcc-status-inactive"
#define GUI_STOCK_CHANNEL	"vqcc-channel"
#define GUI_STOCK_CHANNEL_INACTIVE "vqcc-channel-inactive"
#define GUI_STOCK_OPEN_CHAT	GTK_STOCK_JUSTIFY_LEFT
#define GUI_STOCK_CLOSE_CHAT	GTK_STOCK_CLOSE
#define GUI_STOCK_SET_TOPIC	GTK_STOCK_BOLD
#define GUI_STOCK_SEND_MESSAGE	"vqcc-send-message"
#define GUI_STOCK_REPLY_MESSAGE	"vqcc-reply-message"
#define	GUI_STOCK_QUOTE_MESSAGE	"vqcc-quote-message"
#define GUI_STOCK_IGNORE	GTK_STOCK_STOP
/* #define GUI_STOCK_UNIGNORE	GTK_STOCK_STOP */
#define GUI_STOCK_USER_DEAD	"vqcc-user-dead"
#define GUI_STOCK_USER_INVISIBLE "vqcc-user-invisible"
/* #define GUI_STOCK_BEEP 	"vqcc-beep" */

#define GUI_STOCK_USER_NORMAL		"vqcc-user-normal"
#define GUI_STOCK_USER_NORMAL_AWAY	"vqcc-user-away"
#define GUI_STOCK_USER_NORMAL_DND	"vqcc-user-dnd"
#define GUI_STOCK_USER_NORMAL_OFFLINE	"vqcc-user-offline"

#define GUI_STOCK_USER_INACTIVE		"vqcc-user-inactive"
#define GUI_STOCK_USER_INACTIVE_AWAY	"vqcc-user-inactive-away"
#define GUI_STOCK_USER_INACTIVE_DND	"vqcc-user-inactive-grey"
#define GUI_STOCK_USER_INACTIVE_OFFLINE	"vqcc-user-inactive-grey"

enum gui_response_enum {
	GUI_RESPONSE_SAVE,
	GUI_RESPONSE_SEND,
	GUI_RESPONSE_REPLY,
	GUI_RESPONSE_QUOTE
};

void gui_misc_init();
void gui_misc_destroy();

void gui_misc_set_icon_for(GtkWindow *);

const gchar * util_user_state_stock(enum user_mode_enum mode, gboolean active);
GdkPixbuf * util_user_mode_tag_pixbuf(enum user_mode_enum);

/*
 * widget factories
 */
typedef void (*util_option_changed_cb)(gint, gpointer);
GtkWidget * util_user_mode_option(util_option_changed_cb, gpointer);
GtkWidget * util_net_type_option(util_option_changed_cb, gpointer);

GtkWidget * util_image_menu_item(const gchar *, const gchar *, GCallback, gpointer);

GtkWidget * misc_pix_button(
	const char * label,
	const gchar * pix_stock_id,
	GCallback click_cb,
	gpointer click_cb_param);

#endif	/* #ifndef GUI_MISC_H__ */

