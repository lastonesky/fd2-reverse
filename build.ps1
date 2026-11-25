# build.ps1 - build the port3 native engine as a 64-bit Windows executable.
#
#   pwsh -File build.ps1            # release GUI build - NO console window
#   pwsh -File build.ps1 -Debug     # release GUI build with symbols
#   pwsh -File build.ps1 -Console   # DEBUG ONLY: CONSOLE subsystem -> a console
#                                   # window pops up; run it from a terminal
#                                   # if you do not want the extra window
#   pwsh -File build.ps1 -Test      # build, then run tools/run_tests.ps1
#   pwsh -File build.ps1 -Trace     # add the FD2_TRACE_BOOT startup markers
#                                   # (boot.c fd2_tlog; needed by
#                                   #  testdata\boot_probe.ps1 to time a cold
#                                   #  start segment by segment; ~0 cost)
#
# The default links /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup, so normal play
# shows only the game window; diagnostics still go to FD2_LOG (default
# fd2engine.log).  -Console is purely a development aid (live stdout/stderr).
#
# Unlike port/ this is a *native 64-bit* build: the game was de-guested
# (C1..C4), so there is no LE loader, no fixed window and no /BASE trick - it
# links the restored C (src/) with port/'s proven backend (host/) and a small
# adapter/kernel layer.  Render = sokol_gfx (D3D11), audio = sokol_audio
# (WASAPI), input = sokol_app (Win32).
#
# Run the exe from the directory that holds the FD2 data files (FDOTHER.DAT,
# FDMUS.DAT, ...), e.g.  E:\FD2 .
param([switch]$Debug, [switch]$Console, [switch]$Test, [switch]$Trace)

$ErrorActionPreference = "Stop"
$root   = $PSScriptRoot
$src    = Join-Path $root "src"
$hostd  = Join-Path $root "host"
$out    = Join-Path $root "build"
$sokol  = Join-Path $hostd "vendor\sokol"

# The compiler environment is set up later via tools\msvc_env.ps1 (it handles
# both "vcvars64.bat works" and "it doesn't" - see the comment there).
if (-not (Test-Path $sokol)) { throw "vendor/sokol missing (was host/ imported?)" }
New-Item -ItemType Directory -Force -Path $out | Out-Null

$cfg    = if ($Debug) { "/Od /Zi /MDd" } else { "/O2 /MD /DNDEBUG" }
# /DNDEBUG: sokol only skips its validation/debug paths when NDEBUG is defined
#          (sokol_app.h: `#if !defined(NDEBUG) #define SOKOL_DEBUG`).  Without it
#          every start created the D3D11 *debug layer* device: +94 ms on device
#          creation (250 -> 156 ms), and the layer can refuse the device outright
#          when the Graphics Tools feature is missing.  -Debug keeps asserts. 
# /utf-8 : every source is UTF-8 (lots of Chinese comments).  Without it MSVC
#          decodes them in the system code page (936) and emits C4819 on almost
#          every file (and sokol_app.h).  Verified there are no non-ASCII string
#          literals, so setting the execution charset too is safe.
# /wd4018: signed/unsigned comparison warnings.  The restored game code compares
#          small `int` loop counters with uint32_t globals by design; the POSIX
#          build tolerates the same with -Wno-sign-compare (tools/check_compile.sh),
#          so keep the two platforms consistent instead of casting every site.
# (PowerShell variable names are case-insensitive: don't reuse $Trace.)
$traceFlag = if ($Trace) { " /DFD2_TRACE_BOOT" } else { "" }
$cflags = "/nologo /W3 /utf-8 /wd4018 /EHsc /GS- /D_CRT_SECURE_NO_WARNINGS $cfg$traceFlag"
$inc    = "/I `"$hostd`" /I `"$src`" /I `"$src\game`" /I `"$src\include`" /I `"$sokol`""

$hostSrcs = @(
    "host_args.c", "platform_win32.c", "keys.c", "keys_win32.c",
    "render_sokol.c", "sokol_impl.c",
    "audio_sokol.c", "synth.c", "xmidi.c",
    "opl3/nukedopl3.c", "opl3/opl3_emu.c",
    "fd2_audio.c", "fd2_video.c", "fd2_platform.c", "fd2_kernel.c",
    "main_sokol.c"
) | ForEach-Object { Join-Path $hostd $_ }

$genSrcs = @("fd2_native.c", "fd2_dispatch.c", "fd2_image.c", "fd2_relocate.c") |
    ForEach-Object { Join-Path $src $_ }
$gameSrcs = Get-ChildItem (Join-Path $src "game") -Filter *.c | ForEach-Object { $_.FullName }

$all = $hostSrcs + $genSrcs + $gameSrcs
$missing = $all | Where-Object { -not (Test-Path $_) }
if ($missing) { throw "missing sources: $missing" }

$libs = "user32.lib gdi32.lib winmm.lib d3d11.lib dxgi.lib shell32.lib ole32.lib"
$link = if ($Console) { "/link /SUBSYSTEM:CONSOLE /DYNAMICBASE:NO /MAP:`"$out\fd2engine.map`"" } else { "/link /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /DYNAMICBASE:NO /MAP:`"$out\fd2engine.map`"" }
$objdir = Join-Path $out "fd2engine.obj"
New-Item -ItemType Directory -Force -Path $objdir | Out-Null

# ---- data-segment image (derived from the user's FD2.EXE; not in git) -----
# src/fd2_image.c is gitignored; tools/prepare_image.py reuses analysis/seg_*.bin
# if present, otherwise extracts them from FD2.EXE, and stops with instructions
# if neither exists.  See EXTERNAL_ARTIFACTS.md.
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) { $python = Get-Command py -ErrorAction SilentlyContinue }
if (-not $python) { throw "python not found - needed for tools/prepare_image.py" }
Write-Host "== preparing data-segment image (src/fd2_image.c) =="
& $python.Source (Join-Path $root "tools\prepare_image.py")
if ($LASTEXITCODE -ne 0) { throw "tools/prepare_image.py failed - see the message above" }

# Run cl in the vcvars environment (tools\msvc_env.ps1 falls back to rebuilding
# INCLUDE/LIB from the install layout when vcvars itself is unusable).
. (Join-Path $root "tools\msvc_env.ps1")
$vcvars = Set-MsvcEnvironment

# Write the compiler invocation to a response file so the exact build is
# reproducible (and stays under the command-line limit).
$rsp = @(
    $cflags
    $inc
    "/Fo`"$objdir\\`""
    "/Fe`"$out\fd2engine.exe`""
) + $all + @($libs) + @($link)
Set-Content (Join-Path $out "build.rsp") $rsp -Encoding ascii

Write-Host "== building fd2engine.exe (x64, sokol/D3D11) =="
cmd /c "cl.exe @`"$out\build.rsp`""
if ($LASTEXITCODE -ne 0) { throw "build failed" }
Write-Host "   -> $out\fd2engine.exe"
if ($Console) {
    Write-Host "      subsystem=CONSOLE (a console window will appear - debug build)"
} else {
    Write-Host "      subsystem=WINDOWS GUI (no console window; logs -> FD2_LOG)"
}

if ($Test) {
    Write-Host "== running restored-logic regression tests =="
    & (Join-Path $root "tools\run_tests.ps1")
    if ($LASTEXITCODE -ne 0) { throw "regression tests failed" }
}
