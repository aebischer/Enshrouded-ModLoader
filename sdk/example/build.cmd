@echo off
setlocal
where cl >nul 2>nul
if errorlevel 1 (
 echo Open an x64 Native Tools Command Prompt for Visual Studio first.
 exit /b 1
)
set "package=%~1"
if not defined package set "package=%~dp0package"
if not exist "%package%\Mods\example" mkdir "%package%\Mods\example"
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W4 /WX /LD /I"%~dp0..\include" "%~dp0example_mod.cpp" /Fo:"%package%\example_mod.obj" /Fe:"%package%\Mods\example\example_mod.dll" /link /IMPLIB:"%package%\example_mod.lib"
if errorlevel 1 exit /b 1
copy /y "%~dp0example_mod.dll.mod" "%package%\Mods\example\example_mod.dll.mod" >nul
exit /b %errorlevel%
