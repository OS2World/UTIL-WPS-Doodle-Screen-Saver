/*
 * Screen Saver - Moving Text Module for Doodle's Screen Saver
 * Copyright (C) 2004-2008 Doodle
 * Copyright (C) 2004-2008 Warp5
 *
 * Contact: doodle@dont.spam.me.scenergy.dfmk.hu
 *          os2info@dont.spam.me.gmx.net
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
#include <time.h>
#define INCL_DOS
#define INCL_WIN
#define INCL_GPI
#define INCL_ERRORS
#define INCL_GPIPRIMITIVES
#include <os2.h>
#include "SSModule.h"
#include "Text-Resources.h"
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
#define SAVERWINDOW_CLASS "TEXT_SCREENSAVER_CLASS"

// Private window messages
#define WM_SUBCLASS_INIT       (WM_USER+1)
#define WM_SUBCLASS_UNINIT     (WM_USER+2)
#define WM_ASKPASSWORD         (WM_USER+3)
#define WM_SHOWWRONGPASSWORD   (WM_USER+4)

HMODULE hmodOurDLLHandle;
int     bRunning;
int     bFirstKeyGoesToPwdWindow = 0;
TID     tidSaverThread;
int     iSaverThreadState;
HWND    hwndSaverWindow;
HMQ     hmqSaverThreadMsgQueue;
int     bOnlyPreviewMode;

ULONG   ulAutoHiderTimerID;

char   *apchNLSText[SSMODULE_NLSTEXT_MAX+1];
HMTX    hmtxUseNLSTextArray;

/* fires to update the position of the text on screen */
ULONG   ulAnimationTimerID;

/* coordinates of the moving text */
int     textX, textY;

/* the configuration structure */
typedef struct ConfigDlgInitParams_s {
  USHORT          cbSize; // Required to be able to pass this structure to WM_INITDLG
  char           *pchText; // the text that scrolls on the screen
  char           *pchHomeDirectory;
} ConfigDlgInitParams_t, *ConfigDlgInitParams_p;

ConfigDlgInitParams_t CfgDlgInit;

/* Functions */
void readConfig (char *pchHomeDirectory);
static void internal_SetConfigDialogText(HWND hwnd, char *pchHomeDirectory);
static void internal_AdjustConfigDialogControls(HWND hwnd);
static void internal_GetStaticTextSize(HWND hwnd, ULONG ulID, int *piCX, int *piCY);
MRESULT EXPENTRY fnConfigDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);


#ifdef DEBUG_BUILD
	#define DEBUG_LOGGING
#endif

#ifdef DEBUG_LOGGING
	void AddLog(char *pchMsg);
	void AddLogInt(int number);

	void AddLog(char *pchMsg) {
		FILE *hFile;

		hFile = fopen(DEBUG_LOG_FILE, "at+");
		if (hFile) {
			fprintf(hFile, "%s", pchMsg);
			fclose(hFile);
		}
	}

	void AddToLogInt(long number) {
		FILE *hFile;

		hFile = fopen(DEBUG_LOG_FILE, "at+");
		if (hFile) {
			fprintf(hFile, "%d\n", number);
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

static void internal_GetStaticTextSize(HWND hwnd, ULONG ulID, int *piCX, int *piCY) {
	HPS hps;
	char achTemp[512];
	POINTL aptl[TXTBOX_COUNT];

	// This calculates how much space is needed for a given static text control
	// to hold its text.
	WinQueryDlgItemText(hwnd, ulID, sizeof(achTemp), achTemp);

	hps = WinGetPS(WinWindowFromID(hwnd, ulID));
	GpiQueryTextBox(hps, strlen(achTemp), achTemp, TXTBOX_COUNT, aptl);

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

  switch( msg )
  {
    case WM_SUBCLASS_INIT:
		case WM_CREATE: {
			RECTL rclRect;
			// Any initialization of the window and variables should come here.

			// the timer for the moving text
			ulAnimationTimerID = WinStartTimer(WinQueryAnchorBlock(hwnd), hwnd, ANIMATION_TIMER_ID, 20);

			// setup the initial coordinates for the scrolling text
			WinQueryWindowRect(hwnd, &rclRect);
			textX = rclRect.xRight;
			textY = rclRect.yTop/2;

			// Hide mouse pointer, if we're in real screen-saving mode!
			if (!bOnlyPreviewMode) WinShowPointer(HWND_DESKTOP, FALSE);
			// Initialize WMCHAR record
			internal_InitWMCHARRecord();
		break; }

		case WM_CHAR:
		if (!bOnlyPreviewMode) internal_SaveWMCHAREventToRecord(mp1, mp2);
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
		if (bFirstKeyGoesToPwdWindow) internal_ReplayWMCHARRecord();

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

    case WM_SUBCLASS_UNINIT:
    case WM_DESTROY:
      // All kinds of cleanup (the opposite of everything done in WM_CREATE)
      // should come here.


      WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, ulAnimationTimerID);


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
		case WM_PAINT: {
			HPS hpsBeginPaint;
			RECTL rclRect, blank, rclWindow;
			FONTMETRICS fm;
			int len;

			#ifdef DEBUG_LOGGING
				AddLog("WM_PAINT\n");
			#endif

			hpsBeginPaint = WinBeginPaint(hwnd, NULLHANDLE, &rclRect);
			WinQueryWindowRect(hwnd, &rclRect);
			WinQueryWindowRect(hwnd, &rclWindow);

			WinSetWindowFontMy(hwnd, "10.Courier");	
			GpiQueryFontMetrics(hpsBeginPaint, sizeof(fm), &fm);

			rclRect.xLeft = textX;
			rclRect.yTop  = textY;
			len = strlen(CfgDlgInit.pchText);
			rclRect.xRight = rclRect.xLeft + fm./*lAveCharWidth*/lMaxCharInc * len+5;
			rclRect.yBottom = rclRect.yTop - fm.lMaxBaselineExt;
			WinDrawText(hpsBeginPaint, len ,CfgDlgInit.pchText, &rclRect, CLR_WHITE, CLR_BLACK, DT_ERASERECT | DT_LEFT | DT_VCENTER);
			/* paint the black around the text*/
			blank.xLeft = 0; blank.yBottom = 0; blank.xRight = rclRect.xLeft; blank.yTop = rclWindow.yTop; WinFillRect(hpsBeginPaint, &blank, CLR_BLACK);
			blank.xLeft = rclRect.xLeft-2; blank.yBottom = rclRect.yTop; blank.xRight = rclWindow.xRight; blank.yTop = rclWindow.yTop; WinFillRect(hpsBeginPaint, &blank, CLR_BLACK);
			blank.xLeft = rclRect.xRight; blank.yBottom = 0; blank.xRight = rclWindow.xRight; blank.yTop = rclRect.yTop; WinFillRect(hpsBeginPaint, &blank, CLR_BLACK);
			blank.xLeft = rclRect.xLeft-2; blank.yBottom = 0; blank.xRight = rclRect.xRight+2; blank.yTop = rclRect.yBottom; WinFillRect(hpsBeginPaint, &blank, CLR_BLACK);
			if (rclRect.xRight <= -10) {
				WinQueryWindowRect(hwnd, &rclRect);
				textX = rclRect.xRight;
				textY = rclRect.yTop/2;
			}
			WinEndPaint(hpsBeginPaint);

			#ifdef DEBUG_LOGGING
				AddLog("WM_PAINT done\n");
			#endif
			return (MRESULT) FALSE;
		}

     case WM_TIMER:
      if (((SHORT)mp1)==ANIMATION_TIMER_ID)
      {
	// Timer, so make new image position
	textX -= 2;
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
	sprintf(pchFileName, "%sModules\\Text\\%s.msg", pchHomeDirectory, achRealLocaleName);
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
	  sprintf(pchFileName, "%sModules\\Text\\%s.msg", pchHomeDirectory, pchLang);
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
  pModuleDesc->iVersionMajor = 1;
  pModuleDesc->iVersionMinor = 70;
  strcpy(pModuleDesc->achModuleName, "Moving Text");
  strcpy(pModuleDesc->achModuleDesc,
         "This modules moves text on the screen!\n"
         "Written by Warp5"
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

  readConfig(pchHomeDirectory);
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
      strncat(chFileName, ".dssaver\\Text.CFG", sizeof(chFileName));
      // Try to open that config file!
      hFile = fopen(chFileName, "wt");
    }

    if (!hFile) {
      // If we could not open a config file in the HOME directory, or there was
      // no HOME directory, then try to use the DSSaver Global home directory!
      memset(chFileName, 0 , sizeof(chFileName));
      snprintf(chFileName, sizeof(chFileName), "%sText.CFG", pchHomeDirectory);
      hFile = fopen(chFileName, "wt");
    }

    if (!hFile) {
      // Could not create config file
      //DosBeep(1400,200);
    } else {
      memset(buffer,0, sizeof(buffer));
      sprintf(buffer, "sText = %s\n", CfgDlgInit.pchText);
      chars = fwrite(buffer, sizeof(char), strlen(buffer), hFile);
      if (chars != strlen(buffer)) {
        // bad file
        //DosBeep(1400,200);
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

  readConfig(pchHomeDirectory);

  iSaverThreadState = STATE_UNKNOWN;
  bOnlyPreviewMode = bPreviewMode;
  tidSaverThread = _beginthread(fnSaverThread,
                                0,
                                1024*1024,
                                (void *) hwndParent);

  if (tidSaverThread == 0)
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
  free(CfgDlgInit.pchText);
  CfgDlgInit.pchText = NULL;

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


/* WinSetWindowFontMy */
VOID WinSetWindowFontMy(HWND hwnd, PSZ pszFontName) {
	BOOL brc = FALSE;

	/* pszFontName format - "10.Helv"  "12.Tms Rmn" "14.Tms Rmn" */

	brc = WinSetPresParam (hwnd, PP_FONTNAMESIZE, (ULONG)((ULONG)strlen(pszFontName)+(ULONG)1), (PSZ)pszFontName);
	return;
}

MRESULT EXPENTRY fnConfigDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  HWND hwndOwner;
  SWP swpDlg, swpOwner;
  POINTL ptlTemp;
  ConfigDlgInitParams_p pCfgDlgInit;

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
      WinSendDlgItemMsg(hwnd, EF_TEXT, EM_SETTEXTLIMIT, MPFROMSHORT(256), NULL);
      WinSetWindowText(WinWindowFromID(hwnd, EF_TEXT), pCfgDlgInit->pchText);
      internal_AdjustConfigDialogControls(hwnd);
     return (MRESULT) FALSE;

    case WM_COMMAND:
        switch (SHORT1FROMMP(mp1)) {
          case PB_CONFOK:
            pCfgDlgInit = (ConfigDlgInitParams_p) WinQueryWindowULong(hwnd, QWL_USER);
            if (pCfgDlgInit) {
               char buff[255];
               buff[0]= '\0';
               WinQueryWindowText(WinWindowFromID(hwnd, EF_TEXT), 248, &buff);
               strcpy(pCfgDlgInit->pchText, buff);
               /*AddLog(buff);
               AddLog(pCfgDlgInit->pchText);*/

            }
          break;

          case PB_CONFCANCEL:
            // restore the old image
            pCfgDlgInit = (ConfigDlgInitParams_p) WinQueryWindowULong(hwnd, QWL_USER);
            if (pCfgDlgInit) {
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
  if (CfgDlgInit.pchText == NULL) CfgDlgInit.pchText = (char *)calloc(250,1);
  strcpy(CfgDlgInit.pchText, "OS/2...OS/2");

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
    strncat(chFileName, ".dssaver\\Text.CFG", sizeof(chFileName));
    // Try to open that config file!
    hFile = fopen(chFileName, "r");
  }

  if (!hFile)
  {
    // If we could not open a config file in the HOME directory, or there was
    // no HOME directory, then try to use the DSSaver Global home directory!
    memset(chFileName, 0 , sizeof(chFileName));
    snprintf(chFileName, sizeof(chFileName), "%sText.CFG", pchHomeDirectory);
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
        /* sText */
        if (strcmp(term, "sText\0") == 0)
            strcpy(CfgDlgInit.pchText, value);
        line = lineend+1;
        lineend = strstr(line, "\n");
        free(term);
        free(value);
      }
      fclose(hFile);
    }
  }
}

static void internal_SetConfigDialogText(HWND hwnd, char *pchHomeDirectory) {
	FILE *hfNLSFile;
	char achTemp[128];

	if (!pchHomeDirectory) return;
	hfNLSFile = internal_OpenNLSFile(pchHomeDirectory);
	if (hfNLSFile) {
		// Window title
		if (sprintmsg(achTemp, hfNLSFile, "CFG_0001")) WinSetWindowText(hwnd, achTemp);

		// Label for the moving text label
		if (sprintmsg(achTemp, hfNLSFile, "CFG_0002")) WinSetDlgItemText(hwnd, ST_TEXT, achTemp);

		// OK button
		if (sprintmsg(achTemp, hfNLSFile, "CFG_0003")) WinSetDlgItemText(hwnd, PB_CONFOK, achTemp);
		// Cancel button
		if (sprintmsg(achTemp, hfNLSFile, "CFG_0004")) WinSetDlgItemText(hwnd, PB_CONFCANCEL, achTemp);

		internal_CloseNLSFile(hfNLSFile);
	}
}

static void internal_AdjustConfigDialogControls(HWND hwnd) {
	SWP swpTemp, swpTemp2, swpWindow;
	int staticheight, buttonheight;
	int iCX, iCY, iBottomButtonsWidth;
	ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
	ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
	BOOL rc;

	// Get window size!
	WinQueryWindowPos(hwnd, &swpWindow);

	// Resize the window
	rc = WinQueryWindowPos(WinWindowFromID(hwnd, PB_CONFCANCEL), &swpTemp);
	#ifdef DEBUG_LOGGING
		if (rc) AddLog("TRUE\n"); else AddLog("FALSE\n");
		AddToLogInt(swpTemp.x);
		AddToLogInt(swpTemp.y);
		AddToLogInt(swpTemp.cx);
		AddToLogInt(swpTemp.cy);
	#endif
	internal_GetStaticTextSize(hwnd, PB_CONFCANCEL, &iCX, &iCY);
	iBottomButtonsWidth = iCX;
	internal_GetStaticTextSize(hwnd, PB_CONFOK, &iCX, &iCY);
	if (iBottomButtonsWidth<iCX) iBottomButtonsWidth = iCX;
	WinSetWindowPos(hwnd, HWND_TOP, 0, 0, 4*(iBottomButtonsWidth + 6*CXDLGFRAME), (3.8*(iCY+CYDLGFRAME*3))+(CYDLGFRAME*4)+WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR), SWP_SIZE); /*(swpTemp.cy)+(CYDLGFRAME*4)+WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR), SWP_SIZE);*/

	// Get window size!
	WinQueryWindowPos(hwnd, &swpWindow);

	// Query static height in pixels.
	internal_GetStaticTextSize(hwnd, ST_TEXT, &iCX, &iCY);
	staticheight = iCY;

	// Set the position of buttons
	WinSetWindowPos(WinWindowFromID(hwnd, PB_CONFOK), HWND_TOP, CXDLGFRAME*2, CYDLGFRAME*2, iBottomButtonsWidth + 6*CXDLGFRAME, iCY + 3*CYDLGFRAME, SWP_MOVE | SWP_SIZE);
	WinSetWindowPos(WinWindowFromID(hwnd, PB_CONFCANCEL), HWND_TOP, swpWindow.cx - CXDLGFRAME*2 - (iBottomButtonsWidth + 6*CXDLGFRAME), CYDLGFRAME*2, iBottomButtonsWidth + 6*CXDLGFRAME, iCY + 3*CYDLGFRAME, SWP_MOVE | SWP_SIZE);

	// Query button height in pixels.
	WinQueryWindowPos(WinWindowFromID(hwnd, PB_CONFOK), &swpTemp);
	buttonheight = swpTemp.cy;

	// Set the entry field
	WinSetWindowPos(WinWindowFromID(hwnd, EF_TEXT), HWND_TOP, CXDLGFRAME*2 + 2, CYDLGFRAME*2+buttonheight+ CYDLGFRAME*4+5, swpWindow.cx- (4*CXDLGFRAME*2), staticheight, SWP_MOVE | SWP_SIZE);

	// Set the text for the entry field
	internal_GetStaticTextSize(hwnd, ST_TEXT, &iCX, &iCY);
	WinQueryWindowPos(WinWindowFromID(hwnd, EF_TEXT), &swpTemp2);
	WinSetWindowPos(WinWindowFromID(hwnd, ST_TEXT), HWND_TOP, swpTemp2.x, swpTemp2.y+CYDLGFRAME*2 + swpTemp2.cy, iCX, iCY, SWP_MOVE | SWP_SIZE);
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_SetCommonSetting(int iSettingID, void *pSettingValue) {
	switch (iSettingID) {
		case SSMODULE_COMMONSETTING_FIRSTKEYGOESTOPWDWINDOW:
			bFirstKeyGoesToPwdWindow = (int) pSettingValue;
			return SSMODULE_NOERROR;
		default:
			return SSMODULE_ERROR_NOTSUPPORTED;
	}
}
