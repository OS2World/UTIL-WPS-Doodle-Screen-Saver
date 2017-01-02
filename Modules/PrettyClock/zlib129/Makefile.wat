#=============================================================================
#          This is a Watcom makefile to build PRCLOCK.DLL for OS/2
#
#
#=============================================================================


object_files=adler32.obj compress.obj crc32.obj deflate.obj gzclose.obj gzlib.obj gzread.obj gzwrite.obj infback.obj inffast.obj inflate.obj inftrees.obj trees.obj uncompr.obj zutil.obj

# Extra stuffs to pass to C compiler:
ExtraCFlags=

#
#==============================================================================
#
!include ..\Watcom.mif

.before
    set include=.\;$(%os2tk)\h;$(%include);

all : $(object_files)

clean : .SYMBOLIC
    @if exist *.dll del *.dll
    @if exist *.lib del *.lib
    @if exist *.obj del *.obj
    @if exist *.map del *.map
    @if exist *.res del *.res
    @if exist *.lst del *.lst
