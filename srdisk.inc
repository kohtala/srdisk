;
;       ReSizeable RAMDisk device driver
;
;       Copyright (C) 1992-1996, 2005 Marko Kohtala
;       Released under GNU GPL, read the file 'COPYING' for more information
;
;       To compile with TASM, find the appropriate srd???.asm file and:
;               tasm /m2 srd???.asm
;               tlink /t srd???.obj,srd???.sys

include dosstruc.inc

DR_ATTRIBUTES = DHATTR_NOFAT or DHATTR_OPENCLOSE

if CAPABLE and C_32BITSEC
DR_ATTRIBUTES = DR_ATTRIBUTES or DHATTR_32BIT
endif

if CAPABLE and C_GIOCTL
DR_ATTRIBUTES = DR_ATTRIBUTES or DHATTR_GIOCTL
endif

;**************************************************************************
;
;                Device driver start and the structures
;
;**************************************************************************

d_seg           segment page public
                assume ds:d_seg, cs:d_seg

                org     0
; The following is to be considered as both a device driver header and
; as a starting point for the configuration table. This driver will be
; identified by its segment address and this structure must be at offset
; 0.

                ; Device driver header
drhdr_next      dd      -1              ; Pointer to next device (now last)
drhdr_attr      dw      DR_ATTRIBUTES
drhdr_strategy  dw      offset strategy ; Offset to strategy function
drhdr_commands  dw      offset commands ; Offset to commands function
drhdr_units     db      1               ; Number of units

; The rest has four functions to be considered
;  1) usable as device driver name if this driver is changed
;     into character device on init.
;  2) usable as a label to be returned in media check call
;  3) identifies this device driver as SRDISK driver by always having
;     the letters SRD at offset dr_ID
;  4) identifies the memory used by the 4 char string at offset dr_memory

dr_volume       label byte
dr_ID           db 'SRD'                ; SRDISK signature (3 char)
dr_memory       db MEMSTR               ; Memory type identifier (4 char)
dr_version      db SRD_VERSION          ; Device driver version (4 char)
                db 0                    ; null to end volume label
dr_v_format     db V_FORMAT             ; Configuration format version
dr_conf         dw offset conf          ; Pointer to drive configuration


disk_IO         proc far
                mac_disk_IO             ; Disk access function
disk_IO         endp

ife CAPABLE and C_NOALLOC
malloc          proc far
                mac_malloc              ; Memory allocation function
malloc          endp

freemem         proc far
                mac_freemem
freemem         endp

else    ; ife CAPABLE and C_NOALLOC

freemem equ 0
endif   ; ife CAPABLE and C_NOALLOC

ret_far proc far                        ; Null function to replace
        ret                             ;  strategy and command when appended
ret_far endp                            ;  to another SRDISK drive

if HOOKINT19
int_19_entry    proc far
                mac_int_19              ; INT 19 boot function
int_19_entry    endp

old_int19       label dword             ; Address of old INT 19
old_int19_off   dw      -1
old_int19_seg   dw      -1
endif   ; HOOKINT19

mac_resident_data                       ; Memory specific resident data

;**************************************************************************
;
;                       Debugging code
;
; This code prints out a character and a word in hex. This code can be
; used using "debug char,word" macro in the code to give some output of
; the actions device driver is doing.
;
; A color display is assumed with 80 characters on a row and 25 rows.
;
;**************************************************************************

if DEBUGGING
                assume ds:nothing

debug_c         proc near
                push es
                push di

                mov ah,d_attr           ; Load color attribute
                mov di,0B800h           ; Load screen segment (assumes color)
                mov es,di
                mov di,d_loc            ; Load line
                cmp di,26*160           ; Below screen?
                jb debug_c1
                  mov di,2*160          ; Yes, move to third line (for scroll off)
                  mov d_loc,di
                  add ah,10h            ; Change color
                  cmp ah,70h
                  jb debug_c2
                    sub ah,60h
debug_c2:         mov d_attr,ah

                  push es               ; Wait if shift down
                  push ax
                  mov ax,40h
                  mov es,ax
debug_c3:         test byte ptr es:[17h],3
                  jnz debug_c3
                  pop ax
                  pop es
debug_c1:
                add di,d_col            ; Advance to right column
                mov es:[di],ax          ; Print error letter

                call debug_x            ; Print high byte
                mov dh,dl
                call debug_x            ; Print low byte

                add d_loc,160           ; Next line

                pop di
                pop es
                ret
debug_c         endp

debug_x         proc near               ; Print a byte in hex
                mov al,dh
                shr al,1
                shr al,1
                shr al,1
                shr al,1
                call debug_x1
                mov al,dh
debug_x1:       and al,0Fh              ; Print a hex digit
                cmp al,10
                jae debug_x2
                  add al,'0'
                  jmp debug_x3
debug_x2:       add al,'A'-10
debug_x3:
                inc di
                inc di
                mov es:[di],ax
                ret
debug_x         endp

d_loc   dw 2*160
d_col   dw 150
d_attr  db 40h

                assume ds:d_seg

endif ; DEBUGGING


;**************************************************************************
;
;                       Configuration tables
;
; This structure holds all the formatting data used by the formatter.
; The formatter is passed a pointer to this data and it modifies it
; directly. For this arrangement to work THE BELOW TABLE MAY NOT BE
; MODIFIED WITHOUT PROPER CHANGES IN SRDISK.EXE. dr_v_format contains
; version number which is to be changed when a change is made to this
; structure.
;
; Only the first fields up to label appended_eor is resident and used in
; in every case. The rest is used only in the main driver in a chain of
; RAM disk drivers.
;
; !!! The formatter may use any initial values in this structure as
;     default values i.e. set all needed values here !!!
; !!! A DR-DOS bug must be avoided by defining c_BPB_sectors and
;     c_BPB_FATsectors so that they could be real !!!
;**************************************************************************

READ_ACCESS     equ     1       ; Bit masks for the RW_access
WRITE_ACCESS    equ     2

config_s struc
c_drive         db      ?               ; Drive letter
c_flags         db      CAPABLE         ; Misc capability flags
c_disk_IO       dw      offset disk_IO  ; disk_IO entry
c_disk_IO_seg   dw      0               ; disk_IO entry segment
c_malloc        dw      malloc          ; malloc entry offset (in DS/CS)
c_freemem       dw      freemem         ; free mem query entry offset (in DS/CS)
c_next          dw      0               ; Next driver in chain (segment)
c_maxK          dd      MAX_SIZE        ; Maximum allowed size
c_size          dd      0               ; Current allocated size in Kbytes
c_allocblock    dw      ALLOCBLOCK      ; Memory allocation block size
c_sectorsl      dw      0               ; Sectors in this driver (low word)
c_sectorsh      dw      0               ; Sectors in this driver (high word)
c_bps_shift     db      7               ; 2^c_bps_shift == c_BPB_bps

c_BPB_bps       dw      512             ; Sector size
c_BPB_spc       db      1               ; Cluster size in sectors
c_BPB_reserved  dw      1               ; The boot sector is reserved
c_BPB_FATs      db      1               ; One FAT copy
c_BPB_dir       dw      64              ; 64 entries in root directory
c_BPB_sectors   dw      100             ; number of sectors on 16-bit disk
c_BPB_media     db      0FAh            ; Media is RAM DISK
c_BPB_FATsectors dw     1               ; Sectors per one FAT
c_BPB_spt       dw      8               ; Sectors per track
c_BPB_heads     dw      1               ; Number of heads
c_BPB_hiddenl   dw      0               ; # of hidden sectors (low word)
c_BPB_hiddenh   dw      0               ; # of hidden sectors (high word)
c_BPB_tsectorsl dw      0               ; # of sectors on 32-bit disk (low word)
c_BPB_tsectorsh dw      0               ; # of sectors on 32-bit disk (high)

c_tsize         dd      0               ; Total size in Kbytes (all drivers)

c_RW_access     db      000b            ; B0 = read, B1 = write, B2 = fixed
c_media_change  db      1               ; -1 if media changed, 1 if not
c_device_type   db      0               ; Device type for Generic IOCTL
c_open_files    dw      0               ; Files currently open on drive
c_next_drive    dw      0               ; Segment of next SRDISK drive
c_transfer      dw      batch_xfer      ; Batch xfer function address
config_s ends

;conf            config_s <>
; TASM 3.0 bugs force this approach:
conf config_s <?,CAPABLE,offset disk_IO,0,malloc,freemem,0,MAX_SIZE,0,ALLOCBLOCK,0,0,9,512,1,1,1,64,100,0FAh,1,8,1,0,0,0,0,0,0,1,0,0,0,batch_xfer>

appended_eor    equ offset conf.c_BPB_spc ; End of resident for appended driver


;**************************************************************************
;
;               Other internal and resident data
;
; The order of this data is not significant as it will not be used outside
;
;**************************************************************************

BPB             equ     byte ptr conf.c_BPB_bps
pBPB            dw offset BPB           ; Pointer to BPB (for cmd_init)

old_multiplex   dd ?                    ; Multiplex hook

req_ptr         dd ?                    ; Request structure pointer

ifdef STACKSIZE
caller_sp       dw ?                    ; To store pointer to callers stack
caller_ss       dw ?                    ;   if we use our own stack
endif

                ; Pointers to commands
command_tbl     dw cmd_init             ;  0 Init
                dw cmd_media            ;  1 Media
                dw cmd_BPB              ;  2 Build BPB
                dw cmd_unknown          ;  3 IOCTL input
                dw cmd_input            ;  4 Input
                dw cmd_unknown          ;  5 nondest input (char)
                dw cmd_unknown          ;  6 input status (char)
                dw cmd_unknown          ;  7 input flush (char)
                dw cmd_output           ;  8 Output
                dw cmd_output           ;  9 Output with verify
                dw cmd_unknown          ; 10 output status (char)
                dw cmd_unknown          ; 11 output flush (char)
                dw cmd_unknown          ; 12 IOCTL output
                dw cmd_open             ; 13 Open device
                dw cmd_close            ; 14 Close device
                dw cmd_removable        ; 15 Removable media check
if CAPABLE and C_GIOCTL
                dw cmd_unknown          ; 16 Output untill busy (char)
                dw cmd_unknown          ; 17
                dw cmd_unknown          ; 18
                dw cmd_GIOCTL           ; 19 Generic IOCTL
                dw cmd_unknown          ; 20
                dw cmd_unknown          ; 21
                dw cmd_unknown          ; 22
                dw cmd_getLogical       ; 23
                dw cmd_setLogical       ; 24
endif

high_cmd        db ($-offset command_tbl)/2  ; Number of supported commands

;**************************************************************************
;
;                       Batch transfer of sectors
;
; This transfers sectors according to the parameters passed on stack
; even if the sectors are on different drivers chained to this one.
;
; Returns how many sectors were NOT moved; ax = 0 means all OK.
;
; !!! This function destroys the contents of DS !!!
;
;**************************************************************************

batch_xfer      proc C far
                arg buffer:far ptr, sector:dword, count:word, RW:word
                local byteoffset:dword, bytes:word

                debug 'o',<word ptr buffer>
                debug 's',<word ptr buffer+2>

if CAPABLE and C_GIOCTL
                test RW,-1                      ; Writing boot sector?
                jz batch_notboot
                mov ax,word ptr sector
                or ax,word ptr sector+2
                jnz batch_notboot

                mov ds,word ptr buffer+2
                assume ds:nothing
                mov si,word ptr buffer
                add si,bsBytesPerSec
                mov ax,[si]
                cmp ax,conf.c_BPB_bps
                jnz batch_notboot

                push cs                         ; Copy new BPS to conf
                pop es
                mov di,offset conf.c_BPB_bps

                mov cx,25
                rep movsb

                mov conf.c_media_change,0       ; Tell do not know if changed
batch_notboot:
endif
                push cs
                pop ds

                mov si,offset conf
                ; DS:SI - conf of current driver (main here)
                assume ds:nothing

batch_next_driver:
                ; Transfer sector to byte offset to disk and count the
                ; number of bytes to xfer.
                mov ax,word ptr sector
                mov dx,word ptr sector+2
                mov cx,count
                debug 'a',ax
                debug 'd',dx
                debug 'c',cx

                cmp ax,[si].c_sectorsl          ; Starting sector on disk?
                sbb dx,[si].c_sectorsh
                mov dx,word ptr sector+2
                jae batch_to_next               ; No

                add ax,cx                       ; Count ending sector
                adc dx,0

                cmp dx,[si].c_sectorsh          ; Ending sector on disk?
                jb batch_IO1                    ; Jump if is
                sub ax,[si].c_sectorsl
                jbe batch_IO1                   ; Jump if is

                sub cx,ax                       ; Count how many we CAN transfer
                debug 'T',cx
batch_IO1:
                push cx
                mov ax,cx                       ; Count number of bytes to move
                ;mul [si].c_BPB_bps
                mov cl,[si].c_bps_shift
                sal ax,cl
                mov word ptr bytes,ax           ; Number of bytes to move

                mov ax,word ptr sector+2        ; Count starting byte
                ;mul [si].c_BPB_bps
                sal ax,cl
                mov cx,ax
                mov ax,word ptr sector
                mul [si].c_BPB_bps
                add dx,cx                       ; dx:ax is starting byte
                mov word ptr byteoffset,ax
                mov word ptr byteoffset+2,dx

                call dword ptr [si].c_disk_IO
                pop ax                          ; Pop number of sectors
                jc batch_IOx                    ; I/O error

                sub count,ax                    ; All transferred?
                jbe batch_done                  ;  yes, so exit

if CAPABLE and C_APPENDED
                debug 'N',ax
                push ax
                ;mul [si].c_BPB_bps              ; Increment transfer offset
                mov cl,[si].c_bps_shift
                sal ax,cl
                add word ptr buffer,ax          ;  which can not exceed 0FFFFh
                pop ax

                xor dx,dx
                add ax,word ptr sector          ; Count new starting sector
                adc dx,word ptr sector+2
batch_to_next:
                sub ax,[si].c_sectorsl          ; Subtract passed sectors
                sbb dx,[si].c_sectorsh
                mov word ptr sector,ax          ; And store the result
                mov word ptr sector+2,dx

                mov si,[si].c_next              ; Find next driver
                or si,si
                mov ds,si
                mov si,ds:dr_conf
                jz batch_err
                jmp batch_next_driver

else    ;if CAPABLE and C_APPENDED

batch_to_next:

endif   ;if CAPABLE and C_APPENDED

batch_err:
                ; there is no next driver - sectors not found!
                debug 'E',count
                stc
                jmp batch_IOx
batch_done:
                clc
batch_IOx:
                mov ax,count
                ret

                assume ds:d_seg
batch_xfer      endp

  
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
;                       Commands
;
; Called by DOS. Requested action defined in structure pointed by req_ptr.
;
;**************************************************************************
  
commands        proc far
                assume ds:nothing
                pushf
                push ax
                push bx
                push cx
                push dx
                push si
                push di
                push ds
                push es

                ifdef STACKSIZE
                ; Switch into our own stack
                mov caller_ss,ss
                mov caller_sp,sp
                push cs
                pop ss
                mov sp,end_of_resident
                endif

		if DEBUGGING
                push bp
                mov bp,sp
                endif

                cld
		lds si,req_ptr
                assume ds:nothing
		; We trust Microsoft that the unit is right at [si].rhUnit
		mov cx,[si].rwrBytesSec 	; Sectors/Cmd line/BPB pointer
		mov dx,[si].rwrStartSec 	; Start sector/Device number
		mov bl,[si].rhFunction
                xor bh,bh                       ; Count index to command_tbl
                debug 'F',bx
                cmp bl,high_cmd                 ; Is command supported?
                jae cmd_unknown                 ; Jump if not
		shl bx,1
		les di,[si].rwrBuffer		; ES:DI = transfer address
		push cs
		pop ds				; DS to local data segment
                assume ds:d_seg
		jmp command_tbl[bx]		; Do command
cmd_unknown:
                assume ds:nothing
		mov ax,ERROR or UNKNOWN_COMMAND
		jmp cmd_ret
cmd_IOerr:
		lds bx,req_ptr
		mov [bx].rwrBytesSec,0		; Sector count zero
cmd_error:
		mov ah,HIGH (ERROR OR DONE)	; ERROR and DONE (err #, in al)
		jmp cmd_ret

cmd_ok:
		mov ax,DONE
cmd_ret:
                debug 'Q',ax
		lds bx,req_ptr
		mov [bx].rhStatus,ax

                if DEBUGGING
                cmp bp,sp
                je dbstack1
                debug '/',sp
                debug '/',bp
dbstack1:
                mov sp,bp
                pop bp
                endif

                ifdef STACKSIZE
                ; Switch back into callers stack
                mov ss,caller_ss
                mov sp,caller_sp
;                cmp stack_check,STACK_CHECK_ID
;                je cmd_stack_ok
                   ;report error
;cmd_stack_ok:
                endif

		pop es
		pop ds
		pop di
		pop si
		pop dx
		pop cx
		pop bx
		pop ax
                popf
		retf				; Return far
                assume ds:d_seg
commands	endp

;**************************************************************************
;
;               Media Check command
;
;**************************************************************************

cmd_media       proc near
                les bx,req_ptr
                mov al,conf.c_media_change      ; Read the change return
                debug 'C',ax

                test conf.c_RW_access,READ_ACCESS
                jz dev_not_ready

		mov es:[bx].mrReturn,al
		mov word ptr es:[bx].mrVolumeID,offset dr_volume
		mov word ptr es:[bx].mrVolumeID+2,cs
                jmp cmd_ok

dev_not_ready:
                mov al,DRIVE_NOT_READY
                jmp cmd_error

cmd_media       endp


;**************************************************************************
;
;               Build BPB command
;
;**************************************************************************

cmd_BPB         proc near
                debug 'B',<conf.c_RW_access>
                test conf.c_RW_access,READ_ACCESS
                jz dev_not_ready

                les bx,req_ptr
                mov word ptr es:[bx].bbrBPBAddress,offset BPB
                mov word ptr es:[bx].bbrBPBAddress+2,cs
                mov conf.c_open_files,0         ; Reset open files to 0
                mov conf.c_media_change,1       ; Media no longer changed
                jmp cmd_ok
cmd_BPB         endp


;**************************************************************************
;
;               Device Open command
;
;**************************************************************************

cmd_open        proc near
                debug 'O',-1
                inc conf.c_open_files
                jmp cmd_ok
cmd_open        endp


;**************************************************************************
;
;               Device Close command
;
;**************************************************************************

cmd_close       proc near
                cmp conf.c_open_files,0
                jz cmd_close1
                dec conf.c_open_files
cmd_close1:
                jmp cmd_ok
cmd_close       endp


;**************************************************************************
;
;               Removable media query
;
;**************************************************************************

cmd_removable   proc near
                mov ax,DONE
                test conf.c_RW_access,4
                jz cmd_removable_x
                or ax,BUSY
cmd_removable_x:
                jmp cmd_ret
cmd_removable   endp

;**************************************************************************
;
;               INPUT command
;
;**************************************************************************

cmd_input       proc near
                debug 'R',0
                test conf.c_RW_access,READ_ACCESS
                jz cmd_input1
                xor bl,bl
                push bx                         ; Last parameter to batch_xfer
                jmp cmd_io
cmd_input1:
                mov al,DRIVE_NOT_READY
                jmp cmd_IOerr
cmd_input       endp


;**************************************************************************
;
;               OUTPUT command
;
;**************************************************************************

cmd_output      proc near
                debug 'W',0
                mov al,WRITE_PROTECT
                mov bl,1
                test conf.c_RW_access,WRITE_ACCESS
                jnz to_cmd_io
                  jmp cmd_outerr
to_cmd_io:      push bx                         ; Last parameter to batch_xfer

cmd_io:
                ; CX    - sectors
                ; ES:DI - transfer address
                ; DS    = CS

                mov ax,cx                       ; Count number of bytes to move
                mul conf.c_BPB_bps
                jc cmd_output4                  ; Is it too much? (dx != 0)

                ; check transfer address and count that they do not
                ; exceed segment limit
                add ax,di                       ; (dx = 0 after the mul)
                jnc cmd_output5
cmd_output4:
                mov ax,di                       ; How many bytes CAN we move?
                xor ax,-1
                ;xor dx,dx                       ; How many sectors?
                ;div conf.c_BPB_bps
                mov cl,conf.c_bps_shift
                shr ax,cl
                mov cx,ax
cmd_output5:
                push cx                         ; count parameter to batch_xfer
                                
;                mov cx,es
;                les si,req_ptr
;                mov es:[si].rwrBytesSec,dx     ; Xferred 0 sectors so far
;                cmp es:[si].rwrLength,1Ah        ; Do we use 32-bit sec. address?
;                jbe cmd_output3
;                  debug '3',0
;                  push word ptr es:[si].rwrHugeStartSec+2
;                  push word ptr es:[si].rwrHugeStartSec
;                  jmp cmd_output8
;cmd_output3:
;                xor dx,dx                       ; 16 bit sector parameter
;                push dx                         ;  to batch_xfer
;                push es:[si].rwrStartSec

                mov cx,es
                les si,req_ptr
                mov ax,es:[si].rwrStartSec
                cmp ax,-1
                jne cmd_output3
                  debug '3',0
		  push word ptr es:[si].rwrHugeStartSec+2
		  push word ptr es:[si].rwrHugeStartSec
                  jmp cmd_output8
cmd_output3:
                xor dx,dx                       ; 16 bit sector parameter
                push dx                         ;  to batch_xfer
                push ax

cmd_output8:
                push cx                         ; buffer parameter to batch_xfer
                push di

                call batch_xfer
                assume ds:nothing
                jc cmd_output1
                add sp,12

                les si,cs:req_ptr
		sub es:[si].rwrBytesSec,ax     ;  actually transferred
cmd_output6:
                jmp cmd_ok

cmd_output1:
                add sp,12
                debug 'S',ax
                mov al,SECTOR_NOT_FOUND
cmd_outerr:
                jmp cmd_IOerr
                assume ds:d_seg
cmd_output      endp
  

if 0
;**************************************************************************
;
;               IOCTL write
;
; This function is used to switch to "inside DOS" and avoid some
; multitaskin memory manager problems. We hope that the multitasker does
; not free the memory if it is allocated here.
;
; The actual service routine is located in the caller at the address
; where the data location is. Data size must be IOCTL_ID.
;
;**************************************************************************

cmd_IOCTL_write proc near
                debug 'i',cx
                cmp cx,IOCTL_ID         ; cx is the count
                je cmd_iow1
                  jmp cmd_unknown
cmd_iow1:
                lds si,req_ptr
                assume ds:nothing
                call [si].irwrBuffer
                jmp cmd_ok
                assume ds:d_seg
cmd_IOCTL_write endp
endif

;**************************************************************************
;
;               MULTIPLEX server
;
;**************************************************************************

multiplex       proc far
                cmp ah,MULTIPLEXAH
                jne mplex_old
                cmp al,0
                jz mplex_installed
                push cs                 ; Tell our segment
                pop es
mplex_installed:
                mov al,-1               ; Tell we are installed
                iret
mplex_old:
                jmp cs:old_multiplex
multiplex       endp

;**************************************************************************
;
;               GENERIC IOCTL
;
;**************************************************************************

if CAPABLE and C_GIOCTL
ifdef STACKSIZE
noGIOCTL_eor equ offset $ + STACKSIZE   ; End of resident when no GIOCTL
                                        ; but when we have stack!
else
noGIOCTL_eor equ offset $       ; End of resident when GIOCTL not supported
endif

GIOCTL_MEDIA equ FALSE          ; If true, MID info is handled

if GIOCTL_MEDIA
media_ID        MID <0, 12345678h, "SRDISK     ", "FAT12   ">
endif

gioctl_func_s struc
gioctl_fcode    db ?
gioctl_faddr    dw ?
gioctl_func_s ends

gioctl_func     gioctl_func_s <41h, offset gioctl_write>
                gioctl_func_s <60h, offset gioctl_getparam>
                gioctl_func_s <61h, offset gioctl_read>
                gioctl_func_s <42h, offset cmd_ok>      ; Format
                gioctl_func_s <62h, offset cmd_ok>      ; Verify
;               gioctl_func_s <40h, offset gioctl_setparam>
if GIOCTL_MEDIA
                gioctl_func_s <46h, offset gioctl_setmedia>
                gioctl_func_s <66h, offset gioctl_getmedia>
endif
gioctl_func_end label byte


cmd_GIOCTL      proc near
                les si,req_ptr
		cmp es:[si].giCategory,8
                jnz cmd_gioerr1

		mov al,es:[si].giMinorCode
                debug 'G',ax

		les si,es:[si].giIOCTLData	; Parameter block
                mov bx,offset gioctl_func
cmd_gionfunc:
                cmp [bx].gioctl_fcode,al
                je cmd_giodofunc
                add bx,type gioctl_func_s
                cmp bx,offset gioctl_func_end
                jb cmd_gionfunc
cmd_gioerr1:
                jmp cmd_unknown
cmd_giodofunc:
                jmp [bx].gioctl_faddr           ; Call with ES:SI -> param block

gioctl_dev_not_ready:
                mov al,DRIVE_NOT_READY
                jmp cmd_error

cmd_GIOCTL      endp

;******** GIOCTL write (41h)

gioctl_write    proc near
                test conf.c_RW_access,WRITE_ACCESS
                jz gioctl_write_prot

                mov al,1
                push ax                         ; RW parameter to batch_xfer
                jmp track_xfer

gioctl_write_prot:
                mov al,WRITE_PROTECT
                jmp cmd_error
gioctl_write    endp

;******** GIOCTL read (61h)

gioctl_read     proc near
                test conf.c_RW_access,READ_ACCESS
                jz gioctl_dev_not_ready

                mov al,0
                push ax                         ; RW parameter to batch_xfer
track_xfer:
                ; !!!! Expects c_BPB_heads * cylinders
                ; and c_BPB_heads * c_BPB_spt less than 65536
		push es:[si].rwSectors
		mov ax,es:[si].rwCylinder
                mul conf.c_BPB_heads
                mul conf.c_BPB_spt
                mov bx,ax
                mov cx,dx
		mov ax,es:[si].rwHead
                mul conf.c_BPB_spt
		add ax,es:[si].rwFirstSector
                add ax,bx
                adc dx,cx
                push dx                         ; absolute sector
                push ax

		push word ptr es:[si].rwBuffer+2 ; transfer address
		push word ptr es:[si].rwBuffer

                call batch_xfer                 ; Destroys DS
                add sp,12
                jc track_xfer_IOerr

                jmp cmd_ok

track_xfer_IOerr:
                mov al,SECTOR_NOT_FOUND
                jmp cmd_error
gioctl_read     endp

;******** GIOCTL set parameters (40h)


if 0
gioctl_setparam proc near
;               test conf.c_RW_access,READ_ACCESS       ; !!!! Is this needed?
;               jz gioctl_dev_not_ready

                debug '0',<word ptr es:[si]>
                debug '2',<word ptr es:[si+2]>
                debug '4',<word ptr es:[si+4]>
                debug '6',<word ptr es:[si+6]>
                debug 'R',<word ptr es:[si].dpRootDirEnts>
                debug 'S',<word ptr es:[si].dpSectors>  ; Sectors
		debug 'M',<word ptr es:[si].dpMedia>    ; Media
;               test byte ptr es:[si].dpSpecFunc,3 ; Change format?
;               jnz gioctl_bad_media
;               jmp cmd_ok
gioctl_bad_media:
                mov al,UNKNOWN_MEDIA
                jmp cmd_error
gioctl_setparam endp
endif

;******** GIOCTL get parameters (60h)

gioctl_getparam proc near
;               test conf.c_RW_access,READ_ACCESS       ; !!!! Is this needed?
;               jz gioctl_dev_not_ready

                push ds
                push es
                pop ds
                assume ds:nothing

                ; Tell that we return info about current medium
;               mov [si].dpSpecFunc,GETDPSF_CURRENT

                mov al,conf.c_device_type
                mov [si].dpDevType,al
                mov [si].dpDevAttr,0   ; bit 0 Removable, bit 1 no change line

                mov ax,conf.c_BPB_heads
                mul conf.c_BPB_spt              ; Implicit dx = 0
                mov cx,ax
                mov ax,conf.c_BPB_sectors
                or ax,ax
                jnz gioctl_gp16bit
                  mov ax,conf.c_BPB_tsectorsl   ; 32-bit disk
                  mov dx,conf.c_BPB_tsectorsh
gioctl_gp16bit:
                div cx
                mov [si].dpCylinders,ax

                mov [si].dpMediaType,0  ; Normal media

                pop ds
                assume ds:d_seg

                mov di,si
                mov si,offset conf.c_BPB_bps    ; ds:si -> current BPB
                add di,7                        ; es:di -> Device Params BPB
                mov cx,25
                rep movsb               ; Copy BPB

                jmp cmd_ok
gioctl_getparam endp

if GIOCTL_MEDIA
gioctl_setmedia proc near
                push es
                pop ds
                push cs
                pop es
                mov di,offset media_ID
                mov cx,size MID
                rep movsb
                jmp cmd_ok
gioctl_setmedia endp

gioctl_getmedia proc near
                mov di,si
                mov si,offset media_ID
                mov cx,size MID
                rep movsb
                jmp cmd_ok
gioctl_getmedia endp
endif

cmd_getLogical  proc near
                lds si,req_ptr
                assume ds:nothing
                debug 'l',<word ptr [si].ldrUnit>
                mov [si].ldrUnit,0
to_cmd_ok:
                jmp cmd_ok
                assume ds:d_seg
cmd_getLogical  endp

cmd_setLogical  proc near
                lds si,req_ptr
                assume ds:nothing
                debug 'L',<word ptr [si].ldrUnit>
                cmp [si].ldrUnit,0
                je to_cmd_ok
                mov al,UNKNOWN_UNIT
                jmp cmd_error
                assume ds:d_seg
cmd_setLogical  endp

endif ;if CAPABLE and C_GIOCTL

;**************************************************************************
;
;               LOCAL STACK
;
; The stack replaces part of the initialization code.
;**************************************************************************

ifdef STACKSIZE
;stack_check     dw STACK_CHECK_ID
db STACKSIZE / 8 dup ('STACK   ')       ; To avoid stack over init code
align 16                                ; Make use of space left on page
endif

end_of_resident EQU offset $
; MARKS THE END OF RESIDENT PORTION OF DRIVER

;**************************************************************************
;**************************************************************************
;**************************************************************************
;**************************************************************************
;
;               INITIALIZATION TIME CODE
;
; This code and data is removed from the memory after driver is
; initialized.
;**************************************************************************

dos_drive       db ?                    ; DOS reported drive
def_drive       db 'C'                  ; Defined drive, where to install

install_ok      db FALSE                ; Installed OK

main_config     dd      ?               ; Pointer to main configuration table

FLAG_1ST        EQU 1                   ; First SRDISK driver
FLAG_FORCEDRIVE EQU 2                   ; Force drive letter (in def_drive)
FLAG_APPEND     EQU 4                   ; Append to other SRDISK driver
FLAG_APPENDED   EQU 8                   ; Appended to other SRDISK driver
FLAG_KNOWDRIVE  EQU 10h                 ; Drive letter known
FLAG_32BITS     EQU 20h                 ; Capable of 32 bit sector addressing
FLAG_GIOCTL     EQU 40h                 ; Supports GIOCTL
flags           db 0

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
  
if DEBUGINIT

print_x         proc near               ; Print a dword in hex
print_lx:       push ax
                mov ax,dx
                call print_sx
                mov ah,2
                mov dl,':'
                int 21h
                pop ax
print_sx:                               ; Print a word in hex
                push ax
                mov al,ah
                call print_cx
                pop ax
print_cx:                               ; Print a byte in hex
                push ax
                shr al,1
                shr al,1
                shr al,1
                shr al,1
                call print_x1
                pop ax
print_x1:       and al,0Fh              ; Print a hex digit
                cmp al,10
                jae print_x2
                  add al,'0'
                  jmp print_x3
print_x2:       add al,'A'-10
print_x3:       mov ah,2
                push dx
                mov dl,al
                int 21h
                pop dx
                ret
print_x         endp

idebugc  macro chr
        push ax
        mov al,chr
        call print_cx
        pop ax
endm

idebugs  macro wrd
        push ax
        mov ax,wrd
        call print_sx
        pop ax
endm

idebugl  macro high,low
        push ax
        push dx
        mov ax,low
        mov dx,high
        call print_lx
        mov ah,2
        mov dl,' '
        int 21h
        pop ax
        pop dx
endm

else

idebugc  macro chr
endm

idebugs  macro wrd
endm

idebugl  macro high,low
endm

endif ; DEBUGINIT

  
;**************************************************************************
;
;                       INIT command
;
; Init command does the following:
;  - displays sign-on message
;  - checks DOS version. This driver is built in a way that requires
;    at least dos version 2.00. I'm not sure whether even that is enough.
;  - determine which drive we are by default
;  - read command line
;    - abort on syntax errors
;  - initialize multiplex interrupt
;  - do hooks to other SRDISK drivers (specified in command line)
;  - hook INT 19 bootstrap interrupt
;  - fills in the request header
;**************************************************************************
  
cmd_init        proc near
                ; Compulsory initializations
                mov word ptr conf.c_disk_IO_seg,cs

                ; Sign on message
                mov dx,offset s_sign_on         ; "ReSizeable RAMdisk ver..."
                prints

                ; Find out the features about DOS and the drive letter
                call init_dos
                jnc cmd_init4
                  jmp cmd_init_err
cmd_init4:
                les si,req_ptr

                idebugl es,si
                idebugs <word ptr es:[si]>
                idebugs <word ptr es:[si+2]>
                idebugs <word ptr es:[si+16h]>

                mov al,dos_drive                ; Default install to drive
                mov def_drive,al                ; .. reported by DOS

                ; If DOS has no support for 32 bit sector access, disable it
                test flags,FLAG_32BITS
                jnz cmd_init2
                  and conf.c_flags,NOT C_32BITSEC
                  ;and drhdr_attr,NOT DHATTR_32BIT
cmd_init2:
                ; If DOS has no support for GIOCTL (diskcopy) disable it
                test flags,FLAG_GIOCTL
                jnz cmd_init5
                  and drhdr_attr,NOT DHATTR_GIOCTL
cmd_init5:
                call init_read_cmdline
                jc cmd_init_err

                call init_memory
                jc cmd_init_err

                call init_mplex
                jc cmd_init_err

                call init_hooks
                jc cmd_init_err
if HOOKINT19
                call set_int19
endif

                mov al,def_drive
                mov conf.c_drive,al

                ; --------------- If appended
                test flags,FLAG_APPENDED
                jz cmd_init7
                  mov s_appdrive,al     ; Report append
                  mov dx,offset s_appended
                  prints

                  mov bx,offset ret_far ; Strategy and commands short
                  mov drhdr_strategy,bx
                  mov drhdr_commands,bx
                  mov drhdr_attr,DHATTR_CHARDEV
                  mov drhdr_units,'$'   ; Name for this driver '$SRD'MEMORY

                  lds bx,req_ptr
                  assume ds:nothing
		  mov [bx].irUnits,1
		  mov word ptr [bx].irEndAddress,appended_eor
                  mov install_ok,TRUE
                  jmp cmd_init3
                assume ds:d_seg
cmd_init_err:   ; --------------- Error
                prints
cmd_init_abort:
                call deinit                     ; Remove hooks
                xor ax,ax
                lds bx,req_ptr
                assume ds:nothing
		mov [bx].irUnits,al		; Zero the number of drives
                mov word ptr [bx].irEndAddress,ax
                jmp cmd_init3
                assume ds:d_seg

cmd_init7:      ; --------------- Normal install
                mov al,def_drive
                mov s_drive,al
                cmp al,'$'                      ; Is the drive number known?
                jne cmd_init8
                  mov word ptr s_drive,2020h    ; Don't tell drive
cmd_init8:
                mov dx,offset s_installed       ; Report install
                prints
                mov install_ok,TRUE

                lds bx,req_ptr
                assume ds:nothing
		mov [bx].irUnits,1		; Save number of drives
                mov word ptr [bx].irEndAddress,end_of_resident
if CAPABLE and C_GIOCTL
                test flags,FLAG_GIOCTL
                jnz cmd_init3
                  mov word ptr [bx].irEndAddress,noGIOCTL_eor
                  mov high_cmd,16
endif
cmd_init3:      ; --------------- Return (whether failed or not)
		mov word ptr [bx].irEndAddress+2,cs

		mov word ptr [bx].irParamAddress,offset pBPB
		mov word ptr [bx].irParamAddress+2,cs

                ; --------------- After install operations
if CAPABLE and C_DISK_IMAGE
                cmp install_ok,TRUE
                jne cmd_init6
                  call load_image
cmd_init6:
endif
                jmp cmd_ok

                assume ds:d_seg
cmd_init        endp

;**************************************************************************
;
;               CHECK DOS VERSION AND CAPABILITIES
;
;**************************************************************************

init_dos        proc near
                mov al,'$'
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

                  or flags,FLAG_32BITS or FLAG_GIOCTL or FLAG_KNOWDRIVE
                  les si,req_ptr
                  mov al,es:[si].irDriveNumber
                  add al,'A'
                  mov dos_drive,al
                  jmp idos_x

idos_notc:      mov ah,30h
                int 21h         ; Get DOS version number

                xchg ah,al
                idebugs ax
                cmp ax,300h     ; Multiplex supported only after version 3.00
                jnb idos2
; Let's not be picky
;                cmp ax,700h
;                jb idos2
idos1:
                  mov dx,offset errs_eDOS       ; Invalid DOS version
                  stc
                  ret
idos2:
                cmp ax,314      ; DOS 3.20+ ?
                jb idos3
                  or flags,FLAG_GIOCTL
idos3:
                cmp ax,31Fh     ; DOS 3.31+ ?
                jb idos4
                  or flags,FLAG_32BITS
idos4:
                les si,req_ptr
                cmp byte ptr es:[si],16h        ; Device number supported?
                jbe idos_x                      ; No, make a guess
                  les si,req_ptr
                  mov al,es:[si].irDriveNumber
                  add al,'A'
                  mov dos_drive,al
                  or flags,FLAG_KNOWDRIVE
idos_x:

;; !!!! The following does not work since the drhdr_next is not valid yet.
;
;                cmp dos_drive,'C'               ; Is invalid drive?
;                jb idosx1
;                cmp dos_drive,'Z'
;                jbe idosx2
;idosx1:
;                  les si,drhdr_next             ; Walk links up to count
;                  mov al,'A'                    ; block devices
;cntblk1:
;                  cmp si,0FFFFh
;                  je cntblk3
;                    test es:[si].dhAttributes,DHATTR_CHARDEV
;                    jnz cntblk2
;                      add al,es:[si].dhNameOrUnits
;cntblk2:
;                    les si,es:[si].dhLink
;                    jmp cntblk1
;cntblk3:
;                  mov dos_drive,al
;                  or flags,FLAG_KNOWDRIVE

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

                cmp al,'/'
                jz irc_switch

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
                test flags,FLAG_FORCEDRIVE
                jnz irc_syntax
                or flags,FLAG_FORCEDRIVE
                jmp irc_narg

irc_syntax:     mov dx,offset errs_syntax
                stc
                pop ds
                ret

irc_switch:     lodsb
                and al,11011111b                ; Make lowercase to uppercase
                cmp al,'A'
                jne irc_syntax

                or flags,FLAG_APPEND
                jmp irc_narg

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
;                       Memory initialization
;
; Returns
;   carry set if error
;**************************************************************************
  
init_memory     proc near
                mac_init_memory
                ret
init_memory     endp

;**************************************************************************
;
;               Multiplex service initialization
;
; Queries multiplex interrupt to find out if SRDISK device drivers are
; already installed. If not  install the multiplex server.
;
; Return carry set if error.
;**************************************************************************

init_mplex      proc near
                push ds
                push es
                mov ax,MULTIPLEXAH * 100h
                xor bx,bx
                xor cx,cx
                xor dx,dx
                push ds
                int 2Fh         ; AL installed status
                pop ds

                cmp al,-1       ; Is something installed?
                je im_installed
                cmp al,0        ; Is it OK to install?
                je im_install

im_used:        ; Garbled return
                mov dx,offset errs_ml_used
im_err:         stc
                jmp imx

im_installed:   mov ax,MULTIPLEXAH * 100h + 1
                push ds
                int 2Fh         ; ES segmet of main SRDISK driver
                pop ds

                cmp word ptr es:dr_ID,'RS'      ; Is it SRDISK structure?
                jne im_used                     ; No, multiplex used elsewhere
                cmp byte ptr es:dr_ID+2,'D'
                jne im_used                     ; No, multiplex used elsewhere
                mov dx,offset errs_ml_version
                cmp byte ptr es:dr_v_format,V_FORMAT ; Proper version?
                jne im_err              ; No
                ; OK
                mov di,es:dr_conf
                mov word ptr main_config,di
                mov word ptr main_config+2,es
                jmp im_end

im_install:     mov word ptr main_config,offset conf
                mov word ptr main_config+2,ds
                or flags,FLAG_1ST

                mov ax,352Fh
                int 21h
                mov word ptr old_multiplex,bx
                mov word ptr old_multiplex+2,es

                mov dx,offset multiplex
                mov ax,252Fh
                int 21h

im_end:         clc
imx:            pop es
                pop ds
                ret
init_mplex      endp


;**************************************************************************
;
;               INIT HOOKS to previous SRDISK drivers
;
; Append this driver into the list of installed SRDISK drivers
; Return carry set if error
;**************************************************************************

init_hooks      proc near
                test flags,FLAG_1ST     ; If we are the first driver
                jnz ihxok               ;  no hooks are to be done

                les di,main_config      ; es:di point to a drive config
                mov al,def_drive        ; al is the drive to search
                cmp al,'$'              ; Is drive letter unknown?
                je ih_nodrive           ; Yes, do not check drive letter

                test flags,FLAG_APPEND          ; If we append
                jz ih_find_drive
                test flags,FLAG_FORCEDRIVE      ; but not specify drive
                jnz ih_find_drive
ih_nodrive:       mov al,-1                     ; make sure drive not found
ih_find_drive:
ih1:            cmp es:[di].c_drive,al          ; Is it the same drive?
                je ih_append
ih2:            test word ptr es:[di].c_next_drive,-1 ; Is there next drive
                jz ih_newdrive                  ; No (valid segment is nonzero)
                mov es,es:[di].c_next_drive     ; Yes, find the next drive
                mov di,es:dr_conf
                jmp ih1

ih_append_new:  ; Append this driver into previously installed drive
                mov al,es:[di].c_drive          ; Find the drive letter
                mov def_drive,al
                mov conf.c_drive,al

ih_append:      ; Append this driver into specified drive
                test es:[di].c_flags,C_APPENDED ; Append allowed?
                jz ih_appendfail                ; No, fail

                test word ptr es:[di].c_next,-1 ; Is there next driver
                jz ih_a1                        ; No, append here
                mov es,es:[di].c_next           ; Yes, find the next drive
                mov di,es:dr_conf
                jmp ih_append

ih_appendfail:  mov al,dos_drive
                mov def_drive,al
                mov dx,offset errs_noappend
                stc
                ret

ihxok:          clc
ihx:            ret

ih_a1:          mov es:[di].c_next,ds
                or flags,FLAG_APPENDED  ; Remember to free extra memory
if DEBUGGING
                mov ax,es:d_col         ; Debug data display little left from
                sub ax,16               ;  main data display
                mov d_col,ax
endif ;DEBUGGING
                jmp ihxok

ih_newdrive:    test flags,FLAG_APPEND
                jnz ih_append_new
                ; This driver must be placed at the tail of list of
                ; SRDISK drivers
                mov es:[di].c_next_drive,ds

                jmp ihxok
init_hooks      endp


;**************************************************************************
;
;                       INT 19 hooking
;
; INT 19 is the bootstrap loader interrupt, which is invoked when user
; presses Ctrl-Alt-Del. We must hook it in order to release the
; extended memory allocated for RAM disk.
;**************************************************************************
  
if HOOKINT19

set_int19       proc near
		push ax
		push dx
		push bx
		push es

                mov ax,3519h
                int 21h                         ; Get old int 19 handler
                mov old_int19_off,bx
                mov old_int19_seg,es
                mov dx,offset int_19_entry
		mov ax,2519h
                int 21h                         ; Set new int 19 handler

		pop es
		pop bx
		pop dx
		pop ax
		retn
set_int19       endp
  
endif

;**************************************************************************
;
;               Deinitialization in case of aborted install
;
;**************************************************************************

deinit          proc near
if HOOKINT19
                mov ax,old_int19_seg
                or ax,old_int19_off
                jz di_noint19

                push ds
                mov dx,old_int19_off
                mov ds,old_int19_seg
                mov ax,2519h
                int 21h                         ; Set old int 19 handler
                pop ds
di_noint19:
endif

                mac_deinit_memory

                mov ax,word ptr old_multiplex
                or ax,word ptr old_multiplex+2
                jz no_mplex

                push ds
                mov dx,word ptr old_multiplex
                mov ds,word ptr old_multiplex+2
                mov ax,252Fh
                int 21h                         ; Set old multiplex handler
                pop ds
no_mplex:

                ret
deinit          endp


;**************************************************************************
;
;               Loading of included disk image
;
;**************************************************************************

if CAPABLE and C_DISK_IMAGE
load_image      proc near
                test flags,FLAG_APPENDED
                jz li1
                ; If appended, must copy parameters to the first driver

li1:
                ; !!!! Allocate memory
                ; !!!! Move data to disk
                mov dx,offset errs_load_fail
                prints
                ret
load_image      endp

errs_load_fail  db 'RAMDisk: Loaded disk image', 0Dh, 0Ah, '$'
s_image_loaded  db 'RAMDisk: Loaded disk image', 0Dh, 0Ah, '$'

endif


;**************************************************************************
;
;                       Initialization strings
;
;**************************************************************************

mac_init_data

errs_eDOS       db 'RAMDisk: Incorrect DOS version.'
                db 0Dh, 0Ah, '$'

errs_ml_used    db 'RAMDisk: Multiplex interrupt already in use.'
                db 0Dh, 0Ah, '$'

errs_ml_version db 'RAMDisk: Driver of different version already '
                db 'installed.'
                db 0Dh, 0Ah, '$'

errs_noappend   db 'RAMDisk: Can not append to previously installed driver.'
                db 0Dh, 0Ah, '$'

errs_syntax     db 'RAMDisk: Syntax error', 0Dh, 0Ah, 0Dh, 0Ah
                db 'Syntax: SRDISK.SYS [d:] [/A]', 0Dh, 0Ah, 0Dh, 0Ah
                db ' d:', 9, 'Drive into which to append or the drive letter'
                db 0Dh, 0Ah
                db 9, 'of this device if DOS does not report it to it '
                db 'correctly.'
                db 0Dh, 0Ah
                db ' /A', 9, 'Append this driver to previous SRDISK '
                db 'driver.'
                db 0Dh, 0Ah, '$'

s_sign_on       db 0Dh, 0Ah, 'ReSizeable RAMDisk '
                db '(', MEMORY_STR, ')'
                db ' version ', SRD_VERSION, '. '
                db 'Copyright (c) 2005 Marko Kohtala.'
                db 0Dh, 0Ah, '$'

s_installed     db 'Installed RAMDrive '
s_drive         db 'C:', 0Dh, 0Ah, '$'
  
s_appended      db 'Appended to RAMDrive '
s_appdrive      db 'C:', 0Dh, 0Ah, '$'



;**************************************************************************
;
;                       A note for binary debuggers
;
;**************************************************************************

db 0Dh, 0Ah, "Copyright (C) 1992-1996,2005 Marko Kohtala. "
db 0Dh, 0Ah, "Contact at 'kohtala@users.sourceforge.net'."
db 0Dh, 0Ah

d_seg           ends
                end
