/*
 * user.c: user list management
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
 * $Id: user.c,v 1.20 2004/12/21 15:11:37 bobas Exp $
 */

#include <time.h>

#include <glib.h>

#include "main.h"
#include "prefs.h"
#include "user.h"
#include "util.h"

/** struct defs
 **************/
typedef struct user_info_struct {
	GString * name;
	enum user_mode_enum mode;
	gboolean is_active;	/* if user is active (emm.. watching) */
	time_t last_time_active;/* this can be 0, if the user never changed his 'active' state */
	gboolean replied;	/* if the user has replied to refresh req */
}
user_info_t;
#define PUSER_INFO_T(p) ((user_info_t*)p)

/** static vars
 **************/
static GHashTable * user_hash;
static gint timeout_left, timeout_value;
static guint timeout_source;

/** static routines
 *********************/
static void set_user_mode(user_info_t *, enum user_mode_enum m);
static void set_user_name(user_info_t *, const gchar * new_name);
static void set_user_active(user_info_t * user, gboolean active);

static void
user_hash_destroy_value(user_info_t * user)
{
	/* sad news.. */
	raise_event(EVENT_USER_REMOVED, user, 0);

	/* release the `user_info_t' struct */
	g_string_free(user->name, TRUE);
	g_free(user);
}

/** remove_dead_cb
 *	removes or marks as dead, users, which did not respond to REFRESH_REQ
 *	(for user with g_hash_table_foreach_remove)
 */
static gboolean
remove_dead_cb(char * key, user_info_t * user, gpointer user_data)
{
	gboolean remove;

	if(!user->replied) {
		/* user didn't reply during specified timeout interval
		 */
		set_user_mode(user, UMODE_DEAD);

		/* remove from the list if configured so */
		remove = !prefs_bool(PREFS_USER_KEEP_UNREPLIED);
	} else {
		/* ok, did reply: presume once again as dead
		 */
		user->replied = FALSE;

		/* do not remove */
		remove = FALSE;
	}

	return remove;
}

static void
roll_over_refresh()
{
	/* send user list refresh request */
	raise_event(EVENT_USER_NET_UPDATE_REQ, NULL, 0);

	/* reset timer */
	timeout_left = timeout_value;
}

static gboolean
refresh_timeout_handler(gpointer data)
{
	if(timeout_value) {
		if(--timeout_left==0) {
			/* remove dead users, and reset the rest as `unreplied' */
			g_hash_table_foreach_remove(user_hash, (GHRFunc)remove_dead_cb, NULL);
	
			roll_over_refresh();
		}
	}
	return TRUE;
}

/* add_user:
 *	adds new user entry to user_hash
 * params:
 *	name:		user nickname;
 *	mode:		user's mode;
 */
static gpointer
add_user(const gchar * name, enum user_mode_enum mode)
{
	user_info_t * u;
	char * key;
	gpointer event_v[3];

	g_assert(name);

	/* make user_info_t */
	u = g_malloc(sizeof(user_info_t));
	u->name = g_string_new(name);
	u->mode = mode;
	u->is_active = TRUE;
	u->last_time_active = 0;
	u->replied = TRUE;		/* assume alive */

	/* make key & check that it's the only one in the list */
	key = g_strdup(name);
	g_assert( g_hash_table_lookup(user_hash, key)==NULL );

	/* insert into the hash table */
	g_hash_table_insert(user_hash, key, u);

	/* send event */
	event_v[0] = (gpointer)u;
	event_v[1] = (gpointer)name;
	event_v[2] = GINT_TO_POINTER(u->is_active);
	raise_event(EVENT_USER_NEW, event_v, (int)mode);

	return u;
}

static gboolean
remove_user_hash_func(gchar * key, user_info_t * user, user_info_t * victim)
{
	if(!victim || user==victim) {
		/* set it to dead mode before removing (gui_page.c uses this to mark user dead) */
		set_user_mode(user, UMODE_DEAD);

		raise_event(EVENT_USER_REMOVED, user, 0);
		return TRUE;
	}
	return FALSE;
}

static void
remove_user(gpointer user)
{
	g_hash_table_foreach_remove(user_hash, (GHRFunc)remove_user_hash_func, user);
}

static void
remove_all_users()
{
	g_hash_table_foreach_remove(user_hash, (GHRFunc)remove_user_hash_func, NULL);
}

static gboolean
mark_all_users_dead_hash_func(gchar * key, user_info_t * user, gpointer dummy)
{
	set_user_mode(user, UMODE_DEAD);
	return FALSE;
}

static void
mark_all_users_dead()
{
	g_hash_table_foreach_remove(
		user_hash,
		prefs_bool(PREFS_USER_KEEP_UNREPLIED)
			? (GHRFunc)mark_all_users_dead_hash_func
			: (GHRFunc)remove_user_hash_func,
		NULL
	);
}

static void
update_user_last_time_active(user_info_t * user)
{
	user->last_time_active = time(NULL);
}

static void
set_user_mode(user_info_t * user, enum user_mode_enum new_mode)
{
	if(user->mode != new_mode) {
		enum user_mode_enum prev_mode = user->mode;
		user->mode = new_mode;

		raise_event(EVENT_USER_MODE_CHANGE, user, prev_mode);
	}
}

static void
set_user_active(user_info_t * user, gboolean active)
{
	if(user->is_active != active) {
		/* set new mode */
		user->is_active = active;
		raise_event(EVENT_USER_ACTIVE_CHANGE, (gpointer)user, active);
	}
}

static void
set_user_name(user_info_t * user, const gchar * new_name)
{
	gpointer user_info;
	gpointer key, orig_key, new_key;
	gchar * previous_nick;
	gpointer event[2]; 

	g_assert(user && new_name);

	if(nickname_cmp(user->name->str, new_name)) {
		/* check for nickname collision */
		new_key = g_strdup(new_name);
		if(g_hash_table_lookup(user_hash, new_key)!=NULL) {
			/* nickname collision: remove this user */
			g_free(new_key);
			remove_user(user);
			return;
		}
	
		/* search for original key & struct ptrs */
		key = g_strdup(user->name->str);
		g_hash_table_lookup_extended(user_hash, key, &orig_key, &user_info);

		/* steal key & value from hash table */
		g_hash_table_steal(user_hash, key);

		/* free search & original key */
		g_free(key);
		g_free(orig_key);

		/* insert with the new key */
		g_hash_table_insert(user_hash, new_key, user_info);

		/* set name on struct */
		previous_nick = g_strdup(PUSER_INFO_T(user_info)->name->str);
		g_string_assign(PUSER_INFO_T(user_info)->name, new_name);
	
		/* send update */
		event[0] = user;
		event[1] = previous_nick;
		raise_event(EVENT_USER_RENAME, event, 0);
		g_free(previous_nick);
	}
}

static void
user_prefs_user_list_refresh_changed_cb(const gchar * prefs_name)
{
	/* update refresh timeout value
	 */
	timeout_value = prefs_int(PREFS_USER_LIST_REFRESH);
	roll_over_refresh();
}

static void
user_event_cb(
	enum app_event_enum app,
	gpointer p, gint i)
{
	user_info_t * user;

	switch(app) {
	case EVENT_MAIN_INIT:
		/* build user list hash table */
		user_hash = g_hash_table_new_full(
			(GHashFunc)g_str_hash,
			(GEqualFunc)g_str_equal,
			(GDestroyNotify)g_free,
			(GDestroyNotify)user_hash_destroy_value);

		/* setup & disable timeout handler */
		timeout_source = g_timeout_add(1000, refresh_timeout_handler, 0);
		timeout_value = 0;
		break;

	case EVENT_MAIN_REGISTER_PREFS:
		/* register configuration switches and set them to default values */
		prefs_register(PREFS_USER_LIST_REFRESH, PREFS_TYPE_UINT,
			_("User list update period in seconds"), NULL, NULL);
		prefs_register(PREFS_USER_KEEP_UNREPLIED, PREFS_TYPE_BOOL,
			_("Keep unreplied users on the list"), NULL, NULL);
		
		prefs_add_notifier(PREFS_USER_LIST_REFRESH,
			(GHookFunc)user_prefs_user_list_refresh_changed_cb);
		break;

	case EVENT_MAIN_PRESET_PREFS:
		prefs_set(PREFS_USER_LIST_REFRESH, 15);	/* update user list each 15 secs */
		break;

	case EVENT_MAIN_CLOSE:
		/* destroy hash table (this will also destroy user_info_t's) */
		g_hash_table_destroy(user_hash);

		/* remove timeout event source */
		g_source_remove(timeout_source);
		timeout_source = 0;
		break;

	case EVENT_IFACE_USER_REMOVE_REQ:
		remove_user(p);
		break;

	case EVENT_IFACE_USER_LIST_REFRESH_REQ:
		mark_all_users_dead();
		roll_over_refresh();
		break;

	case EVENT_NET_CONNECTED:
		/* enable timeout handler & request user list update */
		timeout_value = prefs_int(PREFS_USER_LIST_REFRESH);
		roll_over_refresh();
		break;
	
	case EVENT_NET_DISCONNECTED:
		timeout_value = 0;	/* disable timeout handler */
		remove_all_users();
		break;

	case EVENT_NET_NEW_USER:
		user = add_user(EVENT_V(p, 0), (enum user_mode_enum)EVENT_V(p, 1));
		if(!EVENT_V(p, 2))
			update_user_last_time_active(user);
		break;

	case EVENT_NET_MSG_CHANNEL_LEAVE:
		if(!channel_cmp("Main", EVENT_V(p, 1))) {
			/* user has left the net: remove it from the list */
			if(EVENT_V(p, 0))
				remove_user(EVENT_V(p, 0));
		}
		break;

	case EVENT_NET_MSG_CHANNEL_TEXT:
	case EVENT_NET_MSG_PRIVATE_TEXT:
		update_user_last_time_active(EVENT_V(p, 0));
		break;

	case EVENT_NET_MSG_REFRESH_ACK:
		/* mark as alive and change/maintain user mode */
		user = PUSER_INFO_T(EVENT_V(p, 0));
		user->replied = TRUE;
		set_user_mode(user, (enum user_mode_enum)i);
		set_user_active(user, GPOINTER_TO_INT(EVENT_V(p, 1)));
		break;

	case EVENT_NET_MSG_RENAME:
		update_user_last_time_active(PUSER_INFO_T(EVENT_V(p, 0)));
		set_user_name(PUSER_INFO_T(EVENT_V(p, 0)), (const gchar*)EVENT_V(p, 1));
		break;

	case EVENT_NET_MSG_MODE_CHANGE:
		update_user_last_time_active(PUSER_INFO_T(p));
		set_user_mode(PUSER_INFO_T(p), (enum user_mode_enum)i);
		break;

	case EVENT_NET_MSG_ACTIVE_CHANGE:
		update_user_last_time_active(PUSER_INFO_T(p));
		set_user_active(PUSER_INFO_T(p), i);
		break;

	default:
		break;
	}
}

/** exported routines
 *********************/

void user_register_module()
{
	register_event_cb(user_event_cb, EVENT_MAIN | EVENT_NET | EVENT_IFACE);
}

gint user_count(void)
{
	g_assert(user_hash);

	return g_hash_table_size(user_hash);
}

gint user_list_foreach_sorted_func(gconstpointer user1, gconstpointer user2)
{
	return util_utf8_strcasecmp(user_name_of(user1), user_name_of(user2));
}

void user_list_foreach_func(gpointer key, gpointer user_ptr, gpointer * list_data)
{
	if(!list_data[1] || !list_data[0]) {
		/* do not sort the list (or don't care before inserting if it is empty) */
		list_data[0] = g_list_prepend(list_data[0], user_ptr);
	} else {
		/* find place to insert */
		list_data[0] = (gpointer)g_list_insert_sorted(
			(GList*)list_data[0],
			user_ptr,
			user_list_foreach_sorted_func);
	}
}

GList * user_list(gboolean sorted)
{
	gpointer list_data[2] = { NULL, GINT_TO_POINTER(sorted) };

	g_hash_table_foreach(user_hash, (GHFunc)user_list_foreach_func, (gpointer)list_data);

	return list_data[0];
}

gpointer user_by_name(const gchar * name)
{
	g_assert(name);
	return g_hash_table_lookup(user_hash, name);
}

enum user_mode_enum
user_mode_of(gpointer u)
{
	g_assert(u);
	return PUSER_INFO_T(u)->mode;
}

const gchar *
user_name_of(gconstpointer u)
{
	g_assert(u);
	return PUSER_INFO_T(u)->name->str;
}

time_t
user_last_time_active(gpointer user)
{
	g_assert(user);
	return PUSER_INFO_T(user)->last_time_active;
}

gboolean user_is_active(gpointer u)
{
	g_assert(u);
	return PUSER_INFO_T(u)->is_active;
}

const gchar *
user_mode_name(enum user_mode_enum local_mode)
{
	const gchar * name;

	switch(local_mode) {
	case UMODE_NORMAL:	name = _("Normal");	break;
	case UMODE_DND:		name = _("DND");	break;
	case UMODE_AWAY:	name = _("Away");	break;
	case UMODE_OFFLINE:	name = _("Offline");	break;
	case UMODE_DEAD:	name = _("*Dead*");	break;
	case UMODE_INVISIBLE:	name = _("Invisible");	break;
	default:
		name = _("(undefined)");
		break;
	}
	return name;
}


