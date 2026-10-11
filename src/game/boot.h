/* boot.h - program entry (original main @ 0x25BF4) - hand-restored, phase 1. */
#ifndef FD2_GAME_BOOT_H
#define FD2_GAME_BOOT_H

int fd2_game_main(int argc, char **argv);

/* Call sites keep the IDA name (none for main, but manifest symmetry). */
#define sub_25BF4 fd2_game_main

#endif /* FD2_GAME_BOOT_H */
