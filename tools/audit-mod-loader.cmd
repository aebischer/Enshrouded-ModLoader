@echo off
rem The historical probes are now assertions in the regression suite.
call "%~dp0analyze-loader.cmd"
if errorlevel 1 exit /b 1
call "%~dp0..\test-mod-hardening.cmd"
exit /b %errorlevel%
