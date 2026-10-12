/* world_load_alias.inc - module-local aliases for world_load.c (included from it).
 *
 * port/world_load.c used the IDA short names 'p' (0x53A55) and 'buf' (0x53A61).
 * Both cells are native byte-buffer pointers now (src/include/fd2_native.h), so
 * the aliases must point at those real variables, NOT at the 4-byte mirror
 * cells - writing a 64-bit pointer into the mirror truncates it and desyncs
 * unit_sprites_build (which reads the real variable). */
#define p   dword_53A55
#define buf dword_53A61
