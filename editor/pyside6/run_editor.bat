@echo off
setlocal
py -3 -m pip install -r "%~dp0requirements.txt"
if errorlevel 1 exit /b 1
py -3 "%~dp0fx3_editor.py"
