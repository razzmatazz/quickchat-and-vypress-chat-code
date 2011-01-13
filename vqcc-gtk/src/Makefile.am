bin_PROGRAMS = vqcc-gtk

vqcc_gtk_LDADD = \
	$(XSS_LIBS) \
	$(GTK_LIBS) \
	$(STARTUP_NOTIFICATION_LIBS) \
	misc/libegg.a \
	libqcproto/libqcproto.a

vqcc_gtk_SOURCES = \
	cmd_proc.c cmd_proc.h \
	prefs.c prefs.h \
	gui.c gui.h \
	gui_channel.c gui_channel.h \
	gui_config.c gui_config.h \
	gui_ignore.c gui_ignore.h \
	gui_misc.c gui_misc.h \
	gui_msg.c gui_msg.h \
	gui_page.c gui_page.h \
	gui_tray.c gui_tray.h \
	gui_tray_x11.c \
	gui_ulist.c gui_ulist.h \
	gui_topic_dlg.c gui_topic_dlg.h \
	gui_netselect_dlg.c gui_netselect_dlg.h \
	gui_about_dlg.c gui_about_dlg.h \
	idle.c idle.h \
	main.c main.h \
	net.c net.h \
	sess.c sess.h \
	user.c user.h \
	util.c util.h

EXTRA_DIST = \
	gui_tray_win32.c \
	win32.c win32.h \
	win32_resource.h win32_resource.rc \
	Makefile.mingw

AM_CPPFLAGS = \
	-Wall -Wno-uninitialized \
	-DLOCALE_DIR=\""$(datadir)/locale"\" \
	$(GTK_CFLAGS) \
	$(STARTUP_NOTIFICATION_CFLAGS)

SUBDIRS = misc libqcproto
