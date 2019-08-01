#                      -- Makefile to create DLL file --

#-----------------------------------------------------------------------------
# This section should be modified according to the target to build!
#-----------------------------------------------------------------------------
dllname=CaShapes
object_files=General.obj ModuleSpec.obj

# Where to find the import library
cairopath=%unixroot%\usr\lib

# The import library
cairolibname=cairo_dll.lib

# Create debug build?
#debug_build=defined

# Define the following if there is a resource file to be used, and also 
# define the (rcname) to the name of RC file to be used
has_resource_file=defined
rcname=General-Resources

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
cflags = $(debugflags) -zq -bm -bt=OS2 -6s -fpi87 -fp6 -sg -otexanr -wx

.before
    @set include=$(%os2tk)\h;$(%include);$(%UNIXROOT)\usr\include\;..\..

.extensions:
.extensions: .dll .obj .c

all : $(dllname).dll

$(dllname).dll: $(object_files)
    @wlink system os2v2_dll INITINSTANCE TERMINSTANCE name $(dllname) &
    f General f ModuleSpec op MAP=$(dllname) op DESC 'ScreenSaver module' &
    op ELIMINATE op MANYAUTODATA op OSNAME='OS/2 and eComStation' op SHOWDEAD &
    libpath $(cairopath) library $(cairolibname)
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
