/* REXX */

/*
 * Copy all the required package files together so 
 * the WarpIn package can be built
 */

/* ------ Language support files ----------- */

"copy ..\WPSSDesktop\Languages\ss_zh*.hlp Languages\Chinese\"
"copy ..\WPSSDesktop\Languages\ss_zh*.msg Languages\Chinese\"

"copy ..\WPSSDesktop\Languages\ss_nl*.hlp Languages\Dutch\"
"copy ..\WPSSDesktop\Languages\ss_nl*.msg Languages\Dutch\"

"copy ..\WPSSDesktop\Languages\ss_en*.hlp Languages\English\"
"copy ..\WPSSDesktop\Languages\ss_en*.msg Languages\English\"

"copy ..\WPSSDesktop\Languages\ss_fr*.hlp Languages\French\"
"copy ..\WPSSDesktop\Languages\ss_fr*.msg Languages\French\"

"copy ..\WPSSDesktop\Languages\ss_de*.hlp Languages\German\"
"copy ..\WPSSDesktop\Languages\ss_de*.msg Languages\German\"

"copy ..\WPSSDesktop\Languages\ss_hu*.hlp Languages\Hungarian\"
"copy ..\WPSSDesktop\Languages\ss_hu*.msg Languages\Hungarian\"

"copy ..\WPSSDesktop\Languages\ss_it*.hlp Languages\Italian\"
"copy ..\WPSSDesktop\Languages\ss_it*.msg Languages\Italian\"

"copy ..\WPSSDesktop\Languages\ss_ru*.hlp Languages\Russian\"
"copy ..\WPSSDesktop\Languages\ss_ru*.msg Languages\Russian\"

"copy ..\WPSSDesktop\Languages\ss_es*.hlp Languages\Spanish\"
"copy ..\WPSSDesktop\Languages\ss_es*.msg Languages\Spanish\"

"copy ..\WPSSDesktop\Languages\ss_sv*.hlp Languages\Swedish\"
"copy ..\WPSSDesktop\Languages\ss_sv*.msg Languages\Swedish\"


/* ------ Core files ----------- */

"copy ..\SSModWrp\SSModWrp.exe Core-EXE"
"copy ..\sscore.dll Core-DLL"
"copy ..\wpssdesktop\wpssdesk.dll Core-DLL"
"copy ..\ssdpms\ssdpms.dll Core-DLL"


/* ------ Changelog (in Docs) ----------- */

"copy ..\changelog.txt Docs\"


/* ------ Modules ----------- */

fromdir="AnTVSim"
modulename="AnTVSim"
"copy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"copy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="Blank"
modulename="Blank"
"copy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"copy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="CairoClock"
modulename="CaClock"
"copy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"copy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="CairoShapes"
modulename="CaShapes"
"copy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"copy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="Circles"
modulename="Circles"
"copy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"copy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="eCSBall"
modulename="eCSBall"
"copy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"copy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="IFSIM"
modulename="IFSIM"
"copy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"copy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="Matrix"
modulename="Matrix"
"copy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"copy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="Padlock"
modulename="Padlock"
"copy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"copy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

"copy ..\Modules\PrettyClock\*.dll Modules\PrettyClock\"
"copy ..\Modules\PrettyClock\Languages\*.msg Modules\PrettyClock\PrClock\"
"xcopy /S /E ..\Modules\PrettyClock\Skins\* Modules\PrettyClock\PrClock\"

fromdir="Randomizer"
modulename="Randomizer"
"copy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"copy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\Random\"

fromdir="ShowImage"
modulename="ShowImg"
"copy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"copy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="SlideShow"
modulename="SldShow"
"copy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"copy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="Snow"
modulename="Snow"
"copy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"copy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"

fromdir="Text"
modulename="Text"
"copy ..\Modules\"fromdir"\*.dll Modules\"modulename"\"
"copy ..\Modules\"fromdir"\Languages\*.msg Modules\"modulename"\"modulename"\"
