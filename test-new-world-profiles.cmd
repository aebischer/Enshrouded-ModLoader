@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
pushd build
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W4 /WX /I..\third_party\minhook\include ..\tests\new_world_profiles_test.cpp buffer.obj hook.obj trampoline.obj hde64.obj bcrypt.lib user32.lib /Fe:new_world_profiles_test.exe
if errorlevel 1 exit /b 1
new_world_profiles_test.exe
set result=%errorlevel%
popd
exit /b %result%
