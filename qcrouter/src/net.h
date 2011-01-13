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
 *		net.h
 *			encapsulates net connections
 *			and maintains ther list.
 *
 *	(c) Saulius Menkevicius 2002
 */

#ifndef NET_H__
#define NET_H__

enum qnet_type {
	QNETTYPE_QUICK_CHAT,
	QNETTYPE_VYPRESS_CHAT,
	QNETTYPE_ROUTER,
	QNETTYPE_PLUGIN,

		/* for net_connect: does host_accept() to create new conn */
	QNETTYPE_INCOMMING
};

enum qnet_property {
	QNETPROP_ONLINE,
	QNETPROP_RX_SOCKET,
	QNETPROP_RX_PENDING,
	QNETPROP_DAMAGED
};

typedef struct qnet_struct {
	net_id id;
	
	enum qnet_type type;
	void * conn;

	void (*destroy)(struct qnet_struct *);

	void (*send)(struct qnet_struct *, const qnet_msg *);
	qnet_msg * (*recv)(struct qnet_struct *, int *);
	int (*get_prop)(struct qnet_struct *, enum qnet_property);
	int (*set_prop)(struct qnet_struct *, enum qnet_property, int);
} qnet;

#define QNET_SEND(n, m) n->send(n, m)
#define QNET_RECV(n) n->recv(n, m)

struct config;
void net_init(const struct config *);
void net_exit();
qnet * net_self();

qnet * net_connect(
	enum qnet_type,
	const void *,		/* `char*' for remotes, `ulong*' for local*/
	unsigned short);	/* port */
int net_disconnect(qnet *);
qnet * net_qnetbyid(const net_id *);
qnet ** net_enum(unsigned int * p_qnet_count);

#endif	/* #ifndef NET_H__ */

