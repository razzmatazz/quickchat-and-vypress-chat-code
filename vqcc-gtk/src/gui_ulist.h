/*
 * gui_ulist.h: user list widget implementation
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
 * $Id: gui_ulist.h,v 1.2 2004/09/21 12:37:48 bobas Exp $
 */

#ifndef GUI_ULIST_H__
#define GUI_ULIST_H__

GtkWidget * gui_ulist_create();
void gui_ulist_free();
void gui_ulist_register();

#endif /* #ifndef GUI_ULIST_H__ */

