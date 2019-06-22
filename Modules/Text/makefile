#                      -- Makefile to create DLL file --

#-----------------------------------------------------------------------------
# This section should be modified according to the target to build!
#-----------------------------------------------------------------------------
dllname=Text
object_files=Text.obj msgx.obj

# Create debug build?
#debug_build=defined

# Define the following if there is a resource file to be used, and also 
# define the (rcname) to the name of RC file to be used
has_resource_file=defined
rcname=Text-Resources

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
cflags = $(debugflags) -bm -bt=OS2 -6s -fp6 -sg -otexan -wx

.before
    set include=$(%os2tk)\h;$(%include);..\..

.extensions:
.extensions: .dll .obj .c

all : $(dllname).dll

$(dllname).dll: $(object_files)

.c.obj : .AUTODEPEND
    wcc386 $[* $(dllflags) $(cflags)

.obj.dll : .AUTODEPEND
    wlink @$(dllname)
!ifdef has_resource_file
    rc $(rcname) $(dllname).dll
!endif
!ifdef create_packed_dll
    lxlite $(dllname).dll
!endif

   @rem if exist E:\DEV\dssaver\15_src\Tester\Text.dll del E:\DEV\dssaver\15_src\Tester\Text.dll
   @rem copy Text.dll E:\DEV\dssaver\15_src\Tester\
   @rem if exist C:\OS2\APPS\SSaver\Modules\Text.dll del C:\OS2\APPS\SSaver\Modules\Text.dll
   @rem copy Text.dll C:\OS2\APPS\SSaver\Modules\

clean : .SYMBOLIC
        @if exist *.dll del *.dll
        @if exist *.obj del *.obj
        @if exist *.map del *.map
        @if exist *.res del *.res
        @if exist *.lst del *.lst
