
;////////////////////////////////////////////////////////////////////
;/ Copyright (C) 2008 Department of Computer SCience & Engineering
;/ National Institute of Technology - Warangal
;/ Andhra Pradesh 
;/ INDIA
;/ http://www.nitw.ac.in
;/
;/ This software is developed for software based DSM system Project
;/ This is an experimental platform, freely useable and modifiable
;/ 
;/
;/ Team Members:
;/ Prof. T. Ramesh 			Chapram Sudhakar
;/ A. Sundara Ramesh  			T. Santhosi
;/ K. Suresh 				G. Konda Reddy
;/ Vivek Kumar 				T. Vijaya Laxmi
;/ Angad 				Jhahnavi
;/////////////////////////////////////////////////////////////////////


;--------------------------------------------------------------
; This file contains a number of assembly code utility routines 
; needed by the  kernel.
; The routines only guarantee to preserve the registers the C 
; compiler expects to be preserved (ebx, esi, edi, ebp, esp, 
; segment registers, and direction bit in the flags). 
;--------------------------------------------------------------


	  BITS 32

	  SECTION .data
switch_addr:
	dd	0	; Offset
	dw	0	; Segment address

	  SECTION .text

%include "../include/mconst.inc"

global	in_byte	; read a byte from a port and return it 
global	in_word	; read a word from a port and return it 
global	out_byte	; write a byte to a port 
global	out_word	; write a word to a port 
global	get_irqstatus
global	enable_irq	; enable an irq at the 8259 controller 
global	disable_irq	; disable an irq 
global  test_and_set	; test_and_set implementation.
global	restore_eflags	; eflags are restored from the argument flags.
global	bzero		
global	bcopy
global	sti_intr
global	cli_intr
global	get_eflags
global	memset
global	memcmp

global	set_timer_frequency
global	init_8259s
global	enable_timer
global	do_context_switch
global	iret_test
global	reset_busy_bit
global	init_paging
global	clear_paging
global	restore_paging
global	get_cr3
global	get_cr2
global	get_cr0
global	get_tr
global	mark_tsc
global	math_state_save
global	math_state_restore
global	pte_mark_busy
global	pte_unmark_busy
global	clear_bitn
global	test_and_set_bitn

extern scheduler


;*===========================================================================* 
;*				in_byte					     * 
;*===========================================================================* 
; unsigned in_byte(port_t port); 
; Read an (unsigned) byte from the i/o port  port  and return it. 

in_byte:
	push	edx
	mov	edx,[ss: 8+esp]		; port 
	sub	eax, eax
	in	al,dx			; read 1 byte 
	pop	edx
	ret


;*===========================================================================* 
;*				in_word					     * 
;*===========================================================================* 
; unsigned in_word(port_t port); 
; Read an (unsigned) word from the i/o port  port  and return it. 

in_word:
	push	edx
	mov	edx,[ss: 8+esp]		; port 
	sub	eax, eax
    	in	ax,dx			; read 1 word 
	pop	edx
	ret


;*===========================================================================* 
;*				out_byte				     * 
;*===========================================================================* 
; void out_byte(port_t port, u8_t value); 
; Write  value  (cast to a byte)  to the I/O port  port. 

out_byte:
	push	edx
	mov	edx,[ss: 4+4+esp]		; port 
	mov	al, [ss:4+4+4+esp]		; value 
	out	dx,al			; output 1 byte 
	pop	edx
	ret


;*===========================================================================* 
;*				out_word				     * 
;*===========================================================================* 
; void out_word(Port_t port, U16_t value); 
; Write  value  (cast to a word)  to the I/O port  port. 

out_word:
	push	edx
	mov	edx, [ss:4+4+esp]		; port 
	mov	eax, [ss:4+4+4+esp]		; value 
    	out	dx,ax			; output 1 word 
	pop	edx
	ret


;----------------------------------------------------------
; bzero(const char *loc, int no_bytes)
; starting from loc, no_bytes are filled with Zero bytes .
;----------------------------------------------------------
bzero:
	push ebp
	mov ebp,esp
	push edi
	push ecx
	pushfd
	cld
	mov edi,[ss: ebp+8]
	mov ecx,[ss: ebp+12]
	cmp ecx, 0
	je bzero1
	mov al,0x0
	rep stosb
bzero1:
	mov eax,[ss: ebp+12]
	popfd
	pop ecx
	pop edi
	pop ebp
	ret


;----------------------------------------------------------
; memset(const char *loc, int char, int no_bytes)
; starting from loc, no_bytes are filled with Zero bytes .
;----------------------------------------------------------
memset:
	push ebp
	mov ebp,esp
	push edi
	push ecx
	pushfd
	cld
	mov edi,[ss: ebp+8]
	mov eax,[ss: ebp+12]
	mov ecx,[ss: ebp+16]
	cmp ecx, 0
	je memset1
	; mov al,0x0
	rep stosb
memset1:
	mov eax,[ss: ebp+16]
	popfd
	pop ecx
	pop edi
	pop ebp
	ret

;-----------------------------------------------------------
; memcmp(const char *src, const char *dest, int n);
;-----------------------------------------------------------
memcmp:
	push ebp
	mov ebp,esp
	push esi
	push edi
	push ecx
	push es
	push ds
	pop es
	pushfd
	cld
	mov esi,[ss: ebp+8]
	mov edi,[ss: ebp+12]
	mov ecx,[ss: ebp+16]
	cmp ecx, 0
	je memcmp_3
memcmp_1:
	cmpsb 
	jne memcmp_2
	dec	ecx
	jz memcmp_3
	jmp memcmp_1
memcmp_2:
	xor eax, eax
	mov al, [ds: esi-1]
	sub al, [es: edi-1]
	jmp memcmp_4

memcmp_3:
	xor eax, eax
memcmp_4:
	cbw
	cwde
	popfd
	pop es
	pop ecx
	pop edi
	pop esi
	pop ebp
	ret

;----------------------------------------------------------
; bcopy(const char *src, const char *dest, int no_bytes)
; copies no_bytes from src to dest. 
;----------------------------------------------------------
bcopy:
	push ebp
	mov ebp,esp
	push esi
	push edi
	push ecx
	push es
	push ds
	pop es
	pushfd
	cld
	mov esi,[ss: ebp+8]
	mov edi,[ss: ebp+12]
	mov ecx,[ss: ebp+16]
	cmp ecx, 0
	je bcopy1
	rep movsb
bcopy1:
	mov eax,[ss: ebp+16]
	popfd
	pop es
	pop ecx
	pop edi
	pop esi
	pop ebp
	ret

