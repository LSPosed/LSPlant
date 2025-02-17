    .text
    .global lsplant_get_trampoline_address
    .hidden lsplant_get_trampoline_address
    .type   lsplant_get_trampoline_address, @function
lsplant_get_trampoline_address:
// void* (*)(const ClassLinker* class_linker, bool (*checker)(const ClassLinker*, const void*))
    .cfi_startproc

#if defined(__riscv)

    addi    sp, sp, -88
    sd      ra, 80(sp)
    sd      s0, 72(sp)
    sd      s1, 64(sp)
    sd      s2, 56(sp)

    mv      s0, a0  // class_linker
    mv      s1, a1  // checker
    jalr    s1

    sd      a1, 40(sp)
    sd      a2, 48(sp)  // It might be this register
    sd      a3, 32(sp)
    sd      a4, 24(sp)
    sd      a5, 16(sp)
    sd      a6, 8(sp)
    sd      a7, 0(sp)

    addi    s2, sp, 48
.L.loop:
    mv      a0, s0
    ld      a1, 0(s2)
    beqz    a1, .L.continue
    jalr    s1
    bnez    a0, .L.found
    beq     s2, sp, .L.break
.L.continue:
    addi    s2, s2, -8
    j       .L.loop
.L.break:
    li      a0, 0
    j       .L.pop

.L.found:
    ld      a0, 0(s2)

.L.pop:
    ld      ra, 80(sp)
    ld      s0, 72(sp)
    ld      s1, 64(sp)
    ld      s2, 56(sp)
    addi    sp, sp, 88
    ret

#elif defined(__aarch64__)

    sub     sp, sp, #112
    stp     fp, lr, [sp, #64]
    stp     x21, x22, [sp, #80]
    stp     x19, x20, [sp, #96]
    add     fp, sp, #64

    mov     x19, x0  // class_linker
    mov     x20, x1  // checker
    blr     x20

    stp     x8, x9, [sp, #48]  // It might be x9 register
    stp     x10, x11, [sp, #32]
    stp     x12, x13, [sp, #16]
    stp     x14, x15, [sp, #0]

    add     x21, sp, #56
    mov     x22, sp
.L.loop:
    mov     x0, x19
    ldr     x1, [x21, #0]
    cbz     x1, .L.continue
    blr     x20
    cbnz    x0, .L.found
    cmp     x21, x22
    b.eq    .L.break
.L.continue:
    sub     x21, x21, #8
    b       .L.loop
.L.break:
    mov     x0, xzr
    b       .L.pop

.L.found:
    ldr     x0, [x21, #0]

.L.pop:
    ldp     x19, x20, [sp, #96]
    ldp     x21, x22, [sp, #80]
    ldp     fp, lr, [sp, #64]
    add     sp, sp, #112
    ret

#else
#error unsupported
#endif

    .cfi_endproc
    .size   lsplant_get_trampoline_address, .-lsplant_get_trampoline_address
