; SRDISK - XMS memory device driver specific code
; Copyright (C) 1992-1996, 2005 Marko Kohtala
; Released under GNU GPL, read the file 'COPYING' for more information

MEMSTR  equ 'XMS '      ; Define 4 char memory type string
ALLOCBLOCK equ 1        ; Memory allocation block size

ife CAPABLE and C_32BITSEC
MAX_SIZE equ 07FFFh     ; Without 32-bit sector addressing only 32M max
elseife CAPABLE and C_NOALLOC
MAX_SIZE equ 0FFFFh     ; With own allocation only 64M max
else
MAX_SIZE equ 524288     ; Maximum possible size (using 32-bit access)
endif

;**************************************************************************
;
;               I/O ROUTINE TO THE RAM DISK
;
; This routine will read a group of sectors inside this part of
; the ram disk. If starting sector is not on this part of the disk,
; return without error with 0 sectors transferred. If not all sectors
; are on this part of the disk, transfer as many as can and report the
; number of sectors transferred.
;
; On entry
;   DS:SI - local configuration structure
;   dIO_bytes      word ptr [bp-6]   - bytes to move
;   dIO_byteoffset word ptr [bp-4]   - byte offset of start
;   dIO_buffer     dword ptr [bp+6]  - buffer
;   dIO_sector     dword ptr [bp+10] - sector
;   dIO_count      word ptr [bp+14]  - count
;   dIO_RW         byte ptr [bp+16]  - RW (0 = read, nonzero write)
;
; Preserve
;   ds, si, bp and stack contents
;
; Return
;   - carry clear if success
;   - carry set on error
;
;**************************************************************************

mac_disk_IO     macro
                local write, move
                push si
                mov si,offset XMS_alloc.XMS_cblk
                mov cx,dIO_bytes
                mov [si].XMS_sizel,cx
                xor cx,cx                       ; cx is zero here
                mov [si].XMS_sizeh,cx

                les di,dIO_buffer
                mov ax,word ptr dIO_byteoffset
                mov dx,word ptr dIO_byteoffset+2
                mov bx,XMS_alloc.XMS_handle

                test dIO_RW,-1                  ; Input/output?
                jnz write
                                                ; -- Read
                mov [si].XMS_shandle,bx         ; Source in XMS
                mov [si].XMS_soffl,ax
                mov [si].XMS_soffh,dx
                mov [si].XMS_dhandle,cx         ; Destination in main memory
                mov [si].XMS_doffl,di
                mov [si].XMS_doffh,es
                jmp move
write:                                          ; -- Write
                mov [si].XMS_shandle,cx         ; Destination in main memory
                mov [si].XMS_soffl,di
                mov [si].XMS_soffh,es
                mov [si].XMS_dhandle,bx         ; Source in XMS
                mov [si].XMS_doffl,ax
                mov [si].XMS_doffh,dx
move:
                mov ah,0Bh                      ; Move XMS block
                call XMS_alloc.XMS_entry        ; ds:si -> control block
                pop si
                debug 'X',ax
                shr ax,1
                cmc                             ; Carry set if err
                ret
                endm


;**************************************************************************
;
;               QUERY FREE MEMORY
;
; This routine is only used if CAPABLE has C_NOALLOC bit clear.
;
; On entry
;
; Preserve
;   ds, si, di, bp and stack contents
;
; Return
;   - dx:ax set to maximum safe size to reallocate to
;   - cx:bx set to maximum size to reallocate to (may not be valid)
;
;**************************************************************************

mac_freemem     macro
                assume ds:nothing
                mov ah,8
                call XMS_alloc.XMS_entry
                xor dx,dx
                mov bx,ax
                mov cx,dx
                add bx,word ptr conf.size
                adc cx,word ptr conf.size+2
                ret
                assume ds:d_seg
                endm

;**************************************************************************
;
;               EXTERNAL MEMORY ALLOCATION ROUTINE
;
; This routine is only used if CAPABLE has C_NOALLOC bit clear.
;
; Allocate requested amount of memory. If memory is available in chunks,
; the amount can be rounded up. If not enough memory available, allocate
; as much as possible or just report the amount that was previously
; allocated. It is expected that at least as much memory can be allocated
; as there previously was. Reallocation should not destroy memory
; contents - it is essential to be able to resize a disk without loosing
; the contents (a feature under development).
;
; On entry
;   DWORD [sp+4] - Kbytes to allocate
;
; Preserve
;   es, ds
;   si, di
;
; Return dx:ax = Kbytes allocated
;
;**************************************************************************

mac_malloc      macro
                arg kbytes:dword
                local fail, ok, alloc
                assume ds:nothing
                test word ptr kbytes+2,-1       ; Over 0FFFFh K is impossible
                jnz fail
                mov bx,word ptr kbytes          ; New disk size
                mov dx,XMS_alloc.XMS_handle     ; Handle to the memory
                mov ah,0Fh                      ; Reallocate
                or dx,dx
                jnz alloc                       ; If no handle, then
                  mov dx,bx                     ; allocate
                  mov ah,9
alloc:
                call XMS_alloc.XMS_entry
                or ax,ax
                jnz ok

fail:           mov ax,word ptr conf.c_size     ; Fail, return current
                xor dx,dx
                ret

ok:             mov ax,word ptr kbytes          ; Ok, return requested
                xor dx,dx
                ret
                assume ds:d_seg
                endm

;**************************************************************************
;
;                       Warm Boot of Machine
;
; Release used XMS memory on warm boot.
;
; I guess this may be important if some virtual machine (VM) in some
; multitasking system has installed this driver and the VM is ended.
; Without this the other VMs would loose the space reserved for RAM disk
; in this VM.
;**************************************************************************
  
if HOOKINT19

mac_int_19      macro
                local int19_1
                assume ds:nothing
                pusha   ; If XMS used, it must be 286 or above
                mov     dx,XMS_alloc.XMS_handle
                or      dx,dx
                jz      int19_1                 ; Jump if no XMS handle
		mov	ah,0Ah
                call    XMS_alloc.XMS_entry     ; Free XMS memory
                mov     XMS_alloc.XMS_handle,0
int19_1:
                xor     ax,ax
		mov	ds,ax
                mov     ax,old_int19_off
		cli				; Disable interrupts
                mov     ds:[19h*4],ax           ; for the time to write
                mov     ax,old_int19_seg        ; old interrupt vector back
                mov     ds:[19h*4+2],ax
                popa                            ; Enable interrupts
                jmp     old_int19
                assume ds:d_seg
                endm
endif

;**************************************************************************
;
;                       Local data
;
; This data is used by the two above routines that are needed
; resident in any case.
;
;**************************************************************************

XMS_block       struc
XMS_sizel       dw ?
XMS_sizeh       dw ?
XMS_shandle     dw ?
XMS_soffl       dw ?
XMS_soffh       dw ?
XMS_dhandle     dw ?
XMS_doffl       dw ?
XMS_doffh       dw ?
XMS_block       ends

XMS_alloc_s struc       ; Changing this structure needs changes in srdisk.exe
XMS_handle      dw      0       ; XMS handle to disk memory (0=no handle)
XMS_entry       dd      ?       ; XMS driver entry point
XMS_cblk        XMS_block <>    ; XMS move command data structure
XMS_alloc_s ends

mac_resident_data macro
XMS_alloc XMS_alloc_s <>

if CAPABLE and C_NOALLOC
malloc EQU offset XMS_alloc
endif
                endm


;**************************************************************************
;
;                       Memory initialization
;
; Returns
;   carry set if error
;**************************************************************************
  
; Get XMS driver API address

mac_init_memory macro
                local init1, init2, init3, init4, init_ret
                push es
                mov ax,4300h
                int 2Fh                         ; Get XMS installed status
                cmp al,80h
                jne init1                       ; Jump if not installed
                mov ax,4310h
                int 2Fh                         ; Get XMS entry point
                jnc init2                       ; Jump if no error
init1:
                  mov dx,offset errs_noXMS      ; "No extended mem driver"
                  jmp init4
init2:
                mov word ptr XMS_alloc.XMS_entry,bx
                mov word ptr XMS_alloc.XMS_entry+2,es

                mov ah,0                        ; Verify XMS version
                call XMS_alloc.XMS_entry
                cmp ax,200h
                jae init3
                  mov dx,offset errs_badXMS
                  jmp init4
init3:
                mov     XMS_alloc.XMS_handle,0  ; Zero handle for no handle

                clc
                jmp     init_ret
init4:
                stc
init_ret:
                pop     es
                endm

;**************************************************************************
;
;                       Memory deinitialization
;
;**************************************************************************

mac_deinit_memory macro
                local done
                cmp XMS_alloc.XMS_handle,0
                je done

                mov dx,XMS_alloc.XMS_handle
                mov ah,0Ah
                call XMS_alloc.XMS_entry        ; Free XMS memory
                mov XMS_alloc.XMS_handle,0
done:
                endm

;**************************************************************************
;
;                       Initialization time data
;
;**************************************************************************

mac_init_data   macro
errs_noXMS      db 'RAMDisk: Extended Memory Manager not present.'
                db 0Dh, 0Ah, '$'

errs_badXMS     db 'RAMDisk: Invalid XMS version. XMS 2.0 or above needed.'
                db 0Dh, 0Ah, '$'
                endm

