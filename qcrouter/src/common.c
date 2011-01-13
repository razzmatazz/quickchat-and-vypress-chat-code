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
 *		common.c
 *			implements common structures & types
 *
 *	(c) Saulius Menkevicius 2002,2003
 */

#define COMMON_C__

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "common.h"
#include "usercache.h"

/** private struct defs
 */
struct timer_def {
	int timer, left;	/* secs till next shot */
	int shot_count;
	int one_shot;
	void * user_data;

	void (*proc)(timer_id, int, void *);

	struct timer_def * next, * prev;
};

/** static vars
 */
static char log_str[256]="";
static char user_str[256];
static char net_str[256];

static struct timer_def
		* tm_first, * tm_last;
static void * alrm_previous;

/** exported vars
 */
volatile int timer_ignited;

/** forward references
 */
static struct timer_def * tml_append();
static void tml_delete(struct timer_def *);
static void timer_alrm_proc(int sigalrm);

/** const net/user_id's
 */
static user_id	null_user;
static net_id	local_net, broadcast_net, null_net, last_id;

const user_id * null_user_id() { return &null_user; }
const net_id * local_net_id() { return &local_net; }
const net_id * broadcast_net_id() { return &broadcast_net; }
const net_id * null_net_id() { return &null_net; }

/** common_init:
 *	initializes static module vars
 */
void common_init()
{
	/** setup null net id */
	null_net = 0;

	/** setup broadcast net id */
	broadcast_net = 0xffff;

	/** setup null user */
	null_user.num = 0;
	null_user.net = null_net;

	srand(time(NULL));

	/* setup timer */
	timer_ignited = 0;
	tm_first = tm_last = NULL;

	alrm_previous = (void*)signal(SIGALRM, timer_alrm_proc);

	/* start ticking.. */
	alarm(1);
}

/** common_free:
 *	frees any allocated vars
 */
void common_free()
{
	struct timer_def * tm, * dead;

	alarm(0);

	/* free any timer structs that are setup */
	tm = tm_first;
	while(tm) {
		dead = tm;
		tm = tm->next;
		xfree(dead);
	}
	tm_first = tm_last = NULL;
	
	/* restore signal handler */
	signal(SIGALRM, (void(*)(int))alrm_previous);
}

/** common_post_fork:
 *	restarts alarm() after daemonizing
 */
void common_post_fork()
{
	alarm(1);
}

/** common_set_local_id:
 * 	sets local id (once it becomes available
 * 	after config parse)
 */
void common_set_local_id(net_id nid)
{
	local_net = nid;

	last_id = nid;
}

net_id common_next_id()
{
	return ++last_id;
}

/** panic_real
 * 	prints msg and abort()s
 */
void panic_real(const char * where, const char * msg)
{
	if(where && msg) {
		log_a("panic at ");
		log_a(where);
		log_a(": ");
		log(msg);
	}

	abort();
}

/** xalloc
 * 	does the job of malloc with ENOMEM handling
 */
void * xalloc(size_t sz)
{
	void * blk = malloc(sz);
	if(blk==NULL) {
		panic("out of mem");
	}
	return blk;
}

/** xfree
 *	free's the memory at the specified block;
 *	handles NULL case with panic
 */
void xfree(void * blk)
{
	if(blk==NULL) {
		panic("freeing NULL block");
	}
	free(blk);
}

/** xrealloc
 * 	reallocs memory space
 * 	(works as xalloc if blk==NULL)
 */
void * xrealloc(void * blk, size_t sz)
{
	if(blk==NULL) {
		return xalloc(sz);
	}

	blk = realloc(blk, sz);
	if(blk==NULL) {
		panic("out of mem");
	}
	return blk;
}

/** net_id_dump:
 * 	returns char * to str identifying the net
 */
const char * net_id_dump(const net_id * nid)
{
	if(!*nid) {
		strcpy(net_str, "null_net");
	}
	sprintf(net_str, "net_%hu", *nid);

	return net_str;
}

/** user_id_dump:
 * 	returns char * to str identifying the user
 */
const char * user_id_dump(const user_id * uid)
{
	if(uid->net==*null_net_id())
	{
		strcpy(user_str, "null_user");
	} else {
		sprintf(user_str, "num_%d@%s",
			uid->num, net_id_dump(&uid->net)
		);
	}
	return user_str;
}

/** log*
 * 	prepares and prints out log msg
 *
 * 	note, that the limit of log string is 256
 * 	and NO CHECKING IS DONE to avoid overflows !!
 */
void log_a(const char * msg)
{
	assert(msg);
	strcat(log_str, msg);
}

void log(const char * msg)
{
	assert(msg);

	strcat(log_str, msg);
	strcat(log_str, "\n");

	fputs(log_str, stderr);

	/* empty place for next msg */
	log_str[0]='\0';
}

/** eq_nickname:
 * 	compares 2 nicknames
 * 		(1 if equal, zero otherwise)
 */
int eq_nickname(
	const char * nick1, const char * nick2)
{
	assert(nick1 && nick2);

	return !strncmp(nick1, nick2, NICKNAME_LEN_MAX);
}

/** eq_user_id:
 * 	compares 2 user_id's
 * 		(1 if equal, zero otherwise)
 */
int eq_user_id(
	const user_id * uid1,
	const user_id * uid2 )
{
	assert(uid1 && uid2);

	return uid1->num==uid2->num
		&& uid1->net==uid2->net;
}

/** is_valid_channel
 *	returns if the string is a valid channel name
 */
int is_valid_channel(const chanlist_t * channel)
{
	/* channel name must not contain a '#' character
	 */
	return strchr(channel, '#')==0;
}

/** is_valid_nickname
 *	returns if string is valid to be network nickname
 */
int is_valid_nickname(const nickname_t * nickname)
{
	return nickname && strlen(nickname);
}

/* is_valid_chanlist
 *	return if string is valid chanlist
 *	("#chan1#chan2....#lastchan")
 */
int is_valid_chanlist(const chanlist_t * chanlist)
{
	/* XXX: do more exhaustive checking */

	return chanlist && strlen(chanlist) && *chanlist=='#';
}

/** timer_alrm_proc
 *	handler SIGALRM
 */
static void timer_alrm_proc(int sigalrm)
{
	timer_ignited = 1;
}

/** timer_process
 *	invokes timer handler callbacks once their timer
 *	have finished tickin'
 */
void timer_process()
{
	struct timer_def * tm, * tm_dead;

	/* we'are are processing the signal already */
	timer_ignited = 0;

	/* reset alarm */
	alarm(1);

	/* process each of the timers */
	tm = tm_first;
	while(tm) {
		tm->left--;
		if(tm->left) {	/* still not the right time */
			tm = tm->next;
			continue;
		}

		/* invoke timer handler */
		tm->proc((timer_id)tm, tm->shot_count, tm->user_data);

		/* reset or remove the timer */
		if(tm->one_shot) {
			/* skip to next */
			tm_dead = tm;
			tm = tm->next;

			/* delete from the list */
			tml_delete(tm);
		} else {
			/* not single shot timer */
			tm->left = tm->timer;
			++ tm->shot_count;

			/* go to next one */
			tm = tm->next;
		}
	}
}

/** timer_start:
 *	setups timer
 */
timer_id timer_start(
	int seconds,
	int one_shot,
	void (*tm_proc)(timer_id, int, void *),
	void * user_data)
{
	struct timer_def * tm;

	assert(seconds>=1 && tm_proc);

	debug("timer_start: new timer");

	/* insert new timer */
	tm = tml_append();

	/* setup timer */
	tm->timer = seconds;
	tm->left = seconds;
	tm->proc = tm_proc;
	tm->one_shot = one_shot;
	tm->shot_count = 0;
	tm->user_data = user_data;

	return (timer_id)tm;
}

/** timer_stop:
 *	destroys timer
 */
void timer_stop(timer_id tm)
{
	assert(tm && tm_first && tm_last);
	tml_delete((struct timer_def *)tm);
}

/** common_umode_by_name
 *	return net_umode enumeration value
 * mapping of specified mode name
 */
enum net_umode
common_umode_by_name(
	const char * mode_name)
{
	if(!strcasecmp(mode_name, "normal")) {
		return UMODE_NORMAL;
	} else if(!strcasecmp(mode_name, "away")) {
		return UMODE_AWAY;
	} else if(!strcasecmp(mode_name, "dnd")) {
		return UMODE_DND;
	} else if(!strcasecmp(mode_name, "offline")) {
		return UMODE_OFFLINE;
	}
	return UMODE_INVALID;
}

/** private routines:
 **************************************************/

/* timer list mngment */
static struct timer_def *
	tml_append()
{
	struct timer_def * tm;

	assert((tm_first && tm_last)
		|| (!tm_first && !tm_last));

	/* alloc & setup new timer_def */
	tm = xalloc(sizeof(struct timer_def));
	tm->next = NULL;

	/* insert into the list */
	if(!tm_first) {
		/* empty list */
		tm->prev = NULL;
		tm_first = tm_last = tm;
	} else {
		/* non-empty - append on the end */
		tm->prev = tm_last;
		tm_last->next = tm;
		tm_last = tm;
	}

	return tm;
}

static void tml_delete(
	struct timer_def *tm)
{
	assert(tm && tm_first && tm_last);

	/* remove from the list */
	if(tm_first==tm_last) {
		/* single-entry list */
		tm_first = tm_last = NULL;
	}
	else if(tm_first==tm) {
		/* we're at the beginning */
		tm->next->prev = NULL;
		tm_first = tm->next;
	}
	else if(tm_last==tm) {
		/* we're at the end */
		tm->prev->next = NULL;
		tm_last = tm->prev;
	} else {
		/* we're at the middle */
		tm->prev->next = tm->next;
		tm->next->prev = tm->prev;
	}

	/* free the struct */
	xfree(tm);
}

