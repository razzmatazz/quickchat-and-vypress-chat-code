/*
 * win32.c: win32 compatibility routine header
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
 * $Id: win32.h,v 1.2 2005/01/03 20:36:55 bobas Exp $
 */

#ifndef _VQCC_GTK_WIN32_H__

HINSTANCE winvqcc_get_hinstance();
unsigned int winvqcc_get_last_active();

#endif	/* #ifndef _VQCC_GTK_WIN32_H__ */

