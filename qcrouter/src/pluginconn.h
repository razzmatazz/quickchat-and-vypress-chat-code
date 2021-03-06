#ifndef PLUGINCONN_H__
#define PLUGINCONN_H__

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
 *		pluginconn.h
 *			implements Perl interface for writing plugins
 *		(for bots and et al)
 *
 *	(c) Saulius Menkevicius 2002,2003
 */

void plugin_init();

qnet * plugin_open(const char * filename);

#endif /* #ifndef PLUGINCONN_H__ */

