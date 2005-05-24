/* SRDISK - Bitmap handling functions header
 * Copyright (C) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifndef _BITMAP_H
#define _BITMAP_H

#include <malloc.h>

typedef char *bitmap;

bitmap bitmap_new(unsigned int);
#define bitmap_delete(map) free(map)

#define bitmap_test(map,bit) ((map)[(bit)/8] & (1 << ((bit) % 8)))
#define bitmap_set(map,bit) ((map)[(bit)/8] |= (1 << ((bit) % 8)))
#define bitmap_reset(map,bit) ((map)[(bit)/8] &= ~(1 << ((bit) % 8)))

#endif
