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
#define INCL_DOS
#define INCL_DOSMISC
#define INCL_WIN
#define INCL_ERRORS
#include <os2.h>

#include "SSCore.h"
#include "SSDPMS/SSDPMS.h"          // ScreenSaver DPMS support
#include "userctl.h"                // Security/2 support
#include "SSModWrp/SSModuleNPipe.h" // Module Wrapper support

#define GLOBAL_VAR  __based( __segname("GLOBAL_DATA_SEG") )

#define ACTION_EVENT_SHARED_SEM_NAME      "\\SEM32\\SSAVER\\USERACT"
#define STATECHANGE_EVENT_SHARED_SEM_NAME "\\SEM32\\SSAVER\\STATCHG"

#define WORKER_STATE_UNKNOWN        0
#define WORKER_STATE_RUNNING        1
#define WORKER_STATE_STOPPED        2

#define WORKER_COMMAND_NONE                  (WM_USER+1)
#define WORKER_COMMAND_SHUTDOWN              (WM_USER+2)
#define WORKER_COMMAND_SENTINELTIMEOUT       (WM_USER+3)
#define WORKER_COMMAND_SENTINELACTIVITY      (WM_USER+4)
#define WORKER_COMMAND_STARTSAVINGNOW        (WM_USER+5)
#define WORKER_COMMAND_STOPSAVINGNOW         (WM_USER+6)
#define WORKER_COMMAND_STARTPREVIEW          (WM_USER+7)
#define WORKER_COMMAND_STOPPREVIEW           (WM_USER+8)

// States of DPMS saving, starting from the first to the last:
#define SSCORE_DPMSSTATE_ON            0
#define SSCORE_DPMSSTATE_STANDBY       1
#define SSCORE_DPMSSTATE_SUSPEND       2
#define SSCORE_DPMSSTATE_OFF           3

// Mouse movement sensitivity:
// (If the mouse moves less than this value, then it won't be treated as movement)
#define SSCORE_MOUSE_MOVEMENT_SENSITIVITY   3


// ---------------------- Shared variables across processes --------------
int GLOBAL_VAR     iDLLUsageCounter = 0;

int GLOBAL_VAR     bGlobalInitialized;          // TRUE if saver service is running
PID GLOBAL_VAR     pidGlobalInitializerClient;  // PID in which the saver thread runs (initializer client)
HAB GLOBAL_VAR     habGlobalInitializerClient;  // HAB -- || --
int GLOBAL_VAR     iGlobalTempDisableCounter;   //

HEV GLOBAL_VAR     hevGlobalSentinelEvent;      // Shared semaphore, posted when the input hook detects an event

int GLOBAL_VAR     iGlobalCurrentState;         // The current state of the screen saving (SSCORE_STATE_NORMAL or SSCORE_STATE_SAVING)
HEV GLOBAL_VAR     hevGlobalStateChangeEvent;   // Shared semaphore, posted when the screen saving state changes

TID GLOBAL_VAR     tidGlobalSentinelThread;     // ThreadID of sentinel thread (the thread which timeouts if no event comes ina given time)
int GLOBAL_VAR     iGlobalSentinelThreadState;  // State of sentinel thread (running, stopped...)
int GLOBAL_VAR     bGlobalSentinelThreadShutdownRequest; // TRUE if the sentinel thread should stop

TID GLOBAL_VAR     tidGlobalWorkerThread;       // ThreadID of worker thread (the thread which calls saver modules)
int GLOBAL_VAR     iGlobalWorkerThreadState;    // State of worker thread
HMQ GLOBAL_VAR     hmqGlobalWorkerThreadCommandQueue;  // Command queue for worker thread

char GLOBAL_VAR    achGlobalHomeDirectory[CCHMAXPATHCOMP]; // Home directory
SSCoreSettings_t GLOBAL_VAR GlobalCurrentSettings;         // Current settings

SHORT GLOBAL_VAR   sGlobalCurrDesktopMouseXPos = 0;
SHORT GLOBAL_VAR   sGlobalCurrDesktopMouseYPos = 0;

unsigned long GLOBAL_VAR ulGlobalLastKeyboardEventTimestamp; // Content of the QSV_MS_COUNT when the last keyboard event happened
unsigned long GLOBAL_VAR ulGlobalLastMouseEventTimestamp;    // Content of the QSV_MS_COUNT when the last mouse event happened

HMODULE GLOBAL_VAR hmodGlobalDLLHandle;                    //

// ---------------------- Per-process variables             --------------
PID     pidThisClient;
int     iThisTempDisableCounter;

int     bThisDPMSSupportPresent; // TRUE, if SSDPMS.DLL could be loaded for this process, and function pointers got.
HMODULE hmodThisSSDPMS;          // NULLHANDLE, or handle of loaded SSDPMS.DLL
pfn_SSDPMS_Initialize       fn_SSDPMS_Initialize;
pfn_SSDPMS_Uninitialize     fn_SSDPMS_Uninitialize;
pfn_SSDPMS_GetCapabilities  fn_SSDPMS_GetCapabilities;
pfn_SSDPMS_SetState         fn_SSDPMS_SetState;
pfn_SSDPMS_GetState         fn_SSDPMS_GetState;

char *apchThisNLSText[SSCORE_NLSTEXT_MAX+1];  // Array of strings to store NLS texts.
                                              // Filled only for the Initializer Client!
HMTX hmtxThisUseNLSTextArray;                 // Mutex semaphore to protect the array

// - - Security/2 support - -
HMODULE hmodThisUSERCTL;                      // Containing the module handle of USERCTL.DLL, if Security/2 is present (NULLHANDLE otherwise)
APIRET APIENTRY (*pfnThisDosUserRegister)(PUSERREGISTER);
APIRET APIENTRY (*pfnThisDosUserQueryCurrent)(PSZ pszUser, ULONG cbUser, PLONG plUid);
// - -                    - -

//#define DEBUG_LOGGING
#ifdef DEBUG_LOGGING
void AddLog(char *pchMsg)
{
  FILE *hFile;

  hFile = fopen("c:\\sscore.log", "at+");
  if (hFile)
  {
    fprintf(hFile, "%s", pchMsg);
    fclose(hFile);
  }
}
#endif

static int internal_CallThroughPipe(void *pOutbuf, ULONG ulOutBytes, void *pInBuf, ULONG ulInBytes)
{
  APIRET rc;
  ULONG ulRead;
  int iResult = SSMODULE_ERROR_INTERNALERROR;

  rc = DosCallNPipe(SSMODULE_NPIPE_SERVER_PATH, pOutbuf, ulOutBytes, pInBuf, ulInBytes, &ulRead, -1);
  if (rc==NO_ERROR)
    iResult = SSMODULE_NOERROR;

  if (ulInBytes == ulRead)
    iResult = SSMODULE_NOERROR;

  return iResult;
}

static int internal_Worker_CallSetNLSText(int bUseModuleWrapper, pfn_SSModule_SetNLSText fnSetNLSText, int iNLSTextID, char *pchNLSText)
{
  int iResult;
  if (!bUseModuleWrapper)
    return fnSetNLSText(iNLSTextID, pchNLSText);
  else
  {
    SSModule_NPipe_SetNLSText_Req_t  request;
    SSModule_NPipe_SetNLSText_Resp_t response;
    request.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_SETNLSTEXT_REQ;
    request.iNLSTextID = iNLSTextID;
    request.bIsNULL = (pchNLSText==NULL);
    if (pchNLSText)
      snprintf(request.achNLSText, sizeof(request.achNLSText), "%s", pchNLSText);
    iResult = internal_CallThroughPipe(&request, sizeof(request), &response, sizeof(response));
    if (iResult==SSMODULE_NOERROR) iResult=response.iResult;

    return iResult;
  }
}

static int internal_Worker_CallStopSaving(int bUseModuleWrapper, pfn_SSModule_StopSaving fnStopSaving)
{
  int iResult;
  if (!bUseModuleWrapper)
    return fnStopSaving();
  else
  {
    SSModule_NPipe_StopSaving_Req_t  request;
    SSModule_NPipe_StopSaving_Resp_t response;
    request.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_STOPSAVING_REQ;
    iResult = internal_CallThroughPipe(&request, sizeof(request), &response, sizeof(response));
    if (iResult==SSMODULE_NOERROR) iResult=response.iResult;
    return iResult;
  }
}

static int internal_Worker_CallStartSaving(int bUseModuleWrapper, pfn_SSModule_StartSaving fnStartSaving, HWND hwndParent, char *pchHomeDirectory, int bPreviewMode)
{
  int iResult;
  if (!bUseModuleWrapper)
    return fnStartSaving(hwndParent, pchHomeDirectory, bPreviewMode);
  else
  {
    SSModule_NPipe_StartSaving_Req_t  request;
    SSModule_NPipe_StartSaving_Resp_t response;
    request.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_STARTSAVING_REQ;
    request.hwndParent = hwndParent;
    snprintf(request.achHomeDirectory, sizeof(request.achHomeDirectory), "%s", pchHomeDirectory);
    request.bPreviewMode = bPreviewMode;
    iResult = internal_CallThroughPipe(&request, sizeof(request), &response, sizeof(response));
    if (iResult==SSMODULE_NOERROR) iResult=response.iResult;
    return iResult;
  }
}

static int internal_Worker_CallPauseSaving(int bUseModuleWrapper, pfn_SSModule_PauseSaving fnPauseSaving)
{
  int iResult;
  if (!bUseModuleWrapper)
    return fnPauseSaving();
  else
  {
    SSModule_NPipe_PauseSaving_Req_t  request;
    SSModule_NPipe_PauseSaving_Resp_t response;
    request.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_PAUSESAVING_REQ;
    iResult = internal_CallThroughPipe(&request, sizeof(request), &response, sizeof(response));
    if (iResult==SSMODULE_NOERROR) iResult=response.iResult;
    return iResult;
  }
}

static int internal_Worker_CallResumeSaving(int bUseModuleWrapper, pfn_SSModule_ResumeSaving fnResumeSaving)
{
  int iResult;
  if (!bUseModuleWrapper)
    return fnResumeSaving();
  else
  {
    SSModule_NPipe_ResumeSaving_Req_t  request;
    SSModule_NPipe_ResumeSaving_Resp_t response;
    request.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_RESUMESAVING_REQ;
    iResult = internal_CallThroughPipe(&request, sizeof(request), &response, sizeof(response));
    if (iResult==SSMODULE_NOERROR) iResult=response.iResult;
    return iResult;
  }
}

static int internal_Worker_CallAskUserForPassword(int bUseModuleWrapper, pfn_SSModule_AskUserForPassword fnAskUserForPassword, int iPwdBuffSize, char *pchPwdBuff)
{
  int iResult;
  if (!bUseModuleWrapper)
    return fnAskUserForPassword(iPwdBuffSize, pchPwdBuff);
  else
  {
    SSModule_NPipe_AskUserForPassword_Req_t  request;
    SSModule_NPipe_AskUserForPassword_Resp_t response;
    request.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_ASKUSERFORPASSWORD_REQ;
    iResult = internal_CallThroughPipe(&request, sizeof(request), &response, sizeof(response));
    if (iResult==SSMODULE_NOERROR)
    {
      iResult=response.iResult;
      snprintf(pchPwdBuff, iPwdBuffSize, "%s", response.achPassword);
    }
    return iResult;
  }
}

static int internal_Worker_CallShowWrongPassword(int bUseModuleWrapper, pfn_SSModule_ShowWrongPasswordNotification fnShowWrongPasswordNotification)
{
  int iResult;
  if (!bUseModuleWrapper)
    return fnShowWrongPasswordNotification();
  else
  {
    SSModule_NPipe_ShowWrongPasswordNotification_Req_t  request;
    SSModule_NPipe_ShowWrongPasswordNotification_Resp_t response;
    request.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_SHOWWRONGPASSWORDNOTIFICATION_REQ;
    iResult = internal_CallThroughPipe(&request, sizeof(request), &response, sizeof(response));
    if (iResult==SSMODULE_NOERROR) iResult=response.iResult;
    return iResult;
  }
}


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

static void internal_LoadSettings(char *pchHomeDirectory)
{
  char achFileName[CCHMAXPATH];
  char *pchHomeEnvVar;
  char *pchKey, *pchValue;
  FILE *hFile;

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
    strncat(achFileName, ".dssaver\\SSCORE.CFG", sizeof(achFileName));
    // Try to open that config file!
    hFile = fopen(achFileName, "rt");
  }

  if (!hFile)
  {
    // If we could not open a config file in the HOME directory, or there was
    // no HOME directory, then try to use the DSSaver Global home directory!
    snprintf(achFileName, sizeof(achFileName), "%sSSCORE.CFG", pchHomeDirectory);
    hFile = fopen(achFileName, "rt");
    if (!hFile)
      return; // Could not open file!
  }

  // Go to the beginning of the file
  fseek(hFile, 0, SEEK_SET);
  // Process every entry of the config file
  pchKey = NULL;
  pchValue = NULL;

  while (internal_GetConfigFileEntry(hFile, &pchKey, &pchValue))
  {
    // Found a key/value pair!
    if (!stricmp(pchKey, "Enabled"))
      GlobalCurrentSettings.bEnabled = atoi(pchValue);
    else
    if (!stricmp(pchKey, "Activation_Time"))
    {
      GlobalCurrentSettings.iActivationTime = atoi(pchValue);
      if (GlobalCurrentSettings.iActivationTime<1000)
        GlobalCurrentSettings.iActivationTime = 1000;
    }
    else
    if (!stricmp(pchKey, "Module_FileName"))
      strncpy(GlobalCurrentSettings.achModuleFileName, pchValue, sizeof(GlobalCurrentSettings.achModuleFileName));
    else
    if (!stricmp(pchKey, "Password_Protected"))
      GlobalCurrentSettings.bPasswordProtected = atoi(pchValue);
    else
    if (!stricmp(pchKey, "Use_Current_Security_Password"))
      GlobalCurrentSettings.bUseCurrentSecurityPassword = atoi(pchValue);
    else
    if (!stricmp(pchKey, "Password"))
      strncpy(GlobalCurrentSettings.achEncryptedPassword, pchValue, sizeof(GlobalCurrentSettings.achEncryptedPassword));
    else
    if (!stricmp(pchKey, "Delayed_Password_Protection"))
      GlobalCurrentSettings.bUseDelayedPasswordProtection = atoi(pchValue);
    else
    if (!stricmp(pchKey, "Password_Delay_Time"))
    {
      GlobalCurrentSettings.iPasswordDelayTime = atoi(pchValue);
      if (GlobalCurrentSettings.iPasswordDelayTime<60000)
        GlobalCurrentSettings.iPasswordDelayTime = 60000; // 1 min
    }
    else
    if (!stricmp(pchKey, "Use_DPMS_Standby_State"))
      GlobalCurrentSettings.bUseDPMSStandbyState = atoi(pchValue);
    else
    if (!stricmp(pchKey, "Use_DPMS_Suspend_State"))
      GlobalCurrentSettings.bUseDPMSSuspendState = atoi(pchValue);
    else
    if (!stricmp(pchKey, "Use_DPMS_Off_State"))
      GlobalCurrentSettings.bUseDPMSOffState = atoi(pchValue);
    else
    if (!stricmp(pchKey, "DPMS_Standby_Time"))
    {
      GlobalCurrentSettings.iDPMSStandbyTime = atoi(pchValue);
      if (GlobalCurrentSettings.iDPMSStandbyTime<0)
        GlobalCurrentSettings.iDPMSStandbyTime = 0;
    }
    else
    if (!stricmp(pchKey, "DPMS_Suspend_Time"))
    {
      GlobalCurrentSettings.iDPMSSuspendTime = atoi(pchValue);
      if (GlobalCurrentSettings.iDPMSSuspendTime<0)
        GlobalCurrentSettings.iDPMSSuspendTime = 0;
    }
    else
    if (!stricmp(pchKey, "DPMS_Off_Time"))
    {
      GlobalCurrentSettings.iDPMSOffTime = atoi(pchValue);
      if (GlobalCurrentSettings.iDPMSOffTime<0)
        GlobalCurrentSettings.iDPMSOffTime = 0;
    }
    else
    if (!stricmp(pchKey, "Wake_By_Keyboard_Only"))
      GlobalCurrentSettings.bWakeByKeyboardOnly = atoi(pchValue);
    else
    if (!stricmp(pchKey, "First_Key_Event_Goes_To_Password_Window"))
      GlobalCurrentSettings.bFirstKeyEventGoesToPwdWindow = atoi(pchValue);
    else
    if (!stricmp(pchKey, "Disable_VIO_Popups"))
      GlobalCurrentSettings.bDisableVIOPopups = atoi(pchValue);
    else
    if (!stricmp(pchKey, "Use_Module_Wrapper"))
      GlobalCurrentSettings.bUseModuleWrapper = atoi(pchValue);

    free(pchKey); pchKey = NULL;
    if (pchValue)
    {
      free(pchValue); pchValue = NULL;
    }
  }

  fclose(hFile);
}

static void internal_SaveSettings(char *pchHomeDirectory)
{
  FILE *hFile;
  char *pchHomeEnvVar;
  char achFileName[CCHMAXPATH];

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
    strncat(achFileName, "\\SSCORE.CFG", sizeof(achFileName));
    // Try to open that config file!
    hFile = fopen(achFileName, "wt");
  }

  if (!hFile)
  {
    // If we could not open a config file in the HOME directory, or there was
    // no HOME directory, then try to use the DSSaver Global home directory!
    snprintf(achFileName, sizeof(achFileName), "%sSSCORE.CFG", pchHomeDirectory);
    hFile = fopen(achFileName, "wt");
    if (!hFile)
      return; // Could not open file!
  }

  // Save our values to the first line!
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fprintf(hFile, "; This is an automatically generated configuration file for\n");
  fprintf(hFile, "; the Screen Saver core.\n");
  fprintf(hFile, "; Do not modify it by hand!\n");
  fprintf(hFile, "\n");
  fprintf(hFile, "Enabled = %d\n",
          GlobalCurrentSettings.bEnabled);
  fprintf(hFile, "Activation_Time = %d\n",
          GlobalCurrentSettings.iActivationTime);
  fprintf(hFile, "Module_FileName = %s\n",
          GlobalCurrentSettings.achModuleFileName);
  fprintf(hFile, "Password_Protected = %d\n",
          GlobalCurrentSettings.bPasswordProtected);
  fprintf(hFile, "Use_Current_Security_Password = %d\n",
          GlobalCurrentSettings.bUseCurrentSecurityPassword);
  fprintf(hFile, "Password = %s\n",
          GlobalCurrentSettings.achEncryptedPassword);
  fprintf(hFile, "Delayed_Password_Protection = %d\n",
          GlobalCurrentSettings.bUseDelayedPasswordProtection);
  fprintf(hFile, "Password_Delay_Time = %d\n",
          GlobalCurrentSettings.iPasswordDelayTime);
  fprintf(hFile, "Use_DPMS_Standby_State = %d\n",
          GlobalCurrentSettings.bUseDPMSStandbyState);
  fprintf(hFile, "DPMS_Standby_Time = %d\n",
          GlobalCurrentSettings.iDPMSStandbyTime);
  fprintf(hFile, "Use_DPMS_Suspend_State = %d\n",
          GlobalCurrentSettings.bUseDPMSSuspendState);
  fprintf(hFile, "DPMS_Suspend_Time = %d\n",
          GlobalCurrentSettings.iDPMSSuspendTime);
  fprintf(hFile, "Use_DPMS_Off_State = %d\n",
          GlobalCurrentSettings.bUseDPMSOffState);
  fprintf(hFile, "DPMS_Off_Time = %d\n",
          GlobalCurrentSettings.iDPMSOffTime);
  fprintf(hFile, "Wake_By_Keyboard_Only = %d\n",
          GlobalCurrentSettings.bWakeByKeyboardOnly);
  fprintf(hFile, "First_Key_Event_Goes_To_Password_Window = %d\n",
          GlobalCurrentSettings.bFirstKeyEventGoesToPwdWindow);
  fprintf(hFile, "Disable_VIO_Popups = %d\n",
          GlobalCurrentSettings.bDisableVIOPopups);
  fprintf(hFile, "Use_Module_Wrapper = %d\n",
          GlobalCurrentSettings.bUseModuleWrapper);
  fprintf(hFile, "\n");
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fprintf(hFile, "; End of configuration file\n");
  fprintf(hFile, "; ----------------------------------------------------------\n");
  fclose(hFile);
}

static void internal_TellCommonModuleSettings(int bIsModuleWrapperUsed, pfn_SSModule_SetCommonSetting fnSetCommonSetting)
{
  // Tell the module the Common Module Settings.

  if (bIsModuleWrapperUsed)
  {
    // * First Key Event Goes To Password Window:
    SSModule_NPipe_SetCommonSetting0_Req_t request0;
    SSModule_NPipe_SetCommonSetting0_Resp_t response0;

    request0.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_SETCOMMONSETTING0_REQ;
    request0.uiFirstKeyGoesToPwdWindow = GlobalCurrentSettings.bFirstKeyEventGoesToPwdWindow;
    internal_CallThroughPipe(&request0, sizeof(request0), &response0, sizeof(response0));

    // ...

  } else
  if (fnSetCommonSetting)
  {

    // * First Key Event Goes To Password Window:
    fnSetCommonSetting(SSMODULE_COMMONSETTING_FIRSTKEYGOESTOPWDWINDOW,
                       (void *) (GlobalCurrentSettings.bFirstKeyEventGoesToPwdWindow));

    // ...
  }

  // No more Common Module Settings yet, but once we'll have more, they should come here.
}

static void internal_TryToLoadSSDPMS()
{
  ULONG rc;
  int bFunctionsOk;

  rc = DosLoadModule(NULL, 0, "SSDPMS.DLL", &hmodThisSSDPMS);

  _control87(0x37f, 0x1fff);
  if (rc!=NO_ERROR)
  {
    // Error loading the DLL.
#ifdef DEBUG_LOGGING
    AddLog("[internal_TryToLoadSSDPMS] : Error loading DLL, DPMS disabled!\n");
#endif

    // Disable DPMS support then!
    bThisDPMSSupportPresent = FALSE;
    hmodThisSSDPMS = NULLHANDLE;
    fn_SSDPMS_Initialize = NULL;
    fn_SSDPMS_Uninitialize = NULL;
    fn_SSDPMS_GetCapabilities = NULL;
    fn_SSDPMS_SetState = NULL;
    fn_SSDPMS_GetState = NULL;
  } else
  {
    // Okay, DLL loaded. Now query function pointers!
    bFunctionsOk = TRUE;

    rc = DosQueryProcAddr(hmodThisSSDPMS, 0,
                          "SSDPMS_Initialize",
                          (PFN *) &fn_SSDPMS_Initialize);
    if (rc!=NO_ERROR)
      bFunctionsOk = FALSE;

    rc = DosQueryProcAddr(hmodThisSSDPMS, 0,
                          "SSDPMS_Uninitialize",
                          (PFN *) &fn_SSDPMS_Uninitialize);
    if (rc!=NO_ERROR)
      bFunctionsOk = FALSE;

    rc = DosQueryProcAddr(hmodThisSSDPMS, 0,
                          "SSDPMS_GetCapabilities",
                          (PFN *) &fn_SSDPMS_GetCapabilities);
    if (rc!=NO_ERROR)
      bFunctionsOk = FALSE;

    rc = DosQueryProcAddr(hmodThisSSDPMS, 0,
                          "SSDPMS_SetState",
                          (PFN *) &fn_SSDPMS_SetState);
    if (rc!=NO_ERROR)
      bFunctionsOk = FALSE;

    rc = DosQueryProcAddr(hmodThisSSDPMS, 0,
                          "SSDPMS_GetState",
                          (PFN *) &fn_SSDPMS_GetState);
    if (rc!=NO_ERROR)
      bFunctionsOk = FALSE;

    if (!bFunctionsOk)
    {
#ifdef DEBUG_LOGGING
      AddLog("[internal_TryToLoadSSDPMS] : Error querying functions of DLL, DPMS disabled!\n");
#endif

      // Disable DPMS support then!
      DosFreeModule(hmodThisSSDPMS); hmodThisSSDPMS = NULLHANDLE;
      bThisDPMSSupportPresent = FALSE;
      fn_SSDPMS_Initialize = NULL;
      fn_SSDPMS_Uninitialize = NULL;
      fn_SSDPMS_GetCapabilities = NULL;
      fn_SSDPMS_SetState = NULL;
      fn_SSDPMS_GetState = NULL;
    } else
    {
      // Okay, DPMS support DLL loaded, it's time to initialize it!
#ifdef DEBUG_LOGGING
      AddLog("[internal_TryToLoadSSDPMS] : DPMS support DLL loaded, initializing!\n");
#endif
      rc = fn_SSDPMS_Initialize();
      if (rc!=SSDPMS_NOERROR)
      {
#ifdef DEBUG_LOGGING
        AddLog("[internal_TryToLoadSSDPMS] : Error initializing DPMS support, DPMS disabled!\n");
#endif

        // Disable DPMS support then!
        DosFreeModule(hmodThisSSDPMS); hmodThisSSDPMS = NULLHANDLE;
        bThisDPMSSupportPresent = FALSE;
        fn_SSDPMS_Initialize = NULL;
        fn_SSDPMS_Uninitialize = NULL;
        fn_SSDPMS_GetCapabilities = NULL;
        fn_SSDPMS_SetState = NULL;
        fn_SSDPMS_GetState = NULL;
      } else
      {
#ifdef DEBUG_LOGGING
        AddLog("[internal_TryToLoadSSDPMS] : DPMS support loaded\n");
        {
          int iCaps;
          char achTemp[256];
          AddLog("[internal_TryToLoadSSDPMS] : Calling SSDPMS_GetCapabilities\n");

          fn_SSDPMS_GetCapabilities(&iCaps);

          sprintf(achTemp, "  DPMS caps: %d\n", iCaps);
          AddLog(achTemp);
        }
#endif
        bThisDPMSSupportPresent = TRUE;
      }
    }
  }
}

static void internal_UnloadSSDPMS()
{
  if (hmodThisSSDPMS)
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_UnloadSSDPMS] : Uninitializing DPMS support\n");
#endif
    fn_SSDPMS_Uninitialize();

    // Now unload dll from memory!
    DosFreeModule(hmodThisSSDPMS); hmodThisSSDPMS = NULLHANDLE;
    bThisDPMSSupportPresent = FALSE;
    fn_SSDPMS_Initialize = NULL;
    fn_SSDPMS_Uninitialize = NULL;
    fn_SSDPMS_GetCapabilities = NULL;
    fn_SSDPMS_SetState = NULL;
    fn_SSDPMS_GetState = NULL;
#ifdef DEBUG_LOGGING
    AddLog("[internal_UnloadSSDPMS] : DPMS support unloaded\n");
#endif

  }
}

#if 0
// ---------------------------------------------------------------------------------
// The following code has been removed, and replaced with one 16bits API call,
// which is much more reliable, and works as written in the documents. :)
// ---------------------------------------------------------------------------------

// The prototype of the undocumented WinQueryExtIdFocus() API:

// -- For 16bits development:
//USHORT EXPENTRY WinQueryExtIdFocus(PUSHORT pSessionId);

// -- For 32bits development:
USHORT APIENTRY16 WIN16QUERYEXTIDFOCUS(PUSHORT pSessionId);
#define WinQueryExtIdFocus WIN16QUERYEXTIDFOCUS

#ifdef NO_WINQUERYEXTIDFOCUS_HACK
int internal_WinQueryExtIdFocus()
{
  int rc;
  USHORT usResult;

  rc = WinQueryExtIdFocus(&usResult);
  if (rc!=0)
    usResult = 1;

  return usResult;
}
#else

// Hack to use WinQueryExtIdFocus() with OpenWatcom compiler
// The OpenWatcom compiler v1.2 generates invalid code for
// the original function when optimization is turned on,
// so we work around that this way.

extern ULONG _FlatToFar16(ULONG);
#pragma aux _FlatToFar16 "_*" parm [EAX];
extern VOID _Far16Func2(ULONG, ULONG, ULONG);
#pragma aux _Far16Func2 "_*" parm [EAX] [ESI] [ECX];

int internal_WinQueryExtIdFocus()
{
  int rc;
  ULONG ulResult;
  ULONG ulPtr1616;

  rc = rc = 0;               // Make compiler/optimizer happy!
  ulResult = ulResult = 0;
  ulPtr1616 = ulPtr1616 = 0;

  _asm {
    lea eax, ulResult
    call _FlatToFar16
    mov ulPtr1616, eax

    mov ecx, 4
    lea esi, ulPtr1616
    mov eax, offset WIN16QUERYEXTIDFOCUS
    call _Far16Func2

    and eax, 0ffffH

    je ResultOk
    mov dword ptr ulResult, 1

  ResultOk:
  }

  return ulResult;
}
#endif

static int internal_IsPMTheForegroundFSSession()
{
  APIRET rc;
  ULONG ulResult1, ulResult2;

  // Returns TRUE if PM is the foreground (active) fullscreen session

  // For this, we query the current FS session ID, and check if that's
  // 1. The session ID of the PM is always 1.

  // Update: It seems that even though it is described in the documents, that
  // this function returns the fullscreen session ID, it returns some other session ID.
  // This means, that when the PM is in foreground, it still returns different values
  // as the user changes between e.g. VIO windows, and never returns 1.
  // So, the new method is to query session ID with this call, then query session ID
  // from PM, and compare them. If they are the same, then we're in PM.

#ifdef DEBUG_LOGGING
  AddLog("[internal_IsPMTheForegroundFSSession] : Calling WinQueryExtIdFocus()\n");
#endif

  ulResult1 = internal_WinQueryExtIdFocus();

#ifdef DEBUG_LOGGING
  AddLog("[internal_IsPMTheForegroundFSSession] : Calling DosQuerySysInfo()\n");
#endif


  rc = DosQuerySysInfo(QSV_FOREGROUND_FS_SESSION,
                       QSV_FOREGROUND_FS_SESSION,
                       &ulResult2,
                       sizeof(ulResult2));
  if (rc!=NO_ERROR)
    ulResult2 = 1;

#ifdef DEBUG_LOGGING
  if ((ulResult2==1) ||
      (ulResult1 == ulResult2))
    AddLog("[internal_IsPMTheForegroundFSSession] : PM is in foreground\n");
  else
    AddLog("[internal_IsPMTheForegroundFSSession] : PM is in background\n");
#endif

  return ((ulResult2 == 1) ||
          (ulResult1 == ulResult2));
}
// ---------------------------------------------------------------------------------
// End of removed code
// ---------------------------------------------------------------------------------
#endif

// -- Definition of old Semaphore API to be called frfrom 32bits code: --
typedef ULONG HSEM16;
typedef HSEM16 FAR *PHSEM16;
typedef CHAR FAR *PSZ16;
USHORT APIENTRY16 DOS16OPENSEM(PHSEM16, PSZ16);
#define DosOpenSem DOS16OPENSEM
USHORT APIENTRY16 DOS16SEMCLEAR(HSEM16);
#define DosSemClear DOS16SEMCLEAR
USHORT APIENTRY16 DOS16SEMREQUEST(HSEM16, LONG);
#define DosSemRequest DOS16SEMREQUEST
USHORT APIENTRY16 DOS16CLOSESEM(HSEM16);
#define DosCloseSem DOS16CLOSESEM

// -- Definition of DosGetInfoSeg() API to be called from 32bits code: --
USHORT APIENTRY16 DOS16GETINFOSEG(PUSHORT pselGlobal, PUSHORT pselLocal);
#define DosGetInfoSeg DOS16GETINFOSEG

/* Global Information Segment */
typedef struct _GINFOSEG {	/* gis */
    ULONG   time;
    ULONG   msecs;
    UCHAR   hour;
    UCHAR   minutes;
    UCHAR   seconds;
    UCHAR   hundredths;
    USHORT  timezone;
    USHORT  cusecTimerInterval;
    UCHAR   day;
    UCHAR   month;
    USHORT  year;
    UCHAR   weekday;
    UCHAR   uchMajorVersion;
    UCHAR   uchMinorVersion;
    UCHAR   chRevisionLetter;
    UCHAR   sgCurrent;
    UCHAR   sgMax;
    UCHAR   cHugeShift;
    UCHAR   fProtectModeOnly;
    USHORT  pidForeground;
    UCHAR   fDynamicSched;
    UCHAR   csecMaxWait;
    USHORT  cmsecMinSlice;
    USHORT  cmsecMaxSlice;
    USHORT  bootdrive;
    UCHAR   amecRAS[32];
    UCHAR   csgWindowableVioMax;
    UCHAR   csgPMMax;
    USHORT  SIS_Syslog;		 /* Error logging status (NOT DOCUMENTED) */
    USHORT  SIS_MMIOBase;	 /* Memory mapped I/O selector */
    USHORT  SIS_MMIOAddr;	 /* Memory mapped I/O selector */
    UCHAR   SIS_MaxVDMs;	 /* Max. no. of Virtual DOS machines */
    UCHAR   SIS_Reserved;
} GINFOSEG;
typedef GINFOSEG FAR *PGINFOSEG;

static int internal_IsPMTheForegroundFSSession()
{
  APIRET         rc;
  USHORT         globalSeg, localSeg;
  GINFOSEG       *pInfo;

  // Returns TRUE if PM is the foreground (active) fullscreen session

#ifdef DEBUG_LOGGING
  AddLog("[internal_IsPMTheForegroundFSSession] : Enter\n");
#endif

  // For this, we query the current FS session ID, and check if that's 1.
  // The session ID of the PM is always 1.

  globalSeg = localSeg = 0;
  rc = DosGetInfoSeg(&globalSeg, &localSeg);

  if (rc!=NO_ERROR)
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_IsPMTheForegroundFSSession] : Error calling DosGetInfoSeg()\n");
#endif
    return 1; // Default: yes
   }

  // Make FLAT pointer from selector
  pInfo = (PGINFOSEG) (((ULONG)(globalSeg & 0xFFF8))<<13);
  if (!pInfo)
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_IsPMTheForegroundFSSession] : Error making flat pointer from selector\n");
#endif
    return 1; // Default: yes
  }

#ifdef DEBUG_LOGGING
  {
    char achTemp[32];
    sprintf(achTemp, "%d", pInfo->sgCurrent);
    AddLog("[internal_IsPMTheForegroundFSSession] : Current fullscreen session: ");
    AddLog(achTemp);
    AddLog("\n");
  }
#endif

  // PM is in foreground, if the current fullscreen session ID is 1
  return (pInfo->sgCurrent==1);
}


static unsigned long internal_GetCurrentPID(void)
{
  PPIB pib;
  DosGetInfoBlocks(NULL, &pib);
  return((unsigned long) (pib->pib_ulpid));
}

BOOL APIENTRY fnInputHook(HAB hab, PQMSG pQmsg, ULONG ulfs)
{
  APIRET rc;
  unsigned long ulTemp;
  HEV hevLocalSentinelEvent;

  switch (pQmsg->msg)
  {
// ----------- Mouse activity ----------------
    case WM_MOUSEMOVE:
      if (pQmsg->hwnd)
      {
        POINTL ptlTemp, ptlDelta;

        // Get the current mouse position
        ptlTemp.x = (short) SHORT1FROMMP(pQmsg->mp1);
        ptlTemp.y = (short) SHORT2FROMMP(pQmsg->mp1);

        // Map it to be relative to Desktop
        WinMapWindowPoints(pQmsg->hwnd, HWND_DESKTOP, &ptlTemp, 1);

        // Calculate mouse movement distance from previous mouse position
        ptlDelta.x = sGlobalCurrDesktopMouseXPos - ptlTemp.x;
        ptlDelta.y = sGlobalCurrDesktopMouseYPos - ptlTemp.y;

        // Save the new mouse position
        sGlobalCurrDesktopMouseXPos = ptlTemp.x;
        sGlobalCurrDesktopMouseYPos = ptlTemp.y;

        // Check if this mouse movement is significant or not!
        if (((ptlDelta.x * ptlDelta.x) + (ptlDelta.y * ptlDelta.y)) <=
            (SSCORE_MOUSE_MOVEMENT_SENSITIVITY * SSCORE_MOUSE_MOVEMENT_SENSITIVITY))
        {
          // Either the mouse have not moved at all (so it's just a WM_MOUSEMOVE message
          // from a window or from WinSetFocus() API), or the mouse have moved, but the
          // movement wasn't great enough to report user activity.
          // So, don't report activity!
          break;
        }
      }
      // Let the control flow to the other stuffs, so
      // there is INTENTIONALLY no break here!
    case WM_VSCROLL:
      // Scrolling with vertical scrollbar.
      // This is here to support AMouse, which can send this message directly when
      // the user uses the scroll-wheel on the mouse.
    case WM_HSCROLL:
      // Detto, see WM_VSCROLL!
    case WM_BUTTON1DOWN:
    case WM_BUTTON2DOWN:
    case WM_BUTTON3DOWN:
//    case WM_BUTTON1UP:
//    case WM_BUTTON2UP:
//    case WM_BUTTON3UP:
    case WM_BUTTON1DBLCLK:
    case WM_BUTTON2DBLCLK:
    case WM_BUTTON3DBLCLK:

      // Every mouse event will flow here.

      // Save last mouse event timestap
      rc = DosQuerySysInfo(QSV_MS_COUNT,
                           QSV_MS_COUNT,
                           &ulTemp,
                           sizeof(ulTemp));
      if (rc==NO_ERROR)
        ulGlobalLastMouseEventTimestamp = ulTemp;


      // If we're saving the screen, and it's told to wake the system up only by
      // a keyboard event, then don't let the control flow to the wakeup part,
      // but get out of here now!
      if ((iGlobalCurrentState == SSCORE_STATE_SAVING) &&
	  (GlobalCurrentSettings.bWakeByKeyboardOnly))
	break;

// ----------- Keyboard activity ----------------
    case WM_CHAR:

      if (pQmsg->msg==WM_CHAR)
      {
        /*
        char achTemp[256];
        sprintf(achTemp, "WM_CHAR fsFlags: 0x%x\n", SHORT1FROMMP(pQmsg->mp1));
        AddLog(achTemp);
        sprintf(achTemp, "WM_CHAR ucScanCode: 0x%x\n", CHAR4FROMMP(pQmsg->mp1));
        AddLog(achTemp);
        sprintf(achTemp, "WM_CHAR usch: 0x%x\n", SHORT1FROMMP(pQmsg->mp2));
        AddLog(achTemp);
        sprintf(achTemp, "WM_CHAR usvk: 0x%x\n", SHORT2FROMMP(pQmsg->mp2));
        AddLog(achTemp);
        */

        // Save last keyboard event timestap
        rc = DosQuerySysInfo(QSV_MS_COUNT,
                             QSV_MS_COUNT,
                             &ulTemp,
                             sizeof(ulTemp));
        if (rc==NO_ERROR)
          ulGlobalLastKeyboardEventTimestamp = ulTemp;
      }

// ----------- Things to do in case of activity ----------------
      if (hab == habGlobalInitializerClient)
      {
        // Calling from the application initialized it!
        // Simply post the event semaphore to note that something happened.
        DosPostEventSem(hevGlobalSentinelEvent);
      } else
      {
        // Calling from a different application
        hevLocalSentinelEvent = NULL;
        // Get the handle to the shared event semaphore,then post it
        // to note that something happened.
        if (DosOpenEventSem(ACTION_EVENT_SHARED_SEM_NAME,
                            &hevLocalSentinelEvent)==NO_ERROR)
        {
          DosPostEventSem(hevLocalSentinelEvent);
          DosCloseEventSem(hevLocalSentinelEvent);
        }

      }
      break;

    default:
#ifdef DEBUG_LOGGING
      /*
      {
        char achTemp[20];
        AddLog("[fnInputHook] : Unprocessed message: ");
        sprintf(achTemp, "0x%x\n", pQmsg->msg);
        AddLog(achTemp);
        }
        */
#endif
      break;
  }
  return (FALSE);    /* let any other hook process it */
}

static int internal_Worker_SetDPMSState(int iDPMSState)
{
  int rc;
  if ((bThisDPMSSupportPresent) &&
      (hmodThisSSDPMS) &&
      (fn_SSDPMS_SetState))
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_Worker_SetDPMSState] : Setting DPMS state.\n");
#endif
    rc = fn_SSDPMS_SetState(iDPMSState);
  } else
    rc = SSDPMS_ERROR_INVALIDPARAMETER;

  return (rc == SSDPMS_NOERROR);
}

static void internal_Worker_PostMsgToParent(HWND hwndParent,
                                            ULONG ulMsg,
                                            MPARAM mp1,
                                            MPARAM mp2)
{
#ifdef DEBUG_LOGGING
  if (ulMsg==WM_SSCORENOTIFY_PREVIEWSTOPPED)
    AddLog("Posting PREVIEWSTOPPED message to parent\n");
  if (ulMsg==WM_SSCORENOTIFY_PREVIEWSTARTED)
    AddLog("Posting PREVIEWSTOPPED message to parent\n");

  {
    char achTemp[64];
    sprintf(achTemp, " hwnd = 0x%08x\n", (int) hwndParent);
    AddLog(achTemp);
  }
#endif

  if ((hwndParent) && (hwndParent!=HWND_DESKTOP))
  {
    HWND hwndPreviewParent;

    // Get the parent window of the preview window which was subclassed,
    // and send the info message to it!
    hwndPreviewParent = WinQueryWindow(hwndParent, QW_PARENT);
    WinPostMsg(hwndPreviewParent,
               ulMsg, mp1, mp2);
  }
}

static void internal_Worker_CreateStateChangeEvent()
{
  HEV hevLocalStateChangeEvent;

  if (pidThisClient == pidGlobalInitializerClient)
  {
    // Calling from the application initialized it!
    // Simply post the event semaphore to note that something happened.
    DosPostEventSem(hevGlobalStateChangeEvent);
  } else
  {
    // Calling from a different application
    hevLocalStateChangeEvent = NULL;
    // Get the handle to the shared event semaphore, then post it
    // to note that something happened.
    if (DosOpenEventSem(STATECHANGE_EVENT_SHARED_SEM_NAME,
                        &hevLocalStateChangeEvent)==NO_ERROR)
    {
      DosPostEventSem(hevLocalStateChangeEvent);
      DosCloseEventSem(hevLocalStateChangeEvent);
    }
  }
}

static void internal_Worker_RemoveSentinelActivityFromQueue(HAB hab)
{
  QMSG qmsgTemp;
  int rc;
  ULONG ulPostCount;

  // Make sure there is no pending stuffs in Sentinel thread
  DosResetEventSem(hevGlobalSentinelEvent, &ulPostCount);

  // Remove the messages already put into queue by Sentinel thread
  do {
    rc = WinPeekMsg(hab, &qmsgTemp, NULLHANDLE,
                    WORKER_COMMAND_SENTINELACTIVITY,
                    WORKER_COMMAND_SENTINELACTIVITY,
                    PM_REMOVE);
  } while (rc);
}



static void internal_Worker_StopSaver(HWND hwndParent,
                                      HMODULE *phmodDLL,
                                      int *pbIsModuleWrapperUsed,
                                      pfn_SSModule_StartSaving *ppfnStartSaving,
                                      pfn_SSModule_StopSaving  *ppfnStopSaving,
                                      pfn_SSModule_AskUserForPassword             *ppfnAskUserForPassword,
                                      pfn_SSModule_ShowWrongPasswordNotification  *ppfnShowWrongPassword,
                                      pfn_SSModule_PauseSaving                    *ppfnPauseSaving,
                                      pfn_SSModule_ResumeSaving                   *ppfnResumeSaving,
                                      int *pbSaverModulePaused,
                                      int *piInPreviewMode,
                                      ULONG *pulDelayedPwdProtTimerID,
                                      ULONG *pulDPMSTimerID,
                                      HSEM16  *phsemVIOPopup,
                                      int   *pbDisabledVIOPopup)
{
  pfn_SSModule_SetNLSText pfnSetNLSText;
  ULONG ulrc;

  if (*phmodDLL)
  {

    // ----- Undo pausing of module -----

    if ((*pbSaverModulePaused) && (*ppfnResumeSaving))
    {
#ifdef DEBUG_LOGGING
      AddLog("[StopSaver] : Calling SSModule_ResumeSaving()\n");
#endif
      internal_Worker_CallResumeSaving(*pbIsModuleWrapperUsed, *ppfnResumeSaving);
    }

    // ----- Stop saving -----

#ifdef DEBUG_LOGGING
    AddLog("[StopSaver] : Calling SSModule_StopSaving()\n");
#endif
    internal_Worker_CallStopSaving(*pbIsModuleWrapperUsed, *ppfnStopSaving);

    // ----- Free module resources -----

    // Now, if the module supports SSModule_SetNLSText() call,
    // tell it to free all the texts!

    if (*pbIsModuleWrapperUsed)
    {
      int i;

#ifdef DEBUG_LOGGING
      AddLog("[StopSaver] : Clearing NLS texts from module through pipe...\n");
#endif
      for (i=0; i<SSCORE_NLSTEXT_MAX+1; i++)
        internal_Worker_CallSetNLSText(*pbIsModuleWrapperUsed, NULL, i, NULL);

#ifdef DEBUG_LOGGING
      AddLog("[StopSaver] : Stopping wrapper process\n");
#endif
      DosKillProcess(DKP_PROCESS, (PID) (*phmodDLL));
    } else
    {

      ulrc = DosQueryProcAddr(*phmodDLL,
                              0,
                              "SSModule_SetNLSText",
                              (PFN *) &pfnSetNLSText);
      if (ulrc==NO_ERROR)
      {
        int i;

        // So, this module supports SetNLSText.
        // We've possibly sent it the NLS texts, which has been
        // stored by the module then.
        // Now, before unloading the module,
        // send them NULLs, so it will free the texts.
#ifdef DEBUG_LOGGING
        AddLog("[StopSaver] : Clearing NLS texts from module...\n");
#endif
        for (i=0; i<SSCORE_NLSTEXT_MAX+1; i++)
          internal_Worker_CallSetNLSText(*pbIsModuleWrapperUsed, pfnSetNLSText, i, NULL);
      }

#ifdef DEBUG_LOGGING
      AddLog("[StopSaver] : DosFreeModule()\n");
#endif
      DosFreeModule(*phmodDLL);
    }

    if (!(*piInPreviewMode))
    {
      // Stopping a real saving, so:

      // Note the state change!
      iGlobalCurrentState = SSCORE_STATE_NORMAL;
      internal_Worker_CreateStateChangeEvent();
    }

    // Restore variables
    *phmodDLL = NULL;
    *pbIsModuleWrapperUsed = FALSE;
    *piInPreviewMode = FALSE;
    *pbSaverModulePaused = FALSE;

  }
  // Stop running timers
  if (*pulDelayedPwdProtTimerID)
  {
    WinStopTimer(0, 0, *pulDelayedPwdProtTimerID);
    *pulDelayedPwdProtTimerID = 0;
  }

  if (*pulDPMSTimerID)
  {
    WinStopTimer(0, 0, *pulDPMSTimerID);
    *pulDPMSTimerID = 0;

  }

  // Make sure the monitor will be turned on!
  internal_Worker_SetDPMSState(SSDPMS_STATE_ON);

  // Re-enable VIO Popups, if we disabled that before
  if (*pbDisabledVIOPopup)
  {
    DosSemClear(*phsemVIOPopup);
    DosCloseSem(*phsemVIOPopup);
    *pbDisabledVIOPopup = 0;
  }
}

static void internal_Worker_StartSaver(char *pchDLLFileName,
                                       HWND hwndParent,
                                       int iPreviewFlag,
                                       HMODULE *phmodDLL,
                                       int *pbIsModuleWrapperUsed,
                                       pfn_SSModule_StartSaving *ppfnStartSaving,
                                       pfn_SSModule_StopSaving  *ppfnStopSaving,
                                       pfn_SSModule_AskUserForPassword             *ppfnAskUserForPassword,
                                       pfn_SSModule_ShowWrongPasswordNotification  *ppfnShowWrongPassword,
                                       pfn_SSModule_PauseSaving                    *ppfnPauseSaving,
                                       pfn_SSModule_ResumeSaving                   *ppfnResumeSaving,
                                       int *pbSaverModulePaused,
                                       int *piInPreviewMode,
                                       int *pbShouldAskPwd,
                                       ULONG *pulDelayedPwdProtTimerID,
                                       ULONG *pulDPMSTimerID,
                                       int   *piDPMSState,
                                       HAB    hab,
                                       HSEM16  *phsemVIOPopup,
                                       int   *pbDisabledVIOPopup)
{
  ULONG ulrc;
  char achFailure[32];
  pfn_SSModule_SetNLSText pfnSetNLSText;
  pfn_SSModule_SetCommonSetting pfnSetCommonSetting;

  // Make sure no old thing is running now!
  internal_Worker_StopSaver(hwndParent, phmodDLL,
                            pbIsModuleWrapperUsed,
                            ppfnStartSaving, ppfnStopSaving,
                            ppfnAskUserForPassword, ppfnShowWrongPassword,
                            ppfnPauseSaving, ppfnResumeSaving,
                            pbSaverModulePaused,
                            piInPreviewMode,
                            pulDelayedPwdProtTimerID,
                            pulDPMSTimerID,
                            phsemVIOPopup,
                            pbDisabledVIOPopup);


  if ((GlobalCurrentSettings.bUseModuleWrapper) && (!iPreviewFlag))
  {
    // Try to run module wrapper, if it's not a previewing
    char achParams[512];
    RESULTCODES rcExec;

#ifdef DEBUG_LOGGING
    AddLog("[StartSaver] : Using module wrapper for ["); AddLog(pchDLLFileName); AddLog("]\n");
#endif
    memset(achParams, 0, sizeof(achParams));
    snprintf(achParams, sizeof(achParams), "%s", SSMODULE_NPIPE_SERVER_PATH, pchDLLFileName);
    snprintf(achParams+strlen(achParams)+1, sizeof(achParams)-strlen(achParams)-1, "%s", pchDLLFileName);

    ulrc = DosExecPgm(achFailure, sizeof(achFailure),
                      EXEC_ASYNC,
                      achParams,
                      0, // No special env. variables to pass
                      &rcExec,
                      "SSModWrp.exe");

    if (ulrc!=NO_ERROR)
    {
      // Error, could not start process!
#ifdef DEBUG_LOGGING
      AddLog("Worker : Error starting module wrapper!\n");
      achFailure[31] = 0;
      AddLog("  Failure: ["); AddLog(achFailure); AddLog("]\n");
#endif
      *phmodDLL = NULLHANDLE;
    } else
    {
      int iTemp;
      unsigned int uiCounter;

      *phmodDLL = (HMODULE) (rcExec.codeTerminate);
#ifdef DEBUG_LOGGING
      AddLog("[StartSaver] : Wrapper process started, waiting for pipe to become available...\n");
#endif
      uiCounter=0;
      do {
        SSModule_NPipe_Ping_Req_t request;
        SSModule_NPipe_Ping_Resp_t response;
        request.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_PING_REQ;
        iTemp = internal_CallThroughPipe(&request, sizeof(request), &response, sizeof(response));
        if (iTemp!=SSMODULE_NOERROR)
        {
          DosSleep(100);
          uiCounter++;
        }
      } while ((iTemp!=SSMODULE_NOERROR) && (uiCounter<50));

      if (iTemp!=SSMODULE_NOERROR)
      {
#ifdef DEBUG_LOGGING
        AddLog("[StartSaver] : Pipe is not available, stopping wrapper process.\n");
#endif
        DosKillProcess(DKP_PROCESS, (PID) (*phmodDLL));
        *phmodDLL = NULL;
      } else
      {
#ifdef DEBUG_LOGGING
        AddLog("[StartSaver] : Pipe is now available\n");
#endif

        *pbIsModuleWrapperUsed = TRUE;
        *ppfnStartSaving = (void *)1;
        *ppfnStopSaving = (void *)1;
        *ppfnAskUserForPassword = (void *)1;
        *ppfnShowWrongPassword = (void *)1;
        *ppfnPauseSaving = (void *)1;
        *ppfnResumeSaving = (void *)1;
        pfnSetNLSText = (void *)1;
        pfnSetCommonSetting = (void *)1;
      }
    }
  }

  // If we did not try to run wrapper, or it has failed, load the DLL directly.
  if (!*phmodDLL)
  {
    *pbIsModuleWrapperUsed = FALSE;
    // Now load and start the new one!
#ifdef DEBUG_LOGGING
    AddLog("[StartSaver] : DosLoadModule() of ["); AddLog(pchDLLFileName); AddLog("]\n");
#endif

    ulrc = DosLoadModule(achFailure, sizeof(achFailure),
                         pchDLLFileName,
                         phmodDLL);
    _control87(0x37f, 0x1fff);
    if (ulrc!=NO_ERROR)
    {
      // Error, could not start screensaver!
#ifdef DEBUG_LOGGING
      AddLog("Worker : Error loading module!\n");
      achFailure[31] = 0;
      AddLog("  Failure: ["); AddLog(achFailure); AddLog("]\n");
#endif
      *phmodDLL = NULLHANDLE;
    } else
    {
      // Cool, DLL could be loaded!
#ifdef DEBUG_LOGGING
      AddLog("[StartSaver] : DosQueryProcAddr()\n");
#endif

      // Query function pointers!

      // First the mandatory functions
      ulrc = DosQueryProcAddr(*phmodDLL,
                              0,
                              "SSModule_StartSaving",
                              (PFN *) ppfnStartSaving);
      if (ulrc!=NO_ERROR) *ppfnStartSaving = NULL;

      ulrc = DosQueryProcAddr(*phmodDLL,
                              0,
                              "SSModule_StopSaving",
                              (PFN *) ppfnStopSaving);
      if (ulrc!=NO_ERROR) *ppfnStopSaving = NULL;

      ulrc = DosQueryProcAddr(*phmodDLL,
                              0,
                              "SSModule_AskUserForPassword",
                              (PFN *) ppfnAskUserForPassword);
      if (ulrc!=NO_ERROR) *ppfnAskUserForPassword = NULL;

      ulrc = DosQueryProcAddr(*phmodDLL,
                              0,
                              "SSModule_ShowWrongPasswordNotification",
                              (PFN *) ppfnShowWrongPassword);
      if (ulrc!=NO_ERROR) *ppfnShowWrongPassword = NULL;

      // Now the optional functions
      ulrc = DosQueryProcAddr(*phmodDLL,
                              0,
                              "SSModule_PauseSaving",
                              (PFN *) ppfnPauseSaving);
      if (ulrc!=NO_ERROR) *ppfnPauseSaving = NULL;

      ulrc = DosQueryProcAddr(*phmodDLL,
                              0,
                              "SSModule_ResumeSaving",
                              (PFN *) ppfnResumeSaving);
      if (ulrc!=NO_ERROR) *ppfnResumeSaving = NULL;

      ulrc = DosQueryProcAddr(*phmodDLL,
                              0,
                              "SSModule_SetNLSText",
                              (PFN *) &pfnSetNLSText);
      if (ulrc!=NO_ERROR) pfnSetNLSText = NULL;

      ulrc = DosQueryProcAddr(*phmodDLL,
                              0,
                              "SSModule_SetCommonSetting",
                              (PFN *) &pfnSetCommonSetting);
      if (ulrc!=NO_ERROR) pfnSetCommonSetting = NULL;

      // Check if we have all the mandatory functions!
      if ((!*ppfnStartSaving) || (!*ppfnStopSaving) ||
          (!*ppfnAskUserForPassword) || (!*ppfnShowWrongPassword))
      {
#ifdef DEBUG_LOGGING
        AddLog("Worker : Error querying functions\n");
#endif
        // Could not query all functions! Error!
#ifdef DEBUG_LOGGING
        AddLog("[StartSaver] : DosFreeModule()\n");
#endif
        DosFreeModule(*phmodDLL);
        *phmodDLL = NULL;
      } else
      {
#ifdef DEBUG_LOGGING
        AddLog("[StartSaver] : Function pointers ok.\n");
#endif
      }
    }
  }

  if (*phmodDLL)
  {
    // Function pointers OK, start saving!
    int irc;

    if (pfnSetNLSText)
    {
      int i;

#ifdef DEBUG_LOGGING
      AddLog("[StartSaver] : Setting NLS texts...\n");
#endif
      if (DosRequestMutexSem(hmtxThisUseNLSTextArray, SEM_INDEFINITE_WAIT)==NO_ERROR)
      {
        for (i=0; i<SSCORE_NLSTEXT_MAX+1; i++)
          internal_Worker_CallSetNLSText(*pbIsModuleWrapperUsed, pfnSetNLSText, i, apchThisNLSText[i]);

        DosReleaseMutexSem(hmtxThisUseNLSTextArray);
      }
    }

    // If the module supports Common Module Settings, then we tell it the
    // common module settings!
    if (pfnSetCommonSetting)
    {
#ifdef DEBUG_LOGGING
      AddLog("[StartSaver] : Setting Common Module Settings...\n");
#endif
      internal_TellCommonModuleSettings(*pbIsModuleWrapperUsed, pfnSetCommonSetting);
    }

#ifdef DEBUG_LOGGING
      AddLog("[StartSaver] : Start saving!\n");
#endif
    irc = internal_Worker_CallStartSaving(*pbIsModuleWrapperUsed, *ppfnStartSaving,
                                          hwndParent, achGlobalHomeDirectory, iPreviewFlag);
    if (irc!=SSMODULE_NOERROR)
    {
#ifdef DEBUG_LOGGING
      AddLog("Worker : The module reported error in StartSaving!\n");
#endif
      // The module could not start saving! Error!
      if (*pbIsModuleWrapperUsed)
      {
#ifdef DEBUG_LOGGING
        AddLog("[StartSaver] : Kill process\n");
#endif
        DosKillProcess(DKP_PROCESS, (PID) (*phmodDLL));
        *phmodDLL = NULL;
      } else
      {
#ifdef DEBUG_LOGGING
        AddLog("[StartSaver] : DosFreeModule()\n");
#endif
        DosFreeModule(*phmodDLL);
        *phmodDLL = NULL;
      }
    } else
    {
#ifdef DEBUG_LOGGING
      AddLog("Worker : Saving module started!\n");
#endif
      *pbSaverModulePaused = FALSE;

      *piInPreviewMode = iPreviewFlag;

      if (!iPreviewFlag)
      {
        // Starting real saving, so start timers!

        // Start delayed password protection timer, if needed!
        if (GlobalCurrentSettings.bUseDelayedPasswordProtection)
        {
#ifdef DEBUG_LOGGING
          AddLog("Worker : Starting DelayedPwdProt timer!\n");
#endif

          if (*pulDelayedPwdProtTimerID)
            WinStopTimer(0, 0, *pulDelayedPwdProtTimerID);

          *pbShouldAskPwd = FALSE;
          *pulDelayedPwdProtTimerID = WinStartTimer(0, 0, 0,
                                                    GlobalCurrentSettings.iPasswordDelayTime);
        } else
        {
          // If its not delayed password protected, then
          // bShouldAskPwd depends only on the password protection!
          *pbShouldAskPwd = GlobalCurrentSettings.bPasswordProtected;
        }

        // Start DPMS timer, if needed!
        if (*pulDPMSTimerID)
        {
          // Stop old, if running!
#ifdef DEBUG_LOGGING
          AddLog("Worker : Stopping old DPMS timer\n");
#endif

          WinStopTimer(0, 0, *pulDPMSTimerID);
          *pulDPMSTimerID = 0;
        }
        if ((bThisDPMSSupportPresent) &&
            ((GlobalCurrentSettings.bUseDPMSStandbyState) ||
             (GlobalCurrentSettings.bUseDPMSSuspendState) ||
             (GlobalCurrentSettings.bUseDPMSOffState))
           )
        {
          int iDPMSCaps;

          // Query DPMS capabilities!
          if ((!bThisDPMSSupportPresent) ||
              (!fn_SSDPMS_GetCapabilities) ||
              (fn_SSDPMS_GetCapabilities(&iDPMSCaps)!=SSDPMS_NOERROR))
            iDPMSCaps = 0; // Support for nothing

          if (
              (GlobalCurrentSettings.bUseDPMSStandbyState) &&
              (iDPMSCaps & SSDPMS_STATE_STANDBY)
             )
          {
            *piDPMSState = SSCORE_DPMSSTATE_STANDBY-1; // So the next will be Standby
#ifdef DEBUG_LOGGING
            AddLog("Worker : Starting DPMS timer for Standby mode\n");
#endif

            *pulDPMSTimerID = WinStartTimer(0, 0, 0,
                                            GlobalCurrentSettings.iDPMSStandbyTime);
          } else
          if (
              (GlobalCurrentSettings.bUseDPMSSuspendState) &&
              (iDPMSCaps & SSDPMS_STATE_SUSPEND)
             )
          {
            *piDPMSState = SSCORE_DPMSSTATE_SUSPEND-1; // So the next will be Suspend
#ifdef DEBUG_LOGGING
            AddLog("Worker : Starting DPMS timer for Suspend mode\n");
#endif
            *pulDPMSTimerID = WinStartTimer(0, 0, 0,
                                            GlobalCurrentSettings.iDPMSSuspendTime);
          } else
          if (
              (GlobalCurrentSettings.bUseDPMSOffState) &&
              (iDPMSCaps & SSDPMS_STATE_OFF)
             )
          {
            *piDPMSState = SSCORE_DPMSSTATE_OFF-1; // So the next will be Off
#ifdef DEBUG_LOGGING
            AddLog("Worker : Starting DPMS timer for Off mode\n");
#endif
            *pulDPMSTimerID = WinStartTimer(0, 0, 0,
                                            GlobalCurrentSettings.iDPMSOffTime);
#ifdef DEBUG_LOGGING
            {
              char achTemp[256];
              AddLog(" Off timer timeout will be: ");
              sprintf(achTemp, "%d msec, ID is %d\n",
                      GlobalCurrentSettings.iDPMSOffTime,
                      *pulDPMSTimerID);
              AddLog(achTemp);
            }
#endif

          } else
          {
#ifdef DEBUG_LOGGING
            AddLog("Worker : Internal error, not starting DPMS timer!\n");
#endif
            // Ooops, something wrong if execution comes here :)
          }
        }

        // Disable VIO popups if it's have to be disabled
        if ((GlobalCurrentSettings.bDisableVIOPopups) && (!(*pbDisabledVIOPopup)))
        {
          USHORT rc;
#ifdef DEBUG_LOGGING
          char achBuf[1024];
          AddLog("[StartSaver] : Trying to open VIOPOPUP semaphore.\n");
          sprintf(achBuf, "phsem: %p\n", phsemVIOPopup); AddLog(achBuf);
          sprintf(achBuf, "*phsem: %p\n", *phsemVIOPopup); AddLog(achBuf);
#endif
          rc = DosOpenSem(phsemVIOPopup, "\\SEM\\VIOPOPUP");
          if (rc == NO_ERROR)
          {
#ifdef DEBUG_LOGGING
            AddLog("[StartSaver] : Trying to grab VIOPOPUP semaphore.\n");
            sprintf(achBuf, "phsem: %p\n", phsemVIOPopup); AddLog(achBuf);
            sprintf(achBuf, "*phsem: %p\n", *phsemVIOPopup); AddLog(achBuf);
#endif
            rc = DosSemRequest(*phsemVIOPopup, 0);
            if (rc == NO_ERROR)
            {
#ifdef DEBUG_LOGGING
              AddLog("[StartSaver] : VIOPOPUP semaphore grabbed.\n");
#endif
              *pbDisabledVIOPopup = 1;
            }
#ifdef DEBUG_LOGGING
            else
              AddLog("[StartSaver] : Could not grab VIOPOPUP semaphore!\n");
#endif
          }
#ifdef DEBUG_LOGGING
          else
            AddLog("[StartSaver] : Could not open VIOPOPUP semaphore!\n");
#endif
        }

#ifdef DEBUG_LOGGING
        AddLog("Worker : Removing sentinel activity messages from queue!\n");
#endif
        // Now that the saver has been started, we should remove any pending
        // SENTINELACTIVITY messages from the queue, because they would
        // stop the saver at once!
        internal_Worker_RemoveSentinelActivityFromQueue(hab);

        // Note the state change!
        iGlobalCurrentState = SSCORE_STATE_SAVING;
        internal_Worker_CreateStateChangeEvent();
      }
    }
  }
}

static int internal_Worker_PasswordOk(int bIsModuleWrapperUsed,
                                      pfn_SSModule_AskUserForPassword             fnAskUserForPassword,
				      pfn_SSModule_ShowWrongPasswordNotification  fnShowWrongPassword,
				      pfn_SSModule_PauseSaving                    fnPauseSaving,
                                      pfn_SSModule_ResumeSaving                   fnResumeSaving,
				      int *pbSaverModulePaused,
                                      int bShouldAskPwd)
{
  char achTemp[SSCORE_MAXPASSWORDLEN];
  int rc;

  // Check parameters!
  if ((!fnAskUserForPassword) || (!fnShowWrongPassword))
    return 1;

  // Make sure the monitor doesn't stuck in turned off state
  internal_Worker_SetDPMSState(SSDPMS_STATE_ON);

  // Resume saver module, if it was paused!
  if ((*pbSaverModulePaused) &&
      (fnResumeSaving))
  {
#ifdef DEBUG_LOGGING
    AddLog("Worker_PasswordOk : Resuming saver module\n");
#endif
    if (internal_Worker_CallResumeSaving(bIsModuleWrapperUsed, fnResumeSaving) == SSMODULE_NOERROR)
      *pbSaverModulePaused = FALSE;
  }

  // Ask he password if password protected, return with success otherwise!
  if (!bShouldAskPwd)
    return 1;

  // The current settings says that we have to ask for the password!
  // Do it then!

#ifdef DEBUG_LOGGING
  AddLog("Worker : Asking for password!\n");
#endif

  achTemp[0] = 0;
  rc = internal_Worker_CallAskUserForPassword(bIsModuleWrapperUsed, fnAskUserForPassword, sizeof(achTemp), achTemp);

  // If the user pressed cancel, then stay where we are, without showing
  // a window about wrong password.
  if (rc==SSMODULE_ERROR_USERPRESSEDCANCEL)
    return 0;

#ifdef DEBUG_LOGGING
  AddLog("Worker : Not cancel\n");
#endif

  // If we could not ask for password, then something is wrong,
  // so treat it as if the password would have been ok, let the
  // password protected screensaver go away.
  if (rc!=SSMODULE_NOERROR)
    return 1; // Could not ask user password

#ifdef DEBUG_LOGGING
  AddLog("Worker : Not error. Pwd: [");
  AddLog(achTemp);
  AddLog("]\n");
#endif

  // Ok, we have the password entered by the user.

  // Now compare it to the real password!
  // If it was told to use Security/2, and it is installed, then
  // ask Security/2 if this password matches the password of
  // the current user.
  // Otherwise, compare the password to the one we have encrypted!

  if ((GlobalCurrentSettings.bUseCurrentSecurityPassword) &&
      (hmodThisUSERCTL) &&
      (pfnThisDosUserQueryCurrent) &&
      (pfnThisDosUserRegister))
  {
    // Fine, Security/2 is installed, and we were told to use it!
    char achUserName[MAXUSERNAMELEN+1];
    LONG lUID;
    USERREGISTER urTemp;

#ifdef DEBUG_LOGGING
    AddLog("Worker : Comparing to Security/2 user password\n");
#endif

    // Get username and userID of current user logged in!
    rc = pfnThisDosUserQueryCurrent(achUserName, MAXUSERNAMELEN, &lUID);
    if (rc)
    {
      // Could not query current username
#ifdef DEBUG_LOGGING
      AddLog("Worker : Could not query Security/2 username!\n");
#endif
      return 1; // Report success, so the ssaver will stop saving
    }

#ifdef DEBUG_LOGGING
    AddLog("Worker : Current Security/2 user : [");
    AddLog(achUserName);
    AddLog("]\n");
#endif

    // Check password!
    memset(&urTemp, 0, sizeof(urTemp));
    lUID = -1;
    urTemp.cbSize = sizeof(urTemp);
    urTemp.pid = -1;
    urTemp.plUid = &lUID;
    urTemp.pszUser = achUserName;   // UserName
    urTemp.pszPass = achTemp;       // Password
    rc = pfnThisDosUserRegister(&urTemp);

    if (!rc)
    {
#ifdef DEBUG_LOGGING
      AddLog("Worker : Security/2 password matches!\n");
#endif
      return 1;
    }

#ifdef DEBUG_LOGGING
    AddLog("Worker : Security/2 password mismatch!\n");
#endif

    // Let the execution flow...
  } else
  {
    // Security/2 is not installed, or we're told not to use it.

    // Let's encrypt the password we got,
    // and compare to the encrypted one we have in CurrentSettings!
    SSCore_EncryptPassword(achTemp);

#ifdef DEBUG_LOGGING
    AddLog("Worker : Encrypted: [");
    AddLog(achTemp);
    AddLog("] Current: [");
    AddLog(GlobalCurrentSettings.achEncryptedPassword);
    AddLog("]\n");
#endif

    if (!strcmp(achTemp, GlobalCurrentSettings.achEncryptedPassword))
      return 1; // Password matches, fine!

    // If not matches, then go forward...
  }

#ifdef DEBUG_LOGGING
  AddLog("Worker : Wrong pwd!\n");
#endif

  // The password does not match, show window about it!
  internal_Worker_CallShowWrongPassword(bIsModuleWrapperUsed, fnShowWrongPassword);
  // Return with result of wrong password. The screensaver should keep running.
  return 0;
}

void fnSSCoreWorkerThread(void *p)
{
  HAB hab;
  QMSG qmsg;

  HWND hwndSavedPreviewParentWindow;
  char achSavedPreviewModuleFileName[CCHMAXPATHCOMP+15];
  int bPreviewModeStarted;

  int bInPreviewMode;
  HMODULE hmodCurrentSaver;
  int bIsModuleWrapperUsed;

  pfn_SSModule_StartSaving                    fnStartSaving;
  pfn_SSModule_StopSaving                     fnStopSaving;
  pfn_SSModule_AskUserForPassword             fnAskUserForPassword;
  pfn_SSModule_ShowWrongPasswordNotification  fnShowWrongPassword;
  pfn_SSModule_PauseSaving                    fnPauseSaving;
  pfn_SSModule_ResumeSaving                   fnResumeSaving;

  int   iDPMSState;
  ULONG ulDPMSTimerID; // Non-zero, if timer is running
  int   bSaverModulePaused;

  int   bShouldAskPwd; // TRUE, if password has to be asked!
  ULONG ulDelayedPwdProtTimerID; // Non-zero, if timer is running

  HSEM16 hsemDisableVIOPopup;
  int  bDisabledVIOPopup = 0;

  // Initialization

  hmodCurrentSaver = NULLHANDLE;
  bIsModuleWrapperUsed = FALSE;
  bInPreviewMode = FALSE;
  bPreviewModeStarted = FALSE;
  bSaverModulePaused = FALSE;

  ulDPMSTimerID = 0; // No DPMS timer running
  iDPMSState = SSCORE_DPMSSTATE_ON;

  ulDelayedPwdProtTimerID = 0; // No delayed password protection timer running
  bShouldAskPwd = FALSE;

  // Initialize DPMS support for this process
  internal_TryToLoadSSDPMS();

  // Initialize message queue
  hab = WinInitialize(0);
  hmqGlobalWorkerThreadCommandQueue = WinCreateMsgQueue(hab, 0);
  if (hmqGlobalWorkerThreadCommandQueue)
  {
    // Make sure this message queue will not stop system shutdown!
    WinCancelShutdown(hmqGlobalWorkerThreadCommandQueue, TRUE);

    // Note that we're not saving yet.
    iGlobalCurrentState = SSCORE_STATE_NORMAL;

    iGlobalWorkerThreadState = WORKER_STATE_RUNNING;
    qmsg.msg = WORKER_COMMAND_NONE;
    while (qmsg.msg!=WORKER_COMMAND_SHUTDOWN)
    {
      // Process messages!
      // Wait for a message!
      WinGetMsg(hab, &qmsg, NULLHANDLE, 0, 0);

      switch (qmsg.msg)
      {
        case WM_TIMER:
          if ((USHORT) qmsg.mp1 == ulDelayedPwdProtTimerID)
          {
            // Delayed Pwd protection timer timed out!
#ifdef DEBUG_LOGGING
            AddLog("Worker : Delayed pwd protection timer timed out\n");
#endif
            // Stop timer
            WinStopTimer(0, 0, ulDelayedPwdProtTimerID);
            ulDelayedPwdProtTimerID = 0;

            // Change 'bShouldAskPwd' then!
            bShouldAskPwd = GlobalCurrentSettings.bPasswordProtected;
          }
          if ((USHORT) qmsg.mp1 == ulDPMSTimerID)
          {
            int iDPMSCaps;
            int iDPMSrc;

#ifdef DEBUG_LOGGING
            AddLog("Worker : DPMS timer timed out\n");
#endif
            // DPMS timer timed out! Change DPMS state then!

            // Query DPMS capabilities!
            if ((!bThisDPMSSupportPresent) ||
                (!fn_SSDPMS_GetCapabilities) ||
                (fn_SSDPMS_GetCapabilities(&iDPMSCaps)!=SSDPMS_NOERROR))
              iDPMSCaps = 0; // Support for nothing

            // Stop old timer, and let's see if we can go to another state!
            WinStopTimer(0, 0, ulDPMSTimerID);
            ulDPMSTimerID = 0;

            // Go for next state!
            iDPMSState++;

            switch (iDPMSState)
            {
              case SSCORE_DPMSSTATE_STANDBY:
		// Set new DPMS state
		iDPMSrc = internal_Worker_SetDPMSState(SSDPMS_STATE_STANDBY);
		if (iDPMSrc)
		{
		  // Successfully set DPMS mode, so pause saver module!
		  if ((!bSaverModulePaused) &&
		      (fnPauseSaving) &&
		      (fnResumeSaving))
		  {
#ifdef DEBUG_LOGGING
		    AddLog("Worker : Pausing saver module\n");
#endif
                    bSaverModulePaused = (internal_Worker_CallPauseSaving(bIsModuleWrapperUsed, fnPauseSaving) == SSMODULE_NOERROR);
		  }
		}
                // Start timer for next requested state, if there is any!
                if ((bThisDPMSSupportPresent) &&
                    (GlobalCurrentSettings.bUseDPMSSuspendState) &&
                    (iDPMSCaps & SSDPMS_STATE_SUSPEND)
                   )
                {
#ifdef DEBUG_LOGGING
                  AddLog("Worker : Restarting DPMS timer for Suspend mode\n");
#endif
                  iDPMSState = SSCORE_DPMSSTATE_SUSPEND-1;
                  ulDPMSTimerID = WinStartTimer(0, 0, 0,
                                                GlobalCurrentSettings.iDPMSSuspendTime);
                } else
                if ((bThisDPMSSupportPresent) &&
                    (GlobalCurrentSettings.bUseDPMSOffState) &&
                    (iDPMSCaps & SSDPMS_STATE_OFF)
                   )
                {
#ifdef DEBUG_LOGGING
                  AddLog("Worker : Restarting DPMS timer for Off mode\n");
#endif

                  iDPMSState = SSCORE_DPMSSTATE_OFF-1;
                  ulDPMSTimerID = WinStartTimer(0, 0, 0,
                                                GlobalCurrentSettings.iDPMSOffTime);
                }
                break;
              case SSCORE_DPMSSTATE_SUSPEND:
                // Set new DPMS state
		iDPMSrc = internal_Worker_SetDPMSState(SSDPMS_STATE_SUSPEND);
		if (iDPMSrc)
		{
		  // Successfully set DPMS mode, so pause saver module!
		  if ((!bSaverModulePaused) &&
		      (fnPauseSaving) &&
		      (fnResumeSaving))
		  {
#ifdef DEBUG_LOGGING
		    AddLog("Worker : Pausing saver module\n");
#endif
                    bSaverModulePaused = (internal_Worker_CallPauseSaving(bIsModuleWrapperUsed, fnPauseSaving) == SSMODULE_NOERROR);
		  }
		}

                // Start timer for next requested state, if there is any!
                if ((bThisDPMSSupportPresent) &&
                    (GlobalCurrentSettings.bUseDPMSOffState) &&
                    (iDPMSCaps & SSDPMS_STATE_OFF)
                   )
                {
#ifdef DEBUG_LOGGING
                  AddLog("Worker : Restarting DPMS timer for Off mode\n");
#endif
                  iDPMSState = SSCORE_DPMSSTATE_OFF-1;
                  ulDPMSTimerID = WinStartTimer(0, 0, 0,
                                                GlobalCurrentSettings.iDPMSOffTime);
                }
                break;
              case SSCORE_DPMSSTATE_OFF:
                // Set new DPMS state
		iDPMSrc = internal_Worker_SetDPMSState(SSDPMS_STATE_OFF);
		if (iDPMSrc)
		{
		  // Successfully set DPMS mode, so pause saver module!
		  if ((!bSaverModulePaused) &&
		      (fnPauseSaving) &&
		      (fnResumeSaving))
		  {
#ifdef DEBUG_LOGGING
		    AddLog("Worker : Pausing saver module\n");
#endif
                    bSaverModulePaused = (internal_Worker_CallPauseSaving(bIsModuleWrapperUsed, fnPauseSaving) == SSMODULE_NOERROR);
		  }
		}
                break;
              case SSCORE_DPMSSTATE_ON:
              default:
                // The execution should never come here, but who knows...
		iDPMSrc = internal_Worker_SetDPMSState(SSDPMS_STATE_ON);
		if (iDPMSrc)
		{
		  // Successfully set DPMS mode, so resume saver module!
		  if ((bSaverModulePaused) &&
		      (fnResumeSaving))
		  {
#ifdef DEBUG_LOGGING
		    AddLog("Worker : Resuming saver module\n");
#endif
		    if (internal_Worker_CallResumeSaving(bIsModuleWrapperUsed, fnResumeSaving) == SSMODULE_NOERROR)
                      bSaverModulePaused = FALSE;
		  }
		}
                break;
            }

          }
          break;
        case WORKER_COMMAND_SENTINELTIMEOUT:
#ifdef DEBUG_LOGGING
          AddLog("Worker : SENTINELTIMEOUT\n");
#endif
          // It's timeout, so we should start the screensaver, if it's not
          // temporary disabled, and no one is running now!

          if ((GlobalCurrentSettings.bEnabled) &&   // If the Saving is enabled
              (!iGlobalTempDisableCounter) &&       // and not temporarily disabled by an app
              ((bInPreviewMode) ||                  // and we're in preview mode (so no saver running now)
               ((!bInPreviewMode) && (hmodCurrentSaver==NULLHANDLE)) && // or not preview mode and no saver running
               (internal_IsPMTheForegroundFSSession()) // and the user has not switched to another session
              )
             )
          {
            // Time to start the screensaver
#ifdef DEBUG_LOGGING
            AddLog("Worker : Try to start saver\n");
#endif
            internal_Worker_StartSaver(GlobalCurrentSettings.achModuleFileName,
                                       HWND_DESKTOP,
                                       FALSE,
                                       &hmodCurrentSaver,
                                       &bIsModuleWrapperUsed,
                                       &fnStartSaving,
                                       &fnStopSaving,
                                       &fnAskUserForPassword,
                                       &fnShowWrongPassword,
                                       &fnPauseSaving,
                                       &fnResumeSaving,
                                       &bSaverModulePaused,
                                       &bInPreviewMode,
                                       &bShouldAskPwd,
                                       &ulDelayedPwdProtTimerID,
                                       &ulDPMSTimerID,
                                       &iDPMSState,
                                       hab,
                                       &hsemDisableVIOPopup,
                                       &bDisabledVIOPopup);

          }
#ifdef DEBUG_LOGGING
          else
            AddLog("Worker : Not starting saver\n");
#endif

          break;
        case WORKER_COMMAND_SENTINELACTIVITY:
#ifdef DEBUG_LOGGING
//          AddLog("Worker : SENTINELACTIVITY\n");
#endif
          // The user was interacting with the computer!
          // Stop the screensaver, if running, and not in preview mode!
          if ((!bInPreviewMode) && (hmodCurrentSaver!=NULLHANDLE))
          {
#ifdef DEBUG_LOGGING
            AddLog("Worker : Not in preview mode, saver running, so stop saver\n");
#endif
            // If it's password protected, ask for the password first!
            if (internal_Worker_PasswordOk(bIsModuleWrapperUsed,
                                           fnAskUserForPassword,
					   fnShowWrongPassword,
					   fnPauseSaving,
					   fnResumeSaving,
                                           &bSaverModulePaused,
                                           bShouldAskPwd))
            {
              // If the user gave us the right password, then everything is
              // all right! Stop the screensaver, and optionally restart it
              // in preview mode!
              internal_Worker_StopSaver(HWND_DESKTOP,
                                        &hmodCurrentSaver,
                                        &bIsModuleWrapperUsed,
                                        &fnStartSaving,
                                        &fnStopSaving,
                                        &fnAskUserForPassword,
                                        &fnShowWrongPassword,
                                        &fnPauseSaving,
                                        &fnResumeSaving,
                                        &bSaverModulePaused,
                                        &bInPreviewMode,
                                        &ulDelayedPwdProtTimerID,
                                        &ulDPMSTimerID,
                                        &hsemDisableVIOPopup,
                                        &bDisabledVIOPopup);

              // Also, if we was in preview mode before, then restart that preview module!
              if (bPreviewModeStarted)
              {
#ifdef DEBUG_LOGGING
                AddLog("Worker : Restarting preview mode\n");
#endif
                internal_Worker_StartSaver(achSavedPreviewModuleFileName,
                                           hwndSavedPreviewParentWindow,
                                           TRUE,
                                           &hmodCurrentSaver,
                                           &bIsModuleWrapperUsed,
                                           &fnStartSaving,
                                           &fnStopSaving,
                                           &fnAskUserForPassword,
                                           &fnShowWrongPassword,
                                           &fnPauseSaving,
                                           &fnResumeSaving,
                                           &bSaverModulePaused,
                                           &bInPreviewMode,
                                           &bShouldAskPwd,
                                           &ulDelayedPwdProtTimerID,
                                           &ulDPMSTimerID,
                                           &iDPMSState,
                                           hab,
                                           &hsemDisableVIOPopup,
                                           &bDisabledVIOPopup);
              }
            } else
            {
              int iDPMSCaps;

#ifdef DEBUG_LOGGING
              AddLog("Worker : Wrong pwd! Removing sentinelactivity messages from queue\n");
#endif

              // If the user could not give us the right password,
              // then remove any pending SENTINELACTIVITY messages from
              // the queue, because they would stop the saver again!
              internal_Worker_RemoveSentinelActivityFromQueue(hab);

              // Restart DPMS timer, if needed!
#ifdef DEBUG_LOGGING
              AddLog("Worker : Restarting DPMS timer if needed!\n");
#endif

              // Stop old timer
              if (ulDPMSTimerID)
              {
                WinStopTimer(0, 0, ulDPMSTimerID);
                ulDPMSTimerID = 0;
              }

              // Query DPMS capabilities!
              if ((!bThisDPMSSupportPresent) ||
                  (!fn_SSDPMS_GetCapabilities) ||
                  (fn_SSDPMS_GetCapabilities(&iDPMSCaps)!=SSDPMS_NOERROR))
                iDPMSCaps = 0; // Support for nothing

              // Restart timer from new state!
              if ((bThisDPMSSupportPresent) &&
                  ((GlobalCurrentSettings.bUseDPMSStandbyState) ||
                   (GlobalCurrentSettings.bUseDPMSSuspendState) ||
                   (GlobalCurrentSettings.bUseDPMSOffState))
                  )
              {
                if (
                    (GlobalCurrentSettings.bUseDPMSStandbyState) &&
                    (iDPMSCaps & SSDPMS_STATE_STANDBY)
                   )
                {
                  iDPMSState = SSCORE_DPMSSTATE_STANDBY-1; // So the next will be Standby
                  ulDPMSTimerID = WinStartTimer(0, 0, 0,
                                                GlobalCurrentSettings.iDPMSStandbyTime);
                } else
                if (
                    (GlobalCurrentSettings.bUseDPMSSuspendState) &&
                    (iDPMSCaps & SSDPMS_STATE_SUSPEND)
                   )
                {
                  iDPMSState = SSCORE_DPMSSTATE_SUSPEND-1; // So the next will be Suspend
                  ulDPMSTimerID = WinStartTimer(0, 0, 0,
                                                GlobalCurrentSettings.iDPMSSuspendTime);
                } else
                if (
                    (GlobalCurrentSettings.bUseDPMSOffState) &&
                    (iDPMSCaps & SSDPMS_STATE_OFF)
                   )

                {
                  iDPMSState = SSCORE_DPMSSTATE_OFF-1; // So the next will be Off
                  ulDPMSTimerID = WinStartTimer(0, 0, 0,
                                                GlobalCurrentSettings.iDPMSOffTime);
                } else
                {
                  // Ooops, something wrong if execution comes here :)
                  // Don't start timer then!
                }
              }
            }
          }
          break;

        case WORKER_COMMAND_STARTSAVINGNOW:
#ifdef DEBUG_LOGGING
          AddLog("Worker : Command is STARTSAVINGNOW!\n");
#endif
          internal_Worker_StartSaver(GlobalCurrentSettings.achModuleFileName,
                                     HWND_DESKTOP,
                                     FALSE,
                                     &hmodCurrentSaver,
                                     &bIsModuleWrapperUsed,
                                     &fnStartSaving,
                                     &fnStopSaving,
                                     &fnAskUserForPassword,
                                     &fnShowWrongPassword,
                                     &fnPauseSaving,
                                     &fnResumeSaving,
                                     &bSaverModulePaused,
                                     &bInPreviewMode,
                                     &bShouldAskPwd,
                                     &ulDelayedPwdProtTimerID,
                                     &ulDPMSTimerID,
                                     &iDPMSState,
                                     hab,
                                     &hsemDisableVIOPopup,
                                     &bDisabledVIOPopup);

          if (qmsg.mp1)
          {
#ifdef DEBUG_LOGGING
            AddLog("Worker : parameter: DONT DELAY PWD PROT!\n");
#endif
            // Do it as if it would not be delayed password protection!
            // If its not delayed password protected, then
            // bShouldAskPwd depends only on the password protection!
            bShouldAskPwd = GlobalCurrentSettings.bPasswordProtected;
          }

          break;
        case WORKER_COMMAND_STOPSAVINGNOW:
#ifdef DEBUG_LOGGING
          AddLog("Worker : Command is STOPSAVINGNOW!\n");
#endif

          // Somebody asks us to stop the saving immediately!
          // Stop the screensaver, if running, and not in preview mode!
          if ((!bInPreviewMode) && (hmodCurrentSaver!=NULLHANDLE))
          {
#ifdef DEBUG_LOGGING
            AddLog("Worker : Not in preview mode, saver running, so stop saver\n");
#endif
            internal_Worker_StopSaver((HWND) (qmsg.mp1),
                                      &hmodCurrentSaver,
                                      &bIsModuleWrapperUsed,
                                      &fnStartSaving,
                                      &fnStopSaving,
                                      &fnAskUserForPassword,
                                      &fnShowWrongPassword,
                                      &fnPauseSaving,
                                      &fnResumeSaving,
                                      &bSaverModulePaused,
                                      &bInPreviewMode,
                                      &ulDelayedPwdProtTimerID,
                                      &ulDPMSTimerID,
                                      &hsemDisableVIOPopup,
                                      &bDisabledVIOPopup);

            // Also, if we was in preview mode before, then restart that preview module!
            if (bPreviewModeStarted)
            {
#ifdef DEBUG_LOGGING
              AddLog("Worker : Restarting preview mode\n");
#endif
              internal_Worker_StartSaver(achSavedPreviewModuleFileName,
                                         hwndSavedPreviewParentWindow,
                                         TRUE,
                                         &hmodCurrentSaver,
                                         &bIsModuleWrapperUsed,
                                         &fnStartSaving,
                                         &fnStopSaving,
                                         &fnAskUserForPassword,
                                         &fnShowWrongPassword,
                                         &fnPauseSaving,
                                         &fnResumeSaving,
                                         &bSaverModulePaused,
                                         &bInPreviewMode,
                                         &bShouldAskPwd,
                                         &ulDelayedPwdProtTimerID,
                                         &ulDPMSTimerID,
                                         &iDPMSState,
                                         hab,
                                         &hsemDisableVIOPopup,
                                         &bDisabledVIOPopup);
            }
          }
          break;
        case WORKER_COMMAND_STARTPREVIEW:
#ifdef DEBUG_LOGGING
          AddLog("Worker : Command is STARTPREVIEW\n");
#endif
          strcpy(achSavedPreviewModuleFileName, (char *) (qmsg.mp2));
          hwndSavedPreviewParentWindow = (HWND) (qmsg.mp1);
          internal_Worker_StartSaver((char *) (qmsg.mp2),
                                     (HWND) (qmsg.mp1),
                                     TRUE,
                                     &hmodCurrentSaver,
                                     &bIsModuleWrapperUsed,
                                     &fnStartSaving,
                                     &fnStopSaving,
                                     &fnAskUserForPassword,
                                     &fnShowWrongPassword,
                                     &fnPauseSaving,
                                     &fnResumeSaving,
                                     &bSaverModulePaused,
                                     &bInPreviewMode,
                                     &bShouldAskPwd,
                                     &ulDelayedPwdProtTimerID,
                                     &ulDPMSTimerID,
                                     &iDPMSState,
                                     hab,
                                     &hsemDisableVIOPopup,
                                     &bDisabledVIOPopup);

          bPreviewModeStarted = bInPreviewMode;

          if (bPreviewModeStarted)
            internal_Worker_PostMsgToParent((HWND) (qmsg.mp1),
                                            WM_SSCORENOTIFY_PREVIEWSTARTED,
                                            (MPARAM) SSCORE_NOERROR,
                                            (MPARAM) 0);
          else
            internal_Worker_PostMsgToParent((HWND) (qmsg.mp1),
                                            WM_SSCORENOTIFY_PREVIEWSTARTED,
                                            (MPARAM) SSCORE_ERROR_INTERNALERROR,
                                            (MPARAM) 0);
          break;

        case WORKER_COMMAND_STOPPREVIEW:
#ifdef DEBUG_LOGGING
          AddLog("Worker : Command is STOPPREVIEW\n");
#endif
          if (bInPreviewMode)
            internal_Worker_StopSaver((HWND) (qmsg.mp1),
                                      &hmodCurrentSaver,
                                      &bIsModuleWrapperUsed,
                                      &fnStartSaving,
                                      &fnStopSaving,
                                      &fnAskUserForPassword,
                                      &fnShowWrongPassword,
                                      &fnPauseSaving,
                                      &fnResumeSaving,
                                      &bSaverModulePaused,
                                      &bInPreviewMode,
                                      &ulDelayedPwdProtTimerID,
                                      &ulDPMSTimerID,
                                      &hsemDisableVIOPopup,
                                      &bDisabledVIOPopup);

          bPreviewModeStarted = FALSE;
          internal_Worker_PostMsgToParent((HWND) (qmsg.mp1),
                                          WM_SSCORENOTIFY_PREVIEWSTOPPED,
                                          (MPARAM) SSCORE_NOERROR,
                                          (MPARAM) 0);
#ifdef DEBUG_LOGGING
          AddLog("Worker : STOPPREVIEW command processed.\n");
#endif
          break;
        default:
          // Internal error, unknown command!
#ifdef DEBUG_LOGGING
          AddLog("Worker : Command is unknown\n");
#endif
          break;
      }
    }

    // Stop the screensaver, if running!
    // It also stops timers, and turns monitor on.
    internal_Worker_StopSaver(HWND_DESKTOP,
                              &hmodCurrentSaver,
                              &bIsModuleWrapperUsed,
                              &fnStartSaving,
                              &fnStopSaving,
                              &fnAskUserForPassword,
                              &fnShowWrongPassword,
                              &fnPauseSaving,
                              &fnResumeSaving,
                              &bSaverModulePaused,
                              &bInPreviewMode,
                              &ulDelayedPwdProtTimerID,
                              &ulDPMSTimerID,
                              &hsemDisableVIOPopup,
                              &bDisabledVIOPopup);

    // Destroy msg queue
    WinDestroyMsgQueue(hmqGlobalWorkerThreadCommandQueue);
  }

  WinTerminate(hab);
#ifdef DEBUG_LOGGING
  AddLog("Worker : Stopped.\n");
#endif

  // Uninitialize DPMS support of this process (if we could load it)
  internal_UnloadSSDPMS();

  iGlobalWorkerThreadState = WORKER_STATE_STOPPED;
  _endthread();
}


void fnSSCoreSentinelThread(void *p)
{
  int rc;
  ULONG ulPostCount;

  iGlobalSentinelThreadState = WORKER_STATE_RUNNING;
  while (!bGlobalSentinelThreadShutdownRequest)
  {
    // Wait for either an event to happen, or to the timeout to happen!
    rc = DosWaitEventSem(hevGlobalSentinelEvent, GlobalCurrentSettings.iActivationTime);
    DosResetEventSem(hevGlobalSentinelEvent, &ulPostCount);
    // Let's see what happened!
    if (rc==ERROR_TIMEOUT)
    {
#ifdef DEBUG_LOGGING
      AddLog("Sentinel : Out of semwait - by timeout!\n");
#endif
      // It's timeout, so we should start the screensaver
      WinPostQueueMsg(hmqGlobalWorkerThreadCommandQueue,
                      WORKER_COMMAND_SENTINELTIMEOUT,
                      (MPARAM) 0, (MPARAM) 0);
    } else
    if (rc==NO_ERROR)
    {
#ifdef DEBUG_LOGGING
//      AddLog("Sentinel : Out of semwait - NO_ERROR, Posting ACTIVITY!\n");
#endif
      WinPostQueueMsg(hmqGlobalWorkerThreadCommandQueue,
                      WORKER_COMMAND_SENTINELACTIVITY,
                      (MPARAM) 0, (MPARAM) 0);
    } else
    {
#ifdef DEBUG_LOGGING
      AddLog("Sentinel : Out of semwait - by ERROR! Stopping!\n");
#endif
      bGlobalSentinelThreadShutdownRequest = 1;
    }
    // If out of semwait because of the semaphore has been posted,
    // then restart the waiting!
  }

#ifdef DEBUG_LOGGING
  AddLog("Sentinel : Stopped.\n");
#endif

  iGlobalSentinelThreadState = WORKER_STATE_STOPPED;
  _endthread();
}


SSCOREDECLSPEC int SSCORECALL SSCore_GetInfo(SSCoreInfo_p pInfo, int iBufSize)
{
  SSCoreInfo_t Info;
  int iInfoSize;

  if ((!pInfo) || (iBufSize<=0))
    return SSCORE_ERROR_INVALIDPARAMETER;

  // Return version info about SSCore
  Info.iVersionMajor = SSCORE_VERSIONMAJOR;
  Info.iVersionMinor = SSCORE_VERSIONMINOR;

  // Return DPMS capabilities
  Info.iDPMSCapabilities = SSCORE_DPMSCAPS_NODPMS;
  if (bThisDPMSSupportPresent) // If we could load SSDPMS.DLL for this process, then...
  {
    int iCaps;

    // Query DPMS capabilities
    if ((fn_SSDPMS_GetCapabilities) &&
        (fn_SSDPMS_GetCapabilities(&iCaps)==SSDPMS_NOERROR))
    {
      // Okay, we could query DPMS capabilities!
      if (iCaps & SSDPMS_STATE_STANDBY)
        Info.iDPMSCapabilities |= SSCORE_DPMSCAPS_STATE_STANDBY;
      if (iCaps & SSDPMS_STATE_SUSPEND)
        Info.iDPMSCapabilities |= SSCORE_DPMSCAPS_STATE_SUSPEND;
      if (iCaps & SSDPMS_STATE_OFF)
        Info.iDPMSCapabilities |= SSCORE_DPMSCAPS_STATE_OFF;
    }
  }

  // If we could load USERCTL.DLL and query its functions, then Security/2 is present!
  Info.bSecurityPresent = (hmodThisUSERCTL!=NULLHANDLE);

  // Copy the info to the info buffer, but only as much as fits in there!
  iInfoSize = sizeof(Info);
  if (iInfoSize<iBufSize)
    memcpy(pInfo, &Info, iInfoSize);
  else
    memcpy(pInfo, &Info, iBufSize);

#ifdef DEBUG_LOGGING
  {
    char achTemp[256];
    AddLog("[SSCore_GetInfo] : Returning the following info:\n");
    sprintf(achTemp, "  Version info:  v%d.%02d\n", Info.iVersionMajor, Info.iVersionMinor);
    AddLog(achTemp);
    sprintf(achTemp, "  iDPMSCapabilities: %d\n", Info.iDPMSCapabilities);
    AddLog(achTemp);
    sprintf(achTemp, "  bSecurityPresent : %d\n", Info.bSecurityPresent);
    AddLog(achTemp);
  }
#endif

  return SSCORE_NOERROR;
}

SSCOREDECLSPEC int SSCORECALL SSCore_Initialize(HAB habCaller, char *pchHomeDirectory)
{
  int rc;
  int iLen;
  unsigned long ulCurrentTime;

  if (!pchHomeDirectory)
    return SSCORE_ERROR_INVALIDPARAMETER;

  if (bGlobalInitialized)
    return SSCORE_ERROR_ALREADYINITIALIZED;

#ifdef DEBUG_LOGGING
  AddLog("[SSCore_Initialize] : Initializing!\n");
  AddLog("   Home: [");AddLog(pchHomeDirectory);AddLog("]\n");
#endif

  bGlobalInitialized = TRUE;
  // Save directory where the INI/CFG files will be stored
  strncpy(achGlobalHomeDirectory, pchHomeDirectory, sizeof(achGlobalHomeDirectory)-1);

  // Make sure that the trailing '\' will be there!
  iLen = strlen(achGlobalHomeDirectory);
  if ((iLen>0) &&
      (achGlobalHomeDirectory[iLen-1]!='\\'))
  {
    achGlobalHomeDirectory[iLen]='\\';
    achGlobalHomeDirectory[iLen+1]=0;
  }

  // Set current time to be the last time when mouse/keyboard event happened
  rc = DosQuerySysInfo(QSV_MS_COUNT,
                       QSV_MS_COUNT,
                       &ulCurrentTime,
                       sizeof(ulCurrentTime));
  if (rc!=NO_ERROR)
    ulCurrentTime=0;

  ulGlobalLastMouseEventTimestamp =
    ulGlobalLastKeyboardEventTimestamp =
    ulCurrentTime;

#ifdef DEBUG_LOGGING
  AddLog("[SSCore_Initialize] : Setting defaults\n");
#endif

  // Set default configuration/settings
  GlobalCurrentSettings.bEnabled = FALSE;
  strcpy(GlobalCurrentSettings.achModuleFileName, "");
  GlobalCurrentSettings.iActivationTime = 3*60000;  // msec = 3 min
  GlobalCurrentSettings.bPasswordProtected = FALSE;
  GlobalCurrentSettings.bUseCurrentSecurityPassword = FALSE;
  strcpy(GlobalCurrentSettings.achEncryptedPassword, "");
  GlobalCurrentSettings.bUseDelayedPasswordProtection = 0; // No delayed password asking
  GlobalCurrentSettings.iPasswordDelayTime = 60000; // No delayed password asking, but still, 1 min is the minimum
  GlobalCurrentSettings.bUseDPMSStandbyState = 0;
  GlobalCurrentSettings.iDPMSStandbyTime = 5*60000; // Set standby mode in 5 secs
  GlobalCurrentSettings.bUseDPMSSuspendState = 0;
  GlobalCurrentSettings.iDPMSSuspendTime = 5*60000; // Set suspend mode in 5 more secs
  GlobalCurrentSettings.bUseDPMSOffState = 1;
  GlobalCurrentSettings.iDPMSOffTime     = 5*60000; // Turn off monitor in 5 more secs
  GlobalCurrentSettings.bWakeByKeyboardOnly = FALSE;
  GlobalCurrentSettings.bFirstKeyEventGoesToPwdWindow = FALSE;
  GlobalCurrentSettings.bDisableVIOPopups = TRUE;
  GlobalCurrentSettings.bUseModuleWrapper = TRUE;

#ifdef DEBUG_LOGGING
  AddLog("[SSCore_Initialize] : Trying to load settings from cfg file\n");
#endif

  // Load settings from INI file
  internal_LoadSettings(pchHomeDirectory);

  pidGlobalInitializerClient = pidThisClient;
  habGlobalInitializerClient = habCaller;

  // Create sentinel event semaphore
  rc = DosCreateEventSem(ACTION_EVENT_SHARED_SEM_NAME,
                         &hevGlobalSentinelEvent,
			 0,
                         FALSE);
  if (rc!=NO_ERROR)
  {
    // Error creating the event semaphore!
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_Initialize] : Error creating sentinel Event semaphore\n");
#endif

    bGlobalInitialized = FALSE;
    pidGlobalInitializerClient = NULL;
    return SSCORE_ERROR_INTERNALERROR;
  }

  // Create state-change event semaphore
  iGlobalCurrentState = SSCORE_STATE_NORMAL;
  rc = DosCreateEventSem(STATECHANGE_EVENT_SHARED_SEM_NAME,
                         &hevGlobalStateChangeEvent,
			 0,
                         FALSE);
  if (rc!=NO_ERROR)
  {
    // Error creating the event semaphore!
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_Initialize] : Error creating StateChange Event semaphore\n");
#endif

    DosCloseEventSem(hevGlobalSentinelEvent);

    bGlobalInitialized = FALSE;
    pidGlobalInitializerClient = NULL;
    return SSCORE_ERROR_INTERNALERROR;
  }

  // Start worker thread
  iGlobalWorkerThreadState = WORKER_STATE_UNKNOWN;
  tidGlobalWorkerThread = _beginthread(fnSSCoreWorkerThread,
                                       0,
                                       1024*1024,
                                       NULL);

  if (tidGlobalWorkerThread==0)
  {
    // Error creating worker thread!
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_Initialize] : Error creating Worker thread\n");
#endif
    DosCloseEventSem(hevGlobalStateChangeEvent);
    DosCloseEventSem(hevGlobalSentinelEvent);
    bGlobalInitialized = FALSE;
    pidGlobalInitializerClient = NULL;
    return SSCORE_ERROR_INTERNALERROR;
  }

  // Wait for the worker thread to be initialized!
  while (iGlobalWorkerThreadState==WORKER_STATE_UNKNOWN)
    DosSleep(32);

  if (iGlobalWorkerThreadState!=WORKER_STATE_RUNNING)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_Initialize] : Something wrong with worker thread\n");
#endif
    // Something wrong with worker thread!
    WinPostQueueMsg(hmqGlobalWorkerThreadCommandQueue,
                    WORKER_COMMAND_SHUTDOWN,
                    (MPARAM) 0, (MPARAM) 0);
    DosWaitThread(&tidGlobalWorkerThread, DCWW_WAIT);
    DosCloseEventSem(hevGlobalStateChangeEvent);
    DosCloseEventSem(hevGlobalSentinelEvent);
    bGlobalInitialized = FALSE;
    pidGlobalInitializerClient = NULL;
    return SSCORE_ERROR_INTERNALERROR;
  }

  // Ok, worker is running, now start sentinel thread!
  iGlobalSentinelThreadState = WORKER_STATE_UNKNOWN;
  bGlobalSentinelThreadShutdownRequest = 0;
  tidGlobalSentinelThread = _beginthread(fnSSCoreSentinelThread,
                                         0,
                                         1024*1024,
                                         NULL);

  if (tidGlobalSentinelThread==0)
  {
    // Error creating sentinel thread!
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_Initialize] : Error creating sentinel thread\n");
#endif

    WinPostQueueMsg(hmqGlobalWorkerThreadCommandQueue,
                    WORKER_COMMAND_SHUTDOWN,
                    (MPARAM) 0, (MPARAM) 0);
    DosWaitThread(&tidGlobalWorkerThread, DCWW_WAIT);
    DosCloseEventSem(hevGlobalStateChangeEvent);
    DosCloseEventSem(hevGlobalSentinelEvent);
    bGlobalInitialized = FALSE;
    pidGlobalInitializerClient = NULL;
    return SSCORE_ERROR_INTERNALERROR;
  }

  // Wait for the sentinel thread to be initialized!
  while (iGlobalSentinelThreadState==WORKER_STATE_UNKNOWN)
    DosSleep(32);

  if (iGlobalSentinelThreadState!=WORKER_STATE_RUNNING)
  {
    // Something wrong with sentinel thread!
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_Initialize] : Something wrong with sentinel thread\n");
#endif

    bGlobalSentinelThreadShutdownRequest = 1;
    DosPostEventSem(hevGlobalSentinelEvent); // Make sure it gets the change!
    DosWaitThread(&tidGlobalSentinelThread, DCWW_WAIT);

    WinPostQueueMsg(hmqGlobalWorkerThreadCommandQueue,
                    WORKER_COMMAND_SHUTDOWN,
                    (MPARAM) 0, (MPARAM) 0);
    DosWaitThread(&tidGlobalWorkerThread, DCWW_WAIT);
    DosCloseEventSem(hevGlobalStateChangeEvent);
    DosCloseEventSem(hevGlobalSentinelEvent);

    bGlobalInitialized = FALSE;
    pidGlobalInitializerClient = NULL;
    return SSCORE_ERROR_INTERNALERROR;
  }

#ifdef DEBUG_LOGGING
  AddLog("[SSCore_Initialize] : Threads running, setting hook\n");
#endif

  // Ok, everything is initialized, both worker thread and sentinel thread are running!
  rc = WinSetHook(habCaller,
		  NULLHANDLE,
		  HK_INPUT,
		  (PFN) fnInputHook,
		  hmodGlobalDLLHandle);

  if (!rc)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_Initialize] : Error setting input hook!\n");
#endif

    // Stop sentinel thread
    bGlobalSentinelThreadShutdownRequest = 1;
    DosPostEventSem(hevGlobalSentinelEvent); // Make sure it gets the change!
    DosWaitThread(&tidGlobalSentinelThread, DCWW_WAIT);

    // Stop worker thread
    WinPostQueueMsg(hmqGlobalWorkerThreadCommandQueue,
                    WORKER_COMMAND_SHUTDOWN,
                    (MPARAM) 0, (MPARAM) 0);
    DosWaitThread(&tidGlobalWorkerThread, DCWW_WAIT);
    DosCloseEventSem(hevGlobalStateChangeEvent);
    DosCloseEventSem(hevGlobalSentinelEvent);

    bGlobalInitialized = FALSE;
    pidGlobalInitializerClient = NULL;
    return SSCORE_ERROR_INTERNALERROR;
  }

#ifdef DEBUG_LOGGING
  AddLog("[SSCore_Initialize] : Returning ok\n");
#endif

  return SSCORE_NOERROR;
}

SSCOREDECLSPEC int SSCORECALL SSCore_Uninitialize(void)
{
  if (!bGlobalInitialized)
    return SSCORE_ERROR_NOTINITIALIZED;

  // We let the uninitialization only to the process who initialized it!
  if (pidGlobalInitializerClient!=pidThisClient)
    return SSCORE_ERROR_ACCESSDENIED;

  WinReleaseHook(habGlobalInitializerClient,
		 NULLHANDLE,
		 HK_INPUT,
		 (PFN) fnInputHook,
		 hmodGlobalDLLHandle);
  // Also broadcast NULL message to all top-level windows, so they'll
  // release our DLL.
  WinBroadcastMsg(HWND_DESKTOP,WM_NULL,0,0,BMSG_FRAMEONLY|BMSG_POST);

  // Stop sentinel thread
  bGlobalSentinelThreadShutdownRequest = 1;
  DosPostEventSem(hevGlobalSentinelEvent); // Make sure it gets the change!
  DosWaitThread(&tidGlobalSentinelThread, DCWW_WAIT);

  // Stop worker thread
  WinPostQueueMsg(hmqGlobalWorkerThreadCommandQueue,
                  WORKER_COMMAND_SHUTDOWN,
                  (MPARAM) 0, (MPARAM) 0);
  DosWaitThread(&tidGlobalWorkerThread, DCWW_WAIT);

  // Close event semaphores
  DosCloseEventSem(hevGlobalStateChangeEvent);
  DosCloseEventSem(hevGlobalSentinelEvent);

  bGlobalInitialized = FALSE;
  pidGlobalInitializerClient = NULLHANDLE;

  return SSCORE_NOERROR;
}

SSCOREDECLSPEC int SSCORECALL SSCore_GetListOfModules(SSCoreModuleList_p *ppModuleList)
{
  char achSearchPattern[CCHMAXPATHCOMP+15];
  char achFileNamePath[CCHMAXPATHCOMP+15];
  char achFailure[32];
  HDIR hdirFindHandle;
  FILEFINDBUF3 FindBuffer;
  ULONG ulResultBufLen;
  ULONG ulFindCount;
  APIRET rc, rcfind;
  HMODULE hmodDLL;
  SSCoreModuleList_p pNewEntry;

  if (!ppModuleList)
    return SSCORE_ERROR_INVALIDPARAMETER;

  if (!bGlobalInitialized)
    return SSCORE_ERROR_NOTINITIALIZED;

#ifdef DEBUG_LOGGING
  AddLog("[SSCore_GetListOfModules] : Prepare\n");
#endif

  *ppModuleList = NULL;

  hdirFindHandle = HDIR_CREATE;
  ulResultBufLen = sizeof(FindBuffer);
  ulFindCount = 1;

  snprintf(achSearchPattern, sizeof(achSearchPattern), "%sModules\\*.DLL", achGlobalHomeDirectory);

#ifdef DEBUG_LOGGING
  AddLog("[SSCore_GetListOfModules] : DosFindFirst for");
  AddLog(achSearchPattern);
  AddLog("\n");
#endif

  rcfind = DosFindFirst(achSearchPattern, // File pattern
                        &hdirFindHandle,  // Search handle
                        FILE_NORMAL,      // Search attribute
                        &FindBuffer,      // Result buffer
                        ulResultBufLen,   // Result buffer length
                        &ulFindCount,     // Number of entries to find
                        FIL_STANDARD);    // Return level 1 file info

  if (rcfind == NO_ERROR)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_GetListOfModules] : Process results\n");
#endif

    do {
      sprintf(achFileNamePath, "%sModules\\%s",achGlobalHomeDirectory ,FindBuffer.achName);

      // Try to open this DLL
#ifdef DEBUG_LOGGING
      AddLog("[GetListOfModules] : DosLoadModule() [");AddLog(achFileNamePath);AddLog("]\n");
      achFailure[0] = 0;
#endif

      rc = DosLoadModule(achFailure, sizeof(achFailure),
                         achFileNamePath,
                         &hmodDLL);
      if (rc==NO_ERROR)
      {
        // Cool, DLL could be loaded!
        PFN pfn1, pfn2, pfn3, pfn4;
        pfn_SSModule_SetNLSText pfnSetNLSText;

        // Check if it has all the functions exported!
        rc = DosQueryProcAddr(hmodDLL,
                              0,
                              "SSModule_GetModuleDesc",
                              &pfn1);
        if (rc!=NO_ERROR) pfn1 = NULL;

        rc = DosQueryProcAddr(hmodDLL,
                              0,
                              "SSModule_Configure",
                              &pfn2);
        if (rc!=NO_ERROR) pfn2 = NULL;

        rc = DosQueryProcAddr(hmodDLL,
                              0,
                              "SSModule_StartSaving",
                              &pfn3);
        if (rc!=NO_ERROR) pfn3 = NULL;

        rc = DosQueryProcAddr(hmodDLL,
                              0,
                              "SSModule_StopSaving",
                              &pfn4);
        if (rc!=NO_ERROR) pfn4 = NULL;

        rc = DosQueryProcAddr(hmodDLL,
                              0,
                              "SSModule_SetNLSText",
                              (PFN *) &pfnSetNLSText);
        if (rc!=NO_ERROR) pfnSetNLSText = NULL;


        if ((pfn1) && (pfn2) && (pfn3) && (pfn4))
        {
          // Good, it had all the required functions, so it seems
          // to be a valid screensaver module

          // First send the module all the NLS text, so it can use those
          // informations even for the GetModuleDesc() function!
          // However, we can do this only if the caller is the PID which
          // initialized us, becase only that process knows the current
          // NLS texts.
          if ((pidGlobalInitializerClient==pidThisClient) &&
              (pfnSetNLSText))
          {
            int i;

#ifdef DEBUG_LOGGING
            AddLog("[GetListOfModules] : Setting NLS texts...\n");
#endif
            if (DosRequestMutexSem(hmtxThisUseNLSTextArray, SEM_INDEFINITE_WAIT)==NO_ERROR)
            {
              for (i=0; i<SSCORE_NLSTEXT_MAX+1; i++)
                pfnSetNLSText(i, apchThisNLSText[i]);

              DosReleaseMutexSem(hmtxThisUseNLSTextArray);
            }
          }

          pNewEntry = (SSCoreModuleList_p) malloc(sizeof(SSCoreModuleList_t));
          if (pNewEntry)
          {
            pfn_SSModule_GetModuleDesc fnGetModuleDesc;
            int rc;

            pNewEntry->pNext = *ppModuleList;
            strncpy(pNewEntry->achModuleFileName, achFileNamePath, sizeof(pNewEntry->achModuleFileName));
            // Get module desc!
#ifdef DEBUG_LOGGING
            AddLog("[SSCore_GetListOfModules] : Askig module for its description\n");
#endif

            fnGetModuleDesc = (pfn_SSModule_GetModuleDesc) pfn1;
            rc = fnGetModuleDesc(&(pNewEntry->ModuleDesc), achGlobalHomeDirectory);
            if (rc==SSMODULE_NOERROR)
            {
#ifdef DEBUG_LOGGING
              AddLog("[GetListOfModules] : Module: [");AddLog(pNewEntry->ModuleDesc.achModuleName);AddLog("]\n");
#endif
              *ppModuleList = pNewEntry;
            } else
            {
              // Error querying module desc, so don't link it to list of modules!
              free(pNewEntry);
            }
          }

          // Now send all NULLs to the module so it can free its NLS string before unloading it.
          // The same restrictions apply here as well. (See the setting part some lines above!)
          if ((pidGlobalInitializerClient==pidThisClient) &&
              (pfnSetNLSText))
          {
            int i;

#ifdef DEBUG_LOGGING
            AddLog("[GetListOfModules] : Clearing NLS texts...\n");
#endif
            for (i=0; i<SSCORE_NLSTEXT_MAX+1; i++)
              pfnSetNLSText(i, NULL);
          }
        }

        // We don't use the DLL anymore.
#ifdef DEBUG_LOGGING
        AddLog("[GetListOfModules] : DosFreeModule()\n");
#endif
        DosFreeModule(hmodDLL);
      }
#ifdef DEBUG_LOGGING
      else
      {
        char achTemp[128];
        AddLog("[GetListOfModules] : Error in DosLoadModule()\n");
        sprintf(achTemp, "  rc=0x%x\n", rc);
        AddLog(achTemp);
        AddLog("  Failure: ["); AddLog(achFailure); AddLog("]\n");
      }
#endif

      ulFindCount = 1; // Reset find count

      rcfind = DosFindNext( hdirFindHandle,  // Find handle
                            &FindBuffer,     // Result buffer
                            ulResultBufLen,  // Result buffer length
                            &ulFindCount);   // Number of entries to find
    } while (rcfind == NO_ERROR);

    // Close directory search handle
    rcfind = DosFindClose(hdirFindHandle);
  }

#ifdef DEBUG_LOGGING
  AddLog("[SSCore_GetListOfModules] : Return.\n");
#endif

  return SSCORE_NOERROR;
}

SSCOREDECLSPEC int SSCORECALL SSCore_FreeListOfModules(SSCoreModuleList_p pModuleList)
{
  SSCoreModuleList_p pToDelete;

  while (pModuleList)
  {
    pToDelete = pModuleList;
    pModuleList = pModuleList->pNext;

    free(pToDelete);
  }
  return SSCORE_NOERROR;
}


SSCOREDECLSPEC int SSCORECALL SSCore_GetCurrentSettings(SSCoreSettings_p pCurrentSettings)
{
  if (!bGlobalInitialized)
    return SSCORE_ERROR_NOTINITIALIZED;

  // Check parameters
  if (!pCurrentSettings)
    return SSCORE_ERROR_INVALIDPARAMETER;

  memcpy(pCurrentSettings, &GlobalCurrentSettings, sizeof(GlobalCurrentSettings));
  return SSCORE_NOERROR;
}

#define SSCORE_NUMOFPWDCHARS 62
static char GLOBAL_VAR achGlobalPasswordChars[SSCORE_NUMOFPWDCHARS] =
{ 'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D',
  'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
  'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
  'Y', 'Z' };

SSCOREDECLSPEC int SSCORECALL SSCore_EncryptPassword(char *pchPassword)
{
  int i, iLen;
  char chNextSeed, chCurrSeed;

  if (!pchPassword)
    return SSCORE_ERROR_INVALIDPARAMETER;

  // Ok, do some mangling and encrypting...
  chCurrSeed = 0xD0;
  iLen = strlen(pchPassword);
  for (i=0; i<iLen; i++)
  {
    chNextSeed = pchPassword[i];
    pchPassword[i] =
      achGlobalPasswordChars[((pchPassword[i] ^ chCurrSeed)) % SSCORE_NUMOFPWDCHARS];
    chCurrSeed = chNextSeed;
  }

  return SSCORE_NOERROR;
}

SSCOREDECLSPEC int SSCORECALL SSCore_SetCurrentSettings(SSCoreSettings_p pNewSettings)
{
  HEV hevLocalSentinelEvent;

#ifdef DEBUG_LOGGING
  AddLog("[SSCore_SetCurrentSettings] : Enter\n");
#endif

  if (!bGlobalInitialized)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_SetCurrentSettings] : returning SSCORE_ERROR_NOTINITIALIZED\n");
#endif
    return SSCORE_ERROR_NOTINITIALIZED;
  }

  // Check parameters
  if (!pNewSettings)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_SetCurrentSettings] : Invalid parameter ppointer!\n");
#endif
    return SSCORE_ERROR_INVALIDPARAMETER;
  }
  if (pNewSettings->iActivationTime<1000) // Min 1 sec please... :)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_SetCurrentSettings] : iActivationTime is too small!\n");
#endif
    return SSCORE_ERROR_INVALIDPARAMETER;
  }
  if (pNewSettings->iPasswordDelayTime<60000) // No negative please
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_SetCurrentSettings] : iPasswordDelayTime is too small!\n");
#endif
    return SSCORE_ERROR_INVALIDPARAMETER;
  }
  if (pNewSettings->iDPMSStandbyTime<0) // No negative please
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_SetCurrentSettings] : iDPMSStandbyTime is too small!\n");
#endif
    return SSCORE_ERROR_INVALIDPARAMETER;
  }
  if (pNewSettings->iDPMSSuspendTime<0) // No negative please
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_SetCurrentSettings] : iDPMSSuspendTime is too small!\n");
#endif
    return SSCORE_ERROR_INVALIDPARAMETER;
  }
  if (pNewSettings->iDPMSOffTime<0) // No negative please
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_SetCurrentSettings] : iDPMSOffTime is too small!\n");
#endif
    return SSCORE_ERROR_INVALIDPARAMETER;
  }

  memcpy(&GlobalCurrentSettings, pNewSettings, sizeof(GlobalCurrentSettings));

  internal_SaveSettings(achGlobalHomeDirectory);

#ifdef DEBUG_LOGGING
  AddLog("SSCore_SetCurrentSettings : Settings saved, posting semaphore.\n");
#endif

  // Imitate an event, like user keyboard or something to force
  // restarting the counter!
  if (pidGlobalInitializerClient!=pidThisClient)
  {
    // Calling from the application initialized it!
    // Simply post the event semaphore to note that something happened.
    DosPostEventSem(hevGlobalSentinelEvent);
  } else
  {
    // Calling from a different application
    hevLocalSentinelEvent = NULL;
    // Get the handle to the shared event semaphore,then post it
    // to note that something happened.
    if (DosOpenEventSem(ACTION_EVENT_SHARED_SEM_NAME,
                        &hevLocalSentinelEvent)==NO_ERROR)
    {
      DosPostEventSem(hevLocalSentinelEvent);
      DosCloseEventSem(hevLocalSentinelEvent);
    }
  }
  return SSCORE_NOERROR;
}


SSCOREDECLSPEC int SSCORECALL SSCore_StartSavingNow(int bDontDelayPasswordProtection)
{
  if (!bGlobalInitialized)
    return SSCORE_ERROR_NOTINITIALIZED;

  // Send a command to the worker thread to start the screen saving now, using the current settings!
  WinPostQueueMsg(hmqGlobalWorkerThreadCommandQueue,
                  WORKER_COMMAND_STARTSAVINGNOW,
                  (MPARAM) bDontDelayPasswordProtection, (MPARAM) NULL);

  return SSCORE_NOERROR;
}

SSCOREDECLSPEC int SSCORECALL SSCore_StartModulePreview(char *pchModuleFileName, HWND hwndParent)
{
  if (!bGlobalInitialized)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_StartModulePreview] : bGlobalInitialized is FALSE!\n");
#endif
    return SSCORE_ERROR_NOTINITIALIZED;
  }

  if (!pchModuleFileName)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_StartModulePreview] : pchModuleFileName is NULL!\n");
#endif
    return SSCORE_ERROR_INVALIDPARAMETER;
  }

  if ((!hwndParent) || (hwndParent==HWND_DESKTOP))
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_StartModulePreview] : Parent window would be the desktop!\n");
#endif
    return SSCORE_ERROR_INVALIDPARAMETER;
  }

  // We let it to be called from the process who initialized it, only.
  // This is because we have to make sure that this window is in the same
  // process as the worker thread, as their window procs have to communicate
  // with WinSendMsg()!
  if (pidGlobalInitializerClient!=pidThisClient)
    return SSCORE_ERROR_ACCESSDENIED;

  // Send a command to the worker thread to start the module now!
#ifdef DEBUG_LOGGING
  AddLog("[SSCore_StartModulePreview] : Sending STARTPREVIEW to worker!\n");
  AddLog("  Module name: ["); AddLog(pchModuleFileName); AddLog("]\n");
#endif

  WinPostQueueMsg(hmqGlobalWorkerThreadCommandQueue,
                  WORKER_COMMAND_STARTPREVIEW,
                  (MPARAM) hwndParent,
                  (MPARAM) pchModuleFileName);

  // The notification message will be posted to the given window!
  return SSCORE_NOERROR;
}

SSCOREDECLSPEC int SSCORECALL SSCore_StopModulePreview(HWND hwndParent)
{
  if (!bGlobalInitialized)
    return SSCORE_ERROR_NOTINITIALIZED;

  // We let it to be called from the process who initialized it, only.
  // This is because we have to make sure that this window is in the same
  // process as the worker thread, as their window procs have to communicate
  // with WinSendMsg()!
  if (pidGlobalInitializerClient!=pidThisClient)
    return SSCORE_ERROR_ACCESSDENIED;

  if ((!hwndParent) || (hwndParent==HWND_DESKTOP))
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSCore_StopModulePreview] : Parent window would be the desktop!\n");
#endif
    return SSCORE_ERROR_INVALIDPARAMETER;
  }

  // Send a command to the worker thread to stop the preview
  WinPostQueueMsg(hmqGlobalWorkerThreadCommandQueue,
                  WORKER_COMMAND_STOPPREVIEW,
                  (MPARAM) hwndParent, (MPARAM) NULL);

  // The notification message will be posted to the given window!
  return SSCORE_NOERROR;
}

SSCOREDECLSPEC int SSCORECALL SSCore_ConfigureModule(char *pchModuleFileName, HWND hwndOwner)
{
  HMODULE hmodSaverModule;
  char achFailure[32];
  ULONG ulrc;
  pfn_SSModule_Configure  fnConfigure;
  pfn_SSModule_SetNLSText fnSetNLSText;
  pfn_SSModule_SetCommonSetting fnSetCommonSetting;

  if (pidGlobalInitializerClient!=pidThisClient)
    return SSCORE_ERROR_ACCESSDENIED;

  if (!bGlobalInitialized)
    return SSCORE_ERROR_NOTINITIALIZED;

  // Check parameters!
  if (!pchModuleFileName)
    return SSCORE_ERROR_INVALIDPARAMETER;

  // Load DLL
#ifdef DEBUG_LOGGING
  AddLog("[ConfigureModule] : DosLoadModule()\n");
#endif
  ulrc = DosLoadModule(achFailure, sizeof(achFailure),
                       pchModuleFileName,
                       &hmodSaverModule);
  _control87(0x37f, 0x1fff);
  if (ulrc!=NO_ERROR)
    return SSCORE_ERROR_COULDNOTLOADMODULE;

  ulrc = DosQueryProcAddr(hmodSaverModule,
                          0,
                          "SSModule_SetNLSText",
                          (PFN *) &fnSetNLSText);
  if (ulrc!=NO_ERROR) fnSetNLSText = NULL;

  ulrc = DosQueryProcAddr(hmodSaverModule,
                          0,
                          "SSModule_SetCommonSetting",
                          (PFN *) &fnSetCommonSetting);
  if (ulrc!=NO_ERROR) fnSetCommonSetting = NULL;

  ulrc = DosQueryProcAddr(hmodSaverModule,
                          0,
                          "SSModule_Configure",
                          (PFN *) &fnConfigure);
  if (ulrc!=NO_ERROR)
  {
    // Could not query proc address!
#ifdef DEBUG_LOGGING
    AddLog("[ConfigureModule] : DosFreeModule() (Error branch)\n");
#endif

    DosFreeModule(hmodSaverModule);
    return SSCORE_ERROR_COULDNOTLOADMODULE;
  }

  // If the module supports NLS, then we tell it the current
  // language and other things
  if (fnSetNLSText)
  {
    int i;

#ifdef DEBUG_LOGGING
    AddLog("[ConfigureModule] : Setting NLS texts...\n");
#endif
    if (DosRequestMutexSem(hmtxThisUseNLSTextArray, SEM_INDEFINITE_WAIT)==NO_ERROR)
    {
      for (i=0; i<SSCORE_NLSTEXT_MAX+1; i++)
        fnSetNLSText(i, apchThisNLSText[i]);

#ifdef DEBUG_LOGGING
      for (i=0; i<SSCORE_NLSTEXT_MAX+1; i++)
      {
        if (apchThisNLSText[i])
        {
          AddLog("[ConfigureModule] : NLS text : ");
          AddLog(apchThisNLSText[i]);
          AddLog("\n");
        }
      }
#endif

      DosReleaseMutexSem(hmtxThisUseNLSTextArray);
    }
  }

  // If the module supports Common Module Settings, then we tell it the
  // common module settings!
  if (fnSetCommonSetting)
  {
#ifdef DEBUG_LOGGING
    AddLog("[ConfigureModule] : Setting Common Module Settings...\n");
#endif
    internal_TellCommonModuleSettings(FALSE, fnSetCommonSetting);
  }

  // Do the configuration!
  fnConfigure(hwndOwner, achGlobalHomeDirectory);

  // If the module support NLS, then tell it to free
  // its NLS text array!
  if (fnSetNLSText)
  {
    int i;

#ifdef DEBUG_LOGGING
    AddLog("[ConfigureModule] : Clearing NLS texts...\n");
#endif
    for (i=0; i<SSCORE_NLSTEXT_MAX+1; i++)
      fnSetNLSText(i, NULL);
  }

  // All done, free the module!
#ifdef DEBUG_LOGGING
  AddLog("[ConfigureModule] : DosFreeModule()\n");
#endif
  DosFreeModule(hmodSaverModule);

  return SSCORE_NOERROR;
}


SSCOREDECLSPEC int SSCORECALL SSCore_TempDisable(void)
{
#ifdef DEBUG_LOGGING
  AddLog("[TempDisable] : Enter\n");
#endif

  if (!bGlobalInitialized)
  {
#ifdef DEBUG_LOGGING
    AddLog("[TempDisable] : Error, SSaver not initialized!\n");
#endif
    return SSCORE_ERROR_NOTINITIALIZED;
  }

#ifdef DEBUG_LOGGING
  {
    char achTemp[256];
    sprintf(achTemp, "  Before increasing: PerProcess: %d  Global: %d\n",
            iThisTempDisableCounter,
            iGlobalTempDisableCounter);
    AddLog("[TempDisable] : Increasing counter.\n");
    AddLog(achTemp);
  }
#endif


  iThisTempDisableCounter++;
  iGlobalTempDisableCounter++;


#ifdef DEBUG_LOGGING
  AddLog("[TempDisable] : Posting WORKER_COMMAND_STOPSAVINGNOW\n");
#endif

  // Send a command to the worker thread to stop the screen saving now,
  // if it's running!
  WinPostQueueMsg(hmqGlobalWorkerThreadCommandQueue,
                  WORKER_COMMAND_STOPSAVINGNOW,
                  (MPARAM) NULL, (MPARAM) NULL);

#ifdef DEBUG_LOGGING
  AddLog("[TempDisable] : Leave\n");
#endif

  return SSCORE_NOERROR;
}

SSCOREDECLSPEC int SSCORECALL SSCore_TempEnable(void)
{
#ifdef DEBUG_LOGGING
  AddLog("[TempEnable] : Enter\n");
#endif

  if (!bGlobalInitialized)
  {
#ifdef DEBUG_LOGGING
    AddLog("[TempEnable] : Error, SSaver not initialized!\n");
#endif
    return SSCORE_ERROR_NOTINITIALIZED;
  }

  if (iThisTempDisableCounter<=0)
  {
#ifdef DEBUG_LOGGING
    AddLog("[TempEnable] : Error, SSaver not temp-disabled!\n");
#endif
    return SSCORE_ERROR_NOTDISABLED;
  }

#ifdef DEBUG_LOGGING
  {
    char achTemp[256];
    sprintf(achTemp, "  Before decreasing: PerProcess: %d  Global: %d\n",
            iThisTempDisableCounter,
            iGlobalTempDisableCounter);
    AddLog("[TempDisable] : Decreasing counter.\n");
    AddLog(achTemp);
  }
#endif

  iThisTempDisableCounter--;
  iGlobalTempDisableCounter--;

#ifdef DEBUG_LOGGING
  AddLog("[TempEnable] : Leave\n");
#endif

  return SSCORE_NOERROR;
}

SSCOREDECLSPEC int SSCORECALL SSCore_GetCurrentState(void)
{
  if (!bGlobalInitialized)
    return SSCORE_STATE_NORMAL;

  return iGlobalCurrentState;
}

SSCOREDECLSPEC int SSCORECALL SSCore_WaitForStateChange(int iTimeout, int *piNewState)
{
  APIRET ulRc;
  HEV hevLocalStateChangeEvent;
  ULONG ulPostCount;

  // Checks
  if (!bGlobalInitialized)
    return SSCORE_ERROR_NOTINITIALIZED;

  // Wait for state change event!
  if (pidThisClient == pidGlobalInitializerClient)
  {
    // Calling from the application initialized it!
    // Simply wait on the event semaphore!
    DosResetEventSem(hevGlobalStateChangeEvent, &ulPostCount);
    ulRc = DosWaitEventSem(hevGlobalStateChangeEvent, iTimeout);
  } else
  {
    // Calling from a different application
    hevLocalStateChangeEvent = NULL;
    // Get the handle to the shared event semaphore, then wait on it!
    if (DosOpenEventSem(STATECHANGE_EVENT_SHARED_SEM_NAME,
                        &hevLocalStateChangeEvent)==NO_ERROR)
    {
      DosResetEventSem(hevLocalStateChangeEvent, &ulPostCount);
      ulRc = DosWaitEventSem(hevLocalStateChangeEvent, iTimeout);
      DosCloseEventSem(hevLocalStateChangeEvent);
    } else
      ulRc = ERROR_INVALID_HANDLE;
  }

  // Return error code if timeout happened!
  if (ulRc == ERROR_TIMEOUT)
    return SSCORE_ERROR_TIMEOUT;

  if (ulRc != NO_ERROR)
    return SSCORE_ERROR_INTERNALERROR;

  // If there was no timeout, then all is right!
  // Return the new state code, if the second parameter is not NULL!
  if (piNewState)
    *piNewState = iGlobalCurrentState;

  return SSCORE_NOERROR;
}


SSCOREDECLSPEC int SSCORECALL SSCore_SetNLSText(int iNLSTextID, char *pchNLSText)
{
  if (!bGlobalInitialized)
    return SSCORE_ERROR_NOTINITIALIZED;

  // We let it to be called from the process who initialized it, only.
  // This is because we have to make sure that the memory allocated here
  // will be visible from the WorkerThread, so it has to be the
  // same process!

  if (pidGlobalInitializerClient!=pidThisClient)
    return SSCORE_ERROR_ACCESSDENIED;

  if ((iNLSTextID<0) || (iNLSTextID>SSCORE_NLSTEXT_MAX))
    return SSCORE_ERROR_INVALIDPARAMETER;

  if (DosRequestMutexSem(hmtxThisUseNLSTextArray, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
    // Free old text
    if (apchThisNLSText[iNLSTextID])
      free(apchThisNLSText[iNLSTextID]);

    if (pchNLSText)
    {
      // Store new text
      apchThisNLSText[iNLSTextID] = (char *) malloc(strlen(pchNLSText)+1);
      if (apchThisNLSText[iNLSTextID])
        strcpy(apchThisNLSText[iNLSTextID], pchNLSText);
    } else
    {
      // Empty stuff!
      apchThisNLSText[iNLSTextID] = NULL;
    }

    DosReleaseMutexSem(hmtxThisUseNLSTextArray);
    return SSCORE_NOERROR;
  } else
    return SSCORE_ERROR_INTERNALERROR;
}

SSCOREDECLSPEC int SSCORECALL SSCore_GetInactivityTime(unsigned long *pulMouseInactivityTime,
                                                       unsigned long *pulKeyboardInactivityTime)
{
  APIRET rc;
  unsigned long ulTemp;

  if (!bGlobalInitialized)
    return SSCORE_ERROR_NOTINITIALIZED;

  // Check parameters
  if ((!pulMouseInactivityTime) || (!pulKeyboardInactivityTime))
    return SSCORE_ERROR_INVALIDPARAMETER;

  // Query current timestamp
  rc = DosQuerySysInfo(QSV_MS_COUNT,
                       QSV_MS_COUNT,
                       &ulTemp,
                       sizeof(ulTemp));
  if (rc!=NO_ERROR)
    return SSCORE_ERROR_INTERNALERROR;

  // Calculate time difference
  *pulMouseInactivityTime = (ulTemp - ulGlobalLastMouseEventTimestamp);
  *pulKeyboardInactivityTime = (ulTemp - ulGlobalLastKeyboardEventTimestamp);

  return SSCORE_NOERROR;
}

static void internal_InitializeSecurity2()
{
  APIRET rc;
  char   achFailure[256];

  // Try to load USERCTL.DLL!
  rc = DosLoadModule(achFailure,               /* Failure information buffer */
                     sizeof(achFailure),       /* Size of buffer             */
                     "USERCTL.DLL",            /* Module to load             */
                     &hmodThisUSERCTL);        /* Module handle returned     */
  _control87(0x37f, 0x1fff);
  if (rc != NO_ERROR)
  {
    // No userctl.dll, no security/2 installed.
    hmodThisUSERCTL = NULLHANDLE;
    pfnThisDosUserRegister = NULL;
    pfnThisDosUserQueryCurrent = NULL;
    return;
  }

  // USERCTL.DLL loaded. Let's see if it has the required functions!

  if (DosQueryProcAddr(hmodThisUSERCTL, NULL, "DosUserRegister", (PFN *) &pfnThisDosUserRegister)!=NO_ERROR)
  {
    // Not a good userctl.dll, no security/2 installed.
    DosFreeModule(hmodThisUSERCTL);
    hmodThisUSERCTL = NULLHANDLE;
    pfnThisDosUserRegister = NULL;
    pfnThisDosUserQueryCurrent = NULL;
    return;
  }

  if (DosQueryProcAddr(hmodThisUSERCTL, NULL, "DosUserQueryCurrent", (PFN *) &pfnThisDosUserQueryCurrent)!=NO_ERROR)
  {
    // Not a good userctl.dll, no security/2 installed.
    DosFreeModule(hmodThisUSERCTL);
    hmodThisUSERCTL = NULLHANDLE;
    pfnThisDosUserRegister = NULL;
    pfnThisDosUserQueryCurrent = NULL;
    return;
  }

  // Final check for function pointers:
  if ((!pfnThisDosUserRegister) || (!pfnThisDosUserQueryCurrent))
  {
    // Not a good userctl.dll, no security/2 installed.
    DosFreeModule(hmodThisUSERCTL);
    hmodThisUSERCTL = NULLHANDLE;
    pfnThisDosUserRegister = NULL;
    pfnThisDosUserQueryCurrent = NULL;
    return;
  }
  // Okay, DLL loaded, function pointers queried!
}

static void internal_UninitializeSecurity2()
{
  if (hmodThisUSERCTL)
  {
    // Security/2 was loaded, unload it!
    DosFreeModule(hmodThisUSERCTL);
    hmodThisUSERCTL = NULLHANDLE;
    pfnThisDosUserRegister = NULL;
    pfnThisDosUserQueryCurrent = NULL;
  }
}

static void internal_ConnectClient(HMODULE hmod)
{
  int i;

  // Initialize local (per-process) variables!
  pidThisClient = internal_GetCurrentPID();
  iThisTempDisableCounter = 0;

  for (i=0; i<SSCORE_NLSTEXT_MAX+1; i++)
    apchThisNLSText[i] = NULL;

  hmtxThisUseNLSTextArray = NULLHANDLE;
  DosCreateMutexSem(NULL, &hmtxThisUseNLSTextArray, 0, FALSE);

  // Load Security/2 functions, if available!
  internal_InitializeSecurity2();
}

static void internal_DisconnectClient(HMODULE hmod)
{
  int i;

  // Uninitialize local (per-process) variables!
  if (bGlobalInitialized)
  {
    while(iThisTempDisableCounter>0)
      SSCore_TempEnable();
  }

  for (i=0; i<SSCORE_NLSTEXT_MAX+1; i++)
    if (apchThisNLSText[i])
    {
      free(apchThisNLSText[i]);
      apchThisNLSText[i] = NULL;
    }

  DosCloseMutexSem(hmtxThisUseNLSTextArray); hmtxThisUseNLSTextArray = NULLHANDLE;

  // Unload Security/2 functions
  internal_UninitializeSecurity2();
}

static void internal_FirstStartup(HMODULE hmod)
{
  bGlobalInitialized = FALSE;
  pidGlobalInitializerClient = NULLHANDLE;

  iGlobalTempDisableCounter = 0;

  hmodGlobalDLLHandle = hmod;
}

static void internal_LastShutdown()
{
  bGlobalInitialized = FALSE;
  pidGlobalInitializerClient = NULLHANDLE;

  iGlobalTempDisableCounter = 0;
}

//---------------------------------------------------------------------
// LibMain
//
// This gets called at DLL initialization and termination.
//---------------------------------------------------------------------
unsigned _System LibMain(unsigned hmod, unsigned termination)
{
  if (termination)
  {
    // Cleanup!

#ifdef DEBUG_LOGGING
    {
      char achTemp[256];
      sprintf(achTemp, "  PID: %02x\n", pidThisClient);
      AddLog("[LibMain] : TERMINATE DLL\n");
      AddLog(achTemp);
    }
#endif

    // If a client disconnects which initialized the screen saver and forgot to
    // unintialize, then uninitialize!
    if ((pidGlobalInitializerClient) && (pidGlobalInitializerClient==pidThisClient))
      SSCore_Uninitialize();

    // Disconnect the client
    internal_DisconnectClient(hmod);

    // Usage counter
    iDLLUsageCounter--;
    if (iDLLUsageCounter==0)
      internal_LastShutdown();
  } else
  {
    // Startup!
    if (iDLLUsageCounter==0)
      internal_FirstStartup(hmod);

    // Initialize local variables (per-instance)
    internal_ConnectClient(hmod);

#ifdef DEBUG_LOGGING
    {
      char achTemp[256];
      sprintf(achTemp, "  PID: %02x\n", pidThisClient);
      AddLog("[LibMain] : INITIALIZE DLL\n");
      AddLog(achTemp);
    }
#endif

    // Increase dll usage counter (global)
    iDLLUsageCounter++;
  }
  return 1;
}

