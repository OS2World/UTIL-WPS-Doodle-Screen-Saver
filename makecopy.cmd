/* Compile HLP files and Module files and copy them to the directory structure given */
/* Written by MrFawlty */

parse arg targetDir

if targetDir == "" then
do
   say "This program will build all module language files and copy them to your"
   say "operational DSSaver environment."
   say ""
   say "Usage: MAKECOPY 'dssaver directory'"
   say ""
   say "Whereby 'dssaver directory' is location of DSSaver, e.g. c:\tools\dssaver."
   say "Put this command file in the root of language packages."
   exit
end

'cd WPSSDesktop\languages'
'echo WPSSDesktop\Languages            >  ..\..\log.txt'
'call make                             >> ..\..\log.txt'
'xcopy *.hlp %1\Lang'
'cd ..\..'

'cd Modules\AnTVSim\Languages'
'echo Modules\AnTVSim\Languages        >> ..\..\..\log.txt'
'call make                             >> ..\..\..\log.txt'
'xcopy *.msg     %1\modules\AnTVSim'
'cd ..\..\..'

'cd Modules\Blank\Languages'
'echo Modules\Blank\Languages          >> ..\..\..\log.txt'
'call make                             >> ..\..\..\log.txt'
'xcopy *.msg       %1\modules\Blank'
'cd ..\..\..'

'cd Modules\CairoClock\Languages'
'echo Modules\CairoClock\Languages     >> ..\..\..\log.txt'
'call make                             >> ..\..\..\log.txt'
'xcopy *.msg  %1\modules\CaClock'
'cd ..\..\..'

'cd Modules\Circles\Languages'
'echo Modules\Circles\Languages        >> ..\..\..\log.txt'
'call make                             >> ..\..\..\log.txt'
'xcopy *.msg     %1\modules\Circles'
'cd ..\..\..'

'cd Modules\eCSBall\Languages'
'echo Modules\eCSBall\Languages        >> ..\..\..\log.txt'
'call make                             >> ..\..\..\log.txt'
'xcopy *.msg     %1\modules\eCSBall'
'cd ..\..\..'

'cd Modules\IFSIM\Languages'
'echo Modules\IFSIM\Languages          >> ..\..\..\log.txt'
'call make                             >> ..\..\..\log.txt'
'xcopy *.msg       %1\modules\IFSIM'
'cd ..\..\..'

'cd Modules\Matrix\Languages'
'echo Modules\Matrix\Languages         >> ..\..\..\log.txt'
'call make                             >> ..\..\..\log.txt'
'xcopy *.msg      %1\modules\Matrix'
'cd ..\..\..'

'cd Modules\Padlock\Languages'
'echo Modules\Padlock\Languages        >> ..\..\..\log.txt'
'call make                             >> ..\..\..\log.txt'
'xcopy *.msg     %1\modules\Padlock'
'cd ..\..\..'

'cd Modules\PrettyClock\Languages'
'echo Modules\PrettyClock\Languages    >> ..\..\..\log.txt'
'call make                             >> ..\..\..\log.txt'
'xcopy *.msg %1\modules\PrClock'
'cd ..\..\..'

'cd Modules\Randomizer\Languages'
'echo Modules\Randomizer\Languages     >> ..\..\..\log.txt'
'call make                             >> ..\..\..\log.txt'
'xcopy *.msg  %1\modules\Random'
'cd ..\..\..'

'cd Modules\ShowImage\Languages'
'echo Modules\ShowImage\Languages      >> ..\..\..\log.txt'
'call make                             >> ..\..\..\log.txt'
'xcopy *.msg   %1\modules\ShowImg'
'cd ..\..\..'

'cd Modules\SlideShow\Languages'
'echo Modules\SlideShow\Languages      >> ..\..\..\log.txt'
'call make                             >> ..\..\..\log.txt'
'xcopy *.msg   %1\modules\SldShow'
'cd ..\..\..'

'cd Modules\Snow\Languages'
'echo Modules\Snow\Languages           >> ..\..\..\log.txt'
'call make                             >> ..\..\..\log.txt'
'xcopy *.msg        %1\modules\Snow'
'cd ..\..\..'

'cd Modules\Text\Languages'
'echo Modules\Text\Languages           >> ..\..\..\log.txt'
'call make                             >> ..\..\..\log.txt'
'xcopy *.msg        %1\modules\Text'
'cd ..\..\..'
exit
