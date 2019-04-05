/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
*
* Use of this source code is restricted per the CC BY-NC-ND license, a copy of
* which can be found via http://creativecommons.org (and should be included as
* LICENSE.txt within the associated archive or repository).
*/


#include "hilevel.h"
#define no_processes (32) //including console
#include "phil.h"

int offset = 0x00001000;
pcb_t pcb[ no_processes ];
pcb_t* current = NULL;

pipe_t pipes[no_processes];


extern void     main_console();
// general stack for processes
extern uint32_t tos_stack;


void c_print(char* x,int n){
   int i=0;
   while(i<n){
       PL011_putc( UART0, x[i], true );
       i++;
 }
}



void dispatch( ctx_t* ctx, pcb_t* prev, pcb_t* next ) {
  if( NULL != prev ) {
    memcpy( &prev->ctx, ctx, sizeof( ctx_t ) ); // preserve execution context of P_{prev}
  }
  if( NULL != next ) {
    memcpy( ctx, &next->ctx, sizeof( ctx_t ) ); // restore  execution context of P_{next}
  }
  current = next;                             // update   executing index   to P_{next}
  PL011_putc( UART0, '[', true );
  PL011_putc( UART0, '0' + prev->pid, true );
  PL011_putc( UART0, '-', true );
  PL011_putc( UART0, '>', true );
  PL011_putc( UART0, '0' + next->pid, true );
  PL011_putc( UART0, ']', true );

  if (prev->status == STATUS_EXECUTING) prev->status = STATUS_READY;
  next->status = STATUS_EXECUTING;
  return;
}

//Increases age of non executing functions
void increase_age(pcb_t* next){
   next->age = 0; //reset the age of an executing process
   for(int process = 0; process<no_processes ; process++){
       if((pcb[process].pid != next->pid) && pcb[process].status != STATUS_TERMINATED)
           pcb[process].age++;
   }
   return;
}

//Round Robin
void schedule(ctx_t* ctx){
   for (size_t i = 0 ; i < no_processes ; i ++){
       if( current->pid == pcb[ i ].pid ){
           int next_process = (i+1)%no_processes;
           //CHANGE THIS TO switch_process
           dispatch( ctx, &pcb[ i ], &pcb[ next_process ] ); // context switch to from P_i to P_i+1
           pcb[ i ].status = STATUS_READY;
           pcb[  next_process ].status = STATUS_EXECUTING;        // updating execution status
           break;
       }
   }
   return;
}

void priority_schedule(ctx_t* ctx) {
  pcb_t* prev = current;
  pcb_t* next = current;

  int highest_priority = 0;
  for(int i=0; i<no_processes; i++){
    if(pcb[i].status != STATUS_TERMINATED){
      if(current->pid == pcb[i].pid){
        prev = &pcb[i];
      }

      int priority = pcb[i].age + pcb[i].priority;
      if(highest_priority <= priority){
        next = &pcb[i];
        highest_priority = priority;
      }
    }
  }
  increase_age(next);
  dispatch( ctx, prev, next);

}

//allocates stack region to a process given a pid
uint32_t allocate_stack(pid_t pid){
   uint32_t top = (uint32_t) (&tos_stack);
   uint32_t process_stack = top - (offset * pid);
   return process_stack;
}


void hilevel_handler_rst(ctx_t* ctx) {

   TIMER0->Timer1Load  = 0x00100000; // select period = 2^20 ticks ~= 1 sec
   TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
   TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
   TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
   TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

   GICC0->PMR          = 0x000000F0; // unmask all            interrupts
   GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
   GICC0->CTLR         = 0x00000001; // enable GIC interface
   GICD0->CTLR         = 0x00000001; // enable GIC distributor

   //initialise console to be first program
   memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );     // initialise 0-th PCB = console
   pcb[ 0 ].pid      = 0;
   pcb[ 0 ].status   = STATUS_READY;
   pcb[ 0 ].ctx.cpsr = 0x50;
   pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_console );
   pcb[ 0 ].ctx.sp   = ( uint32_t )( &tos_stack );
   pcb[ 0 ].priority = 1;
   pcb[ 0 ].age      = 0;

   //rest of pcbs empty
   for(int i=1; i<no_processes; i++){
       memset( &pcb[ i ], 0, sizeof( pcb_t ) );
       pcb[ i ].pid      = i;
       pcb[ i ].status   = STATUS_TERMINATED;
       pcb[ i ].ctx.cpsr = 0x50;
       pcb[ i ].ctx.pc   = ( uint32_t )( &main_console );
       pcb[ i ].ctx.sp   = allocate_stack(i);
       pcb[ i ].priority = 1;
       pcb[ i ].age      = 0;
    }
    //initialise pipes
    for (int i=0; i<no_processes; i++){
        pipes[ i ].message = EMPTY;
        pipes[ i ].free    = true;
        pipes[ i ].written = false;
        pipes[ i ].closed  = false;
    }

   //EXECUTE CONSOLE
   dispatch(ctx,NULL,&pcb[0]);
   int_enable_irq();

   return;
}

void hilevel_handler_irq(ctx_t* ctx) {
   /* Based on the identifier (i.e., the immediate operand) extracted from the
  * svc instruction,
  *
  * - read  the arguments from preserved usr mode registers,
  * - perform whatever is appropriate for this system call, then
  * - write any return value back to preserved usr mode registers.
  */


  // Step 2: read  the interrupt identifier so we know the source.

  uint32_t id = GICC0->IAR;

  // Step 4: handle the interrupt, then clear (or reset) the source.

  if( id == GIC_SOURCE_TIMER0 ) {
    priority_schedule( ctx );
    TIMER0->Timer1IntClr = 0x01;
  }

  // Step 5: write the interrupt identifier to signal we're done.

  GICC0->EOIR = id;

  return;

}

pcb_t* next_empty_pcb(){
  for(int i=0; i<no_processes; i++){
      if(pcb[i].status == STATUS_TERMINATED) return &pcb[i];
  }
}

//called by a process on itself
void terminate_process(pcb_t* target, ctx_t* ctx){
   target->status   = STATUS_TERMINATED;
   target->age      = 0;
   target->priority = 0; //may not need to do this since its a terminated process
   priority_schedule(ctx);
}

//Helper for fork
void copy_pcb(pcb_t* parent_process, pcb_t* child_process , ctx_t* ctx){
   //memcpy(child_process, parent_process, sizeof(pcb_t));
   memcpy(&child_process->ctx, ctx, sizeof(ctx_t));
   child_process->status = STATUS_READY;
   child_process->age    = 0;
   child_process->priority = parent_process->priority;
   uint32_t stack_offset = allocate_stack(parent_process->pid) - ctx->sp;
   child_process->ctx.sp     = allocate_stack(child_process->pid) - stack_offset;
   //better to do it with stack pointers because of problem when subtracting by offset
   memcpy((void*)(child_process->ctx.sp), (void*) (ctx->sp), stack_offset);
}

//SYSTEM CALLS
void hilevel_handler_svc( ctx_t* ctx, uint32_t id ) {


 /* Based on the identifier (i.e., the immediate operand) extracted from the
  * svc instruction,
  *
  * - read  the arguments from preserved usr mode registers,
  * - perform whatever is appropriate for this system call, then
  * - write any return value back to preserved usr mode registers.
  */

   switch( id ) {
           case 0x00 : { // 0x00 => yield()
               priority_schedule( ctx );
               break;
              }

           case 0x01 : { // 0x01 => write( fd, x, n )
               int   fd = ( int   )( ctx->gpr[ 0 ] );
               char*  x = ( char* )( ctx->gpr[ 1 ] );
               int    n = ( int   )( ctx->gpr[ 2 ] );
               for( int i = 0; i < n; i++ ) {
                   PL011_putc( UART0, *x++, true );
                  }
               ctx->gpr[ 0 ] = n;
               break;
              }

           case 0x02 :{ // 0x02 => read( fd, x, n );
               int   fd = ( int   )( ctx->gpr[ 0 ] );
               char*  x = ( char* )( ctx->gpr[ 1 ] );
               int    n = ( int   )( ctx->gpr[ 2 ] );
               for(int i = 0; i < n; i++){
                   x[i] = PL011_getc(UART0,true);
               }
               ctx->gpr [ 0 ] = n;
               break;
            }


           case 0x03 : { // fork
               pcb_t* parent = current;
               pcb_t* child = next_empty_pcb();
               copy_pcb(parent, child, ctx);
               ctx->gpr[0] = child->pid; //return pid of child for parent
               child->ctx.gpr[0] = 0; // return value for child is 0

               break;
             }

           case 0x04 :{ //exit
               terminate_process(current,ctx);
               break;
             }

           case 0x05 :{ //exec
               PL011_putc( UART0, 'E', true );
               ctx->pc = ctx->gpr[0];
               ctx->sp = allocate_stack(current->pid);
               break;
           }

           case 0x06 :{ // kill
               pid_t pid = ctx->gpr[0];
               int x     = ctx->gpr[1];
               pcb_t* target = NULL;
               for(int i=0; i<no_processes;i++){
                   if(pid == pcb[i].pid ) target = &pcb[i];
               }
               if(target!=NULL && target->status != STATUS_TERMINATED) terminate_process(target,ctx);
               break;
           }
           case 0x7 :{ //nice
               int index = 0;
               pid_t pid = ctx->gpr[0];
               char print = '0' + pid;
               c_print(&print,2);
               int x = (int) (ctx->gpr[1]);
               char print2 = '0' + x;
               c_print(&print2,3);
               for (int i=0; i<no_processes; i++){
                   if(pid == pcb[ i ].pid) index = i;
               }
               pcb[index].priority = x;
               priority_schedule(ctx);

           }
           case 0x08 : { //kill all
               for(int i=1; i<no_processes ; i++){
                   pcb[ i ].status = STATUS_TERMINATED;
                   pcb[ i ].priority = 0;
                   pcb[ i ].age = 0;
               }
               break;
           }
           case 0x09 : { //pipe
               int* fds = (int*)(ctx->gpr[0]);
		       fds[0] = -1;
               fds[1] = -1;
               bool first = false;
               for(int i=0; i<no_processes; i++){
                   if(pipes[i].free){
                       if(!first){
						   //first free
                           fds[0] = i;
                           pipes[i].free = false;
                           first = true;
                       }
                       else {
						   //2nd free
                           fds[1] = i;
                           pipes[i].free = false;
                           break;
                       }
                   }
               }
               break;

           }
           case 0xA : { //pipe_write
               int fd  = (int)(ctx->gpr[ 0 ]);
               if(!pipes[fd].closed && !pipes[fd].written){
                   pipes[fd].message = (int) (ctx->gpr[1]);
                   if(pipes[fd].message == EMPTY) pipes[fd].written = false;
                   else pipes[fd].written = true;
               }
               break;
           }
           case 0xB : { //pipe_read
               int fd  = (int)(ctx->gpr[ 0 ]);
               int delete    = (int)(ctx->gpr[1]);
               if((!pipes[fd].closed) && pipes[fd].written){
                   PL011_putc( UART0, 'R', true );
				   //writing is correct
                   ctx->gpr[0] = pipes[fd].message;
                   if(delete == 1) pipes[fd].written = false;
               }
               else{
				   ctx->gpr[0] = EMPTY;
			   }

               break;
           }
           case 0xC : { //pipe_close
               int fd = ctx->gpr[0];
			   pipes[fd].closed  = true;
			   pipes[fd].message = EMPTY;
			   pipes[fd].free    = true;
               break;


           }

           default   : { // 0x?? => unknown/unsupported
               break;
              }
     }

   return;
}
