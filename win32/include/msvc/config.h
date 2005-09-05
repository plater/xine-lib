/* config.h.  Generated by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Adjusting for MSVC from MinGW:
 *    NLS disabled
 *    x86 disabled
 *    inline defined to __inline
 *    HAVE_SYS_PARAM_H disabled
 *    HAVE_DLFCN_H 1
 *    HAVE_ERRNO_H 1
 *    HAVE_FCNTL_H 1
 *    HAVE_ISOC99_PRAGMA 1
 *    HAVE_MEMCPY 1
 *    HAVE_STDBOOL_H
 *    HAVE_STDIO_H 1
 *    HAVE_VSNPRINTF 1
 *    HAVE_WIN32_CDROM 1
 *    X_DISPLAY_MISSING 1
 *    HAVE_MEMCPY 1
 *    XINE_REL_PLUGINDIR "lib\\xine\\plugins"
 * Disable:
 *  esd, arts, alsa, gnome_vfs, png, sdl, speex, x11, theora
 */


#ifdef inline
/* the strange formatting below is needed to prevent config.status from rewriting it */
#  undef \
     inline
#endif


/* Define this if you're running PowerPC architecture */
/* #undef ARCH_PPC */

/* Define this if you're running SPARC architecture */
/* #undef ARCH_SPARC */

/* Define this if you're running x86 architecture */
/* #undef ARCH_X86 */

/* Define this if you're running x86 architecture */
/* #undef ARCH_X86_64 */

/* maximum supported data alignment */
#define ATTRIBUTE_ALIGNED_MAX 64

/* compiler does lsbf in struct bitfields */
/* #undef BITFIELD_LSBF */

/* Define to one of `_getb67', `GETB67', `getb67' for Cray-2 and Cray-YMP
   systems. This function is required for `alloca.c' support on those systems.
   */
/* #undef CRAY_STACKSEG_END */

/* Define 1 if you are compiling using cygwin */
/* #undef CYGWIN */

/* Define to 1 if using `alloca.c'. */
/* #undef C_ALLOCA */

/* what to put between the brackets for empty arrays */
/* #undef EMPTY_ARRAY_SIZE */

/* Define this if you have a Motorola 74xx CPU */
/* #undef ENABLE_ALTIVEC */

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
/* #undef ENABLE_NLS */

/* Define this if you have Sun UltraSPARC CPU */
/* #undef ENABLE_VIS */

/* Define to select libmad fixed point arithmetic implementation */
/* #undef FPM_64BIT */

/* Define to select libmad fixed point arithmetic implementation */
/* #undef FPM_ARM */

/* Define to select libmad fixed point arithmetic implementation */
/* #undef FPM_DEFAULT */

/* Define to select libmad fixed point arithmetic implementation */
#define FPM_INTEL 1

/* Define to select libmad fixed point arithmetic implementation */
/* #undef FPM_MIPS */

/* Define to select libmad fixed point arithmetic implementation */
/* #undef FPM_PPC */

/* Define to select libmad fixed point arithmetic implementation */
/* #undef FPM_SPARC */

/* Define to 1 if you have `alloca', as a function or macro. */
#define HAVE_ALLOCA 1

/* Define to 1 if you have <alloca.h> and it should be used (not on Ultrix).
   */
/* #undef HAVE_ALLOCA_H */

/* Define this if you have Alsa (libasound) installed */
/* #undef HAVE_ALSA */

/* Define this if you have alsa 0.9.x and more installed */
/* #undef HAVE_ALSA09 */

/* Define this if your asoundlib.h is installed in alsa/ */
/* #undef HAVE_ALSA_ASOUNDLIB_H */

/* Define to 1 if you have the <argz.h> header file. */
/* #undef HAVE_ARGZ_H */

/* Define this if you have ARTS (libartsc) installed */
/* #undef HAVE_ARTS */

/* Define to 1 if you have the <avcodec.h> header file. */
/* #undef HAVE_AVCODEC_H */

/* Define to 1 if you have the `basename' function. */
/* #undef HAVE_BASENAME */

/* Define 1 if you have BSDI-type CD-ROM support */
/* #undef HAVE_BSDI_CDROM */

/* Define to 1 if you have the <byteswap.h> header file. */
/* #undef HAVE_BYTESWAP_H */

/* Define to 1 if you have the `bzero' function. */
/* #undef HAVE_BZERO */

/* Define this if you have CDROM ioctls */
/* #undef HAVE_CDROM_IOCTLS */

/* Define to 1 if you have the <CoreFoundation/CFBase.h> header file. */
/* #undef HAVE_COREFOUNDATION_CFBASE_H */

/* Define 1 if you have Darwin OS X-type CD-ROM support */
/* #undef HAVE_DARWIN_CDROM */

/* Define to 1 if you have the `dcgettext' function. */
/* #undef HAVE_DCGETTEXT */

/* Define this if you have DirectX */
#define HAVE_DIRECTX 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define this if you have a suitable version of libdvdnav */
/* #undef HAVE_DVDNAV */

/* Define to 1 if you have the <dvd.h> header file. */
/* #undef HAVE_DVD_H */

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define this if you have ESD (libesd) installed */
/* #undef HAVE_ESD */

/* Define to 1 if you have the <execinfo.h> header file. */
/* #undef HAVE_EXECINFO_H */

/* Define this if you have linux framebuffer support */
/* #undef HAVE_FB */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `feof_unlocked' function. */
/* #undef HAVE_FEOF_UNLOCKED */

/* Define this if you have ffmpeg library */
/* #undef HAVE_FFMPEG */

/* Define to 1 if you have the `fgets_unlocked' function. */
/* #undef HAVE_FGETS_UNLOCKED */

/* Define 1 if you have FreeBSD CD-ROM support */
/* #undef HAVE_FREEBSD_CDROM */

/* Define to 1 if fseeko (and presumably ftello) exists and is declared. */
/* #undef HAVE_FSEEKO */

/* Define this if you have freetype2 library */
/* #undef HAVE_FT2 */

/* Define to 1 if you have the `getcwd' function. */
#define HAVE_GETCWD 1

/* Define to 1 if you have the `getegid' function. */
/* #undef HAVE_GETEGID */

/* Define to 1 if you have the `geteuid' function. */
/* #undef HAVE_GETEUID */

/* Define to 1 if you have the `getgid' function. */
/* #undef HAVE_GETGID */

/* Define to 1 if you have the `getpagesize' function. */
#define HAVE_GETPAGESIZE 1

/* Define to 1 if you have the `getpwuid_r' function. */
/* #undef HAVE_GETPWUID_R */

/* Define if the GNU gettext() function is already present or preinstalled. */
/* #undef HAVE_GETTEXT */

/* Define to 1 if you have the `getuid' function. */
/* #undef HAVE_GETUID */

/* Define to 1 if you have the <glob.h> header file. */
/* #undef HAVE_GLOB_H */

/* Define this if you have GLU support available */
/* #undef HAVE_GLU */

/* Define this if you have GLut support available */
/* #undef HAVE_GLUT */

/* Define this if you have gnome-vfs installed */
/* #undef HAVE_GNOME_VFS */

/* Define to 1 if you have the `hstrerror' function. */
/* #undef HAVE_HSTRERROR */

/* Define if you have the iconv() function. */
/* #undef HAVE_ICONV */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if the system has the type `int_fast8_t'. */
#define HAVE_INT_FAST8_T 1

/* Define to 1 if you have the <IOKit/IOKitLib.h> header file. */
/* #undef HAVE_IOKIT_IOKITLIB_H */

/* Define this if you have ip_mreqn in netinet/in.h */
/* #undef HAVE_IP_MREQN */

/* Define this if you have a usable IRIX al interface available */
/* #undef HAVE_IRIXAL */

/* Supports ISO _Pragma() macro */
#define HAVE_ISOC99_PRAGMA 1

/* Define this if you have kernel statistics available via kstat interface */
/* #undef HAVE_KSTAT */

/* Define if you have <langinfo.h> and nl_langinfo(CODESET). */
/* #undef HAVE_LANGINFO_CODESET */

/* Define if your <locale.h> file defines LC_MESSAGES. */
/* #undef HAVE_LC_MESSAGES */

/* Define this if you have libfame mpeg encoder installed (fame.sf.net) */
/* #undef HAVE_LIBFAME */

/* Define to 1 if you have the <libgen.h> header file. */
/* #undef HAVE_LIBGEN_H */

/* Define to 1 if you have the <libintl.h> header file. */
/* #undef HAVE_LIBINTL_H */

/* Define this if you have png library */
/* #undef HAVE_LIBPNG */

/* Define to 1 if you have the `posix4' library (-lposix4). */
/* #undef HAVE_LIBPOSIX4 */

/* Define this if you have librte mpeg encoder installed (zapping.sf.net) */
/* #undef HAVE_LIBRTE */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define 1 if you have Linux-type CD-ROM support */
/* #undef HAVE_LINUX_CDROM */

/* Define to 1 if you have the <linux/cdrom.h> header file. */
/* #undef HAVE_LINUX_CDROM_H */

/* Define 1 if timeout is in cdrom_generic_command struct */
/* #undef HAVE_LINUX_CDROM_TIMEOUT */

/* Define to 1 if you have the <linux/version.h> header file. */
/* #undef HAVE_LINUX_VERSION_H */

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define this if the 'lrintf' function is declared in math.h */
#define HAVE_LRINTF 1

/* Define to 1 if you have the <malloc.h> header file. */
#define HAVE_MALLOC_H 1

/* Define to 1 if you have the `memcpy' function. */
#define HAVE_MEMCPY 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `mempcpy' function. */
#define HAVE_MEMPCPY 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define this if you have mlib installed */
/* #undef HAVE_MLIB */

/* Define to 1 if you have a working `mmap' system call. */
/* #undef HAVE_MMAP */

/* define this if you have libmodplug installed */
/* #undef HAVE_MODPLUG */

/* Define to 1 if you have the `munmap' function. */
/* #undef HAVE_MUNMAP */

/* Define to 1 if you have the `nanosleep' function. */
/* #undef HAVE_NANOSLEEP */

/* Define this if you have libfame 0.8.10 or above */
/* #undef HAVE_NEW_LIBFAME */

/* Define to 1 if you have the <nl_types.h> header file. */
/* #undef HAVE_NL_TYPES_H */

/* Define this if you have OpenGL support available */
/* #undef HAVE_OPENGL */

/* Define to 1 if you have the <postprocess.h> header file. */
/* #undef HAVE_POSTPROCESS_H */

/* Define to 1 if you have the `putenv' function. */
#define HAVE_PUTENV 1

/* Define this if you have SDL library installed */
/* #undef HAVE_SDL */

/* Define to 1 if you have the `setenv' function. */
/* #undef HAVE_SETENV */

/* Define to 1 if you have the `setlocale' function. */
#define HAVE_SETLOCALE 1

/* Define to 1 if you have the `sigaction' function. */
/* #undef HAVE_SIGACTION */

/* Define to 1 if you have the `sigset' function. */
/* #undef HAVE_SIGSET */

/* Define to 1 if you have the `snprintf' function. */
/* #undef HAVE_SNPRINTF */

/* Define 1 if you have Solaris CD-ROM support */
/* #undef HAVE_SOLARIS_CDROM */

/* Define this if you have speex */
/* #undef HAVE_SPEEX */

/* Define to 1 if speex headers are eg. <speex/speex.h> */
/* #undef HAVE_SPEEX_SUBDIR */

/* Define to 1 if you have the <stdbool.h> header file. */
#define HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stddef.h> header file. */
#define HAVE_STDDEF_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `stpcpy' function. */
/* #undef HAVE_STPCPY */

/* Define to 1 if you have the `strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strpbrk' function. */
#define HAVE_STRPBRK 1

/* Define to 1 if you have the `strsep' function. */
/* #undef HAVE_STRSEP */

/* Define to 1 if you have the `strtoul' function. */
#define HAVE_STRTOUL 1

/* Define this if your asoundlib.h is installed in sys/ */
/* #undef HAVE_SYS_ASOUNDLIB_H */

/* Define to 1 if you have the <sys/cdio.h> header file. */
/* #undef HAVE_SYS_CDIO_H */

/* Define to 1 if you have the <sys/ioctl.h> header file. */
/* #undef HAVE_SYS_IOCTL_H */

/* Define to 1 if you have the <sys/mixer.h> header file. */
/* #undef HAVE_SYS_MIXER_H */

/* Define to 1 if you have the <sys/mman.h> header file. */
/* #undef HAVE_SYS_MMAN_H */

/* Define to 1 if you have the <sys/param.h> header file. */
/* #undef HAVE_SYS_PARAM_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/times.h> header file. */
/* #undef HAVE_SYS_TIMES_H */

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define this if you have theora */
/* #undef HAVE_THEORA */

/* Define if struct tm has the tm_gmtoff member. */
/* #undef HAVE_TM_GMTOFF */

/* Define to 1 if you have the `tsearch' function. */
/* #undef HAVE_TSEARCH */

/* Define to 1 if you have the <ucontext.h> header file. */
/* #undef HAVE_UCONTEXT_H */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define this if you have a suitable version of libcdio/libvcd */
/* #undef HAVE_VCDNAV */

/* Define to 1 if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define to 1 if you have the `vsscanf' function. */
#define HAVE_VSSCANF 1

/* Define 1 if you have MinGW CD-ROM support */
#define HAVE_WIN32_CDROM 1

/* Define this if you have X11R6 installed */
/* #undef HAVE_X11 */

/* Define this if you have libXinerama installed */
/* #undef HAVE_XINERAMA */

/* Define this if you have libXv installed */
/* #undef HAVE_XV */

/* Define this if you have libXvMC installed */
/* #undef HAVE_XVMC */

/* Define this if you have libXvMC.a */
/* #undef HAVE_XVMC_STATIC */

/* Define this if you have libXv.a */
/* #undef HAVE_XV_STATIC */

/* Define to 1 if you have the `__argz_count' function. */
/* #undef HAVE___ARGZ_COUNT */

/* Define to 1 if you have the `__argz_next' function. */
/* #undef HAVE___ARGZ_NEXT */

/* Define to 1 if you have the `__argz_stringify' function. */
/* #undef HAVE___ARGZ_STRINGIFY */

/* host os/cpu identifier */
/* #undef HOST_ARCH */

/* Define this if built on Mac OS X/Darwin */
/* #undef HOST_OS_DARWIN */

/* Define as const if the declaration of iconv() needs const. */
/* #undef ICONV_CONST */

/* Define this if you have mlib installed */
/* #undef LIBMPEG2_MLIB */

/* Define 1 if you are compiling using MinGW */
/* #undef MINGW32 */

/* Define this if you want to load mlib lazily */
/* #undef MLIB_LAZYLOAD */

/* Name of package */
#define PACKAGE "xine-lib"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* The size of a `int', as computed by sizeof. */
/* #undef SIZEOF_INT */

/* The size of a `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* The size of a `long long', as computed by sizeof. */
/* #undef SIZEOF_LONG_LONG */

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at run-time.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "1-rc6"

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef WORDS_BIGENDIAN */

/* Path where aclocal m4 files will be. */
#define XINE_ACFLAGS "-I ${prefix}/share/aclocal"

/* Define this to osd fonts dir location */
#define XINE_FONTDIR xine_get_fontdir()

/* Path where catalog files will be. */
#define XINE_LOCALEDIR xine_get_localedir()

/* xine major version number */
#define XINE_MAJOR 1

/* xine minor version number */
#define XINE_MINOR 0

/* Define this to plugins directory location */
#define XINE_PLUGINDIR xine_get_plugindir()

/* xine sub version number */
#define XINE_SUB 0

/* Define to 1 if the X Window System is missing or not being used. */
#define X_DISPLAY_MISSING 1

/* enable warnings about being development release */
/* #undef _DEVELOPMENT_ */

/* enable GNU libc extension */
/* #undef _GNU_SOURCE */

/* Define this if you are ISO C9X compliant */
#define _ISOC9X_SOURCE 1

/* Define to 1 to make fseeko visible on some hosts (e.g. glibc 2.2). */
/* #undef _LARGEFILE_SOURCE */

/* Define this if you're running x86 architecture */
#define __i386__ 1

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#define inline __inline

/* Define to `long' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define the real type of socklen_t */
#define socklen_t size_t

/* define ssize_t to __int64 if it's missing in default includes */
#define ssize_t __int64

/* Define this to font directory relative to prefix */
#define XINE_REL_FONTDIR "share\\xine\\libxine1\\fonts"

/* Define this to font directory relative to prefix */
#define XINE_REL_LOCALEDIR "share\\locale"

/* Define this to plugin directory relative to execution prefix */
#define XINE_REL_PLUGINDIR "lib\\xine\\plugins"


#include "os_internal.h"
