/*
 * Copyright (C) 2000-2002 the xine project
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
 * $Id: buffer.c,v 1.21 2003/01/26 23:31:13 f1rmb Exp $
 *
 *
 * contents:
 *
 * buffer_entry structure - serves as a transport encapsulation
 *   of the mpeg audio/video data through xine
 *
 * free buffer pool management routines
 *
 * FIFO buffer structures/routines
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include "buffer.h"
#include "xineutils.h"
#include "xine_internal.h"

/*
 * put a previously allocated buffer element back into the buffer pool
 */
static void buffer_pool_free (buf_element_t *element) {

  fifo_buffer_t *this = (fifo_buffer_t *) element->source;

  pthread_mutex_lock (&this->buffer_pool_mutex);

  element->next = this->buffer_pool_top;
  this->buffer_pool_top = element;

  this->buffer_pool_num_free++;

  pthread_cond_signal (&this->buffer_pool_cond_not_empty);

  pthread_mutex_unlock (&this->buffer_pool_mutex);
}

/* 
 * helper function to release buffer pool lock
 * in case demux thread is cancelled
 */

void pool_release_lock (void *arg) {
   
  pthread_mutex_t *mutex = (pthread_mutex_t *) arg;

  /* printf ("pool release lock\n"); */

  pthread_mutex_unlock (mutex);

}

/*
 * allocate a buffer from buffer pool
 */

static buf_element_t *buffer_pool_alloc (fifo_buffer_t *this) {
  
  buf_element_t *buf;

  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);

  pthread_cleanup_push( pool_release_lock, &this->buffer_pool_mutex);

  pthread_mutex_lock (&this->buffer_pool_mutex);

  while (!this->buffer_pool_top) {
    pthread_cond_wait (&this->buffer_pool_cond_not_empty, &this->buffer_pool_mutex);
  }

  buf = this->buffer_pool_top;
  this->buffer_pool_top = this->buffer_pool_top->next;
  this->buffer_pool_num_free--;

  pthread_cleanup_pop (0); 

  pthread_mutex_unlock (&this->buffer_pool_mutex);

  /* needed because cancellation points defined by POSIX
     (eg. 'read') would leak allocated buffers */
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
  
  /* set sane values to the newly allocated buffer */
  buf->content = buf->mem; /* 99% of demuxers will want this */
  buf->pts = 0;
  buf->size = 0;
  buf->decoder_flags = 0;
  extra_info_reset( buf->extra_info );

  return buf;
}

/*
 * allocate a buffer from buffer pool - may fail if none is available
 */

static buf_element_t *buffer_pool_try_alloc (fifo_buffer_t *this) {
  
  buf_element_t *buf;

  pthread_mutex_lock (&this->buffer_pool_mutex);

  if (this->buffer_pool_top) {

    buf = this->buffer_pool_top;
    this->buffer_pool_top = this->buffer_pool_top->next;
    this->buffer_pool_num_free--;
  
  } else {
    
    buf = NULL;
    
  }

  pthread_mutex_unlock (&this->buffer_pool_mutex);

  /* set sane values to the newly allocated buffer */
  if( buf ) {
    buf->content = buf->mem; /* 99% of demuxers will want this */
    buf->pts = 0;
    buf->size = 0;
    buf->decoder_flags = 0;
    extra_info_reset( buf->extra_info );
  }
  return buf;
}


/*
 * append buffer element to fifo buffer
 */
static void fifo_buffer_put (fifo_buffer_t *fifo, buf_element_t *element) {
  
  pthread_mutex_lock (&fifo->mutex);

  if (fifo->last) 
    fifo->last->next = element;
  else 
    fifo->first = element;

  fifo->last = element;
  element->next = NULL;
  fifo->fifo_size++;
  fifo->data_size += element->size;

  pthread_cond_signal (&fifo->not_empty);

  pthread_mutex_unlock (&fifo->mutex);
}

/*
 * insert buffer element to fifo buffer (demuxers MUST NOT call this one)
 */
static void fifo_buffer_insert (fifo_buffer_t *fifo, buf_element_t *element) {
  
  pthread_mutex_lock (&fifo->mutex);

  element->next = fifo->first;
  fifo->first = element;
  
  if( !fifo->last )
    fifo->last = element;
    
  fifo->fifo_size++;
  fifo->data_size += element->size;

  pthread_cond_signal (&fifo->not_empty);

  pthread_mutex_unlock (&fifo->mutex);
}


/*
 * get element from fifo buffer
 */
static buf_element_t *fifo_buffer_get (fifo_buffer_t *fifo) {

  buf_element_t *buf;
  
  pthread_mutex_lock (&fifo->mutex);

  while (fifo->first==NULL) {
    pthread_cond_wait (&fifo->not_empty, &fifo->mutex);
  }

  buf = fifo->first;

  fifo->first = fifo->first->next;
  if (fifo->first==NULL)
    fifo->last = NULL;

  fifo->fifo_size--;
  fifo->data_size -= buf->size;

  pthread_mutex_unlock (&fifo->mutex);

  return buf;
}

/*
 * clear buffer (put all contained buffer elements back into buffer pool)
 */
static void fifo_buffer_clear (fifo_buffer_t *fifo) {
  
  buf_element_t *buf, *next, *prev;

  pthread_mutex_lock (&fifo->mutex);

  buf = fifo->first;
  prev = NULL;

  while (buf != NULL) {

    next = buf->next;

    if ((buf->type & BUF_MAJOR_MASK) !=  BUF_CONTROL_BASE) {
      /* remove this buffer */

      if (prev)
	prev->next = next;
      else
	fifo->first = next;
      
      if (!next)
	fifo->last = prev;
      
      fifo->fifo_size--;

      buf->free_buffer(buf);
    } else
      prev = buf;
    
    buf = next;
  }

  /*printf("Free buffers after clear: %d\n", fifo->buffer_pool_num_free);*/
  pthread_mutex_unlock (&fifo->mutex);
}

/*
 * Return the number of elements in the fifo buffer
 */
static int fifo_buffer_size (fifo_buffer_t *this) {
  int size;

  pthread_mutex_lock(&this->mutex);
  size = this->fifo_size;
  pthread_mutex_unlock(&this->mutex);

  return size;
}

/*
 * Return the amount of the data in the fifo buffer
 */
static uint32_t fifo_buffer_data_size (fifo_buffer_t *this) {
  int data_size;

  pthread_mutex_lock(&this->mutex);
  data_size = this->data_size;
  pthread_mutex_unlock(&this->mutex);

  return data_size;
}

/*
 * Destroy the buffer
 */
static void fifo_buffer_dispose (fifo_buffer_t *this) {

  buf_element_t *buf, *next;
  int received = 0;

  this->clear( this );
  buf = this->buffer_pool_top;

  while (buf != NULL) {

    next = buf->next;

    free (buf->extra_info);
    free (buf);
    received++;

    buf = next;
  }
  
  while (received < this->buffer_pool_capacity) {
  
    buf = this->get(this);
    
    free(buf->extra_info);
    free(buf);
    received++;
  }

  free (this->buffer_pool_base);
  pthread_mutex_destroy(&this->mutex);
  pthread_cond_destroy(&this->not_empty);
  pthread_mutex_destroy(&this->buffer_pool_mutex);
  pthread_cond_destroy(&this->buffer_pool_cond_not_empty);
  free (this);
}

/*
 * allocate and initialize new (empty) fifo buffer
 */
fifo_buffer_t *fifo_buffer_new (int num_buffers, uint32_t buf_size) {

  fifo_buffer_t *this;
  int            i;
  int            alignment = 2048;
  char          *multi_buffer = NULL;

  this = xine_xmalloc (sizeof (fifo_buffer_t));

  this->first           = NULL;
  this->last            = NULL;
  this->fifo_size       = 0;
  this->put             = fifo_buffer_put;
  this->insert          = fifo_buffer_insert;
  this->get             = fifo_buffer_get;
  this->clear           = fifo_buffer_clear;
  this->size		= fifo_buffer_size;
  this->dispose		= fifo_buffer_dispose;

  pthread_mutex_init (&this->mutex, NULL);
  pthread_cond_init (&this->not_empty, NULL);

  /*
   * init buffer pool, allocate nNumBuffers of buf_size bytes each 
   */


  if (buf_size % alignment != 0)
    buf_size += alignment - (buf_size % alignment);

  /*
  printf ("Allocating %d buffers of %ld bytes in one chunk (alignment = %d)\n", 
	  num_buffers, (long int) buf_size, alignment);
	  */
  multi_buffer = xine_xmalloc_aligned (alignment, num_buffers * buf_size, 
				       &this->buffer_pool_base);

  this->buffer_pool_top = NULL;

  pthread_mutex_init (&this->buffer_pool_mutex, NULL);
  pthread_cond_init (&this->buffer_pool_cond_not_empty, NULL);

  for (i = 0; i<num_buffers; i++) {
    buf_element_t *buf;

    buf = xine_xmalloc (sizeof (buf_element_t));
    
    buf->mem = multi_buffer;
    multi_buffer += buf_size;

    buf->max_size    = buf_size;
    buf->free_buffer = buffer_pool_free;
    buf->source      = this;
    buf->extra_info  = malloc(sizeof(extra_info_t));
    
    buffer_pool_free (buf);
  }
  this->buffer_pool_num_free  = num_buffers;
  this->buffer_pool_capacity  = num_buffers;
  this->buffer_pool_buf_size  = buf_size;
  this->buffer_pool_alloc     = buffer_pool_alloc;
  this->buffer_pool_try_alloc = buffer_pool_try_alloc;

  return this;
}

