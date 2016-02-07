; System call library to be linked with applications.
; Change ebx such that it points to the first paramter on stack.
;
	bits	32
section	.text
global open:function
global close:function
global getchar:function
global getchare:function
global getscanchar:function
global putchar:function
global puts:function
global mkdir:function
global rmdir:function
global chdir:function
global creat:function
global rename:function
global read:function
global write:function
global lseek:function
global readdir:function
global opendir_internal:function
global closedir_internal:function
global stat:function
global unlink:function
global chmod:function
global dup:function
global dup2:function
global setcursor:function
global tsleep:function
global sync:function
global readblocks:function
global writeblocks:function
global txpkt:function
global rxpkt:function
global tsendto:function
global trecvfrom:function
global exectask:function
global pthread_create:function
global pthread_exit:function
global pthread_join:function
global pthread_detach:function
global pthread_yield:function
global pthread_mutex_init:function
global pthread_mutex_trylock:function
global pthread_mutex_lock:function
global pthread_mutex_unlock:function
global pthread_cond_init:function
global pthread_cond_wait:function
global pthread_cond_signal:function
global pthread_cond_broadcast:function
global pthread_barrier_init:function
global pthread_barrier_wait:function
global pthread_self:function
global socket:function
global bind:function
global send:function
global sendto:function
global recv:function
global recvfrom:function
global accept:function
global connect:function
global listen:function
global exit:function
global kill:function
global getprocinfo:function
global getpids:function

global sem_init:function
global sem_destroy_internal:function
global sem_openv:function
global sem_close_internal:function
global sem_unlink:function
global sem_wait:function
global sem_trywait:function
global sem_timedwait:function
global sem_post:function
global sem_getvalue:function

global time:function
global brk_internal:function
global gettsc:function

global memstatus:function

global _wait:function
global pageinfo:function

global machid:function
global getcharf:function
global ungetcharf:function
global broadcast:function
global primalloc_internal:function
global prifree_internal:function
global make_intr:function

global marktsc:function
global timed:function
global pthread_mutex_destroy:function


extern debug_mesg

open:
	push	ebx
	mov	eax, 1			; Syscall number
	jmp	make_intr

close:
	push	ebx
	mov	eax, 2			; syscall number
	jmp	make_intr

read:
	push	ebx
	mov	eax, 3
	jmp	make_intr

write:
	push	ebx
	mov	eax, 4
	jmp	make_intr

lseek:
	push	ebx
	mov	eax, 5
	jmp	make_intr

mkdir:
	push	ebx
	mov	eax,6
	jmp	make_intr

rmdir:
	push	ebx
	mov	eax,7
	jmp	make_intr
chdir:
	push	ebx
	mov	eax,8
	jmp	make_intr
creat:
	push	ebx
	mov	eax,9
	jmp	make_intr

rename:
	push	ebx
	mov	eax,10
	jmp	make_intr

opendir_internal:
	push	ebx
	mov	eax,11
	jmp	make_intr

closedir_internal:
	push	ebx
	mov	eax,12
	jmp	make_intr

readdir:
	push	ebx
	mov	eax,13
	jmp	make_intr

stat:
	push	ebx
	mov	eax,14
	jmp	make_intr

unlink:
	push	ebx
	mov	eax,15
	jmp	make_intr

chmod:
	push	ebx
	mov	eax,16
	jmp	make_intr

dup:
	push	ebx
	mov	eax,17
	jmp	make_intr

dup2:
	push	ebx
	mov	eax,18
	jmp	make_intr

setcursor:
	push	ebx
	mov	eax,19
	jmp	make_intr

getchar:
	push	ebx
	mov	eax, 20
	jmp	make_intr

getchare:
	push	ebx
	mov	eax, 21
	jmp	make_intr

getscanchar:
	push	ebx
	mov	eax, 22
	jmp	make_intr

putchar:
	push	ebx
	mov	eax, 23
	jmp	make_intr

puts:
	push	ebx
	mov	eax, 24
	jmp	make_intr

seekdir:
	push	ebx
	mov	eax, 25
	jmp	make_intr

telldir:
	push	ebx
	mov	eax, 26
	jmp	make_intr

rewinddir:
	push	ebx
	mov	eax, 27
	jmp	make_intr

readblocks:
	push	ebx
	mov	eax, 32
	jmp	make_intr

writeblocks:
	push	ebx
	mov	eax, 33
	jmp	make_intr

sync:
	push	ebx
	mov	eax, 34
	jmp	make_intr

txpkt:
	push	ebx
	mov	eax, 35
	jmp	make_intr

rxpkt:
	push	ebx
	mov	eax, 36
	jmp	make_intr

tsleep:
	push	ebx
	mov	eax, 50
	jmp	make_intr

tsendto:
	push	ebx
	mov	eax, 60
	jmp	make_intr

exectask:
	push	ebx
	mov	eax, 65
	jmp	make_intr
trecvfrom:
	push	ebx
	mov	eax, 62
	jmp	make_intr

exit:
	push	ebx
	mov	eax, 66
	jmp	make_intr

pthread_create:
	push	ebx
	mov	eax, 70
	jmp	make_intr

pthread_exit:
	push	ebx
	mov	eax, 71
	jmp	make_intr

pthread_join:
	push	ebx
	mov	eax, 72
	jmp	make_intr

pthread_detach:
	push	ebx
	mov	eax, 73
	jmp	make_intr

pthread_yield:
	push	ebx
	mov	eax, 74
	jmp	make_intr

pthread_mutex_init:
	push	ebx
	mov	eax, 75
	jmp	make_intr

pthread_mutex_trylock:
	push	ebx
	mov	eax, 76
	jmp	make_intr

pthread_mutex_lock:
	push	ebx
	mov	eax, 77
	jmp	make_intr

pthread_mutex_unlock:
	push	ebx
	mov	eax, 78
	jmp	make_intr

pthread_cond_init:
	push	ebx
	mov	eax, 79
	jmp	make_intr

pthread_cond_wait:
	push	ebx
	mov	eax, 80
	jmp	make_intr

pthread_cond_signal:
	push	ebx
	mov	eax, 81
	jmp	make_intr

pthread_cond_broadcast:
	push	ebx
	mov	eax, 82
	jmp	make_intr

pthread_barrier_init:
	push	ebx
	mov	eax, 83
	jmp	make_intr

pthread_barrier_wait:
	push	ebx
	mov	eax, 84
	jmp	make_intr

; Socket related calls
socket:
	push	ebx
	mov	eax,90
	jmp	make_intr

bind:
	push	ebx
	mov	eax,91
	jmp	make_intr

recv:
	push	ebx
	mov	eax,92
	jmp	make_intr

recvfrom:
	push	ebx
	mov	eax,93
	jmp	make_intr

send:
	push	ebx
	mov	eax,94
	jmp	make_intr

sendto:
	push	ebx
	mov	eax,95
	jmp	make_intr

connect:
	push	ebx
	mov	eax,96
	jmp	make_intr

accept:
	push	ebx
	mov	eax,97
	jmp	make_intr

listen:
	push	ebx
	mov	eax,98
	jmp	make_intr

kill:
	push	ebx
	mov	eax,99
	jmp	make_intr

getprocinfo:
	push	ebx
	mov	eax,100
	jmp	make_intr

getpids:
	push	ebx
	mov	eax,101
	jmp	make_intr

sem_init:
	push	ebx
	mov	eax,102
	jmp	make_intr

sem_destroy_internal:
	push	ebx
	mov	eax,103
	jmp	make_intr

sem_openv:
	push	ebx
	mov	eax,104
	jmp	make_intr

sem_close_internal:
	push	ebx
	mov	eax,105
	jmp	make_intr

sem_unlink:
	push	ebx
	mov	eax,106
	jmp	make_intr

sem_wait:
	push	ebx
	mov	eax,107
	jmp	make_intr

sem_trywait:
	push	ebx
	mov	eax,108
	jmp	make_intr

sem_timedwait:
	push	ebx
	mov	eax,109
	jmp	make_intr

sem_post:
	push	ebx
	mov	eax,110
	jmp	make_intr

sem_getvalue:
	push	ebx
	mov	eax,111
	jmp	make_intr

time:
	push	ebx
	mov	eax,112
	jmp	make_intr

pthread_self:
	push	ebx
	mov	eax,113
	jmp	make_intr

brk_internal:
	push	ebx
	mov	eax,114
	jmp	make_intr

gettsc:
	push	ebx
	mov	eax,115
	jmp	make_intr

memstatus:
	push	ebx
	mov	eax, 140
	jmp	make_intr

_wait:
	push 	ebx
	mov 	eax,147
	jmp	make_intr
	
pageinfo:
	push 	ebx
	mov 	eax,148
	jmp	make_intr

machid:
	push 	ebx
	mov 	eax,153
	jmp	make_intr
	
getcharf:
	push 	ebx
	mov 	eax,154
	jmp	make_intr
	
ungetcharf:
	push 	ebx
	mov 	eax,155
	jmp	make_intr
	
broadcast:
	push 	ebx
	mov 	eax,156
	jmp	make_intr
	
primalloc_internal:
	push 	ebx
	mov 	eax,157
	jmp	make_intr
	
prifree_internal:
	push 	ebx
	mov 	eax,158
	jmp	make_intr
	
marktsc:
	push 	ebx
	mov 	eax,202
	jmp	make_intr

timed:
	push 	ebx
	mov 	eax,203
	jmp	make_intr

pthread_mutex_destroy:
	push 	ebx
	mov 	eax,204
	jmp	make_intr

	
	
make_intr:
	mov	ebx, esp		; first parametr location 
	add	ebx, 8			; pointer
	int	0x30			; trap to OS.
	pop	ebx
	ret


pthread_exit1:
	push	ebx
	mov	eax, 71
	mov	ebx, esp		; first parametr location 
	add	ebx, 8			; pointer
	int	0x30			; trap to OS.
	pop	ebx
	ret

