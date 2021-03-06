/*
 * interrupt.c -
 */
#include <types.h>
#include <interrupt.h>
#include <segment.h>
#include <hardware.h>
#include <io.h>
#include <zeos_interrupt.h>
#include <sched.h>
#include <cbuffer.h>
    
extern int zeos_ticks;
extern union task_union *task; /* Vector de tasques */
extern struct list_head readyqueue, readqueue;

Gate idt[IDT_ENTRIES];
Register    idtR;
char char_map[] =
{
  '\0','\0','1','2','3','4','5','6',
  '7','8','9','0','\'','¡','\0','\0',
  'q','w','e','r','t','y','u','i',
  'o','p','`','+','\0','\0','a','s',
  'd','f','g','h','j','k','l','ñ',
  '\0','º','\0','ç','z','x','c','v',
  'b','n','m',',','.','-','\0','*',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0','\0','\0','\0','\0','\0','7',
  '8','9','-','4','5','6','+','1',
  '2','3','0','\0','\0','\0','<','\0',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0'
};

void setInterruptHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE INTERRUPTION GATE FLAGS:                          R1: pg. 5-11  */
  /* ***************************                                         */
  /* flags = x xx 0x110 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */
    
  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}

void setTrapHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE TRAP GATE FLAGS:                                  R1: pg. 5-11  */
  /* ********************                                                */
  /* flags = x xx 0x111 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);

  //flags |= 0x8F00;    /* P = 1, D = 1, Type = 1111 (Trap Gate) */
  /* Changed to 0x8e00 to convert it to an 'interrupt gate' and so
     the system calls will be thread-safe. */
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}

struct cbuffer read_buffer;

void keyboard_handler();
void clock_handler();
void system_call_handler();
void writeMsr(int msr, int data);
void syscall_handler_sysenter();
void setIdt()
{
  /* Program interrups/exception service routines */
  idtR.base  = (DWord)idt;
  idtR.limit = IDT_ENTRIES * sizeof(Gate) - 1;

  set_handlers();

  /* ADD INITIALIZATION CODE FOR INTERRUPT VECTOR */
  writeMsr(0x174,__KERNEL_CS);
  writeMsr(0x175,INITIAL_ESP);
  writeMsr(0x176,(int)syscall_handler_sysenter);
  setInterruptHandler(33, keyboard_handler, 0);
  setInterruptHandler(32, clock_handler,    0);
  setTrapHandler(0x80, system_call_handler, 3);

  set_idt_reg(&idtR);
}

void keyboard_routine()
{
  unsigned char llegit = inb(0x60);
  if (!(llegit & 0x80))
  {
    if((llegit & 0x7F) < sizeof(char_map)/sizeof(char) && char_map[llegit & 0x7F] != '\0'){
	printc_xy(0, 0,char_map[llegit & 0x7F]);
        if (!cbuffer_full(&read_buffer)){
            cbuffer_push(&read_buffer, char_map[llegit & 0x7F]);

            if(!list_empty(&readqueue)){
		struct list_head* first = list_first(&readqueue);
		struct task_struct* firstt = list_head_to_task_struct(first);

                update_process_state_rr(firstt, &readyqueue);
            }
        }
    }
  }
}

void clock_routine(){
  zeos_show_clock();
  zeos_ticks++;
  update_sched_data_rr();
  if(needs_sched_rr()){
	if(current()->PID)
		enqueue_current(&readyqueue);
	sched_next_rr();
 }
}
