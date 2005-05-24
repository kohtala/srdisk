; SRDISK - device driver definitions
; Copyright (c) 1992-1996, 2005 Marko Kohtala
; Released under GNU GPL, read the file 'COPYING' for more information

SRD_VERSION     equ "2.09"      ; FOUR LETTER VERSION STRING

; General capabilities in conf.c_flags

C_APPENDED      equ 1   ; Capable of having appended drivers
C_MULTIPLE      equ 2   ; Capable of driving many disks
C_32BITSEC      equ 4   ; Capable of handling over 32-bit sector addresses
C_NOALLOC       equ 8   ; Incapable of allocating memory by itself
C_GIOCTL        equ 10h ; Supports Generic IOCTL
C_DISK_IMAGE    equ 20h ; Can load disk image on startup
C_UNKNOWN       equ 0C0h

; If C_NOALLOC is not set, the malloc field in configuration structure points
; into the allocation function. If C_NOALLOC is set, then the malloc
; points into the data that SRDISK.EXE needs to be able to handle the memory
; by itself.

; Boolean
TRUE    EQU 1
FALSE   EQU 0

; Some important internal settings
MULTIPLEXAH     equ     72h             ; Multiplex int 2F AH parameter
V_FORMAT        equ     1               ; config_s format version used here
IOCTL_ID        equ     5253h           ; IOCTL write length for call ('SR')

;IFNDEF DEBUGGING
DEBUGGING       equ     FALSE
;DEBUGGING       equ     TRUE
;ENDIF

DEBUGINIT       equ     FALSE

; disk_IO parameter locations
dIO_bytes       equ word ptr [bp-6]     ; May be changed in disk_IO
dIO_byteoffset  equ dword ptr [bp-4]    ; May be changed in disk_IO
dIO_buffer      equ dword ptr [bp+6]    ; Changed in batch_xfer
dIO_sector      equ dword ptr [bp+10]   ; Changed in batch_xfer
dIO_count       equ word ptr [bp+14]    ; Changed in batch_xfer
dIO_RW          equ byte ptr [bp+16]

;******************* Derived definitions ********************

if DEBUGGING
debug   macro char,num
        push ax
        push dx
        IFDIFI <num>,<dx>
          IF TYPE num EQ 1
          xor dh,dh
          mov dl,num
          ELSE
          mov dx,num
          ENDIF
        ENDIF
        mov al,char
        call debug_c
        pop dx
        pop ax
        endm
else
debug   macro char
        endm
endif ;DEBUGGING


