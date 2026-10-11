/* ev_script.h - event-script interpreter (original 0x1AA1D).
 *
 * Runs `steps` 3-byte {op, u16 arg} entries against the record at rec_index.
 * Opcodes: 0 portrait dialog (+ map window), 1 portrait script, 2 handler-table
 * call, 3 vm_run. See ev_script.c for the full decoded behaviour.
 */
#ifndef FD2_GAME_EV_SCRIPT_H
#define FD2_GAME_EV_SCRIPT_H

void ev_script_run(int rec_index, int steps, const void *script);

/* Call sites keep the IDA name. */
#define sub_1AA1D ev_script_run

#endif /* FD2_GAME_EV_SCRIPT_H */
