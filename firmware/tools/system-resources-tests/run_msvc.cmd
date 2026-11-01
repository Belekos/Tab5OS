@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
python "%~dp0run.py" "%~1" --cxx cl --msvc
if errorlevel 1 exit /b 1
python "%~dp0run.py" "%~1" --cxx cl --msvc --volume
if errorlevel 1 exit /b 1
python "%~dp0test_locale.py" "%~1" --cxx cl --msvc
