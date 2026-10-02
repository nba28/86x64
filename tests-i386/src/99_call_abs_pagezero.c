/* 99_call_abs_pagezero — `call *[abs32]` into page zero is a compiler-folded
 * call through a NULL object (Portal 2 client/server `movl $0,(%esp);
 * call *0x10` on an error path). Nothing to relocate, not a dyld slot:
 * translation used to ABORT on the width guard. It must translate to an
 * ABSOLUTE load (the same bytes are rip-relative in x86_64, which would jump
 * through code bytes) that faults at address 0x10 exactly as i386 does.
 * call_abs_pagezero guard: the fault reporter must say `fault addr=0x10`. */
#include <stdlib.h>

int main(void) {
   __asm__ volatile("pushl $0\n\tcall *0x10\n\taddl $4, %%esp" ::: "memory");
   exit(2);
}
