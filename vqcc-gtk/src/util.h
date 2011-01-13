/*
 * util.h: misc utility routines
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
 * $Id: util.h,v 1.5 2004/12/24 03:16:04 bobas Exp $
 */

#ifndef UTIL_H__
#define UTIL_H__

/* list functions
 */
void util_list_free_with_data(GList *, GDestroyNotify);

typedef gpointer util_list_data_copy_func_t(gpointer);
GList * util_list_copy_with_data(GList *, util_list_data_copy_func_t *);

/* string funcs
 */
gint util_utf8_strcasecmp(const gchar *, const gchar *);
gboolean util_parse_ipv4(const gchar *, guint32 *);
char * util_inet_ntoa(guint32);

/* time funcs
 */
gchar * util_time_stamp();

/* session funcs
 */
gpointer util_session_for_notifies_from(gpointer user);

#endif	/* #ifndef UTIL_H__ */

