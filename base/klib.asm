
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
global	initialize_8259s
global	enable_timer
global	do_context_switch
global	iret_test
global	reset_busy_bit
global	initialize_paging
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


;
; unsigned short get_irqstatus()
;
get_irqstatus:
	pushfd
	cli
	in	al,INT2_CTLMASK
	mov	ah,al
	in	al,INT_CTLMASK
	popfd
	ret
	

;*==========================================================================* 
;*				enable_irq				    * 
;*========================================================================== */
; void enable_irq(unsigned irq) 
; Enable an interrupt request line by clearing an 8259 bit. 
; Equivalent code for irq < 8: 
;	out_byte(INT_CTLMASK, in_byte(INT_CTLMASK) & ~(1 << irq)); 

enable_irq:
	push	ecx
	mov	ecx, [ss:8+esp]		; irq 
	pushfd
	cli
	mov	ah, ~1
	rol	ah, cl			; ah = ~(1 << (irq % 8)) 
	cmp	cl, 8
	jae	enable_8		; enable irq >= 8 at the slave 8259 
enable_0:
	in	al,INT_CTLMASK
	and	al, ah
	out	INT_CTLMASK,al		; clear bit at master 8259 
	popfd
	pop	ecx
	ret
enable_8:
	in	al,INT2_CTLMASK
	and	al, ah
	out	INT2_CTLMASK,al		; clear bit at slave 8259 
	popfd
	pop	ecx
	ret


;*==========================================================================* 
;*				disable_irq				    * 
;*========================================================================== */
; int disable_irq(unsigned irq) 
; Disable an interrupt request line by setting an 8259 bit. 
; Equivalent code for irq < 8: 
;	out_byte(INT_CTLMASK, in_byte(INT_CTLMASK) | (1 << irq)); 
; Returns true iff the interrupt was not already disabled. 

disable_irq:
	push	ecx
	mov	ecx, [ss:8+esp]		; irq 
	pushfd
	cli
	mov	ah, 1
	rol	ah, cl			; ah = (1 << (irq % 8)) 
	cmp	cl, 8
	jae	disable_8		; disable irq >= 8 at the slave 8259 
disable_0:
	in	al,INT_CTLMASK
	test	al, ah
	jnz	dis_already		; already disabled? 
	or	al, ah
	out	INT_CTLMASK,al		; set bit at master 8259 
	popfd
	pop	ecx
	mov	eax, 1			; disabled by this function
	ret
disable_8:
	in	al,INT2_CTLMASK
	test	al, ah
	jnz	dis_already		; already disabled?
	or	al, ah
	out	INT2_CTLMASK,al		; set bit at slave 8259
	popfd
	pop	ecx
	mov	eax, 1			; disabled by this function
	ret
dis_already:
	popfd
	pop	ecx
	xor	eax, eax		; already disabled
	ret

;*************************************************************************
;		test_and_set
;*************************************************************************
;  int test_and_set(unsigned short int * var);
;  Atomic test_and_set instruction implemetation.

test_and_set:		
	  push ebp
	  mov ebp,esp
	  mov eax,[ss:ebp+8]
	  lock bts word [ds: eax],0
	  jnc test_and_set1
	  mov eax,1		; Previous 1 value.
	  jmp test_and_set2
test_and_set1:
	  mov eax,0
test_and_set2:
	  pop ebp
	  ret

;-----------------------------------------
; unsigned long get_eflags()
; get the current flag values
;-----------------------------------------
get_eflags:
	pushfd
	pop	eax
	ret

;------------------------------------
; void cli_intr()
; clear interrupts
;------------------------------------
cli_intr:
	cli
	ret

;------------------------------------
; void sti_intr()
;------------------------------------
sti_intr:
	sti
	ret

;------------------------------------------
; restore_eflags(unsigned long eflags)
; Restore the flags register.
;-------------------------------------------
restore_eflags:
	mov eax,[ss: esp+4]
	push eax
	popfd
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


set_timer_frequency:
	push	ecx
	; mov	cx,47727	; 25 times per sec
	mov	cx,23864	; Timer count to tick 50 times/sec.
	; mov	cx,11932	; Timer count to tick 100 times/sec.
	; mov	cx,0xffff	; Least frequency 18.2 times / sec
	mov	al,0x36
	out	0x43,al
	in	al,0x61
	mov	al,cl
	out	0x40,al
	in	al,0x61
	mov	al,ch
	out	0x40,al
	in	al,0x61
	pop	ecx
	ret

				; Initialse the both 8259 PICS 
				; (vector number assignement to IRQ's).
initialize_8259s:
	mov	al,0x11
	out	0x20,al		; ICW1 - select 8259a-1 init 
	out	0xa0,al
	mov	al,IRQ0_7
	out	0x21,al
	mov	al,IRQ8_F
	out	0xa1,al		; IRQ0-7 mapped to 0x20-0x27
	mov	al,0x04
	out	0x21,al		; Slave connected on IRQ2
	mov	al,0x2
	out	0xa1,al
	mov 	al,0x01
	out	0x21,al		; Normal EOI.
	out	0xa1,al

	mov 	al,0x00
	out	0x21,al		; unmask all irqs of 8259a-1
	out	0xa1,al		; unmask all irqs of 8259a-2
	ret


enable_timer:
	pushfd
	cli			; Enable timer interrupt.
	in	al,INT_CTLMASK
	and	al,~(1)
	out	INT_CTLMASK,al
	popfd
	ret

;------------------------------------------------------------------------------
; C-Language interface for do_context_switch(unsigned int newtaskdescriptor, unsigned int newcr0, unsigned int newcr3)
; Assumes already tss descriptor's busy bit is cleared.
;------------------------------------------------------------------------------

do_context_switch:
	push	ebp
	mov	ebp, esp
	push	eax
	push	ecx
	mov	eax, [ss: ebp + 4 + 4 + 4 + 4] ; //pgdir
	mov	cr3, eax
	mov	eax, [ss: ebp + 4 + 4 + 4] ;	// newcr0
	mov	cr0, eax

	mov	ecx, switch_addr	
	mov	eax, [ss: ebp + 4 + 4] ;	// Tss descriptor
	mov	[ds: ecx+4], ax
	
	jmp	dword far [ds: switch_addr]	; // Far address referring to tss
	pop	ecx
	pop	eax
	pop	ebp
	ret
	
	


;--------------------------------------------------------
; C- Language call void initialize_paging(Page_dir *pd);
;--------------------------------------------------------
initialize_paging:
	push	ebp
	mov	ebp, esp
	push	eax
	mov	eax, [ss: ebp+4+4]	; pd
	mov	cr3,eax	
	mov	eax, cr0		; Extented flags
	or	eax, 0x80010000		; Paging and Write Protect 
					; for supervisor
	mov	cr0, eax
	pop	eax
	pop	ebp
	ret

;-----------------------------------------------------------
; C-Language call unsigned long clear_paging(void);
; Only resetting of the paging related flags
;-----------------------------------------------------------
clear_paging:
	push	ebx
	mov	eax, cr0
	mov	ebx, eax
	and	ebx, 0x7ffeffff
	mov	cr0, ebx
	jmp	clear_paging1		; To clear the pipeline
clear_paging1:
	pop	ebx
	ret		; eax contains old cr0 value

;----------------------------------------------------------------------
; C-Language call void restore_paging(unsigned long old_cr0, unsigned long pgdir);
; Only setting of the paging related flags, 
; assumes page directory is already loaded in to cr3
;----------------------------------------------------------------------
restore_paging:
	mov	eax, [ss: esp+4+4] 	; Old cr3
	mov	cr3, eax
	mov	eax, [ss: esp+4] 	; Old cr0
	mov	cr0, eax
	jmp	restore_paging1		; To clear the pipeline
restore_paging1:
	ret

;---------------------------------------------------------------------
; C-Language interface call: unsigned long get_cr2(void);
; Called during page fault handling
;---------------------------------------------------------------------
get_cr2:
	mov	eax, cr2
	ret

get_cr0:
	mov	eax, cr0;
	ret

get_cr3:
	mov	eax, cr3
	ret

get_tr:
	str	eax
	ret
;---------------------------------------------------------------------
; mark_tsc(unsigned int *hi, unsigned int *lo): Marks time stamp counter global variables tschi, tsclo
; These are CPU time stamp counters, in the order of clock ticks.
;---------------------------------------------------------------------

mark_tsc:
	push	ebp
	mov	ebp, esp
	push	edx
	push	eax
	push	esi
	rdtsc
	mov	esi, [ss: ebp+4+4]
	mov	[ds: esi], edx   ; tschi
	mov	esi, [ss: ebp+4+4+4]
	mov	[ds: esi], eax ; tsclo
	pop	esi
	pop	eax
	pop	edx
	pop	ebp
	ret
;
;void math_state_restore(struct fpu_state *fps);
;
math_state_restore:
	push	ebp
	mov	ebp, esp
	push	esi
	mov	esi, [ss: ebp+4+4]
	frstor  [ds: esi]
	pop	esi
	pop	ebp
	ret

;
;void math_state_save(struct fpu_state *fps);
;
math_state_save:
	push	ebp
	mov	ebp, esp
	push	esi
	mov	esi, [ss: ebp+4+4]
	fnsave  [ds: esi]
	pop	esi
	pop	ebp
	ret


;*************************************************************************
;		pte_mark_busy
;*************************************************************************
;  int pte_mark_busy(unsigned long * pte);
;  Atomic pte_mark_busy instruction implemetation.

pte_mark_busy:		
	  push ebp
	  mov ebp,esp
	  mov eax,[ss: ebp+8]
	  lock bts dword [ds: eax],11	; 11th bit - part of available field
	  jnc pte_mark_busy1
	  mov eax,1		; Previous 1 value.
	  jmp pte_mark_busy2
pte_mark_busy1:
	  mov eax,0
pte_mark_busy2:
	  pop ebp
	  ret

;*************************************************************************
;		pte_unmark_busy
;*************************************************************************
;  int pte_unmark_busy(unsigned long * pte);
;  Atomic pte_unmark_busy instruction implemetation.

pte_unmark_busy:		
	  push ebp
	  mov ebp,esp
	  mov eax,[ss: ebp+8]
	  lock btr dword [ds: eax],11	; 11th bit - part available field
	  jnc pte_unmark_busy1
	  mov eax,1		; Previous 1 value.
	  jmp pte_unmark_busy2
pte_unmark_busy1:
	  mov eax,0
pte_unmark_busy2:
	  pop ebp
	  ret

;*************************************************************************
;		test_and_set_bitn
;*************************************************************************
;  int test_and_set_bitn(unsigned long * val, int bitn);
;  Atomic ptest_and_set_bitn instruction implemetation.

test_and_set_bitn:		
	push ebp
	mov ebp,esp
	push ebx
	mov eax, [ss: ebp+8]
	mov ebx, [ss: ebp+12]
	lock bts dword [ds: eax],ebx	; nth bit 
	jnc test_and_set_bitn1
	mov eax,1		; Previous 1 value.
	jmp test_and_set_bitn2
test_and_set_bitn1:
	mov eax,0
test_and_set_bitn2:
	pop ebx
	pop ebp
	ret

;*************************************************************************
;		clear_bitn
;*************************************************************************
;  int clear_bitn(unsigned long * val, int bitn);
;  Atomic clear_bitn instruction implemetation.

clear_bitn:		
	push ebp
	mov ebp,esp
	push ebx
	mov eax, [ss: ebp+8]
	mov ebx, [ss: ebp+12]
	lock btr dword [ds: eax],ebx	; nth bit 
	pop ebx
	pop ebp
	ret

