; This file assembly language entry points to all interrupt handlers
; and system calls.

	bits 32

section .text

%include "../include/mconst.inc"
%define	_EAX	28
%define ERROR_BADSYSCALL	-1

global ignore_int:function
global divide_error:function
global coprocessor_error:function
global simd_coprocessor_error:function
global device_not_available:function
global debug:function
global nmi:function
global inter3:function
global overflow:function
global bounds:function
global invalid_op:function
global coprocessor_segment_overrun:function
global double_fault:function
global invalid_TSS:function
global segment_not_present:function
global stack_segment:function
global general_protection:function
global alignment_check:function
global page_fault:function
global machine_check:function
global spurious_interrupt_bug:function
global sys_call_entry:function
global sys_call_exit:function
global display_stack_incr
extern disp_int

global irq_entry_0_0:function
global irq_entry_0_1:function
global irq_entry_0_2:function
global irq_entry_0_3:function
global irq_entry_0_4:function
global irq_entry_0_5:function
global irq_entry_0_6:function
global irq_entry_0_7:function
global irq_entry_1_0:function
global irq_entry_1_1:function
global irq_entry_1_2:function
global irq_entry_1_3:function
global irq_entry_1_4:function
global irq_entry_1_5:function
global irq_entry_1_6:function
global irq_entry_1_7:function
global initialize_idt:function
global idt_table:data

extern do_divide_error
extern do_coprocessor_error
extern do_simd_coprocessor_error
extern do_debug
extern do_nmi
extern do_inter3
extern do_overflow
extern do_bounds
extern do_invalid_op
extern do_coprocessor_segment_overrun
extern do_device_not_available
extern do_double_fault
extern do_invalid_TSS
extern do_segment_not_present
extern do_stack_segment
extern do_general_protection
extern do_alignment_check
extern do_page_fault
extern do_machine_check
extern do_spurious_interrupt_bug
extern do_unhandled_interrupt
extern math_state_restore
extern math_emulate
extern systemcall
extern irq_handler
extern irq_mesg
extern print_hex
extern print_context
extern default_irq_handler
extern restore_proc
extern print_tss
extern timer_handler
extern kbd_intr_handler
extern fdc_intr_handler
extern nic_isr
extern debug_mesg
extern check_urgent_flags	

%macro	SAVEALL 0
	push	ds
	push	es
	push	fs
	push	gs
	pusha
%endmacro

%macro	RESTOREALL 0
	popa
	pop	gs
	pop	fs
	pop	es
	pop	ds
%endmacro

align	16
ignore_int:

	SAVEALL
	mov	edx,esp
	push	dword 0x0
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_unhandled_interrupt
	add	esp,8
	jmp	interrupt_exit
align	16
divide_error:		; vect 0
	SAVEALL
	mov	edx,esp
	push	dword 0x0
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_divide_error
	add	esp,8
	jmp	interrupt_exit
align	16
coprocessor_error:	; 
	SAVEALL
	mov	edx,esp
	push	dword 0x0
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_coprocessor_error
	add	esp,8
	jmp	interrupt_exit
align	16
simd_coprocessor_error:	; vect 19
	SAVEALL
	mov	edx,esp
	push	dword 0x0
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_simd_coprocessor_error
	add	esp,8
	jmp	interrupt_exit
align	16
device_not_available:	; vect 7
	clts
	iret
; Actually save and restore code for FPU is done at
; higher lever during context switch. So only task switched 
; flag clearance is sufficient.
align	16
debug:			; vect 1
	SAVEALL
	mov	edx,esp
	push	dword 0x0
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_debug
	add	esp,8
	jmp	interrupt_exit
align	16
nmi:			; vect 2
	SAVEALL
	mov	edx,esp
	push	dword 0x0
	push	edx
	mov	edx,0x10; Make kernel data segment addressable.
	mov	ds,edx
	mov	es,edx

	call	do_nmi
	add	esp,8
	jmp	interrupt_exit
align	16
inter3:			; vect 3
	SAVEALL
	mov	edx,esp
	push	dword 0x0
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_inter3
	add	esp,8
	jmp	interrupt_exit
align	16
overflow:		; vect 4
	SAVEALL
	mov	edx,esp
	push	dword 0x0
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_overflow
	add	esp,8
	jmp	interrupt_exit
align	16
bounds:			; vect 5
	SAVEALL
	mov	edx,esp
	push	dword 0x0
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_bounds
	add	esp,8
	jmp	interrupt_exit
align	16
invalid_op:		; vect 6
	SAVEALL
	mov	edx,esp
	push	dword 0x0
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_invalid_op
	add	esp,8
	jmp	interrupt_exit
align	16
coprocessor_segment_overrun:	; vect 9
	SAVEALL
	mov	edx,esp
	push	dword 0x0
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_coprocessor_segment_overrun
	add	esp,8
	jmp	interrupt_exit
align	16
double_fault:		; vect 8
	SAVEALL
	mov	edx,esp
	mov	ecx,[ss:esp+48]	; Error code
	push	ecx
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_double_fault
	add	esp,8
	jmp fault_exit
align	16
invalid_TSS:		; vect 10
	SAVEALL
	mov	edx,esp
	mov	ecx,[ss:esp+48]	; Error code
	push	ecx
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_invalid_TSS
	add	esp,8
	jmp fault_exit
align	16
segment_not_present:	; vect 11
	SAVEALL
	mov	edx,esp
	mov	ecx,[ss:esp+48]	; Error code
	push	ecx
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_segment_not_present
	add	esp,8
	jmp fault_exit
align	16
stack_segment:		; vect 12
	SAVEALL
	mov	edx,esp
	mov	ecx,[ss:esp+48]	; Error code
	push	ecx
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_stack_segment
	add	esp,8
	jmp fault_exit
align	16
general_protection:	; vect 13
	SAVEALL
	mov	edx,esp
	mov	ecx,[ss:esp+48]	; Error code
	push	ecx
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_general_protection
	add	esp,8
	jmp fault_exit
align	16
alignment_check:	; vect 17
	SAVEALL
	mov	edx,esp
	mov	ecx,[ss:esp+48]	; Error code
	push	ecx
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_alignment_check
	add	esp,8
	jmp fault_exit
align	16
page_fault:		; vect 14
	SAVEALL
	mov	edx,esp
	mov	ecx,[ss:esp+48]	; Error code
	push	ecx
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	; Long time is taken so enable the interrupts
	sti
	call	do_page_fault
	add	esp,8
	jmp fault_exit
align	16
machine_check:		; vect 18
	SAVEALL
	mov	edx,esp
	push	dword 0x0
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_machine_check
	add	esp,8
	jmp interrupt_exit
align	16
spurious_interrupt_bug:	; Unexpected interrupt.
	SAVEALL
	mov	edx,esp
	push	dword 0x0
	push	edx
	mov	edx,0x10
	mov	ds,edx
	mov	es,edx
	call	do_spurious_interrupt_bug
	add	esp,8
	jmp interrupt_exit

align	16
sys_call_entry:

	SAVEALL
	mov	edx,esp		; eax contains sys call number
	push	edx		; Pointer to the REGS struct.
	cmp	eax,NR_SYSCALLS
	ja	bad_sys_call
	mov	edx,0x10	; Load data segment desc to all seg regs
	mov	ds,edx
	mov	es,edx
	sti			; // Enable interrupts
	call	systemcall
	add	esp,4
	mov	dword [ss: esp+_EAX],eax	; System call return code
	jmp	sys_call_exit
bad_sys_call:
	add	esp,4		; remove pointer to REGS struct.
	mov	dword [ss: esp+_EAX],ERROR_BADSYSCALL

irq_exit:		; Exit for irq handlers, processor interrupts, sys_calls
interrupt_exit:
sys_call_exit:
		; Some of the events, other urgent information is indicated 
		; by seeting the flags which must checked before returning 
		; to user mode.
	call	check_urgent_flags	
	RESTOREALL
	iret

;temp_sys_call_exit:
	;call 	restore_proc
	;RESTOREALL
	;iret

fault_exit:		; Exit from some processor faults
		; Some of the events, other urgent information is indicated 
		; by seeting the flags which must checked before returning 
		; to user mode.
	call	check_urgent_flags	
	RESTOREALL
	add	esp,4	; Remove error code.
	iret

align	16
irq_entry_0_0:
	SAVEALL
	mov	edx,esp
	in	al,INT_CTLMASK	; Disable that interrupt.
	or	al, 1
	out	INT_CTLMASK,al
	mov	al,ENABLE	; Enable other irqs
	out	INT_CTL,al
	sti			; enable interrupts

	push	dword 0		; Call timer handler
	call	timer_handler
	add	esp,4

	cli			; Enable that interrupt.
	in	al,INT_CTLMASK
	and	al,~(1)
	out	INT_CTLMASK,al
	jmp	irq_exit

	
align	16
irq_entry_0_1:	; Completely new task
	in	al,INT_CTLMASK	; Disable that interrupt.
	or	al, 1<<1
	out	INT_CTLMASK,al
	mov	al,ENABLE	; Enable other irqs
	out	INT_CTL,al

	push	dword 1		; Call kbd handler
	call	kbd_intr_handler
	add	esp,4
	jmp	irq_entry_0_1

align	16
irq_entry_0_2:
	in	al,INT_CTLMASK	; Disable that interrupt.
	or	al, 1 << 2
	out	INT_CTLMASK,al
	mov	al,ENABLE	; Enable other irqs
	out	INT_CTL,al

	push	dword 2		; Call timer handler
	call	default_irq_handler
	add	esp,4
	jmp	irq_entry_0_2

;	hwint_master 2
align	16
irq_entry_0_3:
	in	al,INT_CTLMASK	; Disable that interrupt.
	or	al, 1 << 3
	out	INT_CTLMASK,al
	mov	al,ENABLE	; Enable other irqs
	out	INT_CTL,al

	push	dword 3		; Call timer handler
	call	default_irq_handler
	add	esp,4
	jmp	irq_entry_0_3

;	hwint_master 3
align	16
irq_entry_0_4:
	in	al,INT_CTLMASK	; Disable that interrupt.
	or	al, 1 << 4
	out	INT_CTLMASK,al
	mov	al,ENABLE	; Enable other irqs
	out	INT_CTL,al

	push	dword 4		; Call timer handler
	call	default_irq_handler
	add	esp,4
	jmp	irq_entry_0_4

;	hwint_master 4
align	16
irq_entry_0_5:
	in	al,INT_CTLMASK	; Disable that interrupt.
	or	al, 1 << 5
	out	INT_CTLMASK,al
	mov	al,ENABLE	; Enable other irqs
	out	INT_CTL,al

	push	dword 5		; Call timer handler
	call	default_irq_handler
	add	esp,4
	jmp	irq_entry_0_5

;	hwint_master 5
align	16
irq_entry_0_6:
	in	al,INT_CTLMASK	; Disable that interrupt.
	or	al, 1 << 6
	out	INT_CTLMASK,al
	mov	al,ENABLE	; Enable other irqs
	out	INT_CTL,al

	push	dword 6		; Call timer handler
	call	fdc_intr_handler
	add	esp,4
	jmp	irq_entry_0_6

align	16
irq_entry_0_7:
	in	al,INT_CTLMASK	; Disable that interrupt.
	or	al, 1 << 7
	out	INT_CTLMASK,al
	mov	al,ENABLE	; Enable other irqs
	out	INT_CTL,al

	push	dword 7		; Call timer handler
	call	default_irq_handler
	add	esp,4
	jmp	irq_entry_0_7

;	hwint_master 7
align	16
irq_entry_1_0:
	in	al,INT2_CTLMASK	; Disable that interrupt.
	or	al,1
	out	INT2_CTLMASK,al
	mov	al,ENABLE	; Enable other irqs
	out	INT_CTL,al
	mov	al,ENABLE
	out	INT2_CTL,al

	push	dword 8		; Call irq handler
	call	default_irq_handler
	add	esp,4
	jmp	irq_entry_1_0
;	hwint_slave 8
align	16
irq_entry_1_1:	; IRQ 9, NIC card
	in	al,INT2_CTLMASK	; Disable that interrupt.
	or	al,1<<1
	out	INT2_CTLMASK,al
	mov	al,ENABLE	; Enable other irqs
	out	INT_CTL,al
	mov	al,ENABLE
	out	INT2_CTL,al

	push	dword 9		; Call irq handler
	call	nic_isr
	add	esp,4
	jmp	irq_entry_1_1
;	hwint_slave 9
align	16
irq_entry_1_2:
	in	al,INT2_CTLMASK	; Disable that interrupt.
	or	al,1<<2
	out	INT2_CTLMASK,al
	mov	al,ENABLE	; Enable other irqs
	out	INT_CTL,al
	mov	al,ENABLE
	out	INT2_CTL,al

	push	dword 10		; Call irq handler
	call	default_irq_handler
	add	esp,4
	jmp	irq_entry_1_2
;	hwint_slave 10
align	16
irq_entry_1_3:
	in	al,INT2_CTLMASK	; Disable that interrupt.
	or	al,1<<3
	out	INT2_CTLMASK,al
	mov	al,ENABLE	; Enable other irqs
	out	INT_CTL,al
	mov	al,ENABLE
	out	INT2_CTL,al

	push	dword 11	; Call irq handler
	call	default_irq_handler
	add	esp,4
	jmp	irq_entry_1_3
;	hwint_slave 11
align	16
irq_entry_1_4:
	in	al,INT2_CTLMASK	; Disable that interrupt.
	or	al,1<<4
	out	INT2_CTLMASK,al
	mov	al,ENABLE	; Enable other irqs
	out	INT_CTL,al
	mov	al,ENABLE
	out	INT2_CTL,al

	push	dword 12	; Call irq handler
	call	default_irq_handler
	add	esp,4
	jmp	irq_entry_1_4
;	hwint_slave 12
align	16
irq_entry_1_5:
	in	al,INT2_CTLMASK	; Disable that interrupt.
	or	al,1<<5
	out	INT2_CTLMASK,al
	mov	al,ENABLE	; Enable other irqs
	out	INT_CTL,al
	mov	al,ENABLE
	out	INT2_CTL,al

	push	dword 13		; Call irq handler
	call	default_irq_handler
	add	esp,4
	jmp	irq_entry_1_5
;	hwint_slave 13
align	16
irq_entry_1_6:
	in	al,INT2_CTLMASK	; Disable that interrupt.
	or	al,1<<6
	out	INT2_CTLMASK,al
	mov	al,ENABLE	; Enable other irqs
	out	INT_CTL,al
	mov	al,ENABLE
	out	INT2_CTL,al

	push	dword 14	; Call irq handler
	call	default_irq_handler
	add	esp,4
	jmp	irq_entry_1_6
;	hwint_slave 14
align	16
irq_entry_1_7:
	in	al,INT2_CTLMASK	; Disable that interrupt.
	or	al,1<<7
	out	INT2_CTLMASK,al
	mov	al,ENABLE	; Enable other irqs
	out	INT_CTL,al
	mov	al,ENABLE
	out	INT2_CTL,al

	push	dword 15	; Call irq handler
	call	default_irq_handler
	add	esp,4
	jmp	irq_entry_1_7
;	hwint_slave 15

%macro	SET_VECTOR 2
	lea	eax,[%2]
	
	lea	edi,[idt_table]
	mov	ecx,%1
	shl	ecx,3
	add	edi,ecx
	mov	word [ds:edi],ax
	shr	eax,16
	mov	word [ds:edi+6],ax
%endmacro

initialize_irq_taskgates:
	mov	ax, NO_SYS_DESCRIPTORS + 1 + 1 	; Starting gdt entry for 
						; irq task descriptors,
						; excluding timer interrupt.
	shl	ax, 3
	lea	edi, [idt_table]
	mov	ecx, 14				; Total irqs - 2
	mov	ebx, 33*8			; In idt irq task gate start + 
						; 1 for timer irq
	add	edi, ebx

initialize_irq_taskgates_1:
	mov	word [ds: edi + 2], ax
	add	edi, 8
	add	ax, 8
	dec	ecx
	jnz	initialize_irq_taskgates_1
	ret

initialize_idt:
	push	eax
	push	ebx
	push	ecx
	push	edx
	push	edi
	SET_VECTOR 0, divide_error
	SET_VECTOR 1, debug
	SET_VECTOR 2, nmi
	SET_VECTOR 3, inter3
	SET_VECTOR 4, overflow
	SET_VECTOR 5, bounds
	SET_VECTOR 6, invalid_op
	SET_VECTOR 7, device_not_available
	SET_VECTOR 8, double_fault
	SET_VECTOR 9, coprocessor_segment_overrun
	SET_VECTOR 10, invalid_TSS
	SET_VECTOR 11, segment_not_present
	SET_VECTOR 12, stack_segment
	SET_VECTOR 13, general_protection
	SET_VECTOR 14, page_fault
	SET_VECTOR 15, spurious_interrupt_bug
	SET_VECTOR 16, coprocessor_error
	SET_VECTOR 17, alignment_check
	SET_VECTOR 18, machine_check
	SET_VECTOR 19, simd_coprocessor_error

	SET_VECTOR 20, ignore_int
	SET_VECTOR 21, ignore_int
	SET_VECTOR 22, ignore_int
	SET_VECTOR 23, ignore_int
	SET_VECTOR 24, ignore_int
	SET_VECTOR 25, ignore_int
	SET_VECTOR 26, ignore_int
	SET_VECTOR 27, ignore_int
	SET_VECTOR 28, ignore_int
	SET_VECTOR 29, ignore_int
	SET_VECTOR 30, ignore_int
	SET_VECTOR 31, ignore_int

	call	initialize_irq_taskgates
	SET_VECTOR 32, irq_entry_0_0

	SET_VECTOR 48, sys_call_entry



	SET_VECTOR 49, ignore_int
	SET_VECTOR 50, ignore_int
	SET_VECTOR 51, ignore_int
	SET_VECTOR 52, ignore_int
	SET_VECTOR 53, ignore_int
	
	lidt	[idt_desc]
	pop	edi
	pop	edx
	pop	ecx
	pop	ebx
	pop	eax
	ret


	SECTION .data
idt_table:
	dw	0	;0
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 1
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 2
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 3
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 4
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 5
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 6
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 7
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 8
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 9
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; a
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; b
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; c
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; d
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; e
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; f
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 10
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 11
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 12
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 13
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 14
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 15
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 16
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 17
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 18
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 19
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 1a
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 1b
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 1c
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 1d
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 1e
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 1f
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; irq 0
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; irq 1
	dw	0x8
	db	0
	db	0x85
	dw	0

	dw	0	; irq 2
	dw	0x8
	db	0
	db	0x85
	dw	0

	dw	0	; irq 3
	dw	0x8
	db	0
	db	0x85
	dw	0

	dw	0	; irq 4
	dw	0x8
	db	0
	db	0x85
	dw	0

	dw	0	; irq 5
	dw	0x8
	db	0
	db	0x85
	dw	0

	dw	0	; irq 6
	dw	0x8
	db	0
	db	0x85
	dw	0

	dw	0	; irq 7
	dw	0x8
	db	0
	db	0x85
	dw	0

	dw	0	; irq 8
	dw	0x8
	db	0
	db	0x85
	dw	0

	dw	0	; irq 9
	dw	0x8
	db	0
	db	0x85
	dw	0

	dw	0	; irq 10
	dw	0x8
	db	0
	db	0x85
	dw	0

	dw	0	; irq 11
	dw	0x8
	db	0
	db	0x85
	dw	0

	dw	0	; irq 12
	dw	0x8
	db	0
	db	0x85
	dw	0

	dw	0
	dw	0x8
	db	0
	db	0x85
	dw	0

	dw	0
	dw	0x8
	db	0
	db	0x85
	dw	0

	dw	0
	dw	0x8
	db	0
	db	0x85
	dw	0

	dw	0	; 30	(System call)
	dw	0x8
	db	0
	db	0xee
	dw	1

	dw	0	; 31	(From here user defined ...)
	dw	0xB8
	db	0
	db	0x85
	dw	1

	dw	0	; 32
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 33
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 34
	dw	0x8
	db	0
	db	0x8e
	dw	1

	dw	0	; 35 (0 - 53 idt entries)
	dw	0x8
	db	0
	db	0x8e
	dw	1



idt_desc:		; Initial IDT with no entries.
	dw (54 * 8) - 1
	dd idt_table
	dw 0

