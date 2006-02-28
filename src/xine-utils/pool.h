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
 * $Id: pool.h,v 1.1 2006/01/27 07:53:28 tmattern Exp $
 *
 * Object Pool
 */

#include <stdlib.h>
#include <inttypes.h>

typedef struct xine_pool_s xine_pool_t;

/* Creates a new pool
 *   object_size:    sizeof(your struct)
 *   create_object:  function called to create an object (can be NULL)
 *   prepare_object: function called to prepare an object to returned to the client (can be NULL)
 *   return_object:  function called to prepare an object to returned to the pool (can be NULL)
 *   delete_object:  function called to delete an object (can be NULL)
 */
xine_pool_t *xine_pool_new(size_t object_size,
                           void (create_object)(void *object),
                           void (prepare_object)(void *object),
                           void (return_object)(void *object),
                           void (delete_object)(void *object));

/* Deletes a pool */
void xine_pool_delete(xine_pool_t *pool);

/* Get an object from the pool */
void *xine_pool_get(xine_pool_t *pool);

/* Returns an object to the pool */
void xine_pool_put(xine_pool_t *pool, void *object);