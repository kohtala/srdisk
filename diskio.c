/* ReSizeable RAMDisk - disk I/O
 * Copyright (C) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "srdisk.h"
#include <assert.h>
#include <dos.h>
#include <string.h>
#include <stdio.h>
#include "max.h"

int mem_allocated = 0;  /* Global flag to tell memory allocation performed */

#define FIND_DEVICE_HDR(conf) ((struct dev_hdr _seg *)FP_SEG(conf))

/*
**  Read/Write sector from/to disk
**
**  Issue error if request could not be satisfied.
*/

static void xfer_sector(char rw, int count, dword start, void far *buffer)
{
  typedef word (far *batch_xfer_fnc)(char far *b,
                                     dword start,
                                     word count,
                                     char RW);

  batch_xfer_fnc batch_xfer =
    (batch_xfer_fnc)MK_FP(FP_SEG(conf), conf->batch_xfer);

  asm {
    push ds
    push si
    push di
    push cx
    push word ptr rw
    push count
    push word ptr start+2
    push word ptr start
    push word ptr buffer+2
    push word ptr buffer
    call batch_xfer
    add sp,12
    pop cx
    pop di
    pop si
    pop ds
    jc fail
    or ax,ax
    jnz fail
  }
  return;

 fail:
  error("Disk %s failure!", rw ? "write" : "read");
  return;
}

/*
**  Read sector from disk
**
**  Return 0 for failure, transferred sector count otherwise
*/

void read_sector(int count, dword start, void far *buffer)
{
  xfer_sector(0, count, start, buffer);
}

/*
**  Write sector to disk
**
**  Return 0 for failure, transferred sector count otherwise
*/

void write_sector(int count, dword start, void far *buffer)
{
  disk_touched = 1;     /* Tell the disk has been written to */
  xfer_sector(1, count, start, buffer);
}

/********************************************************************
********************************************************************/

enum memtype_e {
  MEM_UNKNOWN = -1,
  MEM_XMS,
  MEM_EMS
};

extern int   XMSf_has_memory(void far *);
extern dword XMSf_alloc(void far *, dword);
extern dword XMSf_realloc(void far *, dword, dword);
extern dword XMSf_free(void far *, dword);
extern void  XMSf_save_buffer(void far *, dword);
extern int   XMSf_copy_to_new(void far *, dword);
extern int   XMSf_free_old(void);
extern dword XMSf_restore_buffer(void far *);
extern dword XMSf_mem_avail(void far *);

extern int   EMSf_has_memory(void far *);
extern dword EMSf_alloc(void far *, dword);
extern dword EMSf_realloc(void far *, dword, dword);
extern dword EMSf_free(void far *, dword);
extern void  EMSf_save_buffer(void far *, dword);
extern int   EMSf_copy_to_new(void far *, dword);
extern int   EMSf_free_old(void);
extern dword EMSf_restore_buffer(void far *);
extern dword EMSf_mem_avail(void far *);

static struct phys_interface_s {
  int   (*has_memory)(void far *);
  dword (*alloc)(void far *, dword);
  dword (*realloc)(void far *, dword, dword);
  dword (*free)(void far *, dword);
  void  (*save_buffer)(void far *, dword);
  int   (*copy_to_new)(void far *, dword);
  int   (*free_old)(void);
  dword (*restore_buffer)(void far *);
  int expandable : 1;               /* True if always can expand */
  int can_expand : 1;               /* True if may or may not expand */
  dword (*mem_avail)(void far *);
} phys_interface[2] = {
  { XMSf_has_memory,
    XMSf_alloc,
    XMSf_realloc,
    XMSf_free,
    XMSf_save_buffer,
    XMSf_copy_to_new,
    XMSf_free_old,
    XMSf_restore_buffer,
    0, 1,
    XMSf_mem_avail
  },
  { EMSf_has_memory,
    EMSf_alloc,
    EMSf_realloc,
    EMSf_free,
    EMSf_save_buffer,
    EMSf_copy_to_new,
    EMSf_free_old,
    EMSf_restore_buffer,
    1, 1,
    EMSf_mem_avail
  }
};

static enum memtype_e identify_memory(struct dev_hdr _seg *dev)
{
  return _fstrncmp("XMS ", dev->u.s.memory, 4) == 0 ? MEM_XMS :
         _fstrncmp("EMS ", dev->u.s.memory, 4) == 0 ? MEM_EMS :
         _fstrncmp("EMS3", dev->u.s.memory, 4) == 0 ? MEM_EMS :
         MEM_UNKNOWN;
}

static dword physical_alloc(struct phys_interface_s *f, byte far *alloc,
  dword currsize, dword size)
{
  const WinEnh = 0; /* Do not take Windows into account here */
  dword res_size = currsize;

  /* Windows 3.1 Enhanced mode gives problems with allocation, so
     make a note that something likely to cause problems has happened */
  mem_allocated = 1;

  if (f->has_memory(alloc)) {
    /* Has already memory - reallocate to new size */
    if ((size && data_on_disk) || WinEnh) {
      /* If space wanted and old contents must be preserved */
      dword newsize = f->realloc(alloc, currsize, size);
      res_size = newsize;
      if (newsize < currsize && newsize < size) {
        error("Data lost when allocating");
      }
      if (newsize != size && f->mem_avail(alloc) >= size && !WinEnh) {
        /* Try to implement the realloc without the help from the MM */
        f->save_buffer(alloc, newsize);
        newsize = f->alloc(alloc, size);
        if (newsize < size) {
          res_size = f->restore_buffer(alloc);
        }
        else {
          if (f->copy_to_new(alloc, size)) {
            f->free_old();
            disk_touched = 1;     /* Tell the disk has been altered */
          }
          else
            res_size = f->restore_buffer(alloc);
        }
      }
      else {
        disk_touched = 1;     /* Tell the disk has been altered */
      }
    }
    else {
      /* Old contents are to be destroyed, so free the block */
      res_size = f->free(alloc, currsize);
      disk_touched = 1;     /* Tell the disk has been altered */
      goto alloc_handle;
    }
  }
  else {
    /* No handle - must allocate */
   alloc_handle:
    if (size && !WinEnh)
      res_size = f->alloc(alloc, size);
  }
  return res_size;
}

dword disk_alloc(struct config_s far *conf, dword size)
{
  struct dev_hdr _seg *dev = FIND_DEVICE_HDR(conf);
  byte far *alloc = (byte far *)MK_FP(dev, conf->malloc_off);

  if (!(conf->flags & C_NOALLOC)) {
    size = ((dword (far *)(dword))alloc)(size);
    disk_touched = 1;     /* Tell the disk has been altered */
  }
  else {
    enum memtype_e mt = identify_memory(dev);
    if (mt == MEM_UNKNOWN) {
      fatal("Don't know how to allocate memory");
    }
    size = physical_alloc(&phys_interface[mt], alloc, conf->size, size);
  }
  conf->size = size;
  return size;
}

/********************************************************************
********************************************************************/

dword max_size(struct config_s far *conf)
{
  if (conf->freemem_off) {
    dword (far *freemem)(void) =
      (dword (far*)())MK_FP(FP_SEG(conf), conf->freemem_off);
    dword size;
    asm {
      call freemem
      mov word ptr size,cx
      mov word ptr size+2,dx
    }
    return size;
  }
  else {
    struct dev_hdr _seg *dev = FIND_DEVICE_HDR(conf);
    byte far *alloc = (byte far *)MK_FP(dev, conf->malloc_off);
    dword size;
    enum memtype_e mt = identify_memory(dev);

    if (mt == MEM_UNKNOWN) {
      fatal("Don't know how to handle memory");
    }
    else {
      size = phys_interface[mt].mem_avail(alloc);
      return phys_interface[mt].can_expand ? size + conf->size : size;
    }
  }
  return 0;
}

dword safe_size(struct config_s far *conf)
{
  if (conf->freemem_off) {
    dword (far *freemem)(void) =
      (dword (far*)())MK_FP(FP_SEG(conf), conf->freemem_off);
    return freemem();
  }
  else {
    struct dev_hdr _seg *dev = FIND_DEVICE_HDR(conf);
    byte far *alloc = (byte far *)MK_FP(dev, conf->malloc_off);
    dword size;
    enum memtype_e mt = identify_memory(dev);

    if (mt == MEM_UNKNOWN) {
      fatal("Don't know how to handle memory");
    }
    else {
      size = phys_interface[mt].mem_avail(alloc);
      return phys_interface[mt].expandable ?
        size + conf->size : max(size, conf->size);
    }
  }
  return 0;
}

