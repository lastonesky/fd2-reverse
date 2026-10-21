# run_tests.ps1 - build and run the restored-logic regression tests.
#
#   pwsh -File tools/run_tests.ps1
#
# Each test links real restored game modules with small service stubs:
#   tools/tests/move_check.c      map_move_toward movement budget (+0x3B vs +0x20)
#   tools/tests/unit_anim_check.c map_units_animate edge-cell pointer offsets
# Only the generated data-segment image is needed (from the user's FD2.EXE).
$ErrorActionPreference = "Stop"

$root   = Split-Path $PSScriptRoot -Parent
$src    = Join-Path $root "src"
$out    = Join-Path $root "build"
$obj    = Join-Path $out  "tests.obj"
$vcvars = @(
    "C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vcvars) { throw "vcvars64.bat not found" }

New-Item -ItemType Directory -Force -Path $out | Out-Null
New-Item -ItemType Directory -Force -Path $obj | Out-Null

if (-not (Test-Path (Join-Path $src "fd2_image.c"))) {
    $python = Get-Command python -ErrorAction SilentlyContinue
    if (-not $python) { $python = Get-Command py -ErrorAction SilentlyContinue }
    if (-not $python) { throw "python not found - needed for prepare_image.py" }
    & $python.Source (Join-Path $root "tools/prepare_image.py") | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "prepare_image.py failed" }
}

$cflags = "/nologo /W3 /utf-8 /wd4018 /D_CRT_SECURE_NO_WARNINGS /O2"
$inc    = "/I `"$src`" /I `"$src\game`" /I `"$src\include`""

$tests = @(
    @{ name = "move_check";      srcs = @("src/game/map_move.c", "src/game/path.c", "src/game/tables.c", "src/game/guest_mem.c") },
    @{ name = "unit_anim_check"; srcs = @("src/game/map_units.c", "src/game/guest_mem.c", "src/game/vga.c") }
)

foreach ($t in $tests) {
    $files = @("tools/tests/$($t.name).c") + $t.srcs + @("src/fd2_native.c", "src/fd2_image.c") |
        ForEach-Object { Join-Path $root $_ }
    $exe = Join-Path $out "$($t.name).exe"
    $line = "`"$vcvars`" >nul && cd /d `"$obj`" && cl $cflags $inc /Fe:`"$exe`" $($files -join ' ')"
    cmd /c $line
    if ($LASTEXITCODE -ne 0) { throw "$($t.name) build failed" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "$($t.name) FAILED" }
}
