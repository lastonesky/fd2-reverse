/* host_args.h - one command-line front end for the host's run knobs.
 *
 * Before this every knob was its own FD2_* environment variable, which is
 * awkward to type and easy to forget.  Now each one also has a normal
 * `--name` / `--name=value` switch (the same spelling used by the platform
 * layer's child process), and the environment is only a fallback:
 *
 *     --unfocus=mute,pause      ==  FD2_UNFOCUS=mute,pause
 *     --upscale=sharp           ==  FD2_UPSCALE=sharp
 *     --log=run.log             ==  FD2_LOG=run.log
 *     --hang-exit               ==  FD2_HANG_EXIT=1
 *
 * Matching is case-insensitive and treats '-' like '_', so `--stall-ms` and
 * `--STALL_MS` are the same switch.  `host_arg()` returns the CLI value when
 * present (an empty string for a bare flag), else getenv("FD2_<NAME>"), else
 * NULL.  Call host_args_init() once, at the very top of fd2_engine_init(),
 * before anything reads a knob.
 */
#ifndef FD2_HOST_ARGS_H
#define FD2_HOST_ARGS_H

void host_args_init(int argc, char **argv);

/* Value for `--<name>` (or $FD2_<name>), or NULL if neither is set.  A bare
 * `--name` yields "" (non-NULL), so `host_arg("X") != NULL` is the flag
 * test.  `--name value` (space separated) is accepted too. */
const char *host_arg(const char *name);

/* Convenience: 1 when the switch is present (bare or valued). */
int host_arg_flag(const char *name);

/* Diagnostics: print every recognised switch and its value. */
void host_args_dump(void);

#endif /* FD2_HOST_ARGS_H */
