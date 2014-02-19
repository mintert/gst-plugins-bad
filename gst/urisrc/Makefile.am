plugin_LTLIBRARIES = libgsturisrc.la

libgsturisrc_la_SOURCES = gsturisrc.c gsturisrc.h

libgsturisrc_la_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS)
libgsturisrc_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS)
libgsturisrc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgsturisrc_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

Android.mk: Makefile.am $(BUILT_SOURCES)
	androgenizer \
	-:PROJECT libgsturisrc -:SHARED libgsturisrc \
	 -:TAGS eng debug \
         -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libgsturisrc_la_SOURCES) \
	 -:CFLAGS $(DEFS) $(DEFAULT_INCLUDES) $(libgsturisrc_la_CFLAGS) \
	 -:LDFLAGS $(libgsturisrc_la_LDFLAGS) \
	           $(libgsturisrc_la_LIBADD) \
	           -ldl \
	 -:PASSTHROUGH LOCAL_ARM_MODE:=arm \
		       LOCAL_MODULE_PATH:='$$(TARGET_OUT)/lib/gstreamer-0.10' \
	> $@
