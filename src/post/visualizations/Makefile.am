include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

EXTRA_DIST = fooviz.c

xinepost_LTLIBRARIES = xineplug_post_visualizations.la

xineplug_post_visualizations_la_SOURCES = \
        visualizations.c fft.c fftscope.c oscope.c fftgraph.c
xineplug_post_visualizations_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS) -lm
xineplug_post_visualizations_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_post_visualizations_la_LDFLAGS = $(xineplug_ldflags)

noinst_HEADERS = fft.h visualizations.h
