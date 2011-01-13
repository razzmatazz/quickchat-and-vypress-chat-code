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
 *		host.h
 *			defines interface for hosting socket
 *			(the one where all connections goes to)
 *
 *	(c) Saulius Menkevicius 2002
 */

#ifndef HOST_H__
#define HOST_H__

int host_open(
	const char *bind_name,
	unsigned short bind_port,
	int * p_wait_socket);
int host_close(void);
int host_accept();

int host_get_prop(enum qnet_property);
void host_set_prop(enum qnet_property, int);
int host_enabled();

#endif	/* #ifndef HOST_H__ */

