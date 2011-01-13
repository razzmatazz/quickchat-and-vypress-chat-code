/*
 * idle.h: manages app usage (auto away/offline)
 * Copyright (C) 2002,2003 Saulius Menkevicius
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
 * $Id: idle.h,v 1.4 2004/12/29 15:58:24 bobas Exp $
 */

#ifndef IDLE_H__
#define IDLE_H__

/* preferences registered in the `idle' module */
#define PREFS_IDLE_AUTO_AWAY	"idle/auto_away"
#define PREFS_IDLE_AUTO_OFFLINE	"idle/auto_offline"
#define PREFS_IDLE_ENABLE	"idle/enable"

void idle_register();

#endif /* #ifndef IDLE_H__ */

