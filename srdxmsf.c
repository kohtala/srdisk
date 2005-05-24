/* ReSizeable RAMDisk - XMS disk I/O
 * Copyright (C) 1992-1994, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

/* It might be important to call XMS functions through the pointer in
   the device driver. DesqView can now from this that it is the device
   driver that wants the memory and does not deallocate it. At the end
   of the session where SRDISK.EXE was run. */

/* We use some 32-bit instructions so must compile via TASM */
#pragma inline
#pragma warn -asm

#include "srdisk.h"
#include <dos.h>
#include <stdio.h>
#include <dos.h>
#include <assert.h>
#include "max.h"

#ifndef NDEBUG

/* This selects alternative method to allocate memory
#define VIA_DOS
*/

/* This can be used to force usage of XMS 3.0 functions for testing purpose */
#define USEXMS30 0

/* This can be used to test huge XMS disks -> allocates only 2M */
#define TESTHUGEDISK 0

/* This enables detailed output of what we are doing
*/
#define VERBOSE

#else /* NDEBUG */

#define TESTHUGEDISK 0
#define USEXMS30 0

#endif


/*
** XMS errors
*/

static void XMS_error(byte err)
{
  char *errstr = "Unknown error";
  int e;
  static struct {
    byte err;
    char *str;
  } errs[] = {
    { 0x80, "Function not implemented" },
    { 0x81, "VDISK device is detected" },
    { 0x82, "A20 error occurs" },
    { 0x8E, "General device driver error" },
    { 0x8F, "Unknown device driver error" },
    { 0xA0, "All extended memory is allocated" },
    { 0xA1, "All available handles are in use" },
    { 0xA2, "Handle is invalid" },
    { 0xA3, "Source handle is invalid" },
    { 0xA9, "Parity error" },
    { 0xAB, "Block is locked" }
  };

  for (e = 0; e < sizeof errs / sizeof errs[0]; e++)
    if (errs[e].err == err) {
      errstr = errs[e].str;
      break;
    }

  force_banner();
  printf("XMS error %02X: %s\n", err, errstr);
}

/*
**  Memory allocation support
*/

#define GET_HANDLE(allocs) (*(word far *)((byte far *)allocs + XMS_handle))

static int hasSuperXMS(void far *allocs)
{
  static enum { unknown, yes, no } status;

  if (status == unknown) {
    status = no;
    asm {
      mov ah,0      /* GET VERSION */
      les si,allocs
      call dword ptr es:[si+XMS_entry]
      cmp ax,0x300
      jb noSuperXMS

/* From CPUID by Intel. Some HIMEM.SYS v3.0 functionality crash with 286.
;
;       This procedure determines the type of CPU in a system
;       and sets the cpu_type variable with the appropriate
;       value.
;       All registers are used by this procedure, none are preserved.

;       Intel 8086 CPU check
;       Bits 12-15 of the FLAGS register are always set on the
;       8086 processor.
*/
        pushf
        pushf                   /* push original FLAGS */
        pop     ax              /* get original FLAGS */
        mov     cx, ax          /* save original FLAGS */
        and     ax, 0xfff       /* clear bits 12-15 in FLAGS */
        push    ax              /* save new FLAGS value on stack */
        popf                    /* replace current FLAGS value */
        pushf                   /* get new FLAGS */
        pop     ax              /* store new FLAGS in AX */
        popf
        and     ax, 0xf000      /* if bits 12-15 are set, then CPU */
        cmp     ax, 0xf000      /*   is an 8086/8088 */
        je      noSuperXMS      /* jump if CPU is 8086/8088 */

/*       Intel 286 CPU check */
/*       Bits 12-15 of the FLAGS register are always clear on the */
/*       Intel 286 processor in real-address mode. */

        pushf
        or      cx, 0xf000      /* try to set bits 12-15 */
        push    cx              /* save new FLAGS value on stack */
        popf                    /* replace current FLAGS value */
        pushf                   /* get new FLAGS */
        pop     ax              /* store new FLAGS in AX */
        popf
        and     ax, 0xf000      /* if bits 12-15 clear, CPU=80286 */
        jz      noSuperXMS      /* if no bits set, CPU is 80286 */

      /* Query free extended to see if SuperXMS really supported */
      mov ah,0x88   /* QUERY FREE (32-bit) */
      call dword ptr es:[si+XMS_entry]
      cmp bl,0x80  /* Not Implemented - not a 32-bit system */
      je noSuperXMS
    }
    status = yes;
    noSuperXMS: ;
  }
#ifdef VERBOSE
  printf("XMS: %s XMS support\n", status == yes ? "Super" : "Normal");
#endif
  return status == yes ? 1 : 0;
}

static dword handleSize(void far* allocs)
{
  dword size = 0;
  if (hasSuperXMS(allocs))
  {
    asm {
      les si,allocs
      mov dx,es:[si+XMS_handle]
      mov ah,0xE      /* Get handle information */
      call dword ptr es:[si+XMS_entry]
      or ax,ax
      jz fail
      mov word ptr size,dx
    }
  }
  else
  {
    asm {
      P386N
      les si,allocs
      mov dx,es:[si+XMS_handle]
      mov ah,0x8E     /* Get 32bit handle information */
      call dword ptr es:[si+XMS_entry]
      or ax,ax
      jz fail
      mov size,edx
      P8086
    }
  }
  return size;
 fail:
  return -1;
}

#ifdef VIA_DOS
/* IOCTL_write_allocator is called from the device driver to allocate
** memory. This gimmic is necessary to fool multitaskers (especially 
** Windows) to not free the allocated memory.
*/

byte far *IOCTL_wa_allocs;
void (far *IOCTL_wa_manager)(void);
word IOCTL_wa_size;
word IOCTL_wa_handle;
byte IOCTL_wa_error;

#pragma option -N-

static void far _loadds IOCTL_write_allocator(void)
{
  IOCTL_wa_error = 1;
  _DX = IOCTL_wa_size;
  _AH = 9;              /* Allocate */
  IOCTL_wa_manager();
  if (_AX) {
    IOCTL_wa_handle = _DX;
    GET_HANDLE(IOCTL_wa_allocs) = IOCTL_wa_handle;
  }
  else
    IOCTL_wa_error = _BL;
}

static void far _loadds IOCTL_write_free(void)
{
  IOCTL_wa_error = 1;
  _DX = IOCTL_wa_handle;
  _AH = 10;             /* Free */
  IOCTL_wa_manager();
  if (!_AX)
    IOCTL_wa_error = _BL;
}

static void far _loadds IOCTL_write_realloc(void)
{
  IOCTL_wa_error = 1;
  _DX = IOCTL_wa_handle;
  _BX = IOCTL_wa_size;
  _AH = 15;             /* Realloc */
  IOCTL_wa_manager();
  if (!_AX)
    IOCTL_wa_error = _BL;
}

int call_via_DOS(void (far *func)(void), byte far *allocs, dword size)
{
  union REGS regs;
  struct SREGS sregs;

  IOCTL_wa_allocs = allocs;
  IOCTL_wa_manager = (void (far *)(void))*(dword far *)(allocs + XMS_entry);
  IOCTL_wa_size = size;
  IOCTL_wa_handle = GET_HANDLE(allocs);
  IOCTL_wa_error = 0;

  /* Must do the IOCTL call this way instead of using ioctl() since
     ioctl() takes only a near pointer to DS segment */

  regs.h.bl = drive - 'A' + 1;
  regs.x.cx = IOCTL_ID;         /* Data length is the ID */
  sregs.ds = FP_SEG(func);
  regs.x.dx = FP_OFF(func);
  regs.x.ax = 0x4405;
  int86x(0x21, &regs, &regs, &sregs);

  if (IOCTL_wa_error == 0)
    fatal("Device driver is not valid SRDISK driver");
  if (IOCTL_wa_error != 1) {
    XMS_error(IOCTL_wa_error);
    return 0;
  }
  return 1;
}
#endif  /* VIA_DOS */

/* Local utilities */

static void far *old_allocs;
static word old_handle;
static dword old_size;

static int XMSf_free_handle(void far *allocs, word handle)
{
  byte err;
#ifdef VERBOSE
  printf("XMS: Free handle %04Xh\n", handle);
#endif
  if (handle) {
    asm {
      mov dx,handle
      mov ah,0xA  /* Free extended memory block */
      les si,allocs
      call dword ptr es:[si+XMS_entry]
      or ax,ax
      jnz ok
    }
  }
  XMS_error(err = _BL);
  if (err != 0xa2)       /* If handle is invalid, it is freed... */
    return 0;
 ok:
  return 1;
}

/* External interface */

int XMSf_has_memory(void far *alloc)
{
  return GET_HANDLE(alloc) != 0;
}

dword XMSf_alloc(void far *allocs, dword size)
{
#if !TESTHUGEDISK
  if (USEXMS30 || size >= 0x10000L) {
    if (hasSuperXMS(allocs)) {
#ifdef VERBOSE
      printf("XMS: Allocate %ld K to handle %04Xh\n", size, GET_HANDLE(allocs));
#endif
      asm {
        P386N
        mov ah,0x89   /* Allocate 32-bit */
        mov edx,size
        les si,allocs
        call dword ptr es:[si+XMS_entry]
        P8086
        cmp ax,0
        je fail32bit
        mov es:[si+XMS_handle],dx
        jmp alloc_ok
      }
     fail32bit:
      XMS_error(_BL);
    }
    if (!USEXMS30 || size >= 0x10000L)
      size = 0xFFFFL;
  }
#else /* TESTHUGEDISK */
  dword fakesize = size;
  if (size > 2000) size = 2000;
  printf("XMS DEBUG: Alloc size %ld\n", fakesize);
#endif

#ifdef VERBOSE
  printf("XMS: Allocating %ld K to handle %04Xh\n", size, GET_HANDLE(allocs));
#endif

#ifndef VIA_DOS
  asm {
    mov dx,word ptr size
    mov ah,0x9      /* Allocate */
    les si,allocs
    call dword ptr es:[si+XMS_entry]
    or ax,ax
    jz alloc_fail
    mov es:[si+XMS_handle],dx
    jmp alloc_ok
  }
 alloc_fail:
  XMS_error(_BL);
  size = handleSize(allocs);
  if (size == -1) size = 0;
 alloc_ok:;
#else
  if (!call_via_DOS(IOCTL_write_allocator, allocs, size))
    size = 0;
#endif
#ifdef VERBOSE
  printf("XMS: Allocated %ld K\n", size);
#endif
#if TESTHUGEDISK
  return fakesize;
#else
  return size;
#endif
}

dword XMSf_realloc(void far *allocs, dword currsize, dword size)
{
  int err;

#if !TESTHUGEDISK
  if (USEXMS30 || size >= 0x10000L) {
    if (hasSuperXMS(allocs)) {
#ifdef VERBOSE
      printf("XMS: ReAllocating %ld K to handle %04Xh\n", size, GET_HANDLE(allocs));
#endif
      asm {
        P386N
        mov ah,0x8F   /* Reallocate 32-bit */
        mov ebx,size
        les si,allocs
        mov dx,es:[si+XMS_handle]
        call dword ptr es:[si+XMS_entry]
        P8086
        or ax,ax
        je error
      }
      goto ok;
    }
    if (!USEXMS30 || size >= 0x10000L)
      size = 0xFFFFL;
  }
#else /* TESTHUGEDISK */
  dword fakesize = size;
  if (size > 2000) size = 2000;
#endif

#ifdef VERBOSE
  printf("XMS: ReAllocating %ld K to handle %04Xh\n", size, GET_HANDLE(allocs));
#endif

#ifndef VIA_DOS
  asm {
    les si,allocs
    mov dx,es:[si+XMS_handle]
    mov bx,word ptr size
    mov ah,0xF      /* Reallocate */
    call dword ptr es:[si+XMS_entry]
    or ax,ax
    jnz ok
    cmp bl,0x80     /* Not implemented? */
    je fail         /* Do not report the error */
  }
 error:
  XMS_error(err = _BL);
  if (err == 0xa2)       /* If handle is invalid, it is freed... */
    return 0;
 fail:
  {
    dword s = handleSize(allocs);
    if (s == -1) return currsize;
    else return s;
  }
 ok:
#else
  if (!call_via_DOS(IOCTL_write_realloc, allocs, size)) {
    if (IOCTL_wa_error == 0xa2)   /* If handle is invalid, it is freed... */
      size = 0;
    else
      size = currsize;
  }
#endif
#ifdef VERBOSE
  printf("XMS: ReAllocated %ld K\n", size);
#endif
#if TESTHUGEDISK
  return fakesize;
#else
  return size;
#endif
}

dword XMSf_free(void far *allocs, dword currsize)
{
#ifdef VERBOSE
  printf("XMS: Freeing handle %04Xh, currsize %ld K\n",
         GET_HANDLE(allocs),
         currsize);
#endif
#ifndef VIA_DOS
  if (XMSf_free_handle(allocs, GET_HANDLE(allocs))) {
    GET_HANDLE(allocs) = 0;
    return 0;
  }
  return currsize;
#else
  if (!call_via_DOS(IOCTL_write_free, allocs, 0)) {
    if (IOCTL_wa_error != 0xa2) /* Invalid handle sure is freed... */
      return currsize;
  }
  return 0;
#endif
}

/******************* realloc implementation functions ****************/

void XMSf_save_buffer(void far *alloc, dword size)
{
  old_allocs = alloc;
  old_handle = GET_HANDLE(alloc);
  GET_HANDLE(alloc) = 0;
  old_size = size;
#ifdef VERBOSE
  printf("XMS: Saved handle %04Xh\n", GET_HANDLE(alloc));
#endif
}

int XMSf_copy_to_new(void far *alloc, dword size)
{
  struct XMS_move_s {
    dword length;
    word shandle;
    dword soff;
    word dhandle;
    dword doff;
  } mb, far *pmb = &mb;

  assert(alloc == old_allocs);

#ifdef VERBOSE
  printf("XMS: Copying %04Xh to new %04Xh\n", old_handle, GET_HANDLE(alloc));
#endif

  mb.length = min(old_size, size) * 1024;
  mb.shandle = old_handle;
  mb.soff = 0;
  mb.dhandle = GET_HANDLE(alloc);
  mb.doff = 0;
  asm {
    mov ah,0xB
    push ds
    lds si,pmb
    les di,alloc
    call dword ptr es:[di+XMS_entry]
    pop ds
    or ax,ax
    jnz move_ok
  }
  XMS_error(_BL);
  return 0;
 move_ok:
#ifdef VERBOSE
  printf("XMS: Moved successfully\n", size);
#endif
  return 1;
}

int XMSf_free_old(void)
{
  return XMSf_free_handle(old_allocs, old_handle);
}

dword XMSf_restore_buffer(void far *alloc)
{
  if (GET_HANDLE(alloc))
    XMSf_free(alloc, 1);
  GET_HANDLE(alloc) = old_handle;
  return old_size;
}

/* Memory size determination */

dword XMSf_mem_avail(void far *alloc)
{
#if TESTHUGEDISK
#ifdef VERBOSE
  printf("XMS: Available 0x7FFFFF K\n");
#endif
  return 0x7FFFFFUL;
#else
  dword size;
  if (hasSuperXMS(alloc)) {
    asm {
      P386N
      mov ah,0x88
      les si,alloc;
      call dword ptr es:[si+XMS_entry]
      cmp bl,0
      je ok32bit
        xor eax,eax
      P8086
    }
   ok32bit:
    asm {
      P386N
      mov size,eax
      P8086
    }
  }
  else {
    asm {
      mov ah,8
      les si,alloc;
      call dword ptr es:[si+XMS_entry]
      mov word ptr size,ax
      mov word ptr size+2,0
    }
  }
#ifdef VERBOSE
  printf("XMS: %ld K in largest available block\n", size);
#endif
  return size;
#endif
}

