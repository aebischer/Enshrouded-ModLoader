@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -Command "Get-Content src/dinput8.def | Select-Object -Skip 1 | Set-Content build/analyzed-exports.def -Encoding ASCII"
if errorlevel 1 exit /b 1
pushd build
cl /nologo /std:c++17 /utf-8 /EHsc /MT /W4 /analyze /LD /I..\third_party\minhook\include ..\src\loader.cpp buffer.obj hook.obj trampoline.obj hde64.obj bcrypt.lib user32.lib /Fo:analyzed-loader.obj /Fe:analyzed-loader.dll /link /DEF:analyzed-exports.def
set result=%errorlevel%
popd
exit /b %result%
