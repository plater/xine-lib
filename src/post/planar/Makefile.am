include $(top_builddir)/misc/Makefile.plugins
include $(top_srcdir)/misc/Makefile.common

POSTPROC_INT_LIB = $(top_builddir)/src/libffmpeg/libavcodec/libpostproc/libpostprocess.la

if HAVE_FFMPEG
postproc_lib = $(FFMPEG_POSTPROC_LIBS)
ff_cflags = $(FFMPEG_CFLAGS) $(FFMPEG_POSTPROC_CFLAGS)
else
ff_cflags = -I$(top_srcdir)/src/libffmpeg/libavcodec
postproc_lib = $(POSTPROC_INT_LIB)
postproc_dep = $(postproc_lib)
endif

# -fomit-frame-pointer is always needed. it might cause debug to not
# work, but at least it compiles.
AM_CFLAGS = $(ff_cflags) -fomit-frame-pointer

# Avoid errors with -O0
xineplug_post_planar_la-eq.o xineplug_post_planar_la-eq.lo: CFLAGS=`echo @CFLAGS@ | sed -e 's/-O0\?\s/-Os /g'`
xineplug_post_planar_la-eq2.o xineplug_post_planar_la-eq2.lo: CFLAGS=`echo @CFLAGS@ | sed -e 's/-O0\?\s/-Os /g'`
xineplug_post_planar_la-noise.o xineplug_post_planar_la-noise.lo: CFLAGS=`echo @CFLAGS@ | sed -e 's/-O0\?\s/-Os /g'`

xinepost_LTLIBRARIES = xineplug_post_planar.la

xineplug_post_planar_la_SOURCES = planar.c invert.c expand.c fill.c boxblur.c \
                                  denoise3d.c eq.c eq2.c unsharp.c pp.c noise.c
xineplug_post_planar_la_DEPENDENCIES = $(postproc_dep)
xineplug_post_planar_la_LIBADD = $(XINE_LIB) $(postproc_lib) -lm $(PTHREAD_LIBS) $(LTLIBINTL)
xineplug_post_planar_la_LDFLAGS = $(xineplug_ldflags) \
	@IMPURE_TEXT_LDFLAGS@
xineplug_post_planar_la_CFLAGS = $(VISIBILITY_FLAG) $(AM_CFLAGS)

$(POSTPROC_INT_LIB):
	cd $(top_builddir)/src/libffmpeg/libavcodec/libpostproc && $(MAKE) libpostprocess.la
