/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
*
* Use of this source code is restricted per the CC BY-NC-ND license, a copy of
* which can be found via http://creativecommons.org (and should be included as
* LICENSE.txt within the associated archive or repository).
*/

/*TODO: pretty print processes
 *
 */

#include "hilevel.h"

#define no_processes 32 //including console



//Currently executing program information
int no_processes_running = 0;
int current_executing = 0; // index of current executing PCB


int offset = 0x00001000;
pcb_t pcb[ no_processes ];
pcb_t* current = NULL;

//console may already have the above programs in it
extern void     main_console();
extern uint32_t tos_console;
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
  //update executing status
  prev->status = STATUS_READY;
  next->status = STATUS_EXECUTING;
  return;
}

//GET RID OF THIS
//Returns index of process with highest priority
pcb_t* highest_priority(){
   pcb_t* next = NULL;
   int highest_priority = 0;
   for(int i = 0 ; i <no_processes ; i++){
       int priority = pcb[ i ].age + pcb[ i ].priority;
       if (priority>highest_priority && pcb[i].status != STATUS_TERMINATED) {
           highest_priority = priority;
           next = &pcb[i];
       }
   }
   return next;
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
/*
void dispatch(ctx_t* ctx, int next){
   //Do nothing if the process hasn't changed
   if (pcb[next].pid != pcb[current_executing].pid && pcb[next].status != STATUS_TERMINATED) {
       memcpy( &pcb[current_executing].ctx, ctx, sizeof( ctx_t ) ); // Preserve executing

       if (pcb[current_executing].status == STATUS_EXECUTING) {
           pcb[current_executing].status = STATUS_READY;
       }
       memcpy( ctx, &pcb[next].ctx, sizeof( ctx_t ) );
       pcb[next].status = STATUS_EXECUTING;
       next_pid = ;
       //change this to print in a nicer format each interrupt
       PL011_putc( UART0, '[',      true );
       PL011_putc( UART0, 'P',      true );
       PL011_putc( UART0, '0' + pcb[current_executing].pid, true );
       PL011_putc( UART0, '-',      true );
       PL011_putc( UART0, '>',      true );
       PL011_putc( UART0, '0' + pcb[next].pid, true );
       PL011_putc( UART0, ']',      true );


       current_executing = next;                                 // update index => next
       //pcb[ current_executing ].age = 0;


   }
   return;
}*/

//Round Robin CHANGE THIS
/*void schedule(ctx_t* ctx){
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
}*/

//WORKS!!!
void priority_schedule(ctx_t* ctx) {
  pcb_t* prev = current;
  pcb_t* next = current;

  int highest_priority = 0;
  for(int i=0; i<no_processes; i++){
    if(pcb[i].status != STATUS_TERMINATED){
    //if last executed process
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

   //Assuming arrays are contiguous
   //memset(pcb, 0, no_processes * sizeof(pcb_t));

   TIMER0->Timer1Load  = 0x00100000; // select period = 2^20 ticks ~= 1 sec
   TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
   TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
   TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
   TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

   GICC0->PMR          = 0x000000F0; // unmask all            interrupts
   GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
   GICC0->CTLR         = 0x00000001; // enable GIC interface
   GICD0->CTLR         = 0x00000001; // enable GIC distributor

   //INITIALISE CONSOLE TO BE THE FIRST PROGRAM

   memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );     // initialise 0-th PCB = console
   pcb[ 0 ].pid      = 0;
   pcb[ 0 ].status   = STATUS_READY;
   pcb[ 0 ].ctx.cpsr = 0x50;
   pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_console );
   pcb[ 0 ].ctx.sp   = ( uint32_t )( &tos_stack );
   pcb[ 0 ].priority = 1;
   pcb[ 0 ].age      = 0;

   //instantiate the rest of the pcbs as empty
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

   //EXECUTE CONSOLE
   dispatch(ctx,NULL,&pcb[0]);
   //memcpy(ctx, &pcb[0].ctx, sizeof(ctx_t));
   //pcb[0].status = STATUS_EXECUTING;
    //dont need this
 //no_processes_running = 1;

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

//NEEDED FOR FORK
pcb_t* next_empty_pcb(){
  for(int i=0; i<no_processes; i++){
      if(pcb[i].status == STATUS_TERMINATED) return &pcb[i];
  }
}


//DESCRIPTION FOR FORK DELETE/CHANGE PHRASING LATER
// fork [11, Page 881]:
//  - create new child process with unique PID,
//  - replicate state (e.g., address space) of parent in child,
//  - parent and child both return from fork, and continue to execute after the call point,
//  - return value is 0 for child, and PID of child for parent.

// Put the ctx back into pcb[executing].ctx
// Copy pcb[executing] to pcb[new]
// change pid of pcb[new]
// find top of stack of parent
// find offset of ctx.sp into that stack
// make new stack space for pcb[new]
// copy tos for parent into pcb[new].sp in full
// offset the sp for child by correct amount (same as parent)
//child->status = STATUS_READY;
// give 0 to child.ctx.gpr[0]
// give new pid to parent.ctx.gpr[0]
//Increment no. of processes


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
           case SYS_YIELD : { // 0x00 => yield()
               priority_schedule( ctx );
               break;
              }

           case SYS_WRITE : { // 0x01 => write( fd, x, n )
               int   fd = ( int   )( ctx->gpr[ 0 ] );
               char*  x = ( char* )( ctx->gpr[ 1 ] );
               int    n = ( int   )( ctx->gpr[ 2 ] );
               for( int i = 0; i < n; i++ ) {
                   PL011_putc( UART0, *x++, true );
                  }
               ctx->gpr[ 0 ] = n;
               break;
              }

           case SYS_READ :{ // 0x02 => read( fd, x, n );
               int   fd = ( int   )( ctx->gpr[ 0 ] );
               char*  x = ( char* )( ctx->gpr[ 1 ] );
               int    n = ( int   )( ctx->gpr[ 2 ] );
               for(int i = 0; i < n; i++){
                   x[i] = PL011_getc(UART0,true);
               }
               ctx->gpr [ 0 ] = n;
               break;
            }


           case SYS_FORK : {

               pcb_t* parent = current;
               pcb_t* child = next_empty_pcb();
               copy_pcb(parent, child, ctx);
               ctx->gpr[0] = child->pid; //return pid of child for parent
               child->ctx.gpr[0] = 0; // return value for child is 0

               break;
             }

           //P5 should terminate after 25 prints -- it does
           case SYS_EXIT :{
               terminate_process(current,ctx);
               break;
             }
           //replace current process image(text segment) with new process image -> THIS BASICALLY MEANS
           //EXECUTE A NEW PROGRAM
           //reset stack pointer(e.g state)
           //then continue to execute at the entry point of new program
           case SYS_EXEC :{
               PL011_putc( UART0, 'E', true );
               //memset((void*)(allocate_stack(current_executing)-offset), 0, offset);
               ctx->pc = ctx->gpr[0];
               //ctx->sp = allocate_stack(current_executing);
               break;
           }

           case SYS_KILL :{
               pid_t pid = ctx->gpr[0];
               int x     = ctx->gpr[1];
               pcb_t* target = NULL;
               for(int i=0; i<no_processes;i++){
                   if(pid == pcb[i].pid ) target = &pcb[i];
               }
               if(target!=NULL && target->status != STATUS_TERMINATED) terminate_process(target,ctx);
               break;
           }
           case SYS_NICE :{
               int pid = ctx->gpr[0];
               pcb[pid].priority = ctx->gpr[1];

           }

           default   : { // 0x?? => unknown/unsupported
               break;
              }
     }

   return;
}
