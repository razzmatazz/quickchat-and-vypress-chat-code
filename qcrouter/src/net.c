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
 *		net.c
 *			encapsulates net connections
 *			and maintains ther list.
 *
 *	(c) Saulius Menkevicius 2002
 */

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "msg.h"
#include "net.h"
#include "usercache.h"
#include "routerconn.h"
#include "localconn.h"
#include "routetbl.h"
#include "cfgparser.h"

typedef struct qnet_list_entry
{
	qnet * net;

	struct qnet_list_entry
		* next, * prev;
} qnet_le;

struct exchange_user_data {
	qnet * net;
	qnet_msg * nmsg;
};

/** static vars
 */
static qnet * local = NULL;

static qnet_le
	* le_first, * le_last;
static unsigned
	le_size;

#define foreach_le(le) \
		for(le=le_first; le; le=le->next)

/** static routines
 **********************/

static qnet_le * le_by_qnet(qnet * net)
{
	qnet_le * le;

	assert(net);

	foreach_le(le) {
		if(le->net==net) break;
	}
	assert(le);

	return le;
}

/** le_remove:
 * 	removes entry from the qnet list
 */
static void le_remove(qnet_le * le)
{
	assert(le_first && le_last && le_size);
	assert(le);

	if(le_first == le_last) {
		le_first = le_last = NULL;
	}
	else if(le == le_first) {
		le_first->next->prev = NULL;

		le_first = le_first->next;
	}
	else if(le == le_last) {
		le_last->prev->next = NULL;

		le_last = le_last->prev;
	} else {
		le->prev->next = le->next;
		le->next->prev = le->prev;
	}

	le_size --;
}

/** le_add:
 * 	adds entry to the qnet list
 */
static void le_add(qnet_le * le)
{
	assert(le);

	le->next = NULL;

	if(le_size == 0) {
		le_first = le_last = le;

		le->prev = NULL;
	} else {
		le_last->next = le;
		le->prev = le_last;

		le_last = le;
	}
	le_size ++;
}

/** remove_users_from:
 *	removes users from the net and it's branches
 */
void remove_users_from(qnet * net_conn)
{
	net_id * dead;
	unsigned dead_count, i;

	assert(net_conn);

	debug_a("remove_users_from: removing users connected through [");
	debug_a(net_id_dump(&net_conn->id)); debug("]");

	/* get `dead' net list */
	dead = routetbl_enum_root(&dead_count, net_conn);
	if(dead) {
		assert(dead_count);

		/* remove users from those nets */
		for(i=0; i < dead_count; i++)
			usercache_remove_net(dead + i);
	}
}


/** do_handshake:
 * 	1. checks version with another net (TODO)
 * 	2. requests & sets netid
 * 	3. requests user lists, net lists
 * NOTE:
 * 	we may return 0, which says we can't speak with
 * 	the peer and should terminate the connection
 */

static int exchange_routetbl(qnet * net)
{
	qnet_msg * nmsg;
	net_id * ids;
	unsigned ids_count, id, more;

	nmsg = msg_new();
	nmsg->type = MSGTYPE_NET_NEW;

	ids = routetbl_enum_all(&ids_count, NULL);
	for(id = 0; id < ids_count; id++) {
		idcache_assign_id(NULL, nmsg);
		nmsg->d_net = ids[id];
		net->send(net, nmsg);
	}
	if(ids) {
		xfree(ids);
	}

	nmsg->type = MSGTYPE_NET_ENUM_ENDS;
	net->send(net, nmsg);
	msg_delete(nmsg);

	/* get their routetbl */
	routetbl_add(net);

	for(;;) {
		nmsg = net->recv(net, &more);
		if(nmsg==NULL || (nmsg->type!=MSGTYPE_NET_ENUM_ENDS
					&& nmsg->type!=MSGTYPE_NET_NEW) )
		{
			if(nmsg) msg_delete(nmsg);
			return 0;	/* handshake failed */
		}

		if(nmsg->type==MSGTYPE_NET_ENUM_ENDS)
			break;

		routetbl_add_branch(net, &nmsg->d_net);
		msg_delete(nmsg);
	}
	msg_delete(nmsg);

	return 1;
}

static int exchange_usercache(qnet * net)
{
	/* do simple request */
	qnet_msg * nmsg = msg_new();

	nmsg->type = MSGTYPE_USER_ENUM_REQUEST;
	net->send(net, nmsg);

	msg_delete(nmsg);
	return 1;
}

static int do_handshake(qnet * net)
{
	qnet_msg * nmsg = msg_new();
	qnet_msg * recvd;
	int more;

	assert(net);

	/* do version chech & exchange ids */
	nmsg->type = MSGTYPE_HANDSHAKE;
	NETMSG_SET_TEXT(nmsg, APP_VERSION_STRING);
	nmsg->d_net = *local_net_id();

	net->send(net, nmsg);
	msg_delete(nmsg);

	/* check version & get net id
	 */
	recvd = net->recv(net, &more);
	if(recvd==NULL || recvd->type!=MSGTYPE_HANDSHAKE) {
		if(recvd) {
			msg_delete(recvd);
		}
		return 0;
	}

	/* XXX: check version */

	net->id = recvd->d_net;
	msg_delete(recvd);

	/* setup routetbl & usercache */
	if(!exchange_routetbl(net)) return 0;
	if(!exchange_usercache(net)) return 0;

	return 1;	/* ok, continue */
}

/** exported routines
 **********************/

/** net_init:
 * 	initializes net interfaces
 */
void net_init(const struct config * cfg)
{
	if(local!=NULL) {
		panic("already initiated");
	}

	/** setup qnet list */
	le_first = le_last = NULL;
	le_size = 0;

	/** allocate & setup 'local' qnet */
	local = xalloc(sizeof(qnet));

	local->id = *local_net_id();
	local->type = QNETTYPE_ROUTER;

	local->conn = NULL;
	
	/** 'local' qnet has no associated manipulator funcs */
	local->destroy = NULL;
	local->send = NULL;
	local->recv = NULL;
	local->get_prop = NULL;
	local->set_prop = NULL;

	/* init local_net & router_net subsystems
	 */
	localconn_init(cfg->local_refresh_timeout);
}

/** net_exit:
 * 	frees net lists (and shutdowns interfaces - if any)
 */
void net_exit()
{
	qnet_le * le, * next;

	/* terminate subsystems
	 */
	localconn_exit();

	if(local==NULL)
		panic("not initiated");

	/** free any net alloc'ed */
	le = le_first;
	while(le) {
		next = le->next;

		/** destroy conn & free (qnet *) */
		le->net->destroy( le->net );
		xfree(le);

		le = next;
	}
	
	le_first = le_last = NULL;
	le_size = 0;

	/** free 'local' net */
	xfree(local);
	local = NULL;
}

/** net_self:
 * 	returns our (router's) qnet
 */
qnet * net_self()
{
	return local;
}

/** net_connect:
 * 	connects to specified net
 * returns:
 * 	qnet * of the net linked to, or NULL in case of failure
 */
qnet * net_connect(
	enum qnet_type type,
	const void * addr, unsigned short port)
{
	char buf[256];

	qnet_le * le = xalloc(sizeof(qnet_le));

	/** do net/type specific connection */
	switch(type)
	{
	case QNETTYPE_QUICK_CHAT:
		sprintf(buf, "net:\tconnecting to QCHAT on port %hu..", port);
		log(buf);

		/* setup local broadcast net connection */
		le->net = local_connect(addr, port, type);
		break;

	case QNETTYPE_VYPRESS_CHAT:
		sprintf(buf, "net:\tconnecting to VYCHAT on port %hu..", port);
		log(buf);

		/* setup local broadcast net connection */
		le->net = local_connect(addr, port, type);
		break;

	case QNETTYPE_ROUTER:
		sprintf(buf, "net:\tconnecting to qcRouter %s:%hu..",
			(const char*)addr, port);
		log(buf);
	
		/* do connect to external router */
		le->net = router_connect((const char*)addr, port, type);
		break;

	case QNETTYPE_INCOMMING:
		log("net:\taccepting incomming connection..");
		/* handle connection from external router */
		le->net = router_connect(NULL, 0, type);
		break;

	default:
		log("net:\tinvalid new connection/net type: ignored");
		le->net = NULL;
		break;
	}

	if(le->net==NULL) {
		/* net allocation failed */
		xfree(le);
		return NULL;
	}
	
	/* add entry to the list */
	le_add(le);

	/* send welcome to the new peer
	 */
	if( ! do_handshake(le->net)) {
		/* oh.. i don't like you, shut it down */
		
		log_a("net:\tcan't speak with the net \"");
		log_a( net_id_dump(&le->net->id) );
		log("\"");

		net_disconnect(le->net);
		return NULL;
	}

	/* handshake went smoothly */
	log_a("net:\tconnected to ");
	log(net_id_dump(&le->net->id));

	return le->net;
}

/** net_disconnect:
 * 	disconnects the net
 */
int net_disconnect(qnet * net)
{
	log_a("net:\tdisconnecting the net [");
	log_a(net_id_dump(&net->id)); log("]");

	/* removing users of qnet parents from the cache */
	remove_users_from(net);

	/* update route table */
	routetbl_remove(net);

	/** destroy the link and remove from the list */
	net->destroy(net);
	le_remove(le_by_qnet(net));

	return 1;
}

/** net_qnetbyid:
 * 	returns qnet * of the net the msg is sent to
 * 	(or NULL if it's destined not at one of our connections)
 */
qnet * net_qnetbyid(
	const net_id * nid)
{
	qnet_le * le;

	assert(nid);

	foreach_le(le) {
		if(le->net->id==*nid) break;
	}
	return le ? le->net: NULL;
}

/** net_enum:
 * 	returns malloc'ed vector of qnet *,
 * 	where the last one in array is NULL
 * returns:
 * 	qnet *[] & number of nets in *p_qnet_count
 */
qnet ** net_enum(unsigned int * p_qnet_count)
{
	qnet ** list;
	qnet_le *le;
	int num;

	list = xalloc(sizeof(qnet*) * (le_size+1));

	num = 0;
	foreach_le(le) {
		list[num++] = le->net;
	}
	list[num] = NULL;

	if(p_qnet_count) {
		*p_qnet_count = le_size;
	}

	return list;
}

