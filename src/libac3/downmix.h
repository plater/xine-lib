/*
 *
 *  downmix.h
 *    
 *	Copyright (C) Aaron Holtzman - Sept 1999
 *
 *	Originally based on code by Yeqing Deng.
 *
 *  This file is part of ac3dec, a free Dolby AC-3 stream decoder.
 *	
 *  ac3dec is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  ac3dec is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 *
 */

typedef struct dm_par_s {
	float unit;  
	float clev;  
	float slev;          
} dm_par_t;                                                                     

void downmix_3f_2r_to_2ch (float *samples, dm_par_t * dm_par);
void downmix_3f_1r_to_2ch (float *samples, dm_par_t * dm_par);
void downmix_2f_2r_to_2ch (float *samples, dm_par_t * dm_par);
void downmix_2f_1r_to_2ch (float *samples, dm_par_t * dm_par);
void downmix_3f_0r_to_2ch (float *samples, dm_par_t * dm_par);

void stream_sample_2ch_to_s16 (int16_t *s16_samples, float *left, float *right);
void stream_sample_1ch_to_s16 (int16_t *s16_samples, float *center);


