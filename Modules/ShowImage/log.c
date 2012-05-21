
#include "log.h"

#ifdef DEBUG_BUILD


#include <stdio.h>
#include <stdarg.h>
/*void AddLog(char *pchMsg) {
  FILE *hFile;

  hFile = fopen(DEBUG_LOG_FILE, "at+");
  if (hFile) {
    fprintf(hFile, "%s", pchMsg);
    fclose(hFile);
  }
}*/

void AddLog(CHAR *szFmt, ...) {
  va_list vars;
  CHAR sz [256+1];
  FILE *hFile;

  va_start(vars, szFmt);
  vsprintf(sz, szFmt, vars);
  va_end(vars);
  hFile = fopen(DEBUG_LOG_FILE, "at+");
  if (hFile) {
    fprintf(hFile, "%s", sz);
    fclose(hFile);
  }
}

void AddLogInt(int number) {
  FILE *hFile;

  hFile = fopen(DEBUG_LOG_FILE, "at+");
  if (hFile) {
    fprintf(hFile, "%i", number);
    fclose(hFile);
  }
}

void AddLogULong(ULONG number) {
  FILE *hFile;

  hFile = fopen(DEBUG_LOG_FILE, "at+");
  if (hFile) {
    fprintf(hFile, "%u", number);
    fclose(hFile);
  }
}


#endif
