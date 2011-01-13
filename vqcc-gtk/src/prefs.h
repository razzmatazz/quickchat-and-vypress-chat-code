/*
 * prefs.h: configuration management routines
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
 * $Id: prefs.h,v 1.21 2004/12/29 15:58:25 bobas Exp $
 */

#ifndef PREFS_H__
#define PREFS_H__

/* preferences registered in the `prefs' module */
#define PREFS_PREFS_AUTO_SAVE	"prefs/auto_save"

enum prefs_type {
	PREFS_TYPE_BOOL,
	PREFS_TYPE_UINT,
	PREFS_TYPE_STR,
	PREFS_TYPE_LIST,
};

void prefs_register_module(gint, gchar **, gchar **);

gboolean prefs_in_sync();

typedef gboolean prefs_validator_func(const gchar *, gpointer);
void 	prefs_register(
		const gchar *, enum prefs_type, const gchar *,
		prefs_validator_func *, gpointer);
void	prefs_add_notifier(const gchar *, GHookFunc);

const gchar *	prefs_description(const gchar *);

void	prefs_set(const gchar *, ...);

guint		prefs_int(const gchar *);
gboolean	prefs_bool(const gchar *);
const gchar *	prefs_str(const gchar *);
GList *		prefs_list(const gchar *);
void		prefs_list_add(const gchar *, const gchar *);
void		prefs_list_add_unique(const gchar *, const gchar *);
gboolean	prefs_list_remove(const gchar *, const gchar *);
void		prefs_list_clear(const gchar *);
gboolean	prefs_list_contains(const gchar *, const gchar *);

#endif /* #ifndef PREFS_H__ */

