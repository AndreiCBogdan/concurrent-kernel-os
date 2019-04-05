#ifndef __WAITER_H
#define __WAITER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>


#include <phil.h>

typedef enum {
    AVAILABLE,
    TAKEN
        
}  chopstick_t;


void manage();

void main_waiter();


#endif