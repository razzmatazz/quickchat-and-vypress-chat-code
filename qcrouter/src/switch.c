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
 *		switch.c
 *			switches and handles messages
 *
 *	(c) Saulius Menkevicius 2002,2003
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "msg.h"
#include "net.h"
#include "switch.h"
#include "routetbl.h"
#include "globals.h"
#include "usercache.h"

/** private routines
 */
static int parse_msg(qnet * net, const qnet_msg *);

static void handle_user_enum_request(qnet *);

/** switch_msg:
 * 	handles qmsg 'nmsg' from net 'net'
 */
void switch_msg(
	qnet * net,
	const qnet_msg * nmsg)
{
	/* check if we if we've seen this msg already */
	if(idcache_is_known(local_idcache, nmsg))
	{
		debug("switch_msg: duplicate msg received: ignored");
		return;
	}
	idcache_register(local_idcache, nmsg);

	if(!parse_msg(net, nmsg)) {
		return;
	}
	
	/* route it, it's not NULL dst and has no */
	if(!is_local_net(nmsg->dst.net) && !is_null_net(nmsg->dst.net)
		&& nmsg->type!=MSGTYPE_INVALID
		&& nmsg->type!=MSGTYPE_NULL
		)
	{
		/* send and forget:
		 *	this will handle broadcast as well
		 *	also, don't make loops by avoiding src net
		 */
		routetbl_send(nmsg);
	}
}

/** parse_msg:
 * 	parses and handles message contents
 */
static int parse_msg(
	qnet * net,
	const qnet_msg *nmsg)
{
	switch(nmsg->type)
	{
	case MSGTYPE_USER_ENUM_REQUEST:
		debug("parse_msg: USER_ENUM_REQUEST");
		handle_user_enum_request(net);
		break;
	case MSGTYPE_NET_NEW:
		debug_a("parse_msg: MSGTYPE_NET_NEW: d_net=");
		debug(net_id_dump(&net->id));

		routetbl_add_branch(net, &nmsg->d_net);
		break;
	case MSGTYPE_NET_LOST:
		debug("parse_msg: MSGTYPE_NET_LOST");

		/* update route tbl */
		routetbl_remove_branch(net, &nmsg->d_net);

		/* remove net's users */
		usercache_remove_net(&nmsg->d_net);
		break;
	case MSGTYPE_USER_NICKCHANGE:
		usercache_add(
			&nmsg->d_user, NULL,
			nmsg->d_nickname, NULL);
		break;
	case MSGTYPE_USER_MODECHANGE:
		usercache_add(
			&nmsg->d_user, &nmsg->d_umode,
			NULL, NULL);
		break;
	case MSGTYPE_USER_NEW:
		debug_a("parse_msg: MSGTYPE_USER_NEW: ");
		debug(nmsg->d_nickname);
	
		usercache_add(
			&nmsg->d_user, &nmsg->d_umode,
			nmsg->d_nickname, nmsg->d_chanlist);
		break;
	case MSGTYPE_USER_LOST:
		debug_a("parse_msg: MSGTYPE_USER_LOST: ");
		debug(user_id_dump(&nmsg->d_user));

		usercache_remove(&nmsg->d_user);
		break;
	case MSGTYPE_CHANNEL_JOIN:
		usercache_join_chan(&nmsg->src, nmsg->d_chanlist);
		break;
	case MSGTYPE_CHANNEL_LEAVE:
		usercache_part_chan(&nmsg->src, nmsg->d_chanlist);
		break;
	case MSGTYPE_TOPIC_CHANGE:
		usercache_set_topic(nmsg->d_chanlist, nmsg->d_text);
		break;
	case MSGTYPE_NULL:
			/* null msg: carries no info */
		break;
	default:break;
	}
	return 1;
}

void broadcast_route_change(
	const net_id * nid,
	int new_net)
{
	qnet_msg * nmsg = msg_new();
	assert(nid);

	nmsg->type = new_net ? MSGTYPE_NET_NEW: MSGTYPE_NET_LOST;
	msg_set_broadcast(nmsg);
	nmsg->src.net = *nid;
	nmsg->d_net = *nid;

	routetbl_send(nmsg);

	msg_delete(nmsg);
}

/** handle_user_enum_request:
 * 	replies the sender with user lists
 */
static void user_enum_cb(
		void * data,
		const user_id * p_uid, enum net_umode umode,
		const char * nickname, const char * chanlist)
{
#define NMSG	((qnet_msg*)data)
	idcache_assign_id(NULL, NMSG);
	NMSG->d_user = *p_uid;
	NMSG->d_umode = umode;
	NETMSG_SET_NICKNAME(NMSG, nickname);
	NETMSG_SET_CHANLIST(NMSG, chanlist);

	routetbl_send(NMSG);
#undef	NMSG
}

static void handle_user_enum_request(qnet * net)
{
	qnet_msg * nmsg = msg_new();
	assert(net);

	nmsg->type = MSGTYPE_USER_NEW;
	nmsg->src.net = *local_net_id();
	nmsg->dst.net = net->id;

	usercache_enum(user_enum_cb, (void*)nmsg);

	msg_delete(nmsg);
}

