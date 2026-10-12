# build.ps1 - build the port3 native engine as a 64-bit Windows executable.
#
#   pwsh -File build.ps1            # release GUI build - NO console window
#   pwsh -File build.ps1 -Debug     # release GUI build with symbols
#   pwsh -File build.ps1 -Console   # DEBUG ONLY: CONSOLE subsystem -> a console
#                                   # window pops up; run it from a terminal
#                                   # if you do not want the extra window
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
param([switch]$Debug, [switch]$Console)

$ErrorActionPreference = "Stop"
$root   = $PSScriptRoot
$src    = Join-Path $root "src"
$hostd  = Join-Path $root "host"
$out    = Join-Path $root "build"
$sokol  = Join-Path $hostd "vendor\sokol"

# VS 2022/18 Enterprise (fall back to the 2019 Professional install).
$vcvars = @(
    "C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vcvars) { throw "vcvars64.bat not found" }
if (-not (Test-Path $sokol)) { throw "vendor/sokol missing (was host/ imported?)" }
New-Item -ItemType Directory -Force -Path $out | Out-Null

$cfg    = if ($Debug) { "/Od /Zi /MDd" } else { "/O2 /MD" }
# /utf-8 : every source is UTF-8 (lots of Chinese comments).  Without it MSVC
#          decodes them in the system code page (936) and emits C4819 on almost
#          every file (and sokol_app.h).  Verified there are no non-ASCII string
#          literals, so setting the execution charset too is safe.
# /wd4018: signed/unsigned comparison warnings.  The restored game code compares
#          small `int` loop counters with uint32_t globals by design; the POSIX
#          build tolerates the same with -Wno-sign-compare (tools/check_compile.sh),
#          so keep the two platforms consistent instead of casting every site.
$cflags = "/nologo /W3 /utf-8 /wd4018 /EHsc /GS- /D_CRT_SECURE_NO_WARNINGS $cfg"
$inc    = "/I `"$hostd`" /I `"$src`" /I `"$src\game`" /I `"$src\include`" /I `"$sokol`""

$hostSrcs = @(
    "platform_win32.c", "keys.c", "keys_win32.c",
    "render_sokol.c", "sokol_impl.c",
    "audio_sokol.c", "synth.c", "xmidi.c", "dls.c",
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

# Run cl in the vcvars environment.  When already inside a dev prompt
# (VSCMD_VER set) vcvars is not re-run.
$vcprompt = if ($env:VSCMD_VER) { "" } else { "`"$vcvars`" >nul && " }
$cmdline = "$vcprompt cl $cflags $inc /Fe:`"$out\fd2engine.exe`" /Fo:`"$objdir\\`" $($all -join ' ') $libs $link"
Write-Host "== building fd2engine.exe (x64, sokol/D3D11) =="
cmd /c $cmdline
if ($LASTEXITCODE -ne 0) { throw "build failed" }
Write-Host "   -> $out\fd2engine.exe"
if ($Console) {
    Write-Host "      subsystem=CONSOLE (a console window will appear - debug build)"
} else {
    Write-Host "      subsystem=WINDOWS GUI (no console window; logs -> FD2_LOG)"
}
