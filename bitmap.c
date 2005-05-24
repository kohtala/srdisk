/* SRDISK - Bitmap handling functions
 * Copyright (C) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "bitmap.h"
#include <string.h>

/* Application specific secured allocation functions */
extern void *xalloc(size_t);

bitmap bitmap_new(unsigned int bits)
{
    int bytes;
    bitmap r;

    bytes = bits/8;
    if (bits & 7) bytes++;

    r = (bitmap)xalloc(bytes);
    memset(r, 0, bytes);

    return r;
}

