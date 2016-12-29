/* REXX */

/*
 * Copy all the required package files together so 
 * the WarpIn package can be built
 */

/* ------ Language support files ----------- */

"xcopy ..\WPSSDesktop\Languages\ss_zh*.hlp Languages\Chinese\"
"xcopy ..\WPSSDesktop\Languages\ss_zh*.msg Languages\Chinese\"

"xcopy ..\WPSSDesktop\Languages\ss_nl*.hlp Languages\Dutch\"
"xcopy ..\WPSSDesktop\Languages\ss_nl*.msg Languages\Dutch\"

"xcopy ..\WPSSDesktop\Languages\ss_en*.hlp Languages\English\"
"xcopy ..\WPSSDesktop\Languages\ss_en*.msg Languages\English\"

"xcopy ..\WPSSDesktop\Languages\ss_fr*.hlp Languages\French\"
"xcopy ..\WPSSDesktop\Languages\ss_fr*.msg Languages\French\"

"xcopy ..\WPSSDesktop\Languages\ss_de*.hlp Languages\German\"
"xcopy ..\WPSSDesktop\Languages\ss_de*.msg Languages\German\"

"xcopy ..\WPSSDesktop\Languages\ss_hu*.hlp Languages\Hungarian\"
"xcopy ..\WPSSDesktop\Languages\ss_hu*.msg Languages\Hungarian\"

"xcopy ..\WPSSDesktop\Languages\ss_it*.hlp Languages\Italian\"
"xcopy ..\WPSSDesktop\Languages\ss_it*.msg Languages\Italian\"

"xcopy ..\WPSSDesktop\Languages\ss_ru*.hlp Languages\Russian\"
"xcopy ..\WPSSDesktop\Languages\ss_ru*.msg Languages\Russian\"

"xcopy ..\WPSSDesktop\Languages\ss_es*.hlp Languages\Spanish\"
"xcopy ..\WPSSDesktop\Languages\ss_es*.msg Languages\Spanish\"

"xcopy ..\WPSSDesktop\Languages\ss_sv*.hlp Languages\Swedish\"
"xcopy ..\WPSSDesktop\Languages\ss_sv*.msg Languages\Swedish\"


/* ------ Core files ----------- */

"xcopy ..\SSModWrp\SSModWrp.exe Core-EXE"
"xcopy ..\sscore.dll Core-DLL"
"xcopy ..\wpssdesktop\wpssdesk.dll Core-DLL"
"xcopy ..\ssdpms\ssdpms.dll Core-DLL"


/* ------ Changelog (in Docs) ----------- */

"xcopy ..\doc\changelog.txt Docs\"


/* ------ Modules ----------- */

fromdir="AnTVSim"
modulename="AnTVSim"
"xcopy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"xcopy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="AOS_Logo"
modulename="AOS_Logo"
"xcopy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"xcopy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="Blank"
modulename="Blank"
"xcopy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"xcopy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="CairoClock"
modulename="CaClock"
"xcopy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"xcopy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="CairoShapes"
modulename="CaShapes"
"xcopy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"xcopy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="Circles"
modulename="Circles"
"xcopy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"xcopy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="eCSBall"
modulename="eCSBall"
"xcopy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"xcopy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="IFSIM"
modulename="IFSIM"
"xcopy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"xcopy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="Matrix"
modulename="Matrix"
"xcopy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"xcopy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="Padlock"
modulename="Padlock"
"xcopy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"xcopy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

"xcopy ..\Modules\PrettyClock\*.dll Modules\PrettyClock\"
"xcopy ..\Modules\PrettyClock\Languages\*.msg Modules\PrettyClock\PrClock\"
"xcopy /S /E ..\Modules\PrettyClock\Skins\* Modules\PrettyClock\PrClock\"

fromdir="Randomizer"
modulename="Randomizer"
"xcopy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"xcopy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\Random\"

fromdir="ShowImage"
modulename="ShowImg"
"xcopy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"xcopy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="SlideShow"
modulename="SldShow"
"xcopy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"xcopy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="Snow"
modulename="Snow"
"xcopy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"xcopy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="Text"
modulename="Text"
"xcopy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"xcopy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"
