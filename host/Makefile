# host/Makefile - build the reused port/ backend layer of port3.
#
#   make -C host            # syntax-check the POSIX backend (no link)
#   make -C host objs       # compile the backend objects
#   make -C host clean
#
# The runnable engine additionally needs the port3 adapter layer described in
# host/README.md §3 (fd2_kernel/fd2_video/fd2_audio/fd2_platform) plus the
# restored game sources under src/.
CC      ?= cc
CFLAGS  ?= -O2 -g -std=gnu11 -Wall -D_FILE_OFFSET_BITS=64 -pthread
CPPFLAGS ?= -I. -I../src -Ivendor/sokol

# POSIX backend (Win32-only files render_gdi.c / keys_win32.c /
# platform_win32.c are excluded here).
BACKEND := dls.c synth.c audio_sokol.c xmidi.c keys.c platform_posix.c \
           render_sokol.c sokol_impl.c fd2_audio.c

.PHONY: all check objs clean
all: check

check:
	@set -e; for f in $(BACKEND); do \
	  printf '%-18s ' $$f; \
	  err=$$($(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only $$f 2>&1 | grep -c error: || true); \
	  echo "errors=$$err"; test $$err -eq 0; \
	done

objs:
	@mkdir -p build
	@for f in $(BACKEND); do \
	  $(CC) $(CPPFLAGS) $(CFLAGS) -c $$f -o build/$${f%.c}.o || exit 1; \
	done
	@echo "backend objects in host/build/"

clean:
	@rm -rf build
