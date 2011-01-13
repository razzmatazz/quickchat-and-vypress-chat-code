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
 *		common.h
 *			defines common structures & types
 *
 *	(c) Saulius Menkevicius 2002,2003
 */

#ifndef COMMON_H__
#define COMMON_H__

#define APP_NAME		"qcRouter"
#define APP_VERSION_STRING	"v0.1-pre0"
#define APP_VER_MAJOR		0
#define APP_VER_MINOR		1
#define APP_VER_PATCH_LEVEL	0

#define LICENCE	("\n"APP_NAME" links several QuickChat & Vypress chat" \
			" nets through internet link\n" \
 " Copyright (C) 2002,2003 Saulius Menkevicius et al\n" \
 "This program is free software; you can redistribute it and/or modify\n"\
 "it under the terms of the GNU General Public License as published by\n"\
 "the Free Software Foundation; either version 2 of the License, or\n"\
 "(at your option) any later version.\n\n"\
 "This program is distributed in the hope that it will be useful,\n"\
 "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"\
 "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"\
 "GNU General Public License for more details.\n\n"\
 "You should have received a copy of the GNU General Public License\n"\
 "along with this program; if not, write to the Free Software\n"\
 "Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA\n")

#define DEFAULT_TOPIC		"[null topic]"

#define CHANNAME_LEN_MAX	31
#define NICKNAME_LEN_MAX	31
#define TOPIC_LEN_MAX		511
#define CHANLIST_LEN_MAX	511

enum net_umode {
	UMODE_INVALID	= 0,

	UMODE_NORMAL	= 2,
	UMODE_DND	= 4,
	UMODE_AWAY	= 6,
	UMODE_OFFLINE	= 8
};

typedef char nickname_t[NICKNAME_LEN_MAX+1];
typedef char chanlist_t[CHANLIST_LEN_MAX+1];
typedef char topic_t[TOPIC_LEN_MAX+1];

/** net_id_struct:
 * 	identifies net in global network
 *
 * NOTES:
 * 	if (ip==0 && port==0) - it's null_net_id
 * 	if (port<1024) - it's local net (qchat/vypress chat)
 * 	else - far (router) net
 */
typedef unsigned short net_id;

typedef struct user_id_struct {
	net_id net;
	unsigned short num;
} user_id;

/** constant ids
 */
const user_id * null_user_id();
const net_id * local_net_id();
const net_id * broadcast_net_id();
const net_id * null_net_id();

/** id & string routines
 */
int is_valid_nickname(const nickname_t *);
int is_valid_channel(const chanlist_t *);
int is_valid_chanlist(const chanlist_t *);
 
int eq_user_id(const user_id *, const user_id *);
int eq_nickname(const char *, const char *);
int eq_channel(const char *, const char *);
#define eq_net_id(nid1, nid2) (*nid1==*nid2)

#define is_local_net(net) eq_net_id(&(net), local_net_id())
#define is_broadcast_net(net) eq_net_id(&(net), broadcast_net_id())
#define is_null_net(net) eq_net_id(&(net), null_net_id())
#define is_null_user(ui) is_null_net((ui).net)

/** info build routines:
 * 	for logging/output
 */
const char * net_id_dump(const net_id *);
const char * user_id_dump(const user_id *);

/** debug msg facilities */
#ifdef NDBEUG
 #define debug_a(s)
 #define debug(s)
#else
 #define debug_a(s) log_a(s)
 #define debug(s) log(s)
#endif

void log_a(const char*);
void log(const char *);
void panic_real(const char *, const char *);
#define panic(s) panic_real(__FILE__ "::" __FUNCTION__, (s))

/* mem alloc primitives
 *	handles out of mem & al situations:
 *	does abort() on ENOMEM 
 */
void * xalloc(size_t);
void xfree(void *);
void * xrealloc(void *, size_t);

void common_init();
void common_free();
void common_post_fork();

/** network id funcs
 */
void common_set_local_id(net_id);
net_id common_next_id();

/** timer facilities
 *	implements second-granularity timer
 *	(using alarm() on UNIX)
 */
#ifndef COMMON_C__
extern volatile int timer_ignited;
#endif

typedef void *timer_id;
timer_id timer_start(
		int seconds, int one_shot,
		void (*proc)(timer_id, int, void *),
		void * user_data);
void timer_stop(timer_id);
void timer_process();

/** miscelaneous functions
 */
enum net_umode common_umode_by_name(const char * name);

#endif	/* #ifndef COMMON_H__ */

