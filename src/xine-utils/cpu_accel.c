/*
 * cpu_accel.c
 * Copyright (C) 1999-2001 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <signal.h>
#include <setjmp.h>
#include <dlfcn.h>

#if defined (__SVR4) && defined (__sun)
#include <sys/systeminfo.h>
#endif

#define LOG_MODULE "cpu_accel"
#define LOG_VERBOSE
/*
#define LOG
*/

#include "xineutils.h"

#if defined(PIC) && ! defined(__PIC__)
#define __PIC__
#endif

#if defined(ARCH_X86) || defined(ARCH_X86_64)

#ifndef __x86_64__
static jmp_buf sigill_return;

static void sigill_handler (int n) {
  longjmp(sigill_return, 1);
}
#endif

static uint32_t arch_accel (void)
{
  uint32_t caps;

#ifdef __x86_64__
  /* No need to test for this on AMD64, we know what the
     platform has.  */
  caps = MM_ACCEL_X86_MMX | MM_ACCEL_X86_SSE | MM_ACCEL_X86_MMXEXT | MM_ACCEL_X86_SSE2;
#else

#ifndef _MSC_VER
  uint32_t eax, ebx, ecx, edx;
  int AMD;

  caps = 0;
#ifndef __PIC__
#define cpuid(op,eax,ebx,ecx,edx)       \
    __asm__ ("cpuid"                    \
         : "=a" (eax),                  \
           "=b" (ebx),                  \
           "=c" (ecx),                  \
           "=d" (edx)                   \
         : "a" (op)                     \
         : "cc")
#else   /* PIC version : save ebx */
#define cpuid(op,eax,ebx,ecx,edx)       \
    __asm__ ("pushl %%ebx\n\t"          \
         "cpuid\n\t"                    \
         "movl %%ebx,%1\n\t"            \
         "popl %%ebx"                   \
         : "=a" (eax),                  \
           "=r" (ebx),                  \
           "=c" (ecx),                  \
           "=d" (edx)                   \
         : "a" (op)                     \
         : "cc")
#endif

  __asm__ ("pushfl\n\t"
       "pushfl\n\t"
       "popl %0\n\t"
       "movl %0,%1\n\t"
       "xorl $0x200000,%0\n\t"
       "pushl %0\n\t"
       "popfl\n\t"
       "pushfl\n\t"
       "popl %0\n\t"
       "popfl"
       : "=r" (eax),
       "=r" (ebx)
       :
       : "cc");

  if (eax == ebx) {
    /* no cpuid */
    return 0;
  }

  cpuid (0x00000000, eax, ebx, ecx, edx);
  if (!eax) {
    /* vendor string only */
    return 0;
  }

  AMD = (ebx == 0x68747541) && (ecx == 0x444d4163) && (edx == 0x69746e65);

  cpuid (0x00000001, eax, ebx, ecx, edx);
  if (edx & 0x00800000) {
    /* MMX */
    caps |= MM_ACCEL_X86_MMX;
  }

  if (edx & 0x02000000) {
    /* SSE - identical to AMD MMX extensions */
    caps |= MM_ACCEL_X86_SSE | MM_ACCEL_X86_MMXEXT;
  }

  if (edx & 0x04000000) {
    /* SSE2 */
    caps |= MM_ACCEL_X86_SSE2;
  }

  cpuid (0x80000000, eax, ebx, ecx, edx);
  if (eax >= 0x80000001) {
    cpuid (0x80000001, eax, ebx, ecx, edx);

    if (edx & 0x80000000) {
      /* AMD 3DNow  extensions */
      caps |= MM_ACCEL_X86_3DNOW;
    }

    if (AMD && (edx & 0x00400000)) {
      /* AMD MMX extensions */
      caps |= MM_ACCEL_X86_MMXEXT;
    }
  }
#else
  caps = 0;
#endif /* _MSC_VER */

  /* test OS support for SSE */
  if (caps & MM_ACCEL_X86_SSE) {
    void (*old_sigill_handler)(int);

    old_sigill_handler = signal (SIGILL, sigill_handler);

    if (setjmp(sigill_return)) {
      lprintf("OS doesn't support SSE instructions.\n");
      caps &= ~(MM_ACCEL_X86_SSE|MM_ACCEL_X86_SSE2);
    } else {
      __asm__ volatile ("xorps %xmm0, %xmm0");
    }

    signal(SIGILL, old_sigill_handler);
  }

#endif /* x86_64 */

  return caps;
}

#endif /* ARCH_X86 */

#if defined(ARCH_PPC) && defined(ENABLE_ALTIVEC)
static sigjmp_buf jmpbuf;
static volatile sig_atomic_t canjump = 0;

static void sigill_handler (int sig)
{
  if (!canjump) {
    signal (sig, SIG_DFL);
    raise (sig);
  }

  canjump = 0;
  siglongjmp (jmpbuf, 1);
}

static uint32_t arch_accel (void)
{
  /* FIXME: Autodetect cache line size via AUX ELF vector or otherwise */
  uint32_t flags = 0;

  signal (SIGILL, sigill_handler);
  if (sigsetjmp (jmpbuf, 1)) {
    signal (SIGILL, SIG_DFL);
    return flags;
  }

  canjump = 1;
#ifndef HOST_OS_DARWIN
  __asm__ volatile ("mtspr 256, %0\n\t"
                    "vand %%v0, %%v0, %%v0"
                    :
                    : "r" (-1));
#else
  __asm__ volatile ("mtspr 256, r0\n\t"
                    "vand v0, v0, v0"
                    :
                    : "r" (-1));
#endif

  signal (SIGILL, SIG_DFL);
  return flags|MM_ACCEL_PPC_ALTIVEC;
}
#endif /* ARCH_PPC */

#if defined(ARCH_SPARC) && defined(ENABLE_VIS)
#if defined (__SVR4) && defined (__sun)
static uint32_t arch_accel (void)
{
  uint32_t flags = 0;
  long len;
  char isalist_[257], *isalist, *s1, *last, *token;

  len = sysinfo(SI_ISALIST, isalist_, 257);

  if (len > 257) {
    isalist = malloc(len);
    sysinfo(SI_ISALIST, isalist, len);
  }
  else {
    isalist = isalist_;
  }

  s1 = isalist;
  while (token = strtok_r(s1, " ", &last)) {
    if (strlen(token) > 4) {
      if (strcmp(token + (strlen(token) - 4), "+vis") == 0) {
        flags |= MM_ACCEL_SPARC_VIS;
      }
    }

    if (strlen(token) > 5) {
      if (strcmp(token + (strlen(token) - 5), "+vis2") == 0) {
        flags |= MM_ACCEL_SPARC_VIS2;
      }
    }

    s1 = NULL;
  }

  if (isalist != isalist_) {
    free(isalist);
  }
  return flags;
}
#else
static sigjmp_buf jmpbuf;
static volatile sig_atomic_t canjump = 0;

static void sigill_handler (int sig)
{
  if (!canjump) {
    signal(sig, SIG_DFL);
    raise(sig);
  }

  canjump = 0;
  siglongjmp(jmpbuf, 1);
}

static uint32_t arch_accel (void)
{
  uint32_t flags = 0;

  signal(SIGILL, sigill_handler);
  if (sigsetjmp(jmpbuf, 1)) {
    signal(SIGILL, SIG_DFL);
    return flags;
  }

  canjump = 1;

  /* pdist %f0, %f0, %f0 */
  __asm__ __volatile__(".word\t0x81b007c0");

  canjump = 0;
  flags |= MM_ACCEL_SPARC_VIS;

  if (sigsetjmp(jmpbuf, 1)) {
    signal(SIGILL, SIG_DFL);
    return flags;
  }

  canjump = 1;

  /* edge8n %g0, %g0, %g0 */
  __asm__ __volatile__(".word\t0x81b00020");

  canjump = 0;
  flags |= MM_ACCEL_SPARC_VIS2;

  signal(SIGILL, SIG_DFL);
  return flags;
}
#endif
#endif /* ARCH_SPARC */

uint32_t xine_mm_accel (void)
{
  static int initialized = 0;
  static uint32_t accel = 0;

  if (!initialized) {
#ifdef HAVE_MLIB
#ifdef MLIB_LAZYLOAD
    void *hndl;

    if ((hndl = dlopen("libmlib.so.2", RTLD_LAZY | RTLD_GLOBAL | RTLD_NODELETE)) != NULL) {
      dlclose(hndl);
      accel |= MM_ACCEL_MLIB;
    }
#else
    accel |= MM_ACCEL_MLIB;
#endif
#endif

#if defined(ARCH_X86) || defined(ARCH_X86_64) || (defined(ARCH_PPC) && defined(ENABLE_ALTIVEC)) || (defined(ARCH_SPARC) && defined(ENABLE_VIS))
    accel |= arch_accel();
#endif

    if(getenv("XINE_NO_ACCEL")) {
      accel = 0;
    }

    initialized = 1;
  }

  return accel;
}
