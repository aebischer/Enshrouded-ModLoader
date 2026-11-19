@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
pushd build
cl /nologo /std:c++17 /EHsc /MT /LD ..\tests\mod_test_plugin.cpp /Fe:mod_test_plugin.dll
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /MT /LD /DMOD_TEST_REJECT ..\tests\mod_test_plugin.cpp /Fe:mod_test_reject.dll
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /MT /I..\third_party\minhook\include ..\tests\mod_profiles_test.cpp buffer.obj hook.obj trampoline.obj hde64.obj bcrypt.lib user32.lib /Fe:mod_profiles_test.exe
if errorlevel 1 exit /b 1
mod_profiles_test.exe
set result=%errorlevel%
popd
exit /b %result%
