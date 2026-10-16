# host/Makefile - build the reused port/ backend + the port3 adapter layer.
#
#   make -C host            # syntax-check the backend + adapters (no link)
#   make -C host engine     # link the full native engine -> host/build/fd2engine
#   make -C host clean
#
# The engine = port3's restored game C (src/) + the host backend/adapters.
# Portable across Linux, macOS and (with the right libs) the BSDs:
#   - sokol picks its own backend: Metal on macOS, OpenGL core elsewhere
#     (Windows uses build.ps1 / MSVC, not this Makefile).
#   - platform_posix.c falls back to the portable paths on non-Linux.
#
# The data-segment image (src/fd2_image.c) is generated from the user's own
# FD2.EXE by tools/prepare_image.py, so it is not in the repository.  Set
# FD2_EXE=/path/to/FD2.EXE if it is not ./FD2.EXE.

CC      ?= cc
PYTHON  ?= python3

UNAME_S := $(shell uname -s)

CFLAGS   ?= -O2 -g -std=gnu11 -Wall -D_FILE_OFFSET_BITS=64 -pthread
CPPFLAGS ?= -I. -I../src -I../src/game -I../src/include -Ivendor/sokol

ifeq ($(UNAME_S),Darwin)
  # sokol_app/gfx/audio use Cocoa + Metal + CoreAudio on macOS.
  CFLAGS    += -DSOKOL_METAL
  HOST_LIBS := -framework Cocoa -framework QuartzCore -framework Metal \
               -framework MetalKit -framework AudioToolbox -framework CoreAudio \
               -framework Foundation -lm -pthread
else
  # Linux (and the default): sokol_app/audio use X11 + ALSA, sokol_gfx GL core.
  CFLAGS    += -DSOKOL_GLCORE
  HOST_LIBS := -lX11 -lXi -lXcursor -lGL -lasound -ldl -lm -lpthread
endif

# Backend (reused from port/) + adapter layer + entry.
HOST_SRCS := host_args.c dls.c synth.c audio_sokol.c xmidi.c keys.c \
             platform_posix.c render_sokol.c sokol_impl.c \
             fd2_audio.c fd2_video.c fd2_platform.c fd2_kernel.c main_sokol.c

# Restored game + generated tables/images.
GAME_SRCS := $(wildcard ../src/game/*.c) \
             ../src/fd2_native.c ../src/fd2_dispatch.c \
             ../src/fd2_image.c ../src/fd2_relocate.c

# Windows-only files (render_gdi.c / keys_win32.c / platform_win32.c) excluded.
CHECK_SRCS := $(filter-out main_sokol.c,$(HOST_SRCS))

.PHONY: all check objs engine image clean
all: check

# Generate src/fd2_image.c from the user's FD2.EXE (or reuse analysis/seg_*.bin).
image:
	@$(PYTHON) ../tools/prepare_image.py || \
	  { echo "tools/prepare_image.py failed (need FD2.EXE; see EXTERNAL_ARTIFACTS.md)"; exit 1; }

check: image
	@set -e; for f in $(CHECK_SRCS); do \
	  printf '%-18s ' $$f; \
	  err=$$($(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only $$f 2>&1 | grep -c error: || true); \
	  echo "errors=$$err"; test $$err -eq 0; \
	done
	@set -e; for f in $(GAME_SRCS); do \
	  printf '%-18s ' $$f; \
	  err=$$($(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only $$f 2>&1 | grep -c error: || true); \
	  echo "errors=$$err"; test $$err -eq 0; \
	done

engine: image
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o build/fd2engine \
	  $(HOST_SRCS) $(GAME_SRCS) $(HOST_LIBS)
	@echo "linked: host/build/fd2engine"

clean:
	@rm -rf build
