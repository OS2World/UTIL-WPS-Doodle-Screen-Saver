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

#ifndef __SSMODULENPIPE_H__
#define __SSMODULENPIPE_H__

#include "SSModule.h"

// --------------------------------------------------------------------

// Structures and definitions for process-separation of modules
// (for named-pipe communication)

#define SSMODULE_NPIPE_SERVER_PATH      "\\PIPE\\SSAVER\\MODULE"
#define SSMODULE_NPIPE_RECOMMENDEDBUFFERSIZE         65536

#define SSMODULE_NPIPE_COMMANDCODE_PING_REQ              0
typedef struct SSModule_NPipe_Ping_Req_s
{
  unsigned short usCommandCode;
} SSModule_NPipe_Ping_Req_t, *SSModule_NPipe_Ping_Req_p;

#define SSMODULE_NPIPE_COMMANDCODE_PING_RESP             1
typedef struct SSModule_NPipe_PING_Resp_s
{
  unsigned short usCommandCode;
  int            iResult;
} SSModule_NPipe_Ping_Resp_t, *SSModule_NPipe_Ping_Resp_p;

#define SSMODULE_NPIPE_COMMANDCODE_GETMODULEDESC_REQ     2
typedef struct SSModule_NPipe_GetModuleDesc_Req_s
{
  unsigned short usCommandCode;
  char           achHomeDirectory[CCHMAXPATHCOMP];
} SSModule_NPipe_GetModuleDesc_Req_t, *SSModule_NPipe_GetModuleDesc_Req_p;

#define SSMODULE_NPIPE_COMMANDCODE_GETMODULEDESC_RESP    3
typedef struct SSModule_NPipe_GetModuleDesc_Resp_s
{
  unsigned short usCommandCode;
  int            iResult;
  SSModuleDesc_t mdModuleDesc;
} SSModule_NPipe_GetModuleDesc_Resp_t, *SSModule_NPipe_GetModuleDesc_Resp_p;

#define SSMODULE_NPIPE_COMMANDCODE_CONFIGURE_REQ         4
typedef struct SSModule_NPipe_Configure_Req_s
{
  unsigned short usCommandCode;
  HWND           hwndOwner;
  char           achHomeDirectory[CCHMAXPATHCOMP];
} SSModule_NPipe_Configure_Req_t, *SSModule_NPipe_Configure_Req_p;

#define SSMODULE_NPIPE_COMMANDCODE_CONFIGURE_RESP        5
typedef struct SSModule_NPipe_Configure_Resp_s
{
  unsigned short usCommandCode;
  int            iResult;
} SSModule_NPipe_Configure_Resp_t, *SSModule_NPipe_Configure_Resp_p;

#define SSMODULE_NPIPE_COMMANDCODE_STARTSAVING_REQ       6
typedef struct SSModule_NPipe_StartSaving_Req_s
{
  unsigned short usCommandCode;
  HWND           hwndParent;
  char           achHomeDirectory[CCHMAXPATHCOMP];
  int            bPreviewMode;
} SSModule_NPipe_StartSaving_Req_t, *SSModule_NPipe_StartSaving_Req_p;

#define SSMODULE_NPIPE_COMMANDCODE_STARTSAVING_RESP      7
typedef struct SSModule_NPipe_StartSaving_Resp_s
{
  unsigned short usCommandCode;
  int            iResult;
} SSModule_NPipe_StartSaving_Resp_t, *SSModule_NPipe_StartSaving_Resp_p;

#define SSMODULE_NPIPE_COMMANDCODE_STOPSAVING_REQ        8
typedef struct SSModule_NPipe_StopSaving_Req_s
{
  unsigned short usCommandCode;
} SSModule_NPipe_StopSaving_Req_t, *SSModule_NPipe_StopSaving_Req_p;

#define SSMODULE_NPIPE_COMMANDCODE_STOPSAVING_RESP       9
typedef struct SSModule_NPipe_StopSaving_Resp_s
{
  unsigned short usCommandCode;
  int            iResult;
} SSModule_NPipe_StopSaving_Resp_t, *SSModule_NPipe_StopSaving_Resp_p;

#define SSMODULE_NPIPE_COMMANDCODE_ASKUSERFORPASSWORD_REQ    10
typedef struct SSModule_NPipe_AskUserForPassword_Req_s
{
  unsigned short usCommandCode;
} SSModule_NPipe_AskUserForPassword_Req_t, *SSModule_NPipe_AskUserForPassword_Req_p;

#define SSMODULE_NPIPE_COMMANDCODE_ASKUSERFORPASSWORD_RESP   11
typedef struct SSModule_NPipe_AskUserForPassword_Resp_s
{
  unsigned short usCommandCode;
  int            iResult;
  char           achPassword[1024];
} SSModule_NPipe_AskUserForPassword_Resp_t, *SSModule_NPipe_AskUserForPassword_Resp_p;

#define SSMODULE_NPIPE_COMMANDCODE_SHOWWRONGPASSWORDNOTIFICATION_REQ     12
typedef struct SSModule_NPipe_ShowWrongPasswordNotification_Req_s
{
  unsigned short usCommandCode;
} SSModule_NPipe_ShowWrongPasswordNotification_Req_t, *SSModule_NPipe_ShowWrongPasswordNotification_Req_p;

#define SSMODULE_NPIPE_COMMANDCODE_SHOWWRONGPASSWORDNOTIFICATION_RESP    13
typedef struct SSModule_NPipe_ShowWrongPasswordNotification_Resp_s
{
  unsigned short usCommandCode;
  int            iResult;
} SSModule_NPipe_ShowWrongPasswordNotification_Resp_t, *SSModule_NPipe_ShowWrongPasswordNotification_Resp_p;

#define SSMODULE_NPIPE_COMMANDCODE_PAUSESAVING_REQ                       14
typedef struct SSModule_NPipe_PauseSaving_Req_s
{
  unsigned short usCommandCode;
} SSModule_NPipe_PauseSaving_Req_t, *SSModule_NPipe_PauseSaving_Req_p;

#define SSMODULE_NPIPE_COMMANDCODE_PAUSESAVING_RESP                      15
typedef struct SSModule_NPipe_PauseSaving_Resp_s
{
  unsigned short usCommandCode;
  int            iResult;
} SSModule_NPipe_PauseSaving_Resp_t, *SSModule_NPipe_PauseSaving_Resp_p;

#define SSMODULE_NPIPE_COMMANDCODE_RESUMESAVING_REQ                      16
typedef struct SSModule_NPipe_ResumeSaving_Req_s
{
  unsigned short usCommandCode;
} SSModule_NPipe_ResumeSaving_Req_t, *SSModule_NPipe_ResumeSaving_Req_p;

#define SSMODULE_NPIPE_COMMANDCODE_RESUMESAVING_RESP                     17
typedef struct SSModule_NPipe_ResumeSaving_Resp_s
{
  unsigned short usCommandCode;
  int            iResult;
} SSModule_NPipe_ResumeSaving_Resp_t, *SSModule_NPipe_ResumeSaving_Resp_p;

#define SSMODULE_NPIPE_COMMANDCODE_SETNLSTEXT_REQ                        18
typedef struct SSModule_NPipe_SetNLSText_Req_s
{
  unsigned short usCommandCode;
  int            iNLSTextID;
  int            bIsNULL;
  char           achNLSText[32768];
} SSModule_NPipe_SetNLSText_Req_t, *SSModule_NPipe_SetNLSText_Req_p;

#define SSMODULE_NPIPE_COMMANDCODE_SETNLSTEXT_RESP                       19
typedef struct SSModule_NPipe_SetNLSText_Resp_s
{
  unsigned short usCommandCode;
  int            iResult;
} SSModule_NPipe_SetNLSText_Resp_t, *SSModule_NPipe_SetNLSText_Resp_p;

#define SSMODULE_NPIPE_COMMANDCODE_SETCOMMONSETTING0_REQ                 20
typedef struct SSModule_NPipe_SetCommonSetting0_Req_s
{
  unsigned short usCommandCode;
  unsigned int   uiFirstKeyGoesToPwdWindow;
} SSModule_NPipe_SetCommonSetting0_Req_t, *SSModule_NPipe_SetCommonSetting0_Req_p;

#define SSMODULE_NPIPE_COMMANDCODE_SETCOMMONSETTING0_RESP                21
typedef struct SSModule_NPipe_SetCommonSetting0_Resp_s
{
  unsigned short usCommandCode;
  int            iResult;
} SSModule_NPipe_SetCommonSetting0_Resp_t, *SSModule_NPipe_SetCommonSetting0_Resp_p;

#endif
