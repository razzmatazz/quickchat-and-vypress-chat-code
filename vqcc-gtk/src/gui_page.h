/*
 * gui_page.h: chat notebook & page implementation
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
 * $Id: gui_page.h,v 1.7 2004/12/29 15:58:24 bobas Exp $
 */

#ifndef GUI_PAGE_H__
#define GUI_PAGE_H__

/* preferences registered in the 'gui_page' module */
#define PREFS_GUI_PAGE_TAB_POS	"gui/page/notebook_pos"

#include "sess.h"
#include "user.h"

enum text_attr {
	ATTR_NULL,
	ATTR_USER,
	ATTR_NORM,
	ATTR_MY_TEXT,
	ATTR_THEIR_TEXT,
	ATTR_ME_TEXT,
	ATTR_CHAN,
	ATTR_ERR,
	ATTR_TOPIC,
	ATTR_TIME,
	ATTR_NOTIFY,
	ATTR_NUM
};

enum gui_page_scroll_enum {
	SCROLL_HOME,
	SCROLL_PAGE_UP,
	SCROLL_PAGE_DOWN,
	SCROLL_END
};

void gui_page_register();
gpointer gui_page_alloc_notebook();	/* returns GtkNotebook * */

void gui_page_set_tab_pos(gboolean top);
int gui_page_new(
	enum session_type,
	const gchar * name, const gchar * topic,
	gboolean closeable,
	unsigned int max_lines,
	gpointer session,
	gpointer assoc_user);
void gui_page_delete(gpointer);
void gui_page_write(gpointer, enum text_attr, const gchar *, gboolean);
void gui_page_hilite(gpointer, gboolean);

void gui_page_scroll(gpointer, enum gui_page_scroll_enum);

gpointer gui_page_current();
gpointer gui_page_prev(gpointer);
gpointer gui_page_next(gpointer);
void gui_page_switch(gpointer);

#endif /* #ifdef GUI_PAGE_H__ */

