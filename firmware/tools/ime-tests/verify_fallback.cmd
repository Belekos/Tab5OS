@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
python -X utf8 "%~dp0..\system-resources-tests\test_locale.py" "%~dp0..\..\Tab5OS_Arduino_v0.8.18_Ecriture" --cxx cl --msvc
