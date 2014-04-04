plugin_LTLIBRARIES = libgsttesseractocr.la

# sources used to compile this plug-in
libgsttesseractocr_la_SOURCES = gsttesseractocr.cpp

libgsttesseractocr_la_CXXFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CXXFLAGS) $(TESSERACTOCR_CFLAGS)

libgsttesseractocr_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) $(TESSERACTOCR_LIBS) \
			$(GSTPB_BASE_LIBS) -lgstvideo-$(GST_API_VERSION)

libgsttesseractocr_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgsttesseractocr_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gsttesseractocr.h

EXTRA_DIST =
