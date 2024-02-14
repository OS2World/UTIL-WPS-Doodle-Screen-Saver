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
#include <time.h>
#include <math.h>
#include <float.h>

#define INCL_DOS
#define INCL_WIN
#define INCL_GPI
#define INCL_ERRORS
#define INCL_TYPES
#include <os2.h>
#include <mmioos2.h>
#include <dive.h>
#include <fourcc.h>

#include "hwvideo.h"  // WarpOverlay header file

#include "SSModule.h"
#include "Snow-Resources.h"
#include "FrameRate.h"
#include "MSGX.h" // NLS support

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
#define SAVERWINDOW_CLASS "SNOW_SCREENSAVER_CLASS"

// Private window messages
#define WM_SUBCLASS_INIT       (WM_USER+1)
#define WM_SUBCLASS_UNINIT     (WM_USER+2)
#define WM_ASKPASSWORD         (WM_USER+3)
#define WM_SHOWWRONGPASSWORD   (WM_USER+4)
#define WM_PAUSESAVING         (WM_USER+5)
#define WM_RESUMESAVING        (WM_USER+6)

#define WM_SCREENSHOT_DONE     (WM_USER+9)

// Some variables, needed for every screen saver
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

// And now the private types and variables
#define MODULE_RESTYPE_SCREEN      0
#define MODULE_RESTYPE_HALFSCREEN  1
#define MODULE_RESTYPE_CUSTOM      2
typedef struct ModuleConfig_s
{
  int iResType;
  int iResCustomXSize;
  int iResCustomYSize;
  int bIdleSaver;
  int iFPS;
  int bUseWO;
  int bUseObjects;
  int bUseAnims;
} ModuleConfig_t, *ModuleConfig_p;

typedef struct ConfigDlgInitParams_s
{
  USHORT          cbSize; // Required to be able to pass this structure to WM_INITDLG
  char           *pchHomeDirectory;
  ModuleConfig_t  ModuleConfig;
} ConfigDlgInitParams_t, *ConfigDlgInitParams_p;

ModuleConfig_t  ModuleConfig;
char     achUndoBuf[512];
int      bShutdownRequest;
HEV      hevBlitEvent; // Event semaphore, posted if the image should be blit!
TID      tidBlitterThread;
int      bUseFrameRegulation;
HDIVE    hDive;
int      bVRNEnabled;
HMTX     hmtxUseDive;
ULONG    ulWorkImage;
char    *pchScreenShot;
FOURCC   fccBufferFormat; // Fourcc of the buffer format
int      iBufferBPP; // Number of bits per pixel in buffer
int      bScreenCaptured, bScreenShotThreadDone;
DIVE_CAPS DiveCaps;
char     *pchVRAM;
int      iBufferXSize;
int      iBufferYSize;

// ---    HWVideo stuff    ---
int      bHWVideoPresent;
ULONG    ulColorKey = 0x000d0d0d; // Special color (Almost black)  (ARGB)
HMODULE  hmodHWVideo;
HWVIDEOCAPS  OverlayCaps;
ULONG APIENTRY (*pfnHWVIDEOInit)(void);
ULONG APIENTRY (*pfnHWVIDEOCaps)(PHWVIDEOCAPS pCaps);
ULONG APIENTRY (*pfnHWVIDEOSetup)(PHWVIDEOSETUP pSetup);
ULONG APIENTRY (*pfnHWVIDEOBeginUpdate)(PVOID *ppBuffer, PULONG pulPhysBuffer);
ULONG APIENTRY (*pfnHWVIDEOEndUpdate)(void);
ULONG APIENTRY (*pfnHWVIDEOGetAttrib)(ULONG ulAttribNum,PHWATTRIBUTE pAttrib);
ULONG APIENTRY (*pfnHWVIDEOSetAttrib)(ULONG ulAttribNum,PHWATTRIBUTE pAttrib);
ULONG APIENTRY (*pfnHWVIDEOClose)(void);

// RGB to YUV conversion uses these:
#define RGB2YUV_SHIFT 8
#define BY ((int)( 0.098*(1<<RGB2YUV_SHIFT)+0.5))
#define BV ((int)(-0.071*(1<<RGB2YUV_SHIFT)+0.5))
#define BU ((int)( 0.439*(1<<RGB2YUV_SHIFT)+0.5))
#define GY ((int)( 0.504*(1<<RGB2YUV_SHIFT)+0.5))
#define GV ((int)(-0.368*(1<<RGB2YUV_SHIFT)+0.5))
#define GU ((int)(-0.291*(1<<RGB2YUV_SHIFT)+0.5))
#define RY ((int)( 0.257*(1<<RGB2YUV_SHIFT)+0.5))
#define RV ((int)( 0.439*(1<<RGB2YUV_SHIFT)+0.5))
#define RU ((int)(-0.148*(1<<RGB2YUV_SHIFT)+0.5))

// -------

#define NUM_OF_CUSTOM_RESOLUTIONS 16
int aiCustomResWidth[NUM_OF_CUSTOM_RESOLUTIONS] =
  { 320, 320, 400, 512, 640, 640, 720, 720, 800, 1024, 1152, 1280, 1600, 1800, 1920, 2048 };
int aiCustomResHeight[NUM_OF_CUSTOM_RESOLUTIONS] =
  { 200, 240, 300, 384, 480, 512, 480, 576, 600,  768,  864, 1024, 1200, 1350, 1440, 1536 };


//#define DEBUG_LOGGING
#ifdef DEBUG_LOGGING
void AddLog(char *pchMsg)
{
  FILE *hFile;

  hFile = fopen("c:\\antvsim.log", "at+");
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
	sprintf(pchFileName, "%sModules\\Snow\\%s.msg", pchHomeDirectory, achRealLocaleName);
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
	  sprintf(pchFileName, "%sModules\\Snow\\%s.msg", pchHomeDirectory, pchLang);
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
  pConfig->iResType = MODULE_RESTYPE_CUSTOM;
  pConfig->iResCustomXSize = 320;
  pConfig->iResCustomYSize = 200;
  pConfig->bIdleSaver = TRUE;
  pConfig->iFPS = 20;
  pConfig->bUseWO = TRUE;
  pConfig->bUseObjects = TRUE;
  pConfig->bUseAnims = TRUE;

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
    strncat(achFileName, ".dssaver\\Snow.CFG", sizeof(achFileName));
    // Try to open that config file!
    hFile = fopen(achFileName, "rt");
  }

  if (!hFile)
  {
    // If we could not open a config file in the HOME directory, or there was
    // no HOME directory, then try to use the DSSaver Global home directory!
    snprintf(achFileName, sizeof(achFileName), "%sSnow.CFG", pchHomeDirectory);
    hFile = fopen(achFileName, "rt");
    if (!hFile)
      return; // Could not open file!
  }

  iScanned = fscanf(hFile, "%d %d %d %d %d %d %d %d",
                    &(TempConfig.iResType),
                    &(TempConfig.iResCustomXSize),
                    &(TempConfig.iResCustomYSize),
                    &(TempConfig.bIdleSaver),
                    &(TempConfig.iFPS),
                    &(TempConfig.bUseWO),
                    &(TempConfig.bUseObjects),
                    &(TempConfig.bUseAnims));
  fclose(hFile);

  if (iScanned == 8)
  {
    // Fine, we could read all we wanted, so use the values we read!
    memcpy(pConfig, &TempConfig, sizeof(TempConfig));
  }
#ifdef DEBUG_LOGGING
  else
    AddLog("[internal_LoadConfig] : Could not parse all the 8 settings, using defaults!\n");
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
    strncat(achFileName, "\\Snow.CFG", sizeof(achFileName));
    // Try to open that config file!
    hFile = fopen(achFileName, "wt");
  }

  if (!hFile)
  {
    // If we could not open a config file in the HOME directory, or there was
    // no HOME directory, then try to use the DSSaver Global home directory!
    snprintf(achFileName, sizeof(achFileName), "%sSnow.CFG", pchHomeDirectory);
    hFile = fopen(achFileName, "wt");
    if (!hFile)
      return; // Could not open file!
  }

  fprintf(hFile, "%d %d %d %d %d %d %d %d",
          (pConfig->iResType),
          (pConfig->iResCustomXSize),
          (pConfig->iResCustomYSize),
          (pConfig->bIdleSaver),
          (pConfig->iFPS),
          (pConfig->bUseWO),
          (pConfig->bUseObjects),
          (pConfig->bUseAnims));

  fclose(hFile);
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

static void internal_GetRadioButtonExtraSize(HWND hwnd, ULONG ulID, int *piCX, int *piCY)
{
  HWND hwndTemp;
  SWP swpTemp;

  // Create temporary radiobutton with no text inside
  hwndTemp = WinCreateWindow(hwnd, WC_BUTTON,
                             "",
                             BS_RADIOBUTTON | BS_AUTOSIZE,
                             10, 10,
                             -1, -1,
                             hwnd,
                             HWND_TOP,
                             ID_TEMPCHECKBOX,
                             NULL, NULL);

  // Query textbox size
  WinQueryWindowPos(hwndTemp, &swpTemp);
  // Destroy temporary radiobutton
  WinDestroyWindow(hwndTemp);

  *piCX = swpTemp.cx+1;
  *piCY = swpTemp.cy+1;
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
      WinSetDlgItemText(hwnd, GB_IMAGERESOLUTION, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0003"))
      WinSetDlgItemText(hwnd, RB_RESOLUTIONOFTHESCREEN, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0004"))
      WinSetDlgItemText(hwnd, RB_HALFTHERESOLUTIONOFTHESCREEN, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0005"))
      WinSetDlgItemText(hwnd, RB_CUSTOMRESOLUTION, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0006"))
      WinSetDlgItemText(hwnd, ST_RESOLUTION, achTemp);

    // Groupbox-2
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0007"))
      WinSetDlgItemText(hwnd, GB_PROCESSORUSAGE, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0008"))
      WinSetDlgItemText(hwnd, RB_RUNINIDLEPRIORITY, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0009"))
      WinSetDlgItemText(hwnd, RB_RUNINREGULARPRIORITY, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0010"))
      WinSetDlgItemText(hwnd, ST_ANIMATIONSPEED, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0011"))
      WinSetDlgItemText(hwnd, ST_FPS, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0012"))
      WinSetDlgItemText(hwnd, CB_USEWARPOVERLAYIFPRESENT, achTemp);

    // Groupbox-0
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0015"))
      WinSetDlgItemText(hwnd, GB_VISUALOPTIONS, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0016"))
      WinSetDlgItemText(hwnd, CB_USERANDOMOBJECTS, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0017"))
      WinSetDlgItemText(hwnd, CB_USERANDOMANIMATIONS, achTemp);

    // Buttons
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0013"))
      WinSetDlgItemText(hwnd, PB_CONFOK, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0014"))
      WinSetDlgItemText(hwnd, PB_CONFCANCEL, achTemp);

    internal_CloseNLSFile(hfNLSFile);
  }
}

static void internal_AdjustConfigDialogControls(HWND hwnd)
{
  SWP swpTemp, swpTemp2, swpTemp3, swpWindow;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  int staticheight, radioheight, buttonheight, gbheight;
  int iCBCX, iCBCY; // Checkbox extra sizes
  int iRBCX, iRBCY; // RadioButton extra sizes
  int iCX, iCY, iBottomButtonsWidth, i;
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(hwnd))
  {
    WinEnableWindowUpdate(hwnd, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Get window size!
  WinQueryWindowPos(hwnd, &swpWindow);

  // Query static height in pixels. 
  internal_GetStaticTextSize(hwnd, ST_ANIMATIONSPEED, &iCX, &iCY);
  staticheight = iCY;

  // Query button height in pixels.
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CONFOK), &swpTemp);
  buttonheight = swpTemp.cy;

  // Query radio button height in pixels.
  WinQueryWindowPos(WinWindowFromID(hwnd, RB_RUNINIDLEPRIORITY), &swpTemp);
  radioheight = swpTemp.cy;

  // Get extra sizes
  internal_GetCheckboxExtraSize(hwnd, CB_USEWARPOVERLAYIFPRESENT, &iCBCX, &iCBCY);
  internal_GetRadioButtonExtraSize(hwnd, RB_CUSTOMRESOLUTION, &iRBCX, &iRBCY);

  // Calculate window width
  internal_GetStaticTextSize(hwnd, RB_HALFTHERESOLUTIONOFTHESCREEN, &iCX, &iCY);
  swpWindow.cx = iRBCX + iCX + 10*CXDLGFRAME;

  internal_GetStaticTextSize(hwnd, CB_USEWARPOVERLAYIFPRESENT, &iCX, &iCY);
  i = iCBCX + iCX + 10*CXDLGFRAME;
  if (swpWindow.cx<i) swpWindow.cx = i;

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


  // First set the "Processor usage" groupbox
  gbheight =
    CYDLGFRAME + // frame
    CYDLGFRAME + // Margin
    radioheight + // Checkbox
    CYDLGFRAME + // Margin
    staticheight + CYDLGFRAME + // Entry field
    2*CYDLGFRAME + // Margin
    radioheight + // radio button
    radioheight + // radio button
    CYDLGFRAME + // Margin
    staticheight; // Text of groupbox

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CONFOK), &swpTemp);
  WinSetWindowPos(WinWindowFromID(hwnd, GB_PROCESSORUSAGE),
		  HWND_TOP,
                  2*CXDLGFRAME,
                  swpTemp.y + swpTemp.cy + CYDLGFRAME,
                  swpWindow.cx - 4*CXDLGFRAME,
                  gbheight,
                  SWP_MOVE | SWP_SIZE);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_PROCESSORUSAGE), &swpTemp);

  // Things inside this groupbox
  internal_GetStaticTextSize(hwnd, CB_USEWARPOVERLAYIFPRESENT, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, CB_USEWARPOVERLAYIFPRESENT), HWND_TOP,
                  swpTemp.x + 2*CXDLGFRAME,
                  swpTemp.y + CYDLGFRAME,
                  iCX+iCBCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, CB_USEWARPOVERLAYIFPRESENT), &swpTemp2);
  internal_GetStaticTextSize(hwnd, ST_ANIMATIONSPEED, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_ANIMATIONSPEED), HWND_TOP,
                  swpTemp.x + 2*CXDLGFRAME,
                  swpTemp2.y + swpTemp2.cy + 2*CYDLGFRAME,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ANIMATIONSPEED), &swpTemp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, EF_FPS), &swpTemp3);
  internal_GetStaticTextSize(hwnd, ST_FPS, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_FPS), HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME,
                  swpTemp2.y,
                  iCX,
                  staticheight,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, EF_FPS), &swpTemp3);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_FPS), HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME + swpTemp3.cx + CXDLGFRAME,
                  swpTemp2.y,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ANIMATIONSPEED), &swpTemp2);
  internal_GetStaticTextSize(hwnd, RB_RUNINREGULARPRIORITY, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, RB_RUNINREGULARPRIORITY), HWND_TOP,
                  swpTemp2.x,
                  swpTemp2.y + swpTemp2.cy + 3*CYDLGFRAME,
                  iCX + iRBCX, iRBCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, RB_RUNINREGULARPRIORITY), &swpTemp2);
  internal_GetStaticTextSize(hwnd, RB_RUNINIDLEPRIORITY, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, RB_RUNINIDLEPRIORITY), HWND_TOP,
                  swpTemp2.x,
                  swpTemp2.y + swpTemp2.cy,
                  iCX + iRBCX, iRBCY,
                  SWP_MOVE | SWP_SIZE);

  // The "Image resolution" groupbox
  gbheight =
    CYDLGFRAME + // frame
    CYDLGFRAME + // Margin
    staticheight + CYDLGFRAME + // Combo box
    CYDLGFRAME + // Margin
    radioheight + // radiobutton
    radioheight + // radiobutton
    radioheight + // radiobutton
    CYDLGFRAME + // Margin
    staticheight; // Text of groupbox

  WinQueryWindowPos(WinWindowFromID(hwnd, GB_PROCESSORUSAGE), &swpTemp);
  WinSetWindowPos(WinWindowFromID(hwnd, GB_IMAGERESOLUTION),
		  HWND_TOP,
                  2*CXDLGFRAME,
                  swpTemp.y + swpTemp.cy + CYDLGFRAME,
                  swpWindow.cx - 4*CXDLGFRAME,
                  gbheight,
                  SWP_MOVE | SWP_SIZE);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_IMAGERESOLUTION), &swpTemp);

  internal_GetStaticTextSize(hwnd, ST_RESOLUTION, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_RESOLUTION), HWND_TOP,
                  swpTemp.x + 6*CXDLGFRAME,
                  swpTemp.y + 2*CYDLGFRAME,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_RESOLUTION), &swpTemp2);
  internal_GetStaticTextSize(hwnd, RB_CUSTOMRESOLUTION, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, CX_RESOLUTION), HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME,
                  swpTemp2.y - 6*staticheight,
                  iCX,
                  staticheight*7+CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);

  internal_GetStaticTextSize(hwnd, RB_CUSTOMRESOLUTION, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, RB_CUSTOMRESOLUTION), HWND_TOP,
                  swpTemp.x + 2*CXDLGFRAME,
                  swpTemp2.y + swpTemp2.cy + CYDLGFRAME,
                  iCX + iRBCX, iRBCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, RB_CUSTOMRESOLUTION), &swpTemp2);
  internal_GetStaticTextSize(hwnd, RB_HALFTHERESOLUTIONOFTHESCREEN, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, RB_HALFTHERESOLUTIONOFTHESCREEN), HWND_TOP,
                  swpTemp2.x,
                  swpTemp2.y + swpTemp2.cy,
                  iCX + iRBCX, iRBCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, RB_HALFTHERESOLUTIONOFTHESCREEN), &swpTemp2);
  internal_GetStaticTextSize(hwnd, RB_RESOLUTIONOFTHESCREEN, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, RB_RESOLUTIONOFTHESCREEN), HWND_TOP,
                  swpTemp2.x,
                  swpTemp2.y + swpTemp2.cy,
                  iCX + iRBCX, iRBCY,
                  SWP_MOVE | SWP_SIZE);

  // The "Visual Options" groupbox
  gbheight =
    CYDLGFRAME + // frame
    radioheight + // Checkbox
    CYDLGFRAME + // Margin
    radioheight + // Checkbox
    CYDLGFRAME + // Margin
    staticheight; // Text of groupbox

  WinQueryWindowPos(WinWindowFromID(hwnd, GB_IMAGERESOLUTION), &swpTemp);
  WinSetWindowPos(WinWindowFromID(hwnd, GB_VISUALOPTIONS),
                  HWND_TOP,
                  2*CXDLGFRAME,
                  swpTemp.y + swpTemp.cy + CYDLGFRAME,
                  swpWindow.cx - 4*CXDLGFRAME,
                  gbheight,
                  SWP_MOVE | SWP_SIZE);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_VISUALOPTIONS), &swpTemp);

  // Things inside this groupbox
  internal_GetStaticTextSize(hwnd, CB_USERANDOMANIMATIONS, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, CB_USERANDOMANIMATIONS), HWND_TOP,
                  swpTemp.x + 2*CXDLGFRAME,
                  swpTemp.y + CYDLGFRAME,
                  iCX+iCBCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, CB_USERANDOMANIMATIONS), &swpTemp2);
  internal_GetStaticTextSize(hwnd, CB_USERANDOMOBJECTS, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, CB_USERANDOMOBJECTS), HWND_TOP,
                  swpTemp2.x,
                  swpTemp2.y + swpTemp2.cy + CYDLGFRAME,
                  iCX+iCBCX, iCBCY,
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
  int i;
  int iToSelect;
  char achTemp[128];

  if (!pCfgDlgInit) return;

  WinSendDlgItemMsg(hwnd, RB_RESOLUTIONOFTHESCREEN,
		    BM_SETCHECK,
		    (MRESULT) (pCfgDlgInit->ModuleConfig.iResType == MODULE_RESTYPE_SCREEN),
		    (MRESULT) 0);
  WinSendDlgItemMsg(hwnd, RB_HALFTHERESOLUTIONOFTHESCREEN,
		    BM_SETCHECK,
		    (MRESULT) (pCfgDlgInit->ModuleConfig.iResType == MODULE_RESTYPE_HALFSCREEN),
		    (MRESULT) 0);
  WinSendDlgItemMsg(hwnd, RB_CUSTOMRESOLUTION,
		    BM_SETCHECK,
		    (MRESULT) (pCfgDlgInit->ModuleConfig.iResType == MODULE_RESTYPE_CUSTOM),
                    (MRESULT) 0);

  WinEnableWindow(WinWindowFromID(hwnd, CX_RESOLUTION), (pCfgDlgInit->ModuleConfig.iResType == MODULE_RESTYPE_CUSTOM));

  WinSendDlgItemMsg(hwnd, CX_RESOLUTION, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);
  iToSelect = 0;
  for (i=0; i<NUM_OF_CUSTOM_RESOLUTIONS; i++)
  {
    sprintf(achTemp, "%dx%d", aiCustomResWidth[i], aiCustomResHeight[i]);
    WinSendDlgItemMsg(hwnd, CX_RESOLUTION, LM_INSERTITEM, (MPARAM) LIT_END, (MPARAM) achTemp);
    if ((aiCustomResWidth[i] == pCfgDlgInit->ModuleConfig.iResCustomXSize) &&
	(aiCustomResHeight[i] == pCfgDlgInit->ModuleConfig.iResCustomYSize))
      iToSelect = i;
  }
  WinSendDlgItemMsg(hwnd, CX_RESOLUTION, LM_SELECTITEM, (MPARAM) iToSelect, (MPARAM) TRUE);

  WinSendDlgItemMsg(hwnd, RB_RUNINIDLEPRIORITY,
		    BM_SETCHECK,
		    (MRESULT) (pCfgDlgInit->ModuleConfig.bIdleSaver == TRUE),
		    (MRESULT) 0);

  WinSendDlgItemMsg(hwnd, RB_RUNINREGULARPRIORITY,
		    BM_SETCHECK,
		    (MRESULT) (pCfgDlgInit->ModuleConfig.bIdleSaver == FALSE),
                    (MRESULT) 0);

  WinSendDlgItemMsg(hwnd, CB_USEWARPOVERLAYIFPRESENT,
		    BM_SETCHECK,
		    (MRESULT) (pCfgDlgInit->ModuleConfig.bUseWO == TRUE),
		    (MRESULT) 0);

  sprintf(achTemp, "%d", pCfgDlgInit->ModuleConfig.iFPS);
  WinSetDlgItemText(hwnd, EF_FPS, achTemp);

  WinSendDlgItemMsg(hwnd, CB_USERANDOMOBJECTS,
		    BM_SETCHECK,
		    (MRESULT) (pCfgDlgInit->ModuleConfig.bUseObjects == TRUE),
		    (MRESULT) 0);
  WinSendDlgItemMsg(hwnd, CB_USERANDOMANIMATIONS,
		    BM_SETCHECK,
		    (MRESULT) (pCfgDlgInit->ModuleConfig.bUseAnims == TRUE),
		    (MRESULT) 0);

}

MRESULT EXPENTRY fnConfigDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  ConfigDlgInitParams_p pCfgDlgInit;
  HWND hwndOwner;
  SWP swpDlg, swpOwner;
  POINTL ptlTemp;
  int iTemp;

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

      // Set window focus to the correct radio button, or otherwise the first control
      // gets the focus (the first radio button), so it will always be selected!

      switch (pCfgDlgInit->ModuleConfig.iResType)
      {
        case MODULE_RESTYPE_CUSTOM:
	  iTemp = RB_CUSTOMRESOLUTION;
	  break;
        case MODULE_RESTYPE_HALFSCREEN:
	  iTemp = RB_HALFTHERESOLUTIONOFTHESCREEN;
	  break;
	case MODULE_RESTYPE_SCREEN:
	default:
	  iTemp = RB_RESOLUTIONOFTHESCREEN;
	  break;
      }
      WinSetFocus(HWND_DESKTOP, WinWindowFromID(hwnd, iTemp));

      return (MRESULT) TRUE; // TRUE, to note that the focus has been changed

    case WM_CONTROL:
      pCfgDlgInit = (ConfigDlgInitParams_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pCfgDlgInit)
      {
        // Fine, we have out private data! Let's see the message then!
	switch (SHORT1FROMMP(mp1))
        {
          case CB_USEWARPOVERLAYIFPRESENT:
	    if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	    {
	      // Selected this radio button!
	      pCfgDlgInit->ModuleConfig.bUseWO = WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));
	      return (MRESULT) 0;
	    }
	    break;

          case CB_USERANDOMOBJECTS:
	    if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	    {
	      // Selected this radio button!
	      pCfgDlgInit->ModuleConfig.bUseObjects = WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));
	      return (MRESULT) 0;
	    }
	    break;

          case CB_USERANDOMANIMATIONS:
	    if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	    {
	      // Selected this radio button!
	      pCfgDlgInit->ModuleConfig.bUseAnims = WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));
	      return (MRESULT) 0;
	    }
	    break;

	  case RB_RESOLUTIONOFTHESCREEN:
	    if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	    {
	      // Selected this radio button!
	      pCfgDlgInit->ModuleConfig.iResType = MODULE_RESTYPE_SCREEN;
	      WinEnableWindow(WinWindowFromID(hwnd, CX_RESOLUTION), FALSE);

	      return (MRESULT) 0;
	    }
	    break;
	  case RB_HALFTHERESOLUTIONOFTHESCREEN:
	    if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	    {
	      // Selected this radio button!
	      pCfgDlgInit->ModuleConfig.iResType = MODULE_RESTYPE_HALFSCREEN;
	      WinEnableWindow(WinWindowFromID(hwnd, CX_RESOLUTION), FALSE);

	      return (MRESULT) 0;
	    }
	    break;
	  case RB_CUSTOMRESOLUTION:
	    if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	    {
	      // Selected this radio button!
	      pCfgDlgInit->ModuleConfig.iResType = MODULE_RESTYPE_CUSTOM;
	      WinEnableWindow(WinWindowFromID(hwnd, CX_RESOLUTION), TRUE);

	      return (MRESULT) 0;
	    }
	    break;

	  case RB_RUNINIDLEPRIORITY:
	    if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	    {
	      // Selected this radio button!
	      pCfgDlgInit->ModuleConfig.bIdleSaver = TRUE;
	      return (MRESULT) 0;
	    }
	    break;

	  case RB_RUNINREGULARPRIORITY:
	    if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	    {
	      // Selected this radio button!
	      pCfgDlgInit->ModuleConfig.bIdleSaver = FALSE;
	      return (MRESULT) 0;
	    }
	    break;
	  case EF_FPS:
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
		// Invalid FPS value, restore old!
		WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achUndoBuf);
	      } else
	      {
		// Valid value!
		pCfgDlgInit->ModuleConfig.iFPS = sTemp;
	      }
              return (MRESULT) 0;
	    }
	    break;
	  case CX_RESOLUTION:
	    if (SHORT2FROMMP(mp1)==LN_SELECT)
	    {
	      long lIndex;

	      lIndex = (long) WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), LM_QUERYSELECTION, (MPARAM) 0, (MPARAM) 0);

	      if ((lIndex>=0) && (lIndex<NUM_OF_CUSTOM_RESOLUTIONS))
	      {
		pCfgDlgInit->ModuleConfig.iResCustomXSize = aiCustomResWidth[lIndex];
		pCfgDlgInit->ModuleConfig.iResCustomYSize = aiCustomResHeight[lIndex];
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
      // A key has been pressed, restart timer!
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
  POINTL    pointl;                /* Point to offset from Desktop         */
  SWP       swp;                   /* Window position                      */
  HRGN      hrgn;                  /* Region handle                        */
  HPS       hps;                   /* Presentation Space handle            */
  RECTL     rcls[50];              /* Rectangle coordinates                */
  RGNRECT   rgnCtl;                /* Processing control structure         */
  SETUP_BLITTER SetupBlitter;      /* structure for DiveSetupBlitter       */

  SWP swpDlg, swpParent;
  HWND hwndDlg;
  int rc;

  switch( msg )
  {
    case WM_SUBCLASS_INIT:
    case WM_CREATE:

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
        bPaused = TRUE; // This will notify blitter thread that it should stop calculating!
      }
      return (MRESULT) SSMODULE_NOERROR;

    case WM_RESUMESAVING:
      // We should resume screen saving
      if (bPaused)
      {
#ifdef DEBUG_LOGGING
	AddLog("[WM_RESUMESAVING] : Resuming saver module\n");
#endif
        if (bUseFrameRegulation)
        {
          ULONG ulPostCount;
          // Clear BlitEvent semaphore!
          DosResetEventSem(hevBlitEvent, &ulPostCount);
        }

        bPaused = FALSE; // Also note blitter thread that it can continue calculating
      }
      return (MRESULT) SSMODULE_NOERROR;

    case WM_SUBCLASS_UNINIT:
    case WM_DESTROY:
#ifdef DEBUG_LOGGING
      AddLog("[fnSaverWindowProc] : WM_DESTROY!\n");
#endif

      // Restore mouse pointer, if we're in real screen-saving mode!
      if (!bOnlyPreviewMode)
        WinShowPointer(HWND_DESKTOP, TRUE);
      break;

    case WM_VRNDISABLED:
      if (DosRequestMutexSem(hmtxUseDive, SEM_INDEFINITE_WAIT)==NO_ERROR)
      {
	if (bHWVideoPresent)
	{
	  // HWVIDEO
          pfnHWVIDEOSetup(NULL);
	} else
	{
          // DIVE
	  DiveSetupBlitter(hDive , 0);
	}

	DosReleaseMutexSem(hmtxUseDive);
      }
      bVRNEnabled = FALSE;
      return (MRESULT) FALSE;

    case WM_ADJUSTWINDOWPOS: // At resizing/moving, re-setup the blitter!
      if (bVRNEnabled)
        WinPostMsg(hwnd, WM_VRNENABLED, (MPARAM) NULL, (MPARAM) NULL);

      if (!bOnlyPreviewMode)
      {
	SWP *pSWP;

	// The following is required so that this window will be on
        // top of the xCenter window, even if that is set to be always on top!

	// Real screensaving, here we should stay on top!
        // Set WS_TOPMOST flag again!
	WinSetWindowBits(hwnd, QWL_STYLE, WS_TOPMOST, WS_TOPMOST);

	pSWP = (SWP *) mp1;
	pSWP->hwndInsertBehind = HWND_TOP;
        pSWP->fl |= SWP_ZORDER;
      }
      break;

    case WM_VRNENABLED:
      if (DosRequestMutexSem(hmtxUseDive, SEM_INDEFINITE_WAIT)==NO_ERROR)
      {
        hps = WinGetPS(hwnd);
        hrgn = GpiCreateRegion(hps, 0L, NULL);

        WinQueryVisibleRegion(hwnd, hrgn);
        rgnCtl.ircStart     = 0;
        rgnCtl.crc          = 50;
        rgnCtl.ulDirection  = 1;
        // Get the all ORed rectangles
        GpiQueryRegionRects(hps, hrgn, NULL, &rgnCtl, &rcls[0]);

        GpiDestroyRegion(hps, hrgn);
        WinReleasePS(hps);

        // Now find the window position and size, relative to parent.
        WinQueryWindowPos( hwnd , &swp );

        // Convert the point to offset from desktop lower left.
        pointl.x = swp.x;
        pointl.y = swp.y;
        WinMapWindowPoints( WinQueryWindow( hwnd , QW_PARENT ) ,
                            HWND_DESKTOP , &pointl , 1 );

	if (!bHWVideoPresent)
	{
	  // Tell DIVE about the new settings.
	  SetupBlitter.ulStructLen = sizeof( SETUP_BLITTER );
	  SetupBlitter.fccSrcColorFormat = fccBufferFormat;
	  SetupBlitter.ulSrcWidth = iBufferXSize;
	  SetupBlitter.ulSrcHeight =iBufferYSize;
	  SetupBlitter.ulSrcPosX = 0;
	  SetupBlitter.ulSrcPosY = 0;
	  SetupBlitter.fInvert = FALSE;
	  SetupBlitter.ulDitherType = 1;
	  SetupBlitter.fccDstColorFormat = FOURCC_SCRN;
	  SetupBlitter.ulDstWidth = swp.cx;
	  SetupBlitter.ulDstHeight = swp.cy;
	  SetupBlitter.lDstPosX = 0;
	  SetupBlitter.lDstPosY = 0;
	  SetupBlitter.lScreenPosX = pointl.x;
	  SetupBlitter.lScreenPosY = pointl.y;
	  SetupBlitter.ulNumDstRects = rgnCtl.crcReturned;
	  SetupBlitter.pVisDstRects = rcls;
	  DiveSetupBlitter( hDive , &SetupBlitter );
	} else
	{
	  // Tell HWVideo about the new settings
	  HWVIDEOSETUP Setup;

	  // Free old config
	  // (Don't know if needed)
          pfnHWVIDEOSetup(NULL);

          // Prepare new setup
	  Setup.ulLength = sizeof(HWVIDEOSETUP); //structure version checking
	  Setup.szlSrcSize.cx = iBufferXSize;  //source width
	  Setup.szlSrcSize.cy = iBufferYSize; //source height
	  Setup.fccColor = FOURCC_Y422;          //source colorspace
	  Setup.rctlSrcRect.xLeft = 0;
	  Setup.rctlSrcRect.xRight = iBufferXSize-1;
	  Setup.rctlSrcRect.yTop = 0;
	  Setup.rctlSrcRect.yBottom = iBufferYSize-1;
	  // calculate requered HW-dependent scanline aligment
	  Setup.ulSrcPitch=(iBufferXSize*2+OverlayCaps.ulScanAlign)&~OverlayCaps.ulScanAlign;
	  // Determine keying color
	  // We need to separate two cases:
	  // screen in 256 color (indexed) and 15,16,24,32 bpp
	  // if indexed colorspace used, we need to send index as KeyColor
	  Setup.ulKeyColor=ulColorKey;
          Setup.rctlDstRect.xLeft=pointl.x;
	  Setup.rctlDstRect.xRight=pointl.x + swp.cx-1;
	  Setup.rctlDstRect.yTop=pointl.y + swp.cy-1;
	  Setup.rctlDstRect.yBottom=pointl.y;
          pfnHWVIDEOSetup(&Setup);
	}
        bVRNEnabled = TRUE;

        DosReleaseMutexSem(hmtxUseDive);
      }
      return (MRESULT) FALSE;

    case WM_REALIZEPALETTE:
      if (DosRequestMutexSem(hmtxUseDive, SEM_INDEFINITE_WAIT)==NO_ERROR)
      {
	if (!bHWVideoPresent)
	{
	  // This tells DIVE that the physical palette may have changed.
	  DiveSetDestinationPalette( hDive , 0 , 0 , 0 );
	}

        DosReleaseMutexSem(hmtxUseDive);
      }
      break;

    case WM_PAINT:
      {
	HPS    hpsBP;
        RECTL  rc;
        char  *pchPreviewAreaText;

        hpsBP = WinBeginPaint(hwnd, 0L, &rc);
        WinQueryWindowRect(hwnd, &rc);

        if ((iSaverThreadState != STATE_RUNNING) && (!bScreenShotThreadDone))
	{
#ifdef DEBUG_LOGGING
	  AddLog("WM_PAINT: Calculating\n");
#endif

          pchPreviewAreaText = "Calculating...";
          // Draw with White on Black
          WinDrawText(hpsBP,
                      strlen(pchPreviewAreaText), pchPreviewAreaText,
                      &rc,
                      SYSCLR_WINDOW,         // Foreground color
                      SYSCLR_WINDOWTEXT,     // Background color
                      DT_ERASERECT | DT_CENTER | DT_VCENTER);
	} else
	{
	  // If we're in HWVideo mode, then fill window with colorkey!
#ifdef DEBUG_LOGGING
	  AddLog("WM_PAINT: NOT Calculating\n");
#endif

          if (bHWVideoPresent)
	  {
#ifdef DEBUG_LOGGING
	    AddLog("WM_PAINT: Setting colorkey\n");
#endif

            // Switch to RGB mode
            GpiCreateLogColorTable(hpsBP,
                                   0,
                                   LCOLF_RGB,
                                   0, 0, NULL);
            WinFillRect(hpsBP, &rc, ulColorKey);
            // Switch back to original mode
            GpiCreateLogColorTable(hpsBP,
                                   LCOL_RESET,
                                   LCOLF_INDRGB,
                                   0, 0, NULL);
#ifdef DEBUG_LOGGING
//            AddLog("[fnSaverWindowProc] : Filling with colorkey\n");
#endif

          }
	}
	WinEndPaint(hpsBP);
	break;
      }

    case WM_SCREENSHOT_DONE:
#ifdef DEBUG_LOGGING
      AddLog("WM_SCREENSHOT_DONE\n");
#endif

      WinInvalidateRect(hwnd, NULL, FALSE);
      return (MRESULT) TRUE;

    default:
#ifdef DEBUG_LOGGING
      /*
      {
        char achTemp[120];
        sprintf(achTemp, "Unprocessed msg: 0x%x\n", msg);
        AddLog(achTemp);
      }
      */
#endif
      break;
  }

  return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}

static void internal_AddToRGB(int *pR, int *pG, int *pB, unsigned char *pchSrc, FOURCC fccSrcFormat)
{
  int iR, iG, iB;

  // Take red, green and blue components, and scale them to 0..256!
  switch (fccSrcFormat)
  {
    case FOURCC_R565:
      iR = (unsigned char) ((*(pchSrc+1)) & 0xF8);
      iG = (unsigned char) (((*(pchSrc+1)) << 5) | (((*(pchSrc)) & 0xE0) >> 3));
      iB = (unsigned char) ((*(pchSrc)) << 3);
      break;
    case FOURCC_R555:
      iR = (unsigned char) (((*(pchSrc+1)) & 0x7C) << 1);
      iG = (unsigned char) (((*(pchSrc+1)) << 6) | (((*(pchSrc)) & 0xE0) >> 2));
      iB = (unsigned char) ((*(pchSrc)) << 3);
      break;
    case FOURCC_R664:
      iR = (unsigned char) (((*(pchSrc+1)) & 0xFC));
      iG = (unsigned char) (((*(pchSrc+1)) << 6) | (((*(pchSrc)) & 0xF0) >> 2));
      iB = (unsigned char) ((*(pchSrc)) << 4);
      break;
    case FOURCC_RGB4:
    case FOURCC_RGB3:
      iR = (unsigned char) (*(pchSrc));
      iG = (unsigned char) (*(pchSrc+1));
      iB = (unsigned char) (*(pchSrc+2));
      break;
    case FOURCC_BGR4:
    case FOURCC_BGR3:
      iR = (unsigned char) (*(pchSrc+2));
      iG = (unsigned char) (*(pchSrc+1));
      iB = (unsigned char) (*(pchSrc));
      break;
    default:
      break;
  }
  *pR += iR;
  *pG += iG;
  *pB += iB;
  return;
}

static void internal_CreateSrcBufferLineFromVRAM(int iDstY,               // Y Position in Dst
                                                 int iSrcWidth,
                                                 int iSrcHeight,
                                                 int iDstWidth,
                                                 int iDstHeight,
                                                 FOURCC fccSrcFormat,  // Source pixel format
                                                 int    iSrcBytesPerPixel,
                                                 FOURCC fccDstFormat,  // Destination pixel format
                                                 int    iDstBytesPerPixel,
                                                 char *pchSrcBuffer,
                                                 ULONG ulSrcScanLineBytes, // Src buffer and scanline size
                                                 char *pchDstBuffer,
                                                 ULONG ulDstScanLineBytes  // Dest buffer and scanline size
                                                )
{
  int iDstX, iSrcX;
  int R,G,B;
  int Y,U,V;
  int iCount;
  int iLinesToMerge, iNeighboursToMerge;
  int iSrcY;
  int iX, iY;
  unsigned char *pchTemp;

  // Calculate how many lines we'll need from source buffer to create one destination buffer line
  if (iSrcHeight==iDstHeight)
  {
    iLinesToMerge = 1;
    iSrcY = iDstY;
  } else
  if (iSrcHeight<iDstHeight)
  {
    iLinesToMerge = 1;
    iSrcY = iDstY * iSrcHeight / iDstHeight;
  } else
  {
    int iNextSrcY;
    // iSrcHeight > iDstHeight
    iSrcY = iDstY * iSrcHeight / iDstHeight;
    iNextSrcY = (iDstY+1) * iSrcHeight / iDstHeight;
    iLinesToMerge = iNextSrcY - iSrcY;
    if (iSrcY + iLinesToMerge > iSrcHeight)
      iLinesToMerge = iSrcHeight-iSrcY;
  }

  // Calculate starting address of sourc and destination buffer lines
  pchSrcBuffer += ulSrcScanLineBytes*iSrcY;
  pchDstBuffer += ulDstScanLineBytes*iDstY;


  // Now calculate every pixel for destination buffer
  for (iDstX = 0; iDstX<iDstWidth; iDstX++)
  {
    // Get how many neighbours we have to merge in a line
    if (iSrcWidth==iDstWidth)
    {
      iNeighboursToMerge = 1;
      iSrcX = iDstX;
    } else
    if (iSrcWidth<iDstWidth)
    {
      iNeighboursToMerge = 1;
      iSrcX = iDstX * iSrcWidth / iDstWidth;
    } else
    {
      int iNextSrcX;
      // iSrcWidth > iDstWidth
      iSrcX = iDstX * iSrcWidth / iDstWidth;
      iNextSrcX = (iDstX+1) * iSrcWidth / iDstWidth;
      iNeighboursToMerge = iNextSrcX - iSrcX;
      if (iSrcX + iNeighboursToMerge > iSrcWidth)
        iNeighboursToMerge = iSrcWidth-iSrcX;
    }

    // Ookay, now make one pixel from a iNeighboursToMerge*iLinesToMerge pixels!
    iCount = 0;
    R = 0; G = 0; B = 0;
    for (iY=0; iY<iLinesToMerge; iY++)
    {
      pchTemp = (unsigned char *) pchSrcBuffer + iY*ulSrcScanLineBytes + iSrcX * iSrcBytesPerPixel;
      for (iX=0; iX<iNeighboursToMerge; iX++)
      {
        iCount++;
        internal_AddToRGB(&R, &G, &B, pchTemp, fccSrcFormat);
        pchTemp += iSrcBytesPerPixel; // Go for next pixel in source buffer!
      }
    }

    if (iCount>1)
    {
      R/=iCount;
      G/=iCount;
      B/=iCount;
    }

    // Make screenshot pixel dim/darker
    R/=4;
    G/=4;
    B/=4;

    // Store pixels!
    // If the destination format is YUV422, then convert to YUV,
    // otherwise assume BGR3 format!
    if (fccDstFormat == FOURCC_Y422)
    {
      // Convert to YUV422

      Y  =  ((RY*R + GY*G + BY*B)>>RGB2YUV_SHIFT) + 16;
      V  =  ((RV*R + GV*G + BV*B)>>RGB2YUV_SHIFT) + 128;
      U  =  ((RU*R + GU*G + BU*B)>>RGB2YUV_SHIFT) + 128;

      // Store pixel (Y U Y V Y U Y V...)

      if (iDstX%2==0)
      {
	// YU
	*(pchDstBuffer++) = Y;
        *(pchDstBuffer++) = U;
      } else
      {
        // YV
	*(pchDstBuffer++) = Y;
        *(pchDstBuffer++) = V;
      }
    } else
    {
      // Assuming that destination format is BGR3!
      *(pchDstBuffer++) = B;
      *(pchDstBuffer++) = G;
      *(pchDstBuffer++) = R;
    }
  }
}


int SetupGraphics()
{
  int rc;

  // Calculate how big buffer we want!
  switch (ModuleConfig.iResType)
  {
    default:
    case MODULE_RESTYPE_SCREEN:
      iBufferXSize = (int) WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
      iBufferYSize = (int) WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
      break;
    case MODULE_RESTYPE_HALFSCREEN:
      iBufferXSize = (int) WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN) / 2;
      iBufferYSize = (int) WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN) / 2;
      break;
    case MODULE_RESTYPE_CUSTOM:
      iBufferXSize = ModuleConfig.iResCustomXSize;
      iBufferYSize = ModuleConfig.iResCustomYSize;
      break;
  }

  // Let's see if HWVideo is present!
  bHWVideoPresent = 0;
  if (ModuleConfig.bUseWO)
  {
    rc = DosLoadModule(NULL, 0, "HWVIDEO", &hmodHWVideo);
    if (rc==NO_ERROR)
    {
      // Seems like HWVIDEO is present!
      // Let's query its functions!
      rc = NO_ERROR;
      rc |= DosQueryProcAddr(hmodHWVideo, 0,
                             "HWVIDEOInit",
                             (PFN *) &pfnHWVIDEOInit);
      rc |= DosQueryProcAddr(hmodHWVideo, 0,
                             "HWVIDEOCaps",
                             (PFN *) &pfnHWVIDEOCaps);
      rc |= DosQueryProcAddr(hmodHWVideo, 0,
                             "HWVIDEOSetup",
                             (PFN *) &pfnHWVIDEOSetup);
      rc |= DosQueryProcAddr(hmodHWVideo, 0,
                             "HWVIDEOBeginUpdate",
                             (PFN *) &pfnHWVIDEOBeginUpdate);
      rc |= DosQueryProcAddr(hmodHWVideo, 0,
                             "HWVIDEOEndUpdate",
                             (PFN *) &pfnHWVIDEOEndUpdate);
      rc |= DosQueryProcAddr(hmodHWVideo, 0,
                             "HWVIDEOGetAttrib",
                             (PFN *) &pfnHWVIDEOGetAttrib);
      rc |= DosQueryProcAddr(hmodHWVideo, 0,
                             "HWVIDEOSetAttrib",
                             (PFN *) &pfnHWVIDEOSetAttrib);
      rc |= DosQueryProcAddr(hmodHWVideo, 0,
                             "HWVIDEOClose",
                             (PFN *) &pfnHWVIDEOClose);
  
      if (!pfnHWVIDEOInit ||
          !pfnHWVIDEOCaps ||
          !pfnHWVIDEOSetup ||
          !pfnHWVIDEOBeginUpdate ||
          !pfnHWVIDEOEndUpdate ||
          !pfnHWVIDEOGetAttrib ||
          !pfnHWVIDEOSetAttrib ||
          !pfnHWVIDEOClose ||
          rc)
      {
        // Error in HWVideo loading!
        DosFreeModule(hmodHWVideo);
      } else
      {
        // All right, HWVideo is present!
        // Initialize it!
        if (pfnHWVIDEOInit())
        {
          // Error initializing it! Not present then!
#ifdef DEBUG_LOGGING
          AddLog("[SetupGraphics] : Error in pfnHWVIDEOInit()\n");
#endif
          DosFreeModule(hmodHWVideo);
        } else
        {
          HWVIDEOSETUP Setup;
  
          // We can use it!
#ifdef DEBUG_LOGGING
          AddLog("[SetupGraphics] : Setting up HWVideo\n");
#endif
  
          // Query capabilities
          memset(&OverlayCaps, 0, sizeof(OverlayCaps));
          OverlayCaps.ulLength = sizeof(OverlayCaps);
          // Query number OverlayCaps.ulNumColors
          rc = pfnHWVIDEOCaps(&OverlayCaps);
#ifdef DEBUG_LOGGING
          {
            char achTemp[128];
            sprintf(achTemp, "HWVIDEOCaps rc is %d\n", rc);
            AddLog(achTemp);
          }
#endif
  
          // Allocate enough space for all stuffs
          OverlayCaps.fccColorType = (PULONG) malloc(OverlayCaps.ulNumColors * sizeof(ULONG));
          // Query caps now!
          rc = pfnHWVIDEOCaps(&OverlayCaps);
          free(OverlayCaps.fccColorType);
#ifdef DEBUG_LOGGING
          {
            char achTemp[128];
            sprintf(achTemp, "HWVIDEOCaps rc is %d\n", rc);
            AddLog(achTemp);
          }
#endif
  
  
          // Initialize HWVideo
          Setup.ulLength = sizeof(HWVIDEOSETUP); //structure version checking
          Setup.szlSrcSize.cx = iBufferXSize;  //source width
          Setup.szlSrcSize.cy = iBufferYSize; //source height
          Setup.fccColor = FOURCC_Y422;          //source colorspace
          Setup.rctlSrcRect.xLeft = 0;
          Setup.rctlSrcRect.xRight = iBufferXSize-1;
          Setup.rctlSrcRect.yTop = 0;
          Setup.rctlSrcRect.yBottom = iBufferYSize-1;
          // calculate requered HW-dependent scanline aligment
          Setup.ulSrcPitch=(iBufferXSize*2+OverlayCaps.ulScanAlign)&~OverlayCaps.ulScanAlign;
          Setup.ulKeyColor=ulColorKey;
          Setup.rctlDstRect.xLeft=0;
          Setup.rctlDstRect.xRight=iBufferXSize-1;
          Setup.rctlDstRect.yTop=iBufferYSize-1;
          Setup.rctlDstRect.yBottom=0;
          rc = pfnHWVIDEOSetup(&Setup);
  
#ifdef DEBUG_LOGGING
          {
            char achTemp[128];
            sprintf(achTemp, "HWVIDEOSetup rc is %d\n", rc);
            AddLog(achTemp);
          }
#endif
  
          // And note that HWVideo is present and we're gonna use it!
          bHWVideoPresent = 1;
        }
      }
    }
  }

#ifdef DEBUG_LOGGING
  if (bHWVideoPresent)
    AddLog("Using HWVideo!\n");
  else
    AddLog("Using DIVE!\n");
#endif


  // Get the screen capabilities, and if the system support only 16 colors
  // the program should be terminated.
  memset(&DiveCaps, 0, sizeof(DIVE_CAPS));

  DiveCaps.pFormatData = NULL;
  DiveCaps.ulFormatLength = 0;
  DiveCaps.ulStructLen = sizeof(DIVE_CAPS);

  rc = DiveQueryCaps(&DiveCaps, DIVE_BUFFER_SCREEN);
  if ((rc != DIVE_SUCCESS) &&
      (rc != DIVE_ERR_INSUFFICIENT_LENGTH))
  {
    // Could not query DIVE capabilities!
#ifdef DEBUG_LOGGING
    AddLog("Could not query DIVE capabilities!\n");
#endif

    if (bHWVideoPresent)
    {
      // Uninitialize HWVideo
#ifdef DEBUG_LOGGING
      AddLog("[SetupGraphics] : Uninitializing HWVideo (Error-1)\n");
#endif
      pfnHWVIDEOClose();
      DosFreeModule(hmodHWVideo);
      bHWVideoPresent = 0;
    }
    return 0;
  }

#ifdef DEBUG_LOGGING
  {
    char achFourCC[5];
    memcpy(&(achFourCC[0]), &(DiveCaps.fccColorEncoding), 4);
    achFourCC[4] = 0;
    AddLog("Desktop FourCC: [");
    AddLog(achFourCC);
    AddLog("]\n");
  }
#endif

  if (DiveCaps.ulDepth <= 8)
  {
    // Eeeek, screen color depth is less than or equals to 8 bits!
#ifdef DEBUG_LOGGING
    AddLog("Eeeek, screen color depth is less than or equals to 8 bits!\n");
#endif
    if (bHWVideoPresent)
    {
      // Uninitialize HWVideo
#ifdef DEBUG_LOGGING
      AddLog("[SetupGraphics] : Uninitializing HWVideo (Error-2)\n");
#endif

      pfnHWVIDEOClose();
      DosFreeModule(hmodHWVideo);
      bHWVideoPresent = 0;
    }
    return 0;
  }

  if ((!DiveCaps.fScreenDirect) || (DiveCaps.fBankSwitched))
  {
    // No direct access to linear VRAM!
#ifdef DEBUG_LOGGING
    AddLog("No direct access to linear VRAM!\n");
#endif
    if (bHWVideoPresent)
    {
      // Uninitialize HWVideo
#ifdef DEBUG_LOGGING
      AddLog("[SetupGraphics] : Uninitializing HWVideo (Error-3)\n");
#endif

      pfnHWVIDEOClose();
      DosFreeModule(hmodHWVideo);
      bHWVideoPresent = 0;
    }
    return 0;
  }

  // Open DIVE
  if (DiveOpen(&hDive, FALSE, &pchVRAM))
  {
    // Error opening DIVE!
#ifdef DEBUG_LOGGING
    AddLog("Error opening DIVE!\n");
#endif
    if (bHWVideoPresent)
    {
      // Uninitialize HWVideo
#ifdef DEBUG_LOGGING
      AddLog("[SetupGraphics] : Uninitializing HWVideo (Error-4)\n");
#endif

      pfnHWVIDEOClose();
      DosFreeModule(hmodHWVideo);
      bHWVideoPresent = 0;
    }
    return 0;
  }

  // Setup variables
  bVRNEnabled = FALSE;

  // Ok, DIVE opened.

  // now allocate image buffer
  // but only if we'll use Dive for blitting!
  if (!bHWVideoPresent)
  {
    fccBufferFormat = mmioFOURCC('B','G','R','3');
    iBufferBPP = 24;
    if (DiveAllocImageBuffer(hDive, &ulWorkImage, fccBufferFormat, iBufferXSize, iBufferYSize, 0, 0))
    {
      // Error allocating DIVE image buffer!
#ifdef DEBUG_LOGGING
      AddLog("Error allocating DIVE image buffer!\n");
#endif

      DiveClose(hDive);

      if (bHWVideoPresent)
      {
        // Uninitialize HWVideo
#ifdef DEBUG_LOGGING
      AddLog("[SetupGraphics] : Uninitializing HWVideo (Error-5)\n");
#endif

        pfnHWVIDEOClose();
        DosFreeModule(hmodHWVideo);
        bHWVideoPresent = 0;
      }
      return 0;
    }

    // Also, the screenshot buffer will use this fccBufferFormat!
  } else
  {
    // We'll use HWVideo, so out buffer format will be YUV422

    fccBufferFormat = FOURCC_Y422;
    iBufferBPP = 16;
  }

  // Now allocate space for the screen-shot in BGR3 or YUV422 format, based
  // on the current fccBufferFormat and iBufferBPP setting!
  pchScreenShot = (char *) malloc(iBufferXSize * iBufferYSize * (iBufferBPP+7)/8);
  if (!pchScreenShot)
  {
    // Error allocating image buffer!
#ifdef DEBUG_LOGGING
    AddLog("Error allocating ScreenShot image buffer!\n");
#endif

    if (!bHWVideoPresent)
      DiveFreeImageBuffer(hDive, ulWorkImage);

    DiveClose(hDive);

    if (bHWVideoPresent)
    {
      // Uninitialize HWVideo
#ifdef DEBUG_LOGGING
      AddLog("[SetupGraphics] : Uninitializing HWVideo (Error-6)\n");
#endif

      pfnHWVIDEOClose();
      DosFreeModule(hmodHWVideo);
      bHWVideoPresent = 0;
    }
    return 0;
  }

  // Gooood, dive opened, buffer(s) allocated!

  if (DosCreateMutexSem(NULL, &hmtxUseDive, 0, FALSE)!=NO_ERROR)
  {
    // Yikes, could not create mutex semaphore!
    free(pchScreenShot); pchScreenShot = NULL;
    if (!bHWVideoPresent)
      DiveFreeImageBuffer(hDive, ulWorkImage);
    DiveClose(hDive);

    if (bHWVideoPresent)
    {
#ifdef DEBUG_LOGGING
      AddLog("[SetupGraphics] : Uninitializing HWVideo (Error-7)\n");
#endif
      // Uninitialize HWVideo
      pfnHWVIDEOClose();
      DosFreeModule(hmodHWVideo);
      bHWVideoPresent = 0;
    }
    return 0;
  }

  // Done!
  return 1;
}

void ShutdownGraphics()
{
  DosCloseMutexSem(hmtxUseDive);
  free(pchScreenShot); pchScreenShot = NULL;
  if (!bHWVideoPresent)
    DiveFreeImageBuffer(hDive, ulWorkImage);
  DiveClose(hDive);

  if (bHWVideoPresent)
  {
#ifdef DEBUG_LOGGING
    AddLog("[ShutdownGraphics] : Uninitializing HWVideo\n");
#endif

    // Uninitialize HWVideo
    pfnHWVIDEOSetup(NULL); // Free internal buffers and other stuffs
    pfnHWVIDEOClose();
    DosFreeModule(hmodHWVideo);
    bHWVideoPresent = 0;
  }
}

void fnScreenShotThread(void *p)
{
  char     *pchCopyOfScreen;
  char     *pchBuffer;
  ULONG     ulBufferScanLineBytes;
  ULONG     ulBufferScanLines;
  RECTL     rclTemp;
  

#ifdef DEBUG_LOGGING
  AddLog("[fnScreenShotThread] : Enter\n");
#endif

  // Now make a snapshot of the screen into our buffer
  ulBufferScanLineBytes = iBufferXSize * (iBufferBPP+7)/8; // BGR3 format or YUV422 format
  ulBufferScanLines = iBufferYSize;
  pchBuffer = pchScreenShot;

  memset(&rclTemp, 0, sizeof(rclTemp));

#ifdef DEBUG_LOGGING
  AddLog("[fnScreenShotThread] : DiveAcquireFrameBuffer()\n");
#endif

  if (DiveAcquireFrameBuffer(hDive, &rclTemp) == DIVE_SUCCESS)
  {
    int iY;

#ifdef DEBUG_LOGGING
    AddLog("[fnScreenShotThread] : Creating quick copy of VRAM\n");
#endif
    // Make a quick copy of VRAM, keep video pixel format!
    pchCopyOfScreen = (char *) malloc(DiveCaps.ulScanLineBytes * DiveCaps.ulVerticalResolution);
    if (pchCopyOfScreen)
    {
      memcpy(pchCopyOfScreen, pchVRAM, DiveCaps.ulScanLineBytes * DiveCaps.ulVerticalResolution);
      bScreenCaptured = 1; // Note that the screen image has been copied to local space

#ifdef DEBUG_LOGGING
      AddLog("[fnScreenShotThread] : Releasing frame buffer\n");
#endif

      // Don't lock the screen anymore!
      DiveDeacquireFrameBuffer(hDive);
    } else
    {
#ifdef DEBUG_LOGGING
      AddLog("[fnScreenShotThread] : Could not create copy, will use original VRAM\n");
#endif

      // Could not allocate memory for copy, use original vram!
      pchCopyOfScreen = pchVRAM;
    }

#ifdef DEBUG_LOGGING
    AddLog("[fnScreenShotThread] : Converting image line-by-line\n");
#endif

    // Conversion and other stuffs, line-by-line
    for (iY = 0; iY<ulBufferScanLines; iY++)
    {
      internal_CreateSrcBufferLineFromVRAM(iY,                              // Y Position in Dst buffer
                                           DiveCaps.ulHorizontalResolution, // Source buffer params
                                           DiveCaps.ulVerticalResolution,
                                           iBufferXSize,                    // Dest buffer params
                                           iBufferYSize,
                                           DiveCaps.fccColorEncoding,  // Source pixel format
                                           (DiveCaps.ulDepth+7)/8,
                                           fccBufferFormat,            // Destination pixel format
                                           (iBufferBPP+7)/8,
                                           pchCopyOfScreen, DiveCaps.ulScanLineBytes,   // Src buffer and scanline size
                                           pchBuffer, ulBufferScanLineBytes // Dst buffer and scanline size
                                          );
    }

#ifdef DEBUG_LOGGING
    AddLog("[fnScreenShotThread] : Conversion done.\n");
#endif

    // Conversion done!
    if (pchCopyOfScreen==pchVRAM)
    {
#ifdef DEBUG_LOGGING
      AddLog("[fnScreenShotThread] : Releasing frame buffer\n");
#endif
      DiveDeacquireFrameBuffer(hDive);
      bScreenCaptured = 1;
    } else
    {
#ifdef DEBUG_LOGGING
      AddLog("[fnScreenShotThread] : Freeing copy of VRAM\n");
#endif
      free(pchCopyOfScreen);
    }
  }

#ifdef DEBUG_LOGGING
  AddLog("[fnScreenShotThread] : Setting state variables\n");
#endif

  bScreenCaptured = 1;
  bScreenShotThreadDone = 1;

#ifdef DEBUG_LOGGING
  AddLog("[fnScreenShotThread] : Posting WM_SCREENSHOT_DONE\n");
#endif

  WinPostMsg(hwndSaverWindow, WM_SCREENSHOT_DONE, (MPARAM) 0, (MPARAM) 0);

  // This seem to be needed because there are some cases where
  // this thread finishes too fast, and the next thread will get
  // the same TID, and a DosWaitThread() will wait on that thread,
  // even though this thread is already finished.
#ifdef DEBUG_LOGGING
  AddLog("[fnScreenShotThread] : Waiting for shutdown.\n");
#endif
  while (!bShutdownRequest)
    DosSleep(256);

#ifdef DEBUG_LOGGING
  AddLog("[fnScreenShotThread] : _endthread()\n");
#endif

  _endthread();
}


// Now the objects and animations and their description
#include "Snow-Images.h"

#define OBJECT_HOUSE         0
#define OBJECT_GREENFUNNEL   1
#define OBJECT_BLUESLIDER    2
#define OBJECT_GREYFUNNEL    3
#define OBJECT_NUM           4

typedef struct ObjDesc_s
{
  // Object source
  int iPicX;
  int iPicY;
  int iMaskX;
  int iMaskY;
  int iWidth;
  int iHeight;
} ObjDesc_t, *ObjDesc_p;

typedef struct ObjState_s
{
  // Object state
  int bVisible;
  int iX;
  int iY;
} ObjState_t, *ObjState_s;

// The objects on the image:
ObjDesc_t  Objects[OBJECT_NUM] =
{
  // House
  {0, 0,     // PicX, PicY
   0, 60,    // MaskX, MaskY
   46, 59},  // Width, Height

  // GreenFunnel
  {49, 0,    // PicX, PicY
   49, 40,   // MaskX, MaskY
   50, 40},  // Width, Height

  // BlueSlider
  {103, 0,   // PicX, PicY
   103, 40,  // MaskX, MaskY
   37, 40},  // Width, Height

  // GrayFunnel
  {141, 0,   // PicX, PicY
   141, 50,  // MaskX, MaskY
   37, 50}   // Width, Height
};

ObjState_t ObjectStates[OBJECT_NUM];

// The animations

#define ANIMATION_REDCAR     0
#define ANIMATION_NUM        1

typedef struct AnimDesc_s
{
  // Animation source
  int iPicX;
  int iPicY;
  int iMaskX;
  int iMaskY;
  int iWidth;
  int iHeight;
  int iNumFrames;
  int iFramesPer100Move;
} AnimDesc_t, *AnimDesc_p;

typedef struct AnimState_s
{
  // Animation state
  int bVisible;
  int iX;
  int iY;
  int iCurFrame;
  int iFrameIncCounter;
} AnimState_t, *AnimState_s;

AnimDesc_t  Animations[ANIMATION_NUM] =
{
  // Red car
  {0, 0,     // PicX, PixY
   0, 36,    // MaskX, MaskY
   56, 35,   // Width, Height,
   6,        // NumFrames
   40}       // Frames per 100 moves
};

AnimState_t AnimationStates[ANIMATION_NUM];

// General stuffs:
#define POSTYPE_AIR      0
#define POSTYPE_SNOW     1
#define POSTYPE_BLOCKER  2

typedef struct PosDesc_s
{
  int iPosType;
  // For POSTYPE_SNOW:
  int iWeight;
  int iPressureFromAbove;
} PosDesc_t, *PosDesc_p;

PosDesc_p pPosDescBuffer;

#define POSSIBLE_NEWPOS_DOWN      0
#define POSSIBLE_NEWPOS_DOWNLEFT  1
#define POSSIBLE_NEWPOS_DOWNRIGHT 2

static void internal_HideFlake(char *pchScreen, ULONG ulScanLineBytes, char *pchBackground, ULONG ulBgScanLineBytes,
                               int iXPos, int iYPos)
{
  char *pchTemp, *pchBg;

  if ((!pchScreen) || (!pchBackground)) return;

  if (bHWVideoPresent)
  {
    // We're using a YUV422 buffer format
    pchTemp = pchScreen + iYPos * ulScanLineBytes + iXPos*2;
    pchBg = pchBackground + iYPos * ulBgScanLineBytes + iXPos*2;
    pchTemp[0] = pchBg[0];
    pchTemp[1] = pchBg[1];
  } else
  {
    // We're using a DIVE buffer format of BGR3
    pchTemp = pchScreen + iYPos * ulScanLineBytes + iXPos*3;
    pchBg = pchBackground + iYPos * ulBgScanLineBytes + iXPos*3;
    pchTemp[0] = pchBg[0];
    pchTemp[1] = pchBg[1];
    pchTemp[2] = pchBg[2];
  }
}

static void internal_ShowFlake(char *pchScreen, ULONG ulScanLineBytes, int iXPos, int iYPos)
{
  char *pchTemp;

  if (!pchScreen) return;

  if (bHWVideoPresent)
  {
    // We're using a YUV422 buffer format
    pchTemp = pchScreen + iYPos * ulScanLineBytes + iXPos*2;
    pchTemp[0] = 255;
    pchTemp[1] = 128;
    if (iXPos%2)
      pchTemp[-1] = 128;
    else
     pchTemp[3] = 128;

  } else
  {
    // We're using a DIVE buffer format of BGR3
    pchTemp = pchScreen + iYPos * ulScanLineBytes + iXPos*3;
    pchTemp[0] = 255;
    pchTemp[1] = 255;
    pchTemp[2] = 255;
  }
}

static void internal_CreateConvertedImages()
{
  int i, iMax;
  int R, G, B, Y, U, V;
  char *pchSrc;
  char *pchDst;

  if (!bHWVideoPresent)
  {
    // The images are already in BGR3 format, so no need to convert them!

    // Objects1 image:
    pchObjects1ConvertedImageData = &(achObjects1ImageData[0]);
    // Anims1 image:
    pchAnims1ConvertedImageData = &(achAnims1ImageData[0]);
  } else
  {
    // The images are in BGR3 format, convert it to YUV422!

    // Objects1 image:
    pchObjects1ConvertedImageData = (char *) malloc(ulObjects1ImageWidth * ulObjects1ImageHeight * 2);
    // TODO: Check for Out of memory condition!
    pchSrc = &(achObjects1ImageData[0]);
    pchDst = pchObjects1ConvertedImageData;
    iMax = ulObjects1ImageWidth * ulObjects1ImageHeight;
    for (i = 0; i<iMax; i++)
    {
      B = *(pchSrc++);
      G = *(pchSrc++);
      R = *(pchSrc++);

      Y  =  ((RY*R + GY*G + BY*B)>>RGB2YUV_SHIFT) + 16;
      V  =  ((RV*R + GV*G + BV*B)>>RGB2YUV_SHIFT) + 128;
      U  =  ((RU*R + GU*G + BU*B)>>RGB2YUV_SHIFT) + 128;

      // Store pixel (Y U Y V Y U Y V...)
  
      if (i%2==0)
      {
        // YU
        *(pchDst++) = Y;
        *(pchDst++) = U;
      } else
      {
        // YV
        *(pchDst++) = Y;
        *(pchDst++) = V;
      }
    }

    // Anims1 image:
    pchAnims1ConvertedImageData = (char *) malloc(ulAnims1ImageWidth * ulAnims1ImageHeight * 2);
    // TODO: Check for Out of memory condition!
    pchSrc = &(achAnims1ImageData[0]);
    pchDst = pchAnims1ConvertedImageData;
    iMax = ulAnims1ImageWidth * ulAnims1ImageHeight;
    for (i = 0; i<iMax; i++)
    {
      B = *(pchSrc++);
      G = *(pchSrc++);
      R = *(pchSrc++);

      Y  =  ((RY*R + GY*G + BY*B)>>RGB2YUV_SHIFT) + 16;
      V  =  ((RV*R + GV*G + BV*B)>>RGB2YUV_SHIFT) + 128;
      U  =  ((RU*R + GU*G + BU*B)>>RGB2YUV_SHIFT) + 128;

      // Store pixel (Y U Y V Y U Y V...)
  
      if (i%2==0)
      {
        // YU
        *(pchDst++) = Y;
        *(pchDst++) = U;
      } else
      {
        // YV
        *(pchDst++) = Y;
        *(pchDst++) = V;
      }
    }
  }
}

static void internal_DestroyConvertedImages()
{
  if (!bHWVideoPresent)
  {
    // Objects1 image:
    pchObjects1ConvertedImageData = NULL;
    // Anims1 image:
    pchAnims1ConvertedImageData = NULL;
  } else
  {
    // Objects1 image:
    if (pchObjects1ConvertedImageData)
    {
      free(pchObjects1ConvertedImageData);
      pchObjects1ConvertedImageData = NULL;
    }
    // Anims1 image:
    if (pchAnims1ConvertedImageData)
    {
      free(pchAnims1ConvertedImageData);
      pchAnims1ConvertedImageData = NULL;
    }
  }
}

static void internal_AddRemoveObjects(PosDesc_p pPosDesc,
                                      char *pchWork, ULONG ulWorkScanLineBytes,
                                      char *pchScreenShot, ULONG ulSSScanLineBytes)
{
  int i, iTemp;
  int iX, iY;
  unsigned long ulPicScanLineBytes;
  char *pchSrcPic;
  char *pchBgPic;
  char *pchSrcMask;
  char *pchDstPic;
  int   iWidth, iHeight;
  PosDesc_p pPos;

  if (rand() % 1000 == 0)
  {
    // Select randomly which object's state is to be changed
    i = rand() % OBJECT_NUM;

    if (ObjectStates[i].bVisible)
    {
      // It's visible, let's hide it!
      ObjectStates[i].bVisible = FALSE;

      if (!bHWVideoPresent)
      {
        // Do the hide in the DIVE way!

        ulPicScanLineBytes = ulObjects1ImageWidth*3;

        pchSrcPic =
          pchObjects1ConvertedImageData +
          Objects[i].iPicY * ulPicScanLineBytes +
          Objects[i].iPicX*3;

        pchSrcMask =
          pchObjects1ConvertedImageData +
          Objects[i].iMaskY * ulPicScanLineBytes +
          Objects[i].iMaskX*3;

        pchDstPic =
          pchWork +
          ObjectStates[i].iY * ulWorkScanLineBytes +
          ObjectStates[i].iX*3;

        iWidth = Objects[i].iWidth;
        iHeight = Objects[i].iHeight;

        pchBgPic =
          pchScreenShot +
          ObjectStates[i].iY * ulSSScanLineBytes +
          ObjectStates[i].iX*3;

        pPos =
          pPosDesc +
          ObjectStates[i].iY * iBufferXSize +
          ObjectStates[i].iX;

        for (iY = 0; iY<iHeight; iY++)
        {
          for (iX = 0; iX<iWidth; iX++)
          {
            // Copy back the background where we overwrote it
            if ((pchSrcPic[0]) || (pchSrcPic[1]) || (pchSrcPic[2]))
            {
              pchDstPic[0] = pchBgPic[0];
              pchDstPic[1] = pchBgPic[1];
              pchDstPic[2] = pchBgPic[2];
            }

            // Also modify the PosDesc array based on mask!
            if (pchSrcMask[0])
            {
              pPos->iPosType = POSTYPE_AIR;
              pPos->iWeight = 0;
              pPos->iPressureFromAbove =0;
            }

            // Go for next pixel in all pointers!
            pPos++;
            pchBgPic += 3;
            pchDstPic += 3;
            pchSrcPic += 3;
            pchSrcMask += 3;
          }

          // Go for next line!
          pPos += (iBufferXSize - iWidth);
          pchBgPic += ulSSScanLineBytes  - (iWidth*3);
          pchDstPic += ulWorkScanLineBytes - (iWidth*3);
          pchSrcPic += ulPicScanLineBytes - (iWidth*3);
          pchSrcMask += ulPicScanLineBytes - (iWidth*3);
        }
      } else
      {
        // Do the hide in the Overlay way!
        // In overlay, we always re-build the whole image, so we
        // only have to hide the object from the background buffer.

        ulPicScanLineBytes = ulObjects1ImageWidth*2;

        pchSrcMask =
          pchObjects1ConvertedImageData +
          Objects[i].iMaskY * ulPicScanLineBytes +
          Objects[i].iMaskX*2;

        iWidth = Objects[i].iWidth;
        iHeight = Objects[i].iHeight;

        pPos =
          pPosDesc +
          ObjectStates[i].iY * iBufferXSize +
          ObjectStates[i].iX;

        for (iY = 0; iY<iHeight; iY++)
        {
          for (iX = 0; iX<iWidth; iX++)
          {
            // Modify the PosDesc array based on mask!
            if ((pchSrcMask[0] != 16) || (pchSrcMask[1] != 128)) // Not black
            {
              pPos->iPosType = POSTYPE_AIR;
              pPos->iWeight = 0;
              pPos->iPressureFromAbove =0;
            }

            // Go for next pixel in all pointers!
            pPos++;
            pchSrcMask += 2;
          }

          // Go for next line!
          pPos += (iBufferXSize - iWidth);
          pchSrcMask += ulPicScanLineBytes - (iWidth*2);
        }
      }
    } else
    {
      // It's not visible, let's show it!
      ObjectStates[i].bVisible = TRUE;

      // Set random position!
      ObjectStates[i].iX = rand() % (iBufferXSize-Objects[i].iWidth);
      switch (i)
      {
        case OBJECT_HOUSE:
          // The house should appear somewhere on the snow
          iTemp = iBufferXSize * iBufferYSize -1;
          while ((iTemp>=0) && (pPosDesc[iTemp].iPosType == POSTYPE_SNOW))
            iTemp--;

          if (iTemp % iBufferXSize)
            iTemp+=iBufferXSize-1;
          iTemp /= iBufferXSize;
          iTemp -= Objects[i].iHeight;
          if (iTemp<0) iTemp = 0;

          ObjectStates[i].iY = iTemp;
          break;
        default:
          // Most of the objects can appear at the top half of the image
          ObjectStates[i].iY = rand() % (iBufferYSize-Objects[i].iWidth)/2;
          break;
      }

      if (!bHWVideoPresent)
      {
        // Do the showing in the DIVE way!

        ulPicScanLineBytes = ulObjects1ImageWidth*3;

        pchSrcPic =
          pchObjects1ConvertedImageData +
          Objects[i].iPicY * ulPicScanLineBytes +
          Objects[i].iPicX*3;

        pchSrcMask =
          pchObjects1ConvertedImageData +
          Objects[i].iMaskY * ulPicScanLineBytes +
          Objects[i].iMaskX*3;

        pchDstPic =
          pchWork +
          ObjectStates[i].iY * ulWorkScanLineBytes +
          ObjectStates[i].iX*3;

        iWidth = Objects[i].iWidth;
        iHeight = Objects[i].iHeight;

        pPos =
          pPosDesc +
          ObjectStates[i].iY * iBufferXSize +
          ObjectStates[i].iX;

        for (iY = 0; iY<iHeight; iY++)
        {
          for (iX = 0; iX<iWidth; iX++)
          {
            // Copy back the background where we overwrote it
            if ((pchSrcPic[0]) || (pchSrcPic[1]) || (pchSrcPic[2]))
            {
              pchDstPic[0] = pchSrcPic[0];
              pchDstPic[1] = pchSrcPic[1];
              pchDstPic[2] = pchSrcPic[2];
            }

            // Also modify the PosDesc array based on mask!
            if (pchSrcMask[0])
            {
              pPos->iPosType = POSTYPE_BLOCKER;
              pPos->iWeight = 0;
              pPos->iPressureFromAbove =0;
            }

            // Go for next pixel in all pointers!
            pPos++;
            pchDstPic += 3;
            pchSrcPic += 3;
            pchSrcMask += 3;
          }

          // Go for next line!
          pPos += (iBufferXSize - iWidth);
          pchDstPic += ulWorkScanLineBytes - (iWidth*3);
          pchSrcPic += ulPicScanLineBytes - (iWidth*3);
          pchSrcMask += ulPicScanLineBytes - (iWidth*3);
        }
      } else
      {
        // Do the showing in the Overlay way!
        // We always rebuild the whole image for Overlay output,
        // so we only have to modify the backbuffer here.

        ulPicScanLineBytes = ulObjects1ImageWidth*2;

        pchSrcMask =
          pchObjects1ConvertedImageData +
          Objects[i].iMaskY * ulPicScanLineBytes +
          Objects[i].iMaskX*2;

        iWidth = Objects[i].iWidth;
        iHeight = Objects[i].iHeight;

        pPos =
          pPosDesc +
          ObjectStates[i].iY * iBufferXSize +
          ObjectStates[i].iX;

        for (iY = 0; iY<iHeight; iY++)
        {
          for (iX = 0; iX<iWidth; iX++)
          {
            // Modify the PosDesc array based on mask!
            if ((pchSrcMask[0] != 16) || (pchSrcMask[1] != 128)) // Not black
            {
              pPos->iPosType = POSTYPE_BLOCKER;
              pPos->iWeight = 0;
              pPos->iPressureFromAbove =0;
            }

            // Go for next pixel in all pointers!
            pPos++;
            pchSrcMask += 2;
          }

          // Go for next line!
          pPos += (iBufferXSize - iWidth);
          pchSrcMask += ulPicScanLineBytes - (iWidth*2);
        }
      }
    }
  }
}

static void internal_DrawObjects(char *pchWork, ULONG ulWorkScanLineBytes)
{
  int i;
  int iX, iY;
  int iPicX;
  unsigned long ulPicScanLineBytes;
  char *pchSrcPic;
  char *pchDstPic;
  int   iWidth, iHeight;

  for (i=0; i<OBJECT_NUM; i++)
  {
    if (ObjectStates[i].bVisible)
    {
      if (bHWVideoPresent)
      {
        // Do the showing in the Overlay way!

        ulPicScanLineBytes = ulObjects1ImageWidth*2;

        pchSrcPic =
          pchObjects1ConvertedImageData +
          Objects[i].iPicY * ulPicScanLineBytes +
          Objects[i].iPicX*2;

        pchDstPic =
          pchWork +
          ObjectStates[i].iY * ulWorkScanLineBytes +
          ObjectStates[i].iX*2;

        iWidth = Objects[i].iWidth;
        iHeight = Objects[i].iHeight;

        for (iY = 0; iY<iHeight; iY++)
        {
          iPicX = Objects[i].iPicX;
          for (iX = 0; iX<iWidth; iX++)
          {
            // Copy the pixels
            if ((pchSrcPic[0] != 16) || (pchSrcPic[1] != 128)) // Not black
            {
              // Y
              pchDstPic[0] = pchSrcPic[0];

              if ((ObjectStates[i].iX+iX)%2==0)
              {
                // U (!)
                pchDstPic[1] = pchSrcPic[1 + 2*(iPicX%2)];
                // V
                //pchDstPic[3] = pchSrcPic[-1 + 2*(iPicX%2)];
              } else
              {
                // U
                //pchDstPic[-1] = pchSrcPic[1 + 2*(iPicX%2)];
                // V (!)
                pchDstPic[1] = pchSrcPic[-1 + 2*(iPicX%2)];
              }
            }

            // Go for next pixel in all pointers!
            pchDstPic += 2;
            pchSrcPic += 2;
            iPicX++;
          }

          // Go for next line!
          pchDstPic += ulWorkScanLineBytes - (iWidth*2);
          pchSrcPic += ulPicScanLineBytes - (iWidth*2);
        }
      } else
      {
        // Do the showing in the Dive way!
        ulPicScanLineBytes = ulObjects1ImageWidth*3;

        pchSrcPic =
          pchObjects1ConvertedImageData +
          Objects[i].iPicY * ulPicScanLineBytes +
          Objects[i].iPicX*3;

        pchDstPic =
          pchWork +
          ObjectStates[i].iY * ulWorkScanLineBytes +
          ObjectStates[i].iX*3;

        iWidth = Objects[i].iWidth;
        iHeight = Objects[i].iHeight;

        for (iY = 0; iY<iHeight; iY++)
        {
          for (iX = 0; iX<iWidth; iX++)
          {
            // Copy the pixels
            if ((pchSrcPic[0]) || (pchSrcPic[1]) || (pchSrcPic[2]))
            {
              pchDstPic[0] = pchSrcPic[0];
              pchDstPic[1] = pchSrcPic[1];
              pchDstPic[2] = pchSrcPic[2];
            }

            // Go for next pixel in all pointers!
            pchDstPic += 3;
            pchSrcPic += 3;
          }

          // Go for next line!
          pchDstPic += ulWorkScanLineBytes - (iWidth*3);
          pchSrcPic += ulPicScanLineBytes - (iWidth*3);
        }
      }
    }
  }
}

static void internal_HideAnimationPics(char *pchWork, ULONG ulWorkScanLineBytes,
                                       char *pchScreenShot, ULONG ulSSScanLineBytes)
{
  int i;
  int iX, iY;
  unsigned long ulPicScanLineBytes;
  char *pchSrcPic;
  char *pchBgPic;
  char *pchDstPic;
  int   iWidth, iHeight;

  // We have nothing to to for overlays, as
  // the overlay's image is always rebuilt from
  // scratch, so nothing to be removed.

  if (bHWVideoPresent)
    return;

  // For DIVE:

  for (i=0; i<ANIMATION_NUM; i++)
  {
    if (AnimationStates[i].bVisible)
    {
      ulPicScanLineBytes = ulAnims1ImageWidth*3;

      iWidth = Animations[i].iWidth;
      iHeight = Animations[i].iHeight;

      pchSrcPic =
        pchAnims1ConvertedImageData +
        Animations[i].iPicY * ulPicScanLineBytes +
        (Animations[i].iPicX + iWidth*AnimationStates[i].iCurFrame) * 3;

      pchDstPic =
        pchWork +
        AnimationStates[i].iY * ulWorkScanLineBytes +
        AnimationStates[i].iX*3;

      pchBgPic =
        pchScreenShot +
        AnimationStates[i].iY * ulSSScanLineBytes +
        AnimationStates[i].iX*3;

      for (iY = 0; iY<iHeight; iY++)
      {
        for (iX = 0; iX<iWidth; iX++)
        {
          // Copy back the background where we overwrote it
          if ((pchSrcPic[0]) || (pchSrcPic[1]) || (pchSrcPic[2]))
          {
            // Check if we're on the map yet :)
            if ((AnimationStates[i].iY + iY >= 0) &&
                (AnimationStates[i].iY + iY <= iBufferYSize-1) &&
                (AnimationStates[i].iX + iX >= 0) &&
                (AnimationStates[i].iX + iX <= iBufferXSize-1))
            {
              pchDstPic[0] = pchBgPic[0];
              pchDstPic[1] = pchBgPic[1];
              pchDstPic[2] = pchBgPic[2];
            }
          }

          // Go for next pixel in all pointers!
          pchBgPic += 3;
          pchDstPic += 3;
          pchSrcPic += 3;
        }

        // Go for next line!
        pchBgPic += ulSSScanLineBytes  - (iWidth*3);
        pchDstPic += ulWorkScanLineBytes - (iWidth*3);
        pchSrcPic += ulPicScanLineBytes - (iWidth*3);
      }
    }
  }
}


static void internal_DrawAnimationPics(char *pchWork, ULONG ulWorkScanLineBytes)
{
  int i;
  int iX, iY;
  unsigned long ulPicScanLineBytes;
  char *pchSrcPic;
  char *pchDstPic;
  int   iWidth, iHeight;
  int   iPicX;

  if (bHWVideoPresent)
  {
    // For Overlay:
    for (i=0; i<ANIMATION_NUM; i++)
    {
      if (AnimationStates[i].bVisible)
      {
        ulPicScanLineBytes = ulAnims1ImageWidth*2;

        iWidth = Animations[i].iWidth;
        iHeight = Animations[i].iHeight;

        pchSrcPic =
          pchAnims1ConvertedImageData +
          Animations[i].iPicY * ulPicScanLineBytes +
          (Animations[i].iPicX + iWidth*AnimationStates[i].iCurFrame) * 2;

        pchDstPic =
          pchWork +
          AnimationStates[i].iY * ulWorkScanLineBytes +
          AnimationStates[i].iX*2;

        for (iY = 0; iY<iHeight; iY++)
        {
          iPicX = Animations[i].iPicX + iWidth*AnimationStates[i].iCurFrame;
          for (iX = 0; iX<iWidth; iX++)
          {
            // Copy pixels where we have to
            if ((pchSrcPic[0] != 16) || (pchSrcPic[1] != 128)) // Not black
            {
              // Check if we're on the map yet :)
              if ((AnimationStates[i].iY + iY >= 0) &&
                  (AnimationStates[i].iY + iY <= iBufferYSize-1) &&
                  (AnimationStates[i].iX + iX >= 0) &&
                  (AnimationStates[i].iX + iX <= iBufferXSize-1))
              {
                // Y
                pchDstPic[0] = pchSrcPic[0];

                if ((AnimationStates[i].iX+iX)%2==0)
                {
                  // U (!)
                  pchDstPic[1] = pchSrcPic[1 + 2*(iPicX%2)];
                  // V
                  //pchDstPic[3] = pchSrcPic[-1 + 2*(iPicX%2)];
                } else
                {
                  // U
                  //pchDstPic[-1] = pchSrcPic[1 + 2*(iPicX%2)];
                  // V (!)
                  pchDstPic[1] = pchSrcPic[-1 + 2*(iPicX%2)];
                }
              }
            }
  
            // Go for next pixel in all pointers!
            pchDstPic += 2;
            pchSrcPic += 2;
            iPicX++;
          }
  
          // Go for next line!
          pchDstPic += ulWorkScanLineBytes - (iWidth*2);
          pchSrcPic += ulPicScanLineBytes - (iWidth*2);
        }
      }
    }
  } else
  {
    // For DIVE:
    for (i=0; i<ANIMATION_NUM; i++)
    {
      if (AnimationStates[i].bVisible)
      {
        ulPicScanLineBytes = ulAnims1ImageWidth*3;

        iWidth = Animations[i].iWidth;
        iHeight = Animations[i].iHeight;

        pchSrcPic =
          pchAnims1ConvertedImageData +
          Animations[i].iPicY * ulPicScanLineBytes +
          (Animations[i].iPicX + iWidth*AnimationStates[i].iCurFrame) * 3;

        pchDstPic =
          pchWork +
          AnimationStates[i].iY * ulWorkScanLineBytes +
          AnimationStates[i].iX*3;

        for (iY = 0; iY<iHeight; iY++)
        {
          for (iX = 0; iX<iWidth; iX++)
          {
            // Copy pixels where we have to
            if ((pchSrcPic[0]) || (pchSrcPic[1]) || (pchSrcPic[2]))
            {
              // Check if we're on the map yet :)
              if ((AnimationStates[i].iY + iY >= 0) &&
                  (AnimationStates[i].iY + iY <= iBufferYSize-1) &&
                  (AnimationStates[i].iX + iX >= 0) &&
                  (AnimationStates[i].iX + iX <= iBufferXSize-1))
              {
                pchDstPic[0] = pchSrcPic[0];
                pchDstPic[1] = pchSrcPic[1];
                pchDstPic[2] = pchSrcPic[2];
              }
            }
  
            // Go for next pixel in all pointers!
            pchDstPic += 3;
            pchSrcPic += 3;
          }
  
          // Go for next line!
          pchDstPic += ulWorkScanLineBytes - (iWidth*3);
          pchSrcPic += ulPicScanLineBytes - (iWidth*3);
        }
      }
    }
  }
}

static void internal_DrawHideAnimationMask(int iAnimID, PosDesc_p pPosDesc, int bDraw)
{
  int iX, iY;
  unsigned long ulPicScanLineBytes;
  char *pchSrcMask;
  int   iWidth, iHeight;
  PosDesc_p pPos;
  int iType;

  // For this (the mask) we always use the non-converted image, so
  // we don't have to branch for YUV or RGB format, we know that it's
  // always in RGB format.

  if (bDraw)
    iType = POSTYPE_BLOCKER;
  else
    iType = POSTYPE_AIR;

  ulPicScanLineBytes = ulAnims1ImageWidth * 3;

  iWidth = Animations[iAnimID].iWidth;
  iHeight = Animations[iAnimID].iHeight;

  pchSrcMask =
    achAnims1ImageData +
    Animations[iAnimID].iMaskY * ulPicScanLineBytes +
    (Animations[iAnimID].iMaskX + iWidth*AnimationStates[iAnimID].iCurFrame) * 3;

  pPos =
    pPosDesc +
    AnimationStates[iAnimID].iY * iBufferXSize +
    AnimationStates[iAnimID].iX;

  for (iY = 0; iY<iHeight; iY++)
  {
    for (iX = 0; iX<iWidth; iX++)
    {
      // Modify the PosDesc array based on mask!
      if (pchSrcMask[0])
      {
        // Check if we're on the map yet :)
        if ((AnimationStates[iAnimID].iY + iY >= 0) &&
            (AnimationStates[iAnimID].iY + iY <= iBufferYSize-1) &&
            (AnimationStates[iAnimID].iX + iX >= 0) &&
            (AnimationStates[iAnimID].iX + iX <= iBufferXSize-1))
        {
          pPos->iPosType = iType;
          pPos->iWeight = 0;
          pPos->iPressureFromAbove =0;
        }
      }

      // Go for next pixel in all pointers!
      pPos++;
      pchSrcMask += 3;
    }
    // Go for next line!
    pPos += (iBufferXSize - iWidth);
    pchSrcMask += ulPicScanLineBytes - (iWidth*3);
  }
}

static void internal_AddRemoveAnimations(PosDesc_p pPosDesc,
                                         char *pchWork, ULONG ulWorkScanLineBytes,
                                         char *pchScreenShot, ULONG ulSSScanLineBytes)
{
  int iSnowHeight;
  int i;

  // Check snow height
  i = iBufferXSize * iBufferYSize -1;
  while ((i>=0) && (pPosDesc[i].iPosType == POSTYPE_SNOW))
    i--;
  if (i % iBufferXSize)
    i+=iBufferXSize-1;
  i /= iBufferXSize;
  iSnowHeight = iBufferYSize-1-i;
  if (iSnowHeight<0) iSnowHeight = 0;

  // Animation-1 : RED-CAR

  if ((rand() % (iBufferYSize/2)) < iSnowHeight)
  {
    if (AnimationStates[ANIMATION_REDCAR].bVisible == FALSE)
    {
      // Start a red car to clean snow!
      AnimationStates[ANIMATION_REDCAR].bVisible = TRUE;

      AnimationStates[ANIMATION_REDCAR].iX =
        -(Animations[ANIMATION_REDCAR].iWidth);

      AnimationStates[ANIMATION_REDCAR].iY =
        iBufferYSize - iSnowHeight- (Animations[ANIMATION_REDCAR].iHeight/2);
      if (AnimationStates[ANIMATION_REDCAR].iY>iBufferYSize-Animations[ANIMATION_REDCAR].iHeight)
        AnimationStates[ANIMATION_REDCAR].iY = iBufferYSize-Animations[ANIMATION_REDCAR].iHeight;

      AnimationStates[ANIMATION_REDCAR].iCurFrame = 0;
      AnimationStates[ANIMATION_REDCAR].iFrameIncCounter = 0;

      internal_DrawHideAnimationMask(ANIMATION_REDCAR, pPosDesc, 1);
    }
  }
}

static void internal_MoveAnimations(PosDesc_p pPosDesc)
{

  // Animation-1 : RED-CAR

  if (AnimationStates[ANIMATION_REDCAR].bVisible)
  {
    // Remove animation mask
    internal_DrawHideAnimationMask(ANIMATION_REDCAR, pPosDesc, 0);

    // Change position and frame

    AnimationStates[ANIMATION_REDCAR].iFrameIncCounter+=Animations[ANIMATION_REDCAR].iFramesPer100Move;
    while (AnimationStates[ANIMATION_REDCAR].iFrameIncCounter>=100)
    {
      AnimationStates[ANIMATION_REDCAR].iFrameIncCounter-=100;

      AnimationStates[ANIMATION_REDCAR].iCurFrame =
        (AnimationStates[ANIMATION_REDCAR].iCurFrame+1) % Animations[ANIMATION_REDCAR].iNumFrames;
    }

    AnimationStates[ANIMATION_REDCAR].iX++;
    if (AnimationStates[ANIMATION_REDCAR].iX >= iBufferXSize)
    {
      // Animation went out of screen
      AnimationStates[ANIMATION_REDCAR].bVisible = FALSE;
    } else
    {
      // Animation is still on screen
      // Draw animation mask at new position
      internal_DrawHideAnimationMask(ANIMATION_REDCAR, pPosDesc, 1);
    }
  }
}


void fnBlitterThread(void *p)
{
  ULONG ulSSScanLineBytes; // output for number of bytes a scan line
  ULONG ulSSScanLines;     // output for height of the buffer
  char *pchWork;           // pointer to the image
  ULONG ulWScanLineBytes; // output for number of bytes a scan line
  ULONG ulWScanLines;     // output for height of the buffer

  int iBufSize;
  int iTimer;
  int bNeedBgCopy;
  int iX, iY;
  int iTemp;
  int i;

#ifdef DEBUG_LOGGING
  AddLog("[fnBlitterThread] : started!\n");
#endif

  // Initialize random number generator
  srand(time(NULL));

  iTimer = 0; // Set current time to zero

  iBufSize = iBufferXSize * iBufferYSize;
  pPosDescBuffer = (PosDesc_p) malloc(sizeof(PosDesc_t) * iBufSize);
  if (!pPosDescBuffer)
  {
    // Eeek, not enough memory!
    while (!bShutdownRequest)
      DosSleep(32);

    _endthread();
    return;
  }

  memset(pPosDescBuffer, 0, sizeof(PosDesc_t) * iBufSize);
  memset(ObjectStates, 0, sizeof(ObjectStates));
  memset(AnimationStates, 0, sizeof(AnimationStates));

  internal_CreateConvertedImages();

  bNeedBgCopy = 1;

  // Main loop
  while (!bShutdownRequest)
  {
    // Start drawing the image into the buffer!
    if ((!bScreenShotThreadDone) || (bPaused))
      DosSleep(32);
    else
    if (DosRequestMutexSem(hmtxUseDive, SEM_INDEFINITE_WAIT)==NO_ERROR)
    {
      // Get screenshot image parameters
      ulSSScanLineBytes = iBufferXSize * (iBufferBPP+7)/8;
      ulSSScanLines = iBufferYSize;

      if (bHWVideoPresent)
      {
        ULONG ulPhysAddr;
        // Get pointer to HWVideo buffer!
#ifdef DEBUG_LOGGING
//        AddLog("[fnBlitterThread] : Getting pchWork from HWVIDEO\n");
#endif
        iTemp = pfnHWVIDEOBeginUpdate(&pchWork, &ulPhysAddr);
        ulWScanLineBytes = (iBufferXSize*2+OverlayCaps.ulScanAlign)&~OverlayCaps.ulScanAlign;
        ulWScanLines = iBufferYSize;

#ifdef DEBUG_LOGGING
//        AddLog("[fnBlitterThread] : BeginUpdate rc = ");
//        {
//          char achTemp[128];
//          sprintf(achTemp, "%d", iTemp);
//          AddLog(achTemp);
//        }
//        AddLog("\n");
#endif

      } else
      {
#ifdef DEBUG_LOGGING
//      AddLog("[fnBlitterThread] : Calling DiveBeginImageBufferAccess\n");
#endif

        iTemp = DiveBeginImageBufferAccess(hDive , ulWorkImage,
                                           &pchWork , &ulWScanLineBytes, &ulWScanLines);
      }

      // If we have the pchWork pointer, then:
      if (!iTemp)
      {
#ifdef DEBUG_LOGGING
//      AddLog("[fnBlitterThread] : Got pchWork\n");
#endif

        if (iTimer<1000)
        {
          iTimer++;
          // Decide randomly if we want to create new flakes.
          i = (rand() % 1000 < iTimer);
        } else
        {
          // Always try to create new flakes!
          i = 1;
        }

        if (i)
        {
          // Time to create a new snowflake!
          i = rand() % iBufferXSize;
          pPosDescBuffer[i].iPosType = POSTYPE_SNOW;
          pPosDescBuffer[i].iWeight = rand() % 10;
          pPosDescBuffer[i].iPressureFromAbove = 0;
        }

        if (ModuleConfig.bUseAnims)
        {
            // Hide animation objects
            internal_HideAnimationPics(pchWork, ulWScanLineBytes, pchScreenShot, ulSSScanLineBytes);
        }

        if (ModuleConfig.bUseObjects)
        {
            // Add/Remove objects randomly
            internal_AddRemoveObjects(pPosDescBuffer, pchWork, ulWScanLineBytes, pchScreenShot, ulSSScanLineBytes);
        }


        if (ModuleConfig.bUseAnims)
        {
            // Add/Remove animations
            internal_AddRemoveAnimations(pPosDescBuffer, pchWork, ulWScanLineBytes, pchScreenShot, ulSSScanLineBytes);

            // Move animations
            internal_MoveAnimations(pPosDescBuffer);
        }

        // Move all the snowflakes!
        i = iBufSize-1;
        iX = iBufferXSize-1;
        iY = iBufferYSize-1;
        while (i>=0)
        {
          // Do something
          if (pPosDescBuffer[i].iPosType == POSTYPE_SNOW)
          {
            // There is a snowflake here!
            // Let's try to do something with it!

            // First, let's see if it has too much pressure and if it gets killed because of it!
            if (pPosDescBuffer[i].iPressureFromAbove>iBufferYSize)
            {
              // This snowflake has too much pressure from above, so we kill it.
              pPosDescBuffer[i].iPosType = POSTYPE_AIR;
              internal_HideFlake(pchWork, ulWScanLineBytes, pchScreenShot, ulSSScanLineBytes, iX, iY);
            } else
            {
              int aiPossibleNewPos[3];
              int iNumPossibleNewPos;
              int iNewXPos, iNewYPos, iNewPos;

              // Ok, this snowflake has no too much pressure, so try to move it!

              if (iY>=iBufferYSize-1)
              {
                // This snowflake cannot fall more, it's already at the bottom of the
                // screen, so it will simply stay there.

                // As we have to re-build the whole image for YUV overlays, we have to redraw the snowflake here.
                if (bHWVideoPresent)
                  internal_ShowFlake(pchWork, ulWScanLineBytes, iX, iY);
              } else
              {
                // We're still far from the bottom of the screen, so try to fall!
                iNumPossibleNewPos = 0;

                // Check if we can fall down
                if (pPosDescBuffer[i+iBufferXSize].iPosType==POSTYPE_AIR)
                {
                  aiPossibleNewPos[iNumPossibleNewPos] = POSSIBLE_NEWPOS_DOWN;
                  iNumPossibleNewPos++;
                }

                // Check if we can fall down plus left
                iNewYPos = iY+1;
                iNewXPos = iX-1;
                if (iNewXPos<0) iNewXPos = iBufferXSize-1;
                if ((pPosDescBuffer[iNewYPos*iBufferXSize+iNewXPos].iPosType==POSTYPE_AIR) &&
                    (pPosDescBuffer[iY*iBufferXSize+iNewXPos].iPosType==POSTYPE_AIR))
                {
                  aiPossibleNewPos[iNumPossibleNewPos] = POSSIBLE_NEWPOS_DOWNLEFT;
                  iNumPossibleNewPos++;
                }

                // Check if we can call down plus right
                iNewYPos = iY+1;
                iNewXPos = iX+1;
                if (iNewXPos>=iBufferXSize) iNewXPos = 0;
                if ((pPosDescBuffer[iNewYPos*iBufferXSize+iNewXPos].iPosType==POSTYPE_AIR) &&
                    (pPosDescBuffer[iY*iBufferXSize+iNewXPos].iPosType==POSTYPE_AIR))
                {
                  aiPossibleNewPos[iNumPossibleNewPos] = POSSIBLE_NEWPOS_DOWNRIGHT;
                  iNumPossibleNewPos++;
                }

                if (iNumPossibleNewPos == 0)
                {
                  // This snowflake cannot fall, so it pushes the something under it.
                  pPosDescBuffer[i+iBufferXSize].iPressureFromAbove =
                    pPosDescBuffer[i].iPressureFromAbove + pPosDescBuffer[i].iWeight;

                  // As we have to re-build the whole image for YUV overlays, we have to redraw the snowflake here.
                  if (bHWVideoPresent)
                    internal_ShowFlake(pchWork, ulWScanLineBytes, iX, iY);
                } else
                {
                  // Find a new position from possible ones
                  switch (aiPossibleNewPos[rand() % iNumPossibleNewPos])
                  {
                    case POSSIBLE_NEWPOS_DOWN:
                      internal_HideFlake(pchWork, ulWScanLineBytes, pchScreenShot, ulSSScanLineBytes, iX, iY);
                      iNewYPos = iY+1;
                      iNewXPos = iX;
                      memcpy(pPosDescBuffer+i+iBufferXSize, pPosDescBuffer+i, sizeof(PosDesc_t));
                      pPosDescBuffer[i].iPosType = POSTYPE_AIR;
                      pPosDescBuffer[i+iBufferXSize].iPressureFromAbove = 0;
                      internal_ShowFlake(pchWork, ulWScanLineBytes, iNewXPos, iNewYPos);
                      break;
                    case POSSIBLE_NEWPOS_DOWNLEFT:
                      internal_HideFlake(pchWork, ulWScanLineBytes, pchScreenShot, ulSSScanLineBytes, iX, iY);
                      iNewYPos = iY+1;
                      iNewXPos = iX-1;
                      if (iNewXPos<0) iNewXPos = iBufferXSize-1;
                      iNewPos = iNewYPos * iBufferXSize + iNewXPos;
                      memcpy(pPosDescBuffer+iNewPos, pPosDescBuffer+i, sizeof(PosDesc_t));
                      pPosDescBuffer[i].iPosType = POSTYPE_AIR;
                      pPosDescBuffer[iNewPos].iPressureFromAbove = 0;
                      internal_ShowFlake(pchWork, ulWScanLineBytes, iNewXPos, iNewYPos);
                      break;
                    case POSSIBLE_NEWPOS_DOWNRIGHT:
                      internal_HideFlake(pchWork, ulWScanLineBytes, pchScreenShot, ulSSScanLineBytes, iX, iY);
                      iNewYPos = iY+1;
                      iNewXPos = iX+1;
                      if (iNewXPos>=iBufferXSize) iNewXPos = 0;
                      iNewPos = iNewYPos * iBufferXSize + iNewXPos;
                      memcpy(pPosDescBuffer+iNewPos, pPosDescBuffer+i, sizeof(PosDesc_t));
                      pPosDescBuffer[i].iPosType = POSTYPE_AIR;
                      pPosDescBuffer[iNewPos].iPressureFromAbove = 0;
                      internal_ShowFlake(pchWork, ulWScanLineBytes, iNewXPos, iNewYPos);
                      break;
                    default:
                      break;
                  }
                }
              }
            }
          } else
          {
            // No snowflake here.
            // As we have to re-build the whole image for YUV overlays, we do a copy from
            // the screenshot image to here. We also have to rebuild the image for DIVE images
            // for the first draw
            if ((bHWVideoPresent) || (bNeedBgCopy))
              internal_HideFlake(pchWork, ulWScanLineBytes, pchScreenShot, ulSSScanLineBytes, iX, iY);
          }

          // Go for next position
          i--;
          iX--;
          if (iX<0)
          {
            iX = iBufferXSize-1;
            iY--;
          }
        }

        bNeedBgCopy = 0;

        if (ModuleConfig.bUseObjects)
        {
            // Re-blit objects
            internal_DrawObjects(pchWork, ulWScanLineBytes);
        }

        if (ModuleConfig.bUseAnims)
        {
            // Draw animation objects
            internal_DrawAnimationPics(pchWork, ulWScanLineBytes);
        }

        if (!bHWVideoPresent)
        {
#ifdef DEBUG_LOGGING
          // AddLog("[fnBlitterThread] : Releasing DIVE buffer\n");
#endif
          // Release DIVE buffer
          DiveEndImageBufferAccess( hDive , ulWorkImage );
        } else
        {
#ifdef DEBUG_LOGGING
          // AddLog("[fnBlitterThread] : Releasing YUV buffer\n");
#endif
          // Release YUV buffer
          pfnHWVIDEOEndUpdate();
        }
      } // End of if (!iTemp)  (if we could got image buffers)

      // Release Dive
      DosReleaseMutexSem(hmtxUseDive);

      // Ok, image has been prepared!
      // Now wait for the time to blit it!
      if (bUseFrameRegulation)
      {
        if (DosWaitEventSem(hevBlitEvent, 1000)==NO_ERROR)
        {
          ULONG ulPostCount;
          ulPostCount = 0;
          DosResetEventSem(hevBlitEvent, &ulPostCount);
        } else
          DosSleep(32);
      } else
        DosSleep(32);

      // Blit the image.
      if (!bHWVideoPresent)
      {
        if (!bPaused)
        {
          if (DosRequestMutexSem(hmtxUseDive, SEM_INDEFINITE_WAIT)==NO_ERROR)
          {
#ifdef DEBUG_LOGGING
            // AddLog("[fnBlitterThread] : Blitting image\n");
#endif

            DiveBlitImage(hDive, ulWorkImage, DIVE_BUFFER_SCREEN);
            DosReleaseMutexSem(hmtxUseDive);
          }
        } else
          DosSleep(32);
      } // No need to blit if HWVideo is used
    }
  }

  internal_DestroyConvertedImages();

  if (pPosDescBuffer)
    free(pPosDescBuffer);

#ifdef DEBUG_LOGGING
  AddLog("[fnBlitterThread] : stopped!\n");
#endif

  _endthread();
}


void fnSaverThread(void *p)
{
  HWND hwndParent = (HWND) p;
  HWND hwndOldFocus;
  HWND hwndOldSysModal;
  HAB hab;
  QMSG msg;
  ULONG ulStyle;
  TID tidScreenShotThread;

  // Set our thread to slightly more than regular to be able
  // to update the screen fine.
  DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR, +5, 0);

  // Initialize global variables
  bShutdownRequest = 0;
  bPaused = FALSE;
  bScreenShotThreadDone = 0;
  bScreenCaptured = 0;
  hwndSaverWindow = NULLHANDLE;

  // Initialize PM usage
  hab = WinInitialize(0);
  hmqSaverThreadMsgQueue = WinCreateMsgQueue(hab, 0);

  // Set up DIVE and/or HWVideo
  if (!SetupGraphics())
  {
#ifdef DEBUG_LOGGING
    AddLog("[fnSaverThread] : Could not set up DIVE and/or HWVideo!\n");
#endif
    // Yikes, could not set up DIVE!
    iSaverThreadState = STATE_STOPPED_ERROR;
  } else
  {
    // Ok, we have DIVE.

    // Start the screen shot thread!
    tidScreenShotThread = _beginthread(fnScreenShotThread,
                                       0,
                                       1024*1024,
                                       NULL);
    DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR,
                   0, tidScreenShotThread);

    // Wait until the screen shot thread copies the screen to its
    // local space!
    // This is needed so we won't put there the biig Calculating thing yet,
    // and the screen shot will not be taken from the Calculating sign.
    while (!bScreenCaptured) DosSleep(32);

    // Let's create a new window, or subclass the old!
    if (bOnlyPreviewMode)
    {
      PFNWP pfnOldWindowProc;

      hwndSaverWindow = hwndParent; // The saver window is what we'll subclass.

#ifdef DEBUG_LOGGING
      AddLog("Subclassing window\n");
#endif
      // We should run in preview mode, so the hwndParent we have is not the
      // desktop, but a special window we have to subclass.
      pfnOldWindowProc = WinSubclassWindow(hwndParent,
                                           (PFNWP) fnSaverWindowProc);

#ifdef DEBUG_LOGGING
      AddLog("Sending WM_SUBCLASS_INIT\n");
#endif
      // Initialize window proc (simulate WM_CREATE)
      WinSendMsg(hwndParent, WM_SUBCLASS_INIT, (MPARAM) NULL, (MPARAM) NULL);
      // Also make sure that the window will be redrawn with the new
      // window proc.
      WinInvalidateRect(hwndParent, NULL, FALSE);

#ifdef DEBUG_LOGGING
      AddLog("WinSetVisibleRegionNotify\n");
#endif
      // Turn on visible region notification.
      WinSetVisibleRegionNotify(hwndSaverWindow, TRUE);

#ifdef DEBUG_LOGGING
      AddLog("Posting WM_VRNENABLED\n");
#endif
      // Initialize blitter (by WndProc)!
      WinPostMsg(hwndSaverWindow, WM_VRNENABLED, 0, 0);

#ifdef DEBUG_LOGGING
      AddLog("Setting up framerate regulation\n");
#endif
      // Set up frame-rate regulation!
      // For this, we have to create an event semaphore, and tell the
      // FRAPI to post this semaphore every time we have to blit!
      if (DosCreateEventSem(NULL, &hevBlitEvent, 0, FALSE)!=NO_ERROR)
      {
#ifdef DEBUG_LOGGING
        AddLog("Could not create event semaphore!\n");
#endif
        // Falling back to DosSleep() API:
        bUseFrameRegulation = 0;
      } else
      {
        if (!StartFrameRegulation(hevBlitEvent, ModuleConfig.iFPS)) // Set up Frame Regulator
        {
#ifdef DEBUG_LOGGING
          AddLog("Could not start frame regulation!\n");
#endif
          // Falling back to DosSleep() API:
          bUseFrameRegulation = 0;
          DosCloseEventSem(hevBlitEvent);
        } else
          bUseFrameRegulation = 1;
      }

#ifdef DEBUG_LOGGING
      AddLog("Starting calculator thread\n");
#endif
      // Start the image calculator thread!
      tidBlitterThread = _beginthread(fnBlitterThread,
                                      0,
                                      1024*1024,
                                      NULL);
      if (ModuleConfig.bIdleSaver)
        DosSetPriority(PRTYS_THREAD, PRTYC_IDLETIME,
                       31, tidBlitterThread);
      else
        DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR,
		       0, tidBlitterThread);

      iSaverThreadState = STATE_RUNNING;

#ifdef DEBUG_LOGGING
      AddLog("[fnSaverThread] : Entering message loop (Preview)\n");
#endif

      // Process messages until WM_QUIT!
      while (WinGetMsg(hab, &msg, 0, 0, 0))
        WinDispatchMsg(hab, &msg);

#ifdef DEBUG_LOGGING
      AddLog("[fnSaverThread] : Leaved message loop (Preview)\n");
#endif

      // Turn off visible region notification.
      WinSetVisibleRegionNotify(hwndSaverWindow, FALSE);

#ifdef DEBUG_LOGGING
      AddLog("Stopping Calculator thread\n");
#endif
      // Stop blitter thread
      bShutdownRequest = 1;
      DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR,
                     0, tidBlitterThread);
      DosWaitThread(&tidBlitterThread, DCWW_WAIT);

#ifdef DEBUG_LOGGING
      AddLog("Stopping framerate regulator\n");
#endif
      // Stop frame rate regulator
      if (bUseFrameRegulation)
      {
        StopFrameRegulation();
        DosCloseEventSem(hevBlitEvent);
      }

#ifdef DEBUG_LOGGING
      AddLog("Sending WM_SUBCLASS_UNINIT\n");
#endif

      // Uinitialize window proc (simulate WM_DESTROY)
      WinSendMsg(hwndParent, WM_SUBCLASS_UNINIT, (MPARAM) NULL, (MPARAM) NULL);

#ifdef DEBUG_LOGGING
      AddLog("Undoing subclassing\n");
#endif
      // Undo subclassing
      WinSubclassWindow(hwndParent,
                        pfnOldWindowProc);
      // Also make sure that the window will be redrawn with the old
      // window proc.
      WinInvalidateRect(hwndParent, NULL, FALSE);

#ifdef DEBUG_LOGGING
      AddLog("State: Stopped.\n");
#endif

      iSaverThreadState = STATE_STOPPED_OK;
    } else
    {
      // We should run in normal mode, so create a new window, topmost, and everything else...
      // Create our window then!
      WinRegisterClass(hab, (PSZ) SAVERWINDOW_CLASS,
                       (PFNWP) fnSaverWindowProc,
                       CS_SIZEREDRAW | CS_CLIPCHILDREN | CS_CLIPSIBLINGS, 0);
  
      hwndOldFocus = WinQueryFocus(HWND_DESKTOP);
  
      // Create the saver output window.
      // Make window 'Always on top'
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
        ShutdownGraphics();
        iSaverThreadState = STATE_STOPPED_ERROR;
      } else
      {
        // Cool, window created!

        // Turn on visible region notification.
        WinSetVisibleRegionNotify(hwndSaverWindow, TRUE);

        // Initialize blitter (by WndProc)!
        WinPostMsg(hwndSaverWindow, WM_VRNENABLED, 0, 0);
  
        // Make sure nobody will be able to switch away if we're saving screen!
        // We do this by making the window system-modal, and giving it the focus!
        hwndOldSysModal = WinQuerySysModalWindow(HWND_DESKTOP);
        WinSetSysModalWindow(HWND_DESKTOP, hwndSaverWindow);
        WinSetFocus(HWND_DESKTOP, hwndSaverWindow);

        // Set up frame-rate regulation!
        // For this, we have to create an event semaphore, and tell the
        // FRAPI to post this semaphore every time we have to blit!
        if (DosCreateEventSem(NULL, &hevBlitEvent, 0, FALSE)!=NO_ERROR)
        {
#ifdef DEBUG_LOGGING
          AddLog("Could not create event semaphore!\n");
#endif
          // Falling back to DosSleep() API:
          bUseFrameRegulation = 0;
        } else
        {
          if (!StartFrameRegulation(hevBlitEvent, ModuleConfig.iFPS)) // Set up Frame Regulator
          {
#ifdef DEBUG_LOGGING
            AddLog("Could not start frame regulation!\n");
#endif
            // Falling back to DosSleep() API:
            bUseFrameRegulation = 0;
            DosCloseEventSem(hevBlitEvent);
          } else
            bUseFrameRegulation = 1;
        }

        // Start the image calculator thread!
        tidBlitterThread = _beginthread(fnBlitterThread,
                                           0,
                                           1024*1024,
                                           NULL);

        DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR,
                       0, tidBlitterThread);

        if (ModuleConfig.bIdleSaver)
          DosSetPriority(PRTYS_THREAD, PRTYC_IDLETIME,
                         31, tidBlitterThread);
        else
          DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR,
                         0, tidBlitterThread);

        iSaverThreadState = STATE_RUNNING;

#ifdef DEBUG_LOGGING
        AddLog("[fnSaverThread] : Entering message loop\n");
#endif
        // Process messages!
        while (WinGetMsg(hab, &msg, 0, 0, 0))
          WinDispatchMsg(hab, &msg);

#ifdef DEBUG_LOGGING
        AddLog("[fnSaverThread] : Leaved message loop\n");
#endif

        DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR,
                       0, tidBlitterThread);

        // Undo system-modal setting
        WinSetSysModalWindow(HWND_DESKTOP, hwndOldSysModal);

        // Turn off visible region notification.
        WinSetVisibleRegionNotify(hwndSaverWindow, FALSE);

        // Stop calculator thread
        bShutdownRequest = 1;
        DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR,
                     0, tidBlitterThread);
        DosWaitThread(&tidBlitterThread, DCWW_WAIT);

        // Stop frame rate regulator
        if (bUseFrameRegulation)
        {
          StopFrameRegulation();
          DosCloseEventSem(hevBlitEvent);
        }

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

#ifdef DEBUG_LOGGING
    AddLog("[fnSaverThread] : Waiting for ScreenShot thread to stop\n");
#endif
    // Make sure the screenshot thread is not running anymore
    DosWaitThread(&tidScreenShotThread, DCWW_WAIT);

#ifdef DEBUG_LOGGING
    AddLog("[fnSaverThread] : Shutting down DIVE and/or HWVideo\n");
#endif

    // Shut down DIVE and/or HWVideo
    ShutdownGraphics();
  }

#ifdef DEBUG_LOGGING
  AddLog("[fnSaverThread] : Destroying msg queue and leaving thread.\n");
#endif

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
  pModuleDesc->iVersionMinor = 1;
  strcpy(pModuleDesc->achModuleName, "Snow");
  strcpy(pModuleDesc->achModuleDesc,
         "This module shows a snowy night.\n"
         "Uses WarpOverlay! or DIVE.\n"
         "Written by Doodle"
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
  // This is called when we should start the saving.
  // Return error if already running, start the saver thread otherwise!

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_StartSaving] : Enter\n");
#endif

  if (bRunning)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSModule_StartSaving] : Already running! Leaving.\n");
#endif
    return SSMODULE_ERROR_ALREADYRUNNING;
  }

  // Read configuration!
  internal_LoadConfig(&ModuleConfig, pchHomeDirectory);

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

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_StartSaving] : Screen saver started and running! Leaving.\n");
#endif

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

#ifdef DEBUG_LOGGING
    AddLog("[---- UNLOADED ----]\n");
#endif
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

#ifdef DEBUG_LOGGING
    AddLog("[---- LOADED ----]\n");
#endif

  }
  return 1;
}
