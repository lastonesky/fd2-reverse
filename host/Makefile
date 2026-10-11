# host/Makefile - build the reused port/ backend + the port3 adapter layer.
#
#   make -C host            # syntax-check the backend + adapters (no link)
#   make -C host engine     # link the full native engine -> host/build/fd2engine
#   make -C host clean
#
# The engine = port3's restored game C (src/) + the host backend/adapters.
CC      ?= cc
CFLAGS  ?= -O2 -g -std=gnu11 -Wall -D_FILE_OFFSET_BITS=64 -pthread -DSOKOL_GLCORE
CPPFLAGS ?= -I. -I../src -I../src/game -I../src/include -Ivendor/sokol
HOST_LIBS := -lX11 -lXi -lXcursor -lGL -lasound -ldl -lm -lpthread

# Backend (reused from port/) + adapter layer + entry.
HOST_SRCS := dls.c synth.c audio_sokol.c xmidi.c keys.c platform_posix.c \
             render_sokol.c sokol_impl.c \
             fd2_audio.c fd2_video.c fd2_platform.c fd2_kernel.c main_sokol.c

# Restored game + generated tables/images.
GAME_SRCS := $(wildcard ../src/game/*.c) \
             ../src/fd2_native.c ../src/fd2_dispatch.c \
             ../src/fd2_image.c ../src/fd2_relocate.c

# Windows-only files (render_gdi.c / keys_win32.c / platform_win32.c) excluded.
CHECK_SRCS := $(filter-out main_sokol.c,$(HOST_SRCS))

.PHONY: all check objs engine clean
all: check

check:
	@set -e; for f in $(CHECK_SRCS); do \
	  printf '%-18s ' $$f; \
	  err=$$($(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only $$f 2>&1 | grep -c error: || true); \
	  echo "errors=$$err"; test $$err -eq 0; \
	done

engine:
	@mkdir -p build
	@python3 ../tools/prepare_image.py --quiet || { echo "tools/prepare_image.py failed (need FD2.EXE; see EXTERNAL_ARTIFACTS.md)"; exit 1; }
	$(CC) $(CPPFLAGS) $(CFLAGS) -o build/fd2engine \
	  $(HOST_SRCS) $(GAME_SRCS) $(HOST_LIBS)
	@echo "linked: host/build/fd2engine"

clean:
	@rm -rf build
