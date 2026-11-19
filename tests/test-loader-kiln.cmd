@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist build\kiln-test mkdir build\kiln-test
pushd build\kiln-test
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W4 /WX ..\..\tests\loader_bootstrap_test.cpp /Fe:bootstrap.exe
if errorlevel 1 exit /b 1
bootstrap.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W4 /WX ..\..\tests\loader_kiln_dll_test.cpp /Fe:native-dll.exe
if errorlevel 1 exit /b 1
cl /nologo /LD /MT ..\..\tests\loader_kiln_proxy_fixture.cpp /Fe:dinput8.dll
if errorlevel 1 exit /b 1
native-dll.exe ..\..\dist\dinput8.dll copy.dll .\dinput8.dll
if errorlevel 1 exit /b 1
copy /y ..\..\dist\dinput8.dll standalone-chain.dll >nul
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W4 /WX ..\..\tests\loader_kiln_chain_test.cpp /Fe:chain.exe
if errorlevel 1 exit /b 1
chain.exe .\dinput8.dll .\standalone-chain.dll
if errorlevel 1 exit /b 1
popd
