include $(top_srcdir)/misc/Makefile.common

LIBTOOL = $(SHELL) $(top_builddir)/libtool
noinst_LTLIBRARIES = libxineutils.la

EXTRA_DIST = ppcasm_string.S ppc_asm.tmpl

if PPC_ARCH
if !HOST_OS_DARWIN
pppc_files = ppcasm_string.S
endif
endif

AM_CFLAGS = $(X_CFLAGS) $(VISIBILITY_FLAG)
AM_CPPFLAGS= -DXINE_LIBRARY_COMPILE

libxineutils_la_SOURCES = $(pppc_files) \
	cpu_accel.c \
	color.c \
	copy.c \
	list.c \
	memcpy.c \
	monitor.c \
	utils.c \
	xine_check.c \
	xine_mutex.c \
	xmllexer.c \
	xine_buffer.c \
	xmlparser.c \
	array.c \
	sorted_array.c \
	pool.c \
	ring_buffer.c

xineinclude_HEADERS = \
	attributes.h \
	compat.h \
	xine_buffer.h \
	xineutils.h \
	xine_mmx.h \
	xmllexer.h \
	xmlparser.h \
	list.h \
	array.h \
	sorted_array.h \
	pool.h \
	ring_buffer.h


noinst_HEADERS = ppcasm_string.h mangle.h

