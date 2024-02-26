#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <process.h>
#include <time.h>
#define INCL_DOS
#define INCL_WIN
#define INCL_ERRORS
#define INCL_GPIPRIMITIVES
#define INCL_GPI
#define INCL_GPIBITMAPS
#define INCL_DEV
#include <os2.h>
#include "SSModule.h"
#include "time.h"
#include "Log.h"
#include "GBMFunc.h"

#include "SldShow-Resources.h"
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

// Timer ID to blank the window
#define BLANK_TIMER_ID 11
#define NEXT_TIMER_ID 12

// Private class name for our saver window
#define SAVERWINDOW_CLASS "SLDSHOW_SCREENSAVER_CLASS"

// Private window messages
#define WM_SUBCLASS_INIT       (WM_USER+1)
#define WM_SUBCLASS_UNINIT     (WM_USER+2)
#define WM_ASKPASSWORD         (WM_USER+3)
#define WM_SHOWWRONGPASSWORD   (WM_USER+4)
#define WM_NEXTIMAGE           (WM_USER+5)
#define WM_BUILDLIST           (WM_USER+6)
#define WM_LOADNEXTIMAGE       (WM_USER+7)

HMODULE hmodOurDLLHandle;
int     bRunning;
int     bFirstKeyGoesToPwdWindow = 0;
TID     tidSaverThread;
int     iSaverThreadState;
HWND    hwndSaverWindow; // If any...
HMQ     hmqSaverThreadMsgQueue;
int     bOnlyPreviewMode;

ULONG   ulBlankTimerID;
ULONG   ulNextTimerID;
ULONG   ulAutoHiderTimerID;

char   *apchNLSText[SSMODULE_NLSTEXT_MAX+1];
HMTX    hmtxUseNLSTextArray;

/* begin config */
MRESULT EXPENTRY fnConfigDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY dirDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
void readConfig (char *pchHomeDirectory);
static void internal_AdjustConfigDialogControls(HWND hwnd);
static void internal_SetConfigDialogText(HWND hwnd, char *pchHomeDirectory);
typedef struct ModuleConfig_s {
  int  iZoomIfSmallerThanScreen;
  char sDirName[1024];
  char sDirNameOrg[1024];
  long lBlankTime;
  long lSwitchTime;
  BOOL bRandom;
} ModuleConfig_t, *ModuleConfig_p;

typedef struct ConfigDlgInitParams_s {
  USHORT          cbSize; // Required to be able to pass this structure to WM_INITDLG
  char           *pchHomeDirectory;
  ModuleConfig_t  ModuleConfig;
} ConfigDlgInitParams_t, *ConfigDlgInitParams_p;

ConfigDlgInitParams_t CfgDlgInit;

/* END config */

#ifdef DEBUG_BUILD
#define DEBUG_LOGGING
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

MRESULT EXPENTRY fnSaverWindowProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  SWP swpDlg, swpParent;
  HWND hwndDlg;
  int rc;
  static char fn_src[CCHMAXPATH+1];
  static GBM gbm;
  static GBMRGB gbmrgb [0x100];
  static BYTE *pbData;
  static HBITMAP hbmBmp;
  static int imageOK;
  static BOOL stretchLarge;
  static BOOL showImage;
  #define maxArray 100
  static char fileNames[maxArray][1024];
  static int currentImageNumber, maxImageNumber;
  static BOOL directoryOK;
  time_t time_of_day; 
  auto struct tm tmbuf; 

  switch( msg )
  {
    case WM_SUBCLASS_INIT:
    case WM_CREATE:
      // Any initialization of the window and variables should come here.
      imageOK = 0;
      showImage = TRUE;
      stretchLarge = TRUE;
      time_of_day = time( NULL ); 
      _localtime( &time_of_day, &tmbuf ); 
      srand(tmbuf.tm_sec*tmbuf.tm_min+tmbuf.tm_hour);
      // Hide mouse pointer, if we're in real screen-saving mode!
      if (!bOnlyPreviewMode) {
	        WinShowPointer(HWND_DESKTOP, FALSE);
           ulBlankTimerID = WinStartTimer(WinQueryAnchorBlock(hwnd), hwnd, BLANK_TIMER_ID, CfgDlgInit.ModuleConfig.lBlankTime);
           ulNextTimerID = WinStartTimer(WinQueryAnchorBlock(hwnd), hwnd, NEXT_TIMER_ID, CfgDlgInit.ModuleConfig.lSwitchTime);
      }
      // Initialize WMCHAR record
      internal_InitWMCHARRecord();
      WinPostMsg(hwnd, WM_BUILDLIST, (MPARAM) NULL, (MPARAM) NULL);
      break;

		case WM_BUILDLIST:
		{
			// using CfgDlgInit.ModuleConfig.sDirName, build a list of images that should be shown
			// the list will be saved in fileNames[]
			// the image that is currently shown is pointed to by currentImageNumber
			HDIR          hdirFindHandle = HDIR_CREATE;
			FILEFINDBUF3  FindBuffer     = {0};      /* Returned from FindFirst/Next */
			ULONG         ulResultBufLen = sizeof(FILEFINDBUF3);
			ULONG         ulFindCount    = 1;        /* Look for 1 file at a time    */
			CHAR          initialDir[355];
			INT           curFile = 0;
			rc = NO_ERROR; 
			directoryOK = TRUE;
			sprintf(initialDir,"%s\\*.*", CfgDlgInit.ModuleConfig.sDirName);
			rc = DosFindFirst( initialDir,                /* File pattern - all files     */
                       &hdirFindHandle,      /* Directory search handle      */
                       FILE_NORMAL,          /* Search attribute             */
                       &FindBuffer,          /* Result buffer                */
                       ulResultBufLen,       /* Result buffer length         */
                       &ulFindCount,         /* Number of entries to find    */
                       FIL_STANDARD);        /* Return Level 1 file info     */

			if (rc != NO_ERROR) {
				#ifdef DEBUG_LOGGING
					AddLog("[WM_BUILDLIST] DosFindFirst error: return code = %d\n",rc);
				#endif
				directoryOK = FALSE;
			} else {
				sprintf(fileNames[curFile],"%s\\%s\0", CfgDlgInit.ModuleConfig.sDirName, FindBuffer.achName);
				maxImageNumber = curFile;
				curFile++;
				#ifdef DEBUG_LOGGING
					AddLog("[WM_BUILDLIST] (first) files[curFile] = %s\n",fileNames[curFile-1]);
				#endif
			} /* endif */

			/* Keep finding the next file until there are no more files */
			while ((rc != ERROR_NO_MORE_FILES) && (directoryOK == TRUE) && (curFile < maxArray)) {
				ulFindCount = 1;                      /* Reset find count.            */

				rc = DosFindNext(hdirFindHandle,      /* Directory handle             */
                        &FindBuffer,         /* Result buffer                */
                        ulResultBufLen,      /* Result buffer length         */
                        &ulFindCount);       /* Number of entries to find    */

				if (rc != NO_ERROR && rc != ERROR_NO_MORE_FILES) {
					#ifdef DEBUG_LOGGING
						AddLog("[WM_BUILDLIST] DosFindNext error: return code = %d\n",rc);
					#endif
					directoryOK = FALSE;
				} else {
					sprintf(fileNames[curFile],"%s\\%s\0", CfgDlgInit.ModuleConfig.sDirName, FindBuffer.achName);
					maxImageNumber = curFile-1;
					curFile++;
					#ifdef DEBUG_LOGGING
						AddLog("[WM_BUILDLIST] (next) files[curFile] = %s\n",fileNames[curFile-1]);
					#endif
				}
			} /* endwhile */
			rc = DosFindClose(hdirFindHandle);
			currentImageNumber = -1;
			if (strlen(CfgDlgInit.ModuleConfig.sDirName) <= 2) directoryOK = FALSE;
			WinPostMsg(hwnd, WM_LOADNEXTIMAGE, (MPARAM) NULL, (MPARAM) NULL);
		}
		break;

	case WM_LOADNEXTIMAGE:
		if (!bOnlyPreviewMode) {
			WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, ulNextTimerID);
			ulNextTimerID = 0;
		}
		if (directoryOK) {
			BOOL nextOK = FALSE;
			INT wrapCount = 0;
			int randcount = 1;
			while (nextOK == FALSE) {
				if (CfgDlgInit.ModuleConfig.bRandom) {
					currentImageNumber = (((float)rand())/((float)RAND_MAX))*min(maxArray, maxImageNumber+1);
					randcount += 1;
					if (currentImageNumber < 0) currentImageNumber = 0;
					if (currentImageNumber >= min(maxArray, maxImageNumber+1)) currentImageNumber = 0;
				} else {
					currentImageNumber += 1;
					if (currentImageNumber >= min(maxArray, maxImageNumber+1)) {
						currentImageNumber = 0;
						wrapCount += 1;
					}
				}
				if (hbmBmp) GpiDeleteBitmap(hbmBmp);
				strcpy(fn_src , fileNames[currentImageNumber]);
				#ifdef DEBUG_LOGGING
					DosBeep(1400,100);
				#endif
				if (LoadBitmap(HWND_DESKTOP, fn_src, "", &gbm, gbmrgb, &pbData)) {
					HPS hps = WinGetPS(HWND_DESKTOP);
					HDC hdc = GpiQueryDevice(hps);
					LONG lPlanes, lBitCount;

					DevQueryCaps(hdc, CAPS_COLOR_PLANES  , 1L, &lPlanes  );
					DevQueryCaps(hdc, CAPS_COLOR_BITCOUNT, 1L, &lBitCount);
					WinReleasePS(hps);
					imageOK = MakeBitmap(HWND_DESKTOP, &gbm, gbmrgb, pbData, &hbmBmp);
					if (imageOK == 0) {
						if (pbData) free(pbData);
						#ifdef DEBUG_LOGGING
							AddLog("[WM_LOADNEXTIMAGE] Cannot convert image to bitmap! (%s)\n", fileNames[currentImageNumber]);
						#endif
					} else {
						if (pbData) free(pbData);
						#ifdef DEBUG_LOGGING
							AddLog("[WM_LOADNEXTIMAGE] Image loaded and OK! (%s)\n", fileNames[currentImageNumber]);
						#endif
						nextOK = TRUE;
					}
				} else {
					#ifdef DEBUG_LOGGING
						AddLog("[WM_LOADNEXTIMAGE] Cannot open image! (%s)\n", fileNames[currentImageNumber]);
					#endif
				}
				if (CfgDlgInit.ModuleConfig.bRandom) {
					if (randcount > (2*min(maxArray, maxImageNumber+1))) {
						directoryOK = FALSE;
						nextOK = TRUE;
						imageOK = 0;
					}
				} else {
					if (wrapCount >= 2) {
						directoryOK = FALSE;
						nextOK = TRUE;
						imageOK = 0;
					}
				}
			}
			WinInvalidateRect(hwnd, NULL, FALSE);
		}
		if (!bOnlyPreviewMode) {
  			ulNextTimerID = WinStartTimer(WinQueryAnchorBlock(hwnd), hwnd, NEXT_TIMER_ID, CfgDlgInit.ModuleConfig.lSwitchTime);
		}
	break;

    case WM_CHAR:
      if (!bOnlyPreviewMode)
        internal_SaveWMCHAREventToRecord(mp1, mp2);
      break;

    case WM_TIMER:
      if (((SHORT)mp1)==BLANK_TIMER_ID) {
        showImage = FALSE;
        WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, ulBlankTimerID);
        ulBlankTimerID = 0;
        WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, ulNextTimerID);
        ulNextTimerID = 0;
        WinInvalidateRect(hwnd, NULL, FALSE);
      }
      if (((SHORT)mp1)==NEXT_TIMER_ID) {
        WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, ulNextTimerID);
        ulNextTimerID = 0;
        ulNextTimerID = WinStartTimer(WinQueryAnchorBlock(hwnd), hwnd, NEXT_TIMER_ID, CfgDlgInit.ModuleConfig.lSwitchTime);
        WinPostMsg(hwnd, WM_LOADNEXTIMAGE, (MPARAM) NULL, (MPARAM) NULL);
      }
    break;

    case WM_ASKPASSWORD:
      {
        // Get parameters
        char *pchPwdBuff = (char *) mp1;
        int iPwdBuffSize = (int) mp2;

	      if (ulBlankTimerID) {
           WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, ulBlankTimerID);
           ulBlankTimerID = 0;
         }
	      if (ulNextTimerID) {
           WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, ulNextTimerID);
           ulNextTimerID = 0;
         }
         if (!ulBlankTimerID) {
           ulBlankTimerID = WinStartTimer(WinQueryAnchorBlock(hwnd), hwnd, BLANK_TIMER_ID, CfgDlgInit.ModuleConfig.lBlankTime);
           showImage = TRUE;
           WinInvalidateRect(hwnd, NULL, FALSE);
          }

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
        if (!ulNextTimerID) {
           ulNextTimerID = WinStartTimer(WinQueryAnchorBlock(hwnd), hwnd, NEXT_TIMER_ID, CfgDlgInit.ModuleConfig.lSwitchTime);
          }
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

    case WM_SUBCLASS_UNINIT:
    case WM_DESTROY:
      // All kinds of cleanup (the opposite of everything done in WM_CREATE)
      // should come here.
      GpiDeleteBitmap(hbmBmp);
      WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, ulBlankTimerID);
      ulBlankTimerID = 0;
      WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, ulNextTimerID);
      ulNextTimerID = 0;
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
        static SIZEL sizl = { 0, 0 };
        RECTL rclRect;
        HPS hps = WinBeginPaint(hwnd, (HPS) NULL, &rclRect);
        HAB hab = WinQueryAnchorBlock(hwnd);
        HDC hdcBmp = DevOpenDC(hab, OD_MEMORY, "*", 0L, (PDEVOPENDATA) NULL, (HDC) NULL);
        HPS hpsBmp = GpiCreatePS(hab, hdcBmp, &sizl, PU_PELS | GPIF_DEFAULT | GPIT_MICRO | GPIA_ASSOC);
        ULONG screenWidth = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
        ULONG screenHight = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
        POINTL aptl [4];
        RECTL rcl;
        BITMAPINFOHEADER bmp;
        LONG colorTable[2] = { 0 };

        /* fill the background with black */
        WinQueryWindowRect(hwnd, &rclRect);
        WinFillRect(hps, &rclRect, CLR_BLACK);
        if (strcmp(CfgDlgInit.ModuleConfig.sDirName, "") == 0 ) {
            WinDrawText(hps, strlen("Please select a directory."), "Please select a directory.", &rclRect, SYSCLR_WINDOWTEXT, SYSCLR_WINDOW, DT_ERASERECT | DT_CENTER | DT_VCENTER);
            WinEndPaint(hps);
            return (MRESULT) FALSE;
        }

        if ((imageOK == 1) && showImage) {
          GpiQueryBitmapParameters(hbmBmp, &bmp);
          GpiSetBitmap(hpsBmp, hbmBmp);

          //	colorTable[0] = lColorBg;
          //	colorTable[1] = lColorFg;
          GpiCreateLogColorTable(hps, 0, LCOLF_CONSECRGB, 0, 2, &colorTable);
          /* if something goes wrong when creating color table, use the closest color */
          //	GpiSetBackColor(hps, GpiQueryColorIndex(hps, 0, lColorBg));
          // GpiSetColor    (hps, GpiQueryColorIndex(hps, 0, lColorFg));

          WinQueryWindowRect(hwnd, &rcl);
          if ((screenHight < bmp.cy) || (screenWidth < bmp.cx)) {
            //image is larger than screen, in X or Y coordinate
            if (stretchLarge) {
              //Zoom Image if it is larger than screen
              //this code does NOT preserve the aspect ration
//              aptl[0].x = rcl.xLeft;
//              aptl[0].y = rcl.yBottom;
//              aptl[1].x = rcl.xRight;
//              aptl[1].y = rcl.yTop;
              //this code will preserve the aspect ration
              float factor = min((float)rcl.yTop / (float)bmp.cy, (float)rcl.xRight / (float)bmp.cx);
              aptl[0].x = (rcl.xRight - ((float)bmp.cx)*factor)/(float)2;
              aptl[0].y = (rcl.yTop - (((float)bmp.cy)*factor))/(float)2;
              aptl[1].x = ((rcl.xRight - ((float)bmp.cx)*factor)/(float)2)+((float)bmp.cx)*factor;
              aptl[1].y = rcl.yTop - (rcl.yTop - (((float)bmp.cy)*factor))/(float)2;
            } else {
              //Center image if it is larger than screen
              //THIS CODE DOES NOT WORK CORRECTLY, AS THE NEGATIVE X-COORDINATE SEEMS TO OVERLAP INTO THE HIGHER X-COORDINATE
              //IT SEEMS TO WRAP ARROUND
              aptl[0].x = ((((float)screenWidth - (float)bmp.cx)/(float)screenWidth)*(float)rcl.xRight)/(float)2;
              aptl[0].y = rcl.yBottom + ((((float)screenHight - (float)bmp.cy)/(float)screenHight)*(float)rcl.yTop)/(float)2;
              aptl[1].x = rcl.xRight - ((((float)screenWidth - (float)bmp.cx)/(float)screenWidth)*(float)rcl.xRight)/(float)2;
              aptl[1].y = rcl.yTop - ((((float)screenHight - (float)bmp.cy)/(float)screenHight)*(float)rcl.yTop)/(float)2;
            } /* endif */
          } else {
            //image is smaller in both coordinates
            if (CfgDlgInit.ModuleConfig.iZoomIfSmallerThanScreen == 1) {
              //Zoom Image if it is smaller than screen
//              this is the code in case the aspect ration should NOT be preserved
//              aptl[0].x = rcl.xLeft;
//              aptl[0].y = rcl.yBottom;
//              aptl[1].x = rcl.xRight;
//              aptl[1].y = rcl.yTop;
//              aptl[1].x = rcl.xRight;
//              aptl[1].y = rcl.yTop;
              //with this code the aspect ration is preserved
              float factor = min((float)rcl.yTop / (float)bmp.cy, (float)rcl.xRight / (float)bmp.cx);
              aptl[0].x = (rcl.xRight - ((float)bmp.cx)*factor)/(float)2;
              aptl[0].y = (rcl.yTop - (((float)bmp.cy)*factor))/(float)2;
              aptl[1].x = ((rcl.xRight - ((float)bmp.cx)*factor)/(float)2)+((float)bmp.cx)*factor;
              aptl[1].y = rcl.yTop - (rcl.yTop - (((float)bmp.cy)*factor))/(float)2;
            } else {
              //Center image if it is smaller than screen
              aptl[0].x = ((((float)screenWidth - (float)bmp.cx)/(float)screenWidth)*(float)rcl.xRight)/(float)2;
              aptl[0].y = rcl.yBottom + ((((float)screenHight - (float)bmp.cy)/(float)screenHight)*(float)rcl.yTop)/(float)2;
              aptl[1].x = rcl.xRight - ((((float)screenWidth - (float)bmp.cx)/(float)screenWidth)*(float)rcl.xRight)/(float)2;
              aptl[1].y = rcl.yTop - ((((float)screenHight - (float)bmp.cy)/(float)screenHight)*(float)rcl.yTop)/(float)2;
            } /* endif */
          }
          aptl[2].x = 0;
          aptl[2].y = 0;
          aptl[3].x = bmp.cx;
          aptl[3].y = bmp.cy;
          GpiBitBlt(hps, hpsBmp, 4L, aptl, ROP_SRCCOPY, BBO_IGNORE);

          GpiSetBitmap(hpsBmp, (HBITMAP) NULL);
          GpiDestroyPS(hpsBmp);

          DevCloseDC(hdcBmp);
        } else {
           if (imageOK == 0 ) WinDrawText(hps, strlen("Image Corrupt or Unknown Image Type."), "Image Corrupt or Unknown Image Type.", &rclRect, SYSCLR_WINDOWTEXT, SYSCLR_WINDOW, DT_ERASERECT | DT_CENTER | DT_VCENTER);
           if (imageOK == 2 ) WinDrawText(hps, strlen("Not enough free shared memory to display image. (internal GpiCreateBitmap error)"), "Not enough free shared memory to display image. (internal GpiCreateBitmap error)", &rclRect, SYSCLR_WINDOWTEXT, SYSCLR_WINDOW, DT_ERASERECT | DT_CENTER | DT_VCENTER);
        }
        WinEndPaint(hps);
        return (MRESULT) FALSE;
      }

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
	sprintf(pchFileName, "%sModules\\SldShow\\%s.msg", pchHomeDirectory, achRealLocaleName);
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
	  sprintf(pchFileName, "%sModules\\SldShow\\%s.msg", pchHomeDirectory, pchLang);
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

SSMODULEDECLSPEC int SSMODULECALL SSModule_GetModuleDesc(SSModuleDesc_p pModuleDesc, char *pchHomeDirectory)
{
  FILE *hfNLSFile;

  if (!pModuleDesc)
    return SSMODULE_ERROR_INVALIDPARAMETER;

  // Return info about module!
  pModuleDesc->iVersionMajor = 2;
  pModuleDesc->iVersionMinor = 0;
  strcpy(pModuleDesc->achModuleName, "Slideshow");
  strcpy(pModuleDesc->achModuleDesc,
         "Slideshow .\n"
         "Written by Warp5."
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
  char chFileName[1024];
  char buffer[4097];
  int  chars;
  FILE *hFile;

  // This stuff here should create a window, to let the user
  // configure the module. The configuration should be read and saved from/to
  // a private config file in pchHomeDirectory, or in INI files.

  // load the dialog
  hwndDlg = WinLoadDlg(HWND_DESKTOP, hwndOwner, fnConfigDialogProc, hmodOurDLLHandle, DLG_CONFIGURE, &CfgDlgInit);

  if (!hwndDlg) return SSMODULE_ERROR_INTERNALERROR;

  if (WinProcessDlg(hwndDlg) == PB_CONFOK) {
    char *pchHomeEnvVar;
    // User dismissed the dialog with OK button,
    // so save configuration!
    memset(chFileName, 0 , sizeof(chFileName));
    // Get home directory of current user
    hFile = NULL;
    pchHomeEnvVar = getenv("HOME");
    // If we have the HOME variable set, then we'll try to use
    // the .dssaver folder in there, otherwise fall back to
    // using the DSSaver Global home directory!
    if (pchHomeEnvVar) {
      int iLen;
      memset(chFileName, 0 , sizeof(chFileName));
      snprintf(chFileName, sizeof(chFileName)-1, "%s", pchHomeEnvVar);
      // Make sure it will have the trailing backslash!
      iLen = strlen(chFileName);
      if ((iLen>0) &&
          (chFileName[iLen-1]!='\\'))
      {
        chFileName[iLen] = '\\';
        chFileName[iLen+1] = 0;
      }
      // Now put there the .dssaver folder name and the config file name!
      strncat(chFileName, ".dssaver\\SldShow.CFG", sizeof(chFileName));
      // Try to open that config file!
      hFile = fopen(chFileName, "wt");
    }

    if (!hFile) {
      // If we could not open a config file in the HOME directory, or there was
      // no HOME directory, then try to use the DSSaver Global home directory!
      memset(chFileName, 0 , sizeof(chFileName));
      snprintf(chFileName, sizeof(chFileName), "%sSldShow.CFG", pchHomeDirectory);
      hFile = fopen(chFileName, "wt");
    }

    if (!hFile) {
      // Could not create config file
      //DosBeep(1400,200);
    } else {
      memset(buffer,0, sizeof(buffer));
      sprintf(buffer, "iZoomIfSmallerThanScreen = %i\n", CfgDlgInit.ModuleConfig.iZoomIfSmallerThanScreen);
      if (strcmp(CfgDlgInit.ModuleConfig.sDirName, "") == 0)
        sprintf(buffer, "%ssDirName = %s\n", buffer, " ");
      else
        sprintf(buffer, "%ssDirName = %s\n", buffer, CfgDlgInit.ModuleConfig.sDirName);
      sprintf(buffer, "%slBlankTime = %i\n", buffer, CfgDlgInit.ModuleConfig.lBlankTime);
      sprintf(buffer, "%slSwitchTime = %i\n", buffer, CfgDlgInit.ModuleConfig.lSwitchTime);
      if (CfgDlgInit.ModuleConfig.bRandom)
         sprintf(buffer, "%sbRandom = TRUE\n", buffer);
      else
         sprintf(buffer, "%sbRandom = FALSE\n", buffer);
      chars = fwrite(buffer, sizeof(char), strlen(buffer), hFile);
      if (chars != strlen(buffer)) {
        // bad file
        //DosBeep(1400,200);
        fclose(hFile);
      }
      fclose(hFile);
    }

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

  // read Config Data
  readConfig(pchHomeDirectory);

  iSaverThreadState = STATE_UNKNOWN;
  bOnlyPreviewMode = bPreviewMode;
  tidSaverThread = _beginthread(fnSaverThread,
                                0,
                                1024*1024*10,
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
    if (! IsGbmAvailable())
    {
        return 0;
    }

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

MRESULT EXPENTRY fnConfigDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  HWND hwndOwner;
  SWP swpDlg, swpOwner;
  POINTL ptlTemp;
  ConfigDlgInitParams_p pCfgDlgInit;
  FILEDLG  fd;
  PCHAR pchBlankTimes[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "15", "20", "30", "40", "50", "60", "90", "120" };
  PCHAR pchSwitchTimes[] = { "0.25", "0.5", "0.75",  "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "15", "20", "30", "40", "50", "60", "90", "120" };
  PCHAR buffer;
  long arrayIndex;

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

      // Set initial position of dialog window and show it
      WinQueryWindowPos(hwnd, &swpDlg);
      hwndOwner = WinQueryWindow(hwnd, QW_OWNER);
      WinQueryWindowPos(hwndOwner, &swpOwner);
      ptlTemp.x = swpOwner.x; ptlTemp.y = swpOwner.y;
      WinMapWindowPoints(WinQueryWindow(hwndOwner, QW_PARENT), HWND_DESKTOP, &ptlTemp, 1);
      WinSetWindowPos(hwnd, HWND_TOP,
            ptlTemp.x + (swpOwner.cx - swpDlg.cx)/2,
            ptlTemp.y + (swpOwner.cy - swpDlg.cy)/2,
            0, 0, SWP_MOVE | SWP_SHOW);
     //setup the dialog
     if (pCfgDlgInit->ModuleConfig.iZoomIfSmallerThanScreen == 1)
         WinSendDlgItemMsg(hwnd, CB_ZOOMIFSMALLERTHANSCREEN, BM_SETCHECK, MPFROMSHORT(TRUE), NULL);
     WinSendDlgItemMsg(hwnd, EF_FILENAME, EM_SETTEXTLIMIT, MPFROMSHORT(256), NULL);
     WinSetWindowText(WinWindowFromID(hwnd, EF_FILENAME), pCfgDlgInit->ModuleConfig.sDirName);
     strcpy (pCfgDlgInit->ModuleConfig.sDirNameOrg, pCfgDlgInit->ModuleConfig.sDirName);
     if (pCfgDlgInit->ModuleConfig.bRandom == TRUE)
         WinSendDlgItemMsg(hwnd, CB_RANDOMORDER, BM_SETCHECK, MPFROMSHORT(TRUE), NULL);
     WinSendDlgItemMsg (hwnd, SB_TIMER, SPBM_SETARRAY, MPFROMP (pchBlankTimes), MPFROMSHORT (18));
     arrayIndex= 0;
     switch (pCfgDlgInit->ModuleConfig.lBlankTime) {
        case   60000: arrayIndex= 0; break;
        case  120000: arrayIndex= 1; break;
        case  180000: arrayIndex= 2; break;
        case  240000: arrayIndex= 3; break;
        case  300000: arrayIndex= 4; break;
        case  360000: arrayIndex= 5; break;
        case  420000: arrayIndex= 6; break;
        case  480000: arrayIndex= 7; break;
        case  540000: arrayIndex= 8; break;
        case  600000: arrayIndex= 9; break;
        case  900000: arrayIndex=10; break;
        case 1200000: arrayIndex=11; break;
        case 1800000: arrayIndex=12; break;
        case 2400000: arrayIndex=13; break;
        case 3000000: arrayIndex=14; break;
        case 3600000: arrayIndex=15; break;
        case 5400000: arrayIndex=16; break;
        case 7200000: arrayIndex=17; break;
        default: arrayIndex= 1; break;
     }
     WinSendDlgItemMsg (hwnd, SB_TIMER, SPBM_SETCURRENTVALUE, MPFROMLONG (arrayIndex), 0);
     WinSendDlgItemMsg (hwnd, SB_SWITCH, SPBM_SETARRAY, MPFROMP (pchSwitchTimes), MPFROMSHORT (21));
     arrayIndex= 0;
     switch (pCfgDlgInit->ModuleConfig.lSwitchTime) {
        case   15000: arrayIndex= 0; break;
        case   30000: arrayIndex= 1; break;
        case   45000: arrayIndex= 2; break;
        case   60000: arrayIndex= 3; break;
        case  120000: arrayIndex= 4; break;
        case  180000: arrayIndex= 5; break;
        case  240000: arrayIndex= 6; break;
        case  300000: arrayIndex= 7; break;
        case  360000: arrayIndex= 8; break;
        case  420000: arrayIndex= 9; break;
        case  480000: arrayIndex=10; break;
        case  540000: arrayIndex=11; break;
        case  600000: arrayIndex=12; break;
        case  900000: arrayIndex=13; break;
        case 1200000: arrayIndex=14; break;
        case 1800000: arrayIndex=15; break;
        case 2400000: arrayIndex=16; break;
        case 3000000: arrayIndex=17; break;
        case 3600000: arrayIndex=18; break;
        case 5400000: arrayIndex=19; break;
        case 7200000: arrayIndex=20; break;
        default: arrayIndex= 1; break;
     }
     WinSendDlgItemMsg (hwnd, SB_SWITCH, SPBM_SETCURRENTVALUE, MPFROMLONG (arrayIndex), 0);
     internal_AdjustConfigDialogControls(hwnd);
     return (MRESULT) FALSE;

    case WM_COMMAND:
        switch (SHORT1FROMMP(mp1)) {
          case PB_CONFOK:
            pCfgDlgInit = (ConfigDlgInitParams_p) WinQueryWindowULong(hwnd, QWL_USER);
            if (pCfgDlgInit) {
              // write values back to struct
              if (WinSendDlgItemMsg(hwnd, CB_ZOOMIFSMALLERTHANSCREEN, BM_QUERYCHECK, NULL, NULL)) {
                pCfgDlgInit->ModuleConfig.iZoomIfSmallerThanScreen = 1;
              } else {
                pCfgDlgInit->ModuleConfig.iZoomIfSmallerThanScreen = 0;
              }
              WinQueryWindowText(WinWindowFromID(hwnd, EF_FILENAME), 248, pCfgDlgInit->ModuleConfig.sDirName);
              buffer = calloc(10,1);
              WinSendDlgItemMsg(hwnd, SB_TIMER, SPBM_QUERYVALUE, buffer, MPFROM2SHORT(9,SPBQ_DONOTUPDATE));
              CfgDlgInit.ModuleConfig.lBlankTime = atol(buffer) * 60000;
              free(buffer);
              buffer = calloc(10,1);
              WinSendDlgItemMsg(hwnd, SB_SWITCH, SPBM_QUERYVALUE, buffer, MPFROM2SHORT(9,SPBQ_DONOTUPDATE));
              CfgDlgInit.ModuleConfig.lSwitchTime = (long)((float)atof(buffer) * (float)60000);
              if (WinSendDlgItemMsg(hwnd, CB_RANDOMORDER, BM_QUERYCHECK, NULL, NULL)) {
                pCfgDlgInit->ModuleConfig.bRandom = TRUE;
              } else {
                pCfgDlgInit->ModuleConfig.bRandom = FALSE;
              }
              free(buffer);
            }
          break;

          case PB_CONFCANCEL:
            // restore the old image
            pCfgDlgInit = (ConfigDlgInitParams_p) WinQueryWindowULong(hwnd, QWL_USER);
            if (pCfgDlgInit) {
               strcpy (pCfgDlgInit->ModuleConfig.sDirName, pCfgDlgInit->ModuleConfig.sDirNameOrg);
            }
          break;


/* GBM File Dialog */
/*          case PB_BROWSE: {
            GBMFILEDLG gbmfild;

            memset(&(gbmfild.fild), 0, sizeof(FILEDLG));
            gbmfild.fild.cbSize = sizeof(FILEDLG);
            gbmfild.fild.fl = (FDS_CENTER|FDS_OPEN_DIALOG|FDS_HELPBUTTON);
            //strcpy(gbmfild.fild.szFullFile, szFileName);
            strcpy(gbmfild.fild.szFullFile, "C:\\");
            strcpy(gbmfild.szOptions, "");	
            GbmFileDlg(HWND_DESKTOP, hwnd, &gbmfild);
            if ( gbmfild.fild.lReturn != DID_OK ) {
                return (MRESULT) 0;
            }
            return (MRESULT) TRUE;
          }*/

          
	case PB_BROWSE: {
		PCHAR directory, temp;
		HWND hwndDlg;
		
		memset(&fd,0,sizeof(FILEDLG));
		fd.cbSize = sizeof( fd );
		// It's an centered 'Open'-dialog
		fd.fl = FDS_CUSTOM|FDS_OPEN_DIALOG|FDS_CENTER;
		fd.pfnDlgProc=dirDialogProc;
		fd.usDlgId=IDDLG_DIRECTORY;
		fd.hMod=hmodOurDLLHandle;
		fd.pszTitle = "Choose a Directory";
		temp = calloc(300, 1);
		directory = calloc(300, 1);
		WinQueryWindowText(WinWindowFromID(hwnd, EF_FILENAME), 248, temp);
		/*if (strrchr(temp, '\\') != NULL) {
			FILESTATUS3  fsts3ConfigInfo = {{0}};
			ULONG        ulBufSize       = sizeof(FILESTATUS3);
			APIRET       rc              = NO_ERROR;
			strncpy(directory, temp,strrchr(temp, '\\') - temp);
			if (strlen(directory) == 2) strcat(directory,"\\\0");
			rc = DosQueryPathInfo(directory, FIL_STANDARD, &fsts3ConfigInfo, ulBufSize);
			if (!rc) {
				memset(directory,0,strlen(directory));
				strncpy(directory, temp,strrchr(temp, '\\') - temp);
				strcpy(fd.szFullFile,directory);
			}
		}*/
		if (temp[strlen(temp)-1] != '\\') strcat(temp, "\\");
		strcpy(fd.szFullFile,temp);

		hwndDlg = WinFileDlg(HWND_DESKTOP, hwnd, &fd );
		if( hwndDlg == NULLHANDLE ) {
			//WinFileDlg failed

		} else {
			if( fd.lReturn == DID_OK ) {
				char tt[1024];
				strcpy(tt, fd.szFullFile);
				WinSetWindowText(WinWindowFromID(hwnd, EF_FILENAME), tt);
				if ((bRunning) && (hwndSaverWindow)) {
					// the preview window is showing something, make it change the image
					pCfgDlgInit = (ConfigDlgInitParams_p) WinQueryWindowULong(hwnd, QWL_USER);
					if (pCfgDlgInit) {
						WinQueryWindowText(WinWindowFromID(hwnd, EF_FILENAME), 248, pCfgDlgInit->ModuleConfig.sDirName);
						#ifdef DEBUG_LOGGING
						  AddLog("[CONFIG WM_COMMAND PB_BROWSE]: Posting WM_IMAGECHANGED\n");
						  AddLog("\n");
                 				#endif
					}
				}		
			}
		}
		WinDestroyWindow(hwndDlg);
		free(temp);
		free(directory);
		return (MRESULT) TRUE;
	}
	break;
        }
    break;

    default:
      break;
  }
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

void readConfig (char *pchHomeDirectory) {
  char chFileName[1024];
  char buffer[4097];
  char *line, *lineend, *space, *term, *value;
  int  chars;
  FILE *hFile;
  char *pchHomeEnvVar;

  /* initialize to default values */
  CfgDlgInit.cbSize = sizeof(CfgDlgInit);
  CfgDlgInit.pchHomeDirectory = pchHomeDirectory;
  CfgDlgInit.ModuleConfig.iZoomIfSmallerThanScreen = 1;
  CfgDlgInit.ModuleConfig.lBlankTime = 120000;
  CfgDlgInit.ModuleConfig.lSwitchTime = 60000;
  memset(CfgDlgInit.ModuleConfig.sDirName,0,sizeof(CfgDlgInit.ModuleConfig.sDirName));
  sprintf(CfgDlgInit.ModuleConfig.sDirName, "");

  // Get home directory of current user
  hFile = NULL;
  pchHomeEnvVar = getenv("HOME");
  // If we have the HOME variable set, then we'll try to use
  // the .dssaver folder in there, otherwise fall back to
  // using the DSSaver Global home directory!
  if (pchHomeEnvVar)
  {
    int iLen;
    memset(chFileName, 0 , sizeof(chFileName));
    snprintf(chFileName, sizeof(chFileName)-1, "%s", pchHomeEnvVar);
    // Make sure it will have the trailing backslash!
    iLen = strlen(chFileName);
    if ((iLen>0) &&
        (chFileName[iLen-1]!='\\'))
    {
      chFileName[iLen] = '\\';
      chFileName[iLen+1] = 0;
    }
    // Now put there the .dssaver folder name and the config file name!
    strncat(chFileName, ".dssaver\\SldShow.CFG", sizeof(chFileName));
    // Try to open that config file!
    hFile = fopen(chFileName, "r");
  }

  if (!hFile)
  {
    // If we could not open a config file in the HOME directory, or there was
    // no HOME directory, then try to use the DSSaver Global home directory!
    memset(chFileName, 0 , sizeof(chFileName));
    snprintf(chFileName, sizeof(chFileName), "%sSldShow.CFG", pchHomeDirectory);
    hFile = fopen(chFileName, "r");
  }


  if (!hFile) {
    // Could not open config file, use defaults!
    // DosBeep(1400,200);
  } else {
    memset(buffer,0, sizeof(buffer));
    chars = fread(buffer, sizeof(char), 4096, hFile);
    if (!chars) {
      // empty or bad file, use defaults!
      //DosBeep(1400,200);
      fclose(hFile);
    } else {
      line = &buffer[0];
      lineend = strstr(line, "\n");
      while (lineend != NULL) {
        term = calloc(500,1);
        value = calloc(1024,1);

        space = strstr(line, " ");
        if (space == NULL) break;
        strncpy(term, line, space - line);
        space = strstr(space+1, " ");
        if (space == NULL) break;
        strncpy(value, space+1, lineend - (space+1));
        /*  iZoomIfSmallerThanScreen  */
        if (strcmp(term, "iZoomIfSmallerThanScreen\0") == 0)
           if (strcmp(value, "0\0") == 0)
             CfgDlgInit.ModuleConfig.iZoomIfSmallerThanScreen = 0;
           else
             CfgDlgInit.ModuleConfig.iZoomIfSmallerThanScreen = 1;
        /*  sFileName  */
        if (strcmp(term, "sDirName\0") == 0)
           if (strcmp(value, " \0") == 0)
             sprintf(CfgDlgInit.ModuleConfig.sDirName, "");
           else
             strcpy(CfgDlgInit.ModuleConfig.sDirName, value);
        /*  lBlankTime  */
        if (strcmp(term, "lBlankTime\0") == 0)
           CfgDlgInit.ModuleConfig.lBlankTime = atol(value);
           if (CfgDlgInit.ModuleConfig.lBlankTime == 0) CfgDlgInit.ModuleConfig.lBlankTime = 120000;
        /*  lSwitchTime  */
        if (strcmp(term, "lSwitchTime\0") == 0)
           CfgDlgInit.ModuleConfig.lSwitchTime = atol(value);
           if (CfgDlgInit.ModuleConfig.lSwitchTime == 0) CfgDlgInit.ModuleConfig.lSwitchTime = 60000;
        /*  bRandom  */
        if (strcmp(term, "bRandom\0") == 0)
           if (strcmp(value, "TRUE\0") == 0)
             CfgDlgInit.ModuleConfig.bRandom = TRUE;
           else
             CfgDlgInit.ModuleConfig.bRandom = FALSE;
             
        line = lineend+1;
        lineend = strstr(line, "\n");
        free(term);
        free(value);
      }
      fclose(hFile);
    }
  }
}

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

static void internal_AdjustConfigDialogControls(HWND hwnd) {
	SWP swpTemp, swpTemp2, swpWindow;
	int staticheight, checkboxheight, buttonheight;
	int iCX, iCX2, iCX1, iCY, iBottomButtonsWidth;
	int iCBCX, iCBCY; // Checkbox extra sizes
	ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
	ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);

	// Get window size!
	WinQueryWindowPos(hwnd, &swpWindow);
	internal_GetCheckboxExtraSize(hwnd, CB_ZOOMIFSMALLERTHANSCREEN, &iCBCX, &iCBCY);

	// Resize the window
	internal_GetStaticTextSize(hwnd, PB_CONFOK, &iCX1, &iCY);
	internal_GetStaticTextSize(hwnd, PB_CONFCANCEL, &iCX2, &iCY);
	internal_GetStaticTextSize(hwnd, CB_ZOOMIFSMALLERTHANSCREEN, &iCX, &iCY);
	WinSetWindowPos(hwnd, HWND_TOP, 0, 0, iCX+iCX1+iCX2, 8.5*(iCY+CYDLGFRAME*4)+WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR), SWP_SIZE);
	
	// Get window size!
	WinQueryWindowPos(hwnd, &swpWindow);
	
	// Query static height in pixels.
	internal_GetStaticTextSize(hwnd, ST_MINUTESMSG, &iCX, &iCY);
	staticheight = iCY;

	// Query button height in pixels.
	WinQueryWindowPos(WinWindowFromID(hwnd, PB_CONFOK), &swpTemp);
	buttonheight = swpTemp.cy;

	// Query checkbox button height in pixels.
	WinQueryWindowPos(WinWindowFromID(hwnd, CB_ZOOMIFSMALLERTHANSCREEN), &swpTemp);
	checkboxheight = swpTemp.cy;
	//swpWindow.cx = swpTemp.cx + 10*CXDLGFRAME;

	// Set the position of buttons
	internal_GetStaticTextSize(hwnd, PB_CONFCANCEL, &iCX, &iCY);
	iBottomButtonsWidth = iCX;
	internal_GetStaticTextSize(hwnd, PB_CONFOK, &iCX, &iCY);
	if (iBottomButtonsWidth<iCX) iBottomButtonsWidth = iCX;

	WinSetWindowPos(WinWindowFromID(hwnd, PB_CONFOK), HWND_TOP, CXDLGFRAME*2, CYDLGFRAME*2, iBottomButtonsWidth + 6*CXDLGFRAME, iCY + 3*CYDLGFRAME, SWP_MOVE | SWP_SIZE);
	WinSetWindowPos(WinWindowFromID(hwnd, PB_CONFCANCEL), HWND_TOP, swpWindow.cx - CXDLGFRAME*2 - (iBottomButtonsWidth + 6*CXDLGFRAME), CYDLGFRAME*2, iBottomButtonsWidth + 6*CXDLGFRAME, iCY + 3*CYDLGFRAME, SWP_MOVE | SWP_SIZE);

	// Set the entry field and the Browse button
	internal_GetStaticTextSize(hwnd, PB_BROWSE, &iCX, &iCY);
	iBottomButtonsWidth = iCX;
	WinSetWindowPos(WinWindowFromID(hwnd, EF_FILENAME), HWND_TOP, CXDLGFRAME*2 + 2, CYDLGFRAME*2+buttonheight+ CYDLGFRAME*4+5, swpWindow.cx-(iBottomButtonsWidth + 6*CXDLGFRAME)-(CXDLGFRAME*2 + 2) - (2*CXDLGFRAME*2), staticheight, SWP_MOVE | SWP_SIZE);
	WinSetWindowPos(WinWindowFromID(hwnd, PB_BROWSE), HWND_TOP, swpWindow.cx - CXDLGFRAME*2 - (iBottomButtonsWidth + 6*CXDLGFRAME), CYDLGFRAME*2+buttonheight+ CYDLGFRAME*4, iBottomButtonsWidth + 6*CXDLGFRAME, iCY + 3*CYDLGFRAME, SWP_MOVE | SWP_SIZE);

	// Set the text for the entry field
	internal_GetStaticTextSize(hwnd, ST_IMAGE, &iCX, &iCY);
	WinQueryWindowPos(WinWindowFromID(hwnd, EF_FILENAME), &swpTemp2);
	WinSetWindowPos(WinWindowFromID(hwnd, ST_IMAGE), HWND_TOP, swpTemp2.x, swpTemp2.y+CYDLGFRAME*2 + swpTemp2.cy, iCX, iCY, SWP_MOVE | SWP_SIZE);

	// Set checkbox
	internal_GetStaticTextSize(hwnd, CB_ZOOMIFSMALLERTHANSCREEN, &iCX, &iCY);
	WinQueryWindowPos(WinWindowFromID(hwnd, ST_IMAGE), &swpTemp2);
	WinSetWindowPos(WinWindowFromID(hwnd, CB_ZOOMIFSMALLERTHANSCREEN), HWND_TOP, swpTemp2.x, swpTemp2.y+CYDLGFRAME*2 + swpTemp2.cy, iCX + iCBCX, iCY, SWP_MOVE | SWP_SIZE);

	// Set the spinbutton and the text BLANK
	internal_GetStaticTextSize(hwnd, ST_MINUTESMSG1, &iCX, &iCY);
	WinQueryWindowPos(WinWindowFromID(hwnd, SB_TIMER), &swpTemp);
	WinQueryWindowPos(WinWindowFromID(hwnd, CB_ZOOMIFSMALLERTHANSCREEN), &swpTemp2);
	WinSetWindowPos(WinWindowFromID(hwnd, SB_TIMER), HWND_TOP, swpTemp2.x, swpTemp2.y+CYDLGFRAME*2 + swpTemp2.cy, swpTemp.cx, iCY + 3*CYDLGFRAME, SWP_MOVE | SWP_SIZE);
	internal_GetStaticTextSize(hwnd, ST_MINUTESMSG1, &iCX, &iCY);
	WinQueryWindowPos(WinWindowFromID(hwnd, SB_TIMER), &swpTemp);
	WinQueryWindowPos(WinWindowFromID(hwnd, CB_ZOOMIFSMALLERTHANSCREEN), &swpTemp2);
	WinSetWindowPos(WinWindowFromID(hwnd, ST_MINUTESMSG1), HWND_TOP, swpTemp.x + swpTemp.cx + CXDLGFRAME*2, swpTemp2.y+CYDLGFRAME*2 + swpTemp2.cy + (swpTemp.cy-swpTemp2.cy) , iCX, iCY, SWP_MOVE | SWP_SIZE);

	// Set the top text BLANK
	internal_GetStaticTextSize(hwnd, ST_MINUTESMSG, &iCX, &iCY);
	WinQueryWindowPos(WinWindowFromID(hwnd, SB_TIMER), &swpTemp2);
	WinSetWindowPos(WinWindowFromID(hwnd, ST_MINUTESMSG), HWND_TOP, swpTemp2.x, swpTemp2.y+CYDLGFRAME*2 + swpTemp2.cy, iCX, iCY, SWP_MOVE | SWP_SIZE);

	// Set the spinbutton and the text SWITCH
	internal_GetStaticTextSize(hwnd, ST_SWITCHMSG1, &iCX, &iCY);
	WinQueryWindowPos(WinWindowFromID(hwnd, SB_TIMER), &swpTemp);
	WinQueryWindowPos(WinWindowFromID(hwnd, ST_MINUTESMSG), &swpTemp2);
	WinSetWindowPos(WinWindowFromID(hwnd, SB_SWITCH), HWND_TOP, swpTemp2.x, swpTemp2.y+CYDLGFRAME*2 + swpTemp2.cy, swpTemp.cx, iCY + 3*CYDLGFRAME, SWP_MOVE | SWP_SIZE);
	internal_GetStaticTextSize(hwnd, ST_SWITCHMSG1, &iCX, &iCY);
	WinQueryWindowPos(WinWindowFromID(hwnd, SB_TIMER), &swpTemp);
	WinQueryWindowPos(WinWindowFromID(hwnd, ST_MINUTESMSG), &swpTemp2);
	WinSetWindowPos(WinWindowFromID(hwnd, ST_SWITCHMSG1), HWND_TOP, swpTemp.x + swpTemp.cx + CXDLGFRAME*2, swpTemp2.y+CYDLGFRAME*2 + swpTemp2.cy + (swpTemp.cy-swpTemp2.cy) , iCX, iCY, SWP_MOVE | SWP_SIZE);

	// Set the top text SWITCH
	internal_GetStaticTextSize(hwnd, ST_SWITCHMSG, &iCX, &iCY);
	WinQueryWindowPos(WinWindowFromID(hwnd, SB_SWITCH), &swpTemp2);
	WinSetWindowPos(WinWindowFromID(hwnd, ST_SWITCHMSG), HWND_TOP, swpTemp2.x, swpTemp2.y+CYDLGFRAME*2 + swpTemp2.cy, iCX, iCY, SWP_MOVE | SWP_SIZE);

	// Set checkbox (random order)
	internal_GetStaticTextSize(hwnd, CB_RANDOMORDER, &iCX, &iCY);
	WinQueryWindowPos(WinWindowFromID(hwnd, ST_SWITCHMSG), &swpTemp2);
	WinSetWindowPos(WinWindowFromID(hwnd, CB_RANDOMORDER), HWND_TOP, swpTemp2.x, swpTemp2.y+CYDLGFRAME*2 + swpTemp2.cy, iCX + iCBCX, iCY, SWP_MOVE | SWP_SIZE);

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

    // Label for blank time
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0002"))
      WinSetDlgItemText(hwnd, ST_MINUTESMSG, achTemp);
    // Minutes label for blank time
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0003"))
      WinSetDlgItemText(hwnd, ST_MINUTESMSG1, achTemp);
    // zoom if smaller then screen check box
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0004"))
      WinSetDlgItemText(hwnd, CB_ZOOMIFSMALLERTHANSCREEN, achTemp);
    // Display Image label before entry field
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0005"))
      WinSetDlgItemText(hwnd, ST_IMAGE, achTemp);
    //browse button
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0006"))
      WinSetDlgItemText(hwnd, PB_BROWSE, achTemp);
    // OK button
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0007"))
      WinSetDlgItemText(hwnd, PB_CONFOK, achTemp);
    // Cancel button
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0008"))
      WinSetDlgItemText(hwnd, PB_CONFCANCEL, achTemp);
    // Label for switch time
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0009"))
      WinSetDlgItemText(hwnd, ST_SWITCHMSG, achTemp);
    // Random order checkbox
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0010"))
      WinSetDlgItemText(hwnd, CB_RANDOMORDER, achTemp);
    // Minutes label for blank time
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0003"))
      WinSetDlgItemText(hwnd, ST_SWITCHMSG1, achTemp);
    internal_CloseNLSFile(hfNLSFile);
  }
}


MRESULT EXPENTRY dirDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2) 
{
  switch(msg)
    {
    case WM_COMMAND:
      {
        if(SHORT1FROMMP(mp1)==DID_OK)
          {
            FILEDLG *fd;
            fd=(FILEDLG*)WinQueryWindowULong(hwnd, QWL_USER);
            WinQueryWindowText(WinWindowFromID(hwnd, 4096), CCHMAXPATH, fd->szFullFile);
            if(fd->szFullFile[strlen(fd->szFullFile)-1]=='\\')
              fd->szFullFile[strlen(fd->szFullFile)-1]=0;
            /*    WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, fd->szFullFile, "", 123, MB_MOVEABLE|MB_OK); */
            fd->lReturn=DID_OK;
            WinDismissDlg(hwnd, DID_OK);
            return (MRESULT)FALSE;
          }
        break;
      }
    case WM_CONTROL:
      {
        if(SHORT2FROMMP(mp1)==CBN_EFCHANGE) {
          FILEDLG *fd;
          char text[CCHMAXPATH];

          fd=(FILEDLG*)WinQueryWindowULong(hwnd, QWL_USER);
          strncpy(text, fd->szFullFile,CCHMAXPATH);
          text[CCHMAXPATH-1]=0;
          text[strlen(text)-1]=0;
          WinSetWindowText(WinWindowFromID(hwnd, 4096), text);
        }
        break;
      }
    case WM_INITDLG:
      {
        MRESULT mr;
#if 0
        FILEDLG *fd;
        
        fd=(FILEDLG*)WinQueryWindowULong(hwnd, QWL_USER);
        if(fd->szFullFile[0]!=0)
          {
            WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, fd->szFullFile, "", 123, MB_MOVEABLE|MB_OK);
            if(fd->szFullFile[strlen(fd->szFullFile)]!='\\') {
              strcat(fd->szFullFile, "\\*.*");
         WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, fd->szFullFile, "", 123, MB_MOVEABLE|MB_OK);
              
            }
          }
#endif
        WinSendMsg(WinWindowFromID(hwnd, 4096), EM_SETTEXTLIMIT, MPFROMSHORT(CCHMAXPATH), 0L);
        mr=WinDefFileDlgProc(hwnd, msg, mp1, mp2);
        WinSetWindowText(WinWindowFromID(hwnd, 258),"");
        WinEnableWindow(WinWindowFromID(hwnd, DID_OK), TRUE);
        return mr;
      }
    default:
      break;
    }
  return WinDefFileDlgProc(hwnd, msg, mp1, mp2);
}
