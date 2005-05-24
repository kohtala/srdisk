; SRDISK - EMS memory device driver features control file
; Copyright (C) 1993-1996, 2005 Marko Kohtala
; Released under GNU GPL, read the file 'COPYING' for more information

; This is the file to compile.

; EMS 3.2

MEMORY_STR  equ 'EMS 3.2' ; Define memory type string of any length
STACKSIZE equ 260       ; Local stack size

include define.inc

; What this driver can actually do:
CAPABLE         equ C_32BITSEC or C_NOALLOC or C_APPENDED or C_GIOCTL

HOOKINT19       equ     FALSE           ; Hook int 19 if TRUE

include srdems3.inc
include srdisk.inc      ; Create the code itself

