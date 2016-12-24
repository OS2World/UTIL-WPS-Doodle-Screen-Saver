@echo off

echo Cleaning all...

echo * SSCORE.DLL
wmake -h clean

echo * WPSSDESK.DLL
cd wpssdesktop
del *.dll
del *.obj
cd ..

echo * TESTER
cd tester
wmake -h clean
cd ..

echo * SSDPMS
cd ssdpms
wmake -h clean
cd ..

echo * MODULES\ANTVSIM
cd Modules\AnTVSim
wmake -h clean
cd ..\..

echo * MODULES\AOS_Logo
cd Modules\AOS_Logo
wmake -h clean
cd ..\..

echo * MODULES\BLANK
cd Modules\Blank
wmake -h clean
cd ..\..

echo * MODULES\CairoClock
cd Modules\CairoClock
wmake -h clean
cd ..\..

echo * MODULES\CairoShapes
cd Modules\CairoShapes
wmake -h clean
cd ..\..

echo * MODULES\CIRCLES
cd Modules\Circles
wmake -h clean
cd ..\..

echo * MODULES\ECSBALL
cd Modules\eCSBall
wmake -h clean
cd ..\..

echo * MODULES\IFSIM
cd Modules\IFSIM
wmake -h clean
cd ..\..

echo * MODULES\MATRIX
cd Modules\Matrix
wmake -h clean
cd ..\..

echo * MODULES\PADLOCK
cd Modules\Padlock
wmake -h clean
cd ..\..

echo * MODULES\PRETTYCLOCK
cd Modules\PrettyClock
wmake -h clean
cd ..\..

echo * MODULES\RANDOMIZER
cd Modules\Randomizer
wmake -h clean
cd ..\..

echo * MODULES\SNOW
cd Modules\Snow
wmake -h clean
cd ..\..

echo * MODULES\SHOWIMAGE
cd Modules\ShowImage
wmake -h clean
cd ..\..

echo * MODULES\TEXT
cd Modules\Text
wmake -h clean
cd ..\..

echo * MODULES\SLIDESHOW
cd Modules\SlideShow
wmake -h clean
cd ..\..

echo * All cleaned!
