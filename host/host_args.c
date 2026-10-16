/* host_args.c - see host_args.h. */
#include "host_args.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int    s_argc;
static char **s_argv;

/* "foo-bar" / "FOO_BAR" / "stall-ms" -> "FOO_BAR" (upper case, '_' for '-'). */
static void norm(const char *in, char *out, size_t n)
{
    size_t i = 0;

    for (; in[i] && i + 1 < n; i++) {
        char c = in[i];
        if (c == '-')
            c = '_';
        out[i] = (char)toupper((unsigned char)c);
    }
    out[i] = 0;
}

/* Find `--needle` in argv.  Returns the value ("", or the text after '=', or
 * the next argv word when it does not start with '-'), or NULL when absent. */
static const char *find_cli(const char *needle)
{
    int i;

    for (i = 1; i < s_argc; i++) {
        const char *a = s_argv[i];
        const char *eq;
        char        key[128];
        size_t      klen;

        if (a[0] != '-' || a[1] != '-')
            continue;
        a += 2;
        eq   = strchr(a, '=');
        klen = eq ? (size_t)(eq - a) : strlen(a);
        if (klen == 0 || klen >= sizeof key)
            continue;
        memcpy(key, a, klen);
        key[klen] = 0;
        norm(key, key, sizeof key);
        if (strcmp(key, needle) != 0)
            continue;
        if (eq)
            return eq + 1;
        if (i + 1 < s_argc && s_argv[i + 1][0] != '-')
            return s_argv[i + 1];
        return "";                       /* bare flag */
    }
    return NULL;
}

void host_args_init(int argc, char **argv)
{
    s_argc = argc;
    s_argv = argv;
}

const char *host_arg(const char *name)
{
    char needle[128];
    char env[160];
    const char *v;

    norm(name, needle, sizeof needle);
    v = find_cli(needle);
    if (v)
        return v;
    snprintf(env, sizeof env, "FD2_%s", needle);
    return getenv(env);
}

int host_arg_flag(const char *name)
{
    return host_arg(name) != NULL;
}

void host_args_dump(void)
{
    static const char *known[] = {
        "LOG", "GAMEDIR", "EXE",
        "UNFOCUS", "UNFOCUS_FADE_MS", "AUTOQUIT_MS",
        "UPSCALE", "ASPECT",
        "VOLUME", "KEYS", "KEY_DELAY", "KEYPLAY", "KEYLOG",
        "STALL_MS", "HANG_EXIT",
        "TYPEMATIC", "TYPEMATIC_DELAY", "TYPEMATIC_RATE", "TYPEMATIC_DEBUG",
        "GL_READBACK", "TESTPATTERN",
    };
    int i;

    for (i = 0; i < (int)(sizeof known / sizeof known[0]); i++) {
        const char *v = host_arg(known[i]);

        if (v)
            printf("fd2: arg   --%s = '%s'\n", known[i], v);
    }
}
