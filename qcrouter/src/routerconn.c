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
 *		routerconn.c
 *			manages connections with other routers
 *			on the internet
 *
 *	(c) Saulius Menkevicius 2002,2003
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

#include "common.h"
#include "msg.h"
#include "net.h"
#include "routerconn.h"
#include "host.h"

#define MAX_MSG_SIZE	sizeof(qnet_msg)

/** structures
 */
struct router_conn_data {
	int damaged;
	int socket;
};
#define NETCONN	((struct router_conn_data*)net->conn)

#define WR_SHORT(v) do { \
	*(unsigned short *)p = (unsigned short)v;	\
	p += sizeof(unsigned short);			\
	} while(0)
	
#define WR_LONG(v) do { \
	*(unsigned long *)p = (unsigned long)v;	\
	p += sizeof(unsigned long);		\
	} while(0)

#define WR_STR(s) do { \
	len = strlen(s)+1;	\
	*(unsigned short *)p = len;	\
	p += sizeof(short);		\
	memcpy(p, s, len);	\
	p += len;		\
	} while(0)

#define WR_NETID(id) do { \
		WR_SHORT(id);	\
	} while(0)

#define WR_USERID(id) do { \
		WR_NETID(id.net);	\
		WR_SHORT(id.num);	\
	} while(0)

static void routerconn_send(
	qnet * net,
	const qnet_msg * nmsg)
{
	unsigned short len;
	char * buf, * p;

	assert(net && net->type==QNETTYPE_ROUTER);

	buf = xalloc(MAX_MSG_SIZE);

	/* make msg */
	p = buf;

	WR_LONG(nmsg->id);
	WR_SHORT(nmsg->type);
	WR_USERID(nmsg->src);
	WR_USERID(nmsg->dst);
	WR_NETID(nmsg->d_net);
	WR_USERID(nmsg->d_user);
	WR_SHORT(nmsg->d_me_text);
	WR_SHORT(nmsg->d_umode);
	WR_STR(nmsg->d_nickname);
	WR_STR(nmsg->d_chanlist);
	WR_STR(nmsg->d_text);
	
	/* size of the msg */
	len = p - buf;

	/* send this stuff out */
	if(send(NETCONN->socket, &len, sizeof(unsigned short), 0)<0) {
		NETCONN->damaged = 1;
		xfree(buf);
		return;
	}
	
	if(send(NETCONN->socket, buf, len, 0)<0) {
		NETCONN->damaged = 1;
	}

	/* clean up */
	xfree(buf);
}

#define RD_SHORT(v) do {	\
	if(msg_len<sizeof(short)) goto fail;	\
	v = *(unsigned short*)p;	\
	p += sizeof(short);		\
	msg_len -= sizeof(short);	\
	} while(0)

#define RD_LONG(v) do {	\
	if(msg_len<sizeof(long)) goto fail;	\
	v = *(unsigned long*)p;		\
	p += sizeof(long);		\
	msg_len -= sizeof(long);	\
	} while(0)

#define RD_USERID(id) do { \
		RD_NETID((id).net);	\
		RD_SHORT((id).num);	\
	} while(0)
	
#define RD_NETID(id) do { \
		RD_SHORT(id);		\
	} while(0)
	
#define RD_STR(s) do { \
		RD_SHORT(str_len);	\
		if(msg_len<str_len) goto fail;	\
		memcpy((s), p, str_len);	\
		p += str_len;		\
		msg_len -= str_len;	\
	} while(0)

static qnet_msg * routerconn_recv(
	qnet * net,
	int * p_more_msg_left)
{
	unsigned short msg_len, str_len;
	char * buf, * p;
	qnet_msg * nmsg;
	unsigned received;

	assert(p_more_msg_left && net && net->type==QNETTYPE_ROUTER);

	/* we do no delayed msg */
	*p_more_msg_left = 0;

	/* recv msg len */
	do {
		received = recv(
			NETCONN->socket, &msg_len,
			sizeof(unsigned short), 0
		);
		if(received==-1 && errno!=EINTR) {
			/* the link is no longer valid */
			NETCONN->damaged = 1;
			return NULL;
		}
	}
	while(received!=sizeof(unsigned short));

	if(msg_len > MAX_MSG_SIZE) {
		NETCONN->damaged = 1;
		return NULL;
	}

	/* recv the msg itself */
	buf = xalloc(MAX_MSG_SIZE);
	do {
		received = recv(
			NETCONN->socket, buf, 
			msg_len, 0
		);
		if(received==-1 && errno!=EINTR) {
			NETCONN->damaged = 1;
			xfree(buf);
			return NULL;
		}
	} while(received!=msg_len);

	/* parse the msg
	 */
	p = buf;
	nmsg = msg_new();

	RD_LONG(nmsg->id);
	RD_SHORT(nmsg->type);
	RD_USERID(nmsg->src);
	RD_USERID(nmsg->dst);
	RD_NETID(nmsg->d_net);
	RD_USERID(nmsg->d_user);
	RD_SHORT(nmsg->d_me_text);
	RD_SHORT(nmsg->d_umode);
	RD_STR(nmsg->d_nickname);
	RD_STR(nmsg->d_chanlist);
	RD_STR(nmsg->d_text);

	goto success;
fail:
	msg_delete(nmsg);
	nmsg = NULL;

success:
	xfree(buf);
	return nmsg;
}

static int routerconn_get_prop(
	qnet * net,
	enum qnet_property prop)
{
	assert(net && net->type==QNETTYPE_ROUTER);

	switch(prop) {
	case QNETPROP_ONLINE:
		return 1;
	case QNETPROP_RX_SOCKET:
		return NETCONN->socket;
	case QNETPROP_DAMAGED:
		return NETCONN->damaged;
	}

	return 0;
}

static int routerconn_set_prop(
	qnet * net,
	enum qnet_property prop,
	int new_value)
{
	assert(net && net->type==QNETTYPE_ROUTER);

	/* no property to set */

	return 0;
}

/** routerconn_destroy:
 * 	destroys router connection
 */
static void routerconn_destroy(
	qnet * net)
{
	assert(net && net->type==QNETTYPE_ROUTER);

	shutdown(NETCONN->socket, 2);
	close(NETCONN->socket);

	xfree(net->conn);
	xfree(net);
}


/** router_connect:
 * 	connects to outer network
 * 	or (if hostname==NULL) - accepts connection
 * returns:
 * 	qnet * of the net connected with
 *
 * 	NULL if connection hasn't been established
 */

qnet * router_connect(
	const char * hostname,
	unsigned short port,
	enum qnet_type type)
{
	int sock;
	qnet * net;
	net_id conn_net_id;

	struct hostent * he;
	struct sockaddr_in sin;

	assert(type==QNETTYPE_INCOMMING
		|| type==QNETTYPE_ROUTER);

	/* setup connection */
	if(type==QNETTYPE_INCOMMING)
	{
		/* accept one from hosting socket
		 */
		sock = host_accept();
		if(sock<0) {
			log_a("net:\tfailed to accept connection from ");
			log(net_id_dump(&conn_net_id));
			return NULL;
		}
	} else {
		assert(hostname);

		/* setup new connection
		 */
		he = gethostbyname(hostname);
		if(!he) {
			log_a("net:\tcouldn't get address for \"");
			log_a(hostname);
			log("\": failing connection attempt");
			return NULL;
		}
		sock = socket(PF_INET, SOCK_STREAM, 0);
		if(sock<0) {
			log_a("net:\tsocket() failed: ");
			log(strerror(errno));
			return NULL;
		}

		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);
		sin.sin_addr.s_addr = *(unsigned long*)he->h_addr;

		if(connect(sock, (struct sockaddr*)&sin, sizeof(sin))) {
			log_a("net:\tconnect() failed: ");
			log(strerror(errno));
			close(sock);
			return NULL;
		}
	}

	/* setup qnet structure */
	net = (qnet *)xalloc(sizeof(qnet));

	net->id = conn_net_id;
	net->type = QNETTYPE_ROUTER;
	net->conn = (struct router_conn_data*)
		xalloc(sizeof(struct router_conn_data));

	NETCONN->socket = sock;
	NETCONN->damaged = 0;
	
	net->destroy = routerconn_destroy;
	net->send = routerconn_send;
	net->recv = routerconn_recv;
	net->get_prop = routerconn_get_prop;
	net->set_prop = routerconn_set_prop;

	return net;
}

