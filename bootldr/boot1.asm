;
;	Author : Chapram Sudhakar
;		 Lecturer, Dept of CSE, RECW.
;
; Disk bootstrap loader program. 
; Loads a given file (IMAGE) from msdos formatted floppy disk (1.44M)
;

	  BITS 16
	  ORG 0x00

STACK_POINTER   equ     0x2000  ; Assuming that FAT (9 sectors) + 
				; Root directory first secort +
				; This routine occupies 6KB.
				; At 8KB boundary stack is established.
IMAGE_LOADING   equ     0x1000
INITIAL_SEGMENT equ     0x9000  ; Bootstrap loader relocates to here.

	  SECTION .text

begin:    jmp start             

oemname db      'MSDOS5.0'
bps     dw      0x200
spc     db      1
ressect dw      1
nfat    db      2
ndire   dw      0x00e0
tsect1  dw      0x0b40
medbyte db      0xf0
fatsect dw      9
spt     dw      0x0012
heads   dw      2
hidsec  dw      0
vers    dw      0
tsect2  dd      0
drive   db      0
dosres1 db      0
sign    db      0x29
volid   db      0xfb,0x5f,0xcd,0x1c
disklab db      'BOOTTESTING'
dosres2 db      'abcdefgh'


; Relocate to 0x90000

start:  
	mov ax,0x07c0	; Boot location.
	mov ds,ax
	cld

; Relocate this to INITIAL_SEGMENT inorder to make space for the kernel.

	mov ax,INITIAL_SEGMENT
	mov es,ax
	mov cx,256
	xor di,di
	xor si,si
	rep movsw

; Jump to relocated bootstarp routine.
	
	jmp INITIAL_SEGMENT:rel_start


rel_start:
	
; Set the segment registers and stack.
	mov ax,INITIAL_SEGMENT
	mov ds,ax
	mov es,ax
	cli
	mov ss,ax
	mov sp,STACK_POINTER
	sti

read_fat:
	sub ah,ah       ; Reset floppy disk system.
	sub dl,dl
	int 0x13

	mov ax,0x0209   ; 9 - sectors of FAT.
	mov dx,0x0000
	mov bx,FAT
	mov cx,0x0002   
	int 0x13
	jnc read_rootdir
	jmp read_fat

read_rootdir:
	sub ah,ah       ; Reset floppy disk system.
	sub dl,dl
	int 0x13

	mov ax,0x0201   ; 1 - sector of ROOT DIRECTORY.
			; Assuming that Kernel Image to be within first 
			; 16 entries of root directory.
	mov dx,0x0100	; Head : 1 drive : 0 (A).	
	mov bx,ROOTDIR	
	mov cx,0x0002
	int 0x13
	jnc search_dir
	jmp read_rootdir

search_dir:
	call search_for_file    ; Search for the file in the sector read.
	cmp byte [searchflag],1 ; Found file ?
	je load_file

filenotfound:
	mov ax,msg_filenotfound
	push ax

	call display_mesg
	pop ax
	hlt

load_file:
	mov ax,load_mesg
	push ax
	call display_mesg
	pop ax

	mov ax,IMAGE_LOADING ; Loading segment address
	mov es,ax
	mov bx,0x0000

loadcluster:
	call cluster2param

loadcluster1:
	mov ah,0
	mov dl,0
	int 0x13

	mov ax,0x0201
	mov ch,[track]
	mov cl,[sect]
	mov dh,[head]
	mov dl,0
	int 0x13

	jnc loadcluster2
	push ax
	mov bp,sp
	call print_hex
	pop ax
	jmp loadcluster1

loadcluster2:
	mov ax,dot
	push ax
	call display_mesg
	pop ax

	add bx,0x200    ; Update the address to read the next cluster.
	cmp bx,0x0000   ; No Wrap around ?
	jne nextcluster
	mov ax,es
	add ax,0x1000
	mov es,ax       ; Goto next 64k segment.

nextcluster:
	call find_next_cluster
	cmp word  [clusterno],0x0fff
        je  jmp_to_image
        cmp word [clusterno],2847       ; Maximum cluster no.
        ja  jmp_to_image
        jmp loadcluster

jmp_to_image:           ; Loading is over ! Jump to start it.
	jmp IMAGE_LOADING:0x0000

;----------------------------------------------------------------------
;       This procedure converts 'clusterno' into physical disk params.
;----------------------------------------------------------------------
cluster2param:
	push ax
	push bx
	mov ax,[clusterno]      ; Two entries are reserved.
	sub ax,2
	add ax,0x21             ; Add first reserved sectors.
	mov bl,18
	div bl
	inc ah
	mov byte [sect],ah
	mov byte [track],al
	shr byte [track],1      ; Track no = (logical sect) div 36
	sub ah,ah       ; Head no = ((logical sect) div 18) % 2
	mov bl,2
	div bl
	mov byte [head],ah
	pop bx
	pop ax
	ret


;--------------------------------------------------------------------
;       given current 'clusterno' and fat loaded at FAT_LOADING this
;       procedure calculates next 'clusterno' in sequence.(12-bit FAT)
;-------------------------------------------------------------------
find_next_cluster:
	push ax
	push bx
	push si
	mov ax,[clusterno]
	mov bx,3
	mul bx
	shr ax,1
	mov si,ax
	mov bx,[si+FAT]
	test word [clusterno],0x0001
	jnz odd
	and bx,0x0fff
	jmp find_next_cluster1
odd:
	shr bx,4
find_next_cluster1:
	mov [clusterno],bx
	pop si
	pop bx
	pop ax
	ret

;----------------------------------------------------------------
;       Displays a message using BIOS interrupt on video page
;       number 1. Expects the address of the message in offset
;       form on stack. End of the message is inddicated by a NULL
;       character. display_mesg(char near * mesg).
;----------------------------------------------------------------
display_mesg:
	push bp
	mov bp,sp
	push si
	push ax
	push bx
	
	mov si,[ss:bp+4]	; String address.
display_mesg1:                  ; Find the length of the string.
	cmp byte[si],0		; Last character of the string ?
	je display_mesg2
	mov ah,0x0e		; Display character in teletype.
	mov al,[si]
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

;-------------------------------------------------------------------
;       Searches for the given 'filename' in the loaded directory
;       sector. As a result 'searchflag' is set to 1 if found.
;       On end of direcory entries 'endofdir' is set to 1.
;       'clusterno' is also set to the starting cluster of the file.
;-------------------------------------------------------------------

search_for_file:
	mov bx,0x10             ; No of entries in one sector.
	mov di,ROOTDIR          ; Dir sector loaded address.
search_for_file1:
	mov si,filename
	mov cx,11
	cmp byte [di],0         ; First byte zero means end of directory.
	je search_for_file3
	push di
	repe cmpsb
	pop di
	jne search_for_file2
	mov byte [searchflag],1
	mov cx,word [es:di+0x1a]        ; Starting cluster number.
	mov word [clusterno],cx
	jmp search_for_file3    ; Go to return.
search_for_file2:
	add di,0x0020           ; Next entry in the directory.
	dec bx
	jnz search_for_file1
search_for_file3:
	ret

print_hex:
	mov	cx, 4		; 4 hex digits
	mov	dx, [bp]	; load word into dx
print_digit:
	rol	dx, 4		; rotate so that lowest 4 bits are used
	mov	ax, 0xe0f	; ah = request, al = mask for nybble
	and	al, dl
	add	al, 0x90	; convert al to ASCII hex (four instructions)
	daa
	adc	al, 0x40
	daa
	int	0x10
	loop	print_digit
	ret

load_mesg	 db 0x0d,0x0a,'Loading IMAGE',0
dot		 db '.',0
msg_filenotfound db 0x0d,0x0a,'Error!',0
filename	 db 'IMAGE      '
zerobytes:      times 510-$+begin db 0
signature       dw 0xAA55
	SECTION .bss
head            resb    1
track           resb    1
sect            resb    1
searchflag      resb    1
retries         resb    1
clusterno       resw    1

FAT             resb    512*9
ROOTDIR         resb    512


	
