include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

xinepost_LTLIBRARIES = xineplug_post_mosaico.la xineplug_post_switch.la

xineplug_post_mosaico_la_SOURCES = mosaico.c
xineplug_post_mosaico_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_post_mosaico_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_post_mosaico_la_LDFLAGS = $(xineplug_ldflags)

xineplug_post_switch_la_SOURCES = switch.c
xineplug_post_switch_la_LIBADD = $(XINE_LIB) $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_post_switch_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_post_switch_la_LDFLAGS = $(xineplug_ldflags)
