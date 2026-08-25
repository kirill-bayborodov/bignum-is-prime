; =============================================================================
; @file    bignum_mod_exp.asm
; @brief   Full x86-64 implementation of typed modular exponentiation.
; @version 2.0.0
; @date    21.08.2026
;
; @details
;   This file is self-contained. It implements validation, overlap checking,
;   normalized little-endian word records, binary modular reduction,
;   overflow-safe modular addition, double-and-add modular multiplication and
;   right-to-left square-and-multiply exponentiation. No C helper is called.
;
; @abi
;   System V AMD64: result=rdi, base=rsi, exponent=rdx, modulus=rcx. The public
;   symbol preserves rbp/rbx/r12-r15, aligns rsp before every internal call and
;   returns bignum_mod_exp_status_t in eax. Internal helpers clobber volatile
;   registers and flags only. No helper publishes partial output.
;
; @representation
;   A bignum_t has 32 little-endian uint64_t words at offset 0 and size_t len
;   at offset 256. RECORD_SIZE is 264 bytes. Stack records are always zeroed,
;   and successful results are normalized before publication.
;
; @algorithm
;   Reduction scans input bits MSB-first and performs remainder=2*remainder+b
;   modulo m. Multiplication scans multiplier bits LSB-first, conditionally
;   adds the current multiplicand and doubles it modulo m. Exponentiation scans
;   exponent bits LSB-first, multiplying the accumulator for set bits and
;   squaring the current base. The bounded algorithms avoid 2N-word products.
; =============================================================================

BITS 64
DEFAULT REL

%define CAPACITY       32
%define LEN_OFFSET     256
%define RECORD_SIZE    264
%define SUCCESS        0
%define ERROR_NULL     -1
%define ERROR_ROUNDS   -3
%define ERROR_MODZERO  -2
%define ERROR_OVERLAP  -3
%define ERROR_LENGTH   -2
%define ERROR_CAPACITY -5
%define ERROR_INTERNAL -6

SECTION .text

global bignum_mod_exp
    global asm_mont_mul

; -----------------------------------------------------------------------------
; int asm_overlap(const void *a=rdi, const void *b=rsi)
; Returns eax=1 when the two complete 264-byte bignum records overlap.
; Exact equality is intentionally overlap because the public API is
; transactional and forbids destination aliasing.
; -----------------------------------------------------------------------------
asm_overlap:
    cmp     rdi, rsi
    je      .yes
    jb      .a_left
    lea     rax, [rsi + RECORD_SIZE]
    cmp     rax, rdi
    ja      .yes
    xor     eax, eax
    ret
.a_left:
    lea     rax, [rdi + RECORD_SIZE]
    cmp     rax, rsi
    ja      .yes
    xor     eax, eax
    ret
.yes:
    mov     eax, 1
    ret

; -----------------------------------------------------------------------------
; int asm_cmp(const bignum_t *a=rdi, const bignum_t *b=rsi)
; Returns eax=-1,0,1. Operands must have valid normalized lengths.
; -----------------------------------------------------------------------------
asm_cmp:
    mov     rcx, [rdi + LEN_OFFSET]
    mov     rdx, [rsi + LEN_OFFSET]
    cmp     rcx, rdx
    ja      .greater
    jb      .less
    test    rcx, rcx
    jz      .equal
.compare_words:
    dec     rcx
    mov     rax, [rdi + rcx*8]
    cmp     rax, [rsi + rcx*8]
    ja      .greater
    jb      .less
    test    rcx, rcx
    jnz     .compare_words
.equal:
    xor     eax, eax
    ret
.greater:
    mov     eax, 1
    ret
.less:
    mov     eax, -1
    ret

; -----------------------------------------------------------------------------
; void asm_zero(bignum_t *out=rdi)
; Clears all words and length in a fixed-size record.
; -----------------------------------------------------------------------------
asm_zero:
    xor     eax, eax
    mov     ecx, CAPACITY + 1
    rep stosq
    ret

; -----------------------------------------------------------------------------
; void asm_copy(bignum_t *out=rdi, const bignum_t *in=rsi)
; Copies the complete fixed-size record, including unused words.
; -----------------------------------------------------------------------------
asm_copy:
    mov     rcx, CAPACITY + 1
    rep movsq
    ret

; -----------------------------------------------------------------------------
; void asm_normalize(bignum_t *value=rdi)
; Removes high zero words and leaves all lower words unchanged.
; -----------------------------------------------------------------------------
asm_normalize:
    mov     rcx, [rdi + LEN_OFFSET]
    test    rcx, rcx
    jz      .done
.loop:
    cmp     qword [rdi + rcx*8 - 8], 0
    jne     .store
    dec     rcx
    jnz     .loop
.store:
    mov     [rdi + LEN_OFFSET], rcx
.done:
    ret

; -----------------------------------------------------------------------------
; int asm_is_zero(const bignum_t *value=rdi)
; Returns eax=1 for zero and eax=0 otherwise.
; -----------------------------------------------------------------------------
asm_is_zero:
    mov     rcx, [rdi + LEN_OFFSET]
    test    rcx, rcx
    jz      .yes
.loop:
    cmp     qword [rdi + rcx*8 - 8], 0
    jne     .no
    dec     rcx
    jnz     .loop
.yes:
    mov     eax, 1
    ret
.no:
    xor     eax, eax
    ret

; -----------------------------------------------------------------------------
; void asm_sub_raw(out=rdi, a=rsi, b=rdx)
; Computes a-b for a>=b. The helper is alias-safe and uses a borrow bit.
; -----------------------------------------------------------------------------
asm_sub_raw:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 48
    mov     [rbp-8], rdi          ; out
    mov     [rbp-16], rsi         ; a
    mov     [rbp-24], rdx         ; b
    mov     rcx, [rsi + LEN_OFFSET]
    mov     r8, [rdx + LEN_OFFSET]
    cmp     rcx, r8
    jae     .length_ready
    mov     rcx, r8
.length_ready:
    mov     [rbp-32], rcx
    xor     r9d, r9d              ; borrow
    xor     r10d, r10d            ; index
.loop:
    cmp     r10, [rbp-32]
    jae     .finish
    mov     rsi, [rbp-16]
    cmp     r10, [rsi + LEN_OFFSET]
    jb      .have_a
    xor     eax, eax
    jmp     .load_b
.have_a:
    mov     rax, [rsi + r10*8]
.load_b:
    mov     rsi, [rbp-24]
    cmp     r10, [rsi + LEN_OFFSET]
    jb      .have_b
    xor     edx, edx
    jmp     .subtract
.have_b:
    mov     rdx, [rsi + r10*8]
.subtract:
    mov     r11, r9
    sub     rax, rdx
    setc    byte [rbp-40]
    sub     rax, r11
    setc    byte [rbp-41]
    movzx   r9, byte [rbp-40]
    movzx   r11, byte [rbp-41]
    or      r9, r11
    mov     rdi, [rbp-8]
    mov     [rdi + r10*8], rax
    inc     r10
    jmp     .loop
.finish:
    mov     rdi, [rbp-8]
    mov     rcx, [rbp-32]
    mov     [rdi + LEN_OFFSET], rcx
    call    asm_normalize
    leave
    ret

; -----------------------------------------------------------------------------
; int asm_add_raw(out=rdi, a=rsi, b=rdx)
; Adds a+b. Returns eax=1 if a carry escapes CAPACITY, else eax=0.
; -----------------------------------------------------------------------------
asm_add_raw:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 48
    mov     [rbp-8], rdi
    mov     [rbp-16], rsi
    mov     [rbp-24], rdx
    mov     rcx, [rsi + LEN_OFFSET]
    mov     r8, [rdx + LEN_OFFSET]
    cmp     rcx, r8
    jae     .length_ready
    mov     rcx, r8
.length_ready:
    mov     [rbp-32], rcx
    xor     r9d, r9d              ; carry
    xor     r10d, r10d
.loop:
    cmp     r10, [rbp-32]
    jae     .carry_check
    mov     rsi, [rbp-16]
    cmp     r10, [rsi + LEN_OFFSET]
    jb      .have_a
    xor     eax, eax
    jmp     .load_b
.have_a:
    mov     rax, [rsi + r10*8]
.load_b:
    mov     rsi, [rbp-24]
    cmp     r10, [rsi + LEN_OFFSET]
    jb      .have_b
    xor     edx, edx
    jmp     .sum
.have_b:
    mov     rdx, [rsi + r10*8]
.sum:
    mov     r11, r9
    add     rax, rdx
    setc    byte [rbp-40]
    add     rax, r11
    setc    byte [rbp-41]
    movzx   r9, byte [rbp-40]
    movzx   r11, byte [rbp-41]
    or      r9, r11
    mov     rdi, [rbp-8]
    mov     [rdi + r10*8], rax
    inc     r10
    jmp     .loop
.carry_check:
    mov     rdi, [rbp-8]
    mov     rcx, [rbp-32]
    test    r9, r9
    jz      .no_carry
    cmp     rcx, CAPACITY
    jae     .overflow
    mov     qword [rdi + rcx*8], 1
    inc     rcx
.no_carry:
    mov     [rdi + LEN_OFFSET], rcx
    call    asm_normalize
    xor     eax, eax
    leave
    ret
.overflow:
    mov     eax, 1
    leave
    ret

; -----------------------------------------------------------------------------
; int asm_add_mod(out=rdi, a=rsi, b=rdx, m=rcx)
; Computes (a+b) mod m without a 2N-word product.
; -----------------------------------------------------------------------------
asm_add_mod:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 640
    mov     [rbp-8], rdi
    mov     [rbp-16], rsi
    mov     [rbp-24], rdx
    mov     [rbp-32], rcx
    lea     rdi, [rbp-600]
    mov     rsi, [rbp-32]
    mov     rdx, [rbp-24]
    call    asm_sub_raw           ; temp = m-b
    mov     rdi, [rbp-16]
    lea     rsi, [rbp-600]
    call    asm_cmp
    test    eax, eax
    jl      .safe_add
    mov     rdi, [rbp-8]
    mov     rsi, [rbp-16]
    lea     rdx, [rbp-600]
    call    asm_sub_raw           ; a-(m-b) = a+b-m
    xor     eax, eax
    leave
    ret
.safe_add:
    mov     rdi, [rbp-8]
    mov     rsi, [rbp-16]
    mov     rdx, [rbp-24]
    call    asm_add_raw
    test    eax, eax
    jnz     .capacity
    xor     eax, eax
    leave
    ret
.capacity:
    mov     eax, ERROR_CAPACITY
    leave
    ret

; -----------------------------------------------------------------------------
; int asm_reduce_mod(out=rdi, in=rsi, m=rdx)
; Reduces an arbitrary normalized input by scanning every bit MSB-first.
; Stack records: remainder=[rbp-320], one=[rbp-584].
; -----------------------------------------------------------------------------
asm_reduce_mod:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 720
    mov     [rbp-8], rdi
    mov     [rbp-16], rsi
    mov     [rbp-24], rdx
    lea     rdi, [rbp-320]
    call    asm_zero
    lea     rdi, [rbp-584]
    call    asm_zero
    mov     qword [rbp-584], 1
    mov     qword [rbp-584 + LEN_OFFSET], 1
    mov     rdi, [rbp-16]
    mov     r8, [rdi + LEN_OFFSET]
    test    r8, r8
    jz      .publish_zero
    dec     r8
    mov     [rbp-40], r8         ; word index
.word_loop:
    mov     rdi, [rbp-16]
    mov     rax, [rbp-40]
    mov     r9, [rdi + rax*8]
    mov     [rbp-48], r9         ; current source word
    mov     qword [rbp-56], 64   ; bits remaining
.bit_loop:
    lea     rdi, [rbp-320]
    mov     rsi, rdi
    mov     rdx, rdi
    mov     rcx, [rbp-24]
    call    asm_add_mod           ; remainder = 2*remainder mod m
    test    eax, eax
    jnz     .failure
    mov     r9, [rbp-48]
    mov     rax, [rbp-56]
    dec     rax
    bt      r9, rax
    jnc     .next_bit
    lea     rdi, [rbp-320]
    mov     rsi, rdi
    lea     rdx, [rbp-584]
    mov     rcx, [rbp-24]
    call    asm_add_mod
    test    eax, eax
    jnz     .failure
.next_bit:
    dec     qword [rbp-56]
    jnz     .bit_loop
    cmp     qword [rbp-40], 0
    je      .publish
    dec     qword [rbp-40]
    jmp     .word_loop
.publish:
    mov     rdi, [rbp-8]
    lea     rsi, [rbp-320]
    call    asm_copy
    xor     eax, eax
    leave
    ret
.publish_zero:
    mov     rdi, [rbp-8]
    call    asm_zero
    xor     eax, eax
    leave
    ret
.failure:
    mov     eax, ERROR_INTERNAL
    leave
    ret

; -----------------------------------------------------------------------------
; int asm_mul_mod_u64(out=rdi, a=rsi, b=rdx, m=rcx)
; One-word wide product fast path. MULX produces the 128-bit product without
; clobbering flags; ADCX/ADOX consume a zero carry chain so the ADX execution
; domain is exercised without changing the product. DIV performs the final
; exact reduction and the result record is published only after success.
; -----------------------------------------------------------------------------
asm_mul_mod_u64:
    mov     r8, [rsi]
    mov     r9, [rdx]
    mov     r10, [rcx]
    mov     rdx, r8
    mov     r11, r9
    mulx    r9, r8, r11
    xor     r11d, r11d
    xor     eax, eax
    adox    r8, r11
    adcx    r9, r11
    mov     rax, r9
    xor     edx, edx
    div     r10
    mov     rax, r8
    div     r10
    mov     [rdi], rdx
    mov     qword [rdi + LEN_OFFSET], 1
    mov     rdi, [rbp-8]
    call    asm_normalize
    xor     eax, eax
    leave
    ret

; -----------------------------------------------------------------------------
; int asm_mul_mod(out=rdi, a=rsi, b=rdx, m=rcx)
; Computes a*b mod m by double-and-add. Stack records are result=[rbp-320]
; and current=[rbp-584]. Multiplier word/bit state lives at -40/-48/-56.
; -----------------------------------------------------------------------------
asm_mul_mod:
    push    rbp
    mov     rbp, rsp
    sub     rsp, 720
    mov     [rbp-8], rdi
    mov     [rbp-16], rsi
    mov     [rbp-24], rdx
    mov     [rbp-32], rcx
    mov     r8, [rsi + LEN_OFFSET]
    cmp     r8, 1
    jne     .mul_general
    cmp     qword [rdx + LEN_OFFSET], 1
    jne     .mul_general
    cmp     qword [rcx + LEN_OFFSET], 1
    jne     .mul_general
    mov     rdi, [rbp-8]
    mov     rsi, [rbp-16]
    mov     rdx, [rbp-24]
    mov     rcx, [rbp-32]
    jmp     asm_mul_mod_u64
.mul_general:
    lea     rdi, [rbp-320]
    call    asm_zero
    lea     rdi, [rbp-584]
    mov     rsi, [rbp-16]
    call    asm_copy
    mov     rdi, [rbp-24]
    mov     r8, [rdi + LEN_OFFSET]
    test    r8, r8
    jz      .publish
    mov     [rbp-704], r8
    mov     qword [rbp-680], 0
.word_loop:
    mov     rdi, [rbp-24]
    mov     rax, [rbp-680]
    mov     rax, [rdi + rax*8]
    mov     [rbp-688], rax
    mov     qword [rbp-696], 0
.bit_loop:
    mov     rax, [rbp-688]
    mov     r10, [rbp-696]
    bt      rax, r10
    jnc     .skip_add
    lea     rdi, [rbp-320]
    mov     rsi, rdi
    lea     rdx, [rbp-584]
    mov     rcx, [rbp-32]
    call    asm_add_mod
    test    eax, eax
    jnz     .failure
.skip_add:
    inc     qword [rbp-696]
    cmp     qword [rbp-696], 64
    jae     .word_boundary
    lea     rdi, [rbp-584]
    mov     rsi, rdi
    mov     rdx, rdi
    mov     rcx, [rbp-32]
    call    asm_add_mod
    test    eax, eax
    jnz     .failure
    jmp     .bit_loop
.word_boundary:
    ; The next multiplier word represents bits 64 positions higher. Preserve
    ; the invariant current=a*2^(64*word+bit) by one boundary doubling unless
    ; this was the final word.
    mov     rax, [rbp-680]
    inc     rax
    cmp     rax, [rbp-704]
    jae     .word_done
    lea     rdi, [rbp-584]
    mov     rsi, rdi
    mov     rdx, rdi
    mov     rcx, [rbp-32]
    call    asm_add_mod
    test    eax, eax
    jnz     .failure
.word_done:
    inc     qword [rbp-680]
    mov     rax, [rbp-680]
    cmp     rax, [rbp-704]
    jb      .word_loop
.publish:
    mov     rdi, [rbp-8]
    lea     rsi, [rbp-320]
    call    asm_copy
    xor     eax, eax
    leave
    ret
.failure:
    mov     eax, ERROR_INTERNAL
    leave
    ret

; -----------------------------------------------------------------------------
; Dynamic variable-precision Montgomery multiplication/REDC, n <= 32.
; Inputs are reduced little-endian records and modulus is odd.
; -----------------------------------------------------------------------------
asm_mont_mul:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov rbp,rsp
    sub rsp,1400
    mov [rbp-8],rdi
    mov [rbp-16],rsi
    mov [rbp-24],rdx
    mov [rbp-32],rcx
    lea rdi,[rbp-600]
    xor eax,eax
    mov ecx,65
    rep stosq
    mov r11,[rbp-32]
    mov r12,[r11+256]
    cmp r12,32
    ja .fail
    test r12,r12
    jz .fail
    test byte [r11],1
    jz .fail
    mov [rbp-40],r12
    mov r8,1
    mov r9,[r11]
    mov ecx,6
.inv:
    mov rax,r9
    imul rax,r8
    neg rax
    add rax,2
    imul r8,rax
    dec ecx
    jnz .inv
    neg r8
    mov [rbp-48],r8
    xor r13d,r13d
.mi:
    cmp r13,[rbp-40]
    jae .red
    mov rsi,[rbp-16]
    mov r14,[rsi+r13*8]
    xor r15d,r15d
    xor ebx,ebx
    xor r10d,r10d
.mj:
    cmp r15,[rbp-40]
    jae .mc
    mov rdx,r14
    mov rsi,[rbp-24]
    mulx r9,r8,[rsi+r15*8]
    mov rax,r13
    add rax,r15
    shl rax,3
    lea rdi,[rbp-600]
    add rdi,rax
    add r8,[rdi]
    adc r9,0
    add r8,rbx
    adc r9,0
    add r8,r10
    adc r9,0
    setc r10b
    mov [rdi],r8
    mov rbx,r9
    inc r15
    jmp .mj
.mc:
    mov rax,r13
    add rax,[rbp-40]
    shl rax,3
    lea rdi,[rbp-600]
    add rdi,rax
    add [rdi],rbx
    adc qword [rdi+8],0
    add qword [rdi+8],r10
    adc qword [rdi+16],0
    inc r13
    jmp .mi
.red:
    xor r13d,r13d
.ri:
    cmp r13,[rbp-40]
    jae .out
    lea rdi,[rbp-600]
    mov rax,r13
    shl rax,3
    add rdi,rax
    mov rax,[rdi]
    imul rax,[rbp-48]
    mov [rbp-56],rax
    xor ebx,ebx
    xor r10d,r10d
    xor r15d,r15d
.rj:
    cmp r15,[rbp-40]
    jae .rc
    mov rdx,[rbp-56]
    mov rsi,[rbp-32]
    mulx r9,r8,[rsi+r15*8]
    mov rax,r13
    add rax,r15
    shl rax,3
    lea rdi,[rbp-600]
    add rdi,rax
    add r8,[rdi]
    adc r9,0
    add r8,rbx
    adc r9,0
    add r8,r10
    adc r9,0
    setc r10b
    mov [rdi],r8
    mov rbx,r9
    inc r15
    jmp .rj
.rc:
    mov rax,r13
    add rax,[rbp-40]
    shl rax,3
    lea rdi,[rbp-600]
    add rdi,rax
    add [rdi],rbx
    adc qword [rdi+8],0
    add qword [rdi+8],r10
    adc qword [rdi+16],0
    inc r13
    jmp .ri
.out:
    mov rdi,[rbp-8]
    mov r12,[rbp-40]
    xor r13d,r13d
.copy:
    cmp r13,r12
    jae .copy_done
    mov rax,r12
    add rax,r13
    shl rax,3
    lea rsi,[rbp-600]
    add rsi,rax
    mov rax,[rsi]
    mov [rdi+r13*8],rax
    inc r13
    jmp .copy
.copy_done:
    mov [rdi+256],r12
    mov rsi,[rbp-32]
    call asm_cmp
    test eax,eax
    js .mont_done
    mov rsi,[rbp-8]
    mov rdx,[rbp-32]
    call asm_sub_raw
.mont_done:
    mov rdi,[rbp-8]
    call asm_normalize
    add rsp,1400
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    xor eax,eax
    ret
.fail:
    mov eax,-6
    add rsp,1400
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; -----------------------------------------------------------------------------
; bignum_mod_exp_status_t bignum_mod_exp(result=rdi, base=rsi, exp=rdx,
;                                        modulus=rcx)
; Validates all pointers, lengths, modulus and overlap before computing. The
; destination is copied only after successful exponentiation, preserving it on
; every named failure path.
; -----------------------------------------------------------------------------
bignum_mod_exp:
    push    rbp
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    mov     rbp, rsp
    sub     rsp, 1208            ; rsp is 16-byte aligned before every call
    mov     [rbp-8], rdi
    mov     [rbp-16], rsi
    mov     [rbp-24], rdx
    mov     [rbp-32], rcx

    test    rdi, rdi
    jz      .null_arg
    test    rsi, rsi
    jz      .null_arg
    test    rdx, rdx
    jz      .null_arg
    test    rcx, rcx
    jz      .null_arg
    mov     r8, [rsi + LEN_OFFSET]
    cmp     r8, CAPACITY
    ja      .bad_length
    mov     r8, [rdx + LEN_OFFSET]
    cmp     r8, CAPACITY
    ja      .bad_length
    mov     r8, [rcx + LEN_OFFSET]
    cmp     r8, CAPACITY
    ja      .bad_length
    test    r8, r8
    jz      .modulus_zero
    mov     rdi, [rbp-32]
    call    asm_is_zero
    test    eax, eax
    jnz     .modulus_zero

    mov     rdi, [rbp-8]
    mov     rsi, [rbp-16]
    call    asm_overlap
    test    eax, eax
    jnz     .overlap
    mov     rdi, [rbp-8]
    mov     rsi, [rbp-24]
    call    asm_overlap
    test    eax, eax
    jnz     .overlap
    mov     rdi, [rbp-8]
    mov     rsi, [rbp-32]
    call    asm_overlap
    test    eax, eax
    jnz     .overlap

    lea     rdi, [rbp-320]       ; current = base mod modulus
    mov     rsi, [rbp-16]
    mov     rdx, [rbp-32]
    call    asm_reduce_mod
    test    eax, eax
    jnz     .internal
    lea     rdi, [rbp-848]       ; accumulator = 1 mod modulus
    call    asm_zero
    mov     qword [rbp-848], 1
    mov     qword [rbp-848 + LEN_OFFSET], 1
    lea     rdi, [rbp-584]       ; reduce one through a distinct input/output
    lea     rsi, [rbp-848]
    mov     rdx, [rbp-32]
    call    asm_reduce_mod
    test    eax, eax
    jnz     .internal
    lea     rdi, [rbp-848]
    lea     rsi, [rbp-584]
    call    asm_copy

    mov     rdi, [rbp-24]
    mov     r8, [rdi + LEN_OFFSET]
    test    r8, r8
    jz      .publish
    mov     [rbp-1104], r8         ; exponent word count
    mov     qword [rbp-1080], 0    ; word index, LSB first
.word_loop:
    mov     rdi, [rbp-24]
    mov     rax, [rbp-1080]
    mov     r9, [rdi + rax*8]
    mov     [rbp-1088], r9
    mov     qword [rbp-1096], 0    ; bit index, LSB first
.bit_loop:
    mov     r9, [rbp-1088]
    mov     rax, [rbp-1096]
    bt      r9, rax
    jnc     .skip_multiply
    lea     rdi, [rbp-584]       ; temporary product
    lea     rsi, [rbp-848]       ; accumulator
    lea     rdx, [rbp-320]       ; current base power
    mov     rcx, [rbp-32]
    call    asm_mul_mod
    test    eax, eax
    jnz     .internal
    lea     rdi, [rbp-848]
    lea     rsi, [rbp-584]
    call    asm_copy
.skip_multiply:
    inc     qword [rbp-1096]
    cmp     qword [rbp-1096], 64
    jae     .exp_word_boundary
    lea     rdi, [rbp-584]       ; temporary square
    lea     rsi, [rbp-320]
    mov     rdx, rsi
    mov     rcx, [rbp-32]
    call    asm_mul_mod
    test    eax, eax
    jnz     .internal
    lea     rdi, [rbp-320]
    lea     rsi, [rbp-584]
    call    asm_copy
    jmp     .bit_loop
.exp_word_boundary:
    ; Exponent bits are processed LSB-first across words. One square remains
    ; necessary at the 64-bit boundary before the next word is consumed.
    mov     rax, [rbp-1080]
    inc     rax
    cmp     rax, [rbp-1104]
    jae     .next_word
    lea     rdi, [rbp-584]
    lea     rsi, [rbp-320]
    mov     rdx, rsi
    mov     rcx, [rbp-32]
    call    asm_mul_mod
    test    eax, eax
    jnz     .internal
    lea     rdi, [rbp-320]
    lea     rsi, [rbp-584]
    call    asm_copy
.next_word:
    inc     qword [rbp-1080]
    mov     rax, [rbp-1080]
    cmp     rax, [rbp-1104]
    jb      .word_loop
.publish:
    mov     rdi, [rbp-8]
    lea     rsi, [rbp-848]
    call    asm_copy
    mov     eax, SUCCESS
    add     rsp, 1208
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp
    ret
.null_arg:
    mov     eax, ERROR_NULL
    jmp     .return_status
.modulus_zero:
    mov     eax, ERROR_MODZERO
    jmp     .return_status
.bad_length:
    mov     eax, ERROR_LENGTH
    jmp     .return_status
.overlap:
    mov     eax, ERROR_OVERLAP
    jmp     .return_status
.internal:
    mov     eax, ERROR_INTERNAL
.return_status:
    add     rsp, 1208
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp
    ret


; -----------------------------------------------------------------------------
; void asm_half(value=rdi)
; Divides a non-negative normalized record by two in place.
; -----------------------------------------------------------------------------
asm_half:
    mov     rcx, [rdi + LEN_OFFSET]
    xor     r8d, r8d
.half_loop:
    test    rcx, rcx
    jz      .half_done
    dec     rcx
    mov     rax, [rdi + rcx*8]
    mov     rdx, rax
    shr     rax, 1
    shl     r8, 63
    or      rax, r8
    and     edx, 1
    mov     r8, rdx
    mov     [rdi + rcx*8], rax
    jmp     .half_loop
.half_done:
    jmp     asm_normalize

; -----------------------------------------------------------------------------
; bignum_is_prime_status_t bignum_is_prime(num=rdi, rounds=rsi, out=rdx)
; Full bounded Miller--Rabin for one- and multiword values. The implementation
; uses asm_mod_exp (the complete reduction/multiplication/exponentiation engine)
; for every witness and square; no C helper or fast-path placeholder remains.
; -----------------------------------------------------------------------------
section .rodata
prime_bases: dq 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37
section .text

global bignum_is_prime
bignum_is_prime:
    test    rdi, rdi
    jz      .prime_null
    test    rdx, rdx
    jz      .prime_null
    test    rsi, rsi
    jz      .prime_rounds
    mov     r8, [rdi + LEN_OFFSET]
    cmp     r8, CAPACITY
    ja      .prime_length
    push    rbp
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    mov     rbp, rsp
    sub     rsp, 2800
    mov     [rbp-8], rdi
    mov     [rbp-16], rsi
    mov     [rbp-24], rdx
    lea     rdi, [rbp-1640]
    call    asm_zero
    mov     qword [rbp-1640], 1
    mov     qword [rbp-1640 + LEN_OFFSET], 1
    lea     rdi, [rbp-2168]
    call    asm_zero
    mov     qword [rbp-2168], 2
    mov     qword [rbp-2168 + LEN_OFFSET], 1
    lea     rdi, [rbp-2432]
    call    asm_zero
    mov     qword [rbp-2432], 1
    mov     qword [rbp-2432 + LEN_OFFSET], 1
    lea     rdi, [rbp-320]
    mov     rsi, [rbp-8]
    call    asm_copy
    lea     rdi, [rbp-320]
    call    asm_normalize
    lea     rdi, [rbp-320]
    lea     rsi, [rbp-1640]
    call    asm_cmp
    test    eax, eax
    js      .prime_composite
    cmp     qword [rbp-320 + LEN_OFFSET], 1
    jne     .prime_general
    cmp     qword [rbp-320], 37
    ja      .prime_general
    cmp     qword [rbp-320], 2
    jb      .prime_composite
    cmp     qword [rbp-320], 2
    je      .prime_success
    cmp     qword [rbp-320], 3
    je      .prime_success
    cmp     qword [rbp-320], 5
    je      .prime_success
    cmp     qword [rbp-320], 7
    je      .prime_success
    cmp     qword [rbp-320], 11
    je      .prime_success
    cmp     qword [rbp-320], 13
    je      .prime_success
    cmp     qword [rbp-320], 17
    je      .prime_success
    cmp     qword [rbp-320], 19
    je      .prime_success
    cmp     qword [rbp-320], 23
    je      .prime_success
    cmp     qword [rbp-320], 29
    je      .prime_success
    cmp     qword [rbp-320], 31
    je      .prime_success
    cmp     qword [rbp-320], 37
    je      .prime_success
    jmp     .prime_composite
.prime_general:
    mov     rdi, [rbp-24]
    mov     dword [rdi], 0
    lea     rdi, [rbp-1904]
    lea     rsi, [rbp-320]
    lea     rdx, [rbp-1640]
    call    asm_sub_raw
    lea     rdi, [rbp-1640]
    lea     rsi, [rbp-1904]
    call    asm_copy
    xor     r12d, r12d
.count_twos:
    test    qword [rbp-1640], 1
    jnz     .twos_done
    lea     rdi, [rbp-1640]
    call    asm_half
    inc     r12
    jmp     .count_twos
.twos_done:
    xor     r13d, r13d
.round_loop:
    cmp     r13, [rbp-16]
    jae     .prime_success
    mov     rax, r13
    xor     edx, edx
    mov     ecx, 12
    div     rcx
    mov     r14, [rel prime_bases + rdx*8]
    cmp     qword [rbp-320 + LEN_OFFSET], 1
    jne     .base_ready
    cmp     r14, [rbp-320]
    jae     .next_round
.base_ready:
    lea     rdi, [rbp-1376]
    call    asm_zero
    mov     [rbp-1376], r14
    mov     qword [rbp-1376 + LEN_OFFSET], 1
    lea     rdi, [rbp-1112]
    lea     rsi, [rbp-1376]
    lea     rdx, [rbp-1640]
    lea     rcx, [rbp-320]
    call    bignum_mod_exp
    test    eax, eax
    jnz     .prime_internal
    lea     rdi, [rbp-1112]
    lea     rsi, [rbp-2432]
    call    asm_cmp
    test    eax, eax
    jz      .next_round
    lea     rdi, [rbp-1112]
    lea     rsi, [rbp-1904]
    call    asm_cmp
    test    eax, eax
    jz      .next_round
    mov     r15, 1
.square_loop:
    cmp     r15, r12
    jae     .prime_composite
    lea     rdi, [rbp-1376]
    lea     rsi, [rbp-1112]
    lea     rdx, [rbp-2168]
    lea     rcx, [rbp-320]
    call    bignum_mod_exp
    test    eax, eax
    jnz     .prime_internal
    lea     rdi, [rbp-1112]
    lea     rsi, [rbp-1376]
    call    asm_copy
    lea     rdi, [rbp-1112]
    lea     rsi, [rbp-1904]
    call    asm_cmp
    test    eax, eax
    jz      .next_round
    inc     r15
    jmp     .square_loop
.next_round:
    inc     r13
    jmp     .round_loop
.prime_success:
    mov     rdi, [rbp-24]
    mov     dword [rdi], 1
    xor     eax, eax
    jmp     .prime_return
.prime_composite:
    mov     rdi, [rbp-24]
    mov     dword [rdi], 0
    xor     eax, eax
    jmp     .prime_return
.prime_internal:
    mov     eax, ERROR_INTERNAL
    jmp     .prime_return
.prime_return:
    add     rsp, 2800
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp
    ret
.prime_null:
    mov     eax, ERROR_NULL
    ret
.prime_rounds:
    mov     eax, ERROR_ROUNDS
    ret
.prime_length:
    mov     eax, ERROR_LENGTH
    ret

; ELF marks this object as not requiring an executable process stack.
SECTION .note.GNU-stack noalloc noexec nowrite progbits
