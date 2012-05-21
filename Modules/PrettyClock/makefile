#                      -- Makefile to create DLL file --

#-----------------------------------------------------------------------------
# This section should be modified according to the target to build!
#-----------------------------------------------------------------------------
dllname=PRClock
object_files=PrettyClock.obj MSGX.obj

# Define the following if there is a resource file to be used, and also 
# define the (rcname) to the name of RC file to be used
has_resource_file=defined
rcname=PrettyClock-Resources

# The result can be LXLite'd too
create_packed_dll=defined

#-----------------------------------------------------------------------------
# The next part is somewhat general, for creation of DLL files.
#-----------------------------------------------------------------------------

!include Watcom.mif

.before
    set include=..\..;.\libpng;.\zlib121;$(%os2tk)\h;$(%include);

all : $(dllname).dll

$(dllname).dll: zlib libpng $(object_files)
    wlink @$(dllname)
!ifdef has_resource_file
    rc $(rcname) $(dllname).dll
!endif
!ifdef create_packed_dll
    lxlite $(dllname).dll
!endif

zlib: .SYMBOLIC
    @cd zlib121
    @wmake -h -f makefile.wat
    @cd ..

libpng: .SYMBOLIC
    @cd libpng
    @wmake -h -f makefile.wat
    @cd ..
    
clean : .SYMBOLIC
        @cd zlib121
        @wmake -h -f makefile.wat clean
        @cd ..
        @cd libpng
        @wmake -h -f makefile.wat clean
        @cd ..
        @if exist *.dll del *.dll
        @if exist *.obj del *.obj
        @if exist *.map del *.map
        @if exist *.res del *.res
        @if exist *.lst del *.lst
