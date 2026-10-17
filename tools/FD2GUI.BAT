@echo off
rem ============================================================
rem  FD2 save editor (GUI)
rem  tkinter cannot be installed via pip; this launcher picks a
rem  CPython that ships with tkinter and starts it windowless.
rem ============================================================
setlocal
cd /d "%~dp0"

set "PYW=C:\Users\Ted\AppData\Roaming\uv\python\cpython-3.12-windows-x86_64-none\pythonw.exe"
set "PY=C:\Users\Ted\AppData\Roaming\uv\python\cpython-3.12-windows-x86_64-none\python.exe"

if not exist "%PY%" (
  for /f "delims=" %%P in ('where python.exe 2^>nul') do if not defined PY set "PY=%%P"
  for /f "delims=" %%P in ('where pythonw.exe 2^>nul') do if not defined PYW set "PYW=%%P"
)

if not exist "%PY%" (
  echo Python not found. Install CPython 3.x with tcl/tk first.
  pause
  exit /b 1
)

"%PY%" -c "import tkinter" >nul 2>&1
if errorlevel 1 (
  echo This Python has no tkinter. tkinter cannot be pip-installed;
  echo install the official CPython installer, or use fd2sav.py instead.
  pause
  exit /b 1
)

if not exist "%PYW%" set "PYW=%PY%"
start "" "%PYW%" "%~dp0fd2sav_gui.py" %*
exit /b 0
