include $(top_srcdir)/misc/Makefile.common

# plugins/Makefile.am distributes the plugins that come with tvtime.
# Copyright (C) 2002, 2003, 2004  Billy Biggs
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or (at
# your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

EXTRA_DIST = greedy2frame_template.c greedyh.asm \
	tomsmocomp/SearchLoop0A.inc tomsmocomp/SearchLoopBottom.inc \
	tomsmocomp/SearchLoopEdgeA.inc tomsmocomp/SearchLoopEdgeA8.inc \
	tomsmocomp/SearchLoopOddA.inc tomsmocomp/SearchLoopOddA2.inc \
	tomsmocomp/SearchLoopOddA6.inc tomsmocomp/SearchLoopOddAH.inc \
	tomsmocomp/SearchLoopOddAH2.inc tomsmocomp/SearchLoopTop.inc \
	tomsmocomp/SearchLoopVA.inc tomsmocomp/SearchLoopVAH.inc \
	tomsmocomp/StrangeBob.inc tomsmocomp/TomsMoCompAll.inc \
	tomsmocomp/TomsMoCompAll2.inc tomsmocomp/WierdBob.inc \
	tomsmocomp/tomsmocompmacros.h x86-64_macros.inc

# libpostproc is here so we can use their nice mangle.h
AM_CFLAGS = -I$(top_srcdir)/src/post/deinterlace \
	-I$(top_srcdir)/src/xine-utils

# Avoid "can't find register" failures with -O0, -O2, -O3 (gcc 4.0)
libdeinterlaceplugins_la-kdetv_greedyh.o libdeinterlaceplugins_la-kdetv_greedyh.lo: CFLAGS=$(shell echo @CFLAGS@ | sed -e 's/$$/ -O1/')

noinst_LTLIBRARIES = libdeinterlaceplugins.la

libdeinterlaceplugins_la_SOURCES = \
	double.c \
	greedy.c \
	linear.c \
	linearblend.c \
	vfir.c \
	weave.c \
	greedy2frame.c \
	scalerbob.c \
	kdetv_greedyh.c \
	kdetv_tomsmocomp.c
libdeinterlaceplugins_la_CFLAGS = $(VISIBILITY_FLAG) $(AM_CFLAGS)
libdeinterlaceplugins_la_LDFLAGS = $(xineplug_ldflags)

noinst_HEADERS = plugins.h greedyhmacros.h
