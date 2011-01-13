/*
 * util.c: misc utility routines
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
 * $Id: util.c,v 1.6 2004/12/24 03:16:04 bobas Exp $
 */

#include <time.h>
#include <glib.h>
#include <stdio.h>

#include "user.h"
#include "sess.h"
#include "util.h"

void util_list_free_with_data(GList * list, GDestroyNotify destroy_notify_cb)
{
	while(list) {
		destroy_notify_cb(list->data);
		list = g_list_delete_link(list, list);
	}
}

GList * util_list_copy_with_data(GList * list, util_list_data_copy_func_t * data_copy_cb)
{
	GList * copy = NULL;

	for(; list; list = list->next)
		copy = g_list_append(copy, data_copy_cb(list->data));

	return copy;
}

gint util_utf8_strcasecmp(const gchar * str1, const gchar * str2)
{
	gchar * folded1 = g_utf8_casefold(str1, -1),
		* folded2 = g_utf8_casefold(str2, -1);
	gint result;

	result = g_utf8_collate(folded1, folded2);

	g_free(folded1);
	g_free(folded2);

	return result;
}

gboolean
util_parse_ipv4(const gchar * text, guint32 * p_address)
{
	unsigned int a1, a2, a3, a4;
	if(sscanf(text, "%u.%u.%u.%u", &a1, &a2, &a3, &a4)!=4)
		return FALSE;

	if((a1 | a2 | a3 | a4) & ~0xff)
		return FALSE;

	*p_address = (a1 << 24) | (a2 << 16) | (a3 << 8) | a4;
	return TRUE;
}

char *
util_inet_ntoa(guint32 ip)
{
	return g_strdup_printf(
		"%u.%u.%u.%u",
		(ip >> 24) & 255,
		(ip >> 16) & 255,
		(ip >> 8) & 255,
		ip & 255
	);
}

gchar *
util_time_stamp()
{
	gchar * stamp = g_malloc(128);
	time_t current_time = time(NULL);
	struct tm * curr_tm;

#ifndef _WIN32
	curr_tm = g_malloc(sizeof(struct tm));
	gmtime_r(&current_time, curr_tm);
#else
	curr_tm = gmtime(&current_time);
#endif
	
	strftime(stamp, 128, "%X, %Y-%m-%d", curr_tm);
#ifndef _WIN32
	g_free(curr_tm);
#endif

	return stamp;
}

gpointer
util_session_for_notifies_from(gpointer user)
{
	gpointer session;

	g_assert(user);
	
	session = sess_find(SESSTYPE_PRIVATE, user_name_of(user));
	if(!session)
		session = sess_find(SESSTYPE_STATUS, NULL);

	return session;
}

