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
 *		usercache.c
 *			implements cache of known users
 *
 *	(c) Saulius Menkevicius 2002,2003
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "usercache.h"

/** ucache_entry
 * 	defines user entry
 */
struct ucache_channel;

struct ucache_user
{
	user_id		uid;
	enum net_umode	umode;
	nickname_t	nickname;
	int 		alive :1;	/* `not dead' flag */

	unsigned int 		chan_count;
	struct ucache_channel	** chan;

	struct ucache_user	* next, * prev;
};

#define foreach_ue(ue) for(ue=ue_first;ue;ue=ue->next)

/** ucache_channel
 * 	defines channel entry 
 */
struct ucache_channel
{
	topic_t	topic;
	char 	name[CHANNAME_LEN_MAX+1];
	unsigned ref_count;

	struct ucache_channel	* next, * prev;
};

/** static vars
 ********************************/
static nickname_t	req_nickname;
static topic_t		req_topic;
static chanlist_t 	req_chanlist;

static struct ucache_user
			* ue_first, * ue_last;
static unsigned int	ue_count;

static struct ucache_channel
			* chan_first, * chan_last;
static unsigned int	chan_count;

/** static routines
 ********************************/

static struct ucache_channel *
	chan_find(
		const char *channame)
{
	struct ucache_channel * ce;
	
	for(ce = chan_first; ce; ce = ce->next) {
		if(!strncmp(ce->name, channame, CHANNAME_LEN_MAX))
			return ce;
	}
	return NULL;
}
#define chan_is_known(chname) (chan_find(chname)!=NULL)

static struct ucache_channel *
	chan_add_ref(
		const char *channame)
{
	struct ucache_channel * ce;

	ce = chan_find(channame);
	if(ce) {
		/* we have this channel already: increase reference count */
		ce->ref_count++;
		return ce;
	}

	/* create new channel entry */
	ce = xalloc(sizeof(struct ucache_channel));
	strncpy(ce->name, channame, CHANNAME_LEN_MAX);
	strcpy(ce->topic, "");
	ce->ref_count = 1;
	ce->next = NULL;

	/* insert into the list */
	if(!chan_count) {
		ce->prev = NULL;

		chan_first = chan_last = ce;
	} else {
		ce->prev = chan_last;
		chan_last->next = ce;

		chan_last = ce;
	}
	chan_count++;

	return ce;
}

static void chan_delete_ref(
	struct ucache_channel *ce)
{
	assert(ce);

	/* ok, chan entry found:
	 * 	decrease ref_count and, if ref_count==0, delete the entry
	 */
	ce->ref_count --;
	if(! ce->ref_count) {
		/* delete channel entry */

		if(chan_count==1) {
			chan_first = chan_last = NULL;
			chan_count = 0;
		} else {
			if(ce==chan_first) {
				chan_first = chan_first->next;
				chan_first->prev = NULL;
			}
			else if(ce==chan_last) {
				chan_last = chan_last->prev;
				chan_last->next = NULL;
			}
			else {
				ce->prev->next = ce->next;
				ce->next->prev = ce->prev;
			}
			chan_count --;
		}

		xfree((void*)ce);
	}
}

static struct ucache_user *
	user_by_id(const user_id * p_uid)
{
	struct ucache_user * p_ue;

	assert(p_uid);

	for(p_ue = ue_first; p_ue; p_ue = p_ue->next) {
		if(eq_user_id(&p_ue->uid, p_uid)) {
			break;
		}
	}
	return p_ue;
}

/** add_ue_channel:
 * 	adds specified channel
 * 	to user's channel list
 */
static void add_ue_channel(
		struct ucache_user * ue,
		const char * channame)
{
	struct ucache_channel * uc, **users_uc;
	int left;

	/** find if we have this channel already in user's chlist
	 */
	uc = chan_find(channame);
	if(uc) {
		left = ue->chan_count;
		for(users_uc = ue->chan; left--; users_uc ++) {
			if(*users_uc == uc) break;
		}
		if(left>=0) {
			log_a("add_ue_channel: the channel \"");
			log_a(channame);
			log_a("\" already in user's \"");
			log_a(user_id_dump(&ue->uid));
			log("\" channel list");
			return;
		}
	}

	/** ok, no such channel found:
	 * 	register if the channel has not been registered
	 * 	and increase reference count	*/
	uc = chan_add_ref(channame);

	/* add ptr to **chan list
	 * 	(allocation granularity of 16) */
	if((ue->chan_count%16)==0) {
		ue->chan = xrealloc(
			(void*)ue->chan,
			sizeof(void*) * (ue->chan_count + 16));
	}

	ue->chan[ ue->chan_count ] = uc;
	ue->chan_count ++;
}

static void remove_ue_channel(
		struct ucache_user * ue,
		const char * channame)
{
	struct ucache_channel * uc, ** p_uc;
	unsigned left;

	uc = chan_find(channame);
	if(!uc) {
		log_a("remove_ue_channel: tried to remove unknown channel \"");
		log_a(channame);
		log_a("\" for user \"");
		log_a(user_id_dump(&ue->uid));
		log("\": ignoring");
		return;
	}

	/* found it: now search user's channel list */
	left = ue->chan_count;
	for(p_uc = ue->chan; left--; p_uc ++) {
		if(*p_uc == uc) break;
	}
	if(left<0) {
		/* no such channel found in user's list */
		log_a("remove_ue_channel: tried to remove channel not in "
			"user's \"");
		log_a(user_id_dump(&ue->uid));
		log("\" channel list: bailing out");
		return;
	}

	/* ok, channel found:
	 * 	remove it's reference */
	chan_delete_ref(uc);

	/* .. and itself from the it's list */
	-- ue->chan_count;
	if(!ue->chan_count) {
		xfree((void*)ue->chan);
		ue->chan = NULL;
	} else {
		memcpy(p_uc, p_uc+1, sizeof(void*)*left);

		if((ue->chan_count % 16)==0) {
			if(!ue->chan_count) {
				xfree(ue->chan);
				ue->chan = NULL;
			} else {
				ue->chan = xrealloc(
					(void*)ue->chan,
					ue->chan_count);
			}
		}
	}
}

static void remove_ue_all_channels(
		struct ucache_user * ue )
{
	struct ucache_channel ** list, ** p_uc;
	unsigned list_sz;

	assert(ue);

	list_sz = ue->chan_count;
	if(!list_sz ) {
		/* no channels */
		return;
	}

	/* make copy of the list */
	list = xalloc(sizeof(void*)*list_sz);
	memcpy((void*)list, ue->chan, sizeof(void*)*list_sz);

	/* remove reference for each of the channels */
	for(p_uc = list; list_sz--; p_uc ++) {
		remove_ue_channel(ue, (*p_uc)->name);
	}

	/* cleanup */
	xfree((void*)list);
}

static void set_ue_channels(
	struct ucache_user * ue,
	const char * chanlist)
{
	char channame[CHANNAME_LEN_MAX+1];
	const char * p, * next;
	unsigned name_sz;

	p = chanlist;

	/* check integrity */
	if(*p!='\0' && *p!='#') {
		log_a("set_ue_channels: invalid chanlist (");
		log_a(chanlist); log(")");
		return;
	}

	/* reset user's chanlist */
	remove_ue_all_channels(ue);

	/* scan throughout */
	while(p) {
		/* find beginning of next channame */
		next = strchr(p+1, '#');

		name_sz = next ? (next - p - 1): strlen(p+1);

		/* copy this one into `channame' */
		memcpy((void*)channame, (void*)p+1, name_sz); 
		*(channame + name_sz) = '\0';

		/* insert this channel into user's chanlist */
		add_ue_channel(ue, channame);

		/* go to next one */
		p = next;
	}
}

static void generate_ue_chanlist(
	struct ucache_user * ue,
	chanlist_t * chanlist )
{
	struct ucache_channel ** p_uc;
	unsigned left = ue->chan_count;

	assert(ue && chanlist);

	*chanlist[0] = '\0';

	for(p_uc = ue->chan; left--; p_uc ++) {
		strcat(*chanlist, "#");
		strcat(*chanlist, (*p_uc)->name);
	}
}

/** exported routines
 ********************************/

/** usercache_init
 * 	initializes the cache
 */
void usercache_init()
{
	ue_first = ue_last = NULL;
	ue_count = 0;

	chan_first = chan_last = NULL;
	chan_count = 0;
}

/** usercache_exit
 * 	destroys and free's cache
 */
void usercache_exit()
{
	struct ucache_channel * c, * c_next;
	struct ucache_user * u, * u_next;

	/* free any users alloc'ed */
	u = ue_first;
	while(u) {
		u_next = u->next;
		xfree((void*)u);
		u = u_next;
	}

	ue_first = ue_last = NULL;
	ue_count = 0;

	/* free any channels alloc'ed */
	c = chan_first;
	while(c) {
		c_next = c->next;
		xfree((void*)c);
		c = c_next;
	}

	chan_first = chan_last = NULL;
	chan_count = 0;
}

/** usercache_set_topic:
 * 	updates current topic
 */
void usercache_set_topic(
	const char * channel,
	const char * new_text)
{
	struct ucache_channel * uc;
	assert(channel && new_text);

	uc = chan_find(channel);
	if(uc) {
		strncpy(uc->topic, new_text, TOPIC_LEN_MAX);
	}
}

/** usercache_topic:
 * 	returns current topic
 * 	(char *) valid until the next call
 */
const char * usercache_topic(const char * channel)
{
	struct ucache_channel * uc;
	assert(channel);

	uc = chan_find(channel);
	if(!uc) {
		return NULL;
	}

	strcpy(req_topic, uc->topic);

	return req_topic;
}

/** usercache_same_topic:
 * 	returns whether the specified topic is the same
 * 	as the current one
 *
 * NOTE
 * 	returns -1: if no such channel found
 */
int usercache_same_topic(
	const char * channel,
	const char * text)
{
	struct ucache_channel * uc;

	assert(channel && text);

	uc = chan_find(channel);
	if(!uc) {
		return -1;
	}

	return strcmp(text, uc->topic)==0 ? 1: 0;
}

/** usercache_known_channels:
 * 	returns list of all channels (in "#chan1#chan2" format)
 */
void usercache_known_channels(
	chanlist_t * chlist)
{
	unsigned list_sz, this_sz;
	struct ucache_channel * chan;

	*chlist[0] = '\0';
	list_sz = 0;

	/* add name of each channel */
	for(chan = chan_first; chan; chan = chan->next)
	{
		this_sz = strlen(chan->name);
		if(list_sz + this_sz + 1 > CHANLIST_LEN_MAX) {
			/* ok this wont fit, bail out with the ones
			 * we've got already into the list
			 */
			log("usercache_known_channels: "
				"too many channels to fit into chanlist_t");
			break;
		}

		strcat(*chlist, "#");
		strcat(*chlist, chan->name);

		list_sz += this_sz + 1;
	}
}

/** usercache_add:
 * 	adds (modifies) specified user info in cache
 */
void usercache_add(
	const user_id *		p_uid,
	const enum net_umode *	p_umode,
	const char *		nickname,
	const char *		chanlist)
{
	char buf[128];
	struct ucache_user * ue;

	assert(p_uid);
	assert(!eq_user_id(p_uid, null_user_id()));

	ue = user_by_id(p_uid);
	if(ue!=NULL) {
		/* we have this one in the cache: modify it
		 */
		if(p_umode)
			ue->umode = *p_umode;

		if(nickname) {
			debug_a("usercache_add: user ");
			debug_a(user_id_dump(p_uid));
			debug_a(" has renamed in \"");
			debug_a(nickname); debug("\"");
	
			strncpy(ue->nickname, nickname, NICKNAME_LEN_MAX);
		}

		if(chanlist)
			set_ue_channels(ue, chanlist);
		return;
	}

	/* allocate & preset new entry
	 */
	assert(p_umode && chanlist && nickname);
	
	ue = xalloc(sizeof(struct ucache_user));

	ue->uid = *p_uid;
	ue->umode = *p_umode;
	strncpy(ue->nickname, nickname, NICKNAME_LEN_MAX);

	ue->chan = NULL;
	ue->chan_count = 0;

	ue->alive = 1;

	set_ue_channels(ue, chanlist);

	/* insert it into list */
	sprintf(buf, "usercache_add: new user \"%s\" %s",
		nickname, user_id_dump(p_uid)
	);
	debug(buf);
	
	ue->next = NULL;
	ue->prev = ue_last;
	if(ue_last) {
		ue_last->next = ue;
		ue_last = ue;
	} else {
		ue_last = ue_first = ue;
	}

	ue_count++;
}

/** usercache_remove:
 * 	removes specified user from the cache
 */
void usercache_remove(
	const user_id * uid)
{
	struct ucache_user * ue;

	assert(uid);
	ue = user_by_id(uid);

	debug_a("usercache_remove: removing user ");
	debug(user_id_dump(uid));

	/* check if we have this one in the cache */
	if(ue==NULL) {
		log_a("usercache_remove: can't remove unknown user \"");
		log_a(user_id_dump(uid));
		log("\"");
		return;
	}

	/* ok, remove it's references & allocated info */
	remove_ue_all_channels(ue);
	assert(ue->chan==0 && ue->chan_count==0);

	/* delete it from the list */
	if(ue_count==1) {
		ue_first = ue_last = NULL;
		ue_count = 0;
	} else {
		if(ue_first==ue) {
			ue_first = ue_first->next;
			ue_first->prev = NULL;
		}
		else if(ue_last==ue) {
			ue_last = ue_last->prev;
			ue_last->next = NULL;
		}
		else {
			ue->prev->next = ue->next;
			ue->next->prev = ue->prev;
		}
		ue_count --;
	}

	xfree((void*)ue);

	/* report other routers about this thing
	 */

}

/** usercache_known:
 * 	returns whether the user is known in usercache
 */
int usercache_known(const char * nickname)
{
	struct ucache_user * ue;

	assert(nickname);

	foreach_ue(ue) {
		if( eq_nickname(nickname, ue->nickname)) break;
	}
	return ue!=NULL;
}

/** usercache_uid_of:
 *	finds uid by nickname
 */
user_id usercache_uid_of(
	const char * nickname)
{
	struct ucache_user * ue;

	assert(nickname);

	foreach_ue(ue)
		if(eq_nickname(nickname, ue->nickname)) break;

	return ue==NULL ? *null_user_id(): ue->uid;
}

/** usercache_umode_of:
 * 	returns user mode for specified user
 */
enum net_umode usercache_umode_of(
		const user_id * uid)
{
	struct ucache_user * ue;

	assert(uid);
	ue = user_by_id(uid);

	return ue->umode;
}

/** usercache_chanlist_of:
 * 	returns chanlist of specified user
 */
const char * usercache_chanlist_of(
		const user_id * uid)
{
	struct ucache_user * ue;

	assert(uid);
	
	ue = user_by_id(uid);

	generate_ue_chanlist(ue, &req_chanlist);
	return req_chanlist;
}

/** usercache_nickname_of:
 * 	returns nickname of user
 * 	(const char * valid until the next call)
 */
const char * usercache_nickname_of(
		const user_id * uid)
{
	struct ucache_user * ue = user_by_id(uid);

	assert(ue && uid);

	strcpy(req_nickname, ue->nickname);

	return req_nickname;
}

/** usercache_enum:
 * 	invokes callback with info about all users
 */
void usercache_enum(
	usercache_enum_proc_t cb_proc,
	void * user)
{
	struct ucache_user * ue;
	chanlist_t chanlist;
	
	foreach_ue(ue) {
		generate_ue_chanlist(ue, &chanlist);

		cb_proc(user, &ue->uid, ue->umode,
			ue->nickname, chanlist);
	}
}

/** usercache_enum_except_net:
 * 	enumerates users which DO NOT belong to specified net
 */
void usercache_enum_except_net(
	const net_id * nid,
	usercache_enum_proc_t cb_proc,
	void * user_data )
{
	struct ucache_user * ue;
	chanlist_t chanlist;

	foreach_ue(ue) {
		if(!eq_net_id(nid, &ue->uid.net))
		{
			generate_ue_chanlist(ue, &chanlist);

			cb_proc(user_data, &ue->uid, ue->umode,
				ue->nickname, chanlist);
		}
	}
}

/** usercache_remove_net:
 * 	removes all users from specified net
 */
void usercache_remove_net(
	const net_id * nid)
{
	struct ucache_user * ue;

	while(1) {
		/* find user with the net specified */
		for(ue = ue_first; ue; ue = ue->next) {
			if(eq_net_id(&ue->uid.net, nid))
				break;
		}
		if(!ue) {
			/* no more such users found */
			break;
		}

		/* ok, remove this one */
		usercache_remove(&ue->uid);
	}
}

/** usercache_join_chan:
 * 	signs that the user has joined specified channel
 */
void usercache_join_chan(
	const user_id * uid,
	const char * channame )
{
	struct ucache_user * ue = user_by_id(uid);

	if(!ue) {
		log_a("usercache_join_chan: unknown user \"");
		log_a(user_id_dump(uid)); log("\"");
		return;
	}

	/* add channame to user's list & update channel references
	 */
	add_ue_channel(ue, channame);
}

/** usercache_part_chan:
 * 	signs that the user has left specified channel
 */
void usercache_part_chan(
	const user_id * uid,
	const char * channame )
{
	struct ucache_user * ue = user_by_id(uid);

	if(!ue) {
		log_a("usercache_part_chan: unknown user \"");
		log_a(user_id_dump(uid)); log("\"");
		return;
	}

	/* remove channame from user's list and
	 * update channel's references
	 */
	remove_ue_channel(ue, channame);
}

/** usercache_tag_dead_from:
 *	marks all users from specified local net as `dead'
 */
void usercache_tag_dead_from(
	net_id net)
{
	struct ucache_user * ue;

	foreach_ue(ue) {
		if(ue->uid.net==net) {
			ue->alive = 0;
		}
	}
}

/** usercache_tag_alive:
 *	marks specified user `not dead'
 */
void usercache_tag_alive(
	user_id user)
{
	struct ucache_user
		* ue = user_by_id(&user);

	if(ue) {
		ue->alive = 1;
	}
}

/** usercache_dead_users_from:
 *	returns xalloc'ed list of `dead' users
 *	from specified local network
 */
user_id * usercache_dead_users_from(
	net_id net,
	unsigned * p_count)
{
	struct ucache_user * ue;
	user_id * ulist;
	unsigned count = 0;

	assert(p_count);

	/* count how many of these we have */
	foreach_ue(ue) {
		if(ue->uid.net==net && !ue->alive)
			count++;
	}
	*p_count = count;

	if(!count) {
		/* no dead users were found */
		return NULL;
	}

	/* alloc list */
	ulist = xalloc(count * sizeof(user_id));
	
	/* fill it in */
	count = 0;
	foreach_ue(ue) {
		if(ue->uid.net==net && !ue->alive)
			ulist[count++] = ue->uid;
	}

	return ulist;
}

