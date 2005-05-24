; SRDISK - boot sector
; Copyright (c) 1996, 2005 Marko Kohtala
; Released under GNU GPL, read the file 'COPYING' for more information

; Do
;   tasm /t boot.asm
;   tlink /t boot.obj,boot.bin
; and add the resulting bytes to writenew.c, not forgetting the ending 0

code segment
        assume cs:code
        org 0h
        jmp begin

        org 3Eh
begin:
        xor ax,ax
        mov ds,ax
        mov si,7C00h+offset msg
        cld
        lodsb
printnext:
        mov bx,7
        mov ah,0eh
        int 10h
        lodsb
        or al,al
        jnz printnext
endprint:
        xor ax,ax
        int 16h
        mov ax,0e0dh
        int 10h
        mov al,0ah
        int 10h
        int 19h
hangup:
        jmp hangup
msg:    db 13,10,'Non-System disk, remove or replace and press key',0
code ends
end
