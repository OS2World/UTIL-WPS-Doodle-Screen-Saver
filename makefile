#                      -- Makefile to create DLL file --

#-----------------------------------------------------------------------------
# This section should be modified according to the target to build!
#-----------------------------------------------------------------------------
dllname=SSCore
object_files=SSCore.obj

# Create debug build?
#debug_build=defined

# Define the following if there is a resource file to be used, and also 
# define the (rcname) to the name of RC file to be used
#has_resource_file=defined
#rcname=SSCore

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
cflags = $(debugflags) -bm -bt=OS2 -6s -fp6 -sg -wx
# The following (full optimization) option has been removed, because
# OpenWatcom generated invalid code, and caused crash in the Thunking 
# code. (in internal_IsPMTheForegroundSession() )
#
# -otexan 

.before
    del description.lnk
    AddToFile.cmd description.lnk,option description,BLDLEVEL,DRY,2.00,Screensaver Core,v24
    set include=$(%os2tk)\h;$(%include);.;

.extensions:
.extensions: .dll .obj .c

all : $(dllname).dll

$(dllname).dll: $(object_files)

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
    $(%os2tk)\bin\implib $(dllname).lib $(dllname).dll

clean : .SYMBOLIC
        @if exist *.dll del *.dll
        @if exist *.obj del *.obj
        @if exist *.map del *.map
        @if exist *.res del *.res
        @if exist *.lst del *.lst
