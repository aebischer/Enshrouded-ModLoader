@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
pushd build
cl /nologo /std:c++17 /EHsc /MT /LD ..\tests\mod_audit_dependency.cpp /Fe:audit_dependency.dll /link /IMPLIB:audit_dependency.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /MT /LD ..\tests\mod_audit_parent.cpp audit_dependency.lib /Fe:audit_parent.dll
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /MT /LD ..\tests\mod_test_plugin.cpp /Fe:mod_test_plugin.dll
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHs /MT /LD ..\tests\mod_throw_plugin.cpp /Fe:mod_test_throw.dll
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W4 /I..\third_party\minhook\include ..\tests\mod_hardening_test.cpp buffer.obj hook.obj trampoline.obj hde64.obj bcrypt.lib user32.lib /Fe:mod_hardening_test.exe
if errorlevel 1 exit /b 1
mod_hardening_test.exe
set result=%errorlevel%
popd
exit /b %result%
