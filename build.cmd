@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build mkdir build
powershell -NoProfile -ExecutionPolicy Bypass -File sdk\tools\compile-translations.ps1 -Catalog assets\localization\loader.json -Output src\loader_texts.h -Symbol LoaderTexts
if errorlevel 1 exit /b 1
if not exist dist mkdir dist
pushd build
cl /nologo /c /O2 /MT /I..\third_party\minhook\include ..\third_party\minhook\src\buffer.c ..\third_party\minhook\src\hook.c ..\third_party\minhook\src\trampoline.c ..\third_party\minhook\src\hde\hde64.c
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /LD /I..\third_party\minhook\include ..\src\loader.cpp buffer.obj hook.obj trampoline.obj hde64.obj bcrypt.lib user32.lib /link /DEF:..\src\dinput8.def /OUT:..\dist\dinput8.dll
if errorlevel 1 exit /b 1
popd
