.text
.globl main

main:
    li t0, 0x10000000
    li t1, 0x10100000
    li t2, 0x12345678

loop:
    sw t2, 0(t0)
    addi t0, t0, 4
    bltu t0, t1, loop

    li a7, 10
    ecall
