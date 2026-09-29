.section .text.boot
.global _start

_start:
    ldr sp, =__stack_top
    bl  bss_clear
    bl  main
hang:
    b   hang

bss_clear:
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
1:
    cmp r0, r1
    bge 2f
    str r2, [r0], #4
    b   1b
2:
    bx  lr
