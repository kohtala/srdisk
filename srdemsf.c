/* ReSizeable RAMDisk - EMS disk I/O
 * Copyright (C) 1992-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "srdisk.h"
#include <dos.h>
#include <stdio.h>
#include <assert.h>
#include "max.h"

/*
** EMS errors
*/

static void EMS_error(byte err)
{
  char *errstr = "Unknown error";
  int e;
  static struct {
    byte err;
    char *str;
  } errs[] = {
    { 0x80, "Driver software failure" },
    { 0x81, "Hardware failure" },
    { 0x83, "Unknown handle" },
    { 0x84, "Unknown function" },
    { 0x85, "No handles available" },
    { 0x87, "Insufficient total pages" },
    { 0x88, "Insufficient available pages" },
    { 0x89, "Zero pages requested for LIM 3.2 functions" }
  };

  for (e = 0; e < sizeof errs / sizeof errs[0]; e++)
    if (errs[e].err == err) {
      errstr = errs[e].str;
      break;
    }

  force_banner();
  printf("EMS error %02X: %s\n", err, errstr);
}

/*
**  Memory allocation support
*/

/* Local utilities */

#define GET_HANDLE(allocs) (*(word far *)((byte far *)allocs + EMS_handle))

static void far *old_allocs;
static word old_handle;
static dword old_size;

#pragma argsused
static int EMSf_free_handle(void far *allocs, word handle)
{
  byte err;
  asm {
    mov dx,handle
    mov ah,0x45  /* Free extended memory block */
    int 0x67
    or ah,ah
    jz ok
  }
  err = _AH;
  EMS_error(err);	/* EMS_error(_AH) gets miscompiled by bcc 3.1 */
  if (err != 0x83)        /* If unknown handle, it is freed... */
    return 0;
 ok:
  return 1;
}

/* External interface */

int EMSf_has_memory(void far *alloc)
{
  return GET_HANDLE(alloc) != (word)-1;
}

dword EMSf_alloc(void far *allocs, dword size)
{
  word pages;
  byte err;

  if (size > 0xFFFF0L) /* Can not allocate over 0xFFFFF K */
    size = 0xFFFF0L;

  pages = (size >> 4) + ((size & 0xF) ? 1 : 0);

  asm {
    mov bx,pages
    mov ah,0x43      /* Allocate */
    int 0x67
    or ah,ah
    jnz alloc_fail
    les si,allocs
    mov es:[si+EMS_handle],dx
    jmp alloc_ok
  }
 alloc_fail:
  err = _AH;
  EMS_error(err);
//  asm { xchg al,ah; push ax; call EMS_error; pop ax; }
  pages = 0;
 alloc_ok:;
  return pages << 4;
}

dword EMSf_realloc(void far *allocs, dword currsize, dword size)
{
  word pages;
  byte err;

  if (size > 0xFFFF0L) /* Can not allocate over 0xFFFFF K */
    size = 0xFFFF0L;

  pages = (size >> 4) + ((size & 0xF) ? 1 : 0);

  asm {
    les si,allocs
    mov dx,es:[si+EMS_handle]
    mov bx,pages
    mov ah,0x51      /* Reallocate */
    int 0x67
    mov pages,bx
    or ah,ah
    jz ok
    cmp bl,0x84         /* Not implemented? */
    je not_implemented  /* Do not report the error */
  }
 error:
  err = _AH;
  EMS_error(err);
//  asm { xchg al,ah; push ax; call EMS_error; pop ax; }
 ok:
  return pages << 4;
 not_implemented:
  return currsize;
}

dword EMSf_free(void far *allocs, dword currsize)
{
  if (EMSf_free_handle(allocs, GET_HANDLE(allocs))) {
    GET_HANDLE(allocs) = (word)-1;
    return 0;
  }
  return currsize;
}

/******************* realloc implementation functions ****************/

void EMSf_save_buffer(void far *alloc, dword size)
{
  old_allocs = alloc;
  old_handle = GET_HANDLE(alloc);
  GET_HANDLE(alloc) = (word)-1;
  old_size = size;
}

int EMSf_copy_to_new(void far *alloc, dword size)
{
  struct EMS_move_s {
    dword length;
    byte stype;
    word shandle;
    word soff;
    word spage;
    byte dtype;
    word dhandle;
    word doff;
    word dpage;
  } mb, far *pmb = &mb;
  byte err;

  assert(alloc == old_allocs);

  mb.length = min(old_size, size) * 1024;
  mb.stype = 1;
  mb.shandle = old_handle;
  mb.soff = 0;
  mb.spage = 0;
  mb.dtype = 1;
  mb.dhandle = GET_HANDLE(alloc);
  mb.doff = 0;
  mb.dpage = 0;
  asm {
    mov ax,0x5700
    push ds
    lds si,pmb
    int 0x67
    pop ds
    or ah,ah
    jz move_ok
  }
  err = _AH;
  EMS_error(err);
//  asm { xchg al,ah; push ax; call EMS_error; pop ax; }
  return 0;
 move_ok:
  return 1;
}

int EMSf_free_old(void)
{
  return EMSf_free_handle(old_allocs, old_handle);
}

dword EMSf_restore_buffer(void far *alloc)
{
  if (GET_HANDLE(alloc) != (word)-1)
    EMSf_free(alloc, 1);
  GET_HANDLE(old_allocs) = old_handle;
  return old_size;
}

/* Memory size determination */

#pragma argsused
dword EMSf_mem_avail(void far *alloc)
{
  dword size;
  asm {
    mov ah,0x42
    int 0x67
    mov ax,16
    mul bx
    mov word ptr size,ax
    mov word ptr size+2,dx
  }
  return size;
}

