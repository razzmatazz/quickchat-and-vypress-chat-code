/*
 * gui_tray.h: system tray support
 * Copyright (C) 2003-2004 Saulius Menkevicius
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
 * $Id: gui_tray.h,v 1.7 2004/09/27 00:25:04 bobas Exp $
 */

#ifndef GUI_TRAY_H__
#define GUI_TRAY_H__

/* preferences registered in the 'gui_tray' module */
#define PREFS_GUI_TRAY_ENABLE		"gui/tray/enable"
#define PREFS_GUI_TRAY_TRIGGERS_JOIN_LEAVE "gui/tray/triggers/join_leave"
#define PREFS_GUI_TRAY_TRIGGERS_CHANNEL	"gui/tray/triggers/channel"
#define PREFS_GUI_TRAY_TRIGGERS_PRIVATE	"gui/tray/triggers/private"
#define PREFS_GUI_TRAY_TRIGGERS_STATUS	"gui/tray/triggers/status"
#define PREFS_GUI_TRAY_TRIGGERS_TOPIC	"gui/tray_triggers/topic"
#define PREFS_GUI_TRAY_HIDE_WND_ON_STARTUP "gui/tray/hide_wnd_on_startup"
#define PREFS_GUI_TRAY_TOOLTIP_LINE_NUM	"gui/tray/tooltip_line_num"

void gui_tray_register();
gboolean gui_tray_is_embedded();

#ifdef GUI_TRAY_IMPL

/* system tray ops common to all implementations
 */
typedef void tray_embedded_notifier();
typedef void tray_removed_notifier();
typedef void tray_clicked_notifier(guint button, guint32 time);

struct tray_impl_ops {
	gboolean (*create)();
	void (*destroy)();
	void (*set_icon)(gboolean, enum user_mode_enum);
	void (*set_tooltip)(const gchar *);

	void (*set_embedded_notifier)(tray_embedded_notifier *);
	void (*set_removed_notifier)(tray_removed_notifier *);
	void (*set_clicked_notifier)(tray_clicked_notifier *);
};

/* tray_impl_get_ops():
 *	returns a pointer to the implementation's ops struct
 */
const struct tray_impl_ops * tray_impl_init();

#endif	/* #ifdef GUI_TRAY_IMPL */

#endif	/* #ifndef GUI_TRAY_H__ */

