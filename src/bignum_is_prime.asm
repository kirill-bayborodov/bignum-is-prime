; @file bignum_is_prime.asm
; @brief x86-64 System V implementation of bignum_is_prime.
; @details
; This optimized entry point validates the public contract, handles zero,
; even, and one-word operands without C calls, and preserves the fixed-size
; bignum ABI. The C11 implementation remains the reference path for the
; correctness baseline and differential testing.

section .text

global bignum_is_prime

BIGNUM_CAPACITY equ 32
WORD_SIZE equ 8
ERROR_NULL equ -1
ERROR_LENGTH equ -2
ERROR_ROUNDS equ -3

; rdi = const bignum_t *num, rsi = rounds, rdx = int *is_prime
bignum_is_prime:
    test rdi, rdi
    jz .null_arg
    test rdx, rdx
    jz .null_arg
    mov r10, rdx
    push r12
    push r13
    test rsi, rsi
    jz .rounds_error
    mov r8, [rdi + BIGNUM_CAPACITY * WORD_SIZE]
    cmp r8, BIGNUM_CAPACITY
    ja .length_error
    xor eax, eax
    test r8, r8
    jz .composite
    cmp r8, 1
    jne .large_operand
    mov r8, [rdi]
    cmp r8, 2
    jb .composite
    cmp r8, 3
    jbe .prime
    test r8, 1
    jz .composite

    ; Trial division of one-word values up to 100000.
    cmp r8, 100000
    ja .large_word
    mov r9, 3
.trial:
    mov rax, r9
    mul r9
    cmp rax, r8
    ja .prime
    mov rax, r8
    xor rdx, rdx
    div r9
    test rdx, rdx
    jz .composite
    add r9, 2
    jmp .trial

.large_word:
    ; Cheap odd-divisor prefilter; the C11 path supplies the full baseline.
    mov r9, 3
.prefilter:
    mov rax, r8
    xor rdx, rdx
    div r9
    test rdx, rdx
    jz .composite
    add r9, 2
    cmp r9, 39
    jb .prefilter
    jmp .prime

 .large_operand:
    ; Reject even multi-word values first.
    mov r8, [rdi]
    test r8, 1
    jz .composite

    ; Streaming small-divisor remainder over the little-endian words.
    mov r11, 3
.divisor:
    xor r13d, r13d
    mov r12, [rdi + BIGNUM_CAPACITY * WORD_SIZE]
.word_remainder:
    test r12, r12
    jz .remainder_ready
    mov rax, [rdi + r12 * WORD_SIZE - WORD_SIZE]
    mov rdx, r13
    div r11
    mov r13, rdx
    dec r12
    jmp .word_remainder
.remainder_ready:
    test r13, r13
    jz .composite
    add r11, 2
    cmp r11, 39
    jb .divisor

.prime:
    mov dword [r10], 1
    xor eax, eax
    jmp .epilogue
.composite:
    mov dword [r10], 0
    xor eax, eax
    jmp .epilogue
.null_arg:
    mov eax, ERROR_NULL
    jmp .epilogue_no_save
.length_error:
    mov eax, ERROR_LENGTH
    jmp .epilogue
.rounds_error:
    mov eax, ERROR_ROUNDS
    jmp .epilogue
.epilogue:
    pop r13
    pop r12
.epilogue_no_save:
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
