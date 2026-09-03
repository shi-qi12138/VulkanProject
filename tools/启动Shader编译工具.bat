@echo off
setlocal
cd /d "%~dp0"
where pyw.exe >nul 2>nul
if %errorlevel% equ 0 (
    start "" pyw.exe "%~dp0shader_compiler_gui.py"
) else (
    pythonw.exe "%~dp0shader_compiler_gui.py"
)
