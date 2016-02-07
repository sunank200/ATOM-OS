
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

	BITS	32

%include "../include/setup.inc"
%include "../include/mconst.inc"

	SECTION .data
extern	gdt_desc
extern	gdt_table
extern	initial_fpustate

extern	bss_start
extern	bss_end

	SECTION .text
global	_main:function

extern	initialize_gdt
extern	initialize_idt
extern	get_sys_data
extern	initialize_clock
extern	initialize_core_info
extern	initialize_tss_table
extern	initialize_kernel_pagetables
extern	initialize_buffer_cache
extern	initialize_floppy_request_bufs
extern	initialize_hash_table
extern	initialize_socktable
extern	initialize_recvpkts
extern	initialize_8259s
extern	initialize_systemprocess
extern	cmain
extern	debug_mesg

_main:
				; Initialze segment registers.
	mov	eax,0x10
	mov	ds,eax
	mov	es,eax
	mov	fs,eax
	mov	gs,eax
	mov	ss,eax
	mov	esp,0xfff0

	push	dword 0		; Clear all bits of flag register.
	popfd
	
				; Initialize bss.
	mov	ecx,bss_end
	sub	ecx,bss_start
	mov	edi,bss_start
	xor	al,al
	rep stosb

	call initialize_gdt		; Initialize IDT and GDT
	call initialize_idt		; Only processor exceptions and
					; system call interrupt are initialized
					

				; Reload GDTR 
	mov	esi,gdt_desc
	mov	ax,GDT_SIZE * 8 - 1
	mov	word [ds: esi],ax
	mov	eax, gdt_table
	mov	dword [ds: esi+2],eax
	lgdt	[ds:esi]
	jmp	dword 0x8 : next
	
next:				
				; Re-initialze segment registers.
	mov	ax,0x10
	mov	ds,ax
	mov	es,ax
	mov	fs,ax
	mov	gs,ax
	mov	ss,ax
	mov	esp,0xfff0


				; Collect the sysdata placed at
				; CONFIG_DATA_SEGMENT before it 
				; is over written.
	call	get_sys_data

	fninit			; Get the initial state of the FPU.
	fnsave	[initial_fpustate]
	or	word [initial_fpustate + 4], 0x3800
				; Set the top of the stack as 7

	mov	eax, 0x100000	; ABove 1MB onwards
	push	eax
	call	initialize_core_info	; Initialize free core list.
	pop	eax

	call	initialize_kernel_pagetables
	call	initialize_tss_table
	call	initialize_buffer_cache
	call	initialize_floppy_request_bufs
	call	initialize_hash_table
	call	initialize_socktable
	call	initialize_recvpkts
	;call	initialize_commlist


				; initialize kbd, timer, 8259s...
				; Change the timer frequency. Don't want to change now.

				; Initilize tss and create idle task.
	call 	initialize_systemprocess

				; Load task register.
	mov	ax,0x18
	ltr	ax

				; Initialize 8259's (PICs) and enable
				; all interrupts.
	call	initialize_8259s

	mov	al,0x80
	xor	al,0xff
	out	0x70,al


				; Call C - language main routine.
	call	cmain
				; Control never returns back.


