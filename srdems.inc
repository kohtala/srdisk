; SRDISK - EMS memory device driver specific code
; Copyright (C) 1992-1996, 2005 Marko Kohtala
; Released under GNU GPL, read the file 'COPYING' for more information

; EMS 4.0

MEMSTR  equ 'EMS '      ; Define 4 char memory type string
MAX_SIZE equ 7FFFh      ; Maximum possible size
ALLOCBLOCK equ 16       ; Memory allocation block size

include EMS.INC

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
;   dIO_byteoffset dword ptr [bp-4]  - byte offset of start
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
                mov si,offset EMS_alloc.EMS_cblk
                mov cx,dIO_bytes
                mov [si].EMS_sizel,cx
                xor cx,cx                       ; cx is zero here
                mov [si].EMS_sizeh,cx

                les di,dIO_buffer
                mov ax,word ptr dIO_byteoffset
                mov dx,word ptr dIO_byteoffset+2
                mov bx,16*1024
                div bx                          ; ax=page, dx=offset
                mov bx,EMS_alloc.EMS_handle

                test dIO_RW,-1                  ; Input/output?
                jnz write
                                                ; -- Read
                mov [si].EMS_stype,1            ; Source in EMS
                mov [si].EMS_shandle,bx
                mov [si].EMS_soff,dx
                mov [si].EMS_spage,ax
                mov [si].EMS_dtype,cl           ; Destination in main memory
                mov [si].EMS_dhandle,cx
                mov [si].EMS_doff,di
                mov [si].EMS_dpage,es
                jmp move
write:                                          ; -- Write
                mov [si].EMS_stype,cl           ; Destination in main memory
                mov [si].EMS_shandle,cx
                mov [si].EMS_soff,di
                mov [si].EMS_spage,es
                mov [si].EMS_dtype,1            ; Source in EMS
                mov [si].EMS_dhandle,bx
                mov [si].EMS_doff,dx
                mov [si].EMS_dpage,ax
move:
                mov ax,EMS_MOVE_MEM_REGION * 100h ; Move EMS region
                int 67h                         ; ds:si -> control block
                pop si
                debug 'X',ax
                cmp ah,1                        ; Carry set if err
                cmc
                ret
                endm


;**************************************************************************
;
;               QUERY FREE MEMORY
;
; This routine is only used if CAPABLE has C_NOALLOC bit clear.
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
                mov ah,EMS_FREE_PAGE_COUNT
                int 67h                 ; bx = unallocated pages
                mov ax,16               ; of 16K each
                mul bx                  ; dx:ax = total free K
                add ax,word ptr conf.size
                adc dx,word ptr conf.size+2
                mov bx,ax
                mov cx,dx
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
; the contents.
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
                local no_roundup, realloc, ok, fail
                assume ds:nothing

                mov ax,word ptr kbytes          ; New disk size
                mov dx,word ptr kbytes+2
                cmp dx,15*1024
                jae fail

                mov cx,16*1024
                div cx
                or dx,dx
                jz no_roundup
                  inc ax
no_roundup:     mov bx,ax
                mov dx,EMS_alloc.EMS_handle     ; Handle to the memory
                or ax,ax
                jz free
                cmp dx,-1                       ; Is the handle valid?
                jne realloc
                  mov ah,EMS_ALLOCATE
                  int 67h
                  or ah,ah
                  jnz fail
                  mov EMS_alloc.EMS_handle,dx
                  jmp ok
free:
                mov ah,EMS_DEALLOCATE
                int 67h
                or ah,ah
                jnz fail
                mov EMS_alloc.EMS_handle,dx
                jmp ok
realloc:
                mov ah,EMS_REALLOCATE_PAGES     ; Reallocate
                int 67h
                or ah,ah
                jnz fail

ok:             mov ax,word ptr kbytes          ; Ok, return requested
                xor dx,dx
                ret

fail:           mov ax,word ptr conf.c_size     ; Fail, return current
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
; Without this the other VMs could loose the space reserved for RAM disk
; in this VM.
;**************************************************************************
  
if HOOKINT19

mac_int_19      macro
                local int19_1
                assume ds:nothing
                push ax
                push dx
                push ds
                pushf
                mov dx,EMS_alloc.XMS_handle
                cmp dx,-1
                jne int19_1                     ; Jump if no EMS handle
                mov ah,EMS_DEALLOCATE
                int 67h                         ; Free EMS memory
                mov EMS_alloc.EMS_handle,-1
int19_1:
                xor ax,ax
                mov ds,ax
                mov ax,old_int19_off
		cli				; Disable interrupts
                mov ds:[19h*4],ax               ; for the time to write
                mov ax,old_int19_seg            ; old interrupt vector back
                mov ds:[19h*4+2],ax
                popf                            ; Enable interrupts
                pop ds
                pop dx
                pop ax
                jmp old_int19
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

EMS_block       struc
EMS_sizel       dw ?
EMS_sizeh       dw ?
EMS_stype       db ?
EMS_shandle     dw ?
EMS_soff        dw ?
EMS_spage       dw ?
EMS_dtype       db ?
EMS_dhandle     dw ?
EMS_doff        dw ?
EMS_dpage       dw ?
EMS_block       ends

EMS_alloc_s struc       ; Changing this structure needs changes in srdisk.exe
EMS_handle      dw -1           ; EMS handle to disk memory (-1=no handle)
EMS_frame       dw ?            ; EMS page frame segment
EMS_cblk        EMS_block <>    ; EMS move command data structure
EMS_alloc_s ends

mac_resident_data macro
EMS_alloc EMS_alloc_s <>

if CAPABLE and C_NOALLOC
malloc EQU offset EMS_alloc
endif
                endm


;**************************************************************************
;
;                       Memory initialization
;
; Returns
;   carry set if error
;**************************************************************************
  
mac_init_memory macro
                local init1, init2, init3, init4, init_ret
                push    es

                mov ax,3567h                    ; Compare device name
                int 21h
                mov di,0Ah
                mov si,offset EMM_name
                mov cx,8
                cld
                repz cmpsb
                jne init1

                mov ah,EMS_GET_STATUS           ; Get status of EMM
                int 67h
                or ah,ah
                jnz init1

                mov ah,EMS_FRAME_SEGMENT        ; Get page frame address
                int 67h
                mov EMS_alloc.EMS_frame,bx
                or ah,ah
                jz init2
init1:
                  mov dx,offset errs_noEMS      ; "No expanded mem driver"
                  jmp init4
init2:
                mov ah,EMS_GET_VERSION          ; Verify EMS version
                int 67h
                or ah,ah
                jnz init1
                cmp al,40h
                jae init3
                  mov dx,offset errs_badEMS
                  jmp init4
init3:
                ; Try out the functions that are needed
                mov ah,EMS_ALLOCATE
                mov bx,1
                int 67h
                or ah,ah
                jnz init6
                mov EMS_alloc.EMS_handle,dx

                mov ah,EMS_REALLOCATE_PAGES
                mov bx,2
                int 67h
                or ah,ah
                jnz init5

                mov ah,EMS_MOVE_MEM_REGION
                mov al,0
                mov si,offset EMS_alloc.EMS_cblk
                mov [si].EMS_sizel,400h
                mov [si].EMS_sizeh,0
                mov [si].EMS_stype,1            ; Source in EMS
                mov [si].EMS_shandle,dx
                mov [si].EMS_soff,0
                mov [si].EMS_spage,0
                mov [si].EMS_dtype,1            ; Destination in EMS
                mov [si].EMS_dhandle,dx
                mov [si].EMS_doff,200h
                mov [si].EMS_dpage,1
                int 67h
                or ah,ah
                jnz init5

                mov ah,EMS_DEALLOCATE
                mov dx,EMS_alloc.EMS_handle
                int 67h
                jz init7

init5:
                mov ah,EMS_DEALLOCATE
                mov dx,EMS_alloc.EMS_handle
                int 67h
init6:
                mov dx,offset errs_nofuncEMS
                jmp init4

init7:
                mov EMS_alloc.EMS_handle,-1 ; -1 handle for no handle

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
                cmp EMS_alloc.EMS_handle,-1
                je done

                mov dx,EMS_alloc.EMS_handle
                mov ah,EMS_DEALLOCATE
                int 67h                         ; Free EMS memory
                mov EMS_alloc.EMS_handle,-1
done:
                endm

;**************************************************************************
;
;                       Initialization time data
;
;**************************************************************************

mac_init_data   macro
EMM_name        db 'EMMXXXX0'

errs_noEMS      db 'RAMDisk: Expanded Memory Manager not present.'
                db 0Dh, 0Ah, '$'

errs_badEMS     db 'RAMDisk: Invalid EMS version. EMS 4.0 needed.'
                db 0Dh, 0Ah, '$'

errs_nofuncEMS  db 'RAMDisk: EMS driver does not support needed functions'
                db ' or no memory available.'
                db 0Dh, 0Ah, '$'

                endm

