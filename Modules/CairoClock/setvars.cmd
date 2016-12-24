/* This REXX script sets some environment variables. */

/* Please change the following line to point to the directory in which you */
/* have unpacked the Cairo/2 DEV package! */

/* Note that cairo.h needs to be patched with cairo_h.patch */
/* And cairo.lib needs to be create with */
/* emximp -o cairo_dll.lib cairo2.dll */

'@set cairopath=%UNIXROOT%\usr'
'@set cairolibname=%UNIXROOT%\usr\lib\cairo_dll.lib'
'@echo CAIROPATH has been set to %CAIROPATH%'
'@echo CAIROLIBNAME has been set to %CAIROLIBNAME%'
