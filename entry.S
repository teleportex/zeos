/*
 * entry.S - Entry point to system mode from user mode
 */

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

#define EOI \
	movb $0x20, %al; \
	outb %al, $0x20;

ENTRY(writeMsr)
	push %ebp
	movl %esp, %ebp
	movl 8(%ebp), %ecx
	movl 12(%ebp), %eax
	movl $0, %edx
	wrmsr
	pop %ebp
	ret

ENTRY(keyboard_handler)
	SAVE_ALL;
	EOI;
	call update_entry_system;
	call keyboard_routine;
	call update_leave_system;
	RESTORE_ALL;
	iret;

ENTRY(system_call_handler)
	SAVE_ALL;
	cmpl $0, %eax;
	jl error;
	cmpl $MAX_SYSCALL, %eax;
	jg error;
	call *sys_call_table(,%eax,4);
	jmp fin;
error:
	movl $-38, %eax;
fin:	movl %eax, 18(%esp);
	RESTORE_ALL;
	iret;

ENTRY(syscall_handler_sysenter)
	push $__USER_DS;
	push %ebp;
	pushfl
	push $__USER_CS
	push 4(%ebp)
	SAVE_ALL	
	push %eax
	call update_entry_system
	pop %eax
	cmpl $0, %eax
	jl sysenter_error
	cmpl $MAX_SYSCALL, %eax
	jg sysenter_error
	call *sys_call_table(,%eax,0x04)
	jmp sysenter_fin
sysenter_error:
	movl $-38, %eax
sysenter_fin:
	movl %eax, 0x18(%esp)
	call update_leave_system;
	RESTORE_ALL
	movl (%esp), %edx
	movl 12(%esp), %ecx
	sti
	sysexit

ENTRY(clock_handler)
	SAVE_ALL;
	EOI;
	call update_entry_system;
	call clock_routine;
	call update_leave_system;
	RESTORE_ALL;
	iret;

ENTRY(task_switch)
	pushl %ebp;
	movl  %esp, %ebp;
	pushl %esi;
	pushl %edi;
	pushl %ebx;
	pushl 8(%ebp);
	call inner_task_switch;
	addl  $4, %esp;
	popl  %ebx;
	popl  %edi;
	popl  %esi;
	movl  %ebp, %esp;
	popl  %ebp;
	ret;

ENTRY(setEsp)
	movl 4(%esp), %esp
	popl %ebp;
	ret;

ENTRY(getEbp)
	movl %ebp, %eax
	ret;

