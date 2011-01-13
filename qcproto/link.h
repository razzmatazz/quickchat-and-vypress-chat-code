/**
 * qcs_link: Vypress/QChat protocol interface library
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
 * (c) Saulius Menkevicius 2001, 2002
 */

#ifndef LINK_H
#define LINK_H

#define QCP_MAXUDPSIZE	0x200

/* qcslink
 *	defines state of a link */
typedef struct link_data_struct {
	int rx, tx;             /* rx. tx socket ids    */
	unsigned short port;    /* link port            */
	unsigned long * broadcasts;
		/* list of broadcast addresses in host byte order	*/
	unsigned int broadcast_count;

	int mode;		/* mode of the link (Qchat/vypress) */
} link_data;

#endif	/* LINK_H */
