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
 *		cfgparser.h
 *			parses config file & cmd line arguments
 *
 *	(c) Saulius Menkevicius 2002
 */

#ifndef CFGPARSER_H__
#define CFGPARSER_H__

/**
 * configuration infrastructure
 */
#define CONFIG_MAX_HOSTNAME 255

struct config_net_entry {
	enum qnet_type type;

	char hostname[CONFIG_MAX_HOSTNAME+1];
	unsigned short port;

	unsigned long * broadcasts;
		/* 0UL terminated list of broadcast networks
		   this link is connected to */

	struct config_net_entry * next;
};

struct config {
	/* local id */
	net_id avail_id;

	/* hosting settings */
	char * cfg_file_name;
	int allow_host, daemonize, local_refresh_timeout;
	char host_if[CONFIG_MAX_HOSTNAME+1];
	unsigned short host_port;

	/* far net settings */
	struct config_net_entry * net_head, * net_tail;
	int net_count;
};

struct config * read_config(int, const char **);
void delete_config(struct config *);

#endif	/* #ifndef CFGPARSER_H__ */

