/*
 * cmd_proc.c: command processor (/join, /leave, etc)
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
 * $Id: cmd_proc.c,v 1.5 2004/12/29 15:58:24 bobas Exp $
 */

#include <string.h>
#include <glib.h>

#include "prefs.h"
#include "main.h"
#include "sess.h"
#include "user.h"
#include "gui.h"
#include "cmd_proc.h"

#define LOG_CMDPROC	N_("Command: ")

/** struct defs */
typedef void (*cmd_handler_proc_t)(GString *);

#define CMD_ALIAS_MAX	2
struct cmd_handler_def_struct {
	const char 		*alias[CMD_ALIAS_MAX];

		/* note: cmd handler can modify the GString they get ptr to,
		 *	though they cannot free it
		*/
	cmd_handler_proc_t	proc;
};

/** forward refs
  **********************/
static GString * extract_first_word(GString *);

static void cmd_quit(GString *);
static void cmd_info(GString *);
static void cmd_query(GString *);
static void cmd_join(GString *);
static void cmd_close(GString *);
static void cmd_me(GString *);
static void cmd_nickname(GString *);
static void cmd_topic(GString *);

/** static vars
  **********************/
static GList * hist_list,
		* hist_current;
static int hist_size;

static struct cmd_handler_def_struct
cmd_handlers[] =
{
	{{ "exit",	"quit"	},		cmd_quit	},
	{{ "info",	NULL	},		cmd_info	},
	{{ "query",	"q"	},		cmd_query	},
	{{ "join",	"j"	},		cmd_join	},
	{{ "close"	"leave"	},		cmd_close	},
	{{ "me",	NULL	},		cmd_me		},
	{{ "nickname",	"nick"	},		cmd_nickname	},
	{{ "topic",	NULL	},		cmd_topic	},
	{{ NULL }}
};

/** static routines
  **********************/

/** chop_str:
  *	removes leading and trailing blanks
  * returns:
  *	ptr to the same str
  */
GString *
chop_str(GString * str)
{
	char * s;

	g_assert(str && str->str);

	/* find beginning of leading blanks */
	for(s=str->str; *s==' '; s++) ;
	if(!*s) {
		/* only blanks found:
		 * return empty string */
		g_string_erase(str, 0, -1);
		return str;
	}

	/* remove leading blanks */
	g_string_erase(str, 0, s - str->str);

	/* find beginning of trailing blanks */
	for(s = str->str + str->len - 1; *s==' '; s--) ;

	/* remove trailing blanks */
	g_string_erase(str, s - str->str+1, -1);

	return str;
}

/** check_channel_name:
  *	checks & modifies channel name (if needed)
  *	(str can be specified as NULL, 0 returned in that case)
  */
static int
check_channel_name(GString * str)
{
	if(!str || str->len<2
		|| (str->len > MAX_CHAN_LENGTH + (unsigned)(str->str[0]=='#'))
		|| strchr(str->str+1, '#')
		|| strchr(str->str, ' ')
	  ) return 0;

	if(str->str[0]!='#') {
		g_string_prepend_c(str, '#');
	}
	return 1;
}

/* cmd handlers */
static void
cmd_quit(GString *line_s)
{
	raise_event(EVENT_IFACE_EXIT, NULL, 0);
}

static void
cmd_info(GString *line_s)
{
	GString * nick;
	gpointer uid;

	g_assert(line_s);

	/* get the id of user we want to get info about */
	nick = extract_first_word(line_s);
	if(nick) {
		uid = user_by_name(nick->str);

		/* check if the user specified is valid nickname */
		if(!uid) {
			log_fevent(
				_(LOG_CMDPROC),
				g_strdup_printf(_("/info: \"%s\" is unknown user"), nick->str));
		} else {
			/* emit user-info-request event */
			raise_event(EVENT_IFACE_USER_INFO_REQ, uid, 0);
		}
		g_string_free(nick, TRUE);
	}
	else {
		/* no nickname specified */
		log_event(_(LOG_CMDPROC), _("/info: no nickname specified"));
	}
}

static void
cmd_query(GString *line_s)
{
	GString * nick;
	gpointer uid;

	g_assert(line_s);

	/* get the id of user we want to get info about */
	nick = extract_first_word(line_s);
	if(nick) {
		uid = user_by_name(nick->str);

		/* check if the user specified is valid nickname */
		if(!uid) {
			log_ferror(
				_(LOG_CMDPROC),
				g_strdup_printf(_("/query: \"%s\" is unknown user"), nick->str));
		} else {
			/* emit query-request event */
			raise_event(EVENT_IFACE_USER_OPEN_REQ, uid, 0);
		}
		g_string_free(nick, TRUE);
	}
	else {
		/* no nickname specified */
		log_error(_(LOG_CMDPROC), _("/query: no nickname specified"));
	}
}

static void
cmd_join(GString *line_s)
{
	GString * target;

	g_assert(line_s);

	target = extract_first_word(line_s);
	if(!check_channel_name(target)) {
		/* channel name unspecified */
		log_error(_(LOG_CMDPROC), _("/join: Invalid/unspecified channel"));
		if(target) g_string_free(target, TRUE);
		return;
	}

	raise_event(EVENT_IFACE_JOIN_CHANNEL, target->str, 0);

	g_string_free(target, TRUE);
}

static void
cmd_close(GString *line_s)
{
	g_assert(line_s);

	raise_event(EVENT_IFACE_PAGE_CLOSE, sess_current(), 0);
}

static void
cmd_me(GString * line_s)
{
	g_assert(line_s);

	chop_str(line_s);
	raise_event(EVENT_CMDPROC_SESSION_TEXT, (gpointer)chop_str(line_s)->str, 1);
}

static void
cmd_nickname(GString * line_s)
{
	GString * n;

	g_assert(line_s);

	n = extract_first_word(line_s);
	if(n) {
		raise_event(EVENT_IFACE_NICKNAME_ENTERED, (void*)n->str, 0);
		g_string_free(n, TRUE);
	}
}

static void
cmd_topic(GString * arg)
{
	gpointer sess;
	gpointer v[2];

	sess = sess_current();
	if(sess_type(sess)==SESSTYPE_CHANNEL) {
		v[0] = sess;
		v[1] = (gpointer)arg->str;

		raise_event(EVENT_IFACE_TOPIC_ENTER, v, 0);
	} else {
		log_event(_(LOG_CMDPROC), _("/topic: topic can be set on channel sessions only"));
	}
}

static void
history_free()
{
	GList * l;

	/* free strings */
	for(l = hist_list; l; l = l->next) {
		g_assert(l->data);
		g_free(l->data);
	}

	/* free list */
	if(hist_list) {
		g_list_free(hist_list);
		hist_list = NULL;
		hist_current = NULL;
		hist_size = 0;
	}
}

static void
history_append(const char * text)
{
	g_assert(text);

	/* check if we didn't max out number of history lines */
	if(hist_size==CMD_PROC_HISTORY_LINES) {
		/* remove first entry */
		g_assert(hist_list && hist_list->data);

		g_free(hist_list->data);
		hist_list = g_list_remove_link(hist_list, hist_list);
	} else {
		/* show history size */
		++ hist_size;
	}

	/* append new text at the entry */
	hist_list = g_list_append(hist_list, g_strdup(text));

	/* point current history after last */
	hist_current = NULL;
}

static void
history_show(int next)
{
	g_assert((hist_size && hist_list)
			|| (!hist_size && !hist_list));

	if(!hist_size) return;

	if(hist_current==0) {
		/* point to last */
		hist_current = g_list_last(hist_list);
	} else {
		if(next)
		{
			if(hist_current->next==NULL) {
				/* nowhere to go */
				return;
			}
			hist_current = hist_current->next;
		} else {
			if(hist_current->prev==NULL) {
				/* nowhere to go */
				return;
			}
			hist_current = hist_current->prev;
		}
	}
	
	/* update (gui) text */
	g_assert(hist_current->data);
	raise_event(
		EVENT_CMDPROC_SET_TEXT,
		hist_current->data, 0);
}


/** extract_first_word:
  *	does it
  *
  * modifies
  *	`from' GString
  * returns:
  *	1. new GString with `word' in it, or
  *	2. NULL, if no word could be extracted
  */
static GString *
extract_first_word(
	GString * from)
{
	GString * word;
	char * s, * b;

	g_assert(from && from->str);

	/* skip blanks */
	for(b = from->str; *b==' '; b++) ;

	if(*b=='\0') {
		/* no word to extract */
		return NULL;
	}

	/* skip word */
	for(s = b; *s!=' ' && *s!='\0'; s++) ;

	/* extract word */
	word = g_string_new(NULL);
	g_string_append_len(word, b, s - b);
	g_string_erase(from, 0, s - from->str);

	return word;
}

/** invoke_cmd_handler:
  *	finds and invokes handler for command
  */
static int 
invoke_cmd_handler(
	const char * cmd_name,
	GString * line_s) /* can be modified, though not freed, by cmd handler*/
{
	struct cmd_handler_def_struct * cmd_h;
	int a_nr;

	g_assert(cmd_name && line_s && *cmd_name);

	for(cmd_h = cmd_handlers; cmd_h->alias[0]!=NULL; cmd_h++)
	{
		for(a_nr=0; a_nr < CMD_ALIAS_MAX; a_nr++) {
			if(cmd_h->alias[a_nr] && !g_strcasecmp(cmd_h->alias[a_nr], cmd_name)) {
				g_assert(cmd_h->proc);
				cmd_h->proc(line_s);

				return TRUE;
			}
		}
	}

	/* obviously we found no handler
	   if we managed to get till here */

	return FALSE;
}

/** process_command:
  *	processes user command
  */
static void
process_command(const char * text)
{
	GString * line_s, * cmd_s;
	g_assert(text);

	line_s = g_string_new(text);

	cmd_s = extract_first_word(line_s);
	if(cmd_s==NULL) {
		/* "empty" cmd, e.g. `/ mistype param1 param2',
		 * and not `/correct param1 param2'
		 */
		log_error(_(LOG_CMDPROC), _("Invalid (empty) command \"/\""));
		g_string_free(line_s, TRUE);
		return;
	}

	/* find command handler or report error */
	if(! invoke_cmd_handler(cmd_s->str, line_s)) {
		/* invalid/unknown command */
		log_ferror(_(LOG_CMDPROC),
			g_strdup_printf(_("Invalid/unknown command \"%s\""), cmd_s->str));
	}

	g_string_free(line_s, TRUE);
	g_string_free(cmd_s, TRUE);
}

/** parse_text:
  *	parses text, which user entered into text box
  */
static gboolean
parse_text(const char * orig_text)
{
	GString * text = g_string_new(orig_text);

	/* remove leading & trailing blanks */
	chop_str(text);

	/* string carries no info, just blanks */
	if(!text->len) {
		g_string_free(text, TRUE);
		return FALSE;
	}

	/* check if it is a command */
	if(text->str[0]==CMD_PROC_COMMAND_CHAR) {
		process_command(text->str+1);
	}
      	else {
		/* text destined to current session */
		raise_event(EVENT_CMDPROC_SESSION_TEXT, (gpointer)text->str, 0);
	}

	g_string_free(text, TRUE);
	return TRUE;
}

static void
text_entered(
	const char * text)
{
	g_assert(text);

	if(!parse_text(text)) {
		/* invalid, probably an empty, line: ignore */
		return;
	}

	/* insert this thing into history */
	history_append(text);

	/* clear current text */
	raise_event(EVENT_CMDPROC_SET_TEXT, "", 0);
}

static void
cmd_proc_event_cb(
	enum app_event_enum e,
	gpointer p, gint i)
{
	switch(e) {
	case EVENT_MAIN_INIT:
		hist_current = hist_list = NULL;
		hist_size = 0;
		break;
	case EVENT_MAIN_CLOSE:
		history_free();
		break;
	case EVENT_IFACE_TEXT_ENTER:
		text_entered((const char*)p);
		break;
	case EVENT_IFACE_HISTORY:
		history_show(i);
		break;
	default:
		break;	/* nothing else */
	}
}

/** exported routines
  **********************/

void cmd_proc_register()
{
	register_event_cb(cmd_proc_event_cb, EVENT_MAIN | EVENT_IFACE);
}

