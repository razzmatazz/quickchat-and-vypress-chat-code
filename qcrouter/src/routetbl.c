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
 *		routetbl.c
 *			implements route table
 *
 *	(c) Saulius Menkevicius 2002
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "msg.h"
#include "net.h"
#include "routetbl.h"
#include "switch.h"

struct rtbl_entry {
	qnet * conn;

	net_id * branches;
	unsigned branch_count;

	struct rtbl_entry * next, * prev;
};

#define foreach_re(re) \
		for((re)=rtbl_first;(re);(re)=(re)->next)

#define re_branch(re, nr) ((const net_id*)(re->branches + nr))

/** static variables
 **********************************/
static struct rtbl_entry
	* rtbl_first, * rtbl_last;
static unsigned rtbl_count;

/** static routines
 **********************************/

/** insert_branch:
 * 	NOTE:	here we don't check if the branch already exists!!!
 * 		(the caller must check)
 */
static void insert_branch(
	struct rtbl_entry * re,
	const net_id * nid)
{
	assert(re && nid);

	/** reallocate vector in 16 granularity */
	if(re->branch_count%16==0) {
		re->branches = re->branches==NULL
			? xalloc(sizeof(re->branches)*16)
			: xrealloc(
				re->branches,
				sizeof(re->branches)*(re->branch_count+16)
				);
	}

	/** insert new branch into vector */
	re->branches[ re->branch_count++ ] = *nid;
}

static void remove_branch(
	struct rtbl_entry * re,
	const net_id * nid)
{
	net_id * p_id;
	unsigned left;

	/** check if there's anything to remove */
	if(re->branch_count==0) {
		log("remove_branch: empty branch[]");
		return;
	}

	/** find the branch */
	p_id = re->branches;
	for(left=re->branch_count; left; left--) {
		if(eq_net_id(p_id, nid)) {
			break;
		}
		p_id ++;
	}
	if(left<0) {
		log("remove_branch: no branch found: bailing out");
		return;
	}

	/** remove it */
	re->branch_count--;
	if(!re->branch_count) {
		/** this was the last one: free vector */
		xfree(re->branches);
		re->branches = NULL;
	} else {
		/** shift vector entries after the specified one */
		memmove((void*)p_id, (void*)(p_id+1), sizeof(net_id)*left);

		/** reallocate in granularity of 16 entries */
		if(re->branch_count%16==0) {
			re->branches = xrealloc(
				re->branches,
				re->branch_count);
		}
	}
}

/** find_entry:
 * 	returns rtbl_entry of specified qnet *
 *	(or NULL, if no such found)
 */
static struct rtbl_entry *
	find_entry(qnet * net)
{
	struct rtbl_entry * re;

	foreach_re(re)
		if(re->conn == net) break;

	return re;
}

/** is_branch_of:
 * 	returns number of (branch_found + 1),
 * 	if found on specified `re'
 */
static int is_branch_of(
	struct rtbl_entry * re,
	const net_id * p_nid)
{
	int b;

	assert(re && p_nid);

	for(b=0; b < re->branch_count; b++) {
		if(eq_net_id(p_nid, &re->branches[b]))
			return (b+1);
	}
	return 0;
}

static struct rtbl_entry *
	find_branch(
		const net_id * nid,
		unsigned * p_branch_num)
{
	struct rtbl_entry * re;
	unsigned b; 

	foreach_re(re) {
		b = is_branch_of(re, nid);
		if(b) {
			/* branch found */
			if(p_branch_num) *p_branch_num = b-1;
			return re;
		}
	}

	if(p_branch_num) *p_branch_num = 0;
	return NULL;
}

static void do_near_broadcast(
	const qnet_msg * nmsg	)
{
	struct rtbl_entry * re;
	assert(nmsg);

	/** the source net must be set
	 * in order to avoid circular message loops!!
	 */
	assert(!is_null_net(nmsg->src.net));

	/* send to each of these
	 */
	foreach_re(re)
		if(!eq_net_id(&re->conn->id, &nmsg->src.net)
			&& !is_branch_of(re, &nmsg->src.net))
		{
			re->conn->send(re->conn, nmsg);
		}
}

/** remove_all_branches
 */
static void remove_all_branches(struct rtbl_entry * re)
{
	net_id * branches;
	int branch_count, i;

	assert(re);

	/* make copy of ids */
	branches = xalloc(re->branch_count * sizeof(void*));
	branch_count = re->branch_count;
	memcpy(branches, re->branches, sizeof(void*)*re->branch_count);

	/* remove each of ids */
	for(i=0; i < branch_count; i++) {
		broadcast_route_change(branches+i, 0);
		remove_branch(re, branches+i);
	}

	xfree(branches);
}

/** exported routines 
 **********************************/

/** routetbl_init:
 * 	inits route table structures
 */
void routetbl_init()
{
	rtbl_first = rtbl_last = NULL;
	rtbl_count = 0;
}

/** routetble_exit:
 * 	deinitializes route table
 */
void routetbl_exit()
{
	struct rtbl_entry * re, * next;

	re = rtbl_first;
	while(re) {
		next = re->next;

		if(re->branches) {
			xfree(re->branches);
		}
		xfree(re);

		re = next;
	}
	rtbl_first = rtbl_last = NULL;
	rtbl_count = 0;
}

/** routetbl_add:
 * 	add new net to route table
 */
void routetbl_add(qnet * net)
{
	struct rtbl_entry * re;

	debug_a("routetbl_add: inserting major route entry for net ");
	debug(net_id_dump(&net->id));

	/** check if we have this one already
	 */
	if(find_entry(net)) {
		debug_a("routetbl_add: route entry for \"");
		debug_a(net_id_dump(&net->id));
		debug("\" already exists");
		return;
	}

	/** ok, create new one
	 *	and insert into the list
	 */
	re = xalloc(sizeof(struct rtbl_entry));

	re->conn = net;
	
	re->branches = NULL;
	re->branch_count = 0;

	re->next = NULL;
	re->prev = rtbl_last;

	if(!rtbl_count) {
		rtbl_last = rtbl_first = re;
	} else {
		rtbl_last->next = re;
		rtbl_last = re;
	}
	rtbl_count++;

	/* broadcast change */
	broadcast_route_change(&net->id, 1);
}

/** routetbl_remove:
 * 	removes qnet connections from route table
 */
void routetbl_remove(qnet * net)
{
	struct rtbl_entry * re = find_entry(net);

	if(re==NULL) {
		/* no such entry found */
		log_a("routetbl_remove: requested removal of unknown conn \"");
		log_a(net_id_dump(&net->id));
		log("\" table entry: ignored");
		return;
	}

	/** remove branches */
	remove_all_branches(re);

	broadcast_route_change(&net->id, 0);

	/** remove it from the list */
	if(rtbl_count==1) {
		rtbl_first = rtbl_last = NULL;
	} else {
		if(re==rtbl_first) {
			rtbl_first = rtbl_first->next;
			rtbl_first->prev = NULL;
		}
		else if(re==rtbl_last) {
			rtbl_last = rtbl_last->prev;
			rtbl_last->next = NULL;
		}
		else {
			re->prev->next = re->next;
			re->next->prev = re->prev;
		}
	}
	rtbl_count --;

	/** remove branches */
	if(re->branches) {
		xfree((void*)re->branches);
	}
	xfree((void*)re);
}

/** routetbl_add_branch:
 * 	adds route branch to specified qnet connection
 */
void routetbl_add_branch(
	qnet * net,
	const net_id * nid )
{
	struct rtbl_entry * re = find_entry(net);
	assert(re);

	debug_a("routetbl_add_branch: route to ");
	debug_a(net_id_dump(nid));
	debug_a(" [through ");
	debug_a(net_id_dump(&net->id));
	debug("]");

	/** check that it is'nt already on the list */
	if(find_branch(nid, NULL)) {
		log_a("routetbl_add_branch: the net \"");
		log_a(net_id_dump(nid));
		log("\" already in route table: ignored");
		return;
	}

	/** ok, not found: insert it */
	insert_branch(re, nid);

	/* broadcast new net */
	broadcast_route_change(nid, 1);
}	

/* routetbl_remove_branch:
 *	removes route branch from specified qnet connection
 */
void routetbl_remove_branch(
	qnet * net,
	const net_id * nid )
{
	struct rtbl_entry * re = find_entry(net);
	assert(re);

	/* broadcast change */
	broadcast_route_change(nid, 0);

	/** remove entry */
	remove_branch(re, nid);
}

/** routetbl_where:
 * 	finds the link where the specified net could be routed to
 * returns:
 * 	qnet * of the link, or
 * 	NULL if the route couldn't be found
 */
qnet * routetbl_where(const net_id * p_nid)
{
	struct rtbl_entry * re;

	/** check if it's any of our neighbours */
	foreach_re(re) {
		if(eq_net_id(p_nid, &re->conn->id))
			break;
	}
	if(re) {
		/** yes, its neigbour.. */
		return re->conn;
	}

	/** no, this must be net more than 1 hops from us... */
	re = find_branch(p_nid, NULL);
	if(!re) {
		/** route search failed */
/*		log_a("routetbl_where: no route found for net \"");
		log_a(net_id_dump(p_nid));
		log("\"");
*/
		return NULL;
	}
	return re->conn;
}

/** routetbl_enum_root:
 *	enumerates all branches of the specified network
 *	(including the root)
 *
 *	NULL if no such network found
 */
net_id * routetbl_enum_root(
	unsigned int * p_id_count,
	qnet * root_net)
{
	net_id * list;
	struct rtbl_entry * re;
	unsigned branch;

	/* find net_id in routetbl */
	re = find_entry(root_net);
	if(!re) {
		debug_a("routetbl_enum_root: can't find rtbl_entry for net [");
		debug_a(net_id_dump(&root_net->id)); debug("]");
		
		if(p_id_count) *p_id_count = 0;
		return NULL;
	}

	/* allocate and fill in the findings */
	list = xalloc(sizeof(net_id)*(1 + re->branch_count));
	list[0] = root_net->id;

	/* enumerate branches if the root has any */
	for(branch=0; branch < re->branch_count; branch++)
		list[branch+1] = re->branches[branch];

	/* return list size */
	if(p_id_count)
		*p_id_count = 1 + re->branch_count;

	return list;
}

/** routetbl_enum_all:
 * 	returns the list of all net_id's known in route table
 */
net_id * routetbl_enum_all(
		unsigned int * p_id_count,
		const net_id * except_net )
{
	net_id * ids, *p_id;
	unsigned branch, id_count = 0;
	struct rtbl_entry * re;

	/** check count of ids */
	foreach_re(re) {
		if(except_net==NULL || !eq_net_id(except_net, &re->conn->id))
			id_count += re->branch_count + 1;
	}

	/** return nr of ids */
	if(p_id_count)
		*p_id_count = id_count;

	/** exit if no ids' available */
	if(!id_count)
		return NULL;

	/** allocate ids vector */
	ids = xalloc(sizeof(net_id)*id_count);

	/** dump net_id's */
	p_id = ids;
	foreach_re(re) {
		/** skip if excluded by parameter */
		if(except_net && eq_net_id(except_net, &re->conn->id))
			continue;

		/** connection/neighbour id.. */
		*(p_id++) = re->conn->id;

		/** .. & ids of branches */
		for(branch=0; branch < re->branch_count; branch++)
			*(p_id++) = re->branches[branch];
	}
	return ids;
}

/** routetbl_send:
 * 	sends message to the net found in it's
 * 	dst.net */
int routetbl_send(
	const qnet_msg *nmsg )
{
	qnet * through_net;

	/* if it's broadcast message
	 * 	send it to neighbour networks
	 */
	if(msg_is_broadcast(nmsg)) {
		do_near_broadcast(nmsg);
		return 1;
	}
	
	/* get the route */
	through_net = routetbl_where(&nmsg->dst.net);

	if(through_net==NULL)
	{
		debug_a("routetbl_send: no known route for msg [dst_net = ");
		debug_a(net_id_dump(&nmsg->dst.net));
		debug("]");
	} else {
		/* send it */
		through_net->send(through_net, nmsg);	
	}
	return 1;
}
