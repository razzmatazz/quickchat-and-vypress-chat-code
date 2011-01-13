AC_INIT(src/main.c)
AM_INIT_AUTOMAKE([vqcc-gtk], [0.5])
AM_CONFIG_HEADER(config.h)
AC_PREREQ([2.50])

dnl For FreeBSD..
CPPFLAGS="$CPPFLAGS -I/usr/local/include"
LDFLAGS="$LDFLAGS -L/usr/local/lib"

dnl Check for header files
AC_HEADER_STDC

dnl Configuration switches
AC_ARG_ENABLE(xscreensaver, [  --disable-xscreensaver  compile without XScreenSaver extension],,enable_xscreensaver=yes)
AC_ARG_ENABLE(startup-notification, [  --disable-startup-notification	compile without startup notification support],,enable_startup_notification=yes)

dnl Program checks
AC_PROG_CC

dnl Gettext
ALL_LINGUAS="lt ru"
AM_GLIB_GNU_GETTEXT

GETTEXT_PACKAGE=$PACKAGE
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["${GETTEXT_PACKAGE}"], [gettext domain])

dnl GLib & GTK+
AM_PATH_GLIB_2_0(2.0.0,,AC_MSG_ERROR("You must have GLib development package installed"))
AM_PATH_GTK_2_0(2.0.0,,AC_MSG_ERROR("You must have GTK+ 2.0 development package installed"))

AC_SUBST(GTK_CFLAGS)
AC_SUBST(GLIB_CFLAGS)

AC_PATH_XTRA

dnl Insert x_libraries
if test x"$x_libraries" = x""; then
	x_libpath_add=
else
	x_libpath_add="-L$x_libraries"
fi
if test x"$x_includes" = x""; then
	x_incpath_add=
else
	x_incpath_add="-I$x_includes"
fi

dnl startup notification (shamelessly taken from gaim's configure.ac)
if test "x$enable_startup_notification" = "xyes"; then
	PKG_CHECK_MODULES(STARTUP_NOTIFICATION, libstartup-notification-1.0 >= 0.5,
	[	AC_DEFINE(HAVE_STARTUP_NOTIFICATION, 1, [Define if we're using startup notification.])
		enable_startup_notification=yes
	],
	[	enable_startup_notification=no
	])

	AC_SUBST(STARTUP_NOTIFICATION_CFLAGS)
	AC_SUBST(STARTUP_NOTIFICATION_LIBS)
fi

dnl XScreenSaver extension
if test "x$enable_xscreensaver" = "xyes" ; then
	old_LIBS="$LIBS"
	LIBS="$LIBS $GTK_LIBS $x_libpath_add"
	XSS_LIBS="no"
	XSS_HEADERS="no"
	AC_CHECK_LIB(Xext, XScreenSaverRegister,[XSS_LIBS="$X_LIBS $X_PRE_LIBS -lX11 -lXext $X_EXTRA_LIBS"],[],[-lX11 -lXext -lm])
	AC_CHECK_LIB(Xss, XScreenSaverRegister,[XSS_LIBS="$X_LIBS $X_PRE_LIBS -lX11 -lXext $X_LIBS $X_EXTRA_LIBS -lXss"],[],[-lX11 -lXext -lm])
	if test \! "$XSS_LIBS" = "no"; then
		oldCPPFLAGS="$CPPFLAGS"
		CPPFLAGS="$CPPFLAGS -I$x_includes"
		AC_TRY_COMPILE([
#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>
				],[],[
				AC_DEFINE(USE_XSCREENSAVER, 1, [Define if we're using XScreenSaver.])],[enable_xscreensaver=no]
				)
		CPPFLAGS="$oldCPPFLAGS"
	else
	       XSS_LIBS=""
	       enable_xscreensaver=no
	fi
	LIBS="$old_LIBS"
else
	XSS_LIBS=""
	enable_xscreensaver=no
fi
AC_SUBST(XSS_LIBS)

dnl Misc programs & funcs
AC_PATH_PROG(gdkpixbufcsourcepath, gdk-pixbuf-csource)
AC_SUBST(gdkpixbufcsourcepath)
if test "x$gdkpixbufcsourcepath" = "x"; then
	AC_MSG_ERROR("You must have GTK+ 2.0+ development package installed")
fi

AC_CHECK_TOOL(AR, ar)
AC_CHECK_TOOL(RANLIB, ranlib, :)

AC_OUTPUT([Makefile \
	pixmaps/Makefile \
	src/Makefile \
	src/misc/Makefile \
	src/libqcproto/Makefile \
	po/Makefile.in
	])

