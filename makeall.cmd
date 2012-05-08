@echo off

echo Making all...

echo * SSCORE.DLL
wmake -h

echo * WPSSDESK.DLL
cd wpssdesktop
call somenv.cmd
call make.cmd
cd Languages
call make.cmd
cd ..
cd ..

echo * TESTER
cd tester
wmake -h
cd ..

echo * MODULES\ANTVSIM
cd Modules\AnTVSim
wmake -h
cd Languages
call make.cmd
cd ..
cd ..\..

echo * MODULES\BLANK
cd Modules\Blank
wmake -h
cd Languages
call make.cmd
cd ..
cd ..\..

echo * MODULES\CairoClock
cd Modules\CairoClock
call setvars.cmd
wmake -h
cd Languages
call make.cmd
cd ..
cd ..\..

echo * MODULES\CairoShapes
cd Modules\CairoShapes
call setvars.cmd
wmake -h
cd ..\..

echo * MODULES\CIRCLES
cd Modules\Circles
wmake -h
cd Languages
call make.cmd
cd ..
cd ..\..

echo * MODULES\ECSBALL
cd Modules\eCSBall
wmake -h
cd Languages
call make.cmd
cd ..
cd ..\..

echo * MODULES\IFSIM
cd Modules\IFSIM
wmake -h
cd Languages
call make.cmd
cd ..
cd ..\..

echo * MODULES\MATRIX
cd Modules\Matrix
wmake -h
cd Languages
call make.cmd
cd ..
cd ..\..

echo * MODULES\PADLOCK
cd Modules\Padlock
wmake -h
cd Languages
call make.cmd
cd ..
cd ..\..

echo * MODULES\PRETTYCLOCK
cd Modules\PrettyClock
wmake -h
cd Languages
call make.cmd
cd ..
cd ..\..

echo * MODULES\RANDOMIZER
cd Modules\Randomizer
wmake -h
cd Languages
call make.cmd
cd ..
cd ..\..

echo * MODULES\SNOW
cd Modules\Snow
wmake -h
cd Languages
call make.cmd
cd ..
cd ..\..

echo * MODULES\SHOWIMAGE
cd Modules\ShowImage
wmake -h
cd Languages
call make.cmd
cd ..
cd ..\..

echo * MODULES\SLIDESHOW
cd Modules\SlideShow
wmake -h
cd Languages
call make.cmd
cd ..
cd ..\..

echo * MODULES\TEXT
cd Modules\Text
wmake -h
cd Languages
call make.cmd
cd ..
cd ..\..

echo * SSDPMS
cd ssdpms
wmake -h
cd ..

echo * All made!
