include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

xineplug_LTLIBRARIES = xineplug_decode_spucc.la

xineplug_decode_spucc_la_SOURCES =  cc_decoder.c xine_cc_decoder.c
xineplug_decode_spucc_la_CFLAGS = $(VISIBILITY_FLAG) $(AM_CFLAGS) -fno-strict-aliasing
xineplug_decode_spucc_la_LIBADD = $(XINE_LIB) $(LTLIBINTL)
xineplug_decode_spucc_la_LDFLAGS = $(xineplug_ldflags)

noinst_HEADERS = cc_decoder.h
