REM note that the build seems to need x:\os2\rc.exe, so fix the last line
call owsetenv
call tkenv
call start-sdk
set PATH=w:\os2;%PATH%