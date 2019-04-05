#include <waiter.h>

#define no_phils 16
int filedes[no_phils][2];
chopstick_t chopsticks[no_phils];


void manage(){
    for (int i=0; i<no_phils; i++){
        chopsticks[i] = AVAILABLE;
    }
    while(1){
        for (int i=0; i<no_phils; i++){
            int signal = pipe_read(filedes[i][1],0);
            if(signal == REQUEST_CHOPS){
                if(chopsticks[i] == AVAILABLE && chopsticks[(i+1) % no_phils] == AVAILABLE){
                    signal = pipe_read(filedes[i][1],1);
                    pipe_write(filedes[i][0], CAN_PICK_UP);
                    write(STDOUT_FILENO,"PHILOSOPHER",11);
                    print_int(i);
                    write(STDOUT_FILENO," HAS PICKED UP THE CHOPSTICKS\n",29);
                
                    chopsticks[i] = TAKEN;
                    chopsticks[(i+1+no_phils) % no_phils] = TAKEN;
                }
            }
            if (signal == MUNCHED){
                signal = pipe_read(filedes[i][1],1);
                if(chopsticks[i] == TAKEN) chopsticks[i] = AVAILABLE;
                if(chopsticks[(i + 1 + no_phils) % no_phils] == TAKEN)
                    chopsticks[(i + 1 + no_phils) % no_phils] = AVAILABLE;
            }
        }
    }
     exit(EXIT_SUCCESS);
}
    
       

void main_waiter(){
    for(int i=0; i<no_phils; i++){
        int pipe_signal = pipe(filedes[i]);
        if (pipe_signal == -1){
            write(STDOUT_FILENO,"ERROR CREATING PIPE",23);
            exit(EXIT_FAILURE);
        }
        else{
            int pid = fork();
            switch(pid){
                case -1:{
                    break;
                }
                case 0:{
                    main_phil(filedes[i]);
                } 
                default:{
                    if(i == no_phils -1) manage();
                    break;
                }
            }
        }
    }
}
