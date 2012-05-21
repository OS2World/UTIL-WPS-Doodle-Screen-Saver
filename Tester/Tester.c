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

#define INCL_DOS
#define INCL_WIN
#define INCL_DOSMISC
#define INCL_ERRORS
#include <os2.h>
#include <process.h>

#define PREVIEW_CLASS          "SSaverPreviewWindowClass"
#define PREVIEWAREA_NOPREVIEW  "No preview"

#include "SSModule.h"
#include "Tester.h"

#define WM_REQUESTDONE       (0xD00D)

HAB hab;
HWND hwndDlg, hwndPreview;
ULONG ulTimer;
char *pchDLLName;

SSModuleDesc_t SSModuleDesc;

HMODULE hmodSaverDLL;
pfn_SSModule_GetModuleDesc                 pfnGetModuleDesc;
pfn_SSModule_Configure                     pfnConfigure;
pfn_SSModule_StartSaving                   pfnStartSaving;
pfn_SSModule_StopSaving                    pfnStopSaving;
pfn_SSModule_AskUserForPassword            pfnAskPwd;
pfn_SSModule_ShowWrongPasswordNotification pfnShowWrongPwd;

//#define DEBUG_LOGGING
void AddLog(char *pchMsg)
{
#ifdef DEBUG_LOGGING
  FILE *hFile;

  hFile = fopen("e:\\tester.log", "at+");
  if (hFile)
  {
    fprintf(hFile, "%s", pchMsg);
    fclose(hFile);
  }
#endif
}

int LoadModule(char *pchFileName)
{
  char achFailure[1024];
  ULONG ulrc;

  ulrc = DosLoadModule(achFailure, sizeof(achFailure),
                       pchFileName,
                       &hmodSaverDLL);
  if (ulrc!=NO_ERROR)
  {
    // Error, could not start screensaver!
    WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                  achFailure,
                  "Error loading DLL",
                  0,
                  MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);

    return 0;
  } else
  {
    // Cool, DLL could be loaded!
    // Query function pointers!
    ulrc = DosQueryProcAddr(hmodSaverDLL,
                            0,
                            "SSModule_GetModuleDesc",
                            (PFN *) &pfnGetModuleDesc);
    if (ulrc!=NO_ERROR) pfnGetModuleDesc = NULL;

    ulrc = DosQueryProcAddr(hmodSaverDLL,
                            0,
                            "SSModule_Configure",
                            (PFN *) &pfnConfigure);
    if (ulrc!=NO_ERROR) pfnConfigure = NULL;

    ulrc = DosQueryProcAddr(hmodSaverDLL,
                            0,
                            "SSModule_StartSaving",
                            (PFN *) &pfnStartSaving);
    if (ulrc!=NO_ERROR) pfnStartSaving = NULL;
  
    ulrc = DosQueryProcAddr(hmodSaverDLL,
                            0,
                            "SSModule_StopSaving",
                            (PFN *) &pfnStopSaving);
    if (ulrc!=NO_ERROR) pfnStopSaving = NULL;

    ulrc = DosQueryProcAddr(hmodSaverDLL,
                            0,
                            "SSModule_AskUserForPassword",
                            (PFN *) &pfnAskPwd);
    if (ulrc!=NO_ERROR) pfnAskPwd = NULL;

    ulrc = DosQueryProcAddr(hmodSaverDLL,
                            0,
                            "SSModule_ShowWrongPasswordNotification",
                            (PFN *) &pfnShowWrongPwd);
    if (ulrc!=NO_ERROR) pfnShowWrongPwd = NULL;


    if ((!pfnStartSaving) || (!pfnStopSaving) ||
        (!pfnAskPwd) || (!pfnShowWrongPwd) ||
        (!pfnGetModuleDesc) || (!pfnConfigure))
    {
      WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                    "Error querying functions from DLL!",
                    "Error loading DLL",
                    0,
                    MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
      DosFreeModule(hmodSaverDLL);
      hmodSaverDLL = NULL;
      return 0;
    }
  }
  return 1;
}

void UnloadModule()
{
  if (hmodSaverDLL)
  {
    DosFreeModule(hmodSaverDLL); hmodSaverDLL = NULL;
  }
}


typedef struct ModuleStarterParm_s
{
  HWND hwndToNotify;
  char *pchHomeDirectory;
  HWND hwndParent;
  int bPreviewMode;
  int bPwdProtected;
} ModuleStarterParm_t, *ModuleStarterParm_p;

void ModuleStarterThread(void *pParm)
{
  ModuleStarterParm_p pStart = pParm;
  int rc;
  HMQ hmq;

  hmq = WinCreateMsgQueue(hab, 0);

  AddLog("Calling pfnStartSaving.\n");
  AddLog("Home dir: "); AddLog(pStart->pchHomeDirectory); AddLog("\n");
  if (pStart->bPreviewMode)
    AddLog("Will be in preview mode\n");
  else
    AddLog("Will be in real saver mode\n");

  rc = pfnStartSaving(pStart->hwndParent,
                      pStart->pchHomeDirectory,
                      pStart->bPreviewMode);

  AddLog("pfnStartSaving returned.\n");

  if (rc==SSMODULE_NOERROR)
  {
    if (!(pStart->bPreviewMode))
    do
    {
      AddLog("Waiting 5 secs\n");
      DosSleep(5000);
      if (pStart->bPwdProtected)
      {
        char achTemp[256];

        AddLog("Asking password\n");

        rc = pfnAskPwd(sizeof(achTemp), achTemp);
        if (rc!=SSMODULE_NOERROR)
        {
          AddLog("Error while asking password, stopping saving\n");
          pfnStopSaving(); // Stop saving
          rc = 1;
        } else
        {
          rc = 0;
          if (rc!=SSMODULE_ERROR_USERPRESSEDCANCEL)
          {
            AddLog("Comparing password\n");
            if (strcmp(achTemp, "password"))
            {
              AddLog("Wrong password! Show info!\n");
              pfnShowWrongPwd();
            } else
            {
              AddLog("Good password, stop saving!\n");
              pfnStopSaving(); // Stop saving
              rc = 1;
            }
          }
        }
      } else
      {
        AddLog("No pwd prot, stop saving\n");
        pfnStopSaving(); // Stop saving
        rc = 1;
      }
    } while (!rc);
  }

  AddLog("Posting ReqDone\n");
  WinPostMsg(pStart->hwndToNotify, WM_REQUESTDONE, (MPARAM) NULL, (MPARAM) NULL); // Notify caller that we're ready!

  WinDestroyMsgQueue(hmq);
  _endthread();
}

void ModuleStopperThread(void *pParm)
{
  ModuleStarterParm_p pStart = pParm;
  int rc;

  rc = pfnStopSaving();

  {
    char achTemp[256];
    sprintf(achTemp, "StopSaving, rc = %d\n", rc);
    AddLog(achTemp);
  }
         
  WinPostMsg(pStart->hwndToNotify, WM_REQUESTDONE, (MPARAM) NULL, (MPARAM) NULL); // Notify caller that we're ready!

  _endthread();
}

void StartSaving(HWND hwndToNotify, HWND hwndParent, char *pchHomeDirectory, int bPreviewMode, int bPwdProtected)
{
  ModuleStarterParm_t StartParm;
  HAB hab = WinQueryAnchorBlock(hwndParent);
  QMSG qmsg;

  WinEnableWindow(WinWindowFromID(hwndToNotify, PB_CONFIGURE), FALSE);
  WinEnableWindow(WinWindowFromID(hwndToNotify, PB_TEST), FALSE);

  UnloadModule();
  LoadModule(pchDLLName);

  StartParm.pchHomeDirectory = pchHomeDirectory;
  StartParm.hwndParent = hwndParent;
  StartParm.hwndToNotify = hwndToNotify;
  StartParm.bPreviewMode = bPreviewMode;
  StartParm.bPwdProtected = bPwdProtected;

  _beginthread(ModuleStarterThread,
               0,
               1024*1024,
               &StartParm);

  AddLog("Entering StartSaving msg loop\n");
  while (WinGetMsg(hab, &qmsg, NULL, 0, 0))
  {
    if (qmsg.msg == WM_REQUESTDONE)
    {
      break;
    } else
      WinDispatchMsg(hab, &qmsg);
  }
  AddLog("Leaving StartSaving\n");

  WinEnableWindow(WinWindowFromID(hwndToNotify, PB_TEST), TRUE);
  WinEnableWindow(WinWindowFromID(hwndToNotify, PB_CONFIGURE), SSModuleDesc.bConfigurable);
}

void StopSaving(HWND hwndToNotify)
{
  ModuleStarterParm_t StartParm;
  HAB hab = WinQueryAnchorBlock(hwndToNotify);
  QMSG qmsg;

  WinEnableWindow(WinWindowFromID(hwndToNotify, PB_CONFIGURE), FALSE);
  WinEnableWindow(WinWindowFromID(hwndToNotify, PB_TEST), FALSE);

  StartParm.hwndToNotify = hwndToNotify;

  _beginthread(ModuleStopperThread,
               0,
               1024*1024,
               &StartParm);

  while (WinGetMsg(hab, &qmsg, NULL, 0, 0))
  {
    if (qmsg.msg == WM_REQUESTDONE)
    {
      break;
    } else
      WinDispatchMsg(hab, &qmsg);
  }

  UnloadModule();
  LoadModule(pchDLLName);

  WinEnableWindow(WinWindowFromID(hwndToNotify, PB_TEST), TRUE);
  WinEnableWindow(WinWindowFromID(hwndToNotify, PB_CONFIGURE), SSModuleDesc.bConfigurable);
}

MRESULT EXPENTRY PreviewWindowProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch( msg )
  {
    case WM_PAINT:
      {
	HPS    hpsBP;
	RECTL  rc;

        hpsBP = WinBeginPaint(hwnd, 0L, &rc);
        WinQueryWindowRect(hwnd, &rc);
	WinDrawText(hpsBP,
		    strlen(PREVIEWAREA_NOPREVIEW), PREVIEWAREA_NOPREVIEW,
		    &rc,
		    SYSCLR_WINDOWTEXT, // Foreground color
		    SYSCLR_WINDOW,     // Background color
                    DT_ERASERECT | DT_CENTER | DT_VCENTER);
	WinEndPaint(hpsBP);
	break;
      }
    default:
      break;
  }

  return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}

MRESULT EXPENTRY TesterDialogProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  int rc;
  char achTemp[32];
  SWP swpTemp, swpTemp2;

  switch( msg )
  {
    case WM_INITDLG:

      // Create extra child window!
      WinQueryWindowPos(hwnd, &swpTemp);
      WinQueryWindowPos(WinWindowFromID(hwnd, MLE_MODULEDESC), &swpTemp2);

      hwndPreview = WinCreateWindow(hwnd, PREVIEW_CLASS, "Preview area",
                                    WS_VISIBLE,
                                    swpTemp2.x + swpTemp2.cx + 10,
                                    swpTemp2.y,
                                    swpTemp.cx - 20 - swpTemp2.x - swpTemp2.cx,
                                    swpTemp2.cy,
                                    hwnd, HWND_TOP,
                                    0x1000,
                                    NULL, NULL);

      rc = pfnGetModuleDesc(&SSModuleDesc, ".\\");
      if (rc==SSMODULE_NOERROR)
      {
        WinSetDlgItemText(hwnd, EF_NAME, SSModuleDesc.achModuleName);
        WinSetDlgItemText(hwnd, MLE_MODULEDESC, SSModuleDesc.achModuleDesc);
        sprintf(achTemp, "%d.%02d", SSModuleDesc.iVersionMajor, SSModuleDesc.iVersionMinor);
        WinSetDlgItemText(hwnd, EF_VERSION, achTemp);
        if (SSModuleDesc.bSupportsPasswordProtection)
          WinSetDlgItemText(hwnd, EF_PWDPROT, "YES");
        else
          WinSetDlgItemText(hwnd, EF_PWDPROT, "NO");

        WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE), SSModuleDesc.bConfigurable);

        StartSaving(hwnd, WinWindowFromID(hwnd, 0x1000), ".\\", TRUE, FALSE); // Start perview mode!

      } else
      {
        WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                      "Error in pfnGetModuleDesc()!",
                      "Error in SSModule API",
                      0,
                      MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
      }
      return (MRESULT) FALSE;

    case WM_COMMAND:
      switch SHORT1FROMMP(mp2) {
	case CMDSRC_PUSHBUTTON:           // ---- A WM_COMMAND from a pushbutton ------
	  switch (SHORT1FROMMP(mp1)) {
            case PB_CONFIGURE:

              WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE), FALSE);
              WinEnableWindow(WinWindowFromID(hwnd, PB_TEST), FALSE);

              pfnConfigure(hwnd, ".\\");

              WinEnableWindow(WinWindowFromID(hwnd, PB_TEST), TRUE);
              WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE), SSModuleDesc.bConfigurable);

              StopSaving(hwnd); // Stop perview mode!
              // Restart preview mode, so the module will pick up changes
              StartSaving(hwnd, WinWindowFromID(hwnd, 0x1000), ".\\", TRUE, FALSE); // Start perview mode!
              return (MRESULT) TRUE;
	    case PB_TEST:

	      StopSaving(hwnd); // Stop perview mode!

	      // Start fullscreen mode
              StartSaving(hwnd, HWND_DESKTOP, ".\\", FALSE, WinQueryButtonCheckstate(hwnd, CB_PASSWORDPROTECTEDTEST));

              AddLog("Restarting preview mode\n");
              // Restart preview mode after it
              StartSaving(hwnd, WinWindowFromID(hwnd, 0x1000), ".\\", TRUE, FALSE); // Start perview mode!

              return (MRESULT) TRUE;
	    default:
	      break;
	  }
        default:
          break;
      }
      break;

    case WM_DESTROY:
      StopSaving(hwnd);
      break;

    default:
      break;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}

int main(int argc, char *argv[])
{
  HMQ hmq;

  hab = WinInitialize(0);
  hmq = WinCreateMsgQueue(hab, 0);

  if (argc<2)
  {
    WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                  "Usage: tester <module.dll>",
                  "Screen Saver module tester",
                  0,
                  MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
  } else
  {
    pchDLLName = argv[1];
    if (LoadModule(pchDLLName))
    {
      WinRegisterClass(hab, (PSZ) PREVIEW_CLASS,
                       (PFNWP) PreviewWindowProc,
                       CS_SIZEREDRAW | CS_CLIPCHILDREN, 0);

      hwndDlg = WinLoadDlg(HWND_DESKTOP, HWND_DESKTOP,
                           //WinDefDlgProc,
                           TesterDialogProc,
                           NULLHANDLE,
                           DLG_SCREENSAVERMODULETESTER, NULL);

      WinProcessDlg(hwndDlg);

      AddLog("WinDestroyWindow()\n");
      WinDestroyWindow(hwndDlg);

      AddLog("UnloadModule()\n");
      UnloadModule();
    }
  }

  WinDestroyMsgQueue(hmq);
  WinTerminate(hab);
  return 1;
}
