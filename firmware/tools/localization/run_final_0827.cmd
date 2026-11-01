@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
python -X utf8 "%~dp0..\ime-tests\run_0827.py"
if errorlevel 1 exit /b 1
python -X utf8 "%~dp0..\ime-tests\test_shortcuts_0827.py"
if errorlevel 1 exit /b 1
python -X utf8 "%~dp0..\ime-tests\audit_0827.py"
if errorlevel 1 exit /b 1
python -X utf8 "%~dp0test_arabic.py" --lvgl "C:\workspace\tab5-deps-0815\lvgl" --sketch "%~dp0..\..\Tab5OS_Arduino_v0.8.27_Ecriture" --output "%~dp0..\verification\arabic-0827.json"
if errorlevel 1 exit /b 1
python -X utf8 "%~dp0audit_glyphs.py"
exit /b %errorlevel%
