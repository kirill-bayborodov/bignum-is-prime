; @file bignum_is_prime.asm
; @brief x86-64 System V implementation of bignum_is_prime.
; @details
; Boundary contract: bignum_t is 32 little-endian uint64_t words at offsets
; 0..248 followed by a size_t len at offset 256. The borrowed input at rdi is
; never modified; rsi is a positive rounds count; rdx is caller-allocated int*
; output and receives 0 or 1 only on success. The function returns 0 on success,
; -1 for NULL pointers, -2 for len > 32, and -3 for zero rounds. No allocation
; or ownership transfer occurs. The implementation performs parity and
; streaming small-divisor checks for multiword operands; it is a fast-path
; candidate and must not be described as a complete multiword Miller--Rabin
; proof until its modular exponentiation path is implemented.
;
; ABI: System V AMD64 arguments are rdi/rsi/rdx and rax carries the status.
; r12 and r13 are callee-saved and are pushed/popped; rbx, rbp, r14 and r15
; are not used. rax, rcx, r8-r11 and flags are caller-clobbered. There are no
; calls, so no call-site stack alignment is required; the two pushes preserve
; the return stack and are balanced on every post-prologue path. All arithmetic
; temporaries are registers, and the output pointer is preserved in r10 across
; DIV, which clobbers rax/rdx. Complexity is O(rounds-independent * n * 6)
; for the multiword small-divisor gate, where n is word count; space is O(1).
; The C11 implementation remains the correctness reference and differential
; testing oracle.

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
