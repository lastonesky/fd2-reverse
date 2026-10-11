#!/usr/bin/env bash
# check_compile.sh - phase-5 light validation for port3 (run via WSL: bash tools/check_compile.sh)
# Usage: bash tools/check_compile.sh [--full]
#   default: syntax-check the restored/migrated game modules in both profiles
#   --full : also the generated sources + a header self-test, with -Werror
set -u
cd "$(dirname "$0")/.." || exit 1
CC=${CC:-gcc}
FLAGS="-fsyntax-only -std=c99"
WARN="-Wall -Wextra"

# Transitional tolerance is now needed ONLY by the 32-bit hybrid profile
# (FD2_GUEST_FIXED): it deliberately keeps the 4-byte LE segment-mirror cells
# and the machine-code ORIG_* callbacks, whose pointer<->int casts are bit-exact
# on the -m32 target.  The standalone/native profile is checked STRICT (no
# int-conversion suppressions) since de-guest phase C1 moved every pointer cell
# to real C storage (src/include/fd2_native.h, src/fd2_native.c).
# sign-compare tolerance remains for both profiles until the C4 ORIG_* sweep.
TOLERANT="-Wno-int-conversion -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -Wno-sign-compare"

hr() { printf '%s\n' "------------------------------------------------------------"; }

echo "########## standalone (native): game modules STRICT (no int-conv) ##########"
$CC $FLAGS $WARN -Wno-sign-compare src/game/*.c src/fd2_native.c src/fd2_dispatch.c 2>&1 | grep -E "error:" | head -40
echo "errors: $($CC $FLAGS $WARN -Wno-sign-compare src/game/*.c src/fd2_native.c src/fd2_dispatch.c 2>&1 | grep -c 'error:')"

hr
echo "########## hybrid (FD2_GUEST_FIXED): game modules ##########"
$CC $FLAGS -DFD2_GUEST_FIXED $WARN $TOLERANT src/game/*.c src/fd2_native.c src/fd2_dispatch.c 2>&1 | grep -E "error:" | head -40
echo "errors: $($CC $FLAGS -DFD2_GUEST_FIXED $TOLERANT src/game/*.c src/fd2_native.c src/fd2_dispatch.c 2>&1 | grep -c 'error:')"

if [ "${1:-}" = "--full" ]; then
  hr
  echo "########## prepare data-segment image (needs FD2.EXE; see prepare_image.py) ##########"
  python3 tools/prepare_image.py --quiet || { echo "prepare_image failed"; exit 1; }
  hr
  echo "########## -Werror: all modules + generated (both profiles) ##########"
  for P in "" "-DFD2_GUEST_FIXED"; do
    $CC $FLAGS -Werror $TOLERANT $P src/game/*.c src/fd2_native.c src/fd2_dispatch.c src/fd2_image.c src/fd2_relocate.c
    echo "profile ${P:-standalone} rc=$?"
  done
  echo "########## header self-test ##########"
  printf '#include "src/game/fd2_game.h"\n' > ./_hdr_test.c
  $CC $FLAGS -I. ./_hdr_test.c && echo "umbrella rc=0"
  rm -f ./_hdr_test.c
fi
