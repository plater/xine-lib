include $(top_srcdir)/misc/Makefile.common

SUBDIRS = cdio MSWindows image

INCLUDES = $(LIBCDIO_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/lib -I$(top_builddir)/lib

libcdio_SRCS = \
	_cdio_bsdi.c \
	_cdio_generic.c \
	_cdio_linux.c \
	_cdio_osx.c \
	_cdio_stdio.c \
	_cdio_stdio.h \
	_cdio_stream.c \
	_cdio_stream.h \
	_cdio_sunos.c \
	cd_types.c \
	cdio.c \
	cdtext.c \
	cdtext_private.h \
	ds.c \
        FreeBSD/freebsd.c \
        FreeBSD/freebsd.h \
        FreeBSD/freebsd_cam.c \
	FreeBSD/freebsd_ioctl.c \
	generic.h \
	image.h \
	image/bincue.c \
	image/cdrdao.c \
	image_common.h \
	image/nrg.c \
	image/nrg.h \
	MSWindows/aspi32.c \
	MSWindows/aspi32.h \
	MSWindows/win32_ioctl.c \
	MSWindows/win32.c \
	MSWindows/win32.h \
	logging.c \
	scsi_mmc.c \
	scsi_mmc_private.h \
	sector.c \
	util.c

libiso9660_SRCS = \
	iso9660.c \
	iso9660_private.h \
	iso9660_fs.c \
	xa.c

EXTRA_DIST = $(libcdio_SRCS) $(libiso9660_SRCS)

if ENABLE_VCD
if HAVE_VCDNAV
else
noinst_LTLIBRARIES = libcdio.la libiso9660.la
libcdio_la_SOURCES = $(libcdio_SRCS)
libcdio_la_LDFLAGS = -avoid-version -module
libiso9660_la_SOURCES = $(libiso9660_SRCS)
libiso9660_la_LDFLAGS = -avoid-version -module
endif
endif

noinst_HEADERS = \
	cdio_assert.h \
	_cdio_stdio.h \
	scsi_mmc.h \
	cdio_private.h \
	_cdio_stream.h \
	iso9660_private.h \
	portable.h
