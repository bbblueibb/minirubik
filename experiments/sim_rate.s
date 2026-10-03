.text
.globl main

main:
    li t0, 300000
    li t1, 0x10000000
    li t2, 0x12345678

loop:
    sw t2, 0(t1)
    addi t0, t0, -1
    bnez t0, loop

    li a7, 10
    ecall
