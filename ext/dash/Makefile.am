
plugin_LTLIBRARIES = libgstdash.la

libgstdash_la_SOURCES =				\
	gstmpdparser.c				\
	gstdashdemux.c				\
	gstisoff.c				\
        gstdashsink.c				\
        gstdashmanager.c			\
        mpd/gstperiod.c				\
        mpd/gstadaptationset.c			\
        mpd/gstmediacommon.c			\
        mpd/gstmediapresentation.c		\
        mpd/gstrepresentation.c			\
        mpd/gstsegmentlist.c			\
        mpd/gstxmlhelper.c			\
	gstplugin.c

# headers we need but don't want installed
noinst_HEADERS =        \
        gstmpdparser.h	\
	gstdashdemux.h	\
	gstisoff.h	\
        gstdashsink.h	\
        gstdashmanager.h \
        mpd/gstperiod.h	\
        mpd/gstadaptationset.h \
        mpd/gstmediacommon.h \
        mpd/gstmediapresentation.h \
        mpd/gstrepresentation.h \
        mpd/gstsegmentlist.h \
        mpd/gstxmlhelper.h \
	gstdash_debug.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstdash_la_CFLAGS = $(GST_PLUGINS_BAD_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS) \
	$(GIO_CFLAGS) \
	$(LIBXML2_CFLAGS)
libgstdash_la_LIBADD = \
	$(top_builddir)/gst-libs/gst/uridownloader/libgsturidownloader-@GST_API_VERSION@.la \
	$(top_builddir)/gst-libs/gst/adaptivedemux/libgstadaptivedemux-@GST_API_VERSION@.la \
	$(top_builddir)/gst-libs/gst/baseadaptive/libgstbaseadaptive-$(GST_API_VERSION).la  \
	$(GST_PLUGINS_BASE_LIBS) \
	-lgsttag-$(GST_API_VERSION) \
	$(GST_BASE_LIBS) \
	$(GST_NET_LIBS) \
	$(GST_LIBS) \
	$(GIO_LIBS) \
	$(LIBXML2_LIBS)
libgstdash_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstdash_la_LIBTOOLFLAGS = --tag=disable-static

Android.mk: Makefile.am $(BUILT_SOURCES)
	androgenizer \
	-:PROJECT libgstdash -:SHARED libgstdash \
	 -:TAGS eng debug \
         -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libgstdash_la_SOURCES) \
	 -:CFLAGS $(DEFS) $(DEFAULT_INCLUDES) $(libgstdash_la_CFLAGS) \
	 -:LDFLAGS $(libgstdash_la_LDFLAGS) \
	           $(libgstdash_la_LIBADD) \
	           -ldl \
	 -:PASSTHROUGH LOCAL_ARM_MODE:=arm \
		       LOCAL_MODULE_PATH:='$$(TARGET_OUT)/lib/gstreamer-0.10' \
	> $@
