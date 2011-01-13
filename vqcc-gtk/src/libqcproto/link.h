/**
 * libqcproto: Vypress/QChat protocol interface library
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

/*
 * QCS: qChat 1.6/VypressChat link interface
 *
 *      link module
 *
 * (c) Saulius Menkevicius 2001-2004
 */

#ifndef LINK_H
#define LINK_H

#define QCP_MAXUDPSIZE	0x2000

/* windows compatibility hacks
 */
#ifdef WIN32
#define strcasecmp	stricmp
#define close		closesocket
typedef int socklen_t;
#define ENETUNREACH	ENOENT
#define ENOMSG		ENOENT
#endif

/* qcslink
 *	defines state of a link */
typedef struct link_data_struct {
	enum qcs_proto proto;		/* link protocol	*/
	int rx, tx;			/* rx and tx sockets	*/
	unsigned short port;		/* link port		*/
	unsigned long broadcast_addr;	/* broadcast/multicast address */
} link_data;

#endif	/* LINK_H */
