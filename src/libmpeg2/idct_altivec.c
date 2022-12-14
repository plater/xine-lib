/*
 * idct_altivec.c
 * Copyright (C) 2000-2002 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#if defined (ARCH_PPC) && defined (ENABLE_ALTIVEC)

#include <altivec.h>

#include <inttypes.h>

#include "mpeg2_internal.h"
#include "xineutils.h"

#define vector_s16_t vector signed short
#define vector_u16_t vector unsigned short
#define vector_s8_t vector signed char
#define vector_u8_t vector unsigned char
#define vector_s32_t vector signed int
#define vector_u32_t vector unsigned int

#define IDCT_HALF					\
    /* 1st stage */					\
    t1 = vec_mradds (a1, vx7, vx1 );			\
    t8 = vec_mradds (a1, vx1, vec_subs (zero, vx7));	\
    t7 = vec_mradds (a2, vx5, vx3);			\
    t3 = vec_mradds (ma2, vx3, vx5);			\
							\
    /* 2nd stage */					\
    t5 = vec_adds (vx0, vx4);				\
    t0 = vec_subs (vx0, vx4);				\
    t2 = vec_mradds (a0, vx6, vx2);			\
    t4 = vec_mradds (a0, vx2, vec_subs (zero,vx6));	\
    t6 = vec_adds (t8, t3);				\
    t3 = vec_subs (t8, t3);				\
    t8 = vec_subs (t1, t7);				\
    t1 = vec_adds (t1, t7);				\
							\
    /* 3rd stage */					\
    t7 = vec_adds (t5, t2);				\
    t2 = vec_subs (t5, t2);				\
    t5 = vec_adds (t0, t4);				\
    t0 = vec_subs (t0, t4);				\
    t4 = vec_subs (t8, t3);				\
    t3 = vec_adds (t8, t3);				\
							\
    /* 4th stage */					\
    vy0 = vec_adds (t7, t1);				\
    vy7 = vec_subs (t7, t1);				\
    vy1 = vec_mradds (c4, t3, t5);			\
    vy6 = vec_mradds (mc4, t3, t5);			\
    vy2 = vec_mradds (c4, t4, t0);			\
    vy5 = vec_mradds (mc4, t4, t0);			\
    vy3 = vec_adds (t2, t6);				\
    vy4 = vec_subs (t2, t6);

#define IDCT								\
    vector_s16_t vx0, vx1, vx2, vx3, vx4, vx5, vx6, vx7;		\
    vector_s16_t vy0, vy1, vy2, vy3, vy4, vy5, vy6, vy7;		\
    vector_s16_t a0, a1, a2, ma2, c4, mc4, zero, bias;			\
    vector_s16_t t0, t1, t2, t3, t4, t5, t6, t7, t8;			\
    vector_u16_t shift;							\
									\
    c4 = vec_splat (constants[0], 0);					\
    a0 = vec_splat (constants[0], 1);					\
    a1 = vec_splat (constants[0], 2);					\
    a2 = vec_splat (constants[0], 3);					\
    mc4 = vec_splat (constants[0], 4);					\
    ma2 = vec_splat (constants[0], 5);					\
    bias = (vector_s16_t)vec_splat ((vector_s32_t)constants[0], 3);	\
									\
    zero = vec_splat_s16 (0);						\
    shift = vec_splat_u16 (4);						\
									\
    vx0 = vec_mradds (vec_sl (block[0], shift), constants[1], zero);	\
    vx1 = vec_mradds (vec_sl (block[1], shift), constants[2], zero);	\
    vx2 = vec_mradds (vec_sl (block[2], shift), constants[3], zero);	\
    vx3 = vec_mradds (vec_sl (block[3], shift), constants[4], zero);	\
    vx4 = vec_mradds (vec_sl (block[4], shift), constants[1], zero);	\
    vx5 = vec_mradds (vec_sl (block[5], shift), constants[4], zero);	\
    vx6 = vec_mradds (vec_sl (block[6], shift), constants[3], zero);	\
    vx7 = vec_mradds (vec_sl (block[7], shift), constants[2], zero);	\
									\
    IDCT_HALF								\
									\
    vx0 = vec_mergeh (vy0, vy4);					\
    vx1 = vec_mergel (vy0, vy4);					\
    vx2 = vec_mergeh (vy1, vy5);					\
    vx3 = vec_mergel (vy1, vy5);					\
    vx4 = vec_mergeh (vy2, vy6);					\
    vx5 = vec_mergel (vy2, vy6);					\
    vx6 = vec_mergeh (vy3, vy7);					\
    vx7 = vec_mergel (vy3, vy7);					\
									\
    vy0 = vec_mergeh (vx0, vx4);					\
    vy1 = vec_mergel (vx0, vx4);					\
    vy2 = vec_mergeh (vx1, vx5);					\
    vy3 = vec_mergel (vx1, vx5);					\
    vy4 = vec_mergeh (vx2, vx6);					\
    vy5 = vec_mergel (vx2, vx6);					\
    vy6 = vec_mergeh (vx3, vx7);					\
    vy7 = vec_mergel (vx3, vx7);					\
									\
    vx0 = vec_adds (vec_mergeh (vy0, vy4), bias);			\
    vx1 = vec_mergel (vy0, vy4);					\
    vx2 = vec_mergeh (vy1, vy5);					\
    vx3 = vec_mergel (vy1, vy5);					\
    vx4 = vec_mergeh (vy2, vy6);					\
    vx5 = vec_mergel (vy2, vy6);					\
    vx6 = vec_mergeh (vy3, vy7);					\
    vx7 = vec_mergel (vy3, vy7);					\
									\
    IDCT_HALF								\
									\
    shift = vec_splat_u16 (6);						\
    vx0 = vec_sra (vy0, shift);						\
    vx1 = vec_sra (vy1, shift);						\
    vx2 = vec_sra (vy2, shift);						\
    vx3 = vec_sra (vy3, shift);						\
    vx4 = vec_sra (vy4, shift);						\
    vx5 = vec_sra (vy5, shift);						\
    vx6 = vec_sra (vy6, shift);						\
    vx7 = vec_sra (vy7, shift);

#if defined( __APPLE_CC__ ) && defined( __APPLE_ALTIVEC__ ) /* apple */
#define VEC_S16(a,b,c,d,e,f,g,h) (vector_s16_t) (a, b, c, d, e, f, g, h)
#else			/* gnu */
#define VEC_S16(a,b,c,d,e,f,g,h) (vector_s16_t) {a, b, c, d, e, f, g, h}
#endif

static vector_s16_t constants[5] = {
    VEC_S16(23170, 13573, 6518, 21895, -23170, -21895, 32, 31),
    VEC_S16(16384, 22725, 21407, 19266, 16384, 19266, 21407, 22725),
    VEC_S16(22725, 31521, 29692, 26722, 22725, 26722, 29692, 31521),
    VEC_S16(21407, 29692, 27969, 25172, 21407, 25172, 27969, 29692),
    VEC_S16(19266, 26722, 25172, 22654, 19266, 22654, 25172, 26722)
};

void mpeg2_idct_copy_altivec (vector_s16_t * block, unsigned char * dest,
			      int stride)
{
    vector_u8_t tmp;

    IDCT

#define COPY(dest,src)						\
    tmp = vec_packsu (src, src);				\
    vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);	\
    vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);

    COPY (dest, vx0)	dest += stride;
    COPY (dest, vx1)	dest += stride;
    COPY (dest, vx2)	dest += stride;
    COPY (dest, vx3)	dest += stride;
    COPY (dest, vx4)	dest += stride;
    COPY (dest, vx5)	dest += stride;
    COPY (dest, vx6)	dest += stride;
    COPY (dest, vx7)
    memset (block, 0, 64 * sizeof (signed short));
}

void mpeg2_idct_add_altivec (vector_s16_t * block, unsigned char * dest,
			     int stride)
{
    vector_u8_t tmp;
    vector_s16_t tmp2, tmp3;
    vector_u8_t perm0;
    vector_u8_t perm1;
    vector_u8_t p0, p1, p;

    IDCT

    p0 = vec_lvsl (0, dest);
    p1 = vec_lvsl (stride, dest);
    p = vec_splat_u8 (-1);
    perm0 = vec_mergeh (p, p0);
    perm1 = vec_mergeh (p, p1);

#define ADD(dest,src,perm)						\
    /* *(uint64_t *)&tmp = *(uint64_t *)dest; */			\
    tmp = vec_ld (0, dest);						\
    tmp2 = (vector_s16_t)vec_perm (tmp, (vector_u8_t)zero, perm);	\
    tmp3 = vec_adds (tmp2, src);					\
    tmp = vec_packsu (tmp3, tmp3);					\
    vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);		\
    vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);

    ADD (dest, vx0, perm0)	dest += stride;
    ADD (dest, vx1, perm1)	dest += stride;
    ADD (dest, vx2, perm0)	dest += stride;
    ADD (dest, vx3, perm1)	dest += stride;
    ADD (dest, vx4, perm0)	dest += stride;
    ADD (dest, vx5, perm1)	dest += stride;
    ADD (dest, vx6, perm0)	dest += stride;
    ADD (dest, vx7, perm1)
    memset (block, 0, 64 * sizeof (signed short));
}

void mpeg2_idct_altivec_init (void)
{
    int i, j;

    /* the altivec idct uses a transposed input, so we patch scan tables */
    for (i = 0; i < 64; i++) {
	j = mpeg2_scan_norm[i];
	mpeg2_scan_norm[i] = (j >> 3) | ((j & 7) << 3);
	j = mpeg2_scan_alt[i];
	mpeg2_scan_alt[i] = (j >> 3) | ((j & 7) << 3);
    }
}

#endif	/* ARCH_PPC && ENABLED_ALTIVEC */

