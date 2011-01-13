/*
 * user.h: user list management
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
 * $Id: user.h,v 1.9 2004/09/27 22:32:18 bobas Exp $
 */

#ifndef USER_H__
#define USER_H__

#include <time.h>

/* preferences registered in the user module */
#define PREFS_USER_LIST_REFRESH		"user/list_refresh"
#define PREFS_USER_KEEP_UNREPLIED	"user/keep_unreplied"

enum user_mode_enum {
	UMODE_FIRST_VALID,
	UMODE_NORMAL = UMODE_FIRST_VALID,
	UMODE_DND,
	UMODE_AWAY,
	UMODE_OFFLINE,
	UMODE_INVISIBLE,
	UMODE_DEAD,
	UMODE_NUM_VALID = UMODE_DEAD,
	UMODE_NULL,
	UMODE_NUM
};

void user_register_module();

gpointer user_by_name(const gchar * name);
#define user_exists(name) (user_by_name(name)!=NULL)
enum user_mode_enum user_mode_of(gpointer);
const gchar * user_name_of(gconstpointer);
gboolean user_is_active(gpointer);
time_t user_last_time_active(gpointer);
gint user_count(void);
GList * user_list(gboolean);

const gchar * user_mode_name(enum user_mode_enum);

#endif /* #ifndef USER_H__ */

