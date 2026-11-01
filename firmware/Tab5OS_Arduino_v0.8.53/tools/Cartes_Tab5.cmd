@echo off
cd /d "%~dp0"
where py >nul 2>nul
if not errorlevel 1 (
    py -3 cartes_tab5.py
    if errorlevel 1 pause
    exit /b
)
where python >nul 2>nul
if not errorlevel 1 (
    python cartes_tab5.py
    if errorlevel 1 pause
    exit /b
)
echo Installer Python 3.10 ou plus recent depuis python.org, puis relancer ce fichier.
pause
