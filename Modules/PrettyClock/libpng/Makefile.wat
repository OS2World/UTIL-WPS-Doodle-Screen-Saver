#=============================================================================
#          This is a Watcom makefile to build PRCLOCK.DLL for OS/2
#
#
#=============================================================================


object_files=png.obj pngerror.obj pnggccrd.obj pngget.obj pngmem.obj pngpread.obj pngread.obj pngrio.obj pngrtran.obj pngrutil.obj pngset.obj pngtrans.obj pngvcrd.obj pngwio.obj pngwrite.obj pngwtran.obj pngwutil.obj

# Extra stuffs to pass to C compiler:
ExtraCFlags=

#
#==============================================================================
#
!include ..\Watcom.mif

.before
    set include=..\zlib121;.\;$(%os2tk)\h;$(%include);

all : $(object_files)

clean : .SYMBOLIC
    @if exist *.dll del *.dll
    @if exist *.lib del *.lib
    @if exist *.obj del *.obj
    @if exist *.map del *.map
    @if exist *.res del *.res
    @if exist *.lst del *.lst
