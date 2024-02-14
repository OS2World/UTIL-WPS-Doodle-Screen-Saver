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

/* This screen saver module is *very* heavily based on the work of  */
/* ErOs2, most of its parts are copied from his source code!        */

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

#include "hwvideo.h"

#include "SSModule.h"
#include "FrameRate.h"
#include "ifsim-Resources.h"
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
#define SAVERWINDOW_CLASS "IFSIM_SCREENSAVER_CLASS"

// Private window messages
#define WM_SUBCLASS_INIT       (WM_USER+1)
#define WM_SUBCLASS_UNINIT     (WM_USER+2)
#define WM_ASKPASSWORD         (WM_USER+3)
#define WM_SHOWWRONGPASSWORD   (WM_USER+4)
#define WM_PAUSESAVING         (WM_USER+5)
#define WM_RESUMESAVING        (WM_USER+6)

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
  int iNumOfPoints;
  int bIdleSaver;
  int iFPS;
  int bUseWO;
} ModuleConfig_t, *ModuleConfig_p;

typedef struct _cifs_par
{
    unsigned long t_Rand;
    unsigned long t_m;
    unsigned long t_points;
    long x_size_half, y_size_half, x_size, y_size;
} cifs_par;

// Structure field offsets
#define cifs_par_t_Rand       0
#define cifs_par_t_m          4
#define cifs_par_t_points     8
#define cifs_par_x_size_half 12
#define cifs_par_y_size_half 16
#define cifs_par_x_size      20
#define cifs_par_y_size      24

typedef struct ConfigDlgInitParams_s
{
  USHORT          cbSize; // Required to be able to pass this structure to WM_INITDLG
  char           *pchHomeDirectory;
  ModuleConfig_t  ModuleConfig;
} ConfigDlgInitParams_t, *ConfigDlgInitParams_p;

// Defines
#define FracBits  16                    // bits per fraction part
//#define UNIT      (1<<FracBits)
#define UNIT        0x10000
#define UNIT_PER_2  0x08000
#define MAX_T     150                   // Maximal Translation (in points).
#define MAX_R     0.85                  // Maximal shrink-factor.
#define MAX_ITER  100000                // Max. number of iterations.

#define M_PI      3.1415

typedef struct _mapping
{
    long ra , rb , rc , rd , tx , ty;
} mapping;

// Structure field offsets
#define mapping_ra      0
#define mapping_rb      4
#define mapping_rc      8
#define mapping_rd     12
#define mapping_tx     16
#define mapping_ty     20

ModuleConfig_t  ModuleConfig;
char     achUndoBuf[512];
int      bShutdownRequest;
HEV      hevBlitEvent; // Event semaphore, posted if the image should be blit!
TID      tidBlitterThread;
int      bUseFrameRegulation;
cifs_par cp;
HDIVE    hDive;
int      bVRNEnabled;
HMTX     hmtxUseDive;
ULONG    ulImage;
int iBufferXSize, iBufferYSize;
BYTE Rand[ MAX_ITER ];
double angle[ 15 ];
double angle_inc[ 15 ];
mapping m[ 3 ];
int DV = 2000000000;

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


#define NUM_OF_CUSTOM_RESOLUTIONS 14
int aiCustomResWidth[NUM_OF_CUSTOM_RESOLUTIONS] =
  { 320, 320, 400, 512, 640, 640, 800, 1024, 1152, 1280, 1600, 1800, 1920, 2048 };
int aiCustomResHeight[NUM_OF_CUSTOM_RESOLUTIONS] =
  { 200, 240, 300, 384, 480, 512, 600,  768,  864, 1024, 1200, 1350, 1440, 1536 };


//#define DEBUG_LOGGING
#ifdef DEBUG_LOGGING
void AddLog(char *pchMsg)
{
  FILE *hFile;

  hFile = fopen("d:\\ifsim.log", "at+");
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
	sprintf(pchFileName, "%sModules\\IFSIM\\%s.msg", pchHomeDirectory, achRealLocaleName);
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
	  sprintf(pchFileName, "%sModules\\IFSIM\\%s.msg", pchHomeDirectory, pchLang);
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
  pConfig->iResCustomXSize = 640;
  pConfig->iResCustomYSize = 480;
  pConfig->iNumOfPoints = 20000;
  pConfig->bIdleSaver = TRUE;
  pConfig->iFPS = 15; // 15 FPS
  pConfig->bUseWO = TRUE;

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
    strncat(achFileName, ".dssaver\\IFSIM.CFG", sizeof(achFileName));
    // Try to open that config file!
    hFile = fopen(achFileName, "rt");
  }

  if (!hFile)
  {
    // If we could not open a config file in the HOME directory, or there was
    // no HOME directory, then try to use the DSSaver Global home directory!
    snprintf(achFileName, sizeof(achFileName), "%sIFSIM.CFG", pchHomeDirectory);
    hFile = fopen(achFileName, "rt");
    if (!hFile)
      return; // Could not open file!
  }


  iScanned = fscanf(hFile, "%d %d %d %d %d %d %d",
                    &(TempConfig.iResType),
                    &(TempConfig.iResCustomXSize),
                    &(TempConfig.iResCustomYSize),
                    &(TempConfig.iNumOfPoints),
                    &(TempConfig.bIdleSaver),
                    &(TempConfig.iFPS),
                    &(TempConfig.bUseWO));
  fclose(hFile);

  if (iScanned == 7)
  {
    // Fine, we could read all we wanted, so use the values we read!
    memcpy(pConfig, &TempConfig, sizeof(TempConfig));
  }
#ifdef DEBUG_LOGGING
  else
    AddLog("[internal_LoadConfig] : Could not parse all the 7 settings, using defaults!\n");
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
    strncat(achFileName, "\\IFSIM.CFG", sizeof(achFileName));
    // Try to open that config file!
    hFile = fopen(achFileName, "wt");
  }

  if (!hFile)
  {
    // If we could not open a config file in the HOME directory, or there was
    // no HOME directory, then try to use the DSSaver Global home directory!
    snprintf(achFileName, sizeof(achFileName), "%sIFSIM.CFG", pchHomeDirectory);
    hFile = fopen(achFileName, "wt");
    if (!hFile)
      return; // Could not open file!
  }

  fprintf(hFile, "%d %d %d %d %d %d %d",
          (pConfig->iResType),
          (pConfig->iResCustomXSize),
          (pConfig->iResCustomYSize),
          (pConfig->iNumOfPoints),
          (pConfig->bIdleSaver),
          (pConfig->iFPS),
          (pConfig->bUseWO));
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
      WinSetDlgItemText(hwnd, GB_NUMBEROFPOINTS, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0008"))
      WinSetDlgItemText(hwnd, ST_PLEASEENTERAVALUEBETWEEN, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0009"))
      WinSetDlgItemText(hwnd, ST_NUMBEROFPOINTS, achTemp);

    // Groupbox-3
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0010"))
      WinSetDlgItemText(hwnd, GB_PROCESSORUSAGE, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0011"))
      WinSetDlgItemText(hwnd, RB_RUNINIDLEPRIORITY, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0012"))
      WinSetDlgItemText(hwnd, RB_RUNINREGULARPRIORITY, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0013"))
      WinSetDlgItemText(hwnd, ST_ANIMATIONSPEED, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0014"))
      WinSetDlgItemText(hwnd, ST_FPS, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0015"))
      WinSetDlgItemText(hwnd, CB_USEWARPOVERLAYIFPRESENT, achTemp);


    // Buttons
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0016"))
      WinSetDlgItemText(hwnd, PB_CONFOK, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0017"))
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
  int iCX, iCY, iBottomButtonsWidth;
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(hwnd))
  {
    WinEnableWindowUpdate(hwnd, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Get window size!
  WinQueryWindowPos(hwnd, &swpWindow);

  // Query static height in pixels. 
  internal_GetStaticTextSize(hwnd, ST_PLEASEENTERAVALUEBETWEEN, &iCX, &iCY);
  staticheight = iCY;
  swpWindow.cx = iCX + 8*CXDLGFRAME;

  // Query button height in pixels.
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CONFOK), &swpTemp);
  buttonheight = swpTemp.cy;

  // Query radio button height in pixels.
  WinQueryWindowPos(WinWindowFromID(hwnd, RB_RUNINIDLEPRIORITY), &swpTemp);
  radioheight = swpTemp.cy;

  // Get extra sizes
  internal_GetCheckboxExtraSize(hwnd, CB_USEWARPOVERLAYIFPRESENT, &iCBCX, &iCBCY);
  internal_GetRadioButtonExtraSize(hwnd, RB_CUSTOMRESOLUTION, &iRBCX, &iRBCY);

  // Window width
  internal_GetStaticTextSize(hwnd, CB_USEWARPOVERLAYIFPRESENT, &iCX, &iCY);
  if (swpWindow.cx < iCX + iCBCX + 8*CXDLGFRAME)
    swpWindow.cx = iCX + iCBCX + 8*CXDLGFRAME;

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

  // "Number of points" groupbox
  gbheight =
    CYDLGFRAME + // frame
    CYDLGFRAME + // Margin
    staticheight + CYDLGFRAME + // Entry field
    2*CYDLGFRAME + // Margin
    staticheight + // static text
    CYDLGFRAME + // Margin
    staticheight; // Text of groupbox

  WinQueryWindowPos(WinWindowFromID(hwnd, GB_PROCESSORUSAGE), &swpTemp);
  WinSetWindowPos(WinWindowFromID(hwnd, GB_NUMBEROFPOINTS),
		  HWND_TOP,
                  2*CXDLGFRAME,
                  swpTemp.y + swpTemp.cy + CYDLGFRAME,
                  swpWindow.cx - 4*CXDLGFRAME,
                  gbheight,
                  SWP_MOVE | SWP_SIZE);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_NUMBEROFPOINTS), &swpTemp);

  internal_GetStaticTextSize(hwnd, ST_NUMBEROFPOINTS, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_NUMBEROFPOINTS), HWND_TOP,
                  swpTemp.x + 2*CXDLGFRAME,
                  swpTemp.y + 2*CYDLGFRAME,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_NUMBEROFPOINTS), &swpTemp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, EF_NUMOFPOINTS), &swpTemp3);
  internal_GetStaticTextSize(hwnd, ST_RESOLUTION, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, EF_NUMOFPOINTS), HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME,
                  swpTemp2.y,
                  iCX,
                  staticheight,
                  SWP_MOVE | SWP_SIZE);

  internal_GetStaticTextSize(hwnd, ST_PLEASEENTERAVALUEBETWEEN, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_PLEASEENTERAVALUEBETWEEN), HWND_TOP,
                  swpTemp.x + 2*CXDLGFRAME,
                  swpTemp2.y + staticheight + 2*CYDLGFRAME,
                  iCX, iCY,
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

  WinQueryWindowPos(WinWindowFromID(hwnd, GB_NUMBEROFPOINTS), &swpTemp);
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
  internal_GetStaticTextSize(hwnd, ST_NUMBEROFPOINTS, &iCX, &iCY);
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

  sprintf(achTemp, "%d", pCfgDlgInit->ModuleConfig.iNumOfPoints);
  WinSetDlgItemText(hwnd, EF_NUMOFPOINTS, achTemp);

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
	  case EF_NUMOFPOINTS:
	    if (SHORT2FROMMP(mp1)==EN_SETFOCUS)
	    {
	      // The entry field is receiving the focus
	      WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1), sizeof(achUndoBuf), achUndoBuf);
              return (MRESULT) 0;
	    }
	    if (SHORT2FROMMP(mp1)==EN_KILLFOCUS)
	    {
	      char achValue[128];
              int iValue;
	      // The entry field is loosing the focus
	      WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1), sizeof(achValue), achValue);
              iValue = atoi(achValue);
	      if ((iValue<1) || (iValue>100000))
	      {
		// Invalid FPS value, restore old!
		WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achUndoBuf);
	      } else
	      {
		// Valid value!
		pCfgDlgInit->ModuleConfig.iNumOfPoints = iValue;
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
        DiveSetupBlitter(hDive , 0);
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
          SetupBlitter.fccSrcColorFormat = FOURCC_LUT8;
          SetupBlitter.ulSrcWidth = iBufferXSize;
          SetupBlitter.ulSrcHeight = iBufferYSize;
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

        hpsBP = WinBeginPaint(hwnd, 0L, &rc);
        WinQueryWindowRect(hwnd, &rc);

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
	WinEndPaint(hpsBP);
	break;
      }

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

int SetupGraphics()
{
  DIVE_CAPS DiveCaps;
  FOURCC    fccFormats[256];
  APIRET    rc;

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
    // Try to use WO!
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
  
        } else
        {
          HWVIDEOSETUP Setup;
  
          // We can use it!
  
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
  DiveCaps.pFormatData = fccFormats;
  DiveCaps.ulFormatLength = sizeof(fccFormats)/sizeof(FOURCC);
  DiveCaps.ulStructLen = sizeof(DIVE_CAPS);

  if (DiveQueryCaps(&DiveCaps, DIVE_BUFFER_SCREEN))
  {
    // Could not query DIVE capabilities!
    if (bHWVideoPresent)
    {
      // Uninitialize HWVideo
      pfnHWVIDEOClose();
      DosFreeModule(hmodHWVideo);
      bHWVideoPresent = 0;
    }
    return 0;
  }

  if (DiveCaps.ulDepth < 8)
  {
    // Eeeek, screen color depth is less than 8 bits!
    if (bHWVideoPresent)
    {
      // Uninitialize HWVideo
      pfnHWVIDEOClose();
      DosFreeModule(hmodHWVideo);
      bHWVideoPresent = 0;
    }
    return 0;
  }

  // Open DIVE
  if (DiveOpen(&hDive, FALSE ,0))
  {
    // Error opening DIVE!
    if (bHWVideoPresent)
    {
      // Uninitialize HWVideo
      pfnHWVIDEOClose();
      DosFreeModule(hmodHWVideo);
      bHWVideoPresent = 0;
    }
    return 0;
  }

  if (!bHWVideoPresent)
  {
    // Ok, DIVE opened, now allocate image buffer!
    if (DiveAllocImageBuffer(hDive, &ulImage ,FOURCC_LUT8, iBufferXSize, iBufferYSize ,0 ,0))
    {
      // Error allocating DIVE image buffer!
      DiveClose(hDive);
      return 0;
    }
  }

  // Gooood, dive opened, buffer allocated!

  if (DosCreateMutexSem(NULL, &hmtxUseDive, 0, FALSE)!=NO_ERROR)
  {
    // Yikes, could not create mutex semaphore!
    if (bHWVideoPresent)
    {
      // Uninitialize HWVideo
      pfnHWVIDEOClose();
      DosFreeModule(hmodHWVideo);
      bHWVideoPresent = 0;
    }
    else
      DiveFreeImageBuffer(hDive, ulImage);
    DiveClose(hDive);
    return 0;
  }
  // Setup variables
  bVRNEnabled = FALSE;
  return 1;
}

void ShutdownGraphics()
{
  DosCloseMutexSem(hmtxUseDive);
  if (bHWVideoPresent)
  {
    // Uninitialize HWVideo
    pfnHWVIDEOClose();
    DosFreeModule(hmodHWVideo);
    bHWVideoPresent = 0;
  }
  else
    DiveFreeImageBuffer(hDive,ulImage);
  DiveClose(hDive);
}


long random(long l)
{
  long long llResult;

  llResult = rand();
  llResult *= l;
  llResult /= RAND_MAX;
  return (long) llResult;
}

void calc_ifs(PBYTE v, cifs_par *cp_ptr)
{
  ULONG counter1 = 0;

  counter1 = counter1; // To make compiler warning happy

  _asm
  {
    pushad

    mov     esi, 0
    mov     edi, 0
    mov     ebx, cp_ptr
    mov     eax, [ebx + cifs_par_t_points]
    mov     counter1, eax

again:
 
    mov     edx, [ebx + cifs_par_t_Rand]
    add     edx, counter1
    mov     al, 24                 ;sizeof mapping
    mul     byte ptr [edx]
    cwde
  
    push    ebx

    mov     ebx, [ebx + cifs_par_t_m]
    add     ebx, eax

    mov     eax, [ebx + mapping_ra]
    imul    esi
    shrd    eax,edx,FracBits
    mov     ecx,eax
    mov     eax,[ebx + mapping_rb]
    imul    edi
    shrd    eax,edx,FracBits

    add     eax,ecx
    add     eax,[ebx + mapping_tx]

    push    eax

    mov     eax,[ebx + mapping_rc]
    imul    esi
    shrd    eax,edx,FracBits
    mov     ecx,eax
    mov     eax,[ebx + mapping_rd]
    imul    edi
    shrd    eax,edx,FracBits
    add     eax,ecx
    add     eax,[ebx + mapping_ty]
    mov     edi,eax

    pop     esi

    add     eax,UNIT_PER_2
    sar     eax,FracBits     ;eax=Round(edi)
    mov     ecx,esi
    add     ecx,UNIT_PER_2
    sar     ecx,FracBits     ;ecx=Round(esi)

    pop     ebx

    ; Is point (ecx,eax) inside the screen ?
    add     ecx, [ebx + cifs_par_x_size_half]
    jl      no_plot
    cmp     ecx, [ebx + cifs_par_x_size]
    jge     no_plot
    add     eax, [ebx + cifs_par_y_size_half]
    jl      no_plot
    cmp     eax, [ebx + cifs_par_y_size]
    jge     no_plot

  ;  dec     counter2
  ;  jz      end_iteration

  ; v[ x + ( y * cp_ptr->x_size ) ]++;

    mul	dword ptr [ebx + cifs_par_x_size]
    add   eax, ecx
    mov	edx, v
    inc	byte ptr [eax+edx]

  no_plot:

    dec     counter1
    jnz     again

  end_iteration:
  
    popad
  }
}

void set_probability( long p0 , long p1 , long p2 )
{
    long i ,j ;
    for ( i = 0 ; i < MAX_ITER ; i++ )
    {
        j = random( p0 + p1 + p2 );
        if ( j < p0 )  Rand[ i ] = 0;
        else if ( j < ( p0 + p1 ) )  Rand[ i ] = 1 ;
        else  Rand[ i ] = 2;
    }
}

double rnd( void )
{
    double r = random( DV );
    return r / DV;
}


void fnBlitThread(void *p)
{
  APIRET rc;
  BYTE nr_skew , k , s;
  double s5k , s5k1 , s5k2 , c5k2;
  RGB2 colors[ 256 ];
  char yuvpal_y[256];
  char yuvpal_u[256];
  char yuvpal_v[256];
  PBYTE vidmem;            // pointer to the image
  ULONG ulScanLineBytes;   // output for number of bytes a scan line
  ULONG ulScanLines;       // output for height of the buffer


#ifdef DEBUG_LOGGING
  AddLog("[fnBlitThread] : started!\n");
#endif

  // Initialize random number generator
  srand(time(NULL));

  cp.t_Rand          = (unsigned long)( Rand - 1 );
  cp.t_m             = (unsigned long)m;
  cp.t_points        = ModuleConfig.iNumOfPoints;
  cp.x_size_half     = iBufferXSize/2;
  cp.y_size_half     = iBufferYSize/2;
  cp.x_size          = iBufferXSize;
  cp.y_size          = iBufferYSize;

  // Presets
  set_probability( random( 100 ) , random( 100 ) , random( 100 ) );

  for ( k = 0 ; k < 15 ; k++ )
  {
    angle[ k ] = 2 * M_PI * rnd();
    angle_inc[ k ] = ( rnd() - 0.5 ) / 66;
  }

  nr_skew= (BYTE)random( 4 );
  // end presets

  // Set palette
  memset( colors , 0 , sizeof( colors ) );
  {
    USHORT i;

    colors[ 1 ].bRed   = 0x00;
    colors[ 1 ].bGreen = 0xc0;
    colors[ 1 ].bBlue  = 0x00;
    colors[ 2 ].bRed   = 0x00;
    colors[ 2 ].bGreen = 0xff;
    colors[ 2 ].bBlue  = 0x00;
    for ( i = 3 ; i < 8 ; i++ )
    {
      colors[ i ].bRed   = 0xff;
      colors[ i ].bGreen = 0xff;
      colors[ i ].bBlue  = 0x00;
    }
    for ( i = 8 ; i < 256 ; i++ )
    {
      colors[ i ].bRed   = 0xff;
      colors[ i ].bGreen = 0xff;
      colors[ i ].bBlue  = 0xff;
    }
  }

  if (bHWVideoPresent)
  {
    int i;

    vidmem = (PBYTE) malloc(iBufferXSize * iBufferYSize);
    ulScanLineBytes = iBufferXSize;
    ulScanLines = iBufferYSize;

    // Also initialize YUV palette
    for ( i=0 ; i<256 ; i++ )
    {
      BYTE r = colors[i].bRed;
      BYTE g = colors[i].bGreen;
      BYTE b = colors[i].bBlue;

      yuvpal_y[i]  =  ((RY*r + GY*g + BY*b)>>RGB2YUV_SHIFT) + 16;
      yuvpal_u[i]  =  ((RV*r + GV*g + BV*b)>>RGB2YUV_SHIFT) + 128;
      yuvpal_v[i]  =  ((RU*r + GU*g + BU*b)>>RGB2YUV_SHIFT) + 128;
    }
  } else
  {
    if (DosRequestMutexSem(hmtxUseDive, SEM_INDEFINITE_WAIT)==NO_ERROR)
    {
      DiveSetSourcePalette(hDive , 0 , 256 , (PBYTE)colors);

      DosReleaseMutexSem(hmtxUseDive);
    }
  }

  s = 1;
  while (!bShutdownRequest)
  {
    if (bPaused)
    {
      // We're paused, should do nothing!
      DosSleep(128);
    } else
    // Start drawing the image into the buffer!
    if (DosRequestMutexSem(hmtxUseDive, SEM_INDEFINITE_WAIT)==NO_ERROR)
    {
      if (bHWVideoPresent)
        rc = 0;
      else
        rc = DiveBeginImageBufferAccess(hDive , ulImage,
                                        &vidmem , &ulScanLineBytes, &ulScanLines);
      if (!rc)
      {
        // calc frame, do it 's' times! (to be able to skip frames when needed)
        while (s>0)
        {
          for ( k = 0 ; k < 15 ; k++ )  angle[ k ] = angle[ k ] + angle_inc[ k ];
          k = nr_skew;

          while ( k > 0 )
          {
            k--;
            angle[ 5 * k + 1 ] = angle[ 5 * k ];
          }

          for ( k = 0 ; k < 3 ; k++ )
          {
            s5k  = sin( angle[ 5 * k ] );
            s5k1 = sin( angle[ 5 * k + 1 ] );
            s5k2 = sin( angle[ 5 * k + 2 ] );
            c5k2 = cos( angle[ 5 * k + 2 ] );
            m[ k ].ra = ( MAX_R * UNIT * s5k * c5k2 );
            m[ k ].rc = ( MAX_R * UNIT * s5k * s5k2 );
            m[ k ].rd = ( MAX_R * UNIT * s5k1 * c5k2 );
            m[ k ].rb = -( MAX_R * UNIT * s5k1 * s5k2 );

            m[ k ].tx = ( MAX_T * UNIT * sin( angle[ 5 * k + 3 ] ) );
            m[ k ].ty = ( MAX_T * UNIT * sin( angle[ 5 * k + 4 ] ) );
          }

          memset( vidmem , 0 , ulScanLineBytes * ulScanLines );
          calc_ifs( vidmem , &cp );
          s--;
        }
        if (!bHWVideoPresent)
          DiveEndImageBufferAccess( hDive , ulImage );

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

            // Blit image.
            // If the event semaphore was posted more than one times, then
            // we have to skip frames! So, we'll not blit next time,
            // but calculate x frames instead.
            // Make sure this X will not go into infinite!
            s = ulPostCount;
            if (s==0) s = 1;
            if (s>10) s = 10;

#ifdef DEBUG_LOGGING
//            AddLog("[fnBlitThread] : Time! Blitting.\n");
#endif

            // Blit the image.
            if (DosRequestMutexSem(hmtxUseDive, SEM_INDEFINITE_WAIT)==NO_ERROR)
            {
              if (!bHWVideoPresent)
                DiveBlitImage(hDive, ulImage, DIVE_BUFFER_SCREEN);
              else
              {
                ULONG ulPhysAddr;
                char *pchWork;
                if (!pfnHWVIDEOBeginUpdate(&pchWork, &ulPhysAddr))
                {
                  int x, y, pitch;
                  char *pchSrc;
                  char *pchDst;

                  pitch=(iBufferXSize*2+OverlayCaps.ulScanAlign)&~OverlayCaps.ulScanAlign;
                  for (y=0; y<iBufferYSize; y++)
                  {
                    pchDst = pchWork + y*pitch;
                    pchSrc = vidmem + y*ulScanLineBytes;
                    for (x=0; x<iBufferXSize; x++)
                    {
                      *(pchDst++) = yuvpal_y[*pchSrc];
                      if ((x%2)==0)
                        *(pchDst++) = yuvpal_u[*pchSrc];
                      else
                        *(pchDst++) = yuvpal_v[*pchSrc];

                      pchSrc++;
                    }
                  }
                  pfnHWVIDEOEndUpdate();
                }
              }
              DosReleaseMutexSem(hmtxUseDive);
            }
          } else
          {
#ifdef DEBUG_LOGGING
//            AddLog("[fnBlitThread] : Timeout in waiting for event sem! ... Blitting.\n");
#endif
            // Blit the image.
            if (DosRequestMutexSem(hmtxUseDive, SEM_INDEFINITE_WAIT)==NO_ERROR)
            {
              if (!bHWVideoPresent)
                DiveBlitImage(hDive, ulImage, DIVE_BUFFER_SCREEN);
              else
              {
                ULONG ulPhysAddr;
                char *pchWork;
                if (!pfnHWVIDEOBeginUpdate(&pchWork, &ulPhysAddr))
                {
                  int x, y, pitch;
                  char *pchSrc;
                  char *pchDst;

                  pitch=(iBufferXSize*2+OverlayCaps.ulScanAlign)&~OverlayCaps.ulScanAlign;
                  for (y=0; y<iBufferYSize; y++)
                  {
                    pchDst = pchWork + y*pitch;
                    pchSrc = vidmem + y*ulScanLineBytes;
                    for (x=0; x<iBufferXSize; x++)
                    {
                      *(pchDst++) = yuvpal_y[*pchSrc];
                      if ((x%2)==0)
                        *(pchDst++) = yuvpal_u[*pchSrc];
                      else
                        *(pchDst++) = yuvpal_v[*pchSrc];

                      pchSrc++;
                    }
                  }
                  pfnHWVIDEOEndUpdate();
                }
              }
              DosReleaseMutexSem(hmtxUseDive);
            }
            DosSleep(32);
          }
        } else
        {
#ifdef DEBUG_LOGGING
//          AddLog("[fnBlitThread] : No framerate regulation... Blitting.\n");
#endif
          // If the framerate regulation could not be started,
          // then we'll go at a constant rate, based on the speed
          // of the computer.
          if (DosRequestMutexSem(hmtxUseDive, SEM_INDEFINITE_WAIT)==NO_ERROR)
          {
            if (!bHWVideoPresent)
              DiveBlitImage(hDive, ulImage, DIVE_BUFFER_SCREEN);
            else
            {
              ULONG ulPhysAddr;
              char *pchWork;
              if (!pfnHWVIDEOBeginUpdate(&pchWork, &ulPhysAddr))
              {
                int x, y, pitch;
                char *pchSrc;
                char *pchDst;

                pitch=(iBufferXSize*2+OverlayCaps.ulScanAlign)&~OverlayCaps.ulScanAlign;
                for (y=0; y<iBufferYSize; y++)
                {
                  pchDst = pchWork + y*pitch;
                  pchSrc = vidmem + y*ulScanLineBytes;
                  for (x=0; x<iBufferXSize; x++)
                  {
                    *(pchDst++) = yuvpal_y[*pchSrc];
                    if ((x%2)==0)
                      *(pchDst++) = yuvpal_u[*pchSrc];
                    else
                      *(pchDst++) = yuvpal_v[*pchSrc];

                    pchSrc++;
                  }
                }
                pfnHWVIDEOEndUpdate();
              }
            }
            DosReleaseMutexSem(hmtxUseDive);
          }
          DosSleep(32); // No better idea...
        }
      } else
      {
        // Could not get access to dive buffer!
        DosReleaseMutexSem(hmtxUseDive);
        DosSleep(32);
      }
    }
  }

  if (bHWVideoPresent)
    free(vidmem);

#ifdef DEBUG_LOGGING
  AddLog("[fnBlitThread] : stopped!\n");
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

  // Set our thread to slightly more than regular to be able
  // to update the screen fine.
  DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR, +5, 0);

  // Initialize global variables
  bShutdownRequest = 0;
  bPaused = FALSE;

  // Initialize frame regulation
  Initialize_FrameRegulation();

  // Initialize PM usage
  hab = WinInitialize(0);
  hmqSaverThreadMsgQueue = WinCreateMsgQueue(hab, 0);

  // Set up DIVE or WO
  if (!SetupGraphics())
  {
#ifdef DEBUG_LOGGING
    AddLog("[fnSaverThread] : Could not set up DIVE!\n");
#endif
    // Yikes, could not set up DIVE!
    iSaverThreadState = STATE_STOPPED_ERROR;
  } else
  {
    // Ok, we have DIVE.

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
      AddLog("Starting blitter thread\n");
#endif
      // Start the image calculator and blitter thread!
      tidBlitterThread = _beginthread(fnBlitThread,
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
      AddLog("Stopping blitter thread\n");
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

        // Start the image calculator and blitter thread!
        tidBlitterThread = _beginthread(fnBlitThread,
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
        AddLog("[fnSaverThread] : Entering message loop\n");
#endif
        // Process messages!
        while (WinGetMsg(hab, &msg, 0, 0, 0))
          WinDispatchMsg(hab, &msg);
#ifdef DEBUG_LOGGING
        AddLog("[fnSaverThread] : Leaved message loop\n");
#endif

        // Undo system-modal setting
        WinSetSysModalWindow(HWND_DESKTOP, hwndOldSysModal);

        // Turn off visible region notification.
        WinSetVisibleRegionNotify(hwndSaverWindow, FALSE);

        // Stop blitter thread
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
    // Shut down DIVE or WO
    ShutdownGraphics();
  }

#ifdef DEBUG_LOGGING
  AddLog("[fnSaverThread] : Destroying msg queue and leaving thread.\n");
#endif

  WinDestroyMsgQueue(hmqSaverThreadMsgQueue);
  WinTerminate(hab);

  // Uninitialize frame regulation
  Uninitialize_FrameRegulation();

  _endthread();
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_GetModuleDesc(SSModuleDesc_p pModuleDesc, char *pchHomeDirectory)
{
  FILE *hfNLSFile;

  if (!pModuleDesc)
    return SSMODULE_ERROR_INVALIDPARAMETER;

  // Return info about module!
  pModuleDesc->iVersionMajor = 2;
  pModuleDesc->iVersionMinor = 0;
  strcpy(pModuleDesc->achModuleName, "IFS In Motion");
  strcpy(pModuleDesc->achModuleDesc,
         "Nice Iterated Function System (IFS) in motion.\n"
         "Written by Doodle\n"
         "Using source code of ErOs2\n"
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

