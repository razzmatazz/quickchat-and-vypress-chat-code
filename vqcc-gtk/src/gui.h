/*
 * gui.h: implementation of the main window
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
 * $Id: gui.h,v 1.3 2004/12/09 22:00:48 bobas Exp $
 */

#ifndef GUI_H__
#define GUI_H__

/* preferences registered in the gui module */
#define PREFS_GUI_KEEP_SIZE		"gui/keep_wnd_size"
#define PREFS_GUI_SIZE			"gui/size"
#define PREFS_GUI_PRESENT_ON_PRIVATE	"gui/present_on_private"
#define PREFS_GUI_TITLE_MAX_LEN		"gui/title_max_len"
#define PREFS_GUI_TOPIC_BAR		"gui/topic_bar"
#define PREFS_GUI_MENU_BAR		"gui/menu bar"

void gui_register(int *, char *** argv);
void gui_run();
void gui_shutdown();

void gui_set_visible(gboolean);
gboolean gui_is_visible();
gboolean gui_is_active();

gpointer gui_get_main_window(); /* returns a GtkWindow, really */

#endif /* #ifndef GUI_H__ */

