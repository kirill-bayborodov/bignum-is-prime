# n=2 failing Montgomery trace

## Case

- Modulus `M = 0xaf1ca53ea6ac10e1b19bd066b17f8f83`
- Base `a = 0x02e658cc5838ce3efffcc1b7c9828013`
- Exponent `e = 0xa2fe398b`
- `R = 2^128 mod M = 0x50e35ac15953ef1e4e642f994e80707d`
- `R² mod M = 0x49783464c265ed286fcf41b95143d75b`
- `aR mod M = 0x637708301eaa5af847533199484412b6`
- `1R mod M = 0x50e35ac15953ef1e4e642f994e80707d`
- Expected final result: `0x9416822e00b7dfcf93cc8509c5872871`

## Trace result

The initial conversions are correct: `asm_mont_mul(a, R²) = aR` and `asm_mont_mul(1, R²) = R`. Calls 1–61 are correct against the Python oracle.

The first mismatch is call 62. Its REDC scratch state is:

```text
T[0]   = 0x0000000000000000
T[1]   = 0x0000000000000000
T[2]   = 0x08449f047db1f419
T[3]   = 0x196107af98b21351
T[4]   = 0x0000000000000001
T[5]   = 0x0000000000000000
```

Thus the REDC candidate is the 129-bit value
`U = 0x1_196107af98b2135108449f047db1f419`, but the output-copy loop copies only `T[n]..T[2n-1]` and ignores `T[2n]`.

The copied candidate is:

```text
0x196107af98b2135108449f047db1f419
```

Because `T[2n] != 0`, the final conditional subtraction of `M` is mandatory. The correct reduction is:

```text
2^128 + candidate - M
= 0x6a446270f206026f56a8ce9dcc326496
```

which exactly matches the Python oracle for call 62. The current code compares only the copied n-limb value with `M`, so it skips subtraction and returns the wrong result. Subsequent Montgomery operations are internally consistent with this corrupted value, and the final decode operation is also correct relative to the corrupted state.

## Source location

`src/bignum_is_prime.asm`, `.out`/`.copy_done`:

```text
.copy_done:
    mov [rdi+256],r12
    mov rsi,[rbp-32]
    call asm_cmp
    test eax,eax
    js .mont_done
    mov rsi,[rbp-8]
    mov rdx,[rbp-32]
    call asm_sub_raw
```

The final reduction now accounts for the extra scratch limb `T[2n]` before deciding whether to subtract the modulus. The fix is implemented in `src/bignum_is_prime.asm` and validated by the failing case plus randomized kernel tests.

Raw artifacts: `/tmp/failing_mont_gdb_all.log`, `/tmp/call62_redc_state.log`, `/tmp/failing_mont_oracle.txt`.
