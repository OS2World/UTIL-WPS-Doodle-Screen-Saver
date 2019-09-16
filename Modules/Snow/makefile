#                      -- Makefile to create DLL file --

#-----------------------------------------------------------------------------
# This section should be modified according to the target to build!
#-----------------------------------------------------------------------------
dllname=Snow
object_files=Snow.obj FrameRate.obj msgx.obj

# Create debug build?
#debug_build=defined

# Define the following if there is a resource file to be used, and also 
# define the (rcname) to the name of RC file to be used
has_resource_file=defined
rcname=Snow-Resources

# The result can be LXLite'd too
create_packed_dll=defined

#-----------------------------------------------------------------------------
# The next part is somewhat general, for creation of DLL files.
#-----------------------------------------------------------------------------

!ifdef debug_build
debugflags = -d2 -dDEBUG_BUILD
!else
debugflags =
!endif

dllflags = -bd
cflags = -zq $(debugflags) -bm -bt=OS2 -6s -fpi87 -fp6 -sg -otexanr -wx

.before
    rm -f description.lnk
    ..\..\AddToFile.cmd description.lnk,option description,BLDLEVEL,DRY,1.70,Snow - ScreenSaver module,v24
    set include=$(%os2tk)\h;$(%include);..\..

.extensions:
.extensions: .dll .obj .c

all : $(dllname).dll

$(dllname).dll: Snow-Images.h $(object_files)

Snow-Images.h: MakeData.exe
    MakeData

MakeData.exe: MakeData.c
    wcl386 -zq MakeData

.c.obj : .AUTODEPEND
    wcc386 $[* $(dllflags) $(cflags)

.obj.dll : .AUTODEPEND
    wlink @$(dllname) @description
!ifdef has_resource_file
    rc $(rcname) $(dllname).dll
!endif
!ifdef create_packed_dll
    lxlite $(dllname).dll
!endif

clean : .SYMBOLIC
        @if exist *.dll del *.dll
        @if exist *.obj del *.obj
        @if exist *.map del *.map
        @if exist *.res del *.res
        @if exist *.lst del *.lst
