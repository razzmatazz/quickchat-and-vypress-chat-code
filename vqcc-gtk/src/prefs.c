/*
 * prefs.c: configuration routines
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
 * $Id: prefs.c,v 1.35 2004/12/21 15:11:37 bobas Exp $
 */

#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "main.h"
#include "prefs.h"
#include "net.h"
#include "user.h"
#include "util.h"
#include "gui_tray.h"

#define LOG_PREFS	N_("Preferences")
#define PREFS_FILE_NAME	".vqcc.conf.xml"

enum prefs_parser_tag {
	PREFS_TAG_NO_TAG,
	PREFS_TAG_VQCC_GTK,
	PREFS_TAG_VQCC_GTK_SETTINGS,
	PREFS_TAG_VQCC_GTK_SETTINGS_PREF,
	PREFS_TAG_VQCC_GTK_SETTINGS_PREF_ENTRY
};

struct prefs_parser_state {
	enum prefs_parser_tag tag;
	gboolean pref_valid;
	GString * pref_name;
	enum prefs_type pref_type;
};

struct prefs_value {
	gchar * description;
	enum prefs_type type;
	union prefs_value_union {
		gboolean boolean;
		gchar * string;
		guint integer;
		GList * list;
	} value;
	
	prefs_validator_func * validator_func;
	gpointer validator_user_data;

	GHookList change_hook;
};

/* static variables */
static gchar * prefs_file_path;
static GHashTable * prefs_hash;
static gboolean prefs_values_saved;

/* forward references */
static gboolean prefs_load();
static gboolean prefs_store();

/** static routines
  *****************/

/* prefs_free_value:
 *	frees data in the internal value struct
 */
static void
prefs_free_value(
	enum prefs_type value_type, union prefs_value_union * value)
{
	switch(value_type) {
	case PREFS_TYPE_LIST:
		util_list_free_with_data(value->list, (GDestroyNotify)g_free);
		break;
	case PREFS_TYPE_STR:
		g_free(value->string);
		break;
	default:
		break;
	}
}

/* prefs_destroy_value_cb:
 *	destroys preference value descriptor in prefs hash table
 */
static void
prefs_destroy_value_cb(struct prefs_value * value)
{
	g_assert(value && value->description);

	g_free(value->description);
	prefs_free_value(value->type, &value->value);
	g_hook_list_clear(&value->change_hook);
	g_free(value);
}

/* prefs_init:
 *	initializes preference module
 */
static void
prefs_init()
{
	prefs_values_saved = FALSE;

	/* setup prefs hash */
	prefs_hash = g_hash_table_new_full(
		(GHashFunc)g_str_hash, (GEqualFunc)g_str_equal,
		(GDestroyNotify)g_free, (GDestroyNotify)prefs_destroy_value_cb);
}

/* prefs_free:
 *	free any variables we might have allocated in
 *	config module since the start of application
 */
static void
prefs_free()
{
	g_hash_table_destroy(prefs_hash);
	prefs_hash = NULL;

	if(prefs_file_path) {
		g_free(prefs_file_path);
		prefs_file_path = NULL;
	}
}

static void
prefs_event_cb(
	enum app_event_enum e,
	gpointer p, gint i)
{
	switch(e) {
	case EVENT_MAIN_INIT:
		prefs_init();
		break;

	case EVENT_MAIN_REGISTER_PREFS:
		prefs_register(PREFS_PREFS_AUTO_SAVE, PREFS_TYPE_BOOL,
			_("Save settings on exit"), NULL, NULL);
		break;

	case EVENT_MAIN_PRESET_PREFS:
		prefs_set(PREFS_PREFS_AUTO_SAVE, TRUE);
		break;

	case EVENT_MAIN_LOAD_PREFS:
		prefs_load();
		break;

	case EVENT_MAIN_CLOSE:
		/* save configuration */
		if(prefs_bool(PREFS_PREFS_AUTO_SAVE))
			prefs_store();
		prefs_free();
		break;

	case EVENT_IFACE_RELOAD_CONFIG:
		/* force reload of config */
		prefs_load();
		break;

	case EVENT_IFACE_STORE_CONFIG:
		prefs_store();
		break;
	default:
		break;
	}
}

/** exported routines
  *******************/

void prefs_register_module(gint argc, gchar ** argv, gchar ** env)
{
	const gchar * home;
	gchar ** p;

	register_event_cb(prefs_event_cb, EVENT_MAIN|EVENT_IFACE);

	/* get home path */
	prefs_file_path = NULL;

	/* check if configuration file was specified */
	for(p=argv+1; *p; p++)
		if(!g_strcasecmp(*p, "-c") && *(p+1)!=NULL) {
			prefs_file_path = g_strdup(*(p+1));
			break;
		}

	/* get & store UNIX home path */
	home = g_get_home_dir();
	if(!prefs_file_path)
		prefs_file_path = g_strdup_printf("%s/"PREFS_FILE_NAME, home ? home: "");
}

void prefs_register(
	const gchar * name,
	enum prefs_type type,
	const gchar * description,
	prefs_validator_func * validator_func,
	gpointer validator_user_data)
{
	struct prefs_value * value;

	g_assert(name && description);
	g_assert(prefs_hash != NULL);

	/* setup preference value struct */
	value = g_new(struct prefs_value, 1);

	value->description = g_strdup(description);
	value->type = type;
	switch(type) {
	case PREFS_TYPE_BOOL:
		value->value.boolean = FALSE;
		break;
	case PREFS_TYPE_STR:
		value->value.string = g_strdup("");
		break;
	case PREFS_TYPE_UINT:
		value->value.integer = 0;
		break;
	case PREFS_TYPE_LIST:
		value->value.list = NULL;
		break;
	}

	value->validator_func = validator_func;
	value->validator_user_data = validator_user_data;

	g_hook_list_init(&value->change_hook, sizeof(GHook));

	/* add it into the prefs hash */
	g_hash_table_insert(prefs_hash, g_strdup(name), value);
}

/* prefs_add_notifier:
 *	registers pref change notifier
 */
void prefs_add_notifier(const gchar * pref_name, GHookFunc func)
{
	GHook * hook;
	struct prefs_value * value;
	const gchar * value_key;
	gboolean found_pref;

	found_pref = g_hash_table_lookup_extended(
			prefs_hash, pref_name, (gpointer*)&value_key, (gpointer*)&value);
	g_assert(found_pref);

	/* setup new hook for this preference */
	hook = g_hook_alloc(&value->change_hook);
	hook->func = func;
	hook->data = (gpointer)value_key;

	g_hook_prepend(&value->change_hook, hook);
}

/* prefs_write_xml_pref:
 *	stores specified value to the disk
 */
static void 
prefs_write_xml_pref(const gchar * prefs_name, struct prefs_value * value, FILE * prefs_f)
{
	gchar * esc;
	GList * entry;

	g_assert(prefs_name && value && prefs_f);

#define WRITE_ESCAPED(format, ...) \
	do { \
		esc = g_markup_printf_escaped(format, __VA_ARGS__); \
		fputs(esc, prefs_f); \
		g_free(esc); \
	} while(0);

	WRITE_ESCAPED("\t\t<pref name=\"%s\"", prefs_name);

	switch(value->type) {
	case PREFS_TYPE_UINT:
		WRITE_ESCAPED(" type=\"uint\">%u</pref>\n", value->value.integer);
		break;
		
	case PREFS_TYPE_BOOL:
		WRITE_ESCAPED(" type=\"bool\">%s</pref>\n",
				value->value.boolean ? "TRUE": "FALSE");
		break;
		
	case PREFS_TYPE_STR:
		WRITE_ESCAPED(" type=\"string\">%s</pref>\n", value->value.string);
		break;
		
	case PREFS_TYPE_LIST:
		fputs(" type=\"list\">", prefs_f);
		if(value->value.list) {
			fputs("\n", prefs_f);
			
			for(entry = value->value.list; entry; entry = entry->next)
				WRITE_ESCAPED("\t\t\t<entry>%s</entry>\n",
						(const gchar*)entry->data);

			fputs("\t\t", prefs_f);
		}
		fputs("</pref>\n", prefs_f);
		break;
	}
}

static void
prefs_write_xml_sort_prefs_cb(const gchar * prefs_name, struct prefs_value * value, GList ** list)
{
	*list = g_list_insert_sorted(*list, (gpointer)prefs_name, (GCompareFunc)g_utf8_collate);
}

static void
prefs_write_xml_to(FILE * prefs_f)
{
	GList * sorted, * entry;

	fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<vqcc_gtk>\n"
		"\t<settings>\n",
		prefs_f);

	/* sort the preferences */
	sorted = NULL;
	g_hash_table_foreach(prefs_hash, (GHFunc)prefs_write_xml_sort_prefs_cb, (gpointer)&sorted);

	for(entry = sorted; entry; entry = entry->next)
		prefs_write_xml_pref(
			(const gchar*) entry->data,
			g_hash_table_lookup(prefs_hash, entry->data),
			prefs_f);

	g_list_free(sorted);

	fputs("\t</settings>\n"
		"</vqcc_gtk>",
		prefs_f);
}


/* prefs_store:
 *	save configuration settings to configuration file
 */
static gboolean
prefs_store()
{
	FILE * cf;

	/* open config file */
	cf = fopen(prefs_file_path, "wb");
	if(!cf) {
		log_ferror(_(LOG_PREFS), g_strdup_printf(
			_("Cannot save configuration settings to file \"%s\": %s"),
			prefs_file_path, strerror(errno)));
	} else {
		/* store settings to file */
		prefs_write_xml_to(cf);
		fclose(cf);

		log_fevent(_(LOG_PREFS), g_strdup_printf(
			_("Configuration settings were stored in \"%s\""),
			prefs_file_path));
	}

	/* config values now are in sync with those on the disk */
	prefs_values_saved = TRUE;
	raise_event(EVENT_PREFS_SAVED, NULL, 0);
	
	return TRUE;
}

static gboolean
prefs_get_type_by_name(const gchar * type_name, enum prefs_type * type)
{
	gboolean found = TRUE;
	
	if(!g_utf8_collate(type_name, "uint"))
		*type = PREFS_TYPE_UINT;
	else if(!g_utf8_collate(type_name, "bool"))
		*type = PREFS_TYPE_BOOL;
	else if(!g_utf8_collate(type_name, "string"))
		*type = PREFS_TYPE_STR;
	else if(!g_utf8_collate(type_name, "list"))
		*type = PREFS_TYPE_LIST;
	else
		found = FALSE;

	return found;
}

static void
prefs_load_xml_start_element(
	GMarkupParseContext * context,
	const gchar * element,
	const gchar ** attribute_names, const gchar ** attribute_values,
	gpointer user_data,
	GError ** error)
{
	struct prefs_parser_state * state = (struct prefs_parser_state*)user_data;

	if(!g_utf8_collate(element, "vqcc_gtk") && state->tag==PREFS_TAG_NO_TAG) {
		state->tag = PREFS_TAG_VQCC_GTK;
	}
	else if(!g_utf8_collate(element, "settings") && state->tag==PREFS_TAG_VQCC_GTK) {
		state->tag = PREFS_TAG_VQCC_GTK_SETTINGS;
	}
	else if(!g_utf8_collate(element, "pref") && state->tag==PREFS_TAG_VQCC_GTK_SETTINGS) {
		struct prefs_value * val;
		const gchar ** attr, ** value;
		const gchar * prefs_type_attr;

		state->tag = PREFS_TAG_VQCC_GTK_SETTINGS_PREF;

		/* fetch pref attributes */
		g_string_assign(state->pref_name, "");
		for(attr = attribute_names, value = attribute_values; *attr; attr++, value++) {
			if(!g_utf8_collate(*attr, "name"))
				g_string_assign(state->pref_name, *value);

			if(!g_utf8_collate(*attr, "type"))
				prefs_type_attr = *value;
		}
		
		/* check if we have a valid preference name and type */
		state->pref_valid = FALSE;
		val = (struct prefs_value*)g_hash_table_lookup(prefs_hash, state->pref_name->str);

		if(val && prefs_get_type_by_name(prefs_type_attr, &state->pref_type)) {
			if(state->pref_type==val->type)
				state->pref_valid = TRUE;
		}
	}
	else if(!g_utf8_collate(element, "entry") && state->tag==PREFS_TAG_VQCC_GTK_SETTINGS_PREF) {
		state->tag = PREFS_TAG_VQCC_GTK_SETTINGS_PREF_ENTRY;
	}
}

static void
prefs_load_xml_end_element(
	GMarkupParseContext * context,
	const gchar * element,
	gpointer user_data,
	GError ** error)
{
	struct prefs_parser_state * state = (struct prefs_parser_state*)user_data;

	if(!g_utf8_collate(element, "vqcc_gtk")
			&& state->tag==PREFS_TAG_VQCC_GTK) {
		state->tag = PREFS_TAG_NO_TAG;
	}
	else if(!g_utf8_collate(element, "settings")
			&& state->tag==PREFS_TAG_VQCC_GTK_SETTINGS) {
		state->tag = PREFS_TAG_VQCC_GTK;
	}
	else if(!g_utf8_collate(element, "pref")
			&& state->tag==PREFS_TAG_VQCC_GTK_SETTINGS_PREF) {
		state->tag = PREFS_TAG_VQCC_GTK_SETTINGS;
	}
	else if(!g_utf8_collate(element, "entry")
			&& state->tag==PREFS_TAG_VQCC_GTK_SETTINGS_PREF_ENTRY) {
		state->tag = PREFS_TAG_VQCC_GTK_SETTINGS_PREF;
	}
}

static void
prefs_load_xml_text(
	GMarkupParseContext * context,
	const gchar * text,
	gsize text_len,
	gpointer user_data,
	GError ** error)
{
	struct prefs_parser_state * state = (struct prefs_parser_state*)user_data;

	if(!state->pref_valid)
		return;

	if(state->tag==PREFS_TAG_VQCC_GTK_SETTINGS_PREF) {
		gchar * stripped;
		guint uint_read;

		switch(state->pref_type) {
		case PREFS_TYPE_UINT:
			if(sscanf(text, "%u", &uint_read)==1) {
				prefs_set(state->pref_name->str, uint_read);
			} else {
				g_set_error(error, G_MARKUP_ERROR,
					G_MARKUP_ERROR_INVALID_CONTENT,
					_("Could not read value for preference \"%s\""),
						state->pref_name->str);
			}
			break;
		case PREFS_TYPE_BOOL:
			stripped = g_strstrip(g_strdup(text));
			if(!g_ascii_strncasecmp(text, "FALSE", sizeof("FALSE")-1)) {
				prefs_set(state->pref_name->str, FALSE);
			}
			else if(!g_ascii_strncasecmp(text, "TRUE", sizeof("TRUE")-1)) {
				prefs_set(state->pref_name->str, TRUE);
			}
			else {
				g_set_error(error, G_MARKUP_ERROR,
					G_MARKUP_ERROR_INVALID_CONTENT,
					_("Could not read value for preference \"%s\""),
						state->pref_name->str);
			}
			g_free(stripped);
			break;
		case PREFS_TYPE_STR:
			prefs_set(state->pref_name->str, text);
			break;
		default:
			break;
		}
	} else if(state->tag==PREFS_TAG_VQCC_GTK_SETTINGS_PREF_ENTRY
			&& state->pref_type==PREFS_TYPE_LIST) {
		/* read list data */
		prefs_list_add(state->pref_name->str, text);
	}
}

static void
prefs_free_parser_state(struct prefs_parser_state * state)
{
	g_string_free(state->pref_name, TRUE);
	g_free(state);
}

/* prefs_load_xml_from:
 *	creates GMarkupParserContext for parsing the prefs
 */
static void
prefs_load_xml_from(const gchar * prefs_filename, FILE * prefs_f)
{
	GMarkupParser parser;
	GMarkupParseContext * context;
	gchar * buf;
	gsize buf_bytes;
	GError * error = NULL;
	struct prefs_parser_state * state;

	parser.start_element = prefs_load_xml_start_element;
	parser.end_element = prefs_load_xml_end_element;
	parser.text = prefs_load_xml_text;
	parser.passthrough = NULL;
	parser.error = NULL;

	state = g_new(struct prefs_parser_state, 1);
	state->tag = PREFS_TAG_NO_TAG;
	state->pref_name = g_string_new(NULL);

	context = g_markup_parse_context_new(
		&parser, 0, (gpointer)state,
		(GDestroyNotify)prefs_free_parser_state);

	buf = g_malloc(4096);

	while(!feof(prefs_f)) {
		buf_bytes = fread(buf, 1, 4096, prefs_f);
		if(buf_bytes < 4096 && ferror(prefs_f)) {
			log_ferror(_(LOG_PREFS),
				g_strdup_printf(_("Error while reading "
						"configuration file \"%s\": %s"),
					prefs_filename, strerror(errno)));

			goto bail_out;
		}

		if(buf_bytes)
			if(g_markup_parse_context_parse(context, buf, buf_bytes, &error)==FALSE)
				goto parse_error;
	}

	if(g_markup_parse_context_end_parse(context, &error)==TRUE) {
		/* parse ok */
		goto bail_out;
	}

parse_error:
	log_ferror(_(LOG_PREFS),
		g_strdup_printf(_("Error parsing configuration from \"%s\": %s"),
			prefs_filename, error->message));

	g_error_free(error);

bail_out:
	g_free(buf);
	g_markup_parse_context_free(context);

}

/* prefs_load:
 *	load configuration settings from file
 */
static gboolean
prefs_load()
{
	FILE * config_file;

	/* parse main file */
	config_file = fopen(prefs_file_path, "r");
	if(!config_file) {
		log_ferror(_(LOG_PREFS), g_strdup_printf(
			_("Cannot open configuration file \"%s\" for reading: %s"),
			prefs_file_path, strerror(errno)));
		return FALSE;
	}

	prefs_load_xml_from(prefs_file_path, config_file);
	fclose(config_file);

	/* config values are in sync with those on disk */
	prefs_values_saved = TRUE;

	return TRUE;
}

gboolean
prefs_in_sync()
{
	return prefs_values_saved;
}

const gchar *
prefs_description(const gchar * prefs_name)
{
	struct prefs_value * value;
	value = g_hash_table_lookup(prefs_hash, prefs_name);

	g_return_val_if_fail(value!=NULL, "<Unknown preference>");

	return value->description;
}

void prefs_set(const gchar * prefs_name, ...)
{
	va_list ap;
	struct prefs_value * value;
	guint new_guint;
	gboolean new_gboolean;
	const gchar * new_string;

	va_start(ap, prefs_name);

	value = g_hash_table_lookup(prefs_hash, prefs_name);
	if(value) {
		gboolean validated;
		union prefs_value_union value_backup;

		/* backup the current value, if the validator decides the new one is invalid */
		value_backup = value->value;

		/* check if the old value hasn't changed
		 * and if so, set the new value */
		switch(value->type) {
		case PREFS_TYPE_UINT:
			new_guint = va_arg(ap, guint);
			if(value->value.integer!=new_guint)
				value->value.integer = new_guint;
			else
				goto no_change;
			break;
		case PREFS_TYPE_BOOL:
			new_gboolean = va_arg(ap, gboolean);
			if(value->value.boolean!=new_gboolean)
				value->value.boolean = new_gboolean;
			else
				goto no_change;
			break;
		case PREFS_TYPE_STR:
			new_string = va_arg(ap, gchar*);
			if(strcmp(value->value.string, new_string))
				value->value.string = g_strdup(new_string);
			else
				goto no_change;	
			break;
		case PREFS_TYPE_LIST:
			value->value.list = util_list_copy_with_data(
				va_arg(ap, GList *),
				(util_list_data_copy_func_t*)g_strdup);
			break;
		}

		/* invoke validator to check new value */
		validated = value->validator_func != NULL
			? value->validator_func(prefs_name, value->validator_user_data)
			: TRUE;

		if(validated) {
			/* free backup data */
			prefs_free_value(value->type, &value_backup);

			/* notify that we've changed to a new value */
			g_hook_list_invoke(&value->change_hook, FALSE);
			raise_event(EVENT_PREFS_CHANGED, (gpointer)prefs_name, 0);

			/* preferences are not in sync with those on the disk */
			prefs_values_saved = FALSE;
		} else {
			/* restore backup value
			 */
			prefs_free_value(value->type, &value->value);
			value->value = value_backup;
		}
	} else {
		log_ferror(_(LOG_PREFS), g_strdup_printf(
			_("Unknown preference value \"%s\""), prefs_name));
	}

no_change:
	va_end(ap);
}

guint prefs_int(const gchar * prefs_name)
{
	struct prefs_value * value = g_hash_table_lookup(prefs_hash, prefs_name);
	g_assert(value!=NULL && value->type==PREFS_TYPE_UINT);

	return value->value.integer;
}

gboolean prefs_bool(const gchar * prefs_name)
{
	struct prefs_value * value = g_hash_table_lookup(prefs_hash, prefs_name);
	g_assert(value!=NULL && value->type==PREFS_TYPE_BOOL);

	return value->value.boolean;
}

const gchar * prefs_str(const gchar * prefs_name)
{
	struct prefs_value * value = g_hash_table_lookup(prefs_hash, prefs_name);
	g_assert(value!=NULL && value->type==PREFS_TYPE_STR);

	return value->value.string;
}

GList * prefs_list(const gchar * prefs_name)
{
	struct prefs_value * value = g_hash_table_lookup(prefs_hash, prefs_name);
	g_assert(value!=NULL && value->type==PREFS_TYPE_LIST);

	return value->value.list;
}

void prefs_list_add(const gchar * prefs_name, const gchar * string)
{
	struct prefs_value * value = g_hash_table_lookup(prefs_hash, prefs_name);
	g_assert(value!=NULL && value->type==PREFS_TYPE_LIST);

	value->value.list = g_list_prepend(value->value.list, g_strdup(string));

	/* notify that we've changed to a new value */
	g_hook_list_invoke(&value->change_hook, FALSE);
	raise_event(EVENT_PREFS_CHANGED, (gpointer)prefs_name, 0);
}

void prefs_list_add_unique(const gchar * prefs_name, const gchar * string)
{
	if(!prefs_list_contains(prefs_name, string))
		prefs_list_add(prefs_name, string);
}

gboolean prefs_list_remove(const gchar * prefs_name, const gchar * string)
{
	GList * entry;

	struct prefs_value * value = g_hash_table_lookup(prefs_hash, prefs_name);
	g_assert(value!=NULL && value->type==PREFS_TYPE_LIST);

	for(entry = value->value.list; entry; entry = entry->next)
		if(!g_utf8_collate((const gchar*)entry->data, string)) {
			g_free(entry->data);
			value->value.list = g_list_delete_link(value->value.list, entry);

			/* notify that we've changed to a new value */
			g_hook_list_invoke(&value->change_hook, FALSE);
			raise_event(EVENT_PREFS_CHANGED, (gpointer)prefs_name, 0);

			return TRUE;
		}

	return FALSE;
}

void prefs_list_clear(const gchar * prefs_name)
{
	struct prefs_value * value = g_hash_table_lookup(prefs_hash, prefs_name);
	g_assert(value!=NULL && value->type==PREFS_TYPE_LIST);

	util_list_free_with_data(value->value.list, (GDestroyNotify)g_free);
	value->value.list = NULL;

	/* notify that we've changed to a new value */
	g_hook_list_invoke(&value->change_hook, FALSE);
	raise_event(EVENT_PREFS_CHANGED, (gpointer)prefs_name, 0);
}

gboolean prefs_list_contains(const gchar * prefs_name, const gchar * string)
{
	GList * entry;
	struct prefs_value * value = g_hash_table_lookup(prefs_hash, prefs_name);
	g_assert(value!=NULL && value->type==PREFS_TYPE_LIST);

	for(entry = value->value.list; entry!=NULL; entry = entry->next)
		if(!g_utf8_collate((const gchar *)entry->data, string))
			return TRUE;
	return FALSE;
}
