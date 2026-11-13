@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build\hook.obj (
 echo Run build.cmd once to build the loader prerequisites.
 exit /b 1
)
call sdk\example\build.cmd "%CD%\build\sdk-package"
if errorlevel 1 exit /b 1
pushd build
cl /nologo /TC /W4 /WX /c ..\tests\mod_sdk_c_test.c /Fo:mod_sdk_c_test.obj
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W4 /WX /I..\third_party\minhook\include ..\tools\validate_mod_package.cpp buffer.obj hook.obj trampoline.obj hde64.obj bcrypt.lib user32.lib /Fe:validate_mod_package.exe
if errorlevel 1 exit /b 1
validate_mod_package.exe sdk-package example/example_mod.dll
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W4 /WX /I..\third_party\minhook\include ..\tests\mod_sdk_test.cpp buffer.obj hook.obj trampoline.obj hde64.obj bcrypt.lib user32.lib /Fe:mod_sdk_test.exe
if errorlevel 1 exit /b 1
mod_sdk_test.exe
set result=%errorlevel%
popd
exit /b %result%
