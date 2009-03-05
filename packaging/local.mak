CC := /usr/bin/i386-mingw32-gcc
CXX := /usr/bin/i386-mingw32-g++
GMSGFMT := /usr/bin/msgfmt
MAKENSIS := /usr/bin/makensis
PERL := /usr/bin/perl
EXTUTILS := /usr/lib/perl5/5.8.8/ExtUtils
WINDRES := /usr/bin/i386-mingw32-windres
STRIP := /usr/bin/i386-mingw32-strip

INCLUDE_PATHS :=  -I$(PIDGIN_TREE_TOP)/../win32-dev/meanwhile-1.0.2_daa1/include/meanwhile/ -I$(PIDGIN_TREE_TOP)/../win32-dev/pcre-6.7-static
LIB_PATHS := -L$(PIDGIN_TREE_TOP)/../win32-dev/meanwhile-1.0.2_daa1/lib/ -L$(PIDGIN_TREE_TOP)/../win32-dev/pcre-6.7-static
