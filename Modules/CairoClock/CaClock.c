/*
 * Screen Saver - Lockup Desktop replacement for OS/2 and eComStation systems
 * Copyright (C) 2004-2008 Doodle
 *
 * Contact: doodle@dont.spam.me.scenergy.dfmk.hu
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <process.h>
#include <direct.h>
#include <locale.h>
#include <math.h>
#include <float.h>
#include <time.h>
#define INCL_DOS
#define INCL_WIN
#define INCL_ERRORS
#define INCL_GPIPRIMITIVES
#include <os2.h>
#include "SSModule.h"
#include "MSGX.h" // NLS support

#define _ULS_CALLCONV_
#define CALLCONV _System
#include <unidef.h>                    // Unicode API
#include <uconv.h>                     // Unicode API (codepage conversion)

#include "CaClock-Resources.h"

#define cairo_public __declspec(__cdecl)
#include <cairo\cairo.h>
#include <cairo\cairo-os2.h>

// Undocumented flag to make a window topmost:
#define WS_TOPMOST  0x00200000L

// Thread states
#define STATE_UNKNOWN        0
#define STATE_RUNNING        1
#define STATE_STOPPED_OK     2
#define STATE_STOPPED_ERROR  3

// Timer ID for Password asking dialog, to hide the dialog
// if the user doesn't press a key for a while!
#define AUTOHIDER_TIMER_ID  10

// Private class name for our saver window
#define SAVERWINDOW_CLASS "CACLOCK_SCREENSAVER_CLASS"

// Private window messages
#define WM_SUBCLASS_INIT       (WM_USER+1)
#define WM_SUBCLASS_UNINIT     (WM_USER+2)
#define WM_ASKPASSWORD         (WM_USER+3)
#define WM_SHOWWRONGPASSWORD   (WM_USER+4)
#define WM_PAUSESAVING         (WM_USER+5)
#define WM_RESUMESAVING        (WM_USER+6)

HMODULE hmodOurDLLHandle;
int     bRunning;
int     bPaused;
int     bFirstKeyGoesToPwdWindow = 0;
TID     tidSaverThread;
int     iSaverThreadState;
HWND    hwndSaverWindow; // If any...
HMQ     hmqSaverThreadMsgQueue;
int     bOnlyPreviewMode;

ULONG   ulAutoHiderTimerID;

char   *apchNLSText[SSMODULE_NLSTEXT_MAX+1];
HMTX    hmtxUseNLSTextArray;

typedef struct ModuleConfig_s
{
  int bShowDate;
  int bShowDigitalClock;
  int bLetTheClockMoveAroundTheScreen;
  int iSizePercent;
} ModuleConfig_t, *ModuleConfig_p;

typedef struct ConfigDlgInitParams_s
{
  USHORT          cbSize; // Required to be able to pass this structure to WM_INITDLG
  char           *pchHomeDirectory;
  ModuleConfig_t  ModuleConfig;
} ConfigDlgInitParams_t, *ConfigDlgInitParams_p;

ModuleConfig_t  ModuleConfig;
char     achUndoBuf[512];

cairo_surface_t *pCairoSurface = NULL;
cairo_t         *pCairoHandle = NULL;
TID              tidCairoDrawThread;
int              bShutdownDrawing;

#ifndef M_PI
#define M_PI 3.14159
#endif


#ifdef DEBUG_BUILD
#define DEBUG_LOGGING
#endif
#ifdef DEBUG_LOGGING
void AddLog(char *pchMsg)
{
  FILE *hFile;

  hFile = fopen("c:\\ssgener.log", "at+");
  if (hFile)
  {
    fprintf(hFile, "%s", pchMsg);
    fclose(hFile);
  }
}
#endif

/*************************************************************************************
 * BEGIN:
 * New as of v1.51:
 * Keypresses activating the password protection window will not be lost from now on
 */
#define SAVERMODULE_RECORDSIZE 64
MPARAM ampMP1[SAVERMODULE_RECORDSIZE];
MPARAM ampMP2[SAVERMODULE_RECORDSIZE];
int iNumEventsInRecord;

static void internal_InitWMCHARRecord()
{
  iNumEventsInRecord = 0;
}

static void internal_SaveWMCHAREventToRecord(MPARAM mp1, MPARAM mp2)
{
  if (iNumEventsInRecord<SAVERMODULE_RECORDSIZE)
  {
    ampMP1[iNumEventsInRecord] = mp1;
    ampMP2[iNumEventsInRecord] = mp2;
    iNumEventsInRecord++;
  } else
  {
    int i;
    for (i=0; i<SAVERMODULE_RECORDSIZE-2; i++)
    {
      ampMP1[i] = ampMP1[i+1];
      ampMP2[i] = ampMP2[i+1];
    }
    ampMP1[SAVERMODULE_RECORDSIZE-1] = mp1;
    ampMP2[SAVERMODULE_RECORDSIZE-1] = mp2;
  }
}

static void internal_ReplayWMCHARRecord()
{
  int i;
  HWND hwndFocus;

  hwndFocus = WinQueryFocus(HWND_DESKTOP);
  for (i=0; i<iNumEventsInRecord; i++)
    WinPostMsg(hwndFocus, WM_CHAR, ampMP1[i], ampMP2[i]);

  iNumEventsInRecord = 0;
}

/* END:
 * New as of v1.51:
 * Keypresses activating the password protection window will not be lost from now on
 *************************************************************************************/

static FILE *internal_OpenNLSFile(char *pchHomeDirectory)
{
  char achRealLocaleName[128];
  char *pchFileName, *pchLang;
  ULONG rc;
  int i;
  FILE *hfNLSFile;

  hfNLSFile = NULLHANDLE;

  pchFileName = (char *) malloc(1024);
  if (pchFileName)
  {
    // Let's check the currently set Languagecode string!
    achRealLocaleName[0] = 0;
    if (DosRequestMutexSem(hmtxUseNLSTextArray, SEM_INDEFINITE_WAIT)==NO_ERROR)
    {
      if (apchNLSText[SSMODULE_NLSTEXT_LANGUAGECODE])
        strncpy(achRealLocaleName, apchNLSText[SSMODULE_NLSTEXT_LANGUAGECODE], sizeof(achRealLocaleName));
      DosReleaseMutexSem(hmtxUseNLSTextArray);
    }

    if (achRealLocaleName[0]==0)
    {
      // The ssaver core did not tell us anything about its language, so
      // try to get language from LANG env variable!
#ifdef DEBUG_LOGGING
      AddLog("[internal_OpenNLSFile] : No language told by SSaver core, using LANG env. variable!\n");
#endif
      rc = DosScanEnv("LANG", &pchLang);
      if ((rc!=NO_ERROR) || (!pchLang))
      {
#ifdef DEBUG_LOGGING
        AddLog("[internal_OpenNLSFile] : Could not query LANG env. var., will use 'en'\n");
#endif
        pchLang = "en"; // Default
      }
      strncpy(achRealLocaleName, pchLang, sizeof(achRealLocaleName));
    }

    // Get language code
    if (achRealLocaleName[0]!=0)
    {
      // Aaah, there is a language set!
      // Try that one!

      pchLang = (char *) malloc(1024);
      if (!pchLang)
      {
	// Not enough memory, so we won't do the long
	// method of searching for every combination of
        // the language string, but only for the string itself!

	// Assemble NLS file name
	sprintf(pchFileName, "%sModules\\CaClock\\%s.msg", pchHomeDirectory, achRealLocaleName);
#ifdef DEBUG_LOGGING
	AddLog("[internal_OpenNLSFile] : Trying to open NLS file: [");
	AddLog(pchFileName);
	AddLog("] by achLocalLanguage setting (in Not enough memory branch)!\n");
#endif

	hfNLSFile = fopenMessageFile(pchFileName);
      } else
      {
	// Fine, we can start trying a lot of filenames!
	sprintf(pchLang, "%s", achRealLocaleName);

	do {
	  // Assemble NLS file name
	  sprintf(pchFileName, "%sModules\\CaClock\\%s.msg", pchHomeDirectory, pchLang);
#ifdef DEBUG_LOGGING
	  AddLog("[internal_OpenNLSFile] : Trying to open NLS file: [");
	  AddLog(pchFileName);
	  AddLog("] by achLocalLanguage setting! (in Loop)\n");
#endif

	  hfNLSFile = fopenMessageFile(pchFileName);

	  // Make pchLang shorter, until the next underscore!
	  i = strlen(pchLang)-1;
	  while ((i>=0) && (pchLang[i]!='_'))
	    i--;
	  if (i<0) i = 0;
          pchLang[i] = 0;

	} while ((pchLang[0]) && (!hfNLSFile));

        free(pchLang); pchLang = NULL;
      }
    }

    free(pchFileName);
  }

#ifdef DEBUG_LOGGING
  if (hfNLSFile)
    AddLog("[internal_OpenNLSFile] : NLS file opened.\n");
  else
    AddLog("[internal_OpenNLSFile] : NLS file could not be opened.\n");

  AddLog("[internal_OpenNLSFile] : Done.\n");
#endif

  return hfNLSFile;
}

static void internal_CloseNLSFile(FILE *hfNLSFile)
{
  if (hfNLSFile)
    fclose(hfNLSFile);
}

static void internal_LoadConfig(ModuleConfig_p pConfig, char *pchHomeDirectory)
{
  FILE *hFile;
  ModuleConfig_t TempConfig;
  char *pchHomeEnvVar;
  char achFileName[1024];
  int iScanned;

  // Set defaults first!
  pConfig->bShowDate = TRUE;
  pConfig->bShowDigitalClock = TRUE;
  pConfig->bLetTheClockMoveAroundTheScreen = TRUE;
  pConfig->iSizePercent = 90;

  hFile = NULL;
  // Get home directory of current user
  pchHomeEnvVar = getenv("HOME");

  // If we have the HOME variable set, then we'll try to use
  // the .dssaver folder in there, otherwise fall back to
  // using the DSSaver Global home directory!
  if (pchHomeEnvVar)
  {
    int iLen;
    
    snprintf(achFileName, sizeof(achFileName)-1, "%s", pchHomeEnvVar);
    // Make sure it will have the trailing backslash!
    iLen = strlen(achFileName);
    if ((iLen>0) &&
        (achFileName[iLen-1]!='\\'))
    {
      achFileName[iLen] = '\\';
      achFileName[iLen+1] = 0;
    }
    // Now put there the .dssaver folder name and the config file name!
    strncat(achFileName, ".dssaver\\CaClock.CFG", sizeof(achFileName));
    // Try to open that config file!
    hFile = fopen(achFileName, "rt");
  }

  if (!hFile)
  {
    // If we could not open a config file in the HOME directory, or there was
    // no HOME directory, then try to use the DSSaver Global home directory!
    snprintf(achFileName, sizeof(achFileName), "%sCaClock.CFG", pchHomeDirectory);
    hFile = fopen(achFileName, "rt");
    if (!hFile)
      return; // Could not open file!
  }

  iScanned = fscanf(hFile, "%d %d %d %d",
                    &(TempConfig.bShowDate),
                    &(TempConfig.bShowDigitalClock),
                    &(TempConfig.bLetTheClockMoveAroundTheScreen),
                    &(TempConfig.iSizePercent));
  fclose(hFile);

  if (iScanned == 4)
  {
    // Fine, we could read all we wanted, so use the values we read!
    memcpy(pConfig, &TempConfig, sizeof(TempConfig));
  }
#ifdef DEBUG_LOGGING
  else
    AddLog("[internal_LoadConfig] : Could not parse all the 4 settings, using defaults!\n");
#endif

}

static void internal_SaveConfig(ModuleConfig_p pConfig, char *pchHomeDirectory)
{
  FILE *hFile;
  char *pchHomeEnvVar;
  char achFileName[1024];

  hFile = NULL;
  // Get home directory of current user
  pchHomeEnvVar = getenv("HOME");

  // If we have the HOME variable set, then we'll try to use
  // the .dssaver folder in there, otherwise fall back to
  // using the DSSaver Global home directory!
  if (pchHomeEnvVar)
  {
    int iLen;
    
    snprintf(achFileName, sizeof(achFileName)-1, "%s", pchHomeEnvVar);
    // Make sure it will have the trailing backslash!
    iLen = strlen(achFileName);
    if ((iLen>0) &&
        (achFileName[iLen-1]!='\\'))
    {
      achFileName[iLen] = '\\';
      achFileName[iLen+1] = 0;
    }

    // Create the .dssaver folder in there, if it doesn't exist yet!
    strncat(achFileName, ".dssaver", sizeof(achFileName));
    mkdir(achFileName);
    // Now put there the config file name, too!
    strncat(achFileName, "\\CaClock.CFG", sizeof(achFileName));
    // Try to open that config file!
    hFile = fopen(achFileName, "wt");
  }

  if (!hFile)
  {
    // If we could not open a config file in the HOME directory, or there was
    // no HOME directory, then try to use the DSSaver Global home directory!
    snprintf(achFileName, sizeof(achFileName), "%sCaClock.CFG", pchHomeDirectory);
    hFile = fopen(achFileName, "wt");
    if (!hFile)
      return; // Could not open file!
  }

  fprintf(hFile, "%d %d %d %d",
          (pConfig->bShowDate),
          (pConfig->bShowDigitalClock),
          (pConfig->bLetTheClockMoveAroundTheScreen),
          (pConfig->iSizePercent));
  fclose(hFile);
}

// Temp checkbox to get size
#define ID_TEMPCHECKBOX     5010

static void internal_GetCheckboxExtraSize(HWND hwnd, ULONG ulID, int *piCX, int *piCY)
{
  HWND hwndTemp;
  SWP swpTemp;

  // Create temporary checkbox with no text inside
  hwndTemp = WinCreateWindow(hwnd, WC_BUTTON,
                             "",
                             BS_CHECKBOX | BS_AUTOSIZE,
                             10, 10,
                             -1, -1,
                             hwnd,
                             HWND_TOP,
                             ID_TEMPCHECKBOX,
                             NULL, NULL);

  // Query textbox size
  WinQueryWindowPos(hwndTemp, &swpTemp);
  // Destroy temporary checkbox
  WinDestroyWindow(hwndTemp);

  *piCX = swpTemp.cx+1;
  *piCY = swpTemp.cy+1;
}

static void internal_GetStaticTextSize(HWND hwnd, ULONG ulID, int *piCX, int *piCY)
{
  HPS hps;
  char achTemp[512];
  POINTL aptl[TXTBOX_COUNT];

  // This calculates how much space is needed for a given static text control
  // to hold its text.

  WinQueryDlgItemText(hwnd, ulID, sizeof(achTemp), achTemp);

  hps = WinGetPS(WinWindowFromID(hwnd, ulID));
  GpiQueryTextBox(hps, strlen(achTemp),
                  achTemp,
		  TXTBOX_COUNT,
		  aptl);

  *piCX = aptl[TXTBOX_TOPRIGHT].x - aptl[TXTBOX_BOTTOMLEFT].x;
  *piCY = aptl[TXTBOX_TOPRIGHT].y - aptl[TXTBOX_BOTTOMLEFT].y;

  WinReleasePS(hps);
}

static void internal_SetPwdProtWindowText(HWND hwnd)
{
  char *pchTemp;

  // Set new window texts!
  if (DosRequestMutexSem(hmtxUseNLSTextArray, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    pchTemp = apchNLSText[SSMODULE_NLSTEXT_PWDPROT_ASK_TITLE];
    if (pchTemp)
      WinSetWindowText(hwnd, pchTemp);

    pchTemp = apchNLSText[SSMODULE_NLSTEXT_PWDPROT_ASK_TEXT];
    if (pchTemp)
      WinSetDlgItemText(hwnd, ST_PLEASEENTERTHEPASSWORD, pchTemp);
  
    pchTemp = apchNLSText[SSMODULE_NLSTEXT_PWDPROT_ASK_OKBUTTON];
    if (pchTemp)
      WinSetDlgItemText(hwnd, PB_OK, pchTemp);
  
    pchTemp = apchNLSText[SSMODULE_NLSTEXT_PWDPROT_ASK_CANCELBUTTON];
    if (pchTemp)
      WinSetDlgItemText(hwnd, PB_CANCEL, pchTemp);

    DosReleaseMutexSem(hmtxUseNLSTextArray);
  }
}

static void internal_ArrangePwdProtWindowControls(HWND hwnd)
{
  SWP swpWindow;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  ULONG CYTITLEBAR = WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);
  int iCX, iCY;     // Control size
  int iMaxBtnCX, iBtnCY;

  // Arrange password protection window controls, and
  // set window size so it will look nice!


  // First get the sizes of texts inside buttons
  iMaxBtnCX = 0;

  internal_GetStaticTextSize(hwnd, PB_OK, &iCX, &iCY);
  if (iMaxBtnCX<iCX) iMaxBtnCX = iCX;

  internal_GetStaticTextSize(hwnd, PB_CANCEL, &iCX, &iCY);
  if (iMaxBtnCX<iCX) iMaxBtnCX = iCX;

  iBtnCY = iCY;

  // Now get size of pwd asking text!
  internal_GetStaticTextSize(hwnd, ST_PLEASEENTERTHEPASSWORD, &iCX, &iCY);

  // Oookay, now we know how big window we want!

  WinSetWindowPos(hwnd,
                  HWND_TOP,
                  0, 0,
                  6*CXDLGFRAME + iCX,
                  CYDLGFRAME + CYTITLEBAR + 2*CYDLGFRAME + iCY + 2*CYDLGFRAME + (iBtnCY+CYDLGFRAME) + 2*CYDLGFRAME + (iBtnCY+4*CYDLGFRAME) + 2*CYDLGFRAME,
                  SWP_SIZE);

  WinQueryWindowPos(hwnd, &swpWindow);

  // Set the two buttons in there
  WinSetWindowPos(WinWindowFromID(hwnd, PB_OK),
                  HWND_TOP,
                  2*CXDLGFRAME,
                  2*CYDLGFRAME,
                  6*CXDLGFRAME + iMaxBtnCX,
                  3*CYDLGFRAME + iBtnCY,
                  SWP_MOVE | SWP_SIZE);

  WinSetWindowPos(WinWindowFromID(hwnd, PB_CANCEL),
                  HWND_TOP,
                  swpWindow.cx - 2*CXDLGFRAME - (6*CXDLGFRAME + iMaxBtnCX),
                  2*CYDLGFRAME,
                  6*CXDLGFRAME + iMaxBtnCX,
                  3*CYDLGFRAME + iBtnCY,
                  SWP_MOVE | SWP_SIZE);

  // Set the position of password entry field
  WinSetWindowPos(WinWindowFromID(hwnd, EF_PASSWORD),
                  HWND_TOP,
                  3*CXDLGFRAME,
                  2*CYDLGFRAME + (3*CYDLGFRAME + iBtnCY) + 2*CYDLGFRAME,
                  swpWindow.cx - 6*CXDLGFRAME,
                  iBtnCY,
                  SWP_MOVE | SWP_SIZE);

  // Set the position of password asking text
  WinSetWindowPos(WinWindowFromID(hwnd, ST_PLEASEENTERTHEPASSWORD),
                  HWND_TOP,
                  3*CXDLGFRAME,
                  swpWindow.cy - CYDLGFRAME - CYTITLEBAR - 2*CYDLGFRAME - iCY,
                  iCX,
                  iCY,
                  SWP_MOVE | SWP_SIZE);
}

static void internal_SetWrongPwdWindowText(HWND hwnd)
{
  char *pchTemp;

  // Set new window texts!
  if (DosRequestMutexSem(hmtxUseNLSTextArray, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    pchTemp = apchNLSText[SSMODULE_NLSTEXT_PWDPROT_WRONG_TITLE];
    if (pchTemp)
      WinSetWindowText(hwnd, pchTemp);
  
    pchTemp = apchNLSText[SSMODULE_NLSTEXT_PWDPROT_WRONG_TEXT];
    if (pchTemp)
      WinSetDlgItemText(hwnd, ST_INVALIDPASSWORDTEXT, pchTemp);
  
    pchTemp = apchNLSText[SSMODULE_NLSTEXT_PWDPROT_WRONG_OKBUTTON];
    if (pchTemp)
      WinSetDlgItemText(hwnd, DID_OK, pchTemp);

    DosReleaseMutexSem(hmtxUseNLSTextArray);
  }
}

static void internal_ArrangeWrongPwdWindowControls(HWND hwnd)
{
  SWP swpWindow;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  ULONG CYTITLEBAR = WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);
  int iCX, iCY;     // Control size
  int iMaxBtnCX, iBtnCY;

  // Arrange password protection window controls, and
  // set window size so it will look nice!


  // First get the sizes of texts inside buttons
  internal_GetStaticTextSize(hwnd, DID_OK, &iMaxBtnCX, &iBtnCY);

  // Now get size of text!
  internal_GetStaticTextSize(hwnd, ST_INVALIDPASSWORDTEXT, &iCX, &iCY);

  // Oookay, now we know how big window we want!

  WinSetWindowPos(hwnd,
                  HWND_TOP,
                  0, 0,
                  6*CXDLGFRAME + iCX,
                  CYDLGFRAME + CYTITLEBAR + 2*CYDLGFRAME + iCY + 2*CYDLGFRAME + (iBtnCY+4*CYDLGFRAME) + 2*CYDLGFRAME,
                  SWP_SIZE);

  WinQueryWindowPos(hwnd, &swpWindow);

  // Set the button in there
  WinSetWindowPos(WinWindowFromID(hwnd, DID_OK),
                  HWND_TOP,
                  (swpWindow.cx - (6*CXDLGFRAME + iMaxBtnCX)) / 2,
                  2*CYDLGFRAME,
                  6*CXDLGFRAME + iMaxBtnCX,
                  3*CYDLGFRAME + iBtnCY,
                  SWP_MOVE | SWP_SIZE);

  // Set the position of wrong password text
  WinSetWindowPos(WinWindowFromID(hwnd, ST_INVALIDPASSWORDTEXT),
                  HWND_TOP,
                  3*CXDLGFRAME,
                  swpWindow.cy - CYDLGFRAME - CYTITLEBAR - 2*CYDLGFRAME - iCY,
                  iCX,
                  iCY,
                  SWP_MOVE | SWP_SIZE);

}

static void internal_SetPageFont(HWND hwnd)
{
  char *pchTemp;

  // Set new window fonts!
  if (DosRequestMutexSem(hmtxUseNLSTextArray, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    pchTemp = apchNLSText[SSMODULE_NLSTEXT_FONTTOUSE];
    if (pchTemp)
    {
      HWND hwndChild;
      HENUM henum;

      // Oookay, we have the font, set the page and all controls inside it!
      WinSetPresParam(hwnd, PP_FONTNAMESIZE,
		      strlen(pchTemp)+1,
		      pchTemp);

      // Now go through all of its children, and set it there, too!
      henum = WinBeginEnumWindows(hwnd);

      while ((hwndChild = WinGetNextWindow(henum))!=NULLHANDLE)
      {
	WinSetPresParam(hwndChild, PP_FONTNAMESIZE,
			strlen(pchTemp)+1,
			pchTemp);

      }
      WinEndEnumWindows(henum);
    }

    DosReleaseMutexSem(hmtxUseNLSTextArray);
  }
}

static void internal_SetConfigDialogText(HWND hwnd, char *pchHomeDirectory)
{
  FILE *hfNLSFile;
  char achTemp[128];

  if (!pchHomeDirectory) return;
  hfNLSFile = internal_OpenNLSFile(pchHomeDirectory);
  if (hfNLSFile)
  {
    // Window title
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0001"))
      WinSetWindowText(hwnd, achTemp);

    // Groupbox-1
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0002"))
      WinSetDlgItemText(hwnd, GB_VISUALOPTIONS, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0003"))
      WinSetDlgItemText(hwnd, CB_SHOWDATE, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0004"))
      WinSetDlgItemText(hwnd, CB_SHOWDIGITALCLOCK, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0005"))
      WinSetDlgItemText(hwnd, CB_LETTHECLOCKMOVEAROUNDTHESCREEN, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0006"))
      WinSetDlgItemText(hwnd, ST_CLOCKSIZEIS, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0007"))
      WinSetDlgItemText(hwnd, ST_PERCENTOFSCREEN, achTemp);

    // Buttons
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0008"))
      WinSetDlgItemText(hwnd, PB_CONFOK, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0009"))
      WinSetDlgItemText(hwnd, PB_CONFCANCEL, achTemp);

    internal_CloseNLSFile(hfNLSFile);
  }
}

static void internal_AdjustConfigDialogControls(HWND hwnd)
{
  SWP swpTemp, swpTemp2, swpTemp3, swpWindow;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  int staticheight, buttonheight, gbheight;
  int iCBCX, iCBCY; // Checkbox extra sizes
  int iCX, iCY, iBottomButtonsWidth;
  int iX;
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(hwnd))
  {
    WinEnableWindowUpdate(hwnd, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Get window size!
  WinQueryWindowPos(hwnd, &swpWindow);

  // Query static height in pixels. 
  internal_GetStaticTextSize(hwnd, ST_CLOCKSIZEIS, &iCX, &iCY);
  staticheight = iCY;

  // Query button height in pixels.
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CONFOK), &swpTemp);
  buttonheight = swpTemp.cy;

  // Get extra sizes
  internal_GetCheckboxExtraSize(hwnd, CB_LETTHECLOCKMOVEAROUNDTHESCREEN, &iCBCX, &iCBCY);

  // Calculate window width
  internal_GetStaticTextSize(hwnd, CB_LETTHECLOCKMOVEAROUNDTHESCREEN, &iCX, &iCY);
  swpWindow.cx = iCBCX + iCX + 10*CXDLGFRAME;

  iX = 10*CXDLGFRAME;
  internal_GetStaticTextSize(hwnd, ST_CLOCKSIZEIS, &iCX, &iCY);
  iX += iCX + 11*CXDLGFRAME;
  internal_GetStaticTextSize(hwnd, ST_PERCENTOFSCREEN, &iCX, &iCY);
  iX += iCX;
  if (swpWindow.cx<iX)
    swpWindow.cx = iX;

  // Calculate width of Ok and Cancel buttons!
  internal_GetStaticTextSize(hwnd, PB_CONFCANCEL, &iCX, &iCY);
  iBottomButtonsWidth = iCX;
  internal_GetStaticTextSize(hwnd, PB_CONFOK, &iCX, &iCY);
  if (iBottomButtonsWidth<iCX) iBottomButtonsWidth = iCX;

  // Set the position of buttons
  WinSetWindowPos(WinWindowFromID(hwnd, PB_CONFOK), HWND_TOP,
                  CXDLGFRAME*2,
                  CYDLGFRAME*2,
                  iBottomButtonsWidth + 6*CXDLGFRAME,
                  iCY + 3*CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);

  WinSetWindowPos(WinWindowFromID(hwnd, PB_CONFCANCEL), HWND_TOP,
                  swpWindow.cx - CXDLGFRAME*2 - (iBottomButtonsWidth + 6*CXDLGFRAME),
                  CYDLGFRAME*2,
                  iBottomButtonsWidth + 6*CXDLGFRAME,
                  iCY + 3*CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);


  // First set the "Visual options" groupbox
  gbheight =
    CYDLGFRAME + // frame
    CYDLGFRAME + // Margin
    staticheight + CYDLGFRAME + // Entry field
    CYDLGFRAME + // Margin
    iCBCY + // Checkbox
    CYDLGFRAME + // Margin
    iCBCY + // Checkbox
    CYDLGFRAME + // Margin
    iCBCY + // Checkbox
    CYDLGFRAME + // Margin
    staticheight; // Text of groupbox

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CONFOK), &swpTemp);
  WinSetWindowPos(WinWindowFromID(hwnd, GB_VISUALOPTIONS),
                  HWND_TOP,
                  2*CXDLGFRAME,
                  swpTemp.y + swpTemp.cy + CYDLGFRAME,
                  swpWindow.cx - 4*CXDLGFRAME,
                  gbheight,
                  SWP_MOVE | SWP_SIZE);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_VISUALOPTIONS), &swpTemp);

  internal_GetStaticTextSize(hwnd, ST_CLOCKSIZEIS, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_CLOCKSIZEIS), HWND_TOP,
                  swpTemp.x + 2*CXDLGFRAME,
                  swpTemp.y + 2*CYDLGFRAME,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_CLOCKSIZEIS), &swpTemp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, EF_SIZEPERCENT), &swpTemp3);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_SIZEPERCENT), HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME,
                  swpTemp2.y,
                  CXDLGFRAME*8,
                  staticheight,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, EF_SIZEPERCENT), &swpTemp3);
  internal_GetStaticTextSize(hwnd, ST_PERCENTOFSCREEN, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_PERCENTOFSCREEN), HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME + swpTemp3.cx + CXDLGFRAME,
                  swpTemp2.y,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);


  WinQueryWindowPos(WinWindowFromID(hwnd, ST_CLOCKSIZEIS), &swpTemp2);
  internal_GetStaticTextSize(hwnd, CB_LETTHECLOCKMOVEAROUNDTHESCREEN, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, CB_LETTHECLOCKMOVEAROUNDTHESCREEN), HWND_TOP,
                  swpTemp2.x,
                  swpTemp2.y + swpTemp2.cy + 3*CYDLGFRAME,
                  iCX + iCBCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, CB_LETTHECLOCKMOVEAROUNDTHESCREEN), &swpTemp2);
  internal_GetStaticTextSize(hwnd, CB_SHOWDIGITALCLOCK, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, CB_SHOWDIGITALCLOCK), HWND_TOP,
                  swpTemp2.x,
                  swpTemp2.y + swpTemp2.cy,
                  iCX + iCBCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, CB_SHOWDIGITALCLOCK), &swpTemp2);
  internal_GetStaticTextSize(hwnd, CB_SHOWDATE, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, CB_SHOWDATE), HWND_TOP,
                  swpTemp2.x,
                  swpTemp2.y + swpTemp2.cy,
                  iCX + iCBCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);

  // Set window size so the contents will fit perfectly!
  swpWindow.cy = swpTemp.y + swpTemp.cy + CYDLGFRAME + CYDLGFRAME + WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);

  WinSetWindowPos(hwnd, HWND_TOP,
                  0, 0,
                  swpWindow.cx, swpWindow.cy,
                  SWP_SIZE);
  // That's all!
  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);

}

static void internal_SetConfigDialogInitialControlValues(HWND hwnd, ConfigDlgInitParams_p pCfgDlgInit)
{
  char achTemp[128];

  if (!pCfgDlgInit) return;

  WinSendDlgItemMsg(hwnd, CB_SHOWDATE,
                    BM_SETCHECK,
                    (MRESULT) (pCfgDlgInit->ModuleConfig.bShowDate),
                    (MRESULT) 0);
  WinSendDlgItemMsg(hwnd, CB_SHOWDIGITALCLOCK,
                    BM_SETCHECK,
                    (MRESULT) (pCfgDlgInit->ModuleConfig.bShowDigitalClock),
                    (MRESULT) 0);
  WinSendDlgItemMsg(hwnd, CB_LETTHECLOCKMOVEAROUNDTHESCREEN,
                    BM_SETCHECK,
                    (MRESULT) (pCfgDlgInit->ModuleConfig.bLetTheClockMoveAroundTheScreen),
                    (MRESULT) 0);

  sprintf(achTemp, "%d", pCfgDlgInit->ModuleConfig.iSizePercent);
  WinSetDlgItemText(hwnd, EF_SIZEPERCENT, achTemp);
}

MRESULT EXPENTRY fnConfigDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  ConfigDlgInitParams_p pCfgDlgInit;
  HWND hwndOwner;
  SWP swpDlg, swpOwner;
  POINTL ptlTemp;

  switch (msg)
  {
    case WM_INITDLG:
      // Store window instance data pointer in window words
      WinSetWindowULong(hwnd, QWL_USER, (ULONG) mp2);
      pCfgDlgInit = (ConfigDlgInitParams_p) mp2;

      // Set dialog control texts and fonts based on
      // current language
      internal_SetPageFont(hwnd);
      internal_SetConfigDialogText(hwnd, pCfgDlgInit->pchHomeDirectory);

      // Arrange controls, and resize dialog window
      internal_AdjustConfigDialogControls(hwnd);

      // Set control values and limits
      internal_SetConfigDialogInitialControlValues(hwnd, pCfgDlgInit);

      // Set initial position of dialog window and show it
      WinQueryWindowPos(hwnd, &swpDlg);
      hwndOwner = WinQueryWindow(hwnd, QW_OWNER);
      WinQueryWindowPos(hwndOwner, &swpOwner);
      ptlTemp.x = swpOwner.x; ptlTemp.y = swpOwner.y;
      WinMapWindowPoints(WinQueryWindow(hwndOwner, QW_PARENT),
                         HWND_DESKTOP,
                         &ptlTemp,
                         1);
      WinSetWindowPos(hwnd, HWND_TOP,
                      ptlTemp.x + (swpOwner.cx - swpDlg.cx)/2,
                      ptlTemp.y + (swpOwner.cy - swpDlg.cy)/2,
                      0, 0,
                      SWP_MOVE | SWP_SHOW);

      return (MRESULT) FALSE;

    case WM_CONTROL:
      pCfgDlgInit = (ConfigDlgInitParams_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pCfgDlgInit)
      {
        // Fine, we have out private data! Let's see the message then!
        switch (SHORT1FROMMP(mp1))
        {
          case CB_SHOWDATE:
            if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
            {
              // Selected this checkbox
              ModuleConfig.bShowDate =
                pCfgDlgInit->ModuleConfig.bShowDate = WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));
              return (MRESULT) 0;
            }
            break;

          case CB_SHOWDIGITALCLOCK:
            if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
            {
              // Selected this checkbox
              ModuleConfig.bShowDigitalClock =
                pCfgDlgInit->ModuleConfig.bShowDigitalClock = WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));
              return (MRESULT) 0;
            }
            break;

          case CB_LETTHECLOCKMOVEAROUNDTHESCREEN:
            if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
            {
              // Selected this radio button!
              ModuleConfig.bLetTheClockMoveAroundTheScreen =
                pCfgDlgInit->ModuleConfig.bLetTheClockMoveAroundTheScreen = WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));
              return (MRESULT) 0;
            }
            break;

          case EF_SIZEPERCENT:
            if (SHORT2FROMMP(mp1)==EN_SETFOCUS)
            {
              // The entry field is receiving the focus
              WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1), sizeof(achUndoBuf), achUndoBuf);
              return (MRESULT) 0;
            }
            if (SHORT2FROMMP(mp1)==EN_KILLFOCUS)
            {
              SHORT sTemp;
              // The entry field is loosing the focus
              if (!WinQueryDlgItemShort(hwnd, SHORT1FROMMP(mp1), &sTemp, FALSE))
                sTemp = -1;
              if ((sTemp<1) || (sTemp>100))
              {
                // Invalid value, restore old!
                WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achUndoBuf);
              } else
              {
                // Valid value!
                ModuleConfig.iSizePercent =
                  pCfgDlgInit->ModuleConfig.iSizePercent = sTemp;
              }
              return (MRESULT) 0;
            }
            break;

          default:
            break;
        }
      }
      break;
    default:
      break;
  }
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}



MRESULT EXPENTRY fnAutoHiderDlgProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch (msg)
  {
    case WM_INITDLG:
      // Initializing the dialog window:
      // Start a timer, which will fire after 10 secs of inactivity, so we can
      // hide this dialog window, if the user doesn't press the keyboard
      // for 10 secs!
      ulAutoHiderTimerID = WinStartTimer(WinQueryAnchorBlock(hwnd),
                                         hwnd,
                                         AUTOHIDER_TIMER_ID,
                                         10000); // 10 secs
      break;
    case WM_MOUSEMOVE:
    case WM_CHAR:
      // A key has been pressed, or the mouse was moved, restart timer!
      WinStopTimer(WinQueryAnchorBlock(hwnd),
                   hwnd,
                   ulAutoHiderTimerID);
      ulAutoHiderTimerID = WinStartTimer(WinQueryAnchorBlock(hwnd),
                                         hwnd,
                                         AUTOHIDER_TIMER_ID,
                                         10000); // 10 secs
      break;
    case WM_TIMER:
      if ((SHORT)mp1==AUTOHIDER_TIMER_ID)
      {
        // Ooops, it's our timer, telling that the user has not pressed any keys
        // for a long time!
        // It's time to dismiss this dialog!
        WinDismissDlg(hwnd, PB_CANCEL);
      }
      break;
    case WM_DESTROY:
      WinStopTimer(WinQueryAnchorBlock(hwnd),
                   hwnd,
                   ulAutoHiderTimerID);
      break;
    default:
      break;
  }
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

MRESULT EXPENTRY fnSaverWindowProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  SWP swpDlg, swpParent;
  HWND hwndDlg;
  int rc;

  switch( msg )
  {
    case WM_SUBCLASS_INIT:
    case WM_CREATE:
      // Any initialization of the window and variables should come here.

      // Hide mouse pointer, if we're in real screen-saving mode!
      if (!bOnlyPreviewMode)
        WinShowPointer(HWND_DESKTOP, FALSE);
      // Initialize WMCHAR record
      internal_InitWMCHARRecord();
      break;

    case WM_CHAR:
      if (!bOnlyPreviewMode)
        internal_SaveWMCHAREventToRecord(mp1, mp2);
      break;

    case WM_ASKPASSWORD:
      {
        // Get parameters
        char *pchPwdBuff = (char *) mp1;
	int iPwdBuffSize = (int) mp2;

	// Show mouse pointer, if we're screensaving.
	if (!bOnlyPreviewMode)
	  WinShowPointer(HWND_DESKTOP, TRUE);

	hwndDlg = WinLoadDlg(hwnd, hwnd,
			     fnAutoHiderDlgProc,
			     hmodOurDLLHandle,
			     DLG_PASSWORDPROTECTION,
			     NULL);
	if (!hwndDlg)
	{
	  // Could not load dialog window resources!
	  if (!bOnlyPreviewMode)
	    WinShowPointer(HWND_DESKTOP, FALSE);

	  return (MRESULT) SSMODULE_ERROR_INTERNALERROR;
	}

        // Ok, dialog window loaded!

        // Now set its texts (NLS)
        internal_SetPageFont(hwndDlg);
        internal_SetPwdProtWindowText(hwndDlg);

        // Resize the window so text will fit!
        internal_ArrangePwdProtWindowControls(hwndDlg);

	// Initialize control(s)!
	WinSendDlgItemMsg(hwndDlg, EF_PASSWORD,
			  EM_SETTEXTLIMIT,
			  (MPARAM) (iPwdBuffSize-1),
			  (MPARAM) 0);
	WinSetDlgItemText(hwndDlg, EF_PASSWORD, "");

	// Center dialog in screen
	if (WinQueryWindowPos(hwndDlg, &swpDlg))
	  if (WinQueryWindowPos(hwnd, &swpParent))
	  {
	    // Center dialog box within the screen
	    int ix, iy;
	    ix = swpParent.x + (swpParent.cx - swpDlg.cx)/2;
	    iy = swpParent.y + (swpParent.cy - swpDlg.cy)/2;
	    WinSetWindowPos(hwndDlg, HWND_TOP, ix, iy, 0, 0,
			    SWP_MOVE);
	  }
	WinSetWindowPos(hwndDlg, HWND_TOP, 0, 0, 0, 0,
			SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);

        // Re-send WM_CHAR messages if needed
        if (bFirstKeyGoesToPwdWindow)
          internal_ReplayWMCHARRecord();

	// Process the dialog!
	rc = WinProcessDlg(hwndDlg);

	if (rc!=PB_OK)
	{
	  // The user pressed cancel!
          rc = SSMODULE_ERROR_USERPRESSEDCANCEL;
	} else
	{
	  // The user pressed OK!
	  // Get the entered password
	  WinQueryDlgItemText(hwndDlg, EF_PASSWORD,
			      iPwdBuffSize,
			      pchPwdBuff);
          rc = SSMODULE_NOERROR;
	}

	// Destroy window
	WinDestroyWindow(hwndDlg);

	// Hide mouse pointer again, if we're screensaving.
	if (!bOnlyPreviewMode)
	  WinShowPointer(HWND_DESKTOP, FALSE);

	return (MRESULT) rc;
      }

    case WM_SHOWWRONGPASSWORD:

      // Show mouse pointer, if we're screensaving.
      if (!bOnlyPreviewMode)
	WinShowPointer(HWND_DESKTOP, TRUE);

      hwndDlg = WinLoadDlg(hwnd, hwnd,
                           fnAutoHiderDlgProc,
                           hmodOurDLLHandle,
                           DLG_WRONGPASSWORD,
                           NULL);
      if (!hwndDlg)
      {
	// Could not load dialog window resources!

	if (!bOnlyPreviewMode)
          WinShowPointer(HWND_DESKTOP, FALSE);

        return (MRESULT) SSMODULE_ERROR_INTERNALERROR;
      }

      // Ok, dialog window loaded!

      // Now set its texts (NLS)
      internal_SetPageFont(hwndDlg);
      internal_SetWrongPwdWindowText(hwndDlg);

      // Resize the window so text will fit!
      internal_ArrangeWrongPwdWindowControls(hwndDlg);

      // Center dialog in screen
      if (WinQueryWindowPos(hwndDlg, &swpDlg))
        if (WinQueryWindowPos(hwnd, &swpParent))
        {
          // Center dialog box within the screen
          int ix, iy;
          ix = swpParent.x + (swpParent.cx - swpDlg.cx)/2;
          iy = swpParent.y + (swpParent.cy - swpDlg.cy)/2;
          WinSetWindowPos(hwndDlg, HWND_TOP, ix, iy, 0, 0,
                          SWP_MOVE);
        }
      WinSetWindowPos(hwndDlg, HWND_TOP, 0, 0, 0, 0,
                      SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);

      // Process the dialog!
      rc = WinProcessDlg(hwndDlg);

      // Destroy window
      WinDestroyWindow(hwndDlg);

      // Hide mouse pointer again, if we're screensaving.
      if (!bOnlyPreviewMode)
        WinShowPointer(HWND_DESKTOP, FALSE);

      return (MRESULT) SSMODULE_NOERROR;

    case WM_PAUSESAVING:
      // We should pause ourselves
      if (!bPaused)
      {
#ifdef DEBUG_LOGGING
	AddLog("[WM_PAUSESAVING] : Pausing saver module\n");
#endif
        bPaused = TRUE; // This will notify draw thread that it should stop calculating!
      }
      return (MRESULT) SSMODULE_NOERROR;

    case WM_RESUMESAVING:
      // We should resume screen saving
      if (bPaused)
      {
#ifdef DEBUG_LOGGING
	AddLog("[WM_RESUMESAVING] : Resuming saver module\n");
#endif
        bPaused = FALSE; // Also note draw thread that it can continue calculating
      }
      return (MRESULT) SSMODULE_NOERROR;

    case WM_SUBCLASS_UNINIT:
    case WM_DESTROY:
      // All kinds of cleanup (the opposite of everything done in WM_CREATE)
      // should come here.

      // Restore mouse pointer, if we're in real screen-saving mode!
      if (!bOnlyPreviewMode)
        WinShowPointer(HWND_DESKTOP, TRUE);
      break;

    case WM_ADJUSTWINDOWPOS:
      if (!bOnlyPreviewMode)
      {
	SWP *pSWP;

	// The following is required so that this window will be on
        // top of the xCenter window, evenif that is set to be always on top!

	// Real screensaving, here we should stay on top!
        // Set WS_TOPMOST flag again!
	WinSetWindowBits(hwnd, QWL_STYLE, WS_TOPMOST, WS_TOPMOST);

	pSWP = (SWP *) mp1;
	pSWP->hwndInsertBehind = HWND_TOP;
        pSWP->fl |= SWP_ZORDER;
      }
      break;
    case WM_PAINT:
      {
        HPS hpsBeginPaint;
        RECTL rclRect;

#ifdef DEBUG_LOGGING
        AddLog("WM_PAINT\n");
#endif

        hpsBeginPaint = WinBeginPaint(hwnd, NULL, &rclRect);

        if (pCairoSurface)
        {
          cairo_os2_surface_paint_window (pCairoSurface,
                                          hpsBeginPaint,
                                          NULL, 1);
        } else
        {
          // Fill with black
          WinQueryWindowRect(hwnd, &rclRect);
          WinFillRect(hpsBeginPaint, &rclRect, CLR_BLACK);
        }

	WinEndPaint(hpsBeginPaint);
	return (MRESULT) FALSE;
      }
    default:
      break;
  }

  return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}

// Locale object and convert object for localized date
UconvObject  ucoTemp;
LocaleObject loTemp;
char         achDateFormatString[128]; // This is filled at StartSaving() time!

int InitLocaleStuff()
{
  char *pchLanguageCode;
  int rc;
  UniChar *pucLanguageCode;
  char    *pchFrom;
  size_t   iFromCount;
  UniChar *pucTo;
  size_t   iToCount;
  size_t   iNonIdentical;
  int iLanguageCodeLen;

  // Set pchLanguageCode variable
  if (apchNLSText[SSMODULE_NLSTEXT_LANGUAGECODE])
    pchLanguageCode = apchNLSText[SSMODULE_NLSTEXT_LANGUAGECODE];
  else
  {
    pchLanguageCode = getenv("LANG");
    if (!pchLanguageCode)
      pchLanguageCode = "en_US";
  }

  // Now, we have to create an unicode string from the locale name
  // we have here. For this, we allocate memory for the UCS string!
  iLanguageCodeLen = strlen(pchLanguageCode);
  pucLanguageCode = (UniChar *) malloc(sizeof(UniChar) * iLanguageCodeLen*4+4);
  if (!pucLanguageCode)
  {
    // Not enough memory!
    loTemp = NULL;
    ucoTemp = NULL;
    return 0;
  }

  // Create unicode convert object
  rc = UniCreateUconvObject(L"", &ucoTemp);
  if (rc!=ULS_SUCCESS)
  {
    // Could not create convert object!
    loTemp = NULL;
    ucoTemp = NULL;
    return 0;
  }

  // Convert language code string to unicode string
  pchFrom = pchLanguageCode;
  iFromCount = strlen(pchLanguageCode)+1;
  pucTo = pucLanguageCode;
  iToCount = iLanguageCodeLen*4;

  if (iFromCount>1)
  {
    // It's not an empty string, so convert it!
    rc = UniUconvToUcs(ucoTemp,
                       &pchFrom,
                       &iFromCount,
                       &pucTo,
                       &iToCount,
                       &iNonIdentical);

    if (rc!=ULS_SUCCESS)
    {
      // Could not convert language code to UCS string!
      UniFreeUconvObject(ucoTemp);
      free(pucLanguageCode);
      loTemp = NULL;
      ucoTemp = NULL;
      return 0;
    }
  } else
  {
    // It's an empty string, no need to convert it, simply make an
    // emty unicode string as a result.
    pucTo[0] = 0;
  }

  // Okay, we have the language code in UCS (or empty UCS string)!
  // Try to open the locale object of it!
  rc = UniCreateLocaleObject(UNI_UCS_STRING_POINTER,
                             pucLanguageCode,
                             &loTemp);
  if (rc!=ULS_SUCCESS)
  {
    // Could not create locale object of this name!
    UniFreeUconvObject(ucoTemp);
    free(pucLanguageCode);
    loTemp = NULL;
    ucoTemp = NULL;
    return 0;
  }

  free(pucLanguageCode);

  return 1;
}

void GetLocalizedDate(char *pchBuffer, int iBufLen, struct tm *pTime)
{
  UniChar  aucTemp[1025];
  UniChar  aucFmt[129];
  int      rc;
  char     *pchTo, *pchFrom;
  size_t   iFromCount;
  UniChar  *pucFrom, *pucTo;
  size_t   iToCount;
  size_t   iNonIdentical;

  if ((!loTemp) || (!ucoTemp))
  {
    strftime(pchBuffer,
             iBufLen,
             "%x",
             pTime);
    return;
  }

  // Convert format string to unicode
  pchFrom = &(achDateFormatString[0]);
  iFromCount = strlen(pchFrom)+1;
  pucTo = aucFmt;
  iToCount = 128;

  rc = UniUconvToUcs(ucoTemp,
                     &pchFrom,
                     &iFromCount,
                     &pucTo,
                     &iToCount,
                     &iNonIdentical);

  if (rc!=ULS_SUCCESS)
  {
    // Could not convert language code to UCS string!
    // Use default then

    // Query time in locale's format
    UniStrftime(loTemp,
                aucTemp,
                1024,
                L"%x",
                pTime);
  } else
  {
    // Query time in locale's format
    UniStrftime(loTemp,
                aucTemp,
                1024,
                aucFmt,
                pTime);
  }

  // Convert it from UCS to current codepage
  pucFrom = aucTemp;
  iFromCount = UniStrlen(aucTemp)+1;
  pchTo = pchBuffer;
  iToCount = iBufLen;
  rc = UniUconvFromUcs(ucoTemp,
                       &pucFrom,
                       &iFromCount,
                       &pchTo,
                       &iToCount,
                       &iNonIdentical);


  if (rc!=ULS_SUCCESS)
  {
    strftime(pchBuffer,
             iBufLen,
             "%x",
             pTime);
    return;
  }
}

void UninitLocaleStuff()
{
  UniFreeLocaleObject(loTemp);
  UniFreeUconvObject(ucoTemp);
  loTemp = NULL;
  ucoTemp = NULL;
}

/* And the real drawing loop: */
void CairoDrawLoop(HWND             hwndClientWindow,
                   int              iWidth,
                   int              iHeight,
                   cairo_surface_t *pCairoSurface,
                   cairo_t         *pCairoHandle)
{
  char achTimeString[256];
  time_t Time;
  struct tm *pTime;
  int iOldSec;
  cairo_text_extents_t CairoTextExtents;
  double xpos, ypos, xspeed, yspeed;
  double x, y, x2, y2;
  double dAlpha;
  double dScale;
  int iUniSize;
  double dXAspect, dYAspect;
  int i;

  // Initialize locale (time formatting)
  InitLocaleStuff();

  // Initialize position and speed
  srand(clock());
  xpos = 0;
  ypos = 0;
  xspeed = 0.001 * ((rand()/(RAND_MAX/2))?1:-1);
  yspeed = 0.001 * ((rand()/(RAND_MAX/2))?1:-1);

  // Do the main drawing loop as long as needed!
  while (!bShutdownDrawing)
  {
    if (bPaused)
    {
      // Do not draw anything if we're paused!
      DosSleep(250);
    }
    else
    {
      // Otherwise draw something!

      // Get current time
      Time = time(NULL);
      pTime = localtime(&Time);

      // Draw the clock!

      // Save cairo canvas state
      cairo_save(pCairoHandle);

      // Calculate screen aspect ratio
      if (iWidth<iHeight)
      {
        iUniSize = iWidth;
        dYAspect = iHeight;
        dYAspect /= iWidth;
        dXAspect = 1;
      }
      else
      {
        iUniSize = iHeight;
        dXAspect = iWidth;
        dXAspect /= iHeight;
        dYAspect = 1;
      }

      dScale = ModuleConfig.iSizePercent / 100.0;

      // Scale canvas so we'll have a
      // normalized coordinate system of (0;0) -> (1;1)
      cairo_scale(pCairoHandle,
                  iUniSize,
                  iUniSize);

      // Clear background with black
      cairo_set_source_rgb (pCairoHandle, 0, 0 ,0);
      cairo_rectangle (pCairoHandle,
                       0, 0,
                       dXAspect, dYAspect);
      cairo_fill(pCairoHandle);

      // Modify clock position, and
      if (ModuleConfig.bLetTheClockMoveAroundTheScreen)
      {
        xpos+=xspeed;
        ypos+=yspeed;

        // Make sure we'll bounce back sometimes.
        if (xpos<-((0.5*dXAspect)-0.45*dScale))
        {
          xpos = -((0.5*dXAspect)-0.45*dScale);
          xspeed = -xspeed;
        }
        if (xpos>(0.5*dXAspect)-0.45*dScale)
        {
          xpos = (0.5*dXAspect)-0.45*dScale;
          xspeed = -xspeed;
        }

        if (ypos<-((0.5*dYAspect)-0.45*dScale))
        {
          ypos = -((0.5*dYAspect)-0.45*dScale);
          yspeed = -yspeed;
        }
        if (ypos>(0.5*dYAspect)-0.45*dScale)
        {
          ypos = (0.5*dYAspect)-0.45*dScale;
          yspeed = -yspeed;
        }
      } else
      {
        xpos = 0;
        ypos = 0;
      }

      // Move all the things around
      cairo_translate(pCairoHandle,
                      0.5*dXAspect + xpos,
                      0.5*dYAspect + ypos);

      // Scale all the drawings coming after this line
      cairo_scale(pCairoHandle,
                  dScale,
                  dScale);

      // Draw markers
      for (i=0; i<60; i++)
      {
        dAlpha = 360.0 * i / 60.0;

        if (i % 5==0)
        {
          cairo_set_line_width(pCairoHandle, 0.018);
          x = 0.32 * sin(dAlpha*M_PI/180.0);
          y = 0.32 * cos(dAlpha*M_PI/180.0);
        } else
        {
          cairo_set_line_width(pCairoHandle, 0.005);
          x = 0.37 * sin(dAlpha*M_PI/180.0);
          y = 0.37 * cos(dAlpha*M_PI/180.0);
        }
        x2 = 0.41 * sin(dAlpha*M_PI/180.0);
        y2 = 0.41 * cos(dAlpha*M_PI/180.0);

        cairo_set_source_rgb (pCairoHandle, 0.8, 0.8, 0.8);
        cairo_move_to (pCairoHandle, x, -y);
        cairo_line_to(pCairoHandle, x2, -y2);
        cairo_close_path (pCairoHandle);
        cairo_stroke (pCairoHandle);
      }

      // Draw hour marker
      dAlpha = 360.0 * (pTime->tm_hour%12) / 12.0;
      dAlpha+= (pTime->tm_min%60)*0.5 + (pTime->tm_sec%60)*0.0083;
      x = 0.25 * sin(dAlpha*M_PI/180.0);
      y = 0.25 * cos(dAlpha*M_PI/180.0);
      cairo_set_line_width(pCairoHandle, 0.015);
      cairo_set_source_rgb (pCairoHandle, 1, 1, 1);
      cairo_arc(pCairoHandle, 0, 0, 0.020, 0, 2*M_PI);
      cairo_fill(pCairoHandle);
      cairo_move_to (pCairoHandle, 0, 0);
      cairo_line_to(pCairoHandle, x, -y);
      cairo_close_path (pCairoHandle);
      cairo_stroke (pCairoHandle);

      // Draw minute marker
      dAlpha = 360.0 * pTime->tm_min / 60.0;
      dAlpha+= (pTime->tm_sec%60)/10.0;
      x = 0.35 * sin(dAlpha*M_PI/180.0);
      y = 0.35 * cos(dAlpha*M_PI/180.0);
      cairo_set_line_width(pCairoHandle, 0.015);
      cairo_set_source_rgb (pCairoHandle, 1, 1, 1);
      cairo_arc(pCairoHandle, 0, 0, 0.017, 0, 2*M_PI);
      cairo_fill(pCairoHandle);
      cairo_move_to (pCairoHandle, 0, 0);
      cairo_line_to(pCairoHandle, x, -y);
      cairo_close_path (pCairoHandle);
      cairo_stroke (pCairoHandle);

      // Draw second marker
      dAlpha = 360.0 * pTime->tm_sec / 60.0;
      x = 0.35 * sin(dAlpha*M_PI/180.0);
      y = 0.35 * cos(dAlpha*M_PI/180.0);
      cairo_set_line_width(pCairoHandle, 0.01);
      cairo_set_source_rgb (pCairoHandle, 0.8, 0.1, 0.1);
      cairo_arc(pCairoHandle, 0, 0, 0.014, 0, 2*M_PI);
      cairo_fill(pCairoHandle);
      cairo_move_to (pCairoHandle, 0, 0);
      cairo_line_to(pCairoHandle, x, -y);
      cairo_close_path (pCairoHandle);

      cairo_stroke (pCairoHandle);

      if (ModuleConfig.bShowDigitalClock)
      {
        // Now print there the time!
        strftime(achTimeString,
                 sizeof(achTimeString),
                 "%X",  // %T means HH:MM:SS
                 pTime);
        cairo_select_font_face(pCairoHandle, "Sans",
                               CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(pCairoHandle,
                            0.08);

        // Get how bit the text will be!
        cairo_text_extents(pCairoHandle,
                           "00:00:00",
                           &CairoTextExtents);

        // Print the text, centered!
        cairo_move_to(pCairoHandle,
                      -CairoTextExtents.width/2,
                      CairoTextExtents.height/2 - 0.15);
        cairo_text_path(pCairoHandle, achTimeString);

        cairo_set_source_rgba(pCairoHandle,
                              0.2, 0.2, 0.8, 0.55);
        cairo_fill_preserve(pCairoHandle);
        cairo_set_source_rgba(pCairoHandle,
                              0.6, 0.6, 1.0, 0.35);
        cairo_set_line_width(pCairoHandle, 0.006);
        cairo_close_path(pCairoHandle);
        cairo_stroke (pCairoHandle);
      }

      if (ModuleConfig.bShowDate)
      {
        // Now print there the date!
        GetLocalizedDate(achTimeString, sizeof(achTimeString), pTime);

        cairo_select_font_face(pCairoHandle, "Sans",
                               CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(pCairoHandle,
                            0.05);

        // Get how bit the text will be!
        cairo_text_extents(pCairoHandle,
                           achTimeString,
                           &CairoTextExtents);

        // Print the text, centered!
        cairo_move_to(pCairoHandle,
                      -CairoTextExtents.width/2,
                      CairoTextExtents.height/2 + 0.15);
        cairo_text_path(pCairoHandle, achTimeString);

        cairo_set_source_rgba(pCairoHandle,
                              0.90, 0.90, 0.2, 0.75);
        cairo_fill_preserve(pCairoHandle);
        cairo_set_source_rgba(pCairoHandle,
                              0, 0, 0, 0.4);
        cairo_set_line_width(pCairoHandle, 0.002);
        cairo_close_path(pCairoHandle);
        cairo_stroke (pCairoHandle);
      }

      // Restore canvas state to original one
      cairo_restore(pCairoHandle);

      // Make changes visible
      WinInvalidateRect(hwndClientWindow, NULL, FALSE);

    }
    // Wait for next second to come, or
    // to get an event of shutdown!
    iOldSec = pTime->tm_sec;
    do {
      Time = time(NULL);
      pTime = localtime(&Time);
      DosSleep(100);
    } while ((pTime->tm_sec == iOldSec) && (!bShutdownDrawing));
  }

  // Got shutdown event, so stopped drawing. That's all.
  UninitLocaleStuff();
}


/* One helper function: */
static void inline DisableFPUException()
{
  unsigned short usCW;

  // Some OS/2 PM API calls modify the FPU Control Word,
  // but forgets to restore it.

  // This can result in XCPT_FLOAT_INVALID_OPCODE exceptions,
  // so to be sure, we always disable Invalid Opcode FPU exception
  // before using FPU stuffs with Cairo or from this thread.

  usCW = _control87(0, 0);
  usCW = usCW | EM_INVALID | 0x80;
  _control87(usCW, MCW_EM | 0x80);
}

void CairoDrawThread(void *pParam)
{
  HWND hwndClientWindow = (HWND) pParam;
  HPS hpsClientWindow;
  SWP swpTemp;

  // Query HPS of HWND
  hpsClientWindow = WinGetPS(hwndClientWindow);
  WinQueryWindowPos(hwndClientWindow, &swpTemp);

  // Set FPU state which might have been changed by some Win* calls!
  // (Workaround for a PM bug)
  DisableFPUException();

  // Initialize cairo support
  cairo_os2_init();

  // Create cairo surface
  pCairoSurface = cairo_os2_surface_create_for_window(hwndClientWindow,
                                                      swpTemp.cx,
                                                      swpTemp.cy);

  // Tell the surface the HWND too, so if the application decides
  // that it wants to draw itself, then it will be able to turn
  // on blit_as_changes.
  cairo_os2_surface_set_hwnd(pCairoSurface, hwndClientWindow);

  // Create Cairo drawing handle for the surface
  pCairoHandle = cairo_create(pCairoSurface);

  // Do the main drawing loop as long as needed!
  bShutdownDrawing = 0;
  CairoDrawLoop(hwndClientWindow,
                swpTemp.cx, swpTemp.cy,
                pCairoSurface,
                pCairoHandle);

  // Clean up!
  cairo_destroy(pCairoHandle);
  cairo_surface_destroy(pCairoSurface);

  cairo_os2_fini();

  WinReleasePS(hpsClientWindow);

  _endthread();
}


void fnSaverThread(void *p)
{
  HWND hwndOldFocus;
  HWND hwndParent = (HWND) p;
  HWND hwndOldSysModal;
  HAB hab;
  QMSG msg;
  ULONG ulStyle;

  // Set our thread to slightly more than regular to be able
  // to update the screen fine.
  DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR, +5, 0);

  hab = WinInitialize(0);
  hmqSaverThreadMsgQueue = WinCreateMsgQueue(hab, 0);

  if (bOnlyPreviewMode)
  {
    PFNWP pfnOldWindowProc;
    // We should run in preview mode, so the hwndParent we have is not the
    // desktop, but a special window we have to subclass.
    pfnOldWindowProc = WinSubclassWindow(hwndParent,
                                         (PFNWP) fnSaverWindowProc);

    // Initialize window proc (simulate WM_CREATE)
    WinSendMsg(hwndParent, WM_SUBCLASS_INIT, (MPARAM) NULL, (MPARAM) NULL);
    // Also make sure that the window will be redrawn with this new
    // window proc.
    WinInvalidateRect(hwndParent, NULL, FALSE);

    iSaverThreadState = STATE_RUNNING;

#ifdef DEBUG_LOGGING
    AddLog("[fnSaverThread] : Starting Cairo drawing thread\n");
#endif
    // Start Cairo drawing thread
    tidCairoDrawThread = _beginthread(CairoDrawThread,
                                      0,
                                      1024*1024,
                                      (void *) hwndParent);

#ifdef DEBUG_LOGGING
    AddLog("[fnSaverThread] : Entering message loop (Preview)\n");
#endif

    // Process messages until WM_QUIT!
    while (WinGetMsg(hab, &msg, 0, 0, 0))
      WinDispatchMsg(hab, &msg);

#ifdef DEBUG_LOGGING
    AddLog("[fnSaverThread] : Leaved message loop (Preview)\n");
#endif

#ifdef DEBUG_LOGGING
    AddLog("[fnSaverThread] : Stopping Cairo drawing thread\n");
#endif
    bShutdownDrawing = 1;
    DosWaitThread(&tidCairoDrawThread, DCWW_WAIT);


    // Uinitialize window proc (simulate WM_DESTROY)
    WinSendMsg(hwndParent, WM_SUBCLASS_UNINIT, (MPARAM) NULL, (MPARAM) NULL);

    // Undo subclassing
    WinSubclassWindow(hwndParent,
                      pfnOldWindowProc);
    // Also make sure that the window will be redrawn with the old
    // window proc.
    WinInvalidateRect(hwndParent, NULL, FALSE);

    iSaverThreadState = STATE_STOPPED_OK;
  } else
  {
    // We should run in normal mode, so create a new window, topmost, and everything else...
    WinRegisterClass(hab, (PSZ) SAVERWINDOW_CLASS,
                     (PFNWP) fnSaverWindowProc,
                     CS_SIZEREDRAW | CS_CLIPCHILDREN | CS_CLIPSIBLINGS, 0);

    hwndOldFocus = WinQueryFocus(HWND_DESKTOP);

    // Create the saver output window so that it will be the child of
    // the given parent window.
    // Make window 'Always on top' because we'll be in real screensaver mode!
    ulStyle = WS_VISIBLE | WS_TOPMOST;
    hwndSaverWindow = WinCreateWindow(HWND_DESKTOP, SAVERWINDOW_CLASS, "Screen saver",
                                      ulStyle,
                                      0, 0,
                                      (int) WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN),
                                      (int) WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN),
                                      HWND_DESKTOP,
                                      HWND_TOP,
                                      0x9fff, // Some ID....
                                      NULL, NULL);
    if (!hwndSaverWindow)
    {
#ifdef DEBUG_LOGGING
      AddLog("[fnSaverThread] : Could not create window!\n");
#endif
      // Yikes, could not create window!
      iSaverThreadState = STATE_STOPPED_ERROR;
    } else
    {
      // Cool, window created!

      // Make sure nobody will be able to switch away of it!
      // We do this by making the window system-modal, and giving it the focus!
      hwndOldSysModal = WinQuerySysModalWindow(HWND_DESKTOP);
      WinSetSysModalWindow(HWND_DESKTOP, hwndSaverWindow);
      WinSetFocus(HWND_DESKTOP, hwndSaverWindow);

      iSaverThreadState = STATE_RUNNING;

#ifdef DEBUG_LOGGING
      AddLog("[fnSaverThread] : Starting Cairo drawing thread\n");
#endif
      // Start Cairo drawing thread
      tidCairoDrawThread = _beginthread(CairoDrawThread,
                                        0,
                                        1024*1024,
                                        (void *) hwndSaverWindow);

#ifdef DEBUG_LOGGING
      AddLog("[fnSaverThread] : Entering message loop (Real)\n");
#endif
      // Process messages until WM_QUIT!
      while (WinGetMsg(hab, &msg, 0, 0, 0))
        WinDispatchMsg(hab, &msg);
#ifdef DEBUG_LOGGING
      AddLog("[fnSaverThread] : Leaved message loop (Real)\n");
#endif

#ifdef DEBUG_LOGGING
      AddLog("[fnSaverThread] : Stopping Cairo drawing thread\n");
#endif
      bShutdownDrawing = 1;
      DosWaitThread(&tidCairoDrawThread, DCWW_WAIT);

      // Undo system modalling
      WinSetSysModalWindow(HWND_DESKTOP, hwndOldSysModal);

      // Window closed, so destroy every resource we created!
      WinDestroyWindow(hwndSaverWindow);

      // Restore focus to old window
      WinSetFocus(HWND_DESKTOP, hwndOldFocus);

#ifdef DEBUG_LOGGING
      AddLog("[fnSaverThread] : STATE_STOPPED_OK\n");
#endif

      iSaverThreadState = STATE_STOPPED_OK;
    }
  }

  WinDestroyMsgQueue(hmqSaverThreadMsgQueue);
  WinTerminate(hab);

  _endthread();
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_GetModuleDesc(SSModuleDesc_p pModuleDesc, char *pchHomeDirectory)
{
  FILE *hfNLSFile;

  if (!pModuleDesc)
    return SSMODULE_ERROR_INVALIDPARAMETER;

  // Return info about module!
  pModuleDesc->iVersionMajor = 2;
  pModuleDesc->iVersionMinor = 10;
  strcpy(pModuleDesc->achModuleName, "Cairo Clock");
  strcpy(pModuleDesc->achModuleDesc,
         "Clock and current date.\n"
         "Using the Cairo library."
        );

  // If we have NLS support, then show the module description in
  // the chosen language!
  hfNLSFile = internal_OpenNLSFile(pchHomeDirectory);
  if (hfNLSFile)
  {
    sprintmsg(pModuleDesc->achModuleName, hfNLSFile, "MOD_NAME");
    sprintmsg(pModuleDesc->achModuleDesc, hfNLSFile, "MOD_DESC");
    internal_CloseNLSFile(hfNLSFile);
  }

  pModuleDesc->bConfigurable = TRUE;
  pModuleDesc->bSupportsPasswordProtection = TRUE;

  return SSMODULE_NOERROR;
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_Configure(HWND hwndOwner, char *pchHomeDirectory)
{
  HWND hwndDlg;
  ConfigDlgInitParams_t CfgDlgInit;

  // This stuff here should create a window, to let the user
  // configure the module. The configuration should be read and saved from/to
  // a private config file in pchHomeDirectory, or in INI files.

  CfgDlgInit.cbSize = sizeof(CfgDlgInit);
  CfgDlgInit.pchHomeDirectory = pchHomeDirectory;
  internal_LoadConfig(&(CfgDlgInit.ModuleConfig), pchHomeDirectory);

  hwndDlg = WinLoadDlg(HWND_DESKTOP, hwndOwner,
                       fnConfigDialogProc,
                       hmodOurDLLHandle,
                       DLG_CONFIGURE,
                       &CfgDlgInit);

  if (!hwndDlg)
    return SSMODULE_ERROR_INTERNALERROR;

  if (WinProcessDlg(hwndDlg) == PB_CONFOK)
  {
    // User dismissed the dialog with OK button,
    // so save configuration!
    internal_SaveConfig(&(CfgDlgInit.ModuleConfig), pchHomeDirectory);
  }

  WinDestroyWindow(hwndDlg);

  return SSMODULE_NOERROR;
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_StartSaving(HWND hwndParent, char *pchHomeDirectory, int bPreviewMode)
{
  FILE *hfNLSFile;

  // This is called when we should start the saving.
  // Return error if already running, start the saver thread otherwise!

  if (bRunning)
    return SSMODULE_ERROR_ALREADYRUNNING;

  // Read configuration!
  internal_LoadConfig(&ModuleConfig, pchHomeDirectory);

  // Get date format string from NLS file
  // Default: %x
  snprintf(achDateFormatString, sizeof(achDateFormatString), "%%x");
  if (pchHomeDirectory)
  {
    hfNLSFile = internal_OpenNLSFile(pchHomeDirectory);
    if (hfNLSFile)
    {
      // Get date format from NLS file
      sprintmsg(achDateFormatString, hfNLSFile, "MOD_DATE");

      internal_CloseNLSFile(hfNLSFile);
    }
  }

  iSaverThreadState = STATE_UNKNOWN;
  bOnlyPreviewMode = bPreviewMode;
  tidSaverThread = _beginthread(fnSaverThread,
                                0,
                                1024*1024,
                                (void *) hwndParent);

  if (tidSaverThread==0)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSModule_StartSaving] : Error creating screensaver thread!\n");
#endif
    // Error creating screensaver thread!
    return SSMODULE_ERROR_INTERNALERROR;
  }

  // Wait for saver thread to start up!
  while (iSaverThreadState==STATE_UNKNOWN) DosSleep(32);
  if (iSaverThreadState!=STATE_RUNNING)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSModule_StartSaving] : Something went wrong in screensaver thread!\n");
#endif

    // Something wrong in saver thread!
    DosWaitThread(&tidSaverThread, DCWW_WAIT);
    return SSMODULE_ERROR_INTERNALERROR;
  }

  // Fine, screen saver started and running!
  bRunning = TRUE;
  return SSMODULE_NOERROR;
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_StopSaving(void)
{
  // This is called when we have to stop saving.
#ifdef DEBUG_LOGGING
  AddLog("[SSModule_StopSaving] : Enter!\n");
#endif
  if (!bRunning)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSModule_StopSaving] : Not running, Leaving.\n");
#endif
    return SSMODULE_ERROR_NOTRUNNING;
  }

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_StopSaving] : Closing saver window\n");
#endif

  // Notify saver thread to stop!
  if (bOnlyPreviewMode)
  {
    // In preview mode, which means that there is no window created, but the
    // window was subclassed. So we cannot close the window, but we have to
    // post the WM_QUIT message into the thread's message queue to notify it
    // that this is the end of its job.
    WinPostQueueMsg(hmqSaverThreadMsgQueue, WM_QUIT, (MPARAM) NULL, (MPARAM) NULL);
  }
  else
  {
    // Close saver window
    WinPostMsg(hwndSaverWindow, WM_QUIT, (MPARAM) NULL, (MPARAM) NULL);
  }

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_StopSaving] : Waiting for thread to die\n");
#endif

  // Wait for the thread to stop
  DosWaitThread(&tidSaverThread, DCWW_WAIT);

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_StopSaving] : Screen saver stopped.\n");
#endif

  // Ok, screensaver stopped.
  bRunning = FALSE;
  return SSMODULE_NOERROR;
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_AskUserForPassword(int iPwdBuffSize, char *pchPwdBuff)
{
  // This is called when we should ask the password from the user.
  if (!bRunning)
    return SSMODULE_ERROR_NOTRUNNING;

  if ((iPwdBuffSize<=0) || (!pchPwdBuff))
    return SSMODULE_ERROR_INVALIDPARAMETER;

  return (int) WinSendMsg(hwndSaverWindow, WM_ASKPASSWORD,
			  (MPARAM) pchPwdBuff,
			  (MPARAM) iPwdBuffSize);
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_ShowWrongPasswordNotification(void)
{
  // This is called if the user has entered a wrong password, so we should
  // notify the user about this, in a way that fits the best to our screensaver.

  if (!bRunning)
    return SSMODULE_ERROR_NOTRUNNING;

  return (int) WinSendMsg(hwndSaverWindow, WM_SHOWWRONGPASSWORD,
			  (MPARAM) NULL,
			  (MPARAM) NULL);
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_PauseSaving(void)
{
  // This is called when the system wants us to pause saving, because
  // a DPMS saving state has been set, and no need for CPU-intensive stuffs.

  if (!bRunning)
    return SSMODULE_ERROR_NOTRUNNING;

  return (int) WinSendMsg(hwndSaverWindow, WM_PAUSESAVING, (MPARAM) NULL, (MPARAM) NULL);
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_ResumeSaving(void)
{
  // This is called when the system wants us to resume saving, because
  // a DPMS state has been ended, and we should continue our CPU-intensive stuffs.

  if (!bRunning)
    return SSMODULE_ERROR_NOTRUNNING;

  return (int) WinSendMsg(hwndSaverWindow, WM_RESUMESAVING, (MPARAM) NULL, (MPARAM) NULL);
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_SetNLSText(int iNLSTextID, char *pchNLSText)
{

  if ((iNLSTextID<0) ||
      (iNLSTextID>SSMODULE_NLSTEXT_MAX))
    return SSMODULE_ERROR_INVALIDPARAMETER;

  if (DosRequestMutexSem(hmtxUseNLSTextArray, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {

    if (pchNLSText)
    {
      // Store this string!
      if (apchNLSText[iNLSTextID])
      {
        free(apchNLSText[iNLSTextID]);
        apchNLSText[iNLSTextID] = NULL;
      }
  
      apchNLSText[iNLSTextID] = (char *) malloc(strlen(pchNLSText)+1);
      if (apchNLSText[iNLSTextID])
        strcpy(apchNLSText[iNLSTextID], pchNLSText);
    } else
    {
      // Free the old stored string
      if (apchNLSText[iNLSTextID])
      {
        free(apchNLSText[iNLSTextID]);
        apchNLSText[iNLSTextID] = NULL;
      }
    }

    DosReleaseMutexSem(hmtxUseNLSTextArray);
  }
  return SSMODULE_NOERROR;
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_SetCommonSetting(int iSettingID, void *pSettingValue)
{
  switch (iSettingID)
  {
    case SSMODULE_COMMONSETTING_FIRSTKEYGOESTOPWDWINDOW:
      bFirstKeyGoesToPwdWindow = (int) pSettingValue;
      return SSMODULE_NOERROR;
    default:
      return SSMODULE_ERROR_NOTSUPPORTED;
  }
}

//---------------------------------------------------------------------
// LibMain
//
// This gets called at DLL initialization and termination.
//---------------------------------------------------------------------
unsigned _System LibMain(unsigned hmod, unsigned termination)
{
  int i;

  if (termination)
  {
    // Cleanup!
    if (bRunning)
      SSModule_StopSaving();

    // Free NLS text, if it wouldn't be (safety net)
    for (i=0; i<SSMODULE_NLSTEXT_MAX+1; i++)
    {
      if (apchNLSText[i])
      {
        free(apchNLSText[i]);
        apchNLSText[i] = NULL;
      }
    }
    // Free semaphore
    DosCloseMutexSem(hmtxUseNLSTextArray); hmtxUseNLSTextArray = NULLHANDLE;
  } else
  {
    // Startup!
    hmodOurDLLHandle = (HMODULE) hmod;
    bRunning = FALSE;
    // Initialize NLS text array
    for (i=0; i<SSMODULE_NLSTEXT_MAX+1; i++)
      apchNLSText[i] = NULL;
    // Create semaphore to protect it
    DosCreateMutexSem(NULL, &hmtxUseNLSTextArray, 0, FALSE);
  }
  return 1;
}

