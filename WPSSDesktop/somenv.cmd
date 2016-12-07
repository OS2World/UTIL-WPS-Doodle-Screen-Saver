@echo off

rem Set the path to the som root directory here:
rem We'll assume that you have it under your os/2 toolkit folder.

set SOMBASE=%os2tk%\som

if "%SOMBASE%" == "" goto firstime

set SMINCLUDE=.;%SOMBASE%\include;%SOMBASE%\..\h;%SOMBASE%\..\idl;
set SMTMP=%SOMBASE%\tmp
set PATH=%SOMBASE%\bin;%PATH%
set INCLUDE=.;%SOMBASE%\include;%os2tk%\h;%INCLUDE%
set DPATH=%SOMBASE%\msg;%DPATH%
set LIB=.;%SOMBASE%\lib;%LIB%
set BEGINLIBPATH=%BEGINLIBPATH%;%SOMBASE%\lib
goto end

:firstime
echo Edit SOMENV.CMD to set SOMBASE.

:end
