/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#include <errno.h>

#define LECTURA 0
#define ESCRIPTURA 1
int zeos_ticks;

extern struct list_head freequeue, readyqueue;
extern int dir_pages_refs[NR_TASKS];
extern int quantum_left;

unsigned long getEbp();
extern int MAX_PID;

int check_fd(int fd, int permissions)
{
	if (fd!=1) return -EBADF;
	if (permissions!=ESCRIPTURA) return -EACCES;
	return 0;
}

int sys_ni_syscall()
{
	return -38; /*ENOSYS*/
}

int sys_getpid()
{
	return current()->PID;
}

int ret_from_fork(){
	return 0;
}

int sys_fork()
{
	int PID=-1;

	// creates the child process
	if (list_empty(&freequeue)) return -EAGAIN; // NO PROCESS LEFT
	struct list_head *new_task_ptr = list_first(&freequeue);
	list_del(new_task_ptr);

	struct task_struct *new_task = list_head_to_task_struct(new_task_ptr);

	// Copy data from parent to child
	copy_data(current(), new_task,(int) sizeof(union task_union));

	allocate_DIR(new_task);
	page_table_entry * n_pt = get_PT(new_task);
	page_table_entry * c_pt = get_PT(current());

	int page;
	for (page = 0; page < NUM_PAG_KERNEL; page++){
		n_pt[1+page].entry = c_pt[1+page].entry;
	}

	for (page = 0; page < NUM_PAG_CODE; page++){
		n_pt[PAG_LOG_INIT_CODE+page].entry = c_pt[PAG_LOG_INIT_CODE+page].entry;
	}

	int new_frames[NUM_PAG_DATA];
	for(page = 0; page< NUM_PAG_DATA; page++){
		if((new_frames[page] = alloc_frame()) < 0){
			for(page=page-1;page >=0;page--) 
				free_frame(new_frames[page]);
			return -ENOMEM; // no free space left	
		}
		set_ss_pag(n_pt, PAG_LOG_INIT_DATA+page, new_frames[page]);
		set_ss_pag(c_pt, PAG_LOG_INIT_DATA+NUM_PAG_DATA+page, new_frames[page]);
		copy_data( (int*)((PAG_LOG_INIT_DATA+page)<<12),
			   (int*)((PAG_LOG_INIT_DATA+NUM_PAG_DATA+page)<<12),
				PAGE_SIZE
			 );
		del_ss_pag(c_pt, PAG_LOG_INIT_DATA+NUM_PAG_DATA+page);
	}
	set_cr3(get_DIR(current()));
	PID = MAX_PID++;
	new_task->PID = PID;
	new_task->state = ST_READY;
	for (unsigned long * p = (unsigned long *) &(new_task->stats); p != (unsigned long *)(&new_task->stats + sizeof(struct stats)); *(p++) = 0);
	int index  = (getEbp() - (int) current())/sizeof(int);
	((union task_union*)new_task)->stack[index] =(int) ret_from_fork;
	((union task_union*)new_task)->stack[index-1] = 0;
	new_task->kernel_esp= &((union task_union*)new_task)->stack[index-1];

	list_add_tail(new_task_ptr, &readyqueue); 	

	return PID;
}

void sys_exit()
{	
	struct task_struct * a = current();
	a->PID = -1;	
	page_table_entry * pt = get_PT(a);
	int page;
	for(page = 0; page< NUM_PAG_DATA; page++){
		free_frame(pt[PAG_LOG_INIT_DATA+page].bits.pbase_addr);
		del_ss_pag(pt, PAG_LOG_INIT_DATA+page);
	}
	update_process_state_rr(a,&freequeue);
	if(!list_empty(&readyqueue)){
		struct task_struct * ts = list_head_to_task_struct(list_first(&readyqueue));
		update_process_state_rr(ts, NULL);
		task_switch((union task_union*)ts);
	}else{
		task_switch((union task_union*) &idle_task);
	}	
}

#define CHUNK_SIZE 64
int sys_write(int fd, char* buffer, int size){
	int err;
	if((err=check_fd(fd, ESCRIPTURA)) < 0) return err;
	if(buffer == NULL) return -EFAULT;
	if(size < 0) return -EINVAL;
	char dest[CHUNK_SIZE];
	int i = 0;
	int writen = 0;
	while (i < size-CHUNK_SIZE){
		copy_from_user(buffer+i,dest, CHUNK_SIZE);
		writen += sys_write_console(dest, CHUNK_SIZE);
		i+=CHUNK_SIZE;
	}

	copy_from_user(buffer+i,dest, size%CHUNK_SIZE);
	writen += sys_write_console(dest, size%CHUNK_SIZE); 
	return writen;
}

int sys_gettime(){
	return zeos_ticks;
}

int sys_get_stats(int pid, struct stats *st){
	struct task_struct *act;
	int i;
	if(!access_ok(VERIFY_WRITE, st, sizeof(struct stats))) return -EFAULT;
	if (pid < 0) return -EINVAL;
	for (act = &(task[i=0].task); i < NR_TASKS; act = &(task[++i].task)){
		if (act -> PID == pid){
			//*st = (act -> stats);
			act->stats.remaining_ticks = quantum_left;
			copy_to_user(&(act->stats), st, sizeof(struct stats));	
			return 0;
		}
	}	
	return -ESRCH;
}

int sys_clone(void (* function)(void), void *stack){

	int PID=-1;

	// creates the child process
	if (list_empty(&freequeue)) return -EAGAIN; // NO PROCESS LEFT
	struct list_head *new_task_ptr = list_first(&freequeue);
	list_del(new_task_ptr);

	struct task_struct *new_task = list_head_to_task_struct(new_task_ptr);

	// Copy data from parent to child
	copy_data(current(), new_task,(int) sizeof(union task_union));

	int pos = ((int) current() - (int)task) / sizeof(union task_union);

	dir_pages_refs[pos]++; 

	PID = MAX_PID++;
	new_task->PID = PID;
	new_task->state = ST_READY;
	for (unsigned long * p = (unsigned long *) &(new_task->stats); 
			p != (unsigned long *)((&new_task->stats) + 1); *(p++) = 0);

	int index  = (getEbp() - (int) current())/sizeof(int);
	((union task_union*)new_task)->stack[index] =(int) function; // TODO: Arreglar-ho
	((union task_union*)new_task)->stack[index-1] = 0;
	new_task->kernel_esp= &((union task_union*)new_task)->stack[index-1];
	((union task_union*)new_task)->stack[KERNEL_STACK_SIZE-2]=stack;

	list_add_tail(new_task_ptr, &readyqueue); 	

	return PID;
	
}
