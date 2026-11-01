@echo off
setlocal
where cl >nul 2>nul
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /utf-8 /I"%~dp0..\..\Tab5OS_Arduino_v0.8.18_Ecriture" "%~dp0test_shell_policy.cpp" /Fo:"%TEMP%\tab5-shell-0818.obj" /Fe:"%TEMP%\tab5-shell-0818.exe"
if errorlevel 1 exit /b 1
"%TEMP%\tab5-shell-0818.exe"
