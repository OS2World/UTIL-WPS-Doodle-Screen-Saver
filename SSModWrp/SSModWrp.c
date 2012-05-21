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
#define INCL_DOSNMPIPES
#define INCL_DOSFILEMGR
#define INCL_DOSSEMAPHORES
#define INCL_DOSMISC
#define INCL_ERRORS
#include <os2.h>
#include <process.h>

#include "SSModuleNPipe.h"

static HMODULE hmodSaverDLL;
static pfn_SSModule_GetModuleDesc                 pfnGetModuleDesc;
static pfn_SSModule_Configure                     pfnConfigure;
static pfn_SSModule_StartSaving                   pfnStartSaving;
static pfn_SSModule_StopSaving                    pfnStopSaving;
static pfn_SSModule_AskUserForPassword            pfnAskPwd;
static pfn_SSModule_ShowWrongPasswordNotification pfnShowWrongPwd;
static pfn_SSModule_PauseSaving                   pfnPauseSaving;
static pfn_SSModule_ResumeSaving                  pfnResumeSaving;
static pfn_SSModule_SetNLSText                    pfnSetNLSText;
static pfn_SSModule_SetCommonSetting              pfnSetCommonSetting;
static unsigned int bShutdownRequest;
static unsigned char achPipeCommunicationBuffer[SSMODULE_NPIPE_RECOMMENDEDBUFFERSIZE];

static int LoadModule(char *pchFileName)
{
  char achFailure[1024];
  ULONG ulrc;

  ulrc = DosLoadModule(achFailure, sizeof(achFailure),
                       pchFileName,
                       &hmodSaverDLL);
  if (ulrc!=NO_ERROR)
  {
    // Error, could not load DLL
    return 0;
  } else
  {
    // Cool, DLL could be loaded

    // Query mandatory function pointers
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

    // Some optional functions:
    ulrc = DosQueryProcAddr(hmodSaverDLL,
                            0,
                            "SSModule_PauseSaving",
                            (PFN *) &pfnPauseSaving);
    if (ulrc!=NO_ERROR) pfnPauseSaving = NULL;

    ulrc = DosQueryProcAddr(hmodSaverDLL,
                            0,
                            "SSModule_ResumeSaving",
                            (PFN *) &pfnResumeSaving);
    if (ulrc!=NO_ERROR) pfnResumeSaving = NULL;

    ulrc = DosQueryProcAddr(hmodSaverDLL,
                            0,
                            "SSModule_SetNLSText",
                            (PFN *) &pfnSetNLSText);
    if (ulrc!=NO_ERROR) pfnSetNLSText = NULL;

    ulrc = DosQueryProcAddr(hmodSaverDLL,
                            0,
                            "SSModule_SetCommonSetting",
                            (PFN *) &pfnSetCommonSetting);
    if (ulrc!=NO_ERROR) pfnSetCommonSetting = NULL;



    // Check if all the mandatory functions are present
    if ((!pfnStartSaving) || (!pfnStopSaving) ||
        (!pfnAskPwd) || (!pfnShowWrongPwd) ||
        (!pfnGetModuleDesc) || (!pfnConfigure))
    {
      DosFreeModule(hmodSaverDLL);
      hmodSaverDLL = NULL;
      return 0;
    }
  }
  return 1;
}

static void UnloadModule()
{
  if (hmodSaverDLL)
  {
    DosFreeModule(hmodSaverDLL); hmodSaverDLL = NULL;
  }
}

static void RespondToIncomingPipeMessage(HFILE hPipe)
{
  unsigned short *pusCommandCode = (unsigned short *) achPipeCommunicationBuffer;
  ULONG ulWritten;

  switch (*pusCommandCode)
  {
    case SSMODULE_NPIPE_COMMANDCODE_GETMODULEDESC_REQ:
      {
        SSModule_NPipe_GetModuleDesc_Req_p pRequest =
          (SSModule_NPipe_GetModuleDesc_Req_p) achPipeCommunicationBuffer;
        SSModule_NPipe_GetModuleDesc_Resp_t response;

        response.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_GETMODULEDESC_RESP;
        if (pfnGetModuleDesc)
          response.iResult = pfnGetModuleDesc(&(response.mdModuleDesc), pRequest->achHomeDirectory);
        else
          response.iResult = SSMODULE_ERROR_NOTSUPPORTED;

        DosWrite(hPipe, &response, sizeof(response), &ulWritten);
      }
      break;

    case SSMODULE_NPIPE_COMMANDCODE_CONFIGURE_REQ:
      {
        SSModule_NPipe_Configure_Req_p pRequest =
          (SSModule_NPipe_Configure_Req_p) achPipeCommunicationBuffer;
        SSModule_NPipe_Configure_Resp_t response;

        response.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_CONFIGURE_RESP;
        if (pfnConfigure)
          response.iResult = pfnConfigure(pRequest->hwndOwner, pRequest->achHomeDirectory);
        else
          response.iResult = SSMODULE_ERROR_NOTSUPPORTED;

        DosWrite(hPipe, &response, sizeof(response), &ulWritten);
      }
      break;

    case SSMODULE_NPIPE_COMMANDCODE_STARTSAVING_REQ:
      {
        SSModule_NPipe_StartSaving_Req_p pRequest =
          (SSModule_NPipe_StartSaving_Req_p) achPipeCommunicationBuffer;
        SSModule_NPipe_StartSaving_Resp_t response;

        response.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_STARTSAVING_RESP;
        if (pfnStartSaving)
          response.iResult = pfnStartSaving(pRequest->hwndParent, pRequest->achHomeDirectory, pRequest->bPreviewMode);
        else
          response.iResult = SSMODULE_ERROR_NOTSUPPORTED;

        DosWrite(hPipe, &response, sizeof(response), &ulWritten);
      }
      break;

    case SSMODULE_NPIPE_COMMANDCODE_STOPSAVING_REQ:
      {
        SSModule_NPipe_StopSaving_Resp_t response;

        response.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_STOPSAVING_RESP;
        if (pfnStopSaving)
          response.iResult = pfnStopSaving();
        else
          response.iResult = SSMODULE_ERROR_NOTSUPPORTED;

        DosWrite(hPipe, &response, sizeof(response), &ulWritten);
      }
      break;

    case SSMODULE_NPIPE_COMMANDCODE_ASKUSERFORPASSWORD_REQ:
      {
        SSModule_NPipe_AskUserForPassword_Resp_t response;

        response.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_ASKUSERFORPASSWORD_RESP;
        if (pfnAskPwd)
          response.iResult = pfnAskPwd(sizeof(response.achPassword), response.achPassword);
        else
          response.iResult = SSMODULE_ERROR_NOTSUPPORTED;

        DosWrite(hPipe, &response, sizeof(response), &ulWritten);
      }
      break;

    case SSMODULE_NPIPE_COMMANDCODE_SHOWWRONGPASSWORDNOTIFICATION_REQ:
      {
        SSModule_NPipe_ShowWrongPasswordNotification_Resp_t response;

        response.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_SHOWWRONGPASSWORDNOTIFICATION_RESP;
        if (pfnShowWrongPwd)
          response.iResult = pfnShowWrongPwd();
        else
          response.iResult = SSMODULE_ERROR_NOTSUPPORTED;

        DosWrite(hPipe, &response, sizeof(response), &ulWritten);
      }
      break;

    case SSMODULE_NPIPE_COMMANDCODE_PAUSESAVING_REQ:
      {
        SSModule_NPipe_PauseSaving_Resp_t response;

        response.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_PAUSESAVING_RESP;
        if (pfnPauseSaving)
          response.iResult = pfnPauseSaving();
        else
          response.iResult = SSMODULE_ERROR_NOTSUPPORTED;

        DosWrite(hPipe, &response, sizeof(response), &ulWritten);
      }
      break;

    case SSMODULE_NPIPE_COMMANDCODE_RESUMESAVING_REQ:
      {
        SSModule_NPipe_ResumeSaving_Resp_t response;

        response.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_RESUMESAVING_RESP;
        if (pfnResumeSaving)
          response.iResult = pfnResumeSaving();
        else
          response.iResult = SSMODULE_ERROR_NOTSUPPORTED;

        DosWrite(hPipe, &response, sizeof(response), &ulWritten);
      }
      break;

    case SSMODULE_NPIPE_COMMANDCODE_SETNLSTEXT_REQ:
      {
        SSModule_NPipe_SetNLSText_Req_p pRequest =
          (SSModule_NPipe_SetNLSText_Req_p) achPipeCommunicationBuffer;
        SSModule_NPipe_SetNLSText_Resp_t response;

        response.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_SETNLSTEXT_RESP;
        if (pfnSetNLSText)
        {
          if (pRequest->bIsNULL)
            response.iResult = pfnSetNLSText(pRequest->iNLSTextID, NULL);
          else
            response.iResult = pfnSetNLSText(pRequest->iNLSTextID, pRequest->achNLSText);

        } else
          response.iResult = SSMODULE_ERROR_NOTSUPPORTED;

        DosWrite(hPipe, &response, sizeof(response), &ulWritten);
      }
      break;

    case SSMODULE_NPIPE_COMMANDCODE_SETCOMMONSETTING0_REQ:
      {
        SSModule_NPipe_SetCommonSetting0_Req_p pRequest =
          (SSModule_NPipe_SetCommonSetting0_Req_p) achPipeCommunicationBuffer;
        SSModule_NPipe_SetCommonSetting0_Resp_t response;

        response.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_SETCOMMONSETTING0_RESP;
        if (pfnSetCommonSetting)
          response.iResult = pfnSetCommonSetting(0, (void *) (pRequest->uiFirstKeyGoesToPwdWindow));
        else
          response.iResult = SSMODULE_ERROR_NOTSUPPORTED;

        DosWrite(hPipe, &response, sizeof(response), &ulWritten);
      }
      break;

    default:
      {
        // Ping, or Unknown message
        SSModule_NPipe_Ping_Resp_t response;
        response.usCommandCode = SSMODULE_NPIPE_COMMANDCODE_PING_RESP;
        response.iResult = SSMODULE_NOERROR;
        DosWrite(hPipe, &response, sizeof(response), &ulWritten);
      }
      break;
  }
  // Flush output, to make sure that it gets to the other side
  DosResetBuffer(hPipe);
}

static void StartNPipeServer()
{
  HFILE hPipe;
  ULONG ulActualRead;
  APIRET rc;

  rc = DosCreateNPipe(SSMODULE_NPIPE_SERVER_PATH,
                      &hPipe,
                      NP_ACCESS_DUPLEX,
                      NP_WAIT |
                      NP_TYPE_MESSAGE |
                      NP_READMODE_MESSAGE |
                      0x01,
                      SSMODULE_NPIPE_RECOMMENDEDBUFFERSIZE,
                      SSMODULE_NPIPE_RECOMMENDEDBUFFERSIZE,
                      100); // msec
  if (rc!=NO_ERROR)
  {
    // Could not create pipe!
    return;
  }

  bShutdownRequest = 0;
  while (!bShutdownRequest)
  {
    if (DosConnectNPipe(hPipe)==NO_ERROR)
    {
      // We have a client connected to the pipe

      // Process messages
      memset(achPipeCommunicationBuffer, 0, sizeof(achPipeCommunicationBuffer));
      rc = DosRead(hPipe, achPipeCommunicationBuffer, sizeof(achPipeCommunicationBuffer), &ulActualRead);
      if ((rc==NO_ERROR) && (ulActualRead>=2))
      {
        // Try to process the incoming message, and send a response
        RespondToIncomingPipeMessage(hPipe);
      }

      // Clean up
      DosDisConnectNPipe(hPipe);
    } else
    {
      // Error!
      bShutdownRequest = 1;
    }
  }


  // Clean up
  DosClose(hPipe);
}


int main(int argc, char *argv[])
{
  HAB hab;
  HMQ hmq;

  // Check parameters
  if (argc<2)
  {
    // Make sure it won't run if started independently
    printf("This program is part of the screen saver, and not meant to be run separately!\n");
    return EXIT_FAILURE;
  }

  // Load the DLL
  if (!LoadModule(argv[1]))
  {
    // Could not load the DLL, don't start!
    return EXIT_FAILURE;
  }

  // Prepare for PM
  hab = WinInitialize(0);
  hmq = WinCreateMsgQueue(hab, 0);
  // Make sure this message queue will not stop system shutdown!
  WinCancelShutdown(hmq, TRUE);

  // Start pipe server
  StartNPipeServer();

  // Clean up PM
  WinDestroyMsgQueue(hmq);
  WinTerminate(hab);

  UnloadModule();
  return EXIT_SUCCESS;
}
