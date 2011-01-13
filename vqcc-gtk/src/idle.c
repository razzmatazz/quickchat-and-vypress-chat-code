/*
 * idle.c: manages app usage (auto away/offline)
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
 * $Id: idle.c,v 1.12 2005/01/03 20:36:55 bobas Exp $
 */

#include <glib.h>

#include "main.h"
#include "user.h"
#include "idle.h"
#include "prefs.h"

#ifndef _WIN32
 #ifdef USE_XSCREENSAVER
  #include <X11/Xlib.h>
  #include <X11/Xutil.h>
  #include <X11/extensions/scrnsaver.h>
  #include <gdk/gdkx.h>
 #endif /* #ifdef USE_XSCREENSAVER */
#else
 #include <windows.h>
 #include "win32.h"
#endif

/* each `IDLE_CHECK_PERIOD_MS' we check if workstation hasn't become idle
 */
#define IDLE_CHECK_PERIOD_MS	1000

/* static variables
 *----------------- */
gint idle_check_timeout;
enum idle_mode_enum { IDLE_NOT, IDLE_AWAY, IDLE_OFFLINE } idle_mode;

/* static routines
 *------------------ */

static gboolean
idle_check_cb(gpointer dummy)
{
	unsigned idle_time; /* idle time in minutes */

	/* do not enter auto-away or auto-offline when we're in invisible mode */
	if(my_mode()==UMODE_INVISIBLE)
		return TRUE;

#ifdef _WIN32
	idle_time = winvqcc_get_last_active() / 60000;
#else
#ifdef USE_XSCREENSAVER
	static XScreenSaverInfo * xss_info = 0;
	int event_base, error_base;

	/* Query X Window System XScreenSaver extension
	 */
	if(XScreenSaverQueryExtension(GDK_DISPLAY(), &event_base, &error_base)) {
		if(!xss_info)
			xss_info = XScreenSaverAllocInfo();

		XScreenSaverQueryInfo(GDK_DISPLAY(), GDK_ROOT_WINDOW(), xss_info);
		idle_time = xss_info->idle / 60000;
	} else {
		idle_time = 0;
	}
#else
	idle_time = 0;
#endif
#endif /* #ifdef _WIN32 */

	/* Check if we reached any of AUTO_AWAY/AUTO_OFFLINE timeouts
	 */
	switch(idle_mode) {
	case IDLE_NOT:
		/*  From IDLE_NOT we can get straight into AUTO_OFFLINE mode, if
		 * prefs_int(PREFS_AUTO_OFFLINE)==prefs_int(PREFS_AUTO_AWAY).
		 *  Otherwise, get into AUTO_AWAY mode.
		 */
		if(idle_time >= prefs_int(PREFS_IDLE_AUTO_OFFLINE)) {
			idle_mode = IDLE_OFFLINE;
			raise_event(EVENT_IDLE_AUTO_OFFLINE, 0, 0);
		} else if(idle_time >= prefs_int(PREFS_IDLE_AUTO_AWAY)) {
			idle_mode = IDLE_AWAY;
			raise_event(EVENT_IDLE_AUTO_AWAY, 0, 0);
		}
		break;
	case IDLE_AWAY:
		/*  From IDLE_AWAY we can get into IDLE_NOT state, if
		 * the user awakes, or into IDLE_OFFLINE, if
		 * idle_time >= prefs_int(PREFS_AUTO_OFFLINE)
		 */
		if(idle_time < prefs_int(PREFS_IDLE_AUTO_AWAY)) {
			idle_mode = IDLE_NOT;
			raise_event(EVENT_IDLE_NOT, 0, 0);
		} else if(idle_time >= prefs_int(PREFS_IDLE_AUTO_OFFLINE)) {
			idle_mode = IDLE_OFFLINE;
			raise_event(EVENT_IDLE_AUTO_OFFLINE, 0, 0);
		}
		break;
	case IDLE_OFFLINE:
		/*  From IDLE_OFFLINE, we can get into IDLE_NOT,
		 * if the user awakes
		 */
		if(idle_time < prefs_int(PREFS_IDLE_AUTO_AWAY)) {
			idle_mode = IDLE_NOT;
			raise_event(EVENT_IDLE_NOT, 0, 0);
		}
		break;
	}
	
	return TRUE;
}

static gboolean
idle_prefs_auto_away_validator(const gchar * prefs_name, gpointer user_data)
{
	guint away_value = prefs_int(prefs_name);

	if(away_value==0 || away_value > 240)
		return FALSE;

	if(prefs_int(PREFS_IDLE_AUTO_OFFLINE) < away_value)
		prefs_set(PREFS_IDLE_AUTO_OFFLINE, away_value);

	return TRUE;
}

static gboolean
idle_prefs_auto_offline_validator(const gchar * prefs_name, gpointer user_data)
{
	guint offline_value = prefs_int(prefs_name);

	if(offline_value==0 || offline_value > 240)
		return FALSE;

	if(prefs_int(PREFS_IDLE_AUTO_AWAY) < offline_value)
		prefs_set(PREFS_IDLE_AUTO_AWAY, offline_value);

	return TRUE;
}

static void
idle_prefs_idle_enable_changed(const gchar * prefs_name)
{
	if(prefs_bool(PREFS_IDLE_ENABLE)) {
		/* create timer */
		if(idle_check_timeout <= 0)
			idle_check_timeout = g_timeout_add(IDLE_CHECK_PERIOD_MS, idle_check_cb, 0);
	} else {
		/* enter the IDLE_NOT mode, if we're disabling idle functionality
		 * and we are in IDLE_AWAY or IDLE_OFFLINE modes
		 */

		/* destroy timer source */
		if(idle_check_timeout>0) {
			g_source_remove(idle_check_timeout);
			idle_check_timeout = 0;
		}

		/* enter the IDLE_NOT mode */
		if(idle_mode != IDLE_NOT) {
			idle_mode = IDLE_NOT;
			raise_event(EVENT_IDLE_NOT, NULL, 0);
		}
	}
}

static void
idle_event_cb(
	enum app_event_enum event,
	gpointer event_ptr, gint event_int)
{
	switch(event) {
	case EVENT_MAIN_INIT:
		idle_check_timeout = 0;
		break;

	case EVENT_MAIN_REGISTER_PREFS:
		prefs_register(PREFS_IDLE_AUTO_AWAY, PREFS_TYPE_UINT,
			_("Auto-away timeout in minutes"), idle_prefs_auto_away_validator, NULL);
		prefs_register(PREFS_IDLE_AUTO_OFFLINE, PREFS_TYPE_UINT,
			_("Auto-offline timeout in minutes"),
			idle_prefs_auto_offline_validator, NULL);
		prefs_register(PREFS_IDLE_ENABLE, PREFS_TYPE_BOOL,
			_("Enter auto-away/auto-offline modes"), NULL, NULL);

		prefs_add_notifier(PREFS_IDLE_ENABLE, (GHookFunc)idle_prefs_idle_enable_changed);
		break;
		
	case EVENT_MAIN_PRESET_PREFS:
		prefs_set(PREFS_IDLE_AUTO_AWAY, 15);
		prefs_set(PREFS_IDLE_AUTO_OFFLINE, 30);
		prefs_set(PREFS_IDLE_ENABLE, TRUE);
		break;

	case EVENT_MAIN_START:
		/* start idle check periodical timeout (timer) */
		idle_mode = IDLE_NOT;
		break;
		
	case EVENT_MAIN_PRECLOSE:
		/* destroy timer source */
		if(idle_check_timeout>0) {
			g_source_remove(idle_check_timeout);
			idle_check_timeout = 0;
		}
		break;
		
	default:
		break;	/* nothing */
	}
}

/* exported routines
 *------------------ */

void idle_register()
{
	register_event_cb(idle_event_cb, EVENT_MAIN);
}

