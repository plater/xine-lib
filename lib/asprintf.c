/*
 * Copyright (C) 2006 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version.
 *
 * xine is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 *
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

int _xine_private_vasprintf (char **buffer, const char *format, va_list ap)
{
  char *buf = NULL;
  int size = 128;
  
  for (;;)
  {
    int ret;
    va_list cp;
    char *newbuf = realloc (buf, size *= 2);
    if (!newbuf)
    {
      free (buf);
      return -1;
    }
    buf = newbuf;
    
    va_copy (cp, ap);
    ret = vsnprintf (buf, size, format, cp);
    va_end (cp);
    
    if (ret >= 0 && ret < size)
    {
      *buffer = realloc (buf, ret + 1);
      return ret;
    }
  }
}

int _xine_private_asprintf (char **buffer, const char *format, ...)
{
  int ret;
  va_list ap;
  va_start (ap, format);
  ret = _xine_private_vasprintf (buffer, format, ap);
  va_end (ap);
  return ret;
}