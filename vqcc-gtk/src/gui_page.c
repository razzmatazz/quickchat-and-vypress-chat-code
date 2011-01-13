/*
 * gui_page.c: chat notebook & page implementation
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
 * $Id: gui_page.c,v 1.34 2004/12/22 04:35:35 bobas Exp $
 */

#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "main.h"
#include "prefs.h"
#include "net.h"
#include "gui.h"
#include "gui_page.h"
#include "gui_misc.h"

#define PAGE_ICON_FLICKER_MS	500

#define PAGE_TEXTVIEW_LEN	360
#define PAGE_TEXTVIEW_HGT	320

/* enums & structs
 */
struct page_info_struct {
	GtkWidget * icon_w, * label_w,
		* text_w, * scroll_w;

	/* we hold no reference to `text' & `tags'[] */
	GtkTextBuffer * text;
	GtkTextTag * tags[ATTR_NUM];

	guint	max_lines,	/* maximum lines allowed in buffer */
		line_count,	/* lines in buffer */
		killed_lines;	/* lines killed off the buffer */

	gint flick_timeout_tag;	/* identifies g_timeout event source which
				 * flicks page icon when `hilited' is set */

	gboolean hilited;	/* the `label' widget is hilited */
	gboolean hilited_greyed;/* set if page icon currently is greyed */
	gboolean closeable;	/* set we should allow the page to be 
				 * closed by user */

	enum session_type type;
	gpointer session;	/* session is page's session_id */
	gpointer user_id;	/* user_id may be null when user is dead ! */
};
#define PPAGE_INFO_STRUCT(p) ((struct page_info_struct*)p)

struct notebook_struct {
	GtkWidget * widget;
	struct page_info_struct * current_page;
	GList * page_list;
};

/* static variables
 */
static struct notebook_struct * the_notebook;

/* static routines
 ******************************/

static gint
get_page_num_by_id(gpointer session)
{
	gint num = 0;
	GList * le;

	for(le = the_notebook->page_list; le; le = le->next) {
		if(PPAGE_INFO_STRUCT(le->data)->session==session)
			break;
		num++;
	}
	g_assert(le!=NULL);
	return num;
}

static struct page_info_struct *
get_page_struct_by_session(gpointer session)
{
	GList * le;
	for(le = the_notebook->page_list; le; le = le->next)
		if(PPAGE_INFO_STRUCT(le->data)->session==session)
			break;

	g_assert(le!=NULL);
	return PPAGE_INFO_STRUCT(le->data);
}

static void
page_menu_set_topic_cb(GtkMenuItem * item_w, gpointer page)
{
	g_return_if_fail(PPAGE_INFO_STRUCT(page)->type==SESSTYPE_CHANNEL);
	raise_event(EVENT_IFACE_SHOW_TOPIC_DLG, PPAGE_INFO_STRUCT(page)->session, 0);
}

static void
page_menu_channels_cb(GtkMenuItem * item_w, gpointer page)
{
	raise_event(EVENT_IFACE_SHOW_CHANNEL_DLG, NULL, 0);
}

static void
page_menu_ignore_list_cb(GtkMenuItem * item_w, gpointer page)
{
	raise_event(EVENT_IFACE_SHOW_IGNORE_DLG, NULL, 0);
}

static void
page_menu_preferences_cb(GtkMenuItem * item_w, gpointer page)
{
	raise_event(EVENT_IFACE_SHOW_CONFIGURE_DLG, NULL, 0);
}

static void
page_menu_menu_bar_cb(GtkCheckMenuItem * item_w, gpointer page)
{
	prefs_set(PREFS_GUI_MENU_BAR, (guint)gtk_check_menu_item_get_active(item_w));
}

static void
page_menu_topic_bar_cb(GtkCheckMenuItem * item_w, gpointer page)
{
	prefs_set(PREFS_GUI_TOPIC_BAR, (guint)gtk_check_menu_item_get_active(item_w));
}

static void
page_menu_quit_cb(GtkMenuItem * item_w, gpointer page)
{
	raise_event(EVENT_IFACE_EXIT, NULL, 0);
}

static void
popup_page_menu(
	struct page_info_struct * page,
	guint32 event_time_stamp, guint event_button)
{
	GtkWidget * menu_w, * item_w;
	
	menu_w = gtk_menu_new();

	if(page->type==SESSTYPE_CHANNEL && !prefs_bool(PREFS_GUI_TOPIC_BAR)) {
		item_w = util_image_menu_item(
			GUI_STOCK_SET_TOPIC, _("Set topic.. [CTRL-T]"),
			G_CALLBACK(page_menu_set_topic_cb), page);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);

		gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), gtk_separator_menu_item_new());
	}

	/* check item for menu bar
	 */
	item_w = gtk_check_menu_item_new_with_mnemonic(prefs_description(PREFS_GUI_MENU_BAR));
	g_signal_connect(G_OBJECT(item_w), "toggled",
		G_CALLBACK(page_menu_menu_bar_cb), page);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_w),
		prefs_bool(PREFS_GUI_MENU_BAR));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);

	/* check item for topic bar */
	item_w = gtk_check_menu_item_new_with_mnemonic(prefs_description(PREFS_GUI_TOPIC_BAR));
	g_signal_connect(G_OBJECT(item_w), "toggled", G_CALLBACK(page_menu_topic_bar_cb), page);
	gtk_widget_set_sensitive(item_w, sess_type(sess_current())==SESSTYPE_CHANNEL);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_w),
		prefs_bool(PREFS_GUI_TOPIC_BAR));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);
	
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), gtk_separator_menu_item_new());

	item_w = util_image_menu_item(
		GUI_STOCK_CHANNEL, _("Channels.."),
		G_CALLBACK(page_menu_channels_cb), page);
	gtk_widget_set_sensitive(item_w, net_connected());
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);

	item_w = util_image_menu_item(
		GUI_STOCK_IGNORE, _("Ignore list.."),
		G_CALLBACK(page_menu_ignore_list_cb), page);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);

	item_w = util_image_menu_item(
		GTK_STOCK_PREFERENCES, _("Preferences.."),
		G_CALLBACK(page_menu_preferences_cb), page);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);

	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), gtk_separator_menu_item_new());

	item_w = util_image_menu_item(
		GTK_STOCK_QUIT, _("Quit"),
		G_CALLBACK(page_menu_quit_cb), page);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);

	gtk_widget_show_all(menu_w);
	gtk_menu_popup(GTK_MENU(menu_w), NULL, NULL, NULL, NULL, event_button, event_time_stamp);
}

static gboolean
page_text_button_event_cb(
	GtkWidget * text_w,
	GdkEventButton * event,
	struct page_info_struct * page)
{
	switch(event->type) {
	case GDK_BUTTON_PRESS:
		if(event->button==3) {
			popup_page_menu(page, event->time, event->button);
			return TRUE;
		}
		break;

	case GDK_BUTTON_RELEASE:
		if(event->button==1) {
			/* move focus to the command line */
			raise_event(EVENT_IFACE_PAGE_RELEASE_FOCUS, 0, 0);
		}
		break;
	default:
		break;
	}

	return FALSE;
}

static struct page_info_struct *
get_page_by_user(gpointer user_id)
{
	GList * l;

	g_assert(user_id);

	for(l=the_notebook->page_list; l; l=l->next)
		if(PPAGE_INFO_STRUCT(l->data)->user_id==user_id)
			break;

	return l ? PPAGE_INFO_STRUCT(l->data): NULL;
}

/* page_update_icon:
 *	updates icon for specified page
 * params:
 *	@icon_mode: 	-1: default (== user_is_active(user_id) if user_id!=NULL)
 *			0: normal
 *			1: active (or shaded)
 */
static void
page_update_icon(struct page_info_struct * page, gint icon_mode)
{
	const gchar * icon;

	g_assert(page);

	switch(page->type) {
	case SESSTYPE_STATUS:
		icon = icon_mode > 0 ? GUI_STOCK_STATUS_INACTIVE: GUI_STOCK_STATUS;
		break;

	case SESSTYPE_CHANNEL:
		icon = icon_mode > 0 ? GUI_STOCK_CHANNEL_INACTIVE: GUI_STOCK_CHANNEL;
		break;
	
	case SESSTYPE_PRIVATE:
		/* current user status icon */
		icon = util_user_state_stock(
			page->user_id==NULL
				? UMODE_DEAD
				: user_mode_of(page->user_id),
			icon_mode < 0
				? (page->user_id==NULL ? TRUE: user_is_active(page->user_id))
				: icon_mode > 0
		);
		break;
	}

	/* set icon */
	gtk_image_set_from_stock(GTK_IMAGE(page->icon_w), icon, GTK_ICON_SIZE_MENU);
}

/*
 * page_update_for_user
 *	updates "chat" page state according to the state of associated user
 *	(sets page icon & label, misc)
 */
static void
page_update_for_user(gpointer user)
{
	struct page_info_struct * page;
	enum user_mode_enum umode;
	
	g_assert(user);

	page = get_page_by_user(user);
	if(page) {
		umode = user_mode_of(user);

		/* set page label */
		gtk_label_set_text(GTK_LABEL(page->label_w), user_name_of(user));

		/* update page icon image (if it's not blinking) */
		if(!page->flick_timeout_tag)
			page_update_icon(page, -1);
	}
}

/*
 * page_set_user_for_dead_name:
 *	updates page of the user which got revived after being dead for some time
 */
static void
page_set_user_for_dead_name(const gchar * name, gpointer user_id)
{
	GList * l;

	g_assert(name && user_id);
	
	for(l=the_notebook->page_list; l; l=l->next) {
		struct page_info_struct * page = PPAGE_INFO_STRUCT(l->data);

		if(page->user_id==NULL
			&& !nickname_cmp(gtk_label_get_text(GTK_LABEL(page->label_w)),name))
		{
			/* revive user's page
			 */
			page->user_id = user_id;

			page_update_for_user(user_id);
			break;
		}
	}
}

/*
 * page_user_removed:
 * 	marks a page as 'without-corresponding-user' for specified user's page
 */
static void
page_user_removed(gpointer user)
{
	struct page_info_struct * page;

	page = get_page_by_user(user);
	if(page) {
		/* mark that the page' user was removed */
		page->user_id = NULL;

		page_update_icon(page, -1);
	}
}

/** make_page_tag:
  *	NOTE:	(1) no need to destroy/unref returned tag
  *		(2) may return NULL, (no tag to apply)
  */
static GtkTextTag *
make_page_tag(
	struct page_info_struct * p,
	enum text_attr attr)
{
	GtkTextTag * t;
	GdkColor c;

	g_assert(attr<ATTR_NUM);
	
	if( p->tags[attr] || attr==ATTR_NULL ) {
		/* the tag has been made previously */
		return p->tags[attr];
	}

	c.red = 0;
	c.blue = 0;
	c.green = 0;

	switch(attr) {
	case ATTR_NORM:
		t = gtk_text_buffer_create_tag(
				p->text, NULL,
				"foreground-gdk", &c,
				NULL);
		break;
	case ATTR_THEIR_TEXT:
		t = gtk_text_buffer_create_tag(
				p->text, NULL,
				"foreground-gdk", &c,
				NULL);
		break;
	case ATTR_ME_TEXT:
		c.green = 0x8000;
		c.red = 0x8000;
		t = gtk_text_buffer_create_tag(
				p->text, NULL,
				"foreground-gdk", &c,
				NULL);
		break;

	case ATTR_USER:
	case ATTR_MY_TEXT:
		c.blue = 0x8000;
		t = gtk_text_buffer_create_tag(
				p->text, NULL,
				"foreground-gdk", &c,
				NULL);
		break;
	
	case ATTR_CHAN:
		c.red = 0x8000;
		t = gtk_text_buffer_create_tag(
				p->text, NULL,
				"foreground-gdk", &c,
				NULL);
		break;

	case ATTR_ERR:
		c.red = 0x8000;
		t = gtk_text_buffer_create_tag(
				p->text, NULL,
				"foreground-gdk", &c,
				NULL);
		break;
	
	case ATTR_TOPIC:
		c.blue = 0x7fff;
		t = gtk_text_buffer_create_tag(
				p->text, NULL,
				"foreground-gdk", &c,
				"weight", (gint)PANGO_WEIGHT_BOLD,
				NULL);
		break;
	case ATTR_TIME:
		c.green = 0x6000;
		t = gtk_text_buffer_create_tag(
				p->text, NULL,
				"foreground-gdk", &c,
				NULL);
		break;
	case ATTR_NOTIFY:
		c.green = 0x8000;
		c.red = 0x8000;
		t = gtk_text_buffer_create_tag(
				p->text, NULL,
				"foreground-gdk", &c,
				NULL);
		break;
	default:
		/* usupported/invisible tag */
		t = NULL;
	}
	
	/* save reference to this tag */
	p->tags[attr] = t;
	return t;
}

/* signal callbacks
 **************************************/

/* page_icon_flick_cb
 *	periodically changes page icon from normal(current) to grayed
 * (when `page->hilited' is set)
 */
static gboolean
page_icon_flick_cb(struct page_info_struct * page)
{
	page_update_icon(page, page->hilited_greyed ? 1: 0);
	page->hilited_greyed = !page->hilited_greyed;

	/* keep the timeout source */
	return TRUE;
}

static void
notebook_switch_page_cb(
	GtkNotebook * notebook_widget,
	GtkNotebookPage * notebook_page,
	gint page_num,
	gpointer data)
{
	if(!the_notebook->page_list) {
		/* gtk emits page-switch signal after creating
		 * empty tab with zero number of pages.
		 * here we avoid SIGSEGV (page_list==0 at that time)*/
		return;
	}

	/* set current page */
	the_notebook->current_page =
		PPAGE_INFO_STRUCT(g_list_nth(the_notebook->page_list, page_num)->data);

	/* inform about change */
	raise_event(EVENT_IFACE_PAGE_SWITCH, the_notebook->current_page->session, page_num);
}

static void
free_notebook_struct(struct notebook_struct * notebook)
{
	/* not much to do here:
	 * the notebook widget will sink with the main page
	 */
	g_free(notebook);
}

static void
gui_page_prefs_page_tab_pos_changed_cb(const gchar * pref_name)
{
	/* check value of PREFS_NOTEBOOK_POS
	 */
	if(prefs_int(PREFS_GUI_PAGE_TAB_POS) > GTK_POS_BOTTOM) {
		/* value invalid: set to default
		 */
		prefs_set(PREFS_GUI_PAGE_TAB_POS, GTK_POS_TOP);
	} else {
		/* value ok, set position */
		gtk_notebook_set_tab_pos(
			GTK_NOTEBOOK(the_notebook->widget),
			(GtkPositionType)prefs_int(PREFS_GUI_PAGE_TAB_POS));
	}
}

static void
gui_page_event_cb(enum app_event_enum event, gpointer p, int i)
{
	switch(event) {
	case EVENT_MAIN_REGISTER_PREFS:
		prefs_register(PREFS_GUI_PAGE_TAB_POS, PREFS_TYPE_UINT,
			_("Page tab position"), NULL, NULL);
		prefs_add_notifier(PREFS_GUI_PAGE_TAB_POS,
			(GHookFunc)gui_page_prefs_page_tab_pos_changed_cb);
		break;

	case EVENT_MAIN_PRESET_PREFS:
		prefs_set(PREFS_GUI_PAGE_TAB_POS, GTK_POS_BOTTOM);
		break;

	case EVENT_MAIN_CLOSE:
		/* forcibly close all the pages */
		while(the_notebook->page_list)
			gui_page_delete(PPAGE_INFO_STRUCT(the_notebook->page_list->data)->session);

		free_notebook_struct(the_notebook);
		the_notebook = NULL;
		break;

	case EVENT_USER_NEW:
		/* revive peer's page, if it was "dead" previously
		 */
		page_set_user_for_dead_name((const gchar*)EVENT_V(p, 1), EVENT_V(p, 0));
		break;

	case EVENT_USER_REMOVED:
		page_user_removed(p);
		break;
	case EVENT_USER_ACTIVE_CHANGE:
	case EVENT_USER_MODE_CHANGE:
		page_update_for_user(p);
		break;
	case EVENT_USER_RENAME:
		page_update_for_user(EVENT_V(p, 0));
		break;
	default:
		break;
	}
}

static void
page_handle_menu_close_cb(GtkMenuItem * item_w, gpointer page)
{
	raise_event(EVENT_IFACE_PAGE_CLOSE, PPAGE_INFO_STRUCT(page)->session, 0);
}

static void
page_handle_menu_persistent_toggled_cb(GtkCheckMenuItem * item_w, gpointer page)
{
	const gchar * channel = sess_name(PPAGE_INFO_STRUCT(page)->session);

	if(gtk_check_menu_item_get_active(item_w))
		prefs_list_add_unique(PREFS_MAIN_PERSISTENT_CHANNELS, channel);
	else
		prefs_list_remove(PREFS_MAIN_PERSISTENT_CHANNELS, channel);
}

static void
page_handle_menu_important_toggled_cb(GtkCheckMenuItem * item_w, gpointer page)
{
	const gchar * channel = sess_name(PPAGE_INFO_STRUCT(page)->session);

	if(gtk_check_menu_item_get_active(item_w))
		prefs_list_add_unique(PREFS_SESS_IMPORTANT_CHANNELS, channel);
	else
		prefs_list_remove(PREFS_SESS_IMPORTANT_CHANNELS, channel);
}

static void
page_handle_menu_tab_toggled_cb(GtkCheckMenuItem * item_w, gpointer new_position)
{
	if(prefs_int(PREFS_GUI_PAGE_TAB_POS) != GPOINTER_TO_INT(new_position))
		prefs_set(PREFS_GUI_PAGE_TAB_POS, GPOINTER_TO_INT(new_position));
}

static GtkWidget *
page_handle_tab_position_submenu(struct page_info_struct * page)
{
	GtkWidget * menu, * item;
	
	menu = gtk_menu_new();

#define APPEND_POS_MENU_ITEM(shell, name, pos) do {	\
		item = gtk_check_menu_item_new_with_mnemonic(name);		\
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),	\
			prefs_int(PREFS_GUI_PAGE_TAB_POS)==pos);		\
		g_signal_connect(G_OBJECT(item), "toggled",			\
			G_CALLBACK(page_handle_menu_tab_toggled_cb), GINT_TO_POINTER(pos));	\
		gtk_menu_shell_append(GTK_MENU_SHELL(shell), item);		\
	} while(0);

	APPEND_POS_MENU_ITEM(menu, _("_Top"), GTK_POS_TOP);
	APPEND_POS_MENU_ITEM(menu, _("_Bottom"), GTK_POS_BOTTOM);
	APPEND_POS_MENU_ITEM(menu, _("_Left"), GTK_POS_LEFT);
	APPEND_POS_MENU_ITEM(menu, _("_Right"), GTK_POS_RIGHT);

	return menu;
}

static void
popup_page_handle_menu(
	struct page_info_struct * page,
	guint32 event_time_stamp,
	guint event_button)
{
	GtkWidget * menu_w, * item_w;

	menu_w = gtk_menu_new();

	/* "Close page" */
	item_w = util_image_menu_item(
		GTK_STOCK_CLOSE, _("Close page [CTRL-W]"),
		G_CALLBACK(page_handle_menu_close_cb), page);
	gtk_widget_set_sensitive(item_w, page->closeable);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);

	if(sess_type(page->session)==SESSTYPE_CHANNEL) {
		/* "[*] Persistent" */
		item_w = gtk_check_menu_item_new_with_mnemonic(_("_Persistent"));
		if(page->closeable) {
			gtk_check_menu_item_set_active(
				GTK_CHECK_MENU_ITEM(item_w),
				prefs_list_contains(PREFS_MAIN_PERSISTENT_CHANNELS,
					sess_name(page->session)));

			g_signal_connect(G_OBJECT(item_w), "toggled",
				G_CALLBACK(page_handle_menu_persistent_toggled_cb), page);
		} else {
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_w), TRUE);
			gtk_widget_set_sensitive(item_w, FALSE);
		}
		gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);

		/* [*] Important */
		item_w = gtk_check_menu_item_new_with_mnemonic(_("_Important"));
		gtk_check_menu_item_set_active(
			GTK_CHECK_MENU_ITEM(item_w),
			prefs_list_contains(PREFS_SESS_IMPORTANT_CHANNELS, sess_name(page->session)));
		g_signal_connect(
			G_OBJECT(item_w), "toggled",
			G_CALLBACK(page_handle_menu_important_toggled_cb), page);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);
	}

	/* --- */
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), gtk_separator_menu_item_new());

	/* page tab position submenu */
	item_w = util_image_menu_item(NULL, prefs_description(PREFS_GUI_PAGE_TAB_POS), NULL, NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_w), page_handle_tab_position_submenu(page));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_w), item_w);
	
	gtk_widget_show_all(menu_w);
	gtk_menu_popup(GTK_MENU(menu_w), NULL, NULL, NULL, NULL, event_button, event_time_stamp);
}

static gboolean
page_handle_button_press_event(
	GtkWidget * widget, GdkEventButton * event,
	struct page_info_struct * page)
{
	if(event->button==3) {
		popup_page_handle_menu(page, event->time, event->button);
		return TRUE;
	}
	return FALSE;
}

static gboolean
page_handle_popup_menu_event(
	GtkWidget * widget,
	struct page_info_struct * page)
{
	popup_page_handle_menu(page, 0, 0);
	return TRUE;
}

/* exported routines
 *************************************/

void gui_page_register()
{
	the_notebook = NULL;

	register_event_cb(gui_page_event_cb, EVENT_USER|EVENT_MAIN);
}

/**
 * gui_page_alloc_notebook:
 *	allocates the main GtkNotebook to contain the
 *	pages in question
 */
gpointer
gui_page_alloc_notebook()
{
	struct notebook_struct * nb;

	nb = g_malloc(sizeof(struct notebook_struct));
	nb->page_list = NULL;
	nb->current_page = NULL;

	nb->widget = (GtkWidget*)gtk_notebook_new();
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(nb->widget),  TRUE);
	g_signal_connect(G_OBJECT(nb->widget), "switch-page",
			G_CALLBACK(notebook_switch_page_cb), NULL);

	/* there can be only one notebook in the current design
	 */
	g_assert(the_notebook==NULL);
	the_notebook = nb;

	return (gpointer)nb->widget;
}

void gui_page_set_tab_pos(gboolean top)
{
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(the_notebook), top ? GTK_POS_TOP: GTK_POS_BOTTOM);
}

int gui_page_new(
	enum session_type page_type,
	const gchar * page_name, const gchar * page_topic,
	gboolean closeable, unsigned int max_lines,
	gpointer session, gpointer uid)
{
	struct page_info_struct * page;
	GtkWidget * event_box_w, * box_w;

	g_assert(session);

	page = g_malloc(sizeof(struct page_info_struct));

	page->type = page_type;
	page->session = session;
	page->max_lines = max_lines;
	page->line_count = page->killed_lines = 0;
	page->hilited = FALSE;
	page->hilited_greyed = FALSE;
	page->closeable = closeable;
	page->user_id = uid;

	/* cleanup tag space */
	memset((gpointer)page->tags, 0, sizeof(gpointer)*ATTR_NUM);

	/* page-handle widget
	 */
	box_w = gtk_hbox_new(FALSE, 2);

	page->icon_w = gtk_image_new_from_stock(GUI_STOCK_STATUS, GTK_ICON_SIZE_MENU);
	page_update_icon(page, -1);
	gtk_box_pack_start(GTK_BOX(box_w), page->icon_w, FALSE, FALSE, 0);

	page->label_w = gtk_label_new(page->type==SESSTYPE_PRIVATE ? user_name_of(uid) : page_name);
	gtk_box_pack_end(GTK_BOX(box_w), page->label_w, TRUE, TRUE, 0);

	event_box_w = gtk_event_box_new();
	g_signal_connect(G_OBJECT(event_box_w), "button-press-event",
			G_CALLBACK(page_handle_button_press_event), (gpointer)page);
	g_signal_connect(G_OBJECT(event_box_w), "popup-menu",
			G_CALLBACK(page_handle_popup_menu_event), (gpointer)page);
	gtk_container_add(GTK_CONTAINER(event_box_w), box_w);

	gtk_widget_show_all(event_box_w);

	/* session text display widget
	 */
	page->scroll_w = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(page->scroll_w), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW(page->scroll_w),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	page->text_w = gtk_text_view_new();
	gtk_widget_set_usize(page->text_w, PAGE_TEXTVIEW_LEN, PAGE_TEXTVIEW_HGT);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(page->text_w), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(page->text_w), GTK_WRAP_CHAR);
	g_signal_connect(
		G_OBJECT(page->text_w), "button-press-event",
		G_CALLBACK(page_text_button_event_cb), (gpointer)page );
	g_signal_connect(
		G_OBJECT(page->text_w), "button-release-event",
		G_CALLBACK(page_text_button_event_cb), (gpointer)page );

	gtk_container_add(GTK_CONTAINER(page->scroll_w), page->text_w);
	gtk_widget_show_all(page->scroll_w);

	/* get text buffer */
	page->text = gtk_text_view_get_buffer(GTK_TEXT_VIEW(page->text_w));

	/* append page to the list */
	the_notebook->page_list = g_list_append(the_notebook->page_list, (gpointer)page);

	/* append page to the notebook */
	gtk_notebook_append_page(GTK_NOTEBOOK(the_notebook->widget), page->scroll_w, event_box_w);

	return get_page_num_by_id(session);
}

void gui_page_delete(gpointer session)
{
	struct page_info_struct * page;
	int page_num;
	
	/* remove page & link from the list */
	page_num = get_page_num_by_id(session);
	page = get_page_struct_by_session(session);

	gtk_notebook_remove_page(GTK_NOTEBOOK(the_notebook->widget), page_num);

	the_notebook->page_list = g_list_remove_link(the_notebook->page_list,
					g_list_nth(the_notebook->page_list, page_num));

	if(page->hilited)
		g_source_remove(page->flick_timeout_tag);

	g_free(page);
}

void gui_page_write(
	gpointer session,
	enum text_attr attr, const char * text, gboolean eoln)
{
	GtkTextTag * tag;
	GtkTextIter start, end;
	struct page_info_struct * p;

	g_assert(text!=NULL);

	p = get_page_struct_by_session(session);

	/* insert new text ... */
	if(strlen(text)) {
		/* delete the very first line if we maxed line count */
		if( eoln && p->line_count==p->max_lines ) {
			/* delete text from buffer */
			gtk_text_buffer_get_start_iter(p->text, &start);
			gtk_text_buffer_get_iter_at_line(p->text, &end, 1);

			gtk_text_buffer_delete(p->text, &start, &end);

			/* increase num of killed lines */
			++ p->killed_lines;
		}

		/* insert new text */
		tag = make_page_tag(p, attr);
		gtk_text_buffer_get_end_iter(p->text, &end);
		gtk_text_buffer_insert_with_tags(p->text, &end, text, -1, tag, NULL);
	}

	/* .. and emit EOLN if needed */
	if(eoln) {
		gtk_text_buffer_get_end_iter(p->text, &end);
		gtk_text_buffer_insert(p->text, &end, "\n", -1);

		++ p->line_count;

		/* do the scrolling to show our text */
		gui_page_scroll(session, SCROLL_END);
	}
}

/* gui_page_current()
 *	returns session of the currently selected page (tab)
 */
gpointer gui_page_current()
{
	return the_notebook->current_page
		? the_notebook->current_page->session: NULL;
}

gpointer gui_page_prev(gpointer session)
{
	GList * le;

	for(le = the_notebook->page_list; le; le = le->next) {
		if(PPAGE_INFO_STRUCT(le->data)->session == session)
			break;
	}
	g_assert(le!=NULL);

	return le->prev ? PPAGE_INFO_STRUCT(le->prev->data)->session: NULL;
}

gpointer gui_page_next(gpointer session)
{
	GList * le;

	for(le = the_notebook->page_list; le; le = le->next) {
		if(PPAGE_INFO_STRUCT(le->data)->session == session)
			break;
	}
	g_assert(le!=NULL);

	return le->next ? PPAGE_INFO_STRUCT(le->next->data)->session: NULL;
}

void gui_page_switch(gpointer session)
{
	/* set current_page
	 */
	the_notebook->current_page = get_page_struct_by_session(session);

	/* change tab on notebook */
	gtk_notebook_set_current_page(
		GTK_NOTEBOOK(the_notebook->widget),
		get_page_num_by_id(session));
}

/**
 * gui_page_hilite:
 *	makes page icon flicker periodically (normal->greyed->normal icons)
 */
void gui_page_hilite(gpointer session, gboolean hi)
{
	struct page_info_struct * page = get_page_struct_by_session(session);

	/* bail out if already hilited/unhilited */
	if(!hi==!page->hilited) return;

	if(hi) {
		/* turn flicker on
		 */
		page->hilited = TRUE;
		page->hilited_greyed = FALSE;

		/* at first toggle icon into greyed mode */
		page_icon_flick_cb(page);

		/* attach timeout event source to it: */
		page->flick_timeout_tag = g_timeout_add(
				PAGE_ICON_FLICKER_MS,
				(GSourceFunc)page_icon_flick_cb,
				(gpointer)page
			);
	} else {
		/* turn flicker off
		 */
		g_source_remove(page->flick_timeout_tag);
		page->flick_timeout_tag = 0;

		/* make sure we're in right mode:
		 *	toggle the mode into normal
		 */
		page_update_icon(page, -1);

		page->hilited = FALSE;
		page->hilited_greyed = FALSE;
	}
}

/**
 * gui_page_scroll:
 *	scrolls page in specified direction
 */
void gui_page_scroll(gpointer session, enum gui_page_scroll_enum scrool)
{
	struct page_info_struct * page;
	GtkAdjustment * adj;
	gdouble new_value;

	page = get_page_struct_by_session(session);
	g_assert(page);

	adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(page->scroll_w));

	switch(scrool) {
	case SCROLL_HOME:
		new_value = adj->lower;
		break;
	case SCROLL_END:
		new_value = adj->upper;
		break;
	case SCROLL_PAGE_UP:
		new_value = adj->value - adj->page_increment;
		break;
	case SCROLL_PAGE_DOWN:
		new_value = adj->value + adj->page_increment;
		break;
	}
	if(new_value < adj->lower) new_value = adj->lower;
	else if(new_value > adj->upper) new_value = adj->upper;

	if(new_value != adj->value)
		gtk_adjustment_set_value(adj, new_value);
}


