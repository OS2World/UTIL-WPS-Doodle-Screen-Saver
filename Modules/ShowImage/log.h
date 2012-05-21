#ifndef __dssaverlog__
// #define __dssaverlog__ 1

#include <os2.h>

// DEBUG_BUILD gets defined in in the makefile
#ifdef DEBUG_BUILD

#define DEBUG_LOG_FILE "d:\\ShowImg.log"
#define DEBUG_LOGGING

void AddLog(CHAR *szFmt, ...);
void AddLogInt(int number);
void AddLogULong(ULONG number);

#endif
#endif
