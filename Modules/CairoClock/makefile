#                      -- Makefile to create DLL file --

#-----------------------------------------------------------------------------
# This section should be modified according to the target to build!
#-----------------------------------------------------------------------------
dllname=CaClock
object_files=CaClock.obj MSGX.obj

# Create debug build?
#debug_build=defined

# Define the following if there is a resource file to be used, and also 
# define the (rcname) to the name of RC file to be used
has_resource_file=defined
rcname=CaClock-Resources

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
cflags = $(debugflags) -zq -bm -bt=OS2 -6s -fp6 -sg -otexanr -wx

.before
    rm -f description.lnk
    ..\..\AddToFile.cmd description.lnk,option description,BLDLEVEL,DRY,2.12,Cairo Clock - ScreenSaver module,v2.5
    @set include=$(%os2tk)\h;$(%include);$(%UNIXROOT)\usr\include;..\..

.extensions:
.extensions: .dll .obj .c

all : $(dllname).dll

$(dllname).dll: $(object_files)
    @wlink system 386 lx dll INITINSTANCE TERMINSTANCE name $(dllname) &
    f CaClock f MSGX op MAP=$(dllname) op DESC 'Cairo Clock - ScreenSaver module' &
    op ELIMINATE op MANYAUTODATA op OSNAME='OS/2 and eComStation' op SHOWDEAD &
    LIBPATH %os2tk%\lib LIBRARY libuls.lib LIBRARY libconv.lib @Imports.lnk @description

!ifdef has_resource_file
    rc $(rcname) $(dllname).dll
!endif
!ifdef create_packed_dll
    lxlite $(dllname).dll
!endif

.c.obj : .AUTODEPEND
    @wcc386 $[* $(dllflags) $(cflags)

clean : .SYMBOLIC
        @if exist *.dll del *.dll
        @if exist *.obj del *.obj
        @if exist *.map del *.map
        @if exist *.res del *.res
        @if exist *.lst del *.lst
