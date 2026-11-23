@echo off
setlocal
where py >nul 2>nul
if %errorlevel%==0 (
  py -3 "%~dp0fx3_editor.py"
) else (
  python "%~dp0fx3_editor.py"
)
