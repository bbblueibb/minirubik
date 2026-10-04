.section .bss
.align 4

ref_stack:
    .space 4096
ref_stack_top:

.section .text
.globl _start

_start:
    .option push
    .option norelax
    la gp, __global_pointer$
    la sp, ref_stack_top
    .option pop

    call main

    li a7, 10
    ecall
