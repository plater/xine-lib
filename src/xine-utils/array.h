/* 
 * Copyright (C) 2000-2006 the xine project
 * 
 * This file is part of xine, a free video player.
 * 
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: array.h,v 1.1 2006/01/16 08:04:44 tmattern Exp $
 *
 * Array that can grow automatically when you add elements.
 * Inserting an element in the middle of the array implies memory moves.
 */
#ifndef XINE_ARRAY_H
#define XINE_ARRAY_H

/* Array type */
typedef struct xine_array_s xine_array_t;

/* Constructor */
xine_array_t *xine_array_new(size_t initial_size);

/* Destructor */
void xine_array_delete(xine_array_t *array);

/* Returns the number of element stored in the array */
size_t xine_array_size(xine_array_t *array);

/* Removes all elements from an array */
void xine_array_clear(xine_array_t *array);

/* Adds the element at the end of the array */
void xine_array_add(xine_array_t *array, void *value);

/* Inserts an element into an array at the position specified */
void xine_array_insert(xine_array_t *array, unsigned int position, void *value);

/* Removes one element from an array at the position specified */
void xine_array_remove(xine_array_t *array, unsigned int position);

/* Get the element at the position specified */
void *xine_array_get(xine_array_t *array, unsigned int position);

/* Set the element at the position specified */
void xine_array_set(xine_array_t *array, unsigned int position, void *value);

#endif

