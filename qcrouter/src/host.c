/**
 * qcRouter links several QuickChat & VypressChat nets over the Internet
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
 *		host.c
 *			implements interface for hosting socket
 *			(the one where all connections goes to)
 *
 *	(c) Saulius Menkevicius 2002,2003
 */

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>

#include "common.h"
#include "msg.h"
#include "net.h"
#include "host.h"

/** static vars
 */
static int host_socket = -1;

/** exported routines
 */

/** host_enabled:
 * 	returns non-zero, if hosting socket is ready
 */
int host_enabled()
{
	return host_socket>=0;
}

int host_open(
	const char *bind_name,
	unsigned short bind_port,
	int * p_wait_socket )
{
	struct sockaddr_in sin;
	struct hostent * he;
	unsigned long if_ip;

	assert(bind_name);

	if(host_socket>=0) {
		panic("host:\talready open");
	}

	/** check port boundaries */
	if(bind_port<1024) {
		log("host:\tinvalid port specified (<1024): exited");
		return 0;
	}

	/** get local bind_name IP */
	if(!*bind_name) {
		if_ip = 0;
	} else {
		he = gethostbyname(bind_name);
		if(!he) {
			log_a("host\t: no IP address found for interface \"");
			log_a(bind_name);
			log("\": exited");
			return 0;
		}
		if_ip = ntohl(*(unsigned long*)he->h_addr);
	}

	/** create socket */
	host_socket = socket(PF_INET, SOCK_STREAM, 0);
	if(host_socket<0) {
		log_a("host:\tcan't create AF_INET socket (");
		log_a(strerror(errno)); log(")");
		return 0;
	}

	/** bind socket */
	sin.sin_family = AF_INET;
	sin.sin_port = htons(bind_port);
	sin.sin_addr.s_addr = htonl(if_ip);
	if(bind(host_socket, (struct sockaddr*)&sin, sizeof(sin))<0) {
		log_a("host:\tcan't bind socket to IP/interface \"");
		log_a(bind_name); log_a("\": ");
		log(strerror(errno));

		close(host_socket);
		host_socket = -1;
		return 0;
	}

	/** accept connections to socket */
	if(listen(host_socket, 2)) {
		log_a("host:\tcan't listen() on socket: ");
		log(strerror(errno));

		close(host_socket);
		host_socket = -1;
		return 0;
	}

	return 1;	/* ok, succeded */
}

int host_close()
{
	if(host_socket<0)
		return 0;

	close(host_socket);
	host_socket = -1;

	return 1;
}

int host_accept()
{
	int net_sock;
	struct sockaddr_in sa_in;
	socklen_t sa_len;

	assert(host_socket!=-1);

	net_sock = accept(host_socket, (struct sockaddr *)&sa_in, &sa_len);

	if(net_sock<0) {
		debug("host:\taccept() failed");
	}
	
	return net_sock;
}

int host_get_prop(enum qnet_property prop)
{
	switch(prop)
	{
	case QNETPROP_ONLINE:
		return host_socket>=0;

	case QNETPROP_RX_SOCKET:
		return host_socket;

	default:
		break;
	}
	return 0;
}

void host_set_prop(enum qnet_property prop, int new_value)
{
	/** no properties to set */
}

