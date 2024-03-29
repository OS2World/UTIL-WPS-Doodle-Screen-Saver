rem @echo off
@echo Creating bldlevel Description linker file
del description.lnk
call ..\AddToFile.cmd description.lnk,option description,BLDLEVEL,DRY,1.50,ScreenSaver-extended WPDesktop class (WPSSDesktop),v2.5
@echo Creating header file from IDL file...
sc -mnotc -maddstar -mpbl -s"h;ih" WPSSDesktop.idl
@echo Compiling MSGX.c file...
wcc386 -zq MSGX.c -bd -bm -bt=OS2 -6s -fpi87 -fp6 -sg -otexanr -wx -wcd=1177 -I..
@echo Compiling WPSSDesktop.c file...
wcc386 -zq WPSSDesktop.c -bd -bm -bt=OS2 -6s -fpi87 -fp6 -sg -otexanr -wx -wcd=1177 -I..
@echo Linking (OpenWatcom v1.4 mode)...
wlink @WPSSDesk @description
if not errorlevel 1 goto linked
@echo Prevous linking failed, trying to linking in OpenWatcom v1.5 mode...
wlink @WPSSDesk-OW15 @description
if not errorlevel 1 goto linked
@echo WARNING! Linking failed!
:linked
@echo Adding resources...
rc WPSSDesktop-Resources.rc WPSSDesk.dll
@echo Packing DLL...
lxlite WPSSDesk.dll
