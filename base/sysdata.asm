
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

; This program  
;  1) finds the system data and places it in some proper place. Later
;     this data is collected by the kernel. (BIOS is used to get 
;     this information.)
;  2) Initialises the keyboard controller.

	BITS	16

	SECTION .text
	org	0x0
%include "../include/setup.inc"
%include "../include/mconst.inc"
stext:
	push	cs
	pop	ax
				; Setup the stack segment.
	cli
	mov	ss,ax
	mov	sp,endstack
	sti

	mov	ax,info_mesg	; Display information collection mesg.
	push 	ax
	call	display_mesg
	pop	ax

	mov	ax,CONFIG_DATA_SEGMENT	; Make configuration information data
	mov	ds,ax			; segment addressable.

	; Find usable RAM segments.
%define SMAP	0x534d4150
mem:
	mov 	di,MEM_SEGMENT_DESC
	push	ds
	pop	es
	mov	byte [ds: E820_FLAG],0
	mov 	ebx,0
E820:	
	dec	byte [cs: mdesc_count]
	jz	E820_1
	mov 	eax,0xe820
	mov 	ecx,20
	mov 	edx,SMAP
	int 	0x15
	jc 	E801	; Some error
	cmp 	eax,SMAP	
	jne 	E801
	; Returned some descriptor, update di to next descriptor.
	inc 	byte [ds: E820_FLAG]
	add 	di,20
	cmp 	ebx,0	; Is this last descriptor?
	jne 	E820
E820_1:
	jmp 	fdisk0
	; Otherwise go for old methods.	
E801:
	mov	ax,0xe801	; Returns total memory in dx: 64k's
	int	0x15		; and cx: 1k's
	jc	oldmemdetect
	
	and	edx,0x0000ffff
	shl	edx,6		; Convert to 1k's
	mov	[ds: MEMSIZE_ADDR],edx
	and	ecx,0x0000ffff
	add	[ds: MEMSIZE_ADDR],ecx
	jmp 	fdisk0

oldmemdetect:
	mov	ah,0x88		; Returns total memory in ax:1k's
	int	0x15
	and	eax,0x0000ffff
	mov	[ds: MEMSIZE_ADDR],eax


	; Find fdisk, hdisk information.
fdisk0:
	xor	ax,ax
	mov	ds,ax
	lds	si,[4 * 0x1e]	; BIOS - FD0 parameter table address.
	mov	ax,CONFIG_DATA_SEGMENT
	mov	es,ax
	mov	di,FD0_DATA_ADDR
	mov	cx,12		; 12 byte parameter table.
	cld 	
	rep movsb

hdisk0:
	xor	ax,ax
	mov	ds,ax
	lds	si,[4 * 0x41]	; BIOS - HD0 parameter table address
	mov	ax,CONFIG_DATA_SEGMENT
	mov 	es,ax
	mov	di,HD0_DATA_ADDR
	mov 	cx,0x10
	cld
	rep movsb


	; Initialise the keyboard repeat rate to maximum speed.
	mov	ax,0x0305
	xor	bx,bx
	int	0x16

	; Get current system time and save it. *****
get_time:
	mov	ax,CONFIG_DATA_SEGMENT
	mov	ds,ax
	mov	ah,0x02	; Get time.
	int	0x1a
	mov	al,ch	; Convert hours BCD to binary.
	call	bcd2binary
	mov	[ds: HOURS],al
	mov	al,cl	; Convert minutes BCD to binary.
	call	bcd2binary
	mov	[ds: MINUTES],al
	mov	al,dh	; Convert seconds BCD to binary.
	call	bcd2binary
	mov	[ds: SECONDS],al

	mov	ah,0x04	; Get date.
	int	0x1a
	mov	al,cl	; Convert year (last 2 digits) BCD to binary.
	call	bcd2binary
	mov	[ds: YEAR],al
	mov	al,dh	; Convert month BCD to binary.
	call	bcd2binary
	mov	[ds: MONTH],al
	mov	al,dl	; Convert day BCD to binary.
	call	bcd2binary
	mov	[ds: DATE],al

	mov	ax,end_info_mesg	; Display information collection mesg.
	push 	ax
	call	display_mesg
	pop	ax
	jmp	0x1000:0x0400


bcd2binary:	; AL contains the BCD number. returns binary in AL.
	push	dx
	mov	dl,10
	mov	dh,al
	shr	al,4
	and	al,0x0f
	mul	dl
	and	dh,0x0f
	add	al,dh
	pop	dx
	ret


display_mesg:
	push bp
	mov bp,sp
	push si
	push ax
	push bx
	
	mov si,[ss:bp+4]	; String address.
display_mesg1:                  ; Find the length of the string.
	cmp byte[cs: si],0		; Last character of the string ?
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

info_mesg 	db 0x0d,0x0a,'Getting system information ...',0
end_info_mesg 	db 0x0d,0x0a,'Getting system information is over...',0
mdesc_count 	db MAX_RAM_SEGMENTS
	
initstack:
	times 128 dd 0
endstack:
	dd 0

