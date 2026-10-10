/* map_records.h - map record-list builders (original 0x1B6B7, 0x149F8).
 *
 * Two leaves that walk the 80-byte record table dword_53A45 and write the
 * indices they pick into a caller buffer, returning the count:
 *
 *   0x1B6B7  collect the 3-byte "script entry" at record offset +49 of every
 *            record that is unflagged (byte +5 bit 0 clear), whose +49 byte is
 *            not 0xFF and whose 16-bit word at +64 is zero.  The caller hands
 *            the result straight to the event-script interpreter (ev_script_run,
 *            0x1AA1D - see sub_20C6F's tail).
 *   0x149F8  start on map cell (start_x, start_y) and step one cell at a time
 *            toward (ref_x, ref_y) for `steps` cells, collecting the record
 *            found on each in-range cell.  `want_empty` picks the predicate: 0
 *            keeps records whose status byte +6 is non-zero, non-zero keeps the
 *            records whose +6 is zero.
 *
 * ABI = cdecl, decoded from the pushes in the offline disassembly
 * (analysis/cseg01_disasm.txt), which is the authority for the constants and
 * the signed comparisons.  Both functions leave the count in EAX by falling
 * into a neighbour's shared epilogue (0x1B64D / 0x114FF); IDA reports those as
 * calls to sub_1B5F1 / sub_1145A, but they are false callees and stay plain
 * returns here.  The Watcom stack probe (`push size; call 0x3702F`) is dropped.
 * Hand-restored, compile-verified, no differential harness yet.
 */
#ifndef GAME_MAP_RECORDS_H
#define GAME_MAP_RECORDS_H

#include <stdint.h>

/* 0x1B6B7 - gather the 3-byte entry at +49 of each qualifying record into
 * `out`; returns how many were written. */
int map_collect_scripts(uint8_t *out);

/* 0x149F8 - walk a line of map cells from (start_x, start_y) toward
 * (ref_x, ref_y) for `steps` cells, collecting the record index on every cell
 * that lies inside the map.  `out` takes the indices; returns the count. */
int map_scan_records(int ref_x, int ref_y, uint8_t *out,
                     int start_x, int start_y, int steps, int want_empty);

#endif /* GAME_MAP_RECORDS_H */
