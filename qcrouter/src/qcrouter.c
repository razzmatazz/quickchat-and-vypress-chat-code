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
 *		qcrouter.c
 *			main file
 *
 *	(c) Saulius Menkevicius 2002,2003
 */

#include <sys/time.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "msg.h"
#include "net.h"
#include "host.h"
#include "usercache.h"
#include "routetbl.h"
#include "switch.h"
#include "cfgparser.h"

/* static vars
 ***********************************/
struct config * cfg;

/* forward references
 ***********************************/
void do_connect();
void start_host();
void do_init();
void do_shutdown();
void route_loop();

struct pollfd * build_poll_table(unsigned int *);
qnet * poll_select_net(struct pollfd *, unsigned int, unsigned short *);
int process_net_event(qnet *, unsigned short);

qnet ** make_no_rx_networks_list();
void route_no_rx_networks(qnet **);

int kill_net_link(qnet *, int);

/** exported/global variables
 */
struct idcache_struct * local_idcache;

/** main
 * 	entry point
 */
int main(int argc, const char ** argv)
{
	/* print banner */
	log(APP_NAME" version "APP_VERSION_STRING" starting..");
	log(APP_NAME" comes with ABSOLUTELY NO WARRANTY;");
	log("This is free software, and you are welcome to distribute it\n"
		"under certain conditions; use -licence argument for details");

	/* init application */
	common_init();
	cfg = read_config(argc, argv);
	common_set_local_id(cfg->avail_id);
	debug_a("local_net_id = ");
	debug(net_id_dump(local_net_id()));

	usercache_init();
	routetbl_init();
	net_init(cfg);

	local_idcache = idcache_new();

	/* make connections */
	do_connect();

	/* start hosting */
	if(cfg->allow_host) {
		start_host(cfg);
	}
	
	/* there should be at least one connection available
	 * or better hosting be enabled */
	if(!cfg->net_count && !host_enabled())
	{
		log("No pre-set network connection(s) available"
			" and hosting not enabled: bailing out");
		exit(EXIT_FAILURE);
	}

	/* daemonize, if needed */
	if(cfg->daemonize) {
		log("Entering background (daemon) mode.."); 
		if(daemon(0,0)==-1) {
			log("daemon() failed.. terminating");
			goto shutdown;
		}

		common_post_fork();
	}

	/* start processing */
	route_loop();

	/* terminate connections & shutdown everything */
shutdown:
	delete_config(cfg);
	do_shutdown();

	return EXIT_SUCCESS;
}

/** route_loop
 * 	does the actual job of watching messages on interfaces
 */
void route_loop()
{
	struct pollfd * pfd;
	unsigned int pfd_size;
	unsigned short revents;
	qnet * net, ** no_rx_nets;

	debug("route_loop...");

	pfd = build_poll_table(&pfd_size);

	/* no-rx-networks have no RX socket, thus must be handled
	 * in a different way
	 */
	no_rx_nets = make_no_rx_networks_list();

	while(1) {
		/* route messages from plugins */
		route_no_rx_networks(no_rx_nets);

		/* we might have missed the timer signal somewhere */
		if(timer_ignited) {
			timer_process();
		}
		
		/* wait for events & signals */
		if(poll(pfd, pfd_size, -1)==-1) {
			if(timer_ignited) {
				timer_process();

				/* plugins might insert messages into their
				 * queues during timer handling..
				 */
				route_no_rx_networks(no_rx_nets);
			}
			continue;
		}

		net = poll_select_net(pfd, pfd_size, &revents);

		if(net==NULL) {
			net = net_connect(QNETTYPE_INCOMMING, NULL, 0);
			if(net) {
				/* connected: update poll table */
				xfree(pfd);
				pfd = build_poll_table(&pfd_size);

				log_a("net:\tconnection with \"");
				log_a(net_id_dump(&net->id));
				log("\" has been established");
			} else {
				log("net:\tinvalid connection: ignored");
			}
			continue;
		}

		/* handle event from network */
		if(!process_net_event(net, revents)) {
			xfree(pfd);
			pfd = build_poll_table(&pfd_size);
		}
	}

	/* do cleanups */
	xfree(pfd);
	xfree(no_rx_nets);
}

/** process_net_event
 *	handles poll() event specified net
 *  returns:
 * 	zero, if the net got down: rebuild poll table
 */
int process_net_event(
	qnet * net, unsigned short revents)
{
	qnet_msg * nmsg;
	int more_msg_left;

	assert(net);

	if(revents & (POLLHUP|POLLERR|POLLNVAL))
	{
		log_a("net:\tlink failure for net \"");
		log_a(net_id_dump(&net->id));
		log("\"");
	
		kill_net_link(net, 1);
		return 0;	/* force pfds table rebuild */
	}

	/* handle the POLLIN case:
	 * 	message(s) are available
	 */
	do {
		nmsg = net->recv(net, &more_msg_left);

		/* check if the link is ok:
		 * 	kill it otherwise
		 */
		if(nmsg==NULL && net->get_prop(net, QNETPROP_DAMAGED)) {
			kill_net_link(net, 1);
			return 0;	/* force rebuild of pfds table */
		}

		/* handle (switch) msg */
		if( nmsg->type!=MSGTYPE_INVALID ) {
			switch_msg(net, nmsg);
		}

		msg_delete(nmsg);
	}
	while(more_msg_left);

	return 1;
}

/** poll_select_net
 *	returns network which has some data to recv()
 * returns:
 * 	a) qnet * of the net which has something to read
 * 	b) NULL, if hosting socket has info
 */
qnet * poll_select_net(
	struct pollfd * pfds,
	unsigned int pfds_count,
	unsigned short * p_revents)	/* number of net in pfds table */
{
	qnet ** net_list, * net;
	int count, i;

	assert(pfds!=NULL);

	/* find net which has `revents' set */
	while(pfds_count--) {
		if(pfds->revents) {
			break;
		}
		pfds++;
	}
	assert(pfds_count>=0);	/* one must've been set */

	/* set (*p_revents) */
	if(p_revents) *p_revents = pfds->revents;

	/* ok, one found; now check whos' net is it */
	if(host_enabled() && pfds->fd==host_get_prop(QNETPROP_RX_SOCKET)) {
		/* it's a hosting socket.. */
		return NULL;
	}

	net_list = net_enum(&count);
	/* at least one net must be present, since it's not hosting socket */
	assert(net_list);

	/* find which one */
	for(i = 0; i < count; i++) {
		net = net_list[i];
		if(net->get_prop(net, QNETPROP_RX_SOCKET)==pfds->fd) {
			break;
		}
	}
	assert(i!=count);

	xfree(net_list);

	return net;
}

/** build_poll_table
 * 	build pollfd's of current net rx sockets
 */
struct pollfd *
build_poll_table(
	unsigned int * p_pfd_num)
{
	qnet ** nets, ** p_net;
	unsigned int count;
	struct pollfd * pfds, * p;
	int sock;

	/* qnet *[] & num of nets */
	nets = net_enum(&count);

	if(host_enabled()) count ++;

	if(p_pfd_num) {
		*p_pfd_num = count;
	}

	if(count==0) {
		/** no active net links found;
		 * err.. this is unusable configuration,
		 */
		panic("no active connections present and hosting not enabled");
	}

	/* setup poll table */
	pfds = xalloc(sizeof(struct pollfd)*count);
	p = pfds;
	if(host_enabled()) {
		p->fd = host_get_prop(QNETPROP_RX_SOCKET);
		p->events = POLLIN;

		p++;
		count--;
	}

	/* setup pfd's for network which has rx socket */
	p_net = nets;
	while(count--) {
		sock = (*p_net)->get_prop(*p_net, QNETPROP_RX_SOCKET);
		if(sock >= 0) {
			p->fd = (*p_net)->get_prop(*p_net, QNETPROP_RX_SOCKET);
			p->events = POLLIN;
			p++;
		}
		p_net++;
	}

	/* cleanup any unneded structs */
	xfree(nets);

	return pfds;
}

/** do_connect
 *	opens primary connections to other routers on the internet
 *	(specified in config files/params)
 */
void do_connect()
{
	struct config_net_entry * net;

	debug("do_connect...");

	for(net = cfg->net_head; net; net=net->next) {
		switch(net->type) {
		case QNETTYPE_VYPRESS_CHAT:
		case QNETTYPE_QUICK_CHAT:
			net_connect(net->type, net->broadcasts, net->port);
			break;
		case QNETTYPE_ROUTER:
			net_connect(net->type, net->hostname, net->port);
			break;
		default:
			panic("cannot net_connect: invalid network type");
		}
	}
}

/** do_shutdown
 * 	shuts down connections and does cleanups
 */
void do_shutdown()
{
	qnet ** net_list, ** p_net;

	debug("do_shutdown...");

	/* shutdown connections */
	net_list = net_enum(NULL);
	if(net_list) {
		/* shutdown one and every of connections */
		for(p_net = net_list; *p_net; p_net++)
			net_disconnect(*p_net);

		xfree(net_list);
	}

	/* cleanup tables/alloc'ed structs */
	net_exit();
	routetbl_exit();
	usercache_exit();

	idcache_delete(local_idcache);
	local_idcache = NULL;

	common_free();
}

/** start_host
 * 	opens host socket and prepares
 * 	for accepting outside connections from other routers
 */
void start_host()
{
	char str[256];

	log("host:\tsetting up remove net connection hosting..");

	if(host_open(cfg->host_if, cfg->host_port, NULL))
	{
		sprintf(str, "host:\twaiting at interface \"%s\" on port %hd",
			*cfg->host_if=='\0' ? "localhost": cfg->host_if,
			cfg->host_port);
		log(str);
	}
}

/* kill_net_link
 * 	kills specified network connection
 */
int kill_net_link(
	qnet * net,	/* hosting socket, if ==NULL */
	int reconnect)	/* if set, tries to reconnect upon shutdown */
{
	net_disconnect(net);

	/* kill_net_link: XXX: implement reconnecting */

	return 1;
}

/* make_no_rx_networks_list
 *	allocates and builds vector-list of networks which
 *	have no RX sockets.
 * last entry on the list is NULL
 */
qnet ** make_no_rx_networks_list()
{
	qnet ** list, ** all_networks, * net;
	int count = 0, all_count, i;

	all_networks = net_enum(&all_count);
	list = xalloc(sizeof(qnet *) * all_count);

	for(i=0; i < all_count; i++) {
		net = all_networks[i];
		if(net->get_prop(net, QNETPROP_RX_SOCKET) < 0) {
			// this network has no RX socket
			list[count++] = net;
		}
	}

	list[count] = 0;
	return list;
}

/* route_no_rx_networks()
 *	recvs & routes messages from networks which have no RX sockets
 */
void route_no_rx_networks(qnet ** networks)
{
	qnet * net;
	qnet_msg * msg;
	int more_msg_left;

	for(; *networks; networks++) {
		net = *networks;
		if(net->get_prop(net, QNETPROP_RX_PENDING)) {
			do {
				msg = net->recv(net, &more_msg_left);
				if(msg && msg->type!=MSGTYPE_INVALID) {
					switch_msg(net, msg);
				}
				msg_delete(msg);
			} while(more_msg_left);
		}
	}
}

