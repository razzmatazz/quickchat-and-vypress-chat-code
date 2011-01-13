/*
 * net.h: network funcs
 * Copyright (C) 2002-2004 Saulius Menkevicius
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: net.h,v 1.7 2004/12/17 02:04:29 bobas Exp $
 */

#ifndef NET_H__
#define NET_H__

/* preferences registered in the `net' module */
#define PREFS_NET_TYPE		"net/type"
#define PREFS_NET_PORT		"net/port"
#define PREFS_NET_IS_CONFIGURED	"net/is_configured"
#define PREFS_NET_BROADCAST_MASK "net/broadcast_mask"
#define PREFS_NET_USE_MULTICAST	"net/use_multicast"
#define PREFS_NET_MULTICAST_ADDR "net/multicast_addr"
#define PREFS_NET_VERBOSE	"net/verbose"
#define PREFS_NET_CHANNEL_NOTIFY	"net/channel_greet"
#define PREFS_NET_IGNORE_MASS_MSG	"net/ignore_mass_msg"
#define PREFS_NET_ENCODING		"net/encoding"
#define PREFS_NET_REPLY_INFO_REQ	"net/reply_info_req"
#define PREFS_NET_MOTD			"net/motd"
#define PREFS_NET_MOTD_WHEN_AWAY	"net/motd_when_away"
#define PREFS_NET_IGNORED_USERS		"net/ignored_users"

#define my_motd() (\
	(prefs_int(PREFS_MAIN_MODE)==UMODE_AWAY || prefs_int(PREFS_MAIN_MODE)==UMODE_OFFLINE) \
		? prefs_str(PREFS_NET_MOTD_WHEN_AWAY) : prefs_str(PREFS_NET_MOTD) )

enum net_type_enum {
	NET_TYPE_QCHAT,
	NET_TYPE_VYPRESS,
	NET_TYPE_NUM,
	NET_TYPE_FIRST = NET_TYPE_QCHAT,
	NET_TYPE_LAST = NET_TYPE_VYPRESS
};

void net_register();
gboolean net_connected();
const gchar * net_name_of_type(enum net_type_enum);

#endif /* #ifndef NET_H__ */

