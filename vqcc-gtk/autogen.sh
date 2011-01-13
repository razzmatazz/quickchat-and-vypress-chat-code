ACLOCAL=aclocal
AUTOHEADER=autoheader
AUTOCONF=autoconf
AUTOMAKE=automake

# check for automake 1.6+
if automake-1.6 --version > /dev/null
then
	AUTOMAKE=automake-1.6
	ACLOCAL=aclocal-1.6
else
	if automake --version > /dev/null
	then
		ACLOCAL=automake
		ACLOCAL=aclocal
	else
		echo "You need automake version 1.5 to build 'configure' for vqcc-gtk"
		exit 1
	fi
fi

# check for autoconf 2.53+
if autoconf253 --version > /dev/null
then
	AUTOCONF=autoconf253
	AUTOHEADER=autoheader253
else
	if autoconf --version > /dev/null
	then
		AUTOCONF=autoconf
		AUTOHEADER=autoheader
	else
		echo "You need autoconf version 2.53 to build 'configure' for vqcc-gtk"
		exit 1
	fi
fi

# check for glib-gettextize
if glib-gettextize --version > /dev/null
then
	GLIB_GETTEXTIZE=glib-gettextize
else
	echo "You need development package for GTK+ 2.x to build 'configure' for vqcc-gtk"
	exit 1
fi

# build the stuff
$ACLOCAL
$AUTOHEADER
$AUTOCONF
$AUTOMAKE -a -c
$GLIB_GETTEXTIZE

CFLAGS="-g" ./configure --enable-maintainer-mode --disable-optimise # --prefix=${HOME}/local

