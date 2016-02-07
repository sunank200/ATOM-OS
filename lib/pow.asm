
	BITS	32
	SECTION	.data
pointfive:	
	dq	0.5

	SECTION	.text
global	pow_internal:function
global	abs:function
global	round:function
global	madd:function
global	ylog2x:function
global	ceil:function
global	p2xminus1:function
global	p2xstary:function
global	frexp:function

pow_internal:
	push	ebp
	mov	ebp, esp
	
	fld	qword [ss:ebp + 4 + 4 + 8]	; y
	fld	qword [ss:ebp + 4 + 4]		; x

	fyl2x
	fst	qword [ss:ebp + 4 + 4]		; y log x
	fld	qword [ss:ebp + 4 + 4]		; Push once again
	fld	qword [ss:ebp + 4 + 4]		; Push once again
	frndint
	fsubp	st1, st0
	f2xm1	
	fld1
	fstp	qword [ss: ebp + 4 + 4 + 8]
	fadd	qword [ss: ebp + 4 + 4 + 8]
	fscale
	fstp	qword [ss: ebp + 4 + 4 + 8]
	fstp	qword [ss: ebp + 4 + 4]
	fld	qword [ss: ebp + 4 + 4 + 8]
			; Result is available on stack st[0]
	pop	ebp
	ret

abs:	
	push	ebp
	mov	ebp, esp
	
	fld	qword [ss:ebp + 4 + 4]	; x
	fabs
	pop	ebp
	ret

round:
	push	ebp
	mov	ebp, esp
	fld	qword [ss:ebp + 4 + 4] ; x
	frndint
	pop	ebp
	ret

madd:
	push	ebp
	mov	ebp, esp
	fld	qword [ss: ebp + 4 + 4]
	fadd	qword [ss: ebp + 4 + 4 + 8]
	pop	ebp
	ret

ylog2x:
	push	ebp
	mov	ebp, esp
	fld	qword [ss:ebp + 4 + 4 + 8] ; x
	fld	qword [ss:ebp + 4 + 4] ; x
	fyl2x
	pop	ebp
	ret

ceil:
	push	ebp
	mov	ebp, esp
	fld	qword [ss:ebp + 4 + 4] ; x
	fld	qword [ds:pointfive]
	faddp	st1, st0
	frndint
	pop	ebp
	ret

p2xminus1:
	push	ebp
	mov	ebp, esp
	fld	qword [ss:ebp + 4 + 4] ; x
	f2xm1
	pop	ebp
	ret

p2xstary:
	push	ebp
	mov	ebp, esp
	fld	qword [ss:ebp + 4 + 4] ; x
	fld	qword [ss:ebp + 4 + 4 + 8] ; y
	fscale
	fstp	qword [ss:ebp + 4 + 4] ; remove top 2
	fstp	qword [ss:ebp + 4 + 4 + 8] ; 
	fld	qword [ss:ebp + 4 + 4] ; place top one res
	pop	ebp
	ret

frexp:
	push	ebp
	mov	ebp, esp
	push	eax
	fld	qword [ss:ebp + 4 + 4] ; x
	fxtract
	fstp	qword [ss:ebp + 4 + 4] ; temorarily save mantissa
	mov	eax, dword [ss:ebp + 4 + 4 + 8]	; *exp
	fstp	qword [ds: eax]
	fld	qword [ss:ebp + 4 + 4]	; Put it back
	pop	eax
	pop	ebp
	ret
