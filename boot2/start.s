; ---------------------------------------------------------------------------------------
; Copyright (c) 2026 luke8086
; Distributed under the terms of GPL-2 License
; ---------------------------------------------------------------------------------------
; File: start.s - Stage 2 bootloader asm code
; ---------------------------------------------------------------------------------------

[cpu 8086]

extern _cmain

section _TEXT class=CODE

resb 0x100
..start:
    jmp _cmain


global _intr
_intr:
    push bp
    mov bp, sp

    push si
    push di
    push bp
    push ds
    push es

    mov ax, [bp+4]
    mov byte [intr_int+1], al

    mov si, [bp+6]

    push word [si+16]
    push word [si+18]

    mov ax, [si+0]
    mov bx, [si+2]
    mov cx, [si+4]
    mov dx, [si+6]
    mov bp, [si+8]
    mov di, [si+10]
    mov si, [si+12]

    pop es
    pop ds

intr_int:
    int 0

    mov [cs:intr_saved_ds], ds
    mov [cs:intr_saved_es], es

    pop es
    pop ds
    pop bp

    mov si, [bp+6]
    mov [si+0], ax
    mov [si+2], bx
    mov [si+4], cx
    mov [si+6], dx

    pushf
    pop word [si+14]

    mov ax, [cs:intr_saved_ds]
    mov [si+16], ax
    mov ax, [cs:intr_saved_es]
    mov [si+18], ax

    pop di
    pop si

    pop bp
    ret

intr_saved_ds: dw 0
intr_saved_es: dw 0


global _start_kernel
_start_kernel:
    cli

    mov ax, 0x1000
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0xffff

    ; Clear area used to detect running under DOS
    mov word [es:0], 0

    sti
    jmp 0x1000:0x100


global _halt
_halt:
    cli
    hlt
    jmp _halt


BDA_SEGMENT             equ 0x0040
BDA_MOTOR_STATUS        equ 0x3f   ; bits 0-3: motors running
BDA_MOTOR_TIMEOUT       equ 0x40
FDC_PORT_DOR            equ 0x3f2
FDC_DOR_MOTORS_OFF      equ 0x0c   ; DMA/IRQ on, not in reset, motors off

global _stop_floppy_motor
_stop_floppy_motor:
    push ax
    push dx
    push es

    mov dx, FDC_PORT_DOR
    mov al, FDC_DOR_MOTORS_OFF
    out dx, al

    mov ax, BDA_SEGMENT
    mov es, ax
    and byte [es:BDA_MOTOR_STATUS], 0xf0
    mov byte [es:BDA_MOTOR_TIMEOUT], 0

    pop es
    pop dx
    pop ax
    ret


section _DATA class=DATA
section _DATAEND class=DATAEND
section _BSS class=BSS
section _BSSEND class=BSSEND

group DGROUP _TEXT _DATA _DATAEND _BSS _BSSEND
