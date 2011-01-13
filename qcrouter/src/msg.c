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
 *		msg.c
 *			manages net_msg'es and msg id caches
 *
 *	(c) Saulius Menkevicius 2002,2003
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "msg.h"
#include "net.h"
#include "globals.h"

/** private structs
 */
struct msgqueue_entry {
	qnet_msg * msg;
	struct msgqueue_entry * next;
};
struct msgqueue {
	struct msgqueue_entry
		* head, * tail;
};
#define PMSGQUEUE(p) ((struct msgqueue*)p)

/** static var
 */
static char ascii_dump[0x200];	/* holds qnet_msg ascii dumps */

/** exported routines
 ***************************/

/** msg_new:
 * 	creates new msg
 */
qnet_msg * msg_new(void)
{
	qnet_msg * msg = xalloc(sizeof(qnet_msg));

	idcache_assign_id(NULL, msg);
	msg->type = MSGTYPE_INVALID;
	msg->src = *null_user_id();
	msg->dst = *null_user_id();
	msg->d_net = *null_net_id();
	msg->d_user = *null_user_id();

	msg->d_umode = UMODE_INVALID;
	msg->d_me_text = 0;

	strcpy(msg->d_nickname, "");
	strcpy(msg->d_chanlist, "");
	strcpy(msg->d_text, "");

	return msg;
}

/* msg_delete:
 * 	destroys msg
 */
void msg_delete(qnet_msg * nmsg)
{
	if(nmsg) {
		xfree(nmsg);
	}
}

/** msg_is_broadcast:
 * 	returns if specified msg is broadcasting one
 */
int msg_is_broadcast(
	const qnet_msg * nmsg)
{
	return eq_net_id(&nmsg->dst.net, broadcast_net_id());
}

/** msg_set_broadcast:
 * 	marks message as broadcasting one
 * 	(sets destination *)
 */
void msg_set_broadcast(qnet_msg *nmsg)
{
	nmsg->dst.net = *broadcast_net_id();
}

/** msg_dump:
 * 	builds ascii dump of specified msg
 * 	(usually for logging purposes)
 */
const char * msg_dump(const qnet_msg * nmsg)
{
	sprintf(ascii_dump, "msg_dump: wheee.. not implemented");
	
	return ascii_dump;
}

/** idcache_new:
 * 	creates & inits new idcache_t
 */
idcache_t * idcache_new()
{
	idcache_t * ic = xalloc(sizeof(idcache_t));

	/** init cache structs */
	ic->ids = xalloc(sizeof(qnet_msg_id)*IDCACHE_MAX_SIZE);
	ic->head = 0;

	/** dump basic "null" ids (the cache must always be of 0x40 size) */
	memset((void*)ic->ids, 'm', sizeof(qnet_msg_id)*IDCACHE_MAX_SIZE);

	return ic;
}

/** idcache_delete:
 * 	destroys idcache_t
 */
void idcache_delete(idcache_t * idc)
{
	assert(idc && idc->ids);

	xfree(idc->ids);
	xfree(idc);
}

/** idcache_is_known:
 *	returns if the msg id has been seen already in cache
 */
int idcache_is_known(idcache_t * idc, const qnet_msg *nmsg)
{
	unsigned int nr;
	for(nr=0; nr < IDCACHE_MAX_SIZE; nr++) {
		if(idc->ids[nr]==nmsg->id)
			break;
	}
	return nr!=IDCACHE_MAX_SIZE;
}

/** idcache_register:
 * 	registers msg id in cache
 */
void idcache_register(idcache_t * idc, const qnet_msg *nmsg)
{
	assert(idc && idc->ids && nmsg);

	idc->head %= IDCACHE_MAX_SIZE;

	idc->ids[idc->head] = nmsg->id;
}

/** idcache_assign_id:
 * 	assigns fresh new unique id for the msg
 * 	and registers in the cache (if idc!=NULL);
 */
void idcache_assign_id(idcache_t * idc, qnet_msg * nmsg)
{
	nmsg->id = (rand() & 0xFFF)
			| ((rand() & 0xFFF)<<12)
			| ((rand() & 0xFF)<<24);

	/** register in the cache */
	if(idc) {
		idcache_register(idc, nmsg);
	}
}

/** msgq_new:
 *	creates new msg queue
 */
msgq_id msgq_new()
{
	struct msgqueue
		* mq = xalloc(sizeof(struct msgqueue));

	mq->head = mq->tail = NULL;

	return (msgq_id)mq;
}

/** msgq_delete:
 *	destroys msg queue
 */
void msgq_delete(msgq_id mq)
{
	qnet_msg * nmsg;

	assert(PMSGQUEUE(mq));
	assert((PMSGQUEUE(mq)->head && PMSGQUEUE(mq)->tail)
		|| (!PMSGQUEUE(mq)->head && !PMSGQUEUE(mq)->tail));

	/* delete any entries that are in the queue */
	for(nmsg=msgq_pop(mq); nmsg; nmsg=msgq_pop(mq))
		msg_delete(nmsg);

	/* free struct */
	xfree((void*)PMSGQUEUE(mq));
}

/** msgq_push:
 *	insert msg into the queue
 */
void msgq_push(
	msgq_id mq,
	qnet_msg * nmsg)
{
	struct msgqueue_entry * me;

	assert(PMSGQUEUE(mq) && nmsg);
	assert((PMSGQUEUE(mq)->head && PMSGQUEUE(mq)->tail)
		|| (!PMSGQUEUE(mq)->head && !PMSGQUEUE(mq)->tail));

	/* alloc & reset queue entry */
	me = xalloc(sizeof(struct msgqueue_entry));
	me->msg = nmsg;
	me->next = NULL;

	/* insert into the queue */
	if(PMSGQUEUE(mq)->head) {
		/* queue is not empty */
		PMSGQUEUE(mq)->head->next = me;
		PMSGQUEUE(mq)->head = me;
	} else {
		/* queue is empty */
		PMSGQUEUE(mq)->tail = PMSGQUEUE(mq)->head = me;
	}
}

/** msgq_pop:
 *	pops msg from the queue
 *	(or NULL if empty)
 */
qnet_msg * msgq_pop(msgq_id mq)
{
	struct msgqueue_entry * me;
	qnet_msg * nmsg;

	assert(PMSGQUEUE(mq));
	assert((PMSGQUEUE(mq)->head && PMSGQUEUE(mq)->tail)
		|| (!PMSGQUEUE(mq)->head && !PMSGQUEUE(mq)->tail));

	if(!PMSGQUEUE(mq)->head) {
		/* the queue is empty */
		return NULL;
	}

	/* remove from the list */
	me = PMSGQUEUE(mq)->tail;
	if(PMSGQUEUE(mq)->tail==PMSGQUEUE(mq)->head) {
		PMSGQUEUE(mq)->tail = PMSGQUEUE(mq)->head = NULL;
	} else {
		PMSGQUEUE(mq)->tail = PMSGQUEUE(mq)->tail->next;
	}
	assert((PMSGQUEUE(mq)->head && PMSGQUEUE(mq)->tail)
		|| (!PMSGQUEUE(mq)->head && !PMSGQUEUE(mq)->tail));

	/* extract data & free entry */
	nmsg = me->msg;
	xfree((void*)me);

	return nmsg;
}

/** msgq_empty:
 *	returns if the msg is empty
 */
int msgq_empty(msgq_id mq)
{
	assert(PMSGQUEUE(mq));
	assert((PMSGQUEUE(mq)->tail && PMSGQUEUE(mq)->head)
		|| (!PMSGQUEUE(mq)->tail && !PMSGQUEUE(mq)->head));

	return PMSGQUEUE(mq)->tail==NULL;
}

