/* render_sokol.c - sokol_gfx implementation of render.h.
 *
 * Pixel contract (render.h): the shared layer hands over 320x200 pixels that
 * are 32bpp BI_RGB, i.e. **BGRA in memory**. We upload them verbatim into an
 * RGBA8 texture and do the channel swizzle once, in the fragment shader, so
 * the per-frame CPU cost stays the same as the GDI path.
 *
 * Everything is sized from the render_desc: the quad covers the whole client
 * area, so the window can be resized freely (the GDI path draws at a fixed
 * integer scale). The CPU never scales; the fragment shader does.
 *
 * ---------------------------------------------------------------------------
 * Upscale modes ("plan A" HD presentation)
 * ---------------------------------------------------------------------------
 * The source is only 320x200, so the final look is decided entirely by the
 * scaler. The mode is chosen once at init from FD2_UPSCALE:
 *
 *   nearest  - the original pixel-exact NEAREST path (reference / A-B tests)
 *              (default: no smoothing, byte-identical reference output)
 *   linear   - plain bilinear (smooth, soft - rarely what pixel art wants)
 *   sharp    - sharp-bilinear: bilinear with the texel-crossing steepened so
 *              edges stay crisp instead of turning to mush
 *   bicubic  - Catmull-Rom 4x4, smooth gradients, good for photo-ish art
 *
 * All smoothing is done by hand from a single NEAREST sampler (4 taps for
 * bilinear, 16 for bicubic) so D3D11 and GL produce the same result and no
 * sampler-filter state has to be rebuilt. The default NEAREST mode keeps the
 * byte-identical reference output; opt into smoothing with FD2_UPSCALE.
 *
 * The source aspect ratio (320:200 = 8:5) is preserved when the window is
 * resized: the draw is confined to a centered viewport with that aspect and
 * the remaining area stays at the (black) clear colour. FD2_ASPECT=off
 * stretches to fill the client area again.
 *
 * This layer is independent of the game: an HD asset pack (plan B) or a
 * high-res internal framebuffer (plan C) would feed bigger frames through the
 * same render_present() seam, and the upscaler can then be switched back to
 * nearest (or the source size grows and the scale factor shrinks) without
 * touching the game code. */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "render.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"

#if defined(SOKOL_GLCORE)
#include <GL/gl.h>
/* Orientation self-check for the GL path (docs/PITFALLS.md §8-67). The window
 * content is *not* what --screenshot captures (that is the shared layer's
 * BMP), so a flipped present path used to be invisible to the frame diff.
 * With FD2_GL_READBACK=<n> every n-th frame reads the just-rendered default
 * framebuffer back and prints how many pixels match the source upright vs
 * vertically flipped; FD2_TESTPATTERN=1 replaces the guest frame with a
 * static high-contrast pattern (green ramps with the row, white block
 * top-left, red block bottom-right) so the answer has no animation noise.
 * Off by default: the cost is one getenv per frame. Only meaningful in
 * nearest mode (a smoothing filter legitimately changes the pixels). */
static void gl_readback_check(const uint32_t *bgra, int w, int h)
{
    static long frames;
    const char *env = getenv("FD2_GL_READBACK");
    long interval = env ? atol(env) : 0;
    int fw, fh, s = 3;                  /* the host's 3x window */
    unsigned char *px;
    long up = 0, dn = 0, n = 0;
    int x, y;

    if (interval <= 0)
        return;
    if (++frames % interval)
        return;
    fw = w * s;
    fh = h * s;
    px = malloc((size_t)fw * fh * 4);
    if (!px)
        return;
    glReadBuffer(GL_BACK);
    glFinish();
    glReadPixels(0, 0, fw, fh, GL_RGBA, GL_UNSIGNED_BYTE, px);
    for (y = 0; y < fh; y += s) {
        for (x = 0; x < fw; x += s) {
            /* the shader writes the true (r, g, b) for a source 0x00RRGGBB */
            uint32_t su = bgra[(h - 1 - y / s) * w + x / s];   /* upright   */
            uint32_t sd = bgra[(y / s) * w + x / s];           /* flipped   */
            const unsigned char *p = &px[((size_t)y * fw + x) * 4];

            if (p[0] != ((su >> 16) & 0xFF) || p[1] != ((su >> 8) & 0xFF) ||
                p[2] != (su & 0xFF))
                up++;
            if (p[0] != ((sd >> 16) & 0xFF) || p[1] != ((sd >> 8) & 0xFF) ||
                p[2] != (sd & 0xFF))
                dn++;
            n++;
        }
    }
    printf("sokol: GL readback f%ld: upright mismatches=%ld/%ld, "
           "flipped mismatches=%ld/%ld\n", frames, up, n, dn, n);
    free(px);
}
#endif

/* ------------------------------------------------------------- shaders ----
 *
 * D3D11 compiles HLSL source at runtime (d3dcompiler_47.dll, loaded on
 * demand, system built-in on Win8+). GL backends need GLSL source instead -
 * sokol cannot cross-compile, so both variants live here and the right one is
 * picked from sg_query_backend(). macOS/Metal would need a third one (MSL),
 * which is what sokol-shdc is for; we only add it when macOS becomes a
 * target (§13.1 cost list).
 *
 * The vertex shader is fixed; the fragment shader is generated once at init
 * so the upscale mode and the source size are compile-time constants (no
 * cbuffer / uniform-block plumbing to get wrong across backends). */

static const char *HLSL_VS =
    "struct vs_in  { float2 pos : POSITION; float2 uv : TEXCOORD0; };\n"
    "struct vs_out { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
    "vs_out main(vs_in i) {\n"
    "    vs_out o;\n"
    "    o.pos = float4(i.pos, 0.0, 1.0);\n"
    "    o.uv  = i.uv;\n"
    "    return o;\n"
    "}\n";

static const char *GLSL_VS =
    "#version 330\n"
    "layout(location=0) in vec2 pos;\n"
    "layout(location=1) in vec2 uv;\n"
    "out vec2 uvv;\n"
    "void main() {\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "    uvv = uv;\n"                 /* v is flipped in the fragment stage */
    "}\n";

/* Fallback fragment shader used only if the generated one fails to compile:
 * the original, pixel-exact NEAREST blit. */
static const char *HLSL_FS =
    "Texture2D tex : register(t0);\n"
    "SamplerState smp : register(s0);\n"
    "float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {\n"
    "    float4 c = tex.Sample(smp, uv);\n"
    "    return float4(c.b, c.g, c.r, 1.0);\n"
    "}\n";

static const char *GLSL_FS =
    "#version 330\n"
    "in vec2 uvv;\n"
    "out vec4 frag;\n"
    "uniform sampler2D tex_smp;\n"
    "void main() {\n"
    "    vec4 c = texture(tex_smp, uvv);\n"
    "    frag = vec4(c.b, c.g, c.r, 1.0);\n"
    "}\n";

/* Upscale modes. Keep the numbering in sync with the shader's MODE. */
enum { UP_NEAREST = 0, UP_LINEAR = 1, UP_SHARP = 2, UP_BICUBIC = 3 };
static int g_up_mode = UP_NEAREST;
static int g_keep_aspect = 1;      /* letterbox instead of stretching */

static int eq_ci(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static void parse_upscale(void)
{
    const char *s = getenv("FD2_UPSCALE");

    if (!s || !*s)
        return;                                   /* keep the default (nearest) */
    if (eq_ci(s, "nearest") || eq_ci(s, "off") || eq_ci(s, "0"))
        g_up_mode = UP_NEAREST;
    else if (eq_ci(s, "linear") || eq_ci(s, "bilinear"))
        g_up_mode = UP_LINEAR;
    else if (eq_ci(s, "sharp") || eq_ci(s, "hd") || eq_ci(s, "1"))
        g_up_mode = UP_SHARP;
    else if (eq_ci(s, "bicubic") || eq_ci(s, "catmull"))
        g_up_mode = UP_BICUBIC;
    else
        printf("sokol: unknown FD2_UPSCALE='%s' (nearest/linear/sharp/bicubic),"
               " using nearest\n", s);
}

static void parse_aspect(void)
{
    const char *s = getenv("FD2_ASPECT");

    if (!s || !*s)
        return;                                   /* keep the default (on) */
    if (eq_ci(s, "off") || eq_ci(s, "0") || eq_ci(s, "stretch"))
        g_keep_aspect = 0;
}

/* Largest iw:ih-aspect rectangle that fits in sw x sh, centred. */
static void fit_viewport(int sw, int sh, int iw, int ih,
                         int *vx, int *vy, int *vw, int *vh)
{
    double ar = (double)iw / (double)ih;
    int w, h;

    if ((double)sw / (double)sh > ar) {
        h = sh;
        w = (int)((double)sh * ar + 0.5);
    } else {
        w = sw;
        h = (int)((double)sw / ar + 0.5);
    }
    if (w > sw) w = sw;
    if (h > sh) h = sh;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    *vw = w; *vh = h;
    *vx = (sw - w) / 2;
    *vy = (sh - h) / 2;
}

static const char *mode_name(int mode)
{
    switch (mode) {
    case UP_NEAREST: return "nearest";
    case UP_LINEAR:  return "linear";
    case UP_SHARP:   return "sharp";
    case UP_BICUBIC: return "bicubic";
    default:         return "?";
    }
}

/* HLSL/GLSL fragment source with MODE/TEXW/TEXH defined. `hlsl` selects the
 * language; `mode` is the UP_* value; w/h are the source frame size. */
static void build_fragment_shader(char *dst, size_t cap, int hlsl,
                                  int mode, int w, int h)
{
    static const char *GLSL_FMT =
        "#version 330\n"
        "#define MODE %d\n"
        "#define TEXW %d.0\n"
        "#define TEXH %d.0\n"
        "#define SHARP 2.0\n"
        "in vec2 uvv;\n"
        "out vec4 frag;\n"
        "uniform sampler2D tex_smp;\n"
        "vec4 tap(vec2 t) { return texture(tex_smp, (t + vec2(0.5)) / vec2(TEXW, TEXH)); }\n"
        "vec4 blerp(vec2 p) {\n"
        "    vec2 b = floor(p); vec2 f = p - b;\n"
        "    vec4 a = mix(tap(b), tap(b + vec2(1.0, 0.0)), f.x);\n"
        "    vec4 c = mix(tap(b + vec2(0.0, 1.0)), tap(b + vec2(1.0, 1.0)), f.x);\n"
        "    return mix(a, c, f.y);\n"
        "}\n"
        "float cat(float x) {\n"
        "    float a = abs(x);\n"
        "    if (a < 1.0) return (1.5 * a - 2.5) * a * a + 1.0;\n"
        "    if (a < 2.0) return ((-0.5 * a + 2.5) * a - 4.0) * a + 2.0;\n"
        "    return 0.0;\n"
        "}\n"
        "vec4 bicubic(vec2 p) {\n"
        "    vec2 q = p - vec2(0.5); vec2 b = floor(q); vec2 f = q - b;\n"
        "    vec4 acc = vec4(0.0);\n"
        "    for (int j = -1; j <= 2; j++)\n"
        "        for (int i = -1; i <= 2; i++) {\n"
        "            float wt = cat(f.x - float(i)) * cat(f.y - float(j));\n"
        "            acc += tap(b + vec2(float(i), float(j))) * wt;\n"
        "        }\n"
        "    return acc;\n"
        "}\n"
        "void main() {\n"
        "    vec2 p = uvv * vec2(TEXW, TEXH);\n"
        "    vec4 c;\n"
        "    if (MODE == 0) c = tap(floor(p));\n"
        "    else if (MODE == 1) c = blerp(p - vec2(0.5));\n"
        "    else if (MODE == 2) {\n"
        "        vec2 b = floor(p); vec2 f = p - b;\n"
        "        f = clamp((f - vec2(0.5)) * SHARP + vec2(0.5), 0.0, 1.0);\n"
        "        c = blerp(b + f - vec2(0.5));\n"
        "    } else c = bicubic(p);\n"
        "    frag = vec4(c.b, c.g, c.r, 1.0);\n"
        "}\n";

    static const char *HLSL_FMT =
        "#define MODE %d\n"
        "#define TEXW %d.0\n"
        "#define TEXH %d.0\n"
        "#define SHARP 2.0\n"
        "Texture2D tex : register(t0);\n"
        "SamplerState smp : register(s0);\n"
        "float4 tap(float2 t) { return tex.Sample(smp, (t + 0.5) / float2(TEXW, TEXH)); }\n"
        "float4 blerp(float2 p) {\n"
        "    float2 b = floor(p); float2 f = p - b;\n"
        "    float4 a = lerp(tap(b), tap(b + float2(1.0, 0.0)), f.x);\n"
        "    float4 c = lerp(tap(b + float2(0.0, 1.0)), tap(b + float2(1.0, 1.0)), f.x);\n"
        "    return lerp(a, c, f.y);\n"
        "}\n"
        "float cat(float x) {\n"
        "    float a = abs(x);\n"
        "    if (a < 1.0) return (1.5 * a - 2.5) * a * a + 1.0;\n"
        "    if (a < 2.0) return ((-0.5 * a + 2.5) * a - 4.0) * a + 2.0;\n"
        "    return 0.0;\n"
        "}\n"
        "float4 bicubic(float2 p) {\n"
        "    float2 q = p - float2(0.5, 0.5); float2 b = floor(q); float2 f = q - b;\n"
        "    float4 acc = float4(0.0, 0.0, 0.0, 0.0);\n"
        "    for (int j = -1; j <= 2; j++)\n"
        "        for (int i = -1; i <= 2; i++) {\n"
        "            float wt = cat(f.x - (float)i) * cat(f.y - (float)j);\n"
        "            acc += tap(b + float2((float)i, (float)j)) * wt;\n"
        "        }\n"
        "    return acc;\n"
        "}\n"
        "float4 main(float4 pos : SV_Position, float2 uvv : TEXCOORD0) : SV_Target {\n"
        "    float2 p = uvv * float2(TEXW, TEXH);\n"
        "    float4 c;\n"
        "    if (MODE == 0) c = tap(floor(p));\n"
        "    else if (MODE == 1) c = blerp(p - float2(0.5, 0.5));\n"
        "    else if (MODE == 2) {\n"
        "        float2 b = floor(p); float2 f = p - b;\n"
        "        f = clamp((f - float2(0.5, 0.5)) * SHARP + float2(0.5, 0.5), 0.0, 1.0);\n"
        "        c = blerp(b + f - float2(0.5, 0.5));\n"
        "    } else c = bicubic(p);\n"
        "    return float4(c.b, c.g, c.r, 1.0);\n"
        "}\n";

    const char *fmt = hlsl ? HLSL_FMT : GLSL_FMT;
    snprintf(dst, cap, fmt, mode, w, h);
}

static const float QUAD[6 * 4] = {
    /* x, y, u, v   (v = 0 is the top row on D3D) */
    -1.0f, -1.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 1.0f, 1.0f,
    -1.0f,  1.0f, 0.0f, 0.0f,
     1.0f, -1.0f, 1.0f, 1.0f,
     1.0f,  1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 0.0f,
};

static sg_image    g_img;
static sg_view     g_view;
static sg_sampler  g_smp;
static sg_buffer   g_vbuf;
static sg_shader   g_shd;
static sg_pipeline g_pipe;
static int         g_ok;
static int         g_w = 320, g_h = 200;

static const char *backend_name(sg_backend b)
{
    switch (b) {
    case SG_BACKEND_GLCORE:      return "GLCORE";
    case SG_BACKEND_GLES3:       return "GLES3";
    case SG_BACKEND_D3D11:       return "D3D11";
    case SG_BACKEND_METAL_MACOS: return "METAL";
    case SG_BACKEND_WGPU:        return "WGPU";
    case SG_BACKEND_VULKAN:      return "VULKAN";
    default:                     return "DUMMY/unknown";
    }
}

/* Create g_shd/g_pipe for the given source pair. Returns 1 only when both the
 * shader and the pipeline are in SG_RESOURCESTATE_VALID, so a runtime shader
 * compile failure (a bad HLSL/GLSL backend, a driver that drops the shader)
 * cannot silently present a black window. */
static int make_program(const char *vs, const char *fs)
{
    sg_shader_desc sd;
    sg_pipeline_desc pd;
    sg_environment env = sglue_environment();

    memset(&sd, 0, sizeof sd);
    sd.vertex_func.source = vs;
    sd.vertex_func.entry  = "main";
    sd.fragment_func.source = fs;
    sd.fragment_func.entry  = "main";
    sd.attrs[0].base_type      = SG_SHADERATTRBASETYPE_FLOAT;
    sd.attrs[0].glsl_name       = "pos";
    sd.attrs[0].hlsl_sem_name   = "POSITION";
    sd.attrs[0].hlsl_sem_index  = 0;
    sd.attrs[1].base_type      = SG_SHADERATTRBASETYPE_FLOAT;
    sd.attrs[1].glsl_name       = "uv";
    sd.attrs[1].hlsl_sem_name   = "TEXCOORD";
    sd.attrs[1].hlsl_sem_index  = 0;
    sd.views[0].texture.stage       = SG_SHADERSTAGE_FRAGMENT;
    sd.views[0].texture.image_type  = SG_IMAGETYPE_2D;
    sd.views[0].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
    sd.views[0].texture.hlsl_register_t_n = 0;
    sd.samplers[0].stage      = SG_SHADERSTAGE_FRAGMENT;
    sd.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;
    sd.samplers[0].hlsl_register_s_n = 0;
    sd.texture_sampler_pairs[0].stage      = SG_SHADERSTAGE_FRAGMENT;
    sd.texture_sampler_pairs[0].view_slot  = 0;
    sd.texture_sampler_pairs[0].sampler_slot = 0;
    sd.texture_sampler_pairs[0].glsl_name  = "tex_smp";
    sd.label = "fd2 blit";
    g_shd = sg_make_shader(&sd);
    if (sg_query_shader_state(g_shd) != SG_RESOURCESTATE_VALID)
        return 0;

    memset(&pd, 0, sizeof pd);
    pd.shader                 = g_shd;
    pd.layout.buffers[0].stride = 16;
    pd.layout.attrs[0].buffer_index = 0;
    pd.layout.attrs[0].offset = 0;
    pd.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
    pd.layout.attrs[1].buffer_index = 0;
    pd.layout.attrs[1].offset = 8;
    pd.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
    pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pd.cull_mode      = SG_CULLMODE_NONE;
    pd.colors[0].pixel_format = env.defaults.color_format;
    pd.sample_count   = env.defaults.sample_count;
    pd.label          = "fd2 blit";
    g_pipe = sg_make_pipeline(&pd);
    return sg_query_pipeline_state(g_pipe) == SG_RESOURCESTATE_VALID;
}

int render_init(const render_desc *desc)
{
    sg_environment env;
    sg_image_desc id;
    sg_buffer_desc bd;
    const char *vs;
    char fs_buf[8192];
    int hlsl;

    if (desc && desc->logical_w > 0 && desc->logical_h > 0) {
        g_w = desc->logical_w;
        g_h = desc->logical_h;
    }
    parse_upscale();
    parse_aspect();

    env = sglue_environment();
    {
        sg_desc gd;
        memset(&gd, 0, sizeof gd);
        gd.environment = env;
        gd.logger.func = slog_func;
        sg_setup(&gd);
    }
    if (sg_query_backend() == SG_BACKEND_DUMMY) {
        printf("sokol: sg_setup failed\n");
        return -1;
    }

    /* 320x200 texture, rewritten every frame with the guest framebuffer */
    memset(&id, 0, sizeof id);
    id.type            = SG_IMAGETYPE_2D;
    id.usage.dynamic_update = true;
    id.width           = g_w;
    id.height          = g_h;
    id.pixel_format    = SG_PIXELFORMAT_RGBA8;
    id.num_mipmaps     = 1;
    id.label           = "fd2 framebuffer";
    g_img = sg_make_image(&id);

    {
        sg_view_desc vd;
        memset(&vd, 0, sizeof vd);
        vd.texture.image = g_img;
        g_view = sg_make_view(&vd);
    }
    {
        sg_sampler_desc smd;
        memset(&smd, 0, sizeof smd);
        /* NEAREST: all smoothing is done by hand in the fragment shader, so
         * the hardware filter must not add its own. Keeps nearest mode (and
         * the GL readback orientation check) pixel-exact. */
        smd.min_filter = SG_FILTER_NEAREST;
        smd.mag_filter = SG_FILTER_NEAREST;
        smd.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        smd.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        smd.label  = "fd2 nearest";
        g_smp = sg_make_sampler(&smd);
    }

    memset(&bd, 0, sizeof bd);
    bd.usage.vertex_buffer = true;
    bd.usage.immutable     = true;
    bd.data.ptr = QUAD;
    bd.data.size = sizeof QUAD;
    bd.label = "fd2 quad";
    g_vbuf = sg_make_buffer(&bd);

    /* pick the shader language the active backend can actually compile */
    hlsl = (sg_query_backend() == SG_BACKEND_D3D11 ||
            sg_query_backend() == SG_BACKEND_METAL_MACOS);
    vs = hlsl ? HLSL_VS : GLSL_VS;

    build_fragment_shader(fs_buf, sizeof fs_buf, hlsl, g_up_mode, g_w, g_h);
    if (!make_program(vs, fs_buf)) {
        printf("sokol: '%s' upscale shader failed to build, falling back to "
               "nearest\n", mode_name(g_up_mode));
        g_up_mode = UP_NEAREST;
        build_fragment_shader(fs_buf, sizeof fs_buf, hlsl, UP_NEAREST, g_w, g_h);
        if (!make_program(vs, fs_buf) &&
            !make_program(vs, hlsl ? HLSL_FS : GLSL_FS)) {
            printf("sokol: shader creation failed\n");
            return -1;
        }
    }

    g_ok = 1;
    printf("sokol: backend=%s image=%dx%d img=%d view=%d sampler=%d vbuf=%d "
           "shader=%d pip=%d upscale=%s\n",
           backend_name(sg_query_backend()), g_w, g_h,
           g_img.id, g_view.id, g_smp.id, g_vbuf.id, g_shd.id, g_pipe.id,
           mode_name(g_up_mode));
    return 0;
}

void render_present(const uint32_t *bgra, int w, int h)
{
    sg_image_data data;
    sg_pass pass;
    sg_bindings binds;
#if defined(SOKOL_GLCORE)
    static uint32_t pattern[320 * 200];

    if (getenv("FD2_TESTPATTERN")) {
        /* Diagnostic: a static pattern with an unmistakable orientation -
         * green ramps with the source row, a white block at the top-left and
         * a red block at the bottom-right. FD2_GL_READBACK then tells
         * orientation without any animation noise. */
        int i, r, c;
        for (i = 0; i < 320 * 200; i++)
            pattern[i] = ((uint32_t)((i / 320) * 255 / 199) << 8) | 0x00000080u;
        for (r = 0; r < 8; r++)
            for (c = 0; c < 8; c++) {
                pattern[r * 320 + c] = 0x00FFFFFFu;                  /* white, TL */
                pattern[(199 - r) * 320 + (319 - c)] = 0x000000FFu;  /* red, BR   */
            }
        bgra = pattern;
        w = 320;
        h = 200;
    }
#endif

    if (!g_ok || !bgra)
        return;

    memset(&data, 0, sizeof data);
    data.mip_levels[0].ptr  = bgra;
    data.mip_levels[0].size = (size_t)w * (size_t)h * 4;
    sg_update_image(g_img, &data);

    memset(&pass, 0, sizeof pass);
    pass.action.colors[0].load_action  = SG_LOADACTION_CLEAR;
    pass.action.colors[0].clear_value.r = 0.0f;
    pass.action.colors[0].clear_value.g = 0.0f;
    pass.action.colors[0].clear_value.b = 0.0f;
    pass.action.colors[0].clear_value.a = 1.0f;
    pass.swapchain = sglue_swapchain();
    if (pass.swapchain.invalid)
        return;

    {
        static int logged;
        if (!logged) {
            logged = 1;
            printf("sokol: swapchain=%dx%d sample=%d color_format=%d (framebuffer %dx%d)\n",
                   pass.swapchain.width, pass.swapchain.height,
                   pass.swapchain.sample_count, (int)pass.swapchain.color_format,
                   g_w, g_h);
        }
    }
    sg_begin_pass(&pass);

    memset(&binds, 0, sizeof binds);
    binds.vertex_buffers[0] = g_vbuf;
    binds.views[0]          = g_view;
    binds.samplers[0]       = g_smp;

    sg_apply_pipeline(g_pipe);
    /* Aspect-preserving letterbox: sg_begin_pass() reset the viewport to the
     * whole framebuffer, so confine it to a centred 8:5 rectangle (the bars
     * keep the black clear colour). */
    {
        int vx = 0, vy = 0, vw = pass.swapchain.width, vh = pass.swapchain.height;
        if (g_keep_aspect && w > 0 && h > 0)
            fit_viewport(pass.swapchain.width, pass.swapchain.height,
                         w, h, &vx, &vy, &vw, &vh);
        sg_apply_viewport(vx, vy, vw, vh, true);
    }
    sg_apply_bindings(&binds);
    sg_draw(0, 6, 1);
    sg_end_pass();
#if defined(SOKOL_GLCORE)
    if (g_up_mode == UP_NEAREST)         /* a smoother filter legitimately
                                          * changes the pixels */
        gl_readback_check(bgra, w, h);
#endif
    sg_commit();
}

void render_resize(int w, int h)
{
    /* the swapchain follows the window automatically; nothing to do */
    (void)w; (void)h;
}

void render_shutdown(void)
{
    if (g_ok) {
        sg_shutdown();
        g_ok = 0;
    }
}

const char *render_name(void)
{
    return "sokol";
}
