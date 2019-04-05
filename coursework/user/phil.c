#include "phil.h"

void print_int(int x) {
  char *r;
  itoa(r, x);
  if (x < 10) write( STDOUT_FILENO, r, 1 );
  else write( STDOUT_FILENO, r, 2 );

  return;
}


void pick_up_chopsticks(int fds[2]){
    pipe_write(fds[1], REQUEST_CHOPS);
    write(STDOUT_FILENO,"PHILOSOPHER ",12);
    print_int(fds[0]/2); 
    write(STDOUT_FILENO," IS WAITING TO PICK UP CHOPSTICKS ",34);
    write(STDOUT_FILENO,"\n",3);
    int read_signal = pipe_read(fds[1],1);
    print_int(read_signal);
    if(read_signal == CAN_PICK_UP){
        write(STDOUT_FILENO,"PHILOSOPHER ",12);
        print_int(fds[0]/2);
        write(STDOUT_FILENO,"PICKED UP CHOPSTICKS\n",26);
        write(STDOUT_FILENO,"\n",3);
    }
    return;
}

void ponder(int fds[2]){
    //add a thinking delay
    write(STDOUT_FILENO,"PHILOSOPHER ",12);
    print_int(fds[0]/2); 
    write(STDOUT_FILENO," IS PONDERING\n",15);
    //write(STDOUT_FILENO,"\n",3);
    for (int i = 0; i < 0x2000000; i++) {
    // thinking delay
    }
    return;
}

void munch(int fds[2]){
    write(STDOUT_FILENO,"PHILOSOPHER ",12);
    print_int(fds[1]/2); 
    write(STDOUT_FILENO," IS MUNCHING\n",14);
    for (int i = 0; i < 0x2000000; i++) {
    // eating delay
    }
    pipe_write(fds[1],MUNCHED);

    return;
}

void main_phil(int fds[2]) {
    
    while(1){
        ponder(fds);
        pick_up_chopsticks(fds);
        munch(fds);
        exit(EXIT_SUCCESS);
    }
   
  exit( EXIT_SUCCESS );
}



