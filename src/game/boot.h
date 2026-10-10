/* boot.h - program entry (original main @ 0x25BF4) - hand-restored, phase 1. */
#ifndef FD2_GAME_BOOT_H
#define FD2_GAME_BOOT_H

int main(int argc, char **argv);

/* Call sites keep the IDA name (none for main, but manifest symmetry). */
#define sub_25BF4 main

#endif /* FD2_GAME_BOOT_H */
