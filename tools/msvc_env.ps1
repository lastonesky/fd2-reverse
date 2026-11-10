# msvc_env.ps1 - make cl.exe usable in the current PowerShell session.
#
#   . "$root\tools\msvc_env.ps1"
#   Set-MsvcEnvironment
#
# 1. Prefer vcvars64.bat (a real dev environment: also sets link.exe's paths).
# 2. If that leaves no cl.exe on PATH, rebuild INCLUDE/LIB/PATH straight from
#    the installation layout.  vcvars64.bat shells out to reg.exe, which some
#    locked-down shells block - the build then failed for no real reason.
#
# Returns the vcvars path it used, or $null when it fell back.

function Set-MsvcEnvironment {
    param([switch]$Quiet)

    $vcvars = @(
        "C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1

    if (-not $env:VSCMD_VER -and $vcvars) { cmd /c "`"$vcvars`" >nul 2>&1" }
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) { return $vcvars }

    if (-not $Quiet) {
        Write-Host "== vcvars unavailable - reconstructing the MSVC/SDK environment =="
    }
    # Depth 5 from the VS root reaches <year>\<edition>\VC\Tools\MSVC.
    # NB: the VC include dir holds the C++ headers (vcruntime.h) but not the CRT
    # ones (stdio.h/stddef.h) - those come from the SDK's ucrt, so probe for a
    # VC-only header here.
    $msvc = Get-ChildItem "C:\Program Files\Microsoft Visual Studio","C:\Program Files (x86)\Microsoft Visual Studio" `
                -Directory -Recurse -Depth 5 -Filter "MSVC" -ErrorAction SilentlyContinue |
            ForEach-Object { Get-ChildItem $_.FullName -Directory -ErrorAction SilentlyContinue } |
            Where-Object { Test-Path (Join-Path $_.FullName "include\vcruntime.h") } |
            Sort-Object { [version]($_.Name + '.0') } -Descending | Select-Object -First 1
    if (-not $msvc) { throw "MSVC toolset with headers not found" }

    $sdkRoot = "C:\Program Files (x86)\Windows Kits\10"
    $sdk = Get-ChildItem (Join-Path $sdkRoot "Include") -Directory -ErrorAction SilentlyContinue |
           Where-Object { Test-Path (Join-Path $_.FullName "ucrt\string.h") } |
           Sort-Object Name -Descending | Select-Object -First 1
    if (-not $sdk) { throw "Windows SDK headers not found under $sdkRoot" }

    $env:INCLUDE = "$($sdk.FullName)\ucrt;$($sdk.FullName)\um;$($sdk.FullName)\shared;$($msvc.FullName)\include"
    $env:LIB     = "$sdkRoot\Lib\$($sdk.Name)\ucrt\x64;$sdkRoot\Lib\$($sdk.Name)\um\x64;$($msvc.FullName)\lib\x64"
    $env:PATH    = "$($msvc.FullName)\bin\Hostx64\x64;$env:PATH"
    if (-not $Quiet) { Write-Host "   msvc=$($msvc.Name)  sdk=$($sdk.Name)" }
    return $null
}
