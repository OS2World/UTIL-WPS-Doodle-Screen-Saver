#                      -- Makefile to create DLL file --

#-----------------------------------------------------------------------------
# This section should be modified according to the target to build!
#-----------------------------------------------------------------------------
dllname=SldShow
object_files=SldShow.obj msgx.obj log.obj gbmfunc.obj

# Create debug build?
#debug_build=defined

# Define the following if there is a resource file to be used, and also
# define the (rcname) to the name of RC file to be used
has_resource_file=defined
rcname=SldShow-Resources

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
# Org. Doodle
# cflags = $(debugflags) -bm -bt=OS2 -6s -fp6 -sg -otexan -wx
# New for GBM from Heiko
cflags = $(debugflags) -bm -sg -onatbrmkiel+ -ei -6 -fp6 -mf -wx -zq

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

clean : .SYMBOLIC
        @if exist *.dll del *.dll
        @if exist *.obj del *.obj
        @if exist *.map del *.map
        @if exist *.res del *.res
        @if exist *.lst del *.lst
