#include <asm.h>
#include <segment.h>
/**************************************************/
/**** Save & Restore ******************************/
/**                                              **/
/** When we change to privilege level 0 (kernel) **/
/** (through an interrupt, a system call, an     **/
/** exception ...) we must save the state of the **/
/** currently running task (save).               **/
/**                                              **/
/** Stack layout in 'systemCall':                **/
/**                                              **/
/**   0(%esp) - %ebx    \                        **/
/**   4(%esp) - %ecx     |                       **/
/**   8(%esp) - %edx     |                       **/
/**   C(%esp) - %esi     | Register saved        **/
/**  10(%esp) - %edi     |  by 'save'            **/
/**  14(%esp) - %ebp     |                       **/
/**  18(%esp) - %eax     |                       **/
/**  1C(%esp) - %ds      |                       **/
/**  20(%esp) - %es      |                       **/
/**  24(%esp) - %fs      |                       **/
/**  28(%esp) - %gs     /                        **/
/**  2C(%esp) - %eip    \                        **/
/**  30(%esp) - %cs      |                       **/
/**  34(%esp) - %eflags  |  Return context saved **/
/**  38(%esp) - %oldesp  |   by the processor.   **/
/**  3C(%esp) - %oldss  /                        **/
/**                                              **/
/**************************************************/

#define SAVE_ALL \
	pushl %gs; \
	pushl %fs; \
	pushl %es; \
	pushl %ds; \
	pushl %eax; \
	pushl %ebp; \
	pushl %edi; \
	pushl %esi; \
	pushl %edx; \
	pushl %ecx; \
	pushl %ebx; \
	movl $__KERNEL_DS, %edx;    \
	movl %edx, %ds;           \
	movl %edx, %es

#define RESTORE_ALL \
	popl %ebx;\
	popl %ecx;\
	popl %edx;\
	popl %esi;\
	popl %edi;\
	popl %ebp;\
	popl %eax;\
	popl %ds; \
	popl %es; \
	popl %fs; \
	popl %gs

#define SYSCALL(syscalln) \
	leal syscall_ret##syscalln, %eax; \
	push %eax; \
	movl $syscalln, %eax; \
	push %ebp; \
	movl %esp, %ebp; \
	sysenter; \
	syscall_ret##syscalln: pop %ebp; \
	addl $4, %esp

#define CHECK_ERROR(end) \
	cmpl $0, %eax; \
	jge  end; \
	negl %eax; \
	leal errno, %ebx; \
	movl %eax, (%ebx); \
	movl $-1, %eax


ENTRY(write)
	push %ebp
	movl %esp, %ebp
	push %ebx
	movl 8(%ebp), %ebx
	movl 12(%ebp), %ecx
	movl 16(%ebp), %edx
	SYSCALL(4);
	CHECK_ERROR(write.no_error);
	write.no_error:
	pop %ebx
	pop %ebp
	ret

ENTRY(read)
        push %ebp
        movl %esp, %ebp
        push %ebx
        movl 8(%ebp), %ebx
        movl 12(%ebp), %ecx
        movl 16(%ebp), %edx
        SYSCALL(3);
        CHECK_ERROR(read.no_error);
        read.no_error:
        pop %ebx
        pop %ebp
        ret

ENTRY(gettime)
	push %ebp
	movl %esp, %ebp
	SYSCALL(10);      
	pop %ebp
	ret

ENTRY(getpid)
	push %ebp
	movl %esp, %ebp
	SYSCALL(20)
	pop  %ebp
	ret 

ENTRY(fork)
	push %ebp
	movl %esp, %ebp
	push %ebx
	SYSCALL(2)
	CHECK_ERROR(fork.no_error)
	fork.no_error:
	pop %ebx
	pop %ebp
	ret

ENTRY(clone)
	push %ebp
	movl %esp,% ebp
	push %ebx
	movl 8(%ebp), %ebx
	movl 12(%ebp), %ecx
	SYSCALL(19)
	CHECK_ERROR(clone.no_error)
	clone.no_error:
	pop %ebx
	pop %ebp
	ret

ENTRY(get_stats)
	push %ebp
	movl %esp, %ebp
	push %ebx
	movl 8(%ebp), %ebx
	movl 12(%ebp), %ecx
	SYSCALL(35)
	CHECK_ERROR(get_stats.no_error)
	get_stats.no_error:
	pop %ebx
	pop %ebp
	ret

ENTRY(sem_init)
	push %ebp
	movl %esp, %ebp
	push %ebx
	movl 8(%ebp), %ebx
	movl 12(%ebp), %ecx 
	SYSCALL(21)
	CHECK_ERROR(sem_init.no_error)
	sem_init.no_error:
	pop %ebx
	pop %ebp
	ret

ENTRY(sem_wait)
	push %ebp
	movl %esp, %ebp
	push %ebx
	movl 8(%ebp), %ebx
	SYSCALL(22)
	CHECK_ERROR(sem_wait.no_error)
	sem_wait.no_error:
	pop %ebx
	pop %ebp
	ret 


ENTRY(sem_signal)
	push %ebp
	movl %esp, %ebp
	push %ebx
	movl 8(%ebp), %ebx
	SYSCALL(23)
	CHECK_ERROR(sem_signal.no_error)
	sem_signal.no_error:
	pop %ebx
	pop %ebp
	ret 

ENTRY(sem_destroy)
	push %ebp
	movl %esp, %ebp
	push %ebx
	movl 8(%ebp), %ebx
	SYSCALL(24)
	CHECK_ERROR(sem_destroy.no_error)
	sem_destroy.no_error:
	pop %ebx
	pop %ebp
	ret 

ENTRY(exit)
	push %ebp
	movl %esp, %ebp
	SYSCALL(1)
	pop %ebp
	ret
