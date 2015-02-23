
plugin_LTLIBRARIES = libgsthdsdemux.la

libgsthdsdemux_la_SOURCES =			\
	gsthdsdemux.c				\
	gstplugin.c

# headers we need but don't want installed
noinst_HEADERS =        \
	gsthdsdemux.h	\
	gsthds_debug.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgsthdsdemux_la_CFLAGS = $(GST_PLUGINS_BAD_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS) \
	$(LIBXML2_CFLAGS)
libgsthdsdemux_la_LIBADD = \
	$(top_builddir)/gst-libs/gst/adaptivedemux/libgstadaptivedemux-@GST_API_VERSION@.la \
	-lgsttag-$(GST_API_VERSION) \
	$(GST_BASE_LIBS) \
	$(GST_LIBS) \
	$(LIBXML2_LIBS)
libgsthdsdemux_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgsthdsdemux_la_LIBTOOLFLAGS = --tag=disable-static

