include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

SUBDIRS = plugins

EXTRA_DIST =

xinepost_LTLIBRARIES = xineplug_post_tvtime.la

xineplug_post_tvtime_la_SOURCES = xine_plugin.c \
	deinterlace.c pulldown.c speedy.c tvtime.c
xineplug_post_tvtime_la_LIBADD = $(XINE_LIB) $(LTLIBINTL) $(PTHREAD_LIBS) \
	$(top_builddir)/src/post/deinterlace/plugins/libdeinterlaceplugins.la

xineplug_post_tvtime_la_CFLAGS = $(VISIBILITY_FLAG)
xineplug_post_tvtime_la_LDFLAGS = $(xineplug_ldflags) \
	@IMPURE_TEXT_LDFLAGS@

noinst_HEADERS = deinterlace.h pulldown.h speedtools.h speedy.h tvtime.h
