/**
 * qcRouter links several QuickChat & VypressChat nets through internet
 *
 *   This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*	qcrouter project
 *		usercache.h
 *			maintains network user list
 *
 *	(c) Saulius Menkevicius 2002,2003
 */

#ifndef USERCACHE_H__
#define USERCACHE_H__

void usercache_init();
void usercache_exit();

/* user chache management */
const char *	usercache_topic(const char *);
void 	usercache_set_topic(const char *, const char *);
int 	usercache_same_topic(const char *, const char *);

void usercache_known_channels(chanlist_t *);

void usercache_add(
	const user_id *, const enum net_umode *,
	const char *, const char *);
void usercache_remove(const user_id *);

enum net_umode usercache_umode_of(const user_id *);
const char * usercache_chanlist_of(const user_id *);
const char * usercache_nickname_of(const user_id *);
user_id usercache_uid_of(const char *);
int usercache_known(const char *);
int usercache_exists(const user_id *);

void usercache_remove_net(const net_id *);
void usercache_join_chan(const user_id *, const char *);
void usercache_part_chan(const user_id *, const char *);

typedef void (*usercache_enum_proc_t)(
		void *, const user_id *, enum net_umode,
		const char *, const char *);

void usercache_enum(usercache_enum_proc_t, void *);
void usercache_enum_net(const net_id *, usercache_enum_proc_t, void *);
void usercache_enum_except_net(const net_id *, usercache_enum_proc_t, void*);

void usercache_tag_dead_from(net_id);
void usercache_tag_alive(user_id);
user_id * usercache_dead_users_from(net_id, unsigned * p_count);

#endif	/* #ifndef USERCACHE_H__ */

