/*
 * sess.h: session management
 * Copyright (C) 2002-2003 Saulius Menkevicius
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
 * $Id: sess.h,v 1.9 2004/12/24 02:56:16 bobas Exp $
 */

#ifndef SESS_H__
#define SESS_H__

/* preferences registered in the `sess' module */
#define PREFS_SESS_STAMP_CHANNEL	"sess/stamp_channel"
#define PREFS_SESS_STAMP_PRIVATE	"sess/stamp_private"
#define PREFS_SESS_STAMP_STATUS		"sess/stamp_status"
#define PREFS_SESS_IMPORTANT_CHANNELS	"sess/important_channels"
#define PREFS_SESS_SUPPRESS_RENAME_TEXT	"sess/suppress_rename_text"
#define PREFS_SESS_SUPPRESS_MODE_TEXT	"sess/suppress_mode_text"
#define PREFS_SESS_SUPPRESS_JOIN_LEAVE_TEXT "sess/suppress_join_leave_text"

enum session_text_type {
	SESSTEXT_NORMAL,
	SESSTEXT_NOTIFY,
	SESSTEXT_ERROR,
	SESSTEXT_JOIN,
	SESSTEXT_LEAVE,
	SESSTEXT_MY_TEXT,
	SESSTEXT_MY_ME,
	SESSTEXT_THEIR_TEXT,
	SESSTEXT_THEIR_ME,
	SESSTEXT_TOPIC
};

enum session_type {
	SESSTYPE_STATUS,
	SESSTYPE_CHANNEL,
	SESSTYPE_PRIVATE
};

void sess_register();

typedef gpointer sess_id;
typedef void (*sess_enum_cb)(gpointer, enum session_type, const gchar *, gpointer, gpointer);

gpointer sess_new(
	enum session_type type, const gchar * name,
	gboolean closeable, gpointer user_id);

void sess_delete(gpointer);
gpointer sess_find(enum session_type type, const gchar * name);
void sess_write(gpointer, enum session_text_type, const gchar * fmt, ...);
gpointer sess_current();
enum session_type sess_type(gpointer);
const gchar * sess_name(gpointer);
const gchar * sess_topic(gpointer);
gboolean sess_is_closeable(gpointer);
gboolean sess_topic_readonly(gpointer);
void sess_switch_to(gpointer);
void sess_enumerate(sess_enum_cb, gpointer);
gchar * sess_make_channel_list();

#endif /* #ifndef SESS_H__ */

