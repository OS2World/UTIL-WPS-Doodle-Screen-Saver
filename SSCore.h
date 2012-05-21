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

#ifndef __SSCORE_H__
#define __SSCORE_H__

#include "SSModule.h"

#define SSCORECALL __syscall
#define SSCOREDECLSPEC __declspec(dllexport)

// ----------------- Type and error definitions -----------------------

#define SSCORE_VERSIONMAJOR  2
#define SSCORE_VERSIONMINOR  0

#define SSCORE_MAXPASSWORDLEN   128

#define SSCORE_DPMSCAPS_NODPMS         0
#define SSCORE_DPMSCAPS_STATE_STANDBY  1
#define SSCORE_DPMSCAPS_STATE_SUSPEND  2
#define SSCORE_DPMSCAPS_STATE_OFF      4

typedef struct SSCoreInfo_s
{
  int iVersionMajor;
  int iVersionMinor;

  int iDPMSCapabilities; // DPMS capabilities detected
  int bSecurityPresent;  // Security/2 is installed, and can be used.

} SSCoreInfo_t, *SSCoreInfo_p;

typedef struct SSCoreModuleList_s
{
  SSModuleDesc_t  ModuleDesc;
  char            achModuleFileName[CCHMAXPATHCOMP];

  void *pNext;
} SSCoreModuleList_t, *SSCoreModuleList_p;

typedef struct SSCoreSettings_s
{
  int             bEnabled;         // Is saving enabled?

  int             iActivationTime;  // In msecs
  char            achModuleFileName[CCHMAXPATHCOMP]; // DLL filename to use

  int             bPasswordProtected; // Is saving password protected?
  int             bUseCurrentSecurityPassword; // Use password of current security user (only if Security/2 is installed)
                                               // Otherwise, use this password:
  char            achEncryptedPassword[SSCORE_MAXPASSWORDLEN]; // The encrypted password
  int             bUseDelayedPasswordProtection; // Is password protection delayed?
  int             iPasswordDelayTime; // In msecs, (or 0), tells how much to wait after starting saver, to ask password, if activity is detected again.

  int             bUseDPMSStandbyState;
  int             iDPMSStandbyTime;   // In msecs, (or 0), tells how much to wait after starting saver, to set DPMS standby mode (if supported)
  int             bUseDPMSSuspendState;
  int             iDPMSSuspendTime;   // In msecs, (or 0), tells how much to wait after standby mode, to set DPMS suspend mode (if supported)
  int             bUseDPMSOffState;
  int             iDPMSOffTime;       // In msecs, (or 0), tells how much to wait after suspend mode, to set DPMS off mode (if supported)

  int             bWakeByKeyboardOnly; // Is it only a keyboard event which can wake it up?

  int             bFirstKeyEventGoesToPwdWindow; // Should first key event already go to password window, or should it be swallowed?

  int             bDisableVIOPopups;  // Should the VIO Popups be disabled while the screensaver is saving the screen?

  int             bUseModuleWrapper;  // Should we use the module wrapper, so the saver dlls will run in a separate process?

} SSCoreSettings_t, *SSCoreSettings_p;


#define SSCORE_NOERROR                     0
#define SSCORE_ERROR_INVALIDPARAMETER      1
#define SSCORE_ERROR_NOTINITIALIZED        2
#define SSCORE_ERROR_NOTDISABLED           3
#define SSCORE_ERROR_ALREADYINITIALIZED    4
#define SSCORE_ERROR_ACCESSDENIED          5
#define SSCORE_ERROR_COULDNOTLOADMODULE    6
#define SSCORE_ERROR_TIMEOUT               7
#define SSCORE_ERROR_INTERNALERROR       255


// These are posted to the parent of the preview window, as the
// result of SSCore_StartModulePreview() and SSCore_StopModulePreview().
// MP1 contains an ULONG of the result of the call (SSCORE_NOERROR or other constant)
// This messages let the above APIs return the error code.
#define WM_SSCORENOTIFY_PREVIEWSTARTED       (0xD00D)
#define WM_SSCORENOTIFY_PREVIEWSTOPPED       (0xD00E)


// The following lines define the NLS texts known by SSCore. These are the strings
// which will be passed to the saver modules if they support it, so they can show
// messages in the currently set language.
#define SSCORE_NLSTEXT_LANGUAGECODE              0
#define SSCORE_NLSTEXT_PWDPROT_ASK_TITLE         1
#define SSCORE_NLSTEXT_PWDPROT_ASK_TEXT          2
#define SSCORE_NLSTEXT_PWDPROT_ASK_OKBUTTON      3
#define SSCORE_NLSTEXT_PWDPROT_ASK_CANCELBUTTON  4
#define SSCORE_NLSTEXT_PWDPROT_WRONG_TITLE       5
#define SSCORE_NLSTEXT_PWDPROT_WRONG_TEXT        6
#define SSCORE_NLSTEXT_PWDPROT_WRONG_OKBUTTON    7
#define SSCORE_NLSTEXT_FONTTOUSE                 8
#define SSCORE_NLSTEXT_MAX                       8


// Defines for system states:
#define SSCORE_STATE_NORMAL                      0
#define SSCORE_STATE_SAVING                      1


// ----------------- DLL Function Declarations -----------------------

SSCOREDECLSPEC int SSCORECALL SSCore_GetInfo(SSCoreInfo_p pInfo, int iBufSize);

SSCOREDECLSPEC int SSCORECALL SSCore_Initialize(HAB habCaller, char *pchHomeDirectory);
SSCOREDECLSPEC int SSCORECALL SSCore_Uninitialize(void);

SSCOREDECLSPEC int SSCORECALL SSCore_GetListOfModules(SSCoreModuleList_p *ppModuleList);
SSCOREDECLSPEC int SSCORECALL SSCore_FreeListOfModules(SSCoreModuleList_p pModuleList);

SSCOREDECLSPEC int SSCORECALL SSCore_GetCurrentSettings(SSCoreSettings_p pCurrentSettings);
SSCOREDECLSPEC int SSCORECALL SSCore_EncryptPassword(char *pchPassword);
SSCOREDECLSPEC int SSCORECALL SSCore_SetCurrentSettings(SSCoreSettings_p pNewSettings);

SSCOREDECLSPEC int SSCORECALL SSCore_StartSavingNow(int bDontDelayPasswordProtection);
SSCOREDECLSPEC int SSCORECALL SSCore_StartModulePreview(char *pchModuleFileName, HWND hwndParent);
SSCOREDECLSPEC int SSCORECALL SSCore_StopModulePreview(HWND hwndParent);

SSCOREDECLSPEC int SSCORECALL SSCore_ConfigureModule(char *pchModuleFileName, HWND hwndOwner);

SSCOREDECLSPEC int SSCORECALL SSCore_SetNLSText(int iNLSTextID, char *pchNLSText);

SSCOREDECLSPEC int SSCORECALL SSCore_TempDisable(void);
SSCOREDECLSPEC int SSCORECALL SSCore_TempEnable(void);
SSCOREDECLSPEC int SSCORECALL SSCore_GetCurrentState(void);
SSCOREDECLSPEC int SSCORECALL SSCore_WaitForStateChange(int iTimeout, int *piNewState);
SSCOREDECLSPEC int SSCORECALL SSCore_GetInactivityTime(unsigned long *pulMouseInactivityTime,
                                                       unsigned long *pulKeyboardInactivityTime);

#endif
