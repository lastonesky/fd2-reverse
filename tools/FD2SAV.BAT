@echo off
rem ===========================================================================
rem  FD2 save editor launcher  (double-click me)
rem  no arguments -> interactive menu;  with arguments -> pass through
rem      FD2SAV.BAT info
rem      FD2SAV.BAT heal
rem ===========================================================================
setlocal
set "HERE=%~dp0"
set "MPY=%USERPROFILE%\.workbuddy\binaries\python\versions\3.13.12\python.exe"

if exist "%MPY%" goto :use_managed
where py >nul 2>nul
if not errorlevel 1 goto :use_py
goto :use_plain

:use_managed
"%MPY%" "%HERE%fd2sav.py" %*
goto :done

:use_py
py -3 "%HERE%fd2sav.py" %*
goto :done

:use_plain
python "%HERE%fd2sav.py" %*
goto :done

:done
if "%~1"=="" pause
endlocal
