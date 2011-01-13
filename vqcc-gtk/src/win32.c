/*
 * win32.c: win32 compatibility routines
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
 * $Id: win32.c,v 1.6 2005/01/03 20:36:55 bobas Exp $
 */

#include <glib.h>

#include "main.h"

#define _WIN32_WINNT 0x0500
#include <windows.h>

/* typedefs
 */
typedef BOOL (WINAPI *GETLASTINPUTINFO)(LASTINPUTINFO *);

/* static data
 */
static HINSTANCE win_hinstance;
static GETLASTINPUTINFO win_GetLastInputInfo;

/* forward references
 */

int _main(int, char **, char **);

/* win_log_handler:
 *	discards logging messages to stop annoying user
 */
void win_log_handler(
	const gchar * log_domain,
	GLogLevelFlags log_level,
	const gchar * message,
	gpointer user_data)
{
	/* just discard it */
}

/* WinMain:
 *	a simple stub for win32
 */
int APIENTRY WinMain(
	HINSTANCE hInstance, HINSTANCE hPrevInstance, 
	char *lpszCmdLine, 
	int nCmdShow) 
{
	WSADATA wsaData;
	HMODULE user32_module;
	int winsock_err;

	/* initialize winsock */
	winsock_err = WSAStartup(MAKEWORD(2, 0), &wsaData);
	if(winsock_err != 0) {
		/* failed to initialize windows socket */
		MessageBox(NULL, "Failed to initialize windows sockets",
			"Application startup failure",
			MB_OK|MB_ICONSTOP);
	}

#ifndef NDEBUG
	/* disable logging to console -- very annoying */
	g_log_set_handler(NULL, G_LOG_LEVEL_MASK, win_log_handler, NULL);
#endif

	win_hinstance = hInstance;
	user32_module = LoadLibrary("user32.dll");
	if(user32_module) {
		win_GetLastInputInfo = (GETLASTINPUTINFO)
			GetProcAddress(user32_module, "GetLastInputInfo");
	}

	/* start the app */
	return _main(__argc, __argv, 0); 
}

HINSTANCE
winvqcc_get_hinstance()
{
	return win_hinstance;
}

unsigned int
winvqcc_get_last_active()
{
	if(win_GetLastInputInfo) {
		LASTINPUTINFO lastInputInfo;
		lastInputInfo.cbSize = sizeof(lastInputInfo);
	
		if(win_GetLastInputInfo(&lastInputInfo))
			return GetTickCount() - lastInputInfo.dwTime;
	}
	return 0;
}
