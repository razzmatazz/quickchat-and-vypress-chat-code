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
 *		routetbl.h
 *			implements route table
 *
 *	(c) Saulius Menkevicius 2002
 */

#ifndef ROUTETBL_H__
#define ROUTETBL_H__

void routetbl_init();
void routetbl_exit();

void routetbl_add(qnet *);
void routetbl_remove(qnet *);

void routetbl_add_branch(qnet *, const net_id *);
void routetbl_remove_branch(qnet *, const net_id *);

/** finds the next hop of for the route to `dest_id' */
qnet * routetbl_where(const net_id *dest_id);

/** enumerates all network id's except our own & specified by parameter */
net_id * routetbl_enum_all(unsigned int *p_szarr, const net_id *except_id);

/** enumerates all branches of the specified network (including itself) */
net_id * routetbl_enum_root(unsigned int *p_szarr, qnet *);

/** routes msg to where it belongs (finds route by net_id) */
int routetbl_send(const qnet_msg *);

#endif	/* #ifndef ROUTETBL_H__ */

