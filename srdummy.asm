;
;       ReSizeable RAMDisk dummy disk driver to take up drive letters
;
;       Copyright (C) 1992-1996, 2005 Marko Kohtala
;       Released under GNU GPL, read the file 'COPYING' for more information
;
;       To compile with TASM, find the appropriate srd???.asm file and:
;               tasm /m2 srd???.asm
;               tlink /t srd???.obj,srd???.sys

include dosstruc.inc
include define.inc

;**************************************************************************
;
;                Device driver start and the structures
;
;**************************************************************************

d_seg           segment page public
                assume ds:d_seg, cs:d_seg

                org     0
                ; Device driver header
drhdr_next      dd      -1              ; Pointer to next device (now last)
drhdr_attr      dw      DHATTR_NOFAT
drhdr_strategy  dw      offset strategy ; Offset to strategy function
drhdr_commands  dw      offset commands ; Offset to commands function
drhdr_units     db      0               ; Number of units

config_s struc
c_BPB_bps       dw      512             ; Sector size
c_BPB_spc       db      2               ; Cluster size in sectors
c_BPB_reserved  dw      1               ; The boot sector is reserved
c_BPB_FATs      db      2               ; One FAT copy
c_BPB_dir       dw      112             ; 112 entries in root directory
c_BPB_sectors   dw      720             ; number of sectors on 16-bit disk
c_BPB_media     db      0FDh            ; Media
c_BPB_FATsectors dw     2               ; Sectors per one FAT
c_BPB_spt       dw      9               ; Sectors per track
c_BPB_heads     dw      2               ; Number of heads
c_BPB_hiddenl   dw      0               ; # of hidden sectors (low word)
config_s ends

;conf            config_s <>
; TASM 3.0 bugs force this approach:
conf config_s <512,2,1,2,112,720,0FDh,2,9,2,0>

;**************************************************************************
;
;                       Set request header address
;
; Called by DOS to set the request structure pointer
;
;**************************************************************************
  
strategy        proc far
                mov word ptr cs:req_ptr,bx
                mov word ptr cs:req_ptr+2,es
                ret
strategy	endp
  
  
;**************************************************************************
;
;               Commands
;
; Called by DOS. Always tells that the drive is not ready.
;
;**************************************************************************
  
commands        proc far
                assume ds:nothing
                push si
                push ds

		lds si,req_ptr
                assume ds:nothing
                mov [si].rhStatus,DRIVE_NOT_READY OR ERROR OR DONE
                cmp [si].rhFunction,0
                jnz cmd_error
                  call cmd_init
cmd_error:
		pop ds
		pop si
                retf
                assume ds:d_seg
commands        endp


;**************************************************************************
;
;               Other internal and resident data
;
; The order of this data is not significant as it will not be used outside
;
;**************************************************************************

req_ptr         dd ?                    ; Request structure pointer

BPB             equ     byte ptr conf.c_BPB_bps

; Pointers to BPB (for cmd_init)
; This must be the last of the resident data, since end of resident is
; calculated from this pointer and number of units
pBPB            dw offset BPB           ; First unit (C)
                dw offset BPB           ; D
                dw offset BPB           ; E
                dw offset BPB           ; F
                dw offset BPB           ; G
                dw offset BPB           ; H
                dw offset BPB           ; I
                dw offset BPB           ; J
                dw offset BPB           ; K
                dw offset BPB           ; L
                dw offset BPB           ; M
                dw offset BPB           ; N
                dw offset BPB           ; O
                dw offset BPB           ; P
                dw offset BPB           ; Q
                dw offset BPB           ; R
                dw offset BPB           ; S
                dw offset BPB           ; T
                dw offset BPB           ; U
                dw offset BPB           ; V
                dw offset BPB           ; W
                dw offset BPB           ; X
                dw offset BPB           ; Y
                dw offset BPB           ; Z


;**************************************************************************
;
;                       prints macro
;
; This macro is used by initialization routines to display text.
; dx must point to the '$' terminated text about to be displayed.
;**************************************************************************
  
prints        macro
		mov	ah,9
                int     21h
              endm
  
;**************************************************************************
;
;                       Initialization time variables
;
;**************************************************************************

dos_drive       db 0                    ; DOS reported drive
def_drive       db 0                    ; User requested drive

;**************************************************************************
;
;                       INIT command
;
; Init command does the following:
;  - displays sign-on message
;  - read command line for drive letter
;    - abort on syntax errors
;  - set everything necessary up
;**************************************************************************
  
cmd_init        proc near
                pushf
                push ax
                push bx
                push cx
                push dx
                push si
                push di
                push ds
                push es
                cld

                ; Sign on message
                push cs
                pop ds
                mov dx,offset s_sign_on         ; "ReSizeable RAMdisk ver..."
                prints

                ; Find out the features about DOS and the drive letter
                call init_dos
                mov dx,offset errs_nodrive
                jc cmd_init_err

                cmp dos_drive,0
                jz cmd_init_err
cmd_init_read:
                call init_read_cmdline
                jnc cmd_init3
                ; call returned with DX set to error text
cmd_init_err:
                  prints
                  mov al,0
                  jmp cmd_init1
cmd_init3:
                mov al,def_drive
                sub al,dos_drive
                cmp al,1
                jge cmd_init1
                  mov al,0
cmd_init1:
                lds bx,req_ptr
                assume ds:nothing

                mov drhdr_units,al
                mov [bx].irUnits,al

                xor ah,ah
                cmp al,0
                jz cmd_init2
                  shl ax,1
                  add ax,offset pBPB
cmd_init2:
                mov word ptr [bx].irEndAddress,ax
                mov word ptr [bx].irEndAddress+2,cs
                mov word ptr [bx].irParamAddress,offset pBPB
		mov word ptr [bx].irParamAddress+2,cs
                mov [bx].rhStatus,DONE

                pop es
		pop ds
		pop di
		pop si
		pop dx
		pop cx
		pop bx
		pop ax
                popf
                ret

                assume ds:d_seg
cmd_init        endp

;**************************************************************************
;
;               CHECK DOS VERSION AND CAPABILITIES
;
;**************************************************************************

init_dos        proc near
                mov al,0
                mov dos_drive,al

                mov ax,4452h    ; DR-DOS?
                stc
                int 21h
                jc idos_notc
                cmp ax,dx
                jne idos_notc
                cmp ah,10h
                jne idos_notc   ; Not installed

                cmp al,67h      ; DR-DOS version 6.0 ?
                jne idos_notc   ; If not, treat it like MS-DOS

                  les si,req_ptr
                  mov al,es:[si].irDriveNumber
                  add al,'A'
                  mov dos_drive,al
                  jmp idos_x

idos_notc:
                les si,req_ptr
                cmp byte ptr es:[si],16h        ; Device number supported?
                jbe idos_fail
                  les si,req_ptr
                  mov al,es:[si].irDriveNumber
                  add al,'A'
                  mov dos_drive,al
idos_x:
                cmp dos_drive,'C'               ; Is invalid drive?
                jb idos_fail
                cmp dos_drive,'Z'
                jb idosx2
idos_fail:
                stc
                ret
idosx2:
                clc
                ret
init_dos        endp

  
;**************************************************************************
;
;               READ COMMAND LINE
;
; Return carry set if error
;**************************************************************************

init_read_cmdline proc near
                push ds

                les bx,req_ptr
		lds si,es:[bx].irParamAddress	; Pointer to cmd line
                assume ds:nothing

irc1:           lodsb                           ; Skip over the driver name
                cmp al,9 ;tab
                je irc2
                cmp al,' '
                je irc2
                ja irc1
                jmp irc_eol
irc2:
irc_narg:       call irc_skip_space

                cmp al,' '                      ; Every ctrl character ends
                jb irc_eol

                and al,11011111b                ; Make lowercase to uppercase
                cmp al,'A'
                jb irc_syntax
                cmp al,'Z'
                ja irc_syntax

                cmp byte ptr [si],':'
                jne irc3
                inc si                          ; Skip ':'
irc3:           
                mov cs:def_drive,al
                jmp irc_narg

irc_syntax:     mov dx,offset errs_syntax
                stc
                pop ds
                ret

irc_eol:        clc
                pop ds
                ret
init_read_cmdline endp

irc_skip_space  proc near
ircs1:          lodsb
                cmp al,' '
                je ircs1
                cmp al,9 ;tab
                je ircs1
                ret
irc_skip_space  endp

                assume ds:d_seg

  

;**************************************************************************
;
;                       Initialization strings
;
;**************************************************************************

errs_syntax     db 'SRDUMMY: Syntax error', 0Dh, 0Ah, 0Dh, 0Ah
                db 'Syntax: SRDUMMY.SYS [d:]', 0Dh, 0Ah, 0Dh, 0Ah
                db ' d:', 9, 'Drive that the next block device should go to.'
                db 0Dh, 0Ah, '$'

errs_nodrive    db 'SRDUMMY: This DOS version does not tell the drive '
                db 'letter to device driver.', 0Dh, 0Ah
                db '         Can not install.', 0Dh, 0Ah, 0Dh, 0Ah, '$'

s_sign_on       db 0Dh, 0Ah, 'SRDISK dummy disk driver'
                db ' version ', SRD_VERSION, '. '
                db 'Copyright (c) 2005 Marko Kohtala.'
                db 0Dh, 0Ah, '$'


;**************************************************************************
;
;                       A note for binary debuggers
;
;**************************************************************************

db 0Dh, 0Ah, "Copyright (c) 1992-2005 Marko Kohtala. "
db 0Dh, 0Ah, "Contact at 'kohtala@users.sourceforge.net'."
db 0Dh, 0Ah

d_seg           ends
                end

