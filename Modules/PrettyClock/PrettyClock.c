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
#include <math.h>
#include <time.h>
#include <setjmp.h>
#define INCL_DOS
#define INCL_WIN
#define INCL_ERRORS
#define INCL_GPI
#include <os2.h>
#include "SSModule.h"
#include "PrettyClock-Resources.h"
#include "MSGX.h" // NLS support
#include "zlib.h" // zlib
#include "png.h"  // libPNG

#ifndef M_PI
#define M_PI 3.14159
#endif
  
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
#define SAVERWINDOW_CLASS "PRETTYCLOCK_SCREENSAVER_CLASS"

// Private window messages
#define WM_SUBCLASS_INIT       (WM_USER+1)
#define WM_SUBCLASS_UNINIT     (WM_USER+2)
#define WM_ASKPASSWORD         (WM_USER+3)
#define WM_SHOWWRONGPASSWORD   (WM_USER+4)
#define WM_PAUSESAVING         (WM_USER+5)
#define WM_RESUMESAVING        (WM_USER+6)
#define WM_SKINCHANGED         (WM_USER+7)

typedef struct ModuleConfig_s
{
  int bSimpleClock;
  char achClockSkin[CCHMAXPATH];

  // Not saved, but used only while running:
  char achHomeDirectory[CCHMAXPATH];
} ModuleConfig_t, *ModuleConfig_p;

typedef struct ConfigDlgInitParams_s
{
  USHORT          cbSize; // Required to be able to pass this structure to WM_INITDLG
  char           *pchHomeDirectory;
  ModuleConfig_t  ModuleConfig;
} ConfigDlgInitParams_t, *ConfigDlgInitParams_p;

typedef struct Image_s
{
  int bValidImage; // True if this image was initialized well, and we
                   // have the pixels here.

  int iImageWidth;
  int iImageHeight;
  int iImagePitch;
  char *pchPixels;

  // Coordinates of center of image
  int iCenterX;
  int iCenterY;
  // Coordinates of point of DrawArea which
  // will be used as the center of this image
  int iCenterOffsetX;
  int iCenterOffsetY;

} Image_t, *Image_p;

typedef struct Skin_s
{
  int bSkinOk; // TRUE if the skin could be loaded.
               // FALSE if the skin had errors, so we have to draw
               //       the clock without using the skin (Simple Clock)

  // Name of skin
  char achSkinName[256];

  // Size of Draw Area
  int iDrawArea_Width;
  int iDrawArea_Height;

  // Background image
  Image_t BackgroundImage;

  // Foreground image
  Image_t ForegroundImage;

  // Description of hour stick
  int bHourAnalogueMode;   // Discreet or analogue position?
  Image_t aHourImages[24];

  // Description of minutes stick
  int bMinuteAnalogueMode;   // Discreet or analogue position?
  Image_t aMinuteImages[60];

  // Description of secondsstick
  Image_t aSecondImages[60];

  // Work buffer:
  unsigned char *pchDrawBuffer;

} Skin_t, *Skin_p;


HMODULE hmodOurDLLHandle;
int     bRunning;
int     bFirstKeyGoesToPwdWindow = 0;
TID     tidSaverThread;
int     iSaverThreadState;
HWND    hwndSaverWindow; // If any...
HMQ     hmqSaverThreadMsgQueue;
int     bOnlyPreviewMode;

ULONG   ulAutoHiderTimerID;

char   *apchNLSText[SSMODULE_NLSTEXT_MAX+1];
HMTX    hmtxUseNLSTextArray;

ModuleConfig_t  ModuleConfig;
Skin_t          CurrentSkin;

int aiCosTable[36000];
int aiSinTable[36000];

// Image movement speed
#define MAX_IMAGE_STEP_IN_PIXELS  2

// Timer ID for moving the bitmap around
#define ANIMATION_TIMER_ID  11
#define ANIMATION_TIMER_TIMEOUT_MSEC 1000
HDC     hdcImage;
HPS     hpsImage;
HBITMAP hbmImage;
POINTL  ptlImageDestPos;
POINTL  ptlImageDestSpeed;
SIZEL   sizImageDestSize, sizImageSize;
ULONG   ulAnimationTimerID;
int     bImageValid;


#ifdef DEBUG_BUILD
#define DEBUG_LOGGING
#endif

#ifdef DEBUG_LOGGING
void AddLog(char *pchMsg)
{
  FILE *hFile;

  hFile = fopen("c:\\PRClock.log", "at+");
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

static int internal_GetConfigFileEntry(FILE *hFile, char **ppchKey, char **ppchValue)
{
  int iLineCount, rc;

  *ppchKey = NULL;
  *ppchValue = NULL;

  // Allocate memory for variables we'll return
  *ppchKey = malloc(4096);
  if (!*ppchKey) return 0;

  *ppchValue = malloc(4096);
  if (!*ppchValue)
  {
    free(*ppchKey);
    *ppchKey = NULL;
    return 0;
  }

  // Process every lines of the config file, but
  // max. 1000, so if something goes wrong, wo won't end up an infinite
  // loop here.
  iLineCount = 0;
  while ((!feof(hFile)) && (iLineCount<1000))
  {
    iLineCount++;

    // First find the beginning of the key
    fscanf(hFile, "%*[\n\t= ]");
    // Now get the key
    rc=fscanf(hFile, "%[^\n\t= ]",*ppchKey);
    if ((rc==1) && ((*ppchKey)[0]!=';'))
    { // We have the key! (and not a remark!)
      // Skip the whitespace after the key, if there are any
      fscanf(hFile, "%*[ \t]");
      // There must be a '=' here
      rc=fscanf(hFile, "%[=]", *ppchValue);
      if (rc==1)
      {
        // Ok, the '=' is there
        // Skip the whitespace, if there are any
        fscanf(hFile, "%*[ \t]");
        // and read anything until the end of line
        rc=fscanf(hFile, "%[^\n]\n", *ppchValue);
        if (rc==1)
        {
          // Found a key/value pair!!
          return 1;
        } else fscanf(hFile, "%*[^\n]\n"); // find EOL
      } else fscanf(hFile, "%*[^\n]\n"); // find EOL
    } else fscanf(hFile, "%*[^\n]\n"); // find EOL
  }
  // Ok, skin file processed, no more pairs!

  free(*ppchKey); *ppchKey = NULL;
  free(*ppchValue); *ppchValue = NULL;
  return 0;
}

static void internal_LoadConfig(ModuleConfig_p pConfig, char *pchHomeDirectory)
{
  FILE *hFile;
  char *pchHomeEnvVar;
  char *pchKey;
  char *pchValue;
  char achFileName[1024];

  // Set defaults first!
  pConfig->bSimpleClock = 1; // Use simple clock by default
  strcpy(pConfig->achClockSkin, "");
  strncpy(pConfig->achHomeDirectory, pchHomeDirectory, sizeof(pConfig->achHomeDirectory));

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
    strncat(achFileName, ".dssaver\\PrClock.CFG", sizeof(achFileName));
    // Try to open that config file!
    hFile = fopen(achFileName, "rt");
  }

  if (!hFile)
  {
    // If we could not open a config file in the HOME directory, or there was
    // no HOME directory, then try to use the DSSaver Global home directory!
    snprintf(achFileName, sizeof(achFileName), "%sPrClock.CFG", pchHomeDirectory);
    hFile = fopen(achFileName, "rt");
    if (!hFile)
      return; // Could not open file!
  }

  // Process every entry of the config file
  pchKey = NULL;
  pchValue = NULL;

  while (internal_GetConfigFileEntry(hFile, &pchKey, &pchValue))
  {
    // Found a key/value pair!
    if (!stricmp(pchKey, "Simple_Clock"))
      pConfig->bSimpleClock = atoi(pchValue);
    else
    if (!stricmp(pchKey, "Clock_Skin"))
      strncpy(pConfig->achClockSkin, pchValue, sizeof(pConfig->achClockSkin));

    free(pchKey); pchKey = NULL;
    if (pchValue)
    {
      free(pchValue); pchValue = NULL;
    }
  }

#ifdef DEBUG_LOGGING
  if (pConfig->bSimpleClock)
    AddLog("Simple_Clock = 1\n");
  else
    AddLog("Simple_Clock = 0\n");

  AddLog("Clock_Skin = ");
  AddLog(pConfig->achClockSkin);
  AddLog("\n");
#endif

  fclose(hFile);
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
    strncat(achFileName, "\\PrClock.CFG", sizeof(achFileName));
    // Try to open that config file!
    hFile = fopen(achFileName, "wt");
  }

  if (!hFile)
  {
    // If we could not open a config file in the HOME directory, or there was
    // no HOME directory, then try to use the DSSaver Global home directory!
    snprintf(achFileName, sizeof(achFileName), "%sPrClock.CFG", pchHomeDirectory);
    hFile = fopen(achFileName, "wt");
    if (!hFile)
      return; // Could not open file!
  }

  // Save our values!
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fprintf(hFile, "; This is an automatically generated configuration file for\n");
  fprintf(hFile, "; the Pretty Clock Screen Saver Module.\n");
  fprintf(hFile, "; Do not modify it by hand!\n");
  fprintf(hFile, "\n");
  fprintf(hFile, "Simple_Clock = %d\n",
          pConfig->bSimpleClock);
  fprintf(hFile, "Clock_Skin = %s\n",
          pConfig->achClockSkin);
  fprintf(hFile, "\n");
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fprintf(hFile, "; End of configuration file\n");
  fprintf(hFile, "; ----------------------------------------------------------\n");
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

void internal_FreeImage(Image_p pImage)
{
  if ((pImage) && (pImage->bValidImage))
  {
    if (pImage->pchPixels)
    {
      free(pImage->pchPixels);
      pImage->pchPixels = NULL;
    }
    pImage->bValidImage = FALSE;
  }
}

void internal_LoadImage(char *pchHomeDirectory, char *pchImageName, Image_p pImage)
{
  char achImgFile[CCHMAXPATH];
  int i;
  int iBitDepth;
  int iColorType;
  ULONG ulWidth;
  ULONG ulHeight;
  ULONG ulChannels;
  ULONG ulRowBytes;
  png_bytepp ppRows = NULL;
  FILE *hFile;
  png_byte pbSig[8];
  png_structp png_ptr;
  png_infop info_ptr;

  if (pImage && pchHomeDirectory && pchImageName)
  {
    // Make sure nobody will load an image onto another one,
    // resulting in memory leak!
    if (pImage->bValidImage)
      internal_FreeImage(pImage);

    // First assemble the full path to the image file from
    // the skin path and image name!
    snprintf(achImgFile, sizeof(achImgFile), "%s\\Modules\\PrClock\\%s", pchHomeDirectory, pchImageName);

    // Okay, we got the full path of the image file.
    // Try to open it then, and check if it's a valid PNG file!

    hFile = fopen(achImgFile, "rb");
    if (!hFile)
    {
#ifdef DEBUG_LOGGING
      AddLog("[internal_LoadSkin] : Error opening image file.\n");
      AddLog(achImgFile);AddLog("\n");
#endif
      // pImage->bValidImage is already set to FALSE.
      return;
    }

    // Check if it's a valid PNG file
    memset(pbSig, 0, sizeof(pbSig));
    fread(pbSig, 1, sizeof(pbSig), hFile);
    if (!png_check_sig(pbSig, sizeof(pbSig)))
    {
#ifdef DEBUG_LOGGING
      AddLog("[internal_LoadSkin] : Not a valid PNG file.\n");
#endif
      // pImage->bValidImage is already set to FALSE.
      fclose(hFile);
      return;
    }

    // Allocate png structures
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                     NULL, NULL, NULL);
    if (!png_ptr)
    {
#ifdef DEBUG_LOGGING
      AddLog("[internal_LoadSkin] : Out of memory! (@1)\n");
#endif
      // pImage->bValidImage is already set to FALSE.
      fclose(hFile);
      return;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
#ifdef DEBUG_LOGGING
      AddLog("[internal_LoadSkin] : Out of memory! (@2)\n");
#endif
      // pImage->bValidImage is already set to FALSE.
      png_destroy_read_struct(&png_ptr, NULL, NULL);
      fclose(hFile);
      return;
    }

    // Set error handling
    if (setjmp(png_jmpbuf(png_ptr)))
    {
#ifdef DEBUG_LOGGING
      AddLog("[internal_LoadSkin] : Error decoding PNG image.\n");
#endif
      // pImage->bValidImage is already set to FALSE.

      if (pImage->pchPixels)
      {
        free(pImage->pchPixels);
        pImage->pchPixels = NULL;
      }
      if (ppRows)
      {
        free(ppRows);
        ppRows = NULL;
      }
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      fclose(hFile);
      return;
    }

    // Tell PNG lib the file handle
    // ('cause PNG lib is statically linked)
    png_init_io(png_ptr, hFile);

    // Tell libPNG the number of pre-read bytes
    png_set_sig_bytes(png_ptr, sizeof(pbSig));

    // Read PNG info
    png_read_info(png_ptr, info_ptr);

    // Get width, height and other stuffs
    png_get_IHDR(png_ptr, info_ptr, &ulWidth, &ulHeight, &iBitDepth, &iColorType, NULL, NULL, NULL);

    // Make every kind of image RGBA!
    if (iBitDepth == 16)
      png_set_strip_16(png_ptr);
    if (iColorType == PNG_COLOR_TYPE_PALETTE)
      png_set_expand(png_ptr);
    if (iBitDepth < 8)
      png_set_expand(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
      png_set_expand(png_ptr);
    if ((iColorType == PNG_COLOR_TYPE_GRAY) ||
        (iColorType == PNG_COLOR_TYPE_GRAY_ALPHA))
      png_set_gray_to_rgb(png_ptr);

    // Update info_ptr
    png_read_update_info(png_ptr, info_ptr);

#if 0 //To quiet stderr from outputting "libpng warning: Ignoring extra png_read_update_info() call; row buffer not reallocated"

    // Get width, height and other stuffs again
    png_get_IHDR(png_ptr, info_ptr, &ulWidth, &ulHeight, &iBitDepth, &iColorType, NULL, NULL, NULL);

    // Generate Alpha channel if there is no such thing
    if (iColorType == PNG_COLOR_TYPE_RGB)
      png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);

    // Update info_ptr
    png_read_update_info(png_ptr, info_ptr);

#endif 

    // Get new (final) width, height etc..
    png_get_IHDR(png_ptr, info_ptr, &ulWidth, &ulHeight, &iBitDepth, &iColorType, NULL, NULL, NULL);

    ulRowBytes = png_get_rowbytes(png_ptr, info_ptr);
    ulChannels = png_get_channels(png_ptr, info_ptr);

    // Check if this image fits our needs!
    if ((ulWidth<10) || (ulHeight<10) ||
        (ulWidth>20000) || (ulHeight>20000) ||
        (iBitDepth!=8) ||
        (ulRowBytes==0) || (ulRowBytes>80000) ||
        (ulChannels!=4) || // RGBA!
        (iColorType!=PNG_COLOR_TYPE_RGB_ALPHA))
    {
#ifdef DEBUG_LOGGING
      AddLog("[internal_LoadSkin] : Wrong PNG format.\n");
#endif
      // pImage->bValidImage is already set to FALSE.
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      fclose(hFile);
      return;
    }

    // Oookay, seems to be a good PNG file!
    // Allocate the memory then, and load the image into that!
    pImage->pchPixels = (char *) malloc(ulRowBytes * ulHeight * sizeof(png_byte));
    if (!pImage->pchPixels)
    {
#ifdef DEBUG_LOGGING
      AddLog("[internal_LoadSkin] : Not enough memory for pixels.\n");
#endif
      // pImage->bValidImage is already set to FALSE.
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      fclose(hFile);
      return;
    }

    ppRows = (png_bytepp) malloc(ulHeight * sizeof(png_bytep));
    if (!ppRows)
    {
#ifdef DEBUG_LOGGING
      AddLog("[internal_LoadSkin] : Not enough memory for rows.\n");
#endif
      // pImage->bValidImage is already set to FALSE.
      free(pImage->pchPixels);
      pImage->pchPixels = NULL;
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      fclose(hFile);
      return;
    }

    // Set row pointers
    for (i=0; i<ulHeight; i++)
      ppRows[i] = pImage->pchPixels + i*ulRowBytes;

    // Read the image then!
    png_read_image(png_ptr, ppRows);

    // We're done!
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    free(ppRows); ppRows = NULL;
    fclose(hFile);

    // Set stuffs in image descriptor
    pImage->iImageWidth = ulWidth;
    pImage->iImageHeight = ulHeight;
    pImage->iImagePitch = ulRowBytes;

    // Everything went well, we have a valid image here!
    pImage->bValidImage = TRUE;
  }
}

void internal_LoadSkin()
{
  FILE *hFile;
  char *pchKey;
  char *pchValue;
  int i, bFound;
  char achTemp[CCHMAXPATHCOMP+15];

  if (ModuleConfig.bSimpleClock)
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_LoadSkin] : Module config tells to use Simple clock, so not loading skin.\n");
#endif
    CurrentSkin.bSkinOk = FALSE;
    return;
  }

  memset(&CurrentSkin, 0, sizeof(CurrentSkin));
  snprintf(achTemp, sizeof(achTemp), "%sModules\\PrClock\\%s", ModuleConfig.achHomeDirectory, ModuleConfig.achClockSkin);
  hFile = fopen(achTemp, "rt");
  if (!hFile)
  {
    // Could not open skin file, use simple
    // clock instead!
#ifdef DEBUG_LOGGING
    AddLog("[internal_LoadSkin] : Error opening skin file, using simple clock.\n");
    AddLog(ModuleConfig.achClockSkin); AddLog("\n");
#endif
    CurrentSkin.bSkinOk = FALSE;
    return;
  }

  // Parse the skin file then!
  // Process every entry of the config file
  pchKey = NULL;
  pchValue = NULL;

  while (internal_GetConfigFileEntry(hFile, &pchKey, &pchValue))
  {
    // Found a key/value pair!
    if (!stricmp(pchKey, "Skin_Name"))
      strncpy(CurrentSkin.achSkinName, pchValue, sizeof(CurrentSkin.achSkinName));
    else
    if (!stricmp(pchKey, "Draw_Area_Width"))
      CurrentSkin.iDrawArea_Width = atoi(pchValue);
    else
    if (!stricmp(pchKey, "Draw_Area_Height"))
      CurrentSkin.iDrawArea_Height = atoi(pchValue);
    else
    if (!stricmp(pchKey, "Background_Image"))
      internal_LoadImage(ModuleConfig.achHomeDirectory, pchValue, &(CurrentSkin.BackgroundImage));
    else
    if (!stricmp(pchKey, "Background_Image_Center"))
    {
      sscanf(pchValue, "%d %d %d %d",
             &(CurrentSkin.BackgroundImage.iCenterOffsetX),
             &(CurrentSkin.BackgroundImage.iCenterOffsetY),
             &(CurrentSkin.BackgroundImage.iCenterX),
             &(CurrentSkin.BackgroundImage.iCenterY));
    } else
    if (!stricmp(pchKey, "Foreground_Image"))
      internal_LoadImage(ModuleConfig.achHomeDirectory, pchValue, &(CurrentSkin.ForegroundImage));
    else
    if (!stricmp(pchKey, "Foreground_Image_Center"))
    {
      sscanf(pchValue, "%d %d %d %d",
             &(CurrentSkin.ForegroundImage.iCenterOffsetX),
             &(CurrentSkin.ForegroundImage.iCenterOffsetY),
             &(CurrentSkin.ForegroundImage.iCenterX),
             &(CurrentSkin.ForegroundImage.iCenterY));
    } else
    if (!stricmp(pchKey, "Hour_Analogue_Mode"))
      CurrentSkin.bHourAnalogueMode = atoi(pchValue);
    else
    if (!stricmp(pchKey, "Minute_Analogue_Mode"))
      CurrentSkin.bMinuteAnalogueMode = atoi(pchValue);

    // Now check for hours
    for (i=0; i<24; i++)
    {
      sprintf(achTemp, "Hour_%d_Image", i);
      if (!stricmp(pchKey, achTemp))
        internal_LoadImage(ModuleConfig.achHomeDirectory, pchValue, &(CurrentSkin.aHourImages[i]));

      sprintf(achTemp, "Hour_%d_Image_Center", i);
      if (!stricmp(pchKey, achTemp))
      {
        sscanf(pchValue, "%d %d %d %d",
               &(CurrentSkin.aHourImages[i].iCenterOffsetX),
               &(CurrentSkin.aHourImages[i].iCenterOffsetY),
               &(CurrentSkin.aHourImages[i].iCenterX),
               &(CurrentSkin.aHourImages[i].iCenterY));
      }
    }

    // Now check for Minutes and Seconds
    for (i=0; i<60; i++)
    {
      sprintf(achTemp, "Minute_%d_Image", i);
      if (!stricmp(pchKey, achTemp))
        internal_LoadImage(ModuleConfig.achHomeDirectory, pchValue, &(CurrentSkin.aMinuteImages[i]));

      sprintf(achTemp, "Minute_%d_Image_Center", i);
      if (!stricmp(pchKey, achTemp))
      {
        sscanf(pchValue, "%d %d %d %d",
               &(CurrentSkin.aMinuteImages[i].iCenterOffsetX),
               &(CurrentSkin.aMinuteImages[i].iCenterOffsetY),
               &(CurrentSkin.aMinuteImages[i].iCenterX),
               &(CurrentSkin.aMinuteImages[i].iCenterY));
      }

      sprintf(achTemp, "Second_%d_Image", i);
      if (!stricmp(pchKey, achTemp))
        internal_LoadImage(ModuleConfig.achHomeDirectory, pchValue, &(CurrentSkin.aSecondImages[i]));

      sprintf(achTemp, "Second_%d_Image_Center", i);
      if (!stricmp(pchKey, achTemp))
      {
        sscanf(pchValue, "%d %d %d %d",
               &(CurrentSkin.aSecondImages[i].iCenterOffsetX),
               &(CurrentSkin.aSecondImages[i].iCenterOffsetY),
               &(CurrentSkin.aSecondImages[i].iCenterX),
               &(CurrentSkin.aSecondImages[i].iCenterY));
      }
    }

    free(pchKey); pchKey = NULL;
    if (pchValue)
    {
      free(pchValue); pchValue = NULL;
    }
  }

  fclose(hFile);

  // Okay, seems like we could read through a text file.
  // Let's see if it's a valid skin, if we could read
  // everything we need for a skin!

  // We must have a valid draw area size
  if ((CurrentSkin.iDrawArea_Width <= 0) ||
      (CurrentSkin.iDrawArea_Height <= 0))
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_LoadSkin] : Invalid Draw Area size! Using simple clock.\n");
#endif
    CurrentSkin.bSkinOk = FALSE;
    return;
  }

  // It's not a must to have background and foreground images,
  // so we don't check that.

  // Check the hour first!
  bFound = 0;
  for (i=0; i<24; i++)
    if (CurrentSkin.aHourImages[0].bValidImage)
      bFound = 1;

  if (!bFound)
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_LoadSkin] : No Hour_x_Image! Using simple clock.\n");
#endif
    CurrentSkin.bSkinOk = FALSE;
    return;
  }

  // Check the Minute then!
  bFound = 0;
  for (i=0; i<60; i++)
    if (CurrentSkin.aMinuteImages[0].bValidImage)
      bFound = 1;

  if (!bFound)
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_LoadSkin] : No Minute_x_Image! Using simple clock.\n");
#endif
    CurrentSkin.bSkinOk = FALSE;
    return;
  }

  // Check the Second then!
  bFound = 0;
  for (i=0; i<60; i++)
    if (CurrentSkin.aSecondImages[0].bValidImage)
      bFound = 1;

  if (!bFound)
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_LoadSkin] : No Second_x_Image! Using simple clock.\n");
#endif
    CurrentSkin.bSkinOk = FALSE;
    return;
  }

  // Okay, all the images and settings seem to be ok.
  // Skin loaded.
#ifdef DEBUG_LOGGING
  AddLog("[internal_LoadSkin] : Skin loaded!\n");
#endif
  CurrentSkin.bSkinOk = TRUE;

  // Allocate memory for skin drawing buffer
  CurrentSkin.pchDrawBuffer = (unsigned char *) malloc(CurrentSkin.iDrawArea_Height * CurrentSkin.iDrawArea_Width * 4);
#ifdef DEBUG_LOGGING
  if (!CurrentSkin.pchDrawBuffer)
    AddLog("[internal_LoadSkin] : Out of memory, falling back to simple clock!\n");
#endif
}

void internal_FreeSkin()
{
  int i;

  // Free foreground and background images

  internal_FreeImage(&(CurrentSkin.BackgroundImage));
  internal_FreeImage(&(CurrentSkin.ForegroundImage));

  for (i=0; i<24; i++)
    internal_FreeImage(&(CurrentSkin.aHourImages[i]));

  for (i=0; i<60; i++)
  {
    internal_FreeImage(&(CurrentSkin.aMinuteImages[i]));
    internal_FreeImage(&(CurrentSkin.aSecondImages[i]));
  }

  if (CurrentSkin.pchDrawBuffer)
    free(CurrentSkin.pchDrawBuffer);

  // Make sure we won't free these again, if for some
  // reason called twice.
  memset(&CurrentSkin, 0, sizeof(CurrentSkin));
}

void internal_CreateBitmapResource(HWND hwnd)
{
  SIZEL  sizPSSize;
  BITMAPINFOHEADER2 bmih;

  // We'll load the bitmap into a memory device context.

  hdcImage = hpsImage = hbmImage = NULL;

  // Create memory DC for bitmap
  hdcImage = DevOpenDC(WinQueryAnchorBlock(hwnd),
                       OD_MEMORY,
                       "*",
                       0L,
                       NULL,
                       NULLHANDLE);

  if (!hdcImage)
    return;


  // Create PS
  sizPSSize.cx = (int) WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
  sizPSSize.cy = (int) WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);

  hpsImage = GpiCreatePS(WinQueryAnchorBlock(hwnd),
                         hdcImage,
                         &sizPSSize,
                         PU_PELS | GPIT_NORMAL | GPIA_ASSOC);

  if (!hpsImage)
  {
    DevCloseDC(hdcImage); hdcImage = NULL;
    return;
  }

  // Create the bitmap
  memset(&bmih, 0, sizeof(BITMAPINFOHEADER2));
  bmih.cbFix = sizeof(BITMAPINFOHEADER2);
  bmih.cx = CurrentSkin.iDrawArea_Width;
  bmih.cy = CurrentSkin.iDrawArea_Height;
  bmih.cPlanes = 1;
  bmih.cBitCount = 32;
  hbmImage = GpiCreateBitmap(hpsImage,
                             &bmih,
                             0,
                             NULL, NULL);
  if (hbmImage)
    // Set the bitmap into the presentation space!
    GpiSetBitmap(hpsImage, hbmImage);
}

void internal_DestroyBitmapResource()
{
  if (hpsImage)
    GpiSetBitmap(hpsImage, NULLHANDLE);
  if (hbmImage)
  {
    GpiDeleteBitmap(hbmImage);
    hbmImage = NULLHANDLE;
  }
  if (hpsImage)
  {
    GpiDestroyPS(hpsImage); hpsImage = NULL;
  }
  if (hdcImage)
    DevCloseDC(hdcImage);
}

void internal_CalculateDestImageSize(HWND hwnd)
{
  BITMAPINFOHEADER2 bmpinfo;
  SWP swpWindow;

  if (hbmImage)
  {
    // Get bitmap dimension
    bmpinfo.cbFix = sizeof(bmpinfo);
    GpiQueryBitmapInfoHeader(hbmImage, &bmpinfo);
    sizImageSize.cx = bmpinfo.cx;
    sizImageSize.cy = bmpinfo.cy;

    if (!bOnlyPreviewMode)
    {
      // Fullscreen saving, we won't stretch the bitmap!
      sizImageDestSize.cx = bmpinfo.cx;
      sizImageDestSize.cy = bmpinfo.cy;
    } else
    {
      // Preview window, so calculate how small destination image we need.
      WinQueryWindowPos(hwnd, &swpWindow);
      sizImageDestSize.cx = ((long long) bmpinfo.cx) * swpWindow.cx / (int) WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
      sizImageDestSize.cy = ((long long) bmpinfo.cy) * swpWindow.cy / (int) WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
    }
  }
}

void internal_MakeRandomImagePosition(HWND hwnd)
{
  int iScreenWidth = (int) WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
  int iScreenHeight = (int) WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
  BITMAPINFOHEADER2 bmpinfo;

  if (hbmImage)
  {
    // Get bitmap dimension
    bmpinfo.cbFix = sizeof(bmpinfo);
    GpiQueryBitmapInfoHeader(hbmImage, &bmpinfo);

    // Make sure the image will never be out of screen
    iScreenWidth -= bmpinfo.cx;
    iScreenHeight -= bmpinfo.cy;
  }

  // Random start position
  ptlImageDestPos.x = rand() * iScreenWidth / RAND_MAX;
  ptlImageDestPos.y = rand() * iScreenHeight / RAND_MAX;

  // Random start speed
  ptlImageDestSpeed.x = rand() * MAX_IMAGE_STEP_IN_PIXELS / RAND_MAX + 1;
  ptlImageDestSpeed.y = rand() * MAX_IMAGE_STEP_IN_PIXELS / RAND_MAX + 1;
  if (rand()>RAND_MAX/2)
    ptlImageDestSpeed.x = -ptlImageDestSpeed.x;
  if (rand()>RAND_MAX/2)
    ptlImageDestSpeed.y = -ptlImageDestSpeed.y;
}

void internal_CreateSimpleClock()
{
  RECTL rclRect;
  ARCPARAMS arcp = {1, 1, 0, 0};
  POINTL ptl, ptlEnd;
  time_t Time;
  struct tm *pTime;
  int i, iLen;
  double dAlphaRad;

#ifdef DEBUG_LOGGING
//  AddLog("internal_CreateSimpleClock : GpiErase\n");
#endif

  Time = time(NULL);
  pTime = localtime(&Time);

  // Erase area
//  GpiSetBackColor(hpsImage, CLR_BLACK);
//  GpiErase(hpsImage);
  rclRect.xLeft = 0;
  rclRect.xRight = CurrentSkin.iDrawArea_Width;
  rclRect.yTop = CurrentSkin.iDrawArea_Height;
  rclRect.yBottom = 0;
  WinFillRect(hpsImage, &rclRect, CLR_BLACK);

#ifdef DEBUG_LOGGING
//  {
//    char achTemp[256];
//    sprintf(achTemp, "Draw circle\n");
//    AddLog(achTemp);
//  }
#endif

  // Set line type we'll use
  GpiSetLineType(hpsImage, LINETYPE_SOLID);
  // Draw the tick marks
  for (i=0; i<60; i++)
  {
    dAlphaRad = i*6 * M_PI/180.0;
    ptl.x = CurrentSkin.iDrawArea_Width/2 + (int) floor(CurrentSkin.iDrawArea_Width/2 * sin(dAlphaRad));
    ptl.y = CurrentSkin.iDrawArea_Width/2 + (int) floor(CurrentSkin.iDrawArea_Width/2 * cos(dAlphaRad));

    GpiSetColor(hpsImage, CLR_WHITE);
    if (i%5)
    {
      iLen = CurrentSkin.iDrawArea_Width/2/20;
      GpiSetLineWidth(hpsImage, LINEWIDTH_NORMAL);
    }
    else
    {
      iLen = CurrentSkin.iDrawArea_Width/2/20*2;
      GpiSetLineWidth(hpsImage, LINEWIDTH_THICK);
    }
    GpiMove(hpsImage, &ptl);

    ptl.x = CurrentSkin.iDrawArea_Width/2 + (int) floor((CurrentSkin.iDrawArea_Width/2-iLen) * sin(dAlphaRad));
    ptl.y = CurrentSkin.iDrawArea_Width/2 + (int) floor((CurrentSkin.iDrawArea_Width/2-iLen) * cos(dAlphaRad));
    GpiLine(hpsImage, &ptl);
  }

  // Draw the Hour stuff
  // Draw a circle
  ptl.x = CurrentSkin.iDrawArea_Width/2;
  ptl.y = CurrentSkin.iDrawArea_Height/2;
  GpiSetArcParams(hpsImage, &arcp);
  GpiSetLineType(hpsImage, LINETYPE_SOLID);
  GpiSetLineWidth(hpsImage, LINEWIDTH_THICK);
  GpiSetColor(hpsImage, CLR_WHITE);
  GpiMove(hpsImage, &ptl);
  GpiFullArc(hpsImage, DRO_OUTLINEFILL, MAKEFIXED(CurrentSkin.iDrawArea_Width/60, 0));

  dAlphaRad = (pTime->tm_hour%12 * 30 + pTime->tm_min%60*0.5) * M_PI / 180.0;
  ptlEnd.x = ptl.x + (int) floor(CurrentSkin.iDrawArea_Width/2/10*6 * sin(dAlphaRad));
  ptlEnd.y = ptl.y + (int) floor(CurrentSkin.iDrawArea_Width/2/10*6 * cos(dAlphaRad));
  GpiLine(hpsImage, &ptlEnd);

  // Draw the minute stuff
  // Draw a circle
  ptl.x = CurrentSkin.iDrawArea_Width/2;
  ptl.y = CurrentSkin.iDrawArea_Height/2;
  GpiSetArcParams(hpsImage, &arcp);
  GpiSetLineType(hpsImage, LINETYPE_SOLID);
  GpiSetLineWidth(hpsImage, LINEWIDTH_THICK);
  GpiSetColor(hpsImage, CLR_WHITE);
  GpiMove(hpsImage, &ptl);
  GpiFullArc(hpsImage, DRO_OUTLINEFILL, MAKEFIXED(CurrentSkin.iDrawArea_Width/60, 0));

  dAlphaRad = (pTime->tm_min%60 * 6 + pTime->tm_sec%60/10.0) * M_PI / 180.0;
  ptlEnd.x = ptl.x + (int) floor(CurrentSkin.iDrawArea_Width/2/10*8 * sin(dAlphaRad));
  ptlEnd.y = ptl.y + (int) floor(CurrentSkin.iDrawArea_Width/2/10*8 * cos(dAlphaRad));
  GpiLine(hpsImage, &ptlEnd);

  // Draw the second stuff
  // Draw a circle
  ptl.x = CurrentSkin.iDrawArea_Width/2;
  ptl.y = CurrentSkin.iDrawArea_Height/2;
  GpiSetArcParams(hpsImage, &arcp);
  GpiSetLineType(hpsImage, LINETYPE_SOLID);
  GpiSetLineWidth(hpsImage, LINEWIDTH_THICK);
  GpiSetColor(hpsImage, CLR_RED);
  GpiMove(hpsImage, &ptl);
  GpiFullArc(hpsImage, DRO_OUTLINEFILL, MAKEFIXED(CurrentSkin.iDrawArea_Width/80, 0));

  dAlphaRad = pTime->tm_sec%60 * 6 * M_PI / 180.0;
  ptlEnd.x = ptl.x + (int) floor(CurrentSkin.iDrawArea_Width/2/10*8 * sin(dAlphaRad));
  ptlEnd.y = ptl.y + (int) floor(CurrentSkin.iDrawArea_Width/2/10*8 * cos(dAlphaRad));
  GpiLine(hpsImage, &ptlEnd);
}

// Define the following to use C code.
// Otherwise a similar Assembly block is used.
//#define USE_PLAIN_C_CODE

void internal_BlitImageToDrawArea(unsigned char *pchBuffer, Image_p pImage)
{
  int iShiftX, iShiftY;
  int iY, iXFrom, iXTo, iYFrom, iYTo;
  int iDestPitch;
  unsigned char *pchSrc;
  unsigned char *pchDst;
#ifdef USE_PLAIN_C_CODE
  int iX, iAlpha, iInvAlpha;
  unsigned int uiR, uiG, uiB;
#endif

  if ((pImage) && (pImage->bValidImage))
  {
    // Calculate how much pixels we have to shift to left and down!
    iShiftX = pImage->iCenterOffsetX - pImage->iCenterX;
    iShiftY = pImage->iCenterOffsetY - pImage->iCenterY;

    // Now go through the pixels, and blend them into the
    // output buffer!
    iDestPitch = CurrentSkin.iDrawArea_Width * 4;

    // Calculate the area we have to draw
    iXFrom = 0; iXTo = pImage->iImageWidth-1;
    if (iXFrom+iShiftX<0)
      iXFrom = 0-iShiftX;
    if (iXFrom+iShiftX>=CurrentSkin.iDrawArea_Width)
    {
#ifdef DEBUG_LOGGING
      AddLog("Blit: Image not in drawarea (@1)\n");
#endif
      return; // Not in the drawarea!
    }
    if (iXTo+iShiftX<0)
    {
#ifdef DEBUG_LOGGING
      AddLog("Blit: Image not in drawarea (@2)\n");
#endif
      return; // Not in the drawarea!
    }
    if (iXTo+iShiftX>=CurrentSkin.iDrawArea_Width)
      iXTo = CurrentSkin.iDrawArea_Width-1-iShiftX;

    iYFrom = 0; iYTo = pImage->iImageHeight-1;
    if (iYFrom+iShiftY<0)
      iYFrom = 0-iShiftY;
    if (iYFrom+iShiftY>=CurrentSkin.iDrawArea_Height)
    {
#ifdef DEBUG_LOGGING
      AddLog("Blit: Image not in drawarea (@3)\n");
#endif
      return; // Not in the drawarea!
    }
    if (iYTo+iShiftY<0)
    {
#ifdef DEBUG_LOGGING
      AddLog("Blit: Image not in drawarea (@4)\n");
#endif
      return; // Not in the drawarea!
    }
    if (iYTo+iShiftY>=CurrentSkin.iDrawArea_Height)
      iYTo = CurrentSkin.iDrawArea_Height-1-iShiftY;

#ifdef DEBUG_LOGGING
    {
      char achTemp[256];
      sprintf(achTemp, "X: %d -> %d  Y: %d -> %d (SH %d;%d)\n", iXFrom, iXTo, iYFrom, iYTo, iShiftX, iShiftY);
      AddLog("Blit: Image is in drawarea\n");
      AddLog(achTemp);
    }
#endif

    for (iY = iYFrom; iY<=iYTo; iY++)
    {
      pchSrc = pImage->pchPixels + iY * pImage->iImagePitch + iXFrom*4;
      pchDst = pchBuffer + (iY+iShiftY) * iDestPitch + (iXFrom+iShiftX)*4;

#ifdef USE_PLAIN_C_CODE
      for (iX = iXFrom; iX<=iXTo; iX++)
      {
        // Get alpha
        iAlpha = pchSrc[3];
        iInvAlpha = 255-iAlpha;
        // Get source pixel
        uiR = ((unsigned int) (*(pchSrc++)))*iAlpha;
        uiG = ((unsigned int) (*(pchSrc++)))*iAlpha;
        uiB = ((unsigned int) (*(pchSrc++)))*iAlpha;
        // Skip alpha
        pchSrc++;

        // Blend them!
        // B
        *(pchDst++) = (((unsigned int) (*(pchDst))) * iInvAlpha + uiB) / 255;
        // G
        *(pchDst++) = (((unsigned int) (*(pchDst))) * iInvAlpha + uiG) / 255;
        // R
        *(pchDst++) = (((unsigned int) (*(pchDst))) * iInvAlpha + uiR) / 255;
        // Skip alpha
        pchDst++;
      }
#else
      _asm
      {
	  push eax
	  push ebx
	  push ecx
	  push edx
	  push esi
	  push edi

	  mov esi, pchSrc
          mov edi, pchDst

	  mov ecx, iXTo
	  sub ecx, iXFrom
	  inc ecx

	loopback:

	  mov al, byte ptr [esi+3]
	  mov bl, 255
	  sub bl, al

	  mov bh, al

	  // bh = Alpha
          // bl = 255-Alpha

	  // Red
	  mov al, byte ptr [esi]
	  mul bh
	  mov dx, ax
	  // dx = srcRed * iAlpha

	  mov al, byte ptr [edi+2]
	  mul bl
          // ax = dstRed * (255-Alpha)

	  add ax, dx
          // ax = srcRed*iAlpha + dstRed*(255-iAlpha)

          mov dl, 255
	  div dl
	  mov byte ptr [edi+2], al

	  // Green
	  mov al, byte ptr [esi+1]
	  mul bh
	  mov dx, ax
	  // dx = srcRed * iAlpha

	  mov al, byte ptr [edi+1]
	  mul bl
          // ax = dstRed * (255-Alpha)

	  add ax, dx
          // ax = srcRed*iAlpha + dstRed*(255-iAlpha)

          mov dl, 255
	  div dl
	  mov byte ptr [edi+1], al

	  // Blue
	  mov al, byte ptr [esi+2]
	  mul bh
	  mov dx, ax
	  // dx = srcRed * iAlpha

	  mov al, byte ptr [edi]
	  mul bl
          // ax = dstRed * (255-Alpha)

	  add ax, dx
          // ax = srcRed*iAlpha + dstRed*(255-iAlpha)

          mov dl, 255
	  div dl
	  mov byte ptr [edi], al

	  add esi,4
          add edi,4

	  dec ecx
          jnz loopback

	  pop edi
	  pop esi
	  pop edx
	  pop ecx
          pop eax
      }
#endif
    }
  }
}

inline void internal_RotatePoint(int iX, int iY, int iRotAngle, int iCenterX, int iCenterY, int *piRotX, int *piRotY)
{
  int iCosAngle, iSinAngle;
  int iXC, iYC;

  iCosAngle = aiCosTable[iRotAngle];
  iSinAngle = aiSinTable[iRotAngle];
  iXC = iX - iCenterX;
  iYC = iY - iCenterY;
  *piRotX = iXC * iCosAngle + iYC * iSinAngle + iCenterX*100;
  *piRotY = iYC * iCosAngle - iXC * iSinAngle + iCenterY*100;
}

void internal_BlitRotatedImageToDrawArea(unsigned char *pchBuffer, Image_p pImage, int iRotAngle)
{
  int iDestPitch;
//  int iSrc;
  unsigned char *pchDst;
  unsigned char *pchSrc;
  int iAlpha, iInvAlpha, iR, iG, iB;
  int iShiftX, iShiftY;
  int iBX1, iBX2, iBY1, iBY2; // Bounding box coordinates
  int iX, iY, iImgX, iImgY;
  int iXFrom, iXTo;
  int iX100, iY100, iX100Rem, iY100Rem;
  int iCenterX, iCenterY, iCenterX100, iCenterY100;
  int iWeight;
  int bTop, bRight;
  int iCosAngle, iSinAngle;
  int iXC, iYC;
  

  iCenterX = pImage->iCenterX;
  iCenterY = pImage->iCenterY;
  iCenterX100 = iCenterX*100;
  iCenterY100 = iCenterY*100;

  // Normalize rotation angle
  while (iRotAngle<0) iRotAngle+=36000;
  while (iRotAngle>=36000) iRotAngle-=36000;

  // Initialize bounding box
  iBX1 = 0;
  iBY1 = 0;
  iBX2 = pImage->iImageWidth-1;
  iBY2 = pImage->iImageHeight-1;

  // Rotate corners, and let's see how the bounding box changes!
  internal_RotatePoint(0, 0, 36000-iRotAngle, iCenterX, iCenterY, &iX100, &iY100);
  iX = iX100/100; iY = iY100/100;
  if (iX<iBX1) iBX1 = iX;
  if (iX>iBX2) iBX2 = iX;
  if (iY<iBY1) iBY1 = iY;
  if (iY>iBY2) iBY2 = iY;

  internal_RotatePoint(0, pImage->iImageHeight-1, 36000-iRotAngle, iCenterX, iCenterY, &iX100, &iY100);
  iX = iX100/100; iY = iY100/100;
  if (iX<iBX1) iBX1 = iX;
  if (iX>iBX2) iBX2 = iX;
  if (iY<iBY1) iBY1 = iY;
  if (iY>iBY2) iBY2 = iY;

  internal_RotatePoint(pImage->iImageWidth-1, 0, 36000-iRotAngle, iCenterX, iCenterY, &iX100, &iY100);
  iX = iX100/100; iY = iY100/100;
  if (iX<iBX1) iBX1 = iX;
  if (iX>iBX2) iBX2 = iX;
  if (iY<iBY1) iBY1 = iY;
  if (iY>iBY2) iBY2 = iY;

  internal_RotatePoint(pImage->iImageWidth-1, pImage->iImageHeight-1, 36000-iRotAngle, iCenterX, iCenterY, &iX100, &iY100);
  iX = iX100/100; iY = iY100/100;
  if (iX<iBX1) iBX1 = iX;
  if (iX>iBX2) iBX2 = iX;
  if (iY<iBY1) iBY1 = iY;
  if (iY>iBY2) iBY2 = iY;

#ifdef DEBUG_LOGGING
  /*
  {
    char achTemp[128];
    sprintf(achTemp, "[internal_BlitRotated] : Bounding box: %d;%d -> %d;%d\n", iBX1, iBY1, iBX2, iBY2);
    AddLog(achTemp);
  }
  */
#endif


  // Okay, now we have a bounding box in where the rotated image will fit for sure!
  // Now go through the scanlines of this bounding box, and calculate the pixel
  // for every one of them.

  iDestPitch = CurrentSkin.iDrawArea_Width * 4;
  iShiftX = pImage->iCenterOffsetX - iCenterX;
  iShiftY = pImage->iCenterOffsetY - iCenterY;

#ifdef DEBUG_LOGGING
  /*
  {
    char achTemp[128];
    sprintf(achTemp, "[internal_BlitRotated] : Shift: %d;%d\n", iShiftX, iShiftY);
    AddLog(achTemp);
  }
  */
#endif

  for (iY = iBY1; iY<=iBY2; iY++)
  {
    if ((iY+iShiftY>=0) && (iY+iShiftY<CurrentSkin.iDrawArea_Height))
    {

      iXFrom = iBX1;
      iXTo = iBX2;

      if (iXFrom+iShiftX<0)
        iXFrom = 0-iShiftX;
      if (iXTo+iShiftX >= CurrentSkin.iDrawArea_Width)
        iXTo = CurrentSkin.iDrawArea_Width-1-iShiftX;

      if ((iXFrom+iShiftX<CurrentSkin.iDrawArea_Width) &&
          (iXTo+iShiftX>=0))
      {
        // In the limits!
        pchDst = pchBuffer + (iY+iShiftY) * iDestPitch + (iXFrom+iShiftX)*4;
        for (iX = iXFrom; iX<=iXTo; iX++)
        {
          // Check if the source is in the source image, and
          // if the destination is in the destination image!

          // Rotate (was a call to internal_RotatePoint()):
          // The rotation has been moved here from the function so
          // it's faster.
          iCosAngle = aiCosTable[iRotAngle];
          iSinAngle = aiSinTable[iRotAngle];
          iXC = iX - iCenterX;
          iYC = iY - iCenterY;
          iX100 = iXC * iCosAngle + iYC * iSinAngle + iCenterX100;
          iY100 = iYC * iCosAngle - iXC * iSinAngle + iCenterY100;

          iImgX = iX100/100; iImgY = iY100/100;

          if ((iImgX>=0) && (iImgX<pImage->iImageWidth) &&
              (iImgY>=0) && (iImgY<pImage->iImageHeight))
          {
            // Okay, we're in limits, so
            // get a pixel from four other pixels (using weighted interpolation),
            // and put it into destination!

            // Make difference from middle of pixel
            iX100 = iX100-iImgX*100-50;
            iY100 = iY100-iImgY*100-50;

            // Check which neighbours will be used
            if (iX100 < 0)
              bRight=0;
            else
              bRight=1;
            if (iY100 < 0)
              bTop=1;
            else
              bTop=0;

            // Make distance positive
            iX100 = abs(iX100);
            iY100 = abs(iY100);
            iX100Rem = 100-iX100;
            iY100Rem = 100-iY100;

            // Set source pointer
            pchSrc = pImage->pchPixels+iImgY * pImage->iImagePitch + iImgX*4;

            // Current pixel:

            // Get Alpha, R, G and B
            iWeight = iX100Rem * iY100Rem;
            iR = pchSrc[0] * iWeight;
            iG = pchSrc[1] * iWeight;
            iB = pchSrc[2] * iWeight;
            iAlpha = pchSrc[3] * iWeight;

            // Top or Bottom pixel
            iWeight = iX100Rem * iY100;
            if (bTop)
            {
              if (iImgY>=1)
              {
                // There is pixel there
                iAlpha += pchSrc[0-pImage->iImagePitch+3] * iWeight;
                iR += pchSrc[0-pImage->iImagePitch+0] * iWeight;
                iG += pchSrc[0-pImage->iImagePitch+1] * iWeight;
                iB += pchSrc[0-pImage->iImagePitch+2] * iWeight;
              }
            } else
            {
              if (iImgY<pImage->iImageHeight-1)
              {
                // There is pixel there
                iAlpha += pchSrc[0+pImage->iImagePitch+3] * iWeight;
                iR += pchSrc[0+pImage->iImagePitch+0] * iWeight;
                iG += pchSrc[0+pImage->iImagePitch+1] * iWeight;
                iB += pchSrc[0+pImage->iImagePitch+2] * iWeight;
              }
            }
            // Left or Right pixel
            iWeight = iX100 * iY100Rem;
            if (bRight)
            {
              if (iImgX<pImage->iImageWidth-1)
              {
                // There is pixel there
                iAlpha += pchSrc[0+4+3] * iWeight;
                iR += pchSrc[0+4+0] * iWeight;
                iG += pchSrc[0+4+1] * iWeight;
                iB += pchSrc[0+4+2] * iWeight;
              }
            } else
            {
              if (iImgX>=1)
              {
                // There is pixel there
                iAlpha += pchSrc[0-4+3] * iWeight;
                iR += pchSrc[0-4+0] * iWeight;
                iG += pchSrc[0-4+1] * iWeight;
                iB += pchSrc[0-4+2] * iWeight;
              }
            }
            // Pixel in corner
            iWeight = iX100 * iY100;
            if (bRight)
            {
              if (bTop)
              {
                // Top right
                if ((iImgX<pImage->iImageWidth-1) && (iImgY>=1))
                {
                  // There is pixel there
                  iAlpha += pchSrc[0+4-pImage->iImagePitch+3] * iWeight;
                  iR += pchSrc[0+4-pImage->iImagePitch+0] * iWeight;
                  iG += pchSrc[0+4-pImage->iImagePitch+1] * iWeight;
                  iB += pchSrc[0+4-pImage->iImagePitch+2] * iWeight;
                }
              } else
              {
                // Bottom right
                if ((iImgX<pImage->iImageWidth-1) && (iImgY<pImage->iImageHeight-1))
                {
                  // There is pixel there
                  iAlpha += pchSrc[0+4+pImage->iImagePitch+3] * iWeight;
                  iR += pchSrc[0+4+pImage->iImagePitch+0] * iWeight;
                  iG += pchSrc[0+4+pImage->iImagePitch+1] * iWeight;
                  iB += pchSrc[0+4+pImage->iImagePitch+2] * iWeight;
                }
              }
            } else
            {
              if (bTop)
              {
                // Top left
                if ((iImgX>=1) && (iImgY>=1))
                {
                  // There is pixel there
                  iAlpha += pchSrc[0-4-pImage->iImagePitch+3] * iWeight;
                  iR += pchSrc[0-4-pImage->iImagePitch+0] * iWeight;
                  iG += pchSrc[0-4-pImage->iImagePitch+1] * iWeight;
                  iB += pchSrc[0-4-pImage->iImagePitch+2] * iWeight;
                }
              } else
              {
                // Bottom left
                if ((iImgX>=1) && (iImgY<pImage->iImageHeight-1))
                {
                  // There is pixel there
                  iAlpha += pchSrc[0-4+pImage->iImagePitch+3] * iWeight;
                  iR += pchSrc[0-4+pImage->iImagePitch+0] * iWeight;
                  iG += pchSrc[0-4+pImage->iImagePitch+1] * iWeight;
                  iB += pchSrc[0-4+pImage->iImagePitch+2] * iWeight;
                }
              }
            }
  
            // Store destination pixel, blended to background
            iInvAlpha = 2550000 - iAlpha;
            iB/=10000;
            iG/=10000;
            iR/=10000;
            // B
            *(pchDst++) =
              (
               (*pchDst)*iInvAlpha +
               iB*iAlpha
              ) / 2550000;
            // G
            *(pchDst++) =
              (
               (*pchDst)*iInvAlpha +
               iG*iAlpha
              ) / 2550000;
            // R
            *(pchDst++) =
              (
               (*pchDst)*iInvAlpha +
               iR*iAlpha
              ) / 2550000;
            // Skip Alpha channel data
            pchDst++;
          } else
            pchDst+=4;
        }
      }
    }
  }
}




void internal_BlitHourMarkerToDrawArea(unsigned char *pchBuffer, struct tm *pTime)
{
  int i, bAllAvailable;
  Image_p pImage;
  int iDiff;
  int iRotAngle;

  // Let's see if we have all the hour images!
  bAllAvailable = 1;
  for (i=0; i<24; i++)
    if (!CurrentSkin.aHourImages[i].bValidImage)
    {
      bAllAvailable = 0;
      break;
    }

  // Find the image we need for this hour mark!
  iRotAngle = 0;

  if (bAllAvailable)
  {
    // We have images for all 24 hours, so we switch to
    // 24-hour mode, select the exact image.
    pImage = &(CurrentSkin.aHourImages[(pTime->tm_hour%24)]);
  } else
  {
    // Some images are missing, so old method!
    pImage = &(CurrentSkin.aHourImages[(pTime->tm_hour%12)]);
  }

  if (!pImage->bValidImage)
  {
    // This exact image is not available, find another one then!
    iDiff = 100;
    for (i=0; i<12; i++)
    {
      if ((CurrentSkin.aHourImages[i].bValidImage) &&
          (iDiff>(abs((pTime->tm_hour%12)-i))))
      {
        pImage = &(CurrentSkin.aHourImages[i]);
        iDiff=abs((pTime->tm_hour%12)-i);
        iRotAngle = iDiff * (100 * 360 / 12);
        if ((pTime->tm_hour%12)-i < 0)
          iRotAngle = -iRotAngle;
      }
    }
  }

  if (pImage->bValidImage)
  {
    // Okay, we have an image we have to show!

    // Let's see if we have to rotate it a little bit more!
    // (Analogue mode)
    if (CurrentSkin.bHourAnalogueMode)
    {
      iRotAngle += (pTime->tm_min%60) * (100 * 30 / 60);
      iRotAngle += (pTime->tm_sec%60) * (100 * 30 / 60 / 60);
    }

    // Now blit this image!
    if (iRotAngle==0)
      internal_BlitImageToDrawArea(pchBuffer, pImage);
    else
      internal_BlitRotatedImageToDrawArea(pchBuffer, pImage, iRotAngle);
  }
}

void internal_BlitMinuteMarkerToDrawArea(unsigned char *pchBuffer, struct tm *pTime)
{
  int i;
  Image_p pImage;
  int iDiff;
  int iRotAngle;

  // Find the image we need for this hour mark!
  iRotAngle = 0;
  pImage = &(CurrentSkin.aMinuteImages[(pTime->tm_min%60)]);

  if (!pImage->bValidImage)
  {
    // This exact image is not available, find another one then!
    iDiff = 100;
    for (i=0; i<60; i++)
    {
      if ((CurrentSkin.aMinuteImages[i].bValidImage) &&
          (iDiff>(abs((pTime->tm_min%60)-i))))
      {
        pImage = &(CurrentSkin.aMinuteImages[i]);
        iDiff=abs((pTime->tm_min%60)-i);
        iRotAngle = iDiff * (100 * 360 / 60);
        if ((pTime->tm_min%60)-i < 0)
          iRotAngle = -iRotAngle;
      }
    }
  }

  if (pImage->bValidImage)
  {
    // Okay, we have an image we have to show!

    // Let's see if we have to rotate it a little bit more!
    // (Analogue mode)
    if (CurrentSkin.bMinuteAnalogueMode)
    {
      iRotAngle += (pTime->tm_sec%60) * (100 * 6 / 60);
    }

    // Now blit this image!
    if (iRotAngle==0)
      internal_BlitImageToDrawArea(pchBuffer, pImage);
    else
      internal_BlitRotatedImageToDrawArea(pchBuffer, pImage, iRotAngle);
  }
}

void internal_BlitSecondMarkerToDrawArea(unsigned char *pchBuffer, struct tm *pTime)
{
  int i;
  Image_p pImage;
  int iDiff;
  int iRotAngle;

  // Find the image we need for this hour mark!
  iRotAngle = 0;
  pImage = &(CurrentSkin.aSecondImages[(pTime->tm_sec%60)]);

  if (!pImage->bValidImage)
  {
    // This exact image is not available, find another one then!
    iDiff = 100;
    for (i=0; i<60; i++)
    {
      if ((CurrentSkin.aSecondImages[i].bValidImage) &&
          (iDiff>(abs((pTime->tm_sec%60)-i))))
      {
        pImage = &(CurrentSkin.aSecondImages[i]);
        iDiff=abs((pTime->tm_sec%60)-i);
        iRotAngle = iDiff * (100 * 360 / 60);
        if ((pTime->tm_sec%60)-i < 0)
          iRotAngle = -iRotAngle;
      }
    }
  }

  if (pImage->bValidImage)
  {
    // Okay, we have an image we have to show!

    // Now blit this image!
    if (iRotAngle==0)
      internal_BlitImageToDrawArea(pchBuffer, pImage);
    else
      internal_BlitRotatedImageToDrawArea(pchBuffer, pImage, iRotAngle);
  }
}
void internal_CreatePrettyClock()
{
  int iY;
  unsigned char *pchBuffer;
  BITMAPINFOHEADER2 bmpinfo;
  time_t Time;
  struct tm *pTime;

  // Query current time
  Time = time(NULL);
  pTime = localtime(&Time);

#ifdef DEBUG_LOGGING
//  AddLog("[internal_CreatePrettyClock] : Assembling pretty clock...\n");
#endif

#ifdef DEBUG_LOGGING
//  AddLog("[internal_CreatePrettyClock] : Allocate memory...\n");
#endif
  pchBuffer = CurrentSkin.pchDrawBuffer;
  if (!pchBuffer)
  {
#ifdef DEBUG_LOGGING
//    AddLog("[internal_CreatePrettyClock] : Out of memory, falling back to simple clock!\n");
#endif
    // Redirected to internal_CreateSimpleClock()
    internal_CreateSimpleClock();
    return;
  }

  // Clear the drawarea
#ifdef DEBUG_LOGGING
//  AddLog("[internal_CreatePrettyClock] : Clearing drawarea!\n");
#endif
  memset(pchBuffer, 0, CurrentSkin.iDrawArea_Height * CurrentSkin.iDrawArea_Width * 4);

  // Blit the background bitmap into the draw area
#ifdef DEBUG_LOGGING
//  AddLog("[internal_CreatePrettyClock] : Blitting background\n");
#endif
  internal_BlitImageToDrawArea(pchBuffer, &(CurrentSkin.BackgroundImage));

  // The hour marker
#ifdef DEBUG_LOGGING
//  AddLog("[internal_CreatePrettyClock] : Blitting hour marker\n");
#endif
  internal_BlitHourMarkerToDrawArea(pchBuffer, pTime);

  // The minute marker
#ifdef DEBUG_LOGGING
//  AddLog("[internal_CreatePrettyClock] : Blitting minute marker\n");
#endif
  internal_BlitMinuteMarkerToDrawArea(pchBuffer, pTime);

  // The second marker
#ifdef DEBUG_LOGGING
//  AddLog("[internal_CreatePrettyClock] : Blitting second marker\n");
#endif
  internal_BlitSecondMarkerToDrawArea(pchBuffer, pTime);

  // Blit the foreground bitmap into the draw area
#ifdef DEBUG_LOGGING
//  AddLog("[internal_CreatePrettyClock] : Blitting foreground\n");
#endif
  internal_BlitImageToDrawArea(pchBuffer, &(CurrentSkin.ForegroundImage));

  // Now set the pixels of presentation space to contain the pixels of our buffer!
#ifdef DEBUG_LOGGING
//  AddLog("[internal_CreatePrettyClock] : Copying to HPS\n");
#endif
  // Get bitmap dimension
  bmpinfo.cbFix = sizeof(bmpinfo);
  GpiQueryBitmapInfoHeader(hbmImage, &bmpinfo);

  for (iY=0; iY<CurrentSkin.iDrawArea_Height; iY++)
  {
    // Gpi images are Bottom->Top, our images are Top->Bottom!
    // So, we have to do it line by line
    GpiSetBitmapBits(hpsImage, CurrentSkin.iDrawArea_Height-1-iY, 1, pchBuffer, (BITMAPINFO2 *) &bmpinfo);
    pchBuffer += CurrentSkin.iDrawArea_Width << 2;
  }

  // All done!
}

void internal_PrepareSimpleClock()
{
  int iScreenHeight = (int) WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);

  if (!CurrentSkin.bSkinOk)
  {
    // Set draw-area to be 60% of height of screen!
    CurrentSkin.iDrawArea_Width  = 60*iScreenHeight/100;
    CurrentSkin.iDrawArea_Height = 60*iScreenHeight/100;
#ifdef DEBUG_LOGGING
//    {
//      char achTemp[256];
//      sprintf(achTemp, "PrepareSimpleClock: Size: %d x %d\n", CurrentSkin.iDrawArea_Width, CurrentSkin.iDrawArea_Height);
//      AddLog(achTemp);
//    }
#endif
  }
}

void internal_MakeNewImage(HWND hwnd)
{
  int iScreenWidth = (int) WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
  int iScreenHeight = (int) WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
  BITMAPINFOHEADER2 bmpinfo;

  if (hbmImage)
  {
    // Get bitmap dimension
    bmpinfo.cbFix = sizeof(bmpinfo);
    GpiQueryBitmapInfoHeader(hbmImage, &bmpinfo);

    // Make sure the image will never be out of screen
    iScreenWidth -= bmpinfo.cx;
    iScreenHeight -= bmpinfo.cy;
  }

  // Now update image position, and make new speed if it touches screen sides!
  ptlImageDestPos.x += ptlImageDestSpeed.x;
  ptlImageDestPos.y += ptlImageDestSpeed.y;

  if (ptlImageDestPos.x>=iScreenWidth)
  {
    // Touched right side of screen
    // Move it to side of screen
    ptlImageDestPos.x = iScreenWidth;
    // Give it new X-speed
    ptlImageDestSpeed.x = -(rand() * MAX_IMAGE_STEP_IN_PIXELS / RAND_MAX + 1);
  }

  if (ptlImageDestPos.x<=0)
  {
    // Touched left side of screen
    // Move it to side of screen
    ptlImageDestPos.x = 0;
    // Give it new X-speed
    ptlImageDestSpeed.x = (rand() * MAX_IMAGE_STEP_IN_PIXELS / RAND_MAX + 1);
  }

  if (ptlImageDestPos.y>=iScreenHeight)
  {
    // Touched top of screen
    // Move it to side of screen
    ptlImageDestPos.y = iScreenHeight;
    // Give it new Y-speed
    ptlImageDestSpeed.y = -(rand() * MAX_IMAGE_STEP_IN_PIXELS / RAND_MAX + 1);
  }

  if (ptlImageDestPos.y<=0)
  {
    // Touched bottom or side of screen
    // Move it to side of screen
    ptlImageDestPos.y = 0;
    // Give it new Y-speed
    ptlImageDestSpeed.y = (rand() * MAX_IMAGE_STEP_IN_PIXELS / RAND_MAX + 1);
  }

  // Now, draw the new clock into the presentation space!
  if (!CurrentSkin.bSkinOk)
    internal_CreateSimpleClock();
  else
    internal_CreatePrettyClock();

  bImageValid = TRUE;
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

      // Randomize
      srand(time(NULL));

      // Load skin and its images
      internal_LoadSkin(hwnd);
      // Prepare for simple clock, if the skin could not be loaded
      internal_PrepareSimpleClock();

      // Create HBITMAP based on current settings and skin.
      internal_CreateBitmapResource(hwnd);

      // Initial image position
      internal_MakeRandomImagePosition(hwnd);

      // Make sure the new bitmap will be drawn at the
      // first WM_PAINT message
      bImageValid = FALSE;

      // Start draw timer!
      ulAnimationTimerID = WinStartTimer(WinQueryAnchorBlock(hwnd), hwnd,
					 ANIMATION_TIMER_ID, // Timer ID
					 ANIMATION_TIMER_TIMEOUT_MSEC); // Speed

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

    case WM_SKINCHANGED:
      // The skin has been changed while we were running.
      // Free the old skin and load the new then!

#ifdef DEBUG_LOGGING
      AddLog("[WM_SKINCHANGED] : Unloading old skin and its resources.\n");
#endif
      // Drop bitmap
      internal_DestroyBitmapResource();
      // Free all allocated Skin space
      internal_FreeSkin(hwnd);

#ifdef DEBUG_LOGGING
      AddLog("[WM_SKINCHANGED] : Loading new skin and its resources.\n");
#endif
      // Load skin and its images
      internal_LoadSkin(hwnd);
      // Prepare for simple clock, if the skin could not be loaded
      internal_PrepareSimpleClock();

      // Create HBITMAP based on current settings and skin.
      internal_CreateBitmapResource(hwnd);

      // Initial image position
      internal_MakeRandomImagePosition(hwnd);

#ifdef DEBUG_LOGGING
      AddLog("[WM_SKINCHANGED] : Skin changed.\n");
#endif

      // Make sure the new bitmap will be drawn at the
      // first WM_PAINT message
      bImageValid = FALSE;

      return (MRESULT) FALSE;

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
      if (ulAnimationTimerID)
      {
#ifdef DEBUG_LOGGING
	AddLog("[WM_PAUSESAVING] : Pausing saver module\n");
#endif

	WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd,
		     ulAnimationTimerID);
	ulAnimationTimerID = 0; // Note that we're paused!
      }
      return (MRESULT) SSMODULE_NOERROR;

    case WM_RESUMESAVING:
      // We should resume screen saving
      if (!ulAnimationTimerID)
      {
#ifdef DEBUG_LOGGING
	AddLog("[WM_RESUMESAVING] : Resuming saver module\n");
#endif

	ulAnimationTimerID = WinStartTimer(WinQueryAnchorBlock(hwnd), hwnd,
                                           ANIMATION_TIMER_ID, // Timer ID
                                           ANIMATION_TIMER_TIMEOUT_MSEC); // Speed
      }
      return (MRESULT) SSMODULE_NOERROR;

    case WM_SUBCLASS_UNINIT:
    case WM_DESTROY:
      // All kinds of cleanup (the opposite of everything done in WM_CREATE)
      // should come here.

      // Stop timer
      WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd,
		   ulAnimationTimerID);

      // Drop bitmap
      internal_DestroyBitmapResource();

      // Free all allocated Skin space
      internal_FreeSkin(hwnd);

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
	RECTL rclRect, rclImage, rclWindow;
        SWP swpWindow;

#ifdef DEBUG_LOGGING
//        AddLog("WM_PAINT\n");
#endif

        if (!bImageValid)
          internal_MakeNewImage(hwnd);

	hpsBeginPaint = WinBeginPaint(hwnd, NULLHANDLE, &rclRect);

        WinQueryWindowRect(hwnd, &rclWindow);

	if (hbmImage)
	{
	  if (bOnlyPreviewMode)
	  {
	    WinQueryWindowPos(hwnd, &swpWindow);
	    internal_CalculateDestImageSize(hwnd);
	    rclImage.xLeft = ((long long) ptlImageDestPos.x) * swpWindow.cx / (int) WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
	    rclImage.yBottom = ((long long) ptlImageDestPos.y) * swpWindow.cy / (int) WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
	    rclImage.xRight = rclImage.xLeft + sizImageDestSize.cx;
	    rclImage.yTop = rclImage.yBottom + sizImageDestSize.cy;

            // Fill with black, but leave the place where the new bitmap will be!
            rclRect.xLeft = rclWindow.xLeft;
            rclRect.xRight = rclWindow.xRight;
            rclRect.yTop = rclWindow.yTop;
            rclRect.yBottom = rclImage.yTop;
            WinFillRect(hpsBeginPaint, &rclRect, CLR_BLACK);
            rclRect.xLeft = rclWindow.xLeft;
            rclRect.xRight = rclWindow.xRight;
            rclRect.yTop = rclImage.yBottom;
            rclRect.yBottom = rclWindow.yBottom;
            WinFillRect(hpsBeginPaint, &rclRect, CLR_BLACK);
            rclRect.xLeft = rclWindow.xLeft;
            rclRect.xRight = rclImage.xLeft;
            rclRect.yTop = rclImage.yTop;
            rclRect.yBottom = rclImage.yBottom;
            WinFillRect(hpsBeginPaint, &rclRect, CLR_BLACK);
            rclRect.xLeft = rclImage.xRight;
            rclRect.xRight = rclWindow.xRight;
            rclRect.yTop = rclImage.yTop;
            rclRect.yBottom = rclImage.yBottom;
            WinFillRect(hpsBeginPaint, &rclRect, CLR_BLACK);

            // Then blit the image
            WinDrawBitmap(hpsBeginPaint, hbmImage,
                          NULL,
                          (PPOINTL) (&rclImage),
                          CLR_NEUTRAL, CLR_BACKGROUND,
                          DBM_STRETCH);
	  } else
          {
            internal_CalculateDestImageSize(hwnd);
            rclImage.xLeft = ptlImageDestPos.x;
	    rclImage.yBottom = ptlImageDestPos.y;
	    rclImage.xRight = rclImage.xLeft + sizImageDestSize.cx;
	    rclImage.yTop = rclImage.yBottom + sizImageDestSize.cy;

            // Fill with black, but leave the place where the new bitmap will be!
            rclRect.xLeft = rclWindow.xLeft;
            rclRect.xRight = rclWindow.xRight;
            rclRect.yTop = rclWindow.yTop;
            rclRect.yBottom = rclImage.yTop;
            WinFillRect(hpsBeginPaint, &rclRect, CLR_BLACK);
            rclRect.xLeft = rclWindow.xLeft;
            rclRect.xRight = rclWindow.xRight;
            rclRect.yTop = rclImage.yBottom;
            rclRect.yBottom = rclWindow.yBottom;
            WinFillRect(hpsBeginPaint, &rclRect, CLR_BLACK);
            rclRect.xLeft = rclWindow.xLeft;
            rclRect.xRight = rclImage.xLeft;
            rclRect.yTop = rclImage.yTop;
            rclRect.yBottom = rclImage.yBottom;
            WinFillRect(hpsBeginPaint, &rclRect, CLR_BLACK);
            rclRect.xLeft = rclImage.xRight;
            rclRect.xRight = rclWindow.xRight;
            rclRect.yTop = rclImage.yTop;
            rclRect.yBottom = rclImage.yBottom;
            WinFillRect(hpsBeginPaint, &rclRect, CLR_BLACK);

            // Then blit the image
            WinDrawBitmap(hpsBeginPaint, hbmImage,
                          NULL,
                          &ptlImageDestPos,
                          CLR_NEUTRAL, CLR_BACKGROUND,
                          DBM_NORMAL);
	  }
	}

	WinEndPaint(hpsBeginPaint);
#ifdef DEBUG_LOGGING
//        AddLog("WM_PAINT done\n");
#endif
        return (MRESULT) FALSE;
      }

    case WM_TIMER:
      if (((SHORT)mp1)==ANIMATION_TIMER_ID)
      {
	// Timer, so make new image at a new position
        bImageValid = FALSE;
	WinInvalidateRect(hwnd, NULL, FALSE);
      }
      break;

    default:
      break;
  }

  return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}


void fnSaverThread(void *p)
{
  HWND hwndOldFocus;
  HWND hwndParent = (HWND) p;
  HWND hwndOldSysModal;
  HAB hab;
  QMSG msg;
  ULONG ulStyle;
  int i;

  // Set our thread to slightly more than regular to be able
  // to update the screen fine.
  DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR, +5, 0);

  // Initialize Sine and Cosine tables
  for (i=0; i<36000; i++)
  {
    aiCosTable[i] = floor(100*cos(i/100.0*M_PI/180.0));
    aiSinTable[i] = floor(100*sin(i/100.0*M_PI/180.0));
  }

  hab = WinInitialize(0);
  hmqSaverThreadMsgQueue = WinCreateMsgQueue(hab, 0);

  if (bOnlyPreviewMode)
  {
    PFNWP pfnOldWindowProc;
    // We should run in preview mode, so the hwndParent we have is not the
    // desktop, but a special window we have to subclass.
    pfnOldWindowProc = WinSubclassWindow(hwndParent,
                                         (PFNWP) fnSaverWindowProc);

    // Store the window handle so we can communicate with it from config dialog!
    hwndSaverWindow = hwndParent;

    // Initialize window proc (simulate WM_CREATE)
    WinSendMsg(hwndParent, WM_SUBCLASS_INIT, (MPARAM) NULL, (MPARAM) NULL);
    // Also make sure that the window will be redrawn with this new
    // window proc.
    WinInvalidateRect(hwndParent, NULL, FALSE);

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

    // Uinitialize window proc (simulate WM_DESTROY)
    WinSendMsg(hwndParent, WM_SUBCLASS_UNINIT, (MPARAM) NULL, (MPARAM) NULL);

    // Undo subclassing
    WinSubclassWindow(hwndParent,
		      pfnOldWindowProc);
    hwndSaverWindow = NULLHANDLE;
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
      AddLog("[fnSaverThread] : Entering message loop (Real)\n");
#endif
      // Process messages until WM_QUIT!
      while (WinGetMsg(hab, &msg, 0, 0, 0))
        WinDispatchMsg(hab, &msg);
#ifdef DEBUG_LOGGING
      AddLog("[fnSaverThread] : Leaved message loop (Real)\n");
#endif

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
	sprintf(pchFileName, "%sModules\\PrClock\\%s.msg", pchHomeDirectory, achRealLocaleName);
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
	  sprintf(pchFileName, "%sModules\\PrClock\\%s.msg", pchHomeDirectory, pchLang);
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

static void internal_SetConfigDialogText(HWND hwnd, ConfigDlgInitParams_p pCfgDlgInit)
{
  FILE *hfNLSFile;
  char achTemp[128];

  if (!pCfgDlgInit) return;
  if (!(pCfgDlgInit->pchHomeDirectory)) return;
  hfNLSFile = internal_OpenNLSFile(pCfgDlgInit->pchHomeDirectory);
  if (hfNLSFile)
  {
    // Window title
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0001"))
      WinSetWindowText(hwnd, achTemp);

    // Groupbox text
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0002"))
      WinSetDlgItemText(hwnd, GB_SKINS, achTemp);

    // Help text
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0003"))
      WinSetDlgItemText(hwnd, ST_PLEASESELECTASKINTOUSE, achTemp);

    // OK Button
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0004"))
      WinSetDlgItemText(hwnd, DID_OK, achTemp);

    // Cancel button
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0005"))
      WinSetDlgItemText(hwnd, DID_CANCEL, achTemp);

    internal_CloseNLSFile(hfNLSFile);
  }
}

static void internal_AdjustConfigDialogControls(HWND hwnd)
{
  SWP swpTemp, swpTemp2, swpWindow;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  int staticheight, buttonheight, gbheight;
  int iBtnWidth, iCX, iCY;
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(hwnd))
  {
    WinEnableWindowUpdate(hwnd, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Get window size!
  WinQueryWindowPos(hwnd, &swpWindow);

  // Query static height in pixels. 
  internal_GetStaticTextSize(hwnd, ST_PLEASESELECTASKINTOUSE, &iCX, &iCY); // Autosize!
  staticheight = iCY;

  // Query button height in pixels.
  buttonheight = 3*CYDLGFRAME + iCY;

  // Set the position and size of buttons
  internal_GetStaticTextSize(hwnd, DID_OK, &iCX, &iCY); // Autosize!
  iBtnWidth = iCX;
  internal_GetStaticTextSize(hwnd, DID_CANCEL, &iCX, &iCY); // Autosize!
  if (iBtnWidth<iCX) iBtnWidth = iCX;

  // Meanwhile: let the window size be 5 times the button width!
  swpWindow.cx = iCX*2 + 5*iBtnWidth; // Set dialog window width

  WinSetWindowPos(WinWindowFromID(hwnd, DID_OK), HWND_TOP,
                  CXDLGFRAME*2,
                  CYDLGFRAME*2,
                  6*CXDLGFRAME + iBtnWidth,
		  3*CYDLGFRAME + iCY,
                  SWP_MOVE | SWP_SIZE);

  WinSetWindowPos(WinWindowFromID(hwnd, DID_CANCEL), HWND_TOP,
                  swpWindow.cx - CXDLGFRAME*2 - (6*CXDLGFRAME + iBtnWidth),
                  CYDLGFRAME*2,
                  6*CXDLGFRAME + iBtnWidth,
		  3*CYDLGFRAME + iCY,
                  SWP_MOVE | SWP_SIZE);

  // First set the "Skins" groupbox
  gbheight =
    CYDLGFRAME + // frame
    CYDLGFRAME + // Margin
    2*CYDLGFRAME+10*staticheight + // Listbox
    CYDLGFRAME + // Margin
    staticheight + // Help text
    CYDLGFRAME + // Margin
    staticheight; // Text of groupbox

  WinQueryWindowPos(WinWindowFromID(hwnd, DID_OK), &swpTemp);
  WinSetWindowPos(WinWindowFromID(hwnd, GB_SKINS),
		  HWND_TOP,
                  2*CXDLGFRAME,
                  swpTemp.y + swpTemp.cy + CYDLGFRAME,
                  swpWindow.cx - 4*CXDLGFRAME,
                  gbheight,
                  SWP_MOVE | SWP_SIZE);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SKINS), &swpTemp);

  // Things inside this groupbox
  WinSetWindowPos(WinWindowFromID(hwnd, LB_SKINS), HWND_TOP,
                  swpTemp.x + 2*CXDLGFRAME,
                  swpTemp.y + CYDLGFRAME,
                  swpTemp.cx - 4*CXDLGFRAME,
                  swpTemp.cy - 3*CYDLGFRAME - 2*staticheight,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, LB_SKINS), &swpTemp2);
  internal_GetStaticTextSize(hwnd, ST_PLEASESELECTASKINTOUSE, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, ST_PLEASESELECTASKINTOUSE), HWND_TOP,
                  swpTemp.x + 2*CXDLGFRAME,
                  swpTemp2.y + swpTemp2.cy + CYDLGFRAME,
                  iCX, iCY,
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
  FILE *hfFile;
  char *pchKey, *pchValue;
  char achTemp[128];
  char achNoSkinName[256];
  char achSkinName[256];
  SHORT sIndex;
  char achSearchPattern[CCHMAXPATHCOMP+15];
  char achFileNamePath[CCHMAXPATHCOMP+15];
  HDIR hdirFindHandle;
  FILEFINDBUF3 FindBuffer;
  ULONG ulResultBufLen;
  ULONG ulFindCount;
  APIRET rcfind;

  // Fill the listbox with available skins

  // First make listbox empty!
  WinSendDlgItemMsg(hwnd, LB_SKINS, LM_DELETEALL, (MPARAM) NULL, (MPARAM) NULL);

  // Now add the first special item, the No-Skin item!
  // Also get the "No name" string from NLS file!
  strcpy(achTemp, "<Unskinned>");
  strcpy(achNoSkinName, "<No name>");
  hfFile = internal_OpenNLSFile(pCfgDlgInit->pchHomeDirectory);
  if (hfFile)
  {
    // Text of no skin
    sprintmsg(achTemp, hfFile, "CFG_0006");
    // Text to show when there is no name of skin defined in skin file
    sprintmsg(achNoSkinName, hfFile, "CFG_0007");

    internal_CloseNLSFile(hfFile);
  }

  sIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_SKINS, LM_INSERTITEM, (MPARAM) LIT_END, (MPARAM) achTemp);
  WinSendDlgItemMsg(hwnd, LB_SKINS, LM_SETITEMHANDLE, (MPARAM) sIndex, (MPARAM) NULL);
  if (pCfgDlgInit->ModuleConfig.bSimpleClock)
    WinSendDlgItemMsg(hwnd, LB_SKINS, LM_SELECTITEM, (MPARAM) sIndex, (MPARAM) TRUE);

  // Now go through the Modules\PrClock directory and look for *.skn files!
  hdirFindHandle = HDIR_CREATE;
  ulResultBufLen = sizeof(FindBuffer);
  ulFindCount = 1;

  snprintf(achSearchPattern, sizeof(achSearchPattern), "%sModules\\PrClock\\*.SKN", pCfgDlgInit->pchHomeDirectory);

  rcfind = DosFindFirst(achSearchPattern, // File pattern
                        &hdirFindHandle,  // Search handle
                        FILE_NORMAL,      // Search attribute
                        &FindBuffer,      // Result buffer
                        ulResultBufLen,   // Result buffer length
                        &ulFindCount,     // Number of entries to find
                        FIL_STANDARD);    // Return level 1 file info

  if (rcfind == NO_ERROR)
  {
    do {
      sprintf(achFileNamePath, "%sModules\\PrClock\\%s",pCfgDlgInit->pchHomeDirectory, FindBuffer.achName);

      // Try to open this skin file and get the skin description string from it!
#ifdef DEBUG_LOGGING
      AddLog("[internal_SetConfigDialogInitialControlValues] : Found SKN file: [");AddLog(achFileNamePath);AddLog("]\n");
#endif

      hfFile = fopen(achFileNamePath, "rt");
      if (hfFile)
      {
        // Good, we could open this Skin file!
        // Go through all its config entries, and look for the Skin_Name!

        // Default is No skin!
        strcpy(achSkinName, achNoSkinName);

        // Process every entry of the skin file
        pchKey = NULL;
        pchValue = NULL;

        while (internal_GetConfigFileEntry(hfFile, &pchKey, &pchValue))
        {
          // Found a key/value pair!
          if ((!stricmp(pchKey, "Skin_Name")) && (pchValue))
            strncpy(achSkinName, pchValue, sizeof(achSkinName));

          free(pchKey); pchKey = NULL;
          if (pchValue)
          {
            free(pchValue); pchValue = NULL;
          }
        }

        fclose(hfFile);
      }

      // Add this skin item into the listbox!
      sIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_SKINS, LM_INSERTITEM, (MPARAM) LIT_END, (MPARAM) achSkinName);
      pchValue = (char *) malloc(strlen(FindBuffer.achName)+1);
      if (pchValue)
        strcpy(pchValue, FindBuffer.achName);
      WinSendDlgItemMsg(hwnd, LB_SKINS, LM_SETITEMHANDLE, (MPARAM) sIndex, (MPARAM) pchValue);
      if (!stricmp(pCfgDlgInit->ModuleConfig.achClockSkin, FindBuffer.achName))
        WinSendDlgItemMsg(hwnd, LB_SKINS, LM_SELECTITEM, (MPARAM) sIndex, (MPARAM) TRUE);

      // Go for the next one!

      ulFindCount = 1; // Reset find count

      rcfind = DosFindNext( hdirFindHandle,  // Find handle
                            &FindBuffer,     // Result buffer
                            ulResultBufLen,  // Result buffer length
                            &ulFindCount);   // Number of entries to find
    } while (rcfind == NO_ERROR);

    // Close directory search handle
    rcfind = DosFindClose(hdirFindHandle);
  }
}

static void internal_FreeConfigDialogListboxMemory(HWND hwnd)
{
  char *pchText;

  while (0<(SHORT) WinSendDlgItemMsg(hwnd, LB_SKINS, LM_QUERYITEMCOUNT, (MPARAM) NULL, (MPARAM) NULL))
  {
    pchText = WinSendDlgItemMsg(hwnd, LB_SKINS, LM_QUERYITEMHANDLE, (MPARAM) 0, (MPARAM) NULL);
    if (pchText)
      free(pchText);
    WinSendDlgItemMsg(hwnd, LB_SKINS, LM_DELETEITEM, (MPARAM) 0, (MPARAM) NULL);
  }
}

MRESULT EXPENTRY fnConfigDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  static int bInitializing = 0;
  ConfigDlgInitParams_p pCfgDlgInit;
  HWND hwndOwner;
  SWP swpDlg, swpOwner;
  POINTL ptlTemp;

  switch (msg)
  {
    case WM_INITDLG:

      // Note that we're in WM_INITDLG!
      bInitializing = 1;

      // Store window instance data pointer in window words
      WinSetWindowULong(hwnd, QWL_USER, (ULONG) mp2);
      pCfgDlgInit = (ConfigDlgInitParams_p) mp2;

      // Set dialog control texts and fonts based on
      // current language
      internal_SetPageFont(hwnd);
      internal_SetConfigDialogText(hwnd, pCfgDlgInit);

      // Arrange controls in window
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

      // Note that we're leaving WM_INITDLG!
      bInitializing = 0;

      return (MRESULT) FALSE;

    case WM_DESTROY:
      // Free memory allocated for the listbox items!
      internal_FreeConfigDialogListboxMemory(hwnd);
      break;

    case WM_CONTROL:
      if (SHORT1FROMMP(mp1)==LB_SKINS)
      {
	// The Skins listbox sent us a message!
	if ((SHORT2FROMMP(mp1)==LN_SELECT) && (!bInitializing))
	{
	  // Something has been selected in the listbox!
	  pCfgDlgInit = (ConfigDlgInitParams_p) WinQueryWindowULong(hwnd, QWL_USER);
	  if (pCfgDlgInit)
	  {
	    SHORT sItem;
            char *pchSkin;

	    // Query setting!
	    sItem = (SHORT) WinSendDlgItemMsg(hwnd, LB_SKINS, LM_QUERYSELECTION, (MPARAM) LIT_CURSOR, (MPARAM) NULL);
	    if (sItem!=LIT_NONE)
	    {
	      pchSkin = (char *) WinSendDlgItemMsg(hwnd, LB_SKINS, LM_QUERYITEMHANDLE, (MPARAM) sItem, (MPARAM) NULL);
	      // Set the new setting in our local ModuleConfig structure!

	      if (!pchSkin)
	      {
		pCfgDlgInit->ModuleConfig.bSimpleClock = TRUE;
		strcpy(pCfgDlgInit->ModuleConfig.achClockSkin, "");
	      } else
	      {
		pCfgDlgInit->ModuleConfig.bSimpleClock = FALSE;
		strncpy(pCfgDlgInit->ModuleConfig.achClockSkin, pchSkin, sizeof(pCfgDlgInit->ModuleConfig.achClockSkin));
	      }

	      // If a module (or preview) is already running, then
	      // also in the global ModuleConfig structure, and send a notification to
	      // the running stuff so it will pick up the changes!
	      if ((bRunning) && (hwndSaverWindow))
	      {
#ifdef DEBUG_LOGGING
		AddLog("[fnConfigDialogProc] : Already running, so modifying current config and posting message!\n");
#endif

		if (!pchSkin)
		{
		  ModuleConfig.bSimpleClock = TRUE;
		  strcpy(ModuleConfig.achClockSkin, "");
		} else
		{
		  ModuleConfig.bSimpleClock = FALSE;
		  strncpy(ModuleConfig.achClockSkin, pchSkin, sizeof(ModuleConfig.achClockSkin));
		}

                WinPostMsg(hwndSaverWindow, WM_SKINCHANGED, (MPARAM) NULL, (MPARAM) NULL);
	      }
	    }
	  }
	}
      }
      break;
    default:
      break;
  }
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}


SSMODULEDECLSPEC int SSMODULECALL SSModule_GetModuleDesc(SSModuleDesc_p pModuleDesc, char *pchHomeDirectory)
{
  FILE *hfNLSFile;

  if (!pModuleDesc)
    return SSMODULE_ERROR_INVALIDPARAMETER;

  // Return info about module!
  pModuleDesc->iVersionMajor = 2;
  pModuleDesc->iVersionMinor = 1;
  strcpy(pModuleDesc->achModuleName, "Pretty Clock");
  strcpy(pModuleDesc->achModuleDesc,
         "This module shows a floating clock. The look and feel of the clock can be changed by selecting different skins for the clock.\n"
         "Using zlib v"ZLIB_VERSION" and libpng v"PNG_LIBPNG_VER_STRING".\n"
         "Written by Doodle"
        );

  // If we have NLS support, then show the module description in
  // the chosen language!
  hfNLSFile = internal_OpenNLSFile(pchHomeDirectory);
  if (hfNLSFile)
  {
    sprintmsg(pModuleDesc->achModuleName, hfNLSFile, "MOD_NAME");
    sprintmsg(pModuleDesc->achModuleDesc, hfNLSFile, "MOD_DESC", ZLIB_VERSION, PNG_LIBPNG_VER_STRING);
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
  // a private config file in the %HOME% of current user, or in pchHomeDirectory,
  // or in INI files.

  CfgDlgInit.cbSize = sizeof(CfgDlgInit);
  CfgDlgInit.pchHomeDirectory = pchHomeDirectory;
  internal_LoadConfig(&(CfgDlgInit.ModuleConfig), pchHomeDirectory);

  hwndDlg = WinLoadDlg(HWND_DESKTOP, hwndOwner,
                       fnConfigDialogProc,
                       hmodOurDLLHandle,
                       DLG_CONFIGUREPRETTYCLOCK,
                       &CfgDlgInit);

  if (!hwndDlg)
    return SSMODULE_ERROR_INTERNALERROR;

  if (WinProcessDlg(hwndDlg) == DID_OK)
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

  if (bRunning)
    return SSMODULE_ERROR_ALREADYRUNNING;

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

