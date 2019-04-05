#ifndef __PHIL_H
#define __PHIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "libc.h"

extern void print_int(int x);

extern void pick_up_chopsticks(int fds[2]);

extern void munch();

extern void ponder();

extern void main_phil();
#endif