/*
 * gui_tray_win32.c: system tray support for Win32
 * Copyright (C) 2004 Saulius Menkevicius
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
 * $Id: gui_tray_win32.c,v 1.8 2004/12/30 19:15:22 bobas Exp $
 */

#define GUI_TRAY_IMPL

#include <windows.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkwin32.h>
#include <gtk/gtk.h>

#include <string.h>

#include "main.h"
#include "win32.h"
#include "user.h"
#include "gui_tray.h"

#include "win32_resource.h"

#define WM_TRAY_MESSAGE WM_USER
#define N_TRAY_ICONS	(UMODE_NUM_VALID * 2)

/* forward references
 */
static gboolean	w32_tray_create(const gchar *);
static void	w32_tray_destroy();
static void	w32_tray_set_icon(gboolean, enum user_mode_enum);
static void	w32_tray_set_embedded_notifer(tray_embedded_notifier *);
static void	w32_tray_set_removed_notifier(tray_removed_notifier *);
static void	w32_tray_set_clicked_notifier(tray_clicked_notifier *);

/* local (static) structs
 */
static struct tray_impl_ops 
impl_ops = {
	w32_tray_create,
	w32_tray_destroy,
	w32_tray_set_icon,
	NULL,			/* no set_tooltip method for w32_tray */
	w32_tray_set_embedded_notifer,
	w32_tray_set_removed_notifier,
	w32_tray_set_clicked_notifier
};


static HWND tray_hwnd;
static HICON tray_icons[N_TRAY_ICONS];
static NOTIFYICONDATA tray_nid;

static tray_embedded_notifier * notifier_embedded;
static tray_removed_notifier * notifier_removed;
static tray_clicked_notifier * notifier_clicked;

/* tray implementation routines
 */

static LRESULT CALLBACK
w32_tray_msg_handler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static UINT taskbar_restart_msg;

	switch(msg) {
	case WM_CREATE:
		taskbar_restart_msg = RegisterWindowMessage("TaskbarCreated");
		break;
		
	case WM_TIMER:
		break;

	case WM_DESTROY:
		break;
	
	case WM_TRAY_MESSAGE:
		if(notifier_clicked) {
			if(lparam==WM_LBUTTONUP)
				notifier_clicked(1, gtk_get_current_event_time());
			if(lparam==WM_RBUTTONUP)
				notifier_clicked(3, gtk_get_current_event_time());
		}
		break;
	
	default:
		if(msg==taskbar_restart_msg)
			Shell_NotifyIcon(NIM_ADD, &tray_nid);
		break;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

static HWND
w32_tray_create_hidden_wnd()
{
	WNDCLASSEX wcex;
	TCHAR wname[32];
	
	strcpy(wname, "Vqcc-gtkTrayWin");

	wcex.cbSize = sizeof(WNDCLASSEX);
	
	wcex.style = 0;
	wcex.lpfnWndProc = (WNDPROC)w32_tray_msg_handler;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = winvqcc_get_hinstance();
	wcex.hIcon = NULL;
	wcex.hCursor = NULL;
	wcex.hbrBackground = NULL;
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = wname;
	wcex.hIconSm = NULL;

	RegisterClassEx(&wcex);

	return CreateWindow(
		wname, "", 0, 0, 0, 0, 0, GetDesktopWindow(),
		NULL, winvqcc_get_hinstance(), 0);
}

static HICON
w32_tray_get_hicon(gboolean active, enum user_mode_enum user_mode)
{
	HICON hicon;
	gint icon_num = (active ? 1: 0) * UMODE_NUM_VALID + (gint)user_mode;
	
	hicon = tray_icons[icon_num];
	if(!hicon) {
		hicon = (HICON)LoadImage(
			winvqcc_get_hinstance(),
			MAKEINTRESOURCE(active ? VQCC_GTK_ICON_ACT: VQCC_GTK_ICON),
			IMAGE_ICON, 16, 16, 0);

		tray_icons[icon_num] = hicon;
	}

	return hicon;
}

static gboolean
w32_tray_create(const gchar * tray_app_name)
{
	tray_hwnd = w32_tray_create_hidden_wnd();

	ZeroMemory(&tray_nid, sizeof(tray_nid));
	tray_nid.cbSize = sizeof(tray_nid);
	tray_nid.hWnd = tray_hwnd;
	tray_nid.uID = 0;
	tray_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	tray_nid.uCallbackMessage = WM_TRAY_MESSAGE;
	tray_nid.hIcon = w32_tray_get_hicon(FALSE, UMODE_NORMAL);
	g_strlcpy(tray_nid.szTip, PACKAGE" "VERSION, sizeof(tray_nid.szTip));

	Shell_NotifyIcon(NIM_ADD, &tray_nid);

	/* send notification that we've the tray icon embedded */
	if(notifier_embedded)
		notifier_embedded();

	return FALSE;
}

static void
w32_tray_destroy()
{
	Shell_NotifyIcon(NIM_DELETE, &tray_nid);
	DestroyWindow(tray_hwnd);
	tray_hwnd = NULL;

	if(notifier_removed)
		notifier_removed();

	notifier_embedded = NULL;
	notifier_removed = NULL;
	notifier_clicked = NULL;
}

static void
w32_tray_set_icon(gboolean active, enum user_mode_enum mode)
{
	tray_nid.hIcon = w32_tray_get_hicon(active, mode);
	Shell_NotifyIcon(NIM_MODIFY, &tray_nid);
}

static void
w32_tray_set_embedded_notifer(tray_embedded_notifier * notifier)
{
	notifier_embedded = notifier;
}

static void
w32_tray_set_removed_notifier(tray_removed_notifier * notifier)
{
	notifier_removed = notifier;
}

static void
w32_tray_set_clicked_notifier(tray_clicked_notifier * notifier)
{
	notifier_clicked = notifier;
}

/* exported methods
 */
const struct tray_impl_ops *
tray_impl_init()
{
	tray_hwnd = NULL;

	notifier_embedded = NULL;
	notifier_removed = NULL;
	notifier_clicked = NULL;

	memset(&tray_icons, 0, sizeof(HICON) * N_TRAY_ICONS);

	return &impl_ops;
}

