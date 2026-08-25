#!/usr/bin/env bash
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
OUT=${COVERAGE_OUT:-"$ROOT/coverage/c11"}
CC=${CC:-cc}
CFLAGS=${CFLAGS:-"-std=c11 -Wall -Wextra -pedantic -O0"}

rm -rf "$OUT"
mkdir -p "$OUT"

INCLUDES=(
  "-I$ROOT/include"
  "-I$ROOT/libs/bignum-core/include"
  "-I$ROOT/benchmarks/adapter"
  "-I$ROOT/libs/benchmark-framework/dist/include"
  "-I$ROOT/libs/benchmark-framework/dist"
)

# Compile the production C11 implementation exactly once. All public test
# binaries below link this same object; no source is included into a test TU.
$CC $CFLAGS --coverage "${INCLUDES[@]}" -c \
  "$ROOT/src/bignum_is_prime.c" -o "$OUT/bignum_is_prime.o"

$CC $CFLAGS --coverage "${INCLUDES[@]}" \
  "$ROOT/tests/test_bignum_is_prime.c" "$OUT/bignum_is_prime.o" \
  -o "$OUT/test_bignum_is_prime"

$CC $CFLAGS --coverage "${INCLUDES[@]}" \
  "$ROOT/tests/test_bignum_is_prime_mt.c" "$OUT/bignum_is_prime.o" \
  -pthread -o "$OUT/test_bignum_is_prime_mt"

$CC $CFLAGS --coverage "${INCLUDES[@]}" \
  "$ROOT/tests/test_bignum_is_prime_runner.c" "$OUT/bignum_is_prime.o" \
  -o "$OUT/test_bignum_is_prime_runner"

"$OUT/test_bignum_is_prime"
"$OUT/test_bignum_is_prime_mt"
"$OUT/test_bignum_is_prime_runner"

# Run gcov from the repository root so the source path resolves to the
# production file. The report must contain one implementation source entry.
(
  cd "$ROOT"
  gcov -b -c -o "$OUT" "$OUT/bignum_is_prime.o" \
    > "$OUT/gcov.txt" 2>&1
)

cp "$ROOT/bignum_is_prime.c.gcov" "$OUT/bignum_is_prime.c.gcov"
rm -f "$ROOT/bignum_is_prime.c.gcov"

if ! grep -q "src/bignum_is_prime.c" "$OUT/gcov.txt"; then
  echo "coverage error: production C11 source is absent from gcov report" >&2
  exit 1
fi

cat "$OUT/gcov.txt"
