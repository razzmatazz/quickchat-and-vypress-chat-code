/*
 * main.c: application initialization, misc funcs
 * Copyright (C) 2002,2004 Saulius Menkevicius
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
 * $Id: main.c,v 1.34 2005/01/05 12:32:43 bobas Exp $
 */

#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include <glib.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_STARTUP_NOTIFICATION
# define SN_API_NOT_YET_FROZEN
# include <libsn/sn-launchee.h>
# include <gdk/gdk.h>
# include <gdk/gdkx.h>
#endif

#include "main.h"
#include "prefs.h"
#include "user.h"
#include "sess.h"
#include "gui.h"
#include "net.h"
#include "cmd_proc.h"
#include "idle.h"
#include "util.h"

/** struct defs
  ***************/

typedef struct event_callback_struct {
	enum app_event_enum mask;
	app_event_cb callback;
} event_callback_t;
#define PEVENT_CALLBACK_T(p) ((event_callback_t*)p)

/** static vars
  ***************/
static GList * event_cb_list;
static sess_id status_sess, main_chan;
static enum app_status_enum status;

/* pre_idle_user_mode:
 *	user mode to return to, if the user awakes
 * and we were in auto-away/auto-offline mode
 * (if pre_idle_user_mode=UMODE_UNKNOWN,
 *  we are not in auto-away/auto-offline mode)
 */
static enum user_mode_enum pre_idle_user_mode;

#ifdef HAVE_STARTUP_NOTIFICATION
static SnLauncheeContext * sn_context = NULL;
static SnDisplay * sn_display = NULL;
#endif

/* static funcs
 **************/

#ifdef HAVE_STARTUP_NOTIFICATION
static void
sn_error_trap_push(SnDisplay * display, Display * xdisplay)
{
	gdk_error_trap_push();
}

static void
sn_error_trap_pop(SnDisplay * display, Display * xdisplay)
{
	gdk_error_trap_pop();
}

static void
startup_notification_complete()
{
	Display * xdisplay;
	xdisplay = GDK_DISPLAY();
	sn_display = sn_display_new(xdisplay, sn_error_trap_push, sn_error_trap_pop);
	sn_context = sn_launchee_context_new_from_environment(sn_display, DefaultScreen(xdisplay));
	if(sn_context) {
		sn_launchee_context_complete(sn_context);
		sn_launchee_context_unref(sn_context);

		sn_display_unref(sn_display);
	}
}
#endif /* HAVE_STARTUP_NOTIFICATION */

static void
connected_to_network()
{
	GList * persistent;
	
	/* open the #main channel */
	main_chan = sess_new(SESSTYPE_CHANNEL, "Main", FALSE, NULL);
	if(!main_chan) {
		/* maybe we have the "#Main" session opened already? */
		main_chan = sess_find(SESSTYPE_CHANNEL, "Main");
	}

	/* open persistent channels, that is channels the
	 * user has specified to be opened on startup
	 */
	persistent = prefs_list(PREFS_MAIN_PERSISTENT_CHANNELS);
	for(; persistent; persistent = persistent->next) {
		if(g_utf8_strlen((const gchar*)persistent->data, -1))
			sess_new(SESSTYPE_CHANNEL, (const gchar*)persistent->data, TRUE, NULL);
	}
	
	/* switch to the #Main channel */
	sess_switch_to(main_chan);
}

static void
beep_received_from(gpointer user)
{
	gpointer session = util_session_for_notifies_from(user);
	if(session)
		sess_write(
			session, SESSTEXT_NOTIFY,
			_("%s has sent you a beep alert"), user_name_of(user));
}

static void 
beep_ack_received_from(gpointer user)
{
	gpointer session = util_session_for_notifies_from(user);
	if(session)
		sess_write(
			session, SESSTEXT_NOTIFY,
			_("%s has received your beep alert"), user_name_of(user));
}

static gboolean
main_prefs_nickname_validator(const gchar * prefs_name, gpointer user_data)
{
	if(g_utf8_strlen(prefs_str(prefs_name), -1)==0) {
		log_error(NULL, _("Can not set nickname to an empty string"));
		return FALSE;
	}
	return TRUE;
}

static gboolean
main_prefs_mode_validator(const gchar * prefs_name, gpointer user_data)
{
	return prefs_int(prefs_name) < UMODE_NUM;
}

static void
main_prefs_main_nickname_changed_cb(const gchar * prefs_name)
{
	if(status==APP_RUNNING) {
		log_fevent(NULL, g_strdup_printf(
			_("Renamed to %s"), prefs_str(PREFS_MAIN_NICKNAME)));
	}
}

static void
main_prefs_main_mode_changed_cb(const gchar * prefs_name)
{
	if(status==APP_RUNNING) {
		log_fevent(NULL, g_strdup_printf(
			_("Changed mode to %s"), user_mode_name(prefs_int(PREFS_MAIN_MODE))));
	}
}

static void
main_register_prefs()
{
	/* register preferences */
	prefs_register(PREFS_MAIN_NICKNAME,	PREFS_TYPE_STR,
		_("Nickname"), main_prefs_nickname_validator, NULL);
	prefs_register(PREFS_MAIN_MODE,		PREFS_TYPE_UINT,
		_("User mode"), main_prefs_mode_validator, NULL);
	prefs_register(PREFS_MAIN_LOG_GLOBAL,	PREFS_TYPE_BOOL,
		_("Log to current page"), NULL, NULL);
	prefs_register(PREFS_MAIN_PERSISTENT_CHANNELS, PREFS_TYPE_LIST,
		_("Persistent channels"), NULL, NULL);
	prefs_register(PREFS_MAIN_POPUP_TIMEOUT,PREFS_TYPE_UINT,
		_("Popup timeout in miliseconds"), NULL, NULL);
	
	prefs_add_notifier(PREFS_MAIN_MODE, (GHookFunc)main_prefs_main_mode_changed_cb);
	prefs_add_notifier(PREFS_MAIN_NICKNAME, (GHookFunc)main_prefs_main_nickname_changed_cb);
}

static void
main_event_cb(
	enum app_event_enum event,
	gpointer p, gint i)
{
	gchar * val;

	switch(event) {
	case EVENT_MAIN_INIT:
		status_sess = NULL;
		main_chan = NULL;
		pre_idle_user_mode = UMODE_NULL;
		break;

	case EVENT_MAIN_REGISTER_PREFS:
		main_register_prefs();
		break;

	case EVENT_MAIN_PRESET_PREFS:
		val = g_strdup_printf(_("Guest_%hu"), rand()&0xffff);
		prefs_set(PREFS_MAIN_NICKNAME, val);
		g_free(val);

		prefs_set(PREFS_MAIN_MODE, UMODE_NORMAL);
		prefs_set(PREFS_MAIN_POPUP_TIMEOUT, 500);
		break;
		
	case EVENT_MAIN_START:
		status_sess = sess_new(SESSTYPE_STATUS, _("Status"), FALSE, NULL);
		sess_write(status_sess, SESSTEXT_NORMAL, _("%s version %s"), PACKAGE, VERSION);
		sess_write(status_sess, SESSTEXT_NORMAL,
			_("This program comes with ABSOLUTELY NO WARRANTY. Licensed under the GPL."));
		sess_write(status_sess, SESSTEXT_NORMAL,
			_("This is free software, and you are welcome to redistribute it \
under certain conditions."));

#ifdef HAVE_STARTUP_NOTIFICATION
		/* notify desktop environment/wm that we've started */
		startup_notification_complete();
#endif
		break;
		
	case EVENT_MAIN_PRECLOSE:
		main_chan = status_sess = NULL;
		break;

	case EVENT_IFACE_EXIT:
		gui_shutdown();
		break;
	case EVENT_IFACE_NICKNAME_ENTERED:
		if(nickname_cmp(my_nickname(), (const char *)p))
			set_nickname((const char *)p);
		break;
	case EVENT_IDLE_AUTO_AWAY:
		/* get into away mode, if not in offline currently */
		if (pre_idle_user_mode==UMODE_NULL) {
			/* save pre-auto-away user mode */
			pre_idle_user_mode = prefs_int(PREFS_MAIN_MODE);

			if(pre_idle_user_mode!=UMODE_OFFLINE)
				prefs_set(PREFS_MAIN_MODE, UMODE_AWAY);
		}
		break;

	case EVENT_IDLE_AUTO_OFFLINE:
		/* get into offline mode */
		if (pre_idle_user_mode==UMODE_NULL) {
			/* save pre-auto-offline user mode */
			pre_idle_user_mode = prefs_int(PREFS_MAIN_MODE);
		}

		/* get into offline mode */
		prefs_set(PREFS_MAIN_MODE, UMODE_OFFLINE);
		break;

	case EVENT_IDLE_NOT:
		/* return to previous user mode */
		prefs_set(PREFS_MAIN_MODE, pre_idle_user_mode);

		/* notify, that we're no longer in auto-away/offline mode */
		pre_idle_user_mode = UMODE_NULL;
		break;

	case EVENT_NET_CONNECTED:
		connected_to_network();
		break;
	
	case EVENT_NET_MSG_BEEP_SEND:
		beep_received_from(p);
		break;

	case EVENT_NET_MSG_BEEP_ACK:
		beep_ack_received_from(p);
		break;

	default:
		break;	/* nothing else */
	}
}

/** exported routines
  ***************************/

enum app_status_enum
app_status()
{
	return status;
}

void register_event_cb(
	app_event_cb event_cb,
	enum app_event_enum event_mask)
{
	event_callback_t * cb;

	g_assert(event_cb && event_mask);

	/* setup new callback struct */
	cb = g_malloc(sizeof(event_callback_t));
	cb->callback = event_cb;
	cb->mask = event_mask;

	/* insert into cb list */
	event_cb_list = g_list_append(event_cb_list, cb);
}

void raise_event(
	enum app_event_enum event,
	void * p, int i)
{
	GList * l;

	/* ensure there's real event out there */
	g_assert(event & 0xff);

	for(l = event_cb_list; l; l = l->next) {
		if(event & PEVENT_CALLBACK_T(l->data)->mask)
			PEVENT_CALLBACK_T(l->data)->callback(event, p, i);
	}
}

/* WIN32 compatibility hacks:
 *	mingw prefers main(), instead of WinMain();
 *	(thus we rename main to _main, in order to link to WinMain)
 */
#ifdef _WIN32
int _main
#else
int main
#endif
	(int argc,
	char ** argv,
	char ** env	)
{
	/* setup callback struct */
	event_cb_list = NULL;
	register_event_cb(main_event_cb, EVENT_NET | EVENT_IFACE | EVENT_MAIN | EVENT_IDLE);

	/* to make use of electric fence */
	free(malloc(8));

#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	bindtextdomain(GETTEXT_PACKAGE, LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif

	/* register "subsystems" */
	prefs_register_module(argc, argv, env);
	gui_register(&argc, &argv);
	net_register();
	sess_register();
	user_register_module();
	cmd_proc_register();
	idle_register();

	/* init application */
	status = APP_INIT;
	raise_event(EVENT_MAIN_INIT, 0, 0);
	raise_event(EVENT_MAIN_REGISTER_PREFS, 0, 0);
	raise_event(EVENT_MAIN_PRESET_PREFS, 0, 0);
	raise_event(EVENT_MAIN_LOAD_PREFS, 0, 0);
	status = APP_START;
	raise_event(EVENT_MAIN_START, 0, 0);
	status = APP_RUNNING;

	gui_run();

	status = APP_PRECLOSE;
	raise_event(EVENT_MAIN_PRECLOSE, 0,0);
	status = APP_CLOSE;
	raise_event(EVENT_MAIN_CLOSE, 0,0);

	/* cleanup event callback structs */
	util_list_free_with_data(event_cb_list, (GDestroyNotify)g_free);

	return 0;
}

/* log_event_nofree:
 *	prints the error message (to stdout/Status session)
 */
void log_event_nofree(const gchar * error_domain, const gchar * error_str, int is_error)
{
	sess_id active_sess;
	gchar * str;

	g_assert(error_str!=0);

	if(status_sess) {
		if(error_domain) {
			str = g_strdup_printf("%s: %s", error_domain, error_str);
		} else {
			str = (gchar*)error_str;
		}

		/* emit error to status tab */
		sess_write(status_sess, is_error ? SESSTEXT_ERROR: SESSTEXT_NORMAL, str);

		/* and to the active session tab, if configured so */
		active_sess = sess_current();
		if(prefs_bool(PREFS_MAIN_LOG_GLOBAL) && active_sess!=status_sess) {
			sess_write(active_sess, SESSTEXT_NOTIFY, str);
		}

		if(str!=error_str) {
			g_free(str);
		}
	}
}

/* log_event_wfree:
 *	do log_event_wfree() on the message, and then
 *	free the string supplied.
 *	(for use with g_sprintf, etc)
 */
void log_event_withfree(const gchar * error_domain, gchar * str, int is_error)
{
	g_assert(str!=NULL);

	log_event_nofree(error_domain, str, is_error);
	g_free(str);
}

gchar *
generate_timestamp()
{
	time_t epoch_time;
	struct tm * local;

	/* get current time */
	epoch_time = time(NULL);
	local = localtime(&epoch_time);

	/* format time string */
	return g_strdup_printf("[%02d:%02d] ", local->tm_hour, local->tm_min);
}

static void
my_channels_enum_cb(
	sess_id session, enum session_type type,
	const gchar * name, gpointer user,
	GList ** plist)
{
	if(type==SESSTYPE_CHANNEL)
		*plist = g_list_append(*plist, (gpointer)name);
}

/* my_channels:
 *	returns list of user's active channels
 */
GList * my_channels()
{
	GList * channels = NULL;
	sess_enumerate((sess_enum_cb)my_channels_enum_cb, (gpointer)&channels);
	return channels;
}

int nickname_valid(const char * n)
{
	g_assert(n);

	/* TODO: replace with something saner */
	return *n!='\0';
}

int channel_cmp(const char * c1, const char * c2)
{
	gchar * nocase_c1, * nocase_c2;
	gint cmp_value;

	g_assert(c1 && c2);

	/* generate no-case variants of n1/n2 */
	nocase_c1 = g_utf8_casefold(c1, -1);
	nocase_c2 = g_utf8_casefold(c2, -1);

	/* compare the strings */
	cmp_value = g_utf8_collate(nocase_c1, nocase_c2);

	/* free the collated strings */
	g_free(nocase_c1);
	g_free(nocase_c2);

	return cmp_value;
}

int channel_valid(const char * c)
{
	g_assert(c);

	/* TODO: replace with something saner */
	return *c!='\0';
}

