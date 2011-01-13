/*
 * gui_msg.h: implements message dialog
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
 * $Id: gui_msg.h,v 1.7 2005/01/06 10:16:09 bobas Exp $
 */

#ifndef GUI_MSG_H__
#define GUI_MSG_H__

/* preferences registered in the `gui_msg' module */
#define PREFS_GUI_MSG_IN_WINDOW		"gui/msg/in_window"
#define PREFS_GUI_MSG_SUGGESTED_SIZE	"gui/msg/suggested_size"
#define PREFS_GUI_MSG_MAX_WINDOWS	"gui/msg/max_windows"
#define PREFS_GUI_MSG_CTRL_ENTER	"gui/msg/ctrl_enter"
#define PREFS_GUI_MSG_SHOW_IN_OFFLINE_DND "gui/msg/show_in_offline_dnd"

void gui_msg_dlg_register();

#endif /* #ifndef GUI_MSG_H__ */

