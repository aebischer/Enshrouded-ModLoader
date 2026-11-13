@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist dist\test-mods mkdir dist\test-mods
pushd build
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W4 /WX /LD ..\src\test_mods\empty_mod.cpp /Fo:empty_mod_a.obj /Fe:..\dist\test-mods\Test_A.dll /link /IMPLIB:empty_mod_a.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W4 /WX /LD /DTEST_MOD_B ..\src\test_mods\empty_mod.cpp /Fo:empty_mod_b.obj /Fe:..\dist\test-mods\Test_B.dll /link /IMPLIB:empty_mod_b.lib
if errorlevel 1 exit /b 1
popd
copy /y src\test_mods\test_a.manifest dist\test-mods\Test_A.dll.mod >nul
if errorlevel 1 exit /b 1
copy /y src\test_mods\test_b.manifest dist\test-mods\Test_B.dll.mod >nul
if errorlevel 1 exit /b 1
pushd build
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /I..\third_party\minhook\include ..\tests\empty_mods_test.cpp buffer.obj hook.obj trampoline.obj hde64.obj bcrypt.lib user32.lib /Fe:empty_mods_test.exe
if errorlevel 1 exit /b 1
empty_mods_test.exe
set result=%errorlevel%
popd
exit /b %result%
