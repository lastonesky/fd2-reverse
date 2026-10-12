#!/usr/bin/env bash
# scan64.sh - systematic 64-bit portability scan for port3.
# Run via WSL:  wsl bash tools/scan64.sh [standalone|hybrid|warn|nums|all]
#
# Profiles:
#   standalone  native 64-bit game modules, STRICT (int-conversion is an error)
#   hybrid      FD2_GUEST_FIXED 32-bit differential profile
#   warn        standalone warnings, unfiltered by default -Werror setup
#   nums        bare decimal literals in guest address ranges
set -u
cd "$(dirname "$0")/.." || exit 1
CC=${CC:-gcc}
WHAT=${1:-standalone}
BASE="-fsyntax-only -std=c99"

hr() { printf '%s\n' "============================================================"; }

case "$WHAT" in
standalone)
  hr; echo "standalone: int<->pointer truncation must be an error"
  $CC $BASE -Wall -Wextra -Wno-sign-compare \
      -Werror=int-conversion -Werror=int-to-pointer-cast \
      -Werror=pointer-to-int-cast \
      src/game/*.c 2>&1 | grep -E "error:|warning:" | head -80
  ;;
hybrid)
  hr; echo "hybrid FD2_GUEST_FIXED"
  $CC $BASE -DFD2_GUEST_FIXED -Wall -Wextra \
      -Wno-int-conversion -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
      -Wno-sign-compare src/game/*.c 2>&1 | grep -E "error:" | head -80
  ;;
warn)
  hr; echo "standalone warnings (non-truncation filtered out)"
  $CC $BASE -Wall -Wextra -Wno-sign-compare \
      -Wno-error=incompatible-pointer-types \
      src/game/*.c 2>&1 \
    | grep -E "warning:|error:" \
    | grep -v "unused parameter\|unused variable\|set but not used" | head -120
  ;;
nums)
  hr; echo "bare decimal literals in guest address ranges"
  rg -n --no-heading -o '\b[0-9]{5,7}\b' src/game/*.c src/game/*.h \
    | python3 - <<'PY'
import re,sys,collections
ranges=[('dseg02',0x50000,0x556B0),('dseg03',0x60000,0x64000),
        ('VGA',0xA0000,0xB0000),('code',0x10000,0x50000)]
for line in sys.stdin:
    m=re.match(r'^(.*?):(\d+):(\d+)$',line.rstrip())
    if not m: continue
    f,ln,num=m.groups(); n=int(num)
    for rn,a,b in ranges:
        if a<=n<b:
            print(f"{rn:8} {f}:{ln}: {num} = 0x{n:X}")
PY
  ;;
*)
  echo "usage: $0 [standalone|hybrid|warn|nums]"; exit 2;;
esac
