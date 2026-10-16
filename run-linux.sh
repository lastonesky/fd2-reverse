#!/usr/bin/env bash
# run-linux.sh - build (if needed) and launch the native FD2 engine on Linux.
#
# The engine itself is built by `make` (= tools/prepare_image.py + host/Makefile
# engine) into host/build/fd2engine from:
#   host/main_sokol.c + host/ adapters   (Linux/macOS entry, OpenGL + sokol)
#   src/                                 (the restored game C)
#
# Usage:
#   ./run-linux.sh [DATA_DIR]
#
# DATA_DIR is the directory that holds the original game data files
# (FDTXT.DAT, FDOTHER.DAT, FDSHAP.DAT, FDFIELD.DAT, DATO.DAT, FDMUS.DAT,
# FDICON.B24, ...).  It defaults to this repository directory.  The engine
# opens those files relative to the current directory.
#
# Any FD2_* environment variables are honoured by the engine:
#   FD2_UPSCALE=nearest|linear|sharp|bicubic, FD2_ASPECT=off, FD2_LOG=<path>,
#   FD2_KEYS=..., FD2_KEYLOG=<path>, FD2_KEYPLAY=<path>, ...
# Set FD2_GL_SOFTWARE=1 to force Mesa's software rasteriser (workaround for a
# misbehaving GPU driver).
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
engine="$here/host/build/fd2engine"

if [ ! -x "$engine" ]; then
    echo "run-linux: engine not built yet, running 'make engine' ..." >&2
    make -C "$here" engine
fi

data="${1:-$here}"
if [ $# -gt 0 ]; then
    shift
fi
if [ ! -d "$data" ]; then
    echo "run-linux: data directory not found: $data" >&2
    exit 1
fi
cd "$data"

missing=0
for f in FDTXT.DAT FDOTHER.DAT FDSHAP.DAT FDFIELD.DAT DATO.DAT FDMUS.DAT \
         BG.DAT FDICON.B24; do
    if [ ! -f "$f" ]; then
        echo "run-linux: missing game data file: $data/$f" >&2
        missing=1
    fi
done
if [ "$missing" -eq 1 ]; then
    echo "run-linux: copy the original FD2 data files into $data and retry." >&2
    exit 1
fi

if [ "${FD2_GL_SOFTWARE:-0}" = "1" ]; then
    export LIBGL_ALWAYS_SOFTWARE=1
fi

exec "$engine" "$@"
