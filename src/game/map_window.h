/* map_window.h - interactive map/dialog window (original 0x19953).
 * Returns 1 on the exit keys, -1 on scan 1/0x53, loops otherwise. */
#ifndef FD2_GAME_MAP_WINDOW_H
#define FD2_GAME_MAP_WINDOW_H

int map_window_run(void);

/* Call sites keep the IDA name. */
#define sub_19953 map_window_run

#endif /* FD2_GAME_MAP_WINDOW_H */
