
;/////////////////////////////////////////////////////////////////////
;// Copyright (C) 2008 Department of Computer SCience & Engineering
;// National Institute of Technology - Warangal
;// Andhra Pradesh 
;// INDIA
;// http://www.nitw.ac.in
;//
;// This software is developed for software based DSM system Project
;// This is an experimental platform, freely useable and modifiable
;// 
;//
;// Team Members:
;// Prof. T. Ramesh 			Chapram Sudhakar
;// A. Sundara Ramesh  			T. Santhosi
;// K. Suresh 				G. Konda Reddy
;// Vivek Kumar 				T. Vijaya Laxmi
;// Angad 				Jhahnavi
;//////////////////////////////////////////////////////////////////////

;---------------------------------------------------------
; Switch to protected mode. Transfer control to kernel.
;---------------------------------------------------------

	BITS	16

	SECTION .text

	org	0x400

pmswitch:
	xor	edx,edx
	mov	dx,cs
	mov	ds,dx		; for using it later.
	mov	es,dx
	shl	edx,4		; Linear 32 bit address.

	sti

				; Display system booting message.
	mov	ax,boot_mesg
	push	ax
	call	display_mesg
	pop	ax

				; Find the linear 32 bit address
				; of gdt_table (base address).
	mov	eax,edx
	xor	ebx,ebx
	mov	bx,gdt_table
	add	eax,ebx
	mov	[gdt_desc+2],eax

	mov	al,0x80		; Disable NMI interrupt.
	out	0x70,al
	cli			; Stop interrupts until system boots.

				; Mask all IRQs
	mov	al,0xff
	out	0xa1,al
	in	al,0x61		; delay

	mov	al,0xff
	out	0x21,al
	in	al,0x61		; delay


	mov	si,gdt_desc
 	lgdt 	[cs:si]
				; (GDT is pre-initialised with minimum 
				; required fixed values).

				; Enable a20 line.
	call 	empty_8042
	mov	al,0xd1
	out	0x64,al
	call	empty_8042
	mov	al,0xdf
	out	0x60,al
	call	empty_8042

	in	al,0x92		; Fast a20 version.
	or	al,0x02
	out	0x92,al
	
				; Wait until a20 is really enabled.
	xor	ax,ax
	mov	fs,ax
	dec	ax
	mov	gs,ax
a20_wait:
	inc	ax
	mov	[fs:0x200],ax
	cmp	[gs:0x210],ax
	je	a20_wait


				; Reset coprocessor
	xor	ax,ax
	out	0xf0,al
	out	0x80,al

	out	0xf1,al
	out	0x80,al


	mov 	ebx,cr0		; Set protected mode flag.
	or 	ebx,0x01
	mov	cr0,ebx

	jmp	clear_queue

clear_queue:
	db	0x66,0xea
	dd	0x00010800 	; Starting of 32-bit kernel
	dw	0x0008		; Code segment selector
	
				; Empties the 8042 control input buffer.
empty_8042:
	push	ecx
	mov	ecx,100000

empty_8042_loop:
	dec	ecx
	jz	empty_8042_end_loop

	in	al,0x61
	in	al,0x64
	test	al,0x01
	jz	no_output
	
	in	al,0x61 ; for delay
	in	al,0x60
	jmp	empty_8042_loop

no_output:
	test	al,0x02
	jnz	empty_8042_loop
empty_8042_end_loop:
	pop	ecx
	ret

				; Displays a message in real mode only.
display_mesg:
	push bp
	mov bp,sp
	push si
	push ax
	push bx
	
	mov si,[ss:bp+4]	; String address.
display_mesg1:                  ; Find the length of the string.
	cmp byte[cs: si],0	; Last character of the string ?
	je display_mesg2
	mov ah,0x0e		; Display character in teletype.
	mov al,[cs: si]
	mov bx,0x0007
	int 0x10
	inc si
	jmp display_mesg1
display_mesg2:
	pop bx
	pop ax
	pop si
	pop bp
	ret

boot_mesg:
	db 0x0d, 0x0a, 'Booting the system ...',0

	align	8
gdt_table:
	dd 0x00000000		; Null
	dd 0x00000000
	dd 0x0000ffff		; Code segment
	dd 0x00cf9a00
	dd 0x0000ffff		; Data segment
	dd 0x00cf9200

gdt_desc:
	dw (3 * 8) - 1
	dd gdt_table		; Should be reloaded in the program.
	dw 0


