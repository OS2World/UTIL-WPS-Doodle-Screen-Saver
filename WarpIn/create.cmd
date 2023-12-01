/*
  This is a script to start the WarpIn Archive Creator to create the WarpIn file
  for Doodle's ScreenSaver

  Make sure it contains the right path for your WarpIn installation!
*/

/* Create Self-Installing EXE file */
'set BEGINLIBPATH=W:\sys\install\WARPIN;%BEGINLIBPATH%'
'W:\sys\install\WARPIN\wic.exe @dssaver_wic_params.txt'
