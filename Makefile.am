lib_LTLIBRARIES = lastTankStanding.la

lastTankStanding_la_SOURCES = lastTankStanding.cpp
lastTankStanding_la_CXXFLAGS= -I$(top_srcdir)/include -I$(top_srcdir)/plugins/plugin_utils
lastTankStanding_la_LDFLAGS = -module -avoid-version -shared
lastTankStanding_la_LIBADD = $(top_builddir)/plugins/plugin_utils/libplugin_utils.la

AM_CPPFLAGS = $(CONF_CPPFLAGS)
AM_CFLAGS = $(CONF_CFLAGS)
AM_CXXFLAGS = $(CONF_CXXFLAGS)

EXTRA_DIST = \
	CHANGELOG.md \
	LICENSE.md \
	README.md \
	lastTankStanding.cfg \
	lastTankStanding.def

MAINTAINERCLEANFILES =	\
	Makefile.in
