; This is meant to be compiled into a .EXE file which can be used as a device
; Copyright (C) 1996, 2005 Marko Kohtala
; Released under GNU GPL, read the file 'COPYING' for more information

; Use commands: "tasm xmssize" and "tlink xmssize". No options necessary.

                .186

_TEXT SEGMENT PARA 'CODE'
                assume ds:nothing, es:nothing, ss:nothing, cs:_TEXT
                ; Device driver header
                dd 0FFFFh       ; next
                dw 8000h        ; character device
                dw offset strategy
                dw offset interrupt
                db 'XMSSIZE '

request_ptr     dd 0
intro_txt       db 'XMSSIZE 1.00 Copyright (c) 1996 Marko Kohtala',13,10
                db 'This program modifies XMS handle table to support more than 64M of RAM.',13,10
                db 'Use at your own risk.',13,10,'$'
usage_txt       db 13,10,'Usage: XMSSIZE <Kbytes>',13,10
                db '<Kbytes> is the total amount of memory, not just XMS.',13,10,'$'
cpu_txt         db 13,10,'This program works only on 386 or better.',13,10,'$'
noXMS_txt       db 13,10,'XMS driver not present or too old.',13,10,'$'
lastUsed_txt    db 13,10,'Operation is too difficult.'
                db ' Do it right after loading HIMEM.SYS',13,10,'$'
tooLittle_txt   db 13,10,'Can not give that little memory.',13,10,'$'
done_txt        db 13,10,'Done',13,10,'$'

ifdef DEBUG
intstart_txt    db 'START $'
toinit_txt    db 'TOINIT $'
init_txt    db 'INIT $'
locarg_txt    db 'LOCARG $'
exit_txt    db 'EXIT $'
tocmd_txt    db 'TOCMD $'
cmd_txt    db 'CMD $'
debugm macro l
        push dx
        push ds
        push ax
        push cs
        pop ds
        mov dx,offset l
        mov ah,9
        int 21h
        pop ax
        pop ds
        pop dx
endm
else
debugm macro l
endm
endif

strategy:       mov word ptr cs:request_ptr,bx
                mov word ptr cs:request_ptr+2,es
                retf

interrupt:      pushf
                push es
                push ds
                push di
                push si
                push dx
                push cx
                push bx
                push ax
                debugm intstart_txt
                lds di,cs:request_ptr
                mov [di+3],8103h        ; error, done, unknown command...
                mov word ptr [di+14],0  ; free this driver
                mov [di+16],cs
                cmp byte ptr [di+2],0   ; is it init?
                jnz int_exit

                debugm toinit_txt
                call init               ; banner, check CPU
                jc int_error

                debugm locarg_txt
                lds si,[di+18]          ; locate arguments
ifdef DEBUG
                mov dx,si
                push [si+4]
                mov byte ptr [si+4],'$'
                mov ah,9
                int 21h
                pop [si+4]
endif
                mov dx,offset usage_txt
                mov cx,80

next_sys_char:  lodsb                   ; must skip the driver name
                cmp al,13
                je int_error
                cmp al,10
                je int_error
                cmp al,' '
                loopne next_sys_char
                jne int_error

                debugm tocmd_txt
ifdef DEBUG
                mov dx,si
                push [si+4]
                mov byte ptr [si+4],'$'
                mov ah,9
                int 21h
                pop [si+4]
endif
                .386
                push eax
                push ebx
                push esi
                call cmdline            ; parse arguments
                pop esi
                pop ebx
                pop eax
                .186
int_error:
                push cs
                pop ds
                mov ah,9
                int 21h
int_exit:
                debugm exit_txt
                pop ax
                pop bx
                pop cx
                pop dx
                pop si
                pop di
                pop ds
                pop es
                popf
                retf

                ;; EXE entry point

main:
                call init
                jc exe_error
                mov cl,ds:[80h]
                xor ch,ch
                mov si,81h
                call cmdline
exe_error:
                pushf
                push cs
                pop ds
                mov ah,9
                int 21h
                mov ax,4C00h
                popf
                adc al,0
                int 21h
                ; This should not be reached
                int 20h
                ; Surely we are dead already...
                ret

                ;; FROM THIS ON, IT IS COMMON TO DEVICE AND EXECUTABLE

                ;; init preserves ds, whatever it is, and returns with
                ;; dx offsetting the error text in cs if carry is set
                ;; or happily with success if carry is not set
init proc
                mov dx,offset intro_txt
                push ds
                push cs
                pop ds
                mov ah,9
                int 21h
                pop ds

                pushf           ; Save these to be sure

; From CPUID by Intel
;
;       This procedure determines the type of CPU in a system
;       and sets the cpu_type variable with the appropriate
;       value.
;       All registers are used by this procedure, none are preserved.

;       Intel 8086 CPU check
;       Bits 12-15 of the FLAGS register are always set on the
;       8086 processor.
;
        pushf                   ; push original FLAGS
        pop     ax              ; get original FLAGS
        mov     cx, ax          ; save original FLAGS
        and     ax, 0fffh       ; clear bits 12-15 in FLAGS
        push    ax              ; save new FLAGS value on stack
        popf                    ; replace current FLAGS value
        pushf                   ; get new FLAGS
        pop     ax              ; store new FLAGS in AX
        and     ax, 0f000h      ; if bits 12-15 are set, then CPU
        cmp     ax, 0f000h      ;   is an 8086/8088
        je      cpu_error       ; jump if CPU is 8086/8088

;       Intel 286 CPU check
;       Bits 12-15 of the FLAGS register are always clear on the
;       Intel 286 processor in real-address mode.
;
        or      cx, 0f000h      ; try to set bits 12-15
        push    cx              ; save new FLAGS value on stack
        popf                    ; replace current FLAGS value
        pushf                   ; get new FLAGS
        pop     ax              ; store new FLAGS in AX
        and     ax, 0f000h      ; if bits 12-15 clear, CPU=80286
        jnz     cpu_ok          ; if no bits set, CPU is 80286
cpu_error:
        mov     dx,offset cpu_txt
        popf                    ; restore original flags
        stc
        ret

cpu_ok:
        popf            ; restore original flags
        clc
        ret
init endp

                ;; cmdline expects to have ds:si pointing to argument string
                ;; and cx telling the argument string length
                ;; Returns with dx pointing to error string in cs.
                ;; Carry is set if there really was an error.
cmdline proc
                .386
                xor ebx,ebx
                cld
                jcxz show_usage
                xor eax,eax     ; clear for easier addition to ebx
nextspace:      lodsb
                cmp al,' '
                jne nextdigit
                loop nextspace
                jmp show_usage
nextdigit:
                cmp al,13
                jz end_of_line
                cmp al,10
                jz end_of_line
                cmp al,'9'
                ja show_usage
                sub al,'0'
                jb show_usage
                imul ebx,10
                add ebx,eax
                lodsb
                loop nextdigit
end_of_line:
                or ebx,ebx
                jnz doit
show_usage:
                mov dx,offset usage_txt
                stc
                ret
doit:
                push ebx
                mov ax,4309h            ; get handle table pointer
                int 2Fh
                cmp al,43h
                jnz noXMS_error
                mov dl,es:[bx+1]
                xor dh,dh               ; dx is the structure size
                mov cx,es:[bx+2]        ; cx is the table size
                jcxz noXMS_error
                lds bx,es:[bx+4]        ; ds:bx points to structure table
                xor esi,esi     ; esi found top of memory
                xor di,di       ; di best top of memory handle
checkhandle:
                mov al,[bx]    ; 1 = free, 2 = used, 4 = ignored
                cmp al,4
                jz nexthandle
                cmp al,1
                jz goodhandle
                cmp al,2
                jz goodhandle
noXMS_error:
                pop ebx
                mov dx,offset noXMS_txt
error_ret:
                stc
                ret

goodhandle:     mov eax,[bx+2]
                add eax,[bx+6]         ; eax is now the top of the block
                cmp eax,esi
                jb nexthandle
                mov di,bx
                mov esi,eax
nexthandle:     add bx,dx
                loop checkhandle

                pop eax
                mov dx,offset lastUsed_txt
                cmp word ptr [di],1
                jne error_ret

                mov dx,offset tooLittle_txt
                sub eax,[di+2]
                jbe error_ret
                mov [di+6],eax
                mov dx,offset done_txt
                clc
                ret
                .186
cmdline endp
_TEXT ENDS

_STACK   SEGMENT STACK 'CODE'
                db 200 dup (?)
_STACK ENDS
                end main

