/*
 * Screen Saver - Lockup Desktop replacement for OS/2 and eComStation systems
 * Copyright (C) 2004-2006 Doodle
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

#ifndef __SSMODULE_H__
#define __SSMODULE_H__

#define SSMODULECALL __syscall
#define SSMODULEDECLSPEC __declspec(dllexport)

// ----------------- Type and error definitions -----------------------

#define SSMODULE_MAXNAMELEN  1024
#define SSMODULE_MAXDESCLEN  4096
typedef struct SSModuleDesc_s
{
  int iVersionMajor;
  int iVersionMinor;

  char achModuleName[SSMODULE_MAXNAMELEN];
  char achModuleDesc[SSMODULE_MAXDESCLEN];

  int bConfigurable;
  int bSupportsPasswordProtection;
} SSModuleDesc_t, *SSModuleDesc_p;

#define SSMODULE_NOERROR                     0
#define SSMODULE_ERROR_INVALIDPARAMETER      1
#define SSMODULE_ERROR_NOTINITIALIZED        2
#define SSMODULE_ERROR_ALREADYINITIALIZED    3
#define SSMODULE_ERROR_NOTSUPPORTED          4
#define SSMODULE_ERROR_NOTRUNNING            5
#define SSMODULE_ERROR_ALREADYRUNNING        6
#define SSMODULE_ERROR_USERPRESSEDCANCEL     7
#define SSMODULE_ERROR_INTERNALERROR       255

// The following lines define the NLS texts known by SSCore. These are the strings
// which will be passed to the saver modules if they support it, so they can show
// messages in the currently set language.
// These defines are identical to the SSCORE_NLSTEXT_* defines in the SSCore.h file!
#define SSMODULE_NLSTEXT_LANGUAGECODE              0
#define SSMODULE_NLSTEXT_PWDPROT_ASK_TITLE         1
#define SSMODULE_NLSTEXT_PWDPROT_ASK_TEXT          2
#define SSMODULE_NLSTEXT_PWDPROT_ASK_OKBUTTON      3
#define SSMODULE_NLSTEXT_PWDPROT_ASK_CANCELBUTTON  4
#define SSMODULE_NLSTEXT_PWDPROT_WRONG_TITLE       5
#define SSMODULE_NLSTEXT_PWDPROT_WRONG_TEXT        6
#define SSMODULE_NLSTEXT_PWDPROT_WRONG_OKBUTTON    7
#define SSMODULE_NLSTEXT_FONTTOUSE                 8
#define SSMODULE_NLSTEXT_MAX                       8

// The following defines are the common settings told to the module after the NLS text
// via SSModule_SetCommonSetting() API, if that API is exported by the module.

// * First Key Event Goes To Password Window:
//   The pSettingValue has to be interpreted as a boolean value.
//   NULL value means that first key event(s) should not go to password window,
//   non-NULL value means that first key event(s) should go to password window
#define SSMODULE_COMMONSETTING_FIRSTKEYGOESTOPWDWINDOW   0
// No more common settings are defined yet.

// ----------------- DLL Function Declarations -----------------------

// Mandatory functions to implement:

SSMODULEDECLSPEC int SSMODULECALL SSModule_GetModuleDesc(SSModuleDesc_p pModuleDesc, char *pchHomeDirectory);

SSMODULEDECLSPEC int SSMODULECALL SSModule_Configure(HWND hwndOwner, char *pchHomeDirectory);

SSMODULEDECLSPEC int SSMODULECALL SSModule_StartSaving(HWND hwndParent, char *pchHomeDirectory, int bPreviewMode);
SSMODULEDECLSPEC int SSMODULECALL SSModule_StopSaving(void);

SSMODULEDECLSPEC int SSMODULECALL SSModule_AskUserForPassword(int iPwdBuffSize, char *pchPwdBuff);
SSMODULEDECLSPEC int SSMODULECALL SSModule_ShowWrongPasswordNotification(void);

// Optional functions to implement (to support DPMS saving)

SSMODULEDECLSPEC int SSMODULECALL SSModule_PauseSaving(void);
SSMODULEDECLSPEC int SSMODULECALL SSModule_ResumeSaving(void);

// Even more optional functions to implement
// For NLS:
SSMODULEDECLSPEC int SSMODULECALL SSModule_SetNLSText(int iNLSTextID, char *pchNLSText);
// For extra common settings and module behaviour:
// See SSMODULE_COMMONSETTING_* defines!
SSMODULEDECLSPEC int SSMODULECALL SSModule_SetCommonSetting(int iSettingID, void *pSettingValue);

// --------------------------------------------------------------------

// All these again as function pointer types

typedef int SSMODULECALL (*pfn_SSModule_GetModuleDesc)(SSModuleDesc_p pModuleDesc, char *pchHomeDirectory);

typedef int SSMODULECALL (*pfn_SSModule_Configure)(HWND hwndOwner, char *pchHomeDirectory);

typedef int SSMODULECALL (*pfn_SSModule_StartSaving)(HWND hwndParent, char *pchHomeDirectory, int bPreviewMode);
typedef int SSMODULECALL (*pfn_SSModule_StopSaving)(void);

typedef int SSMODULECALL (*pfn_SSModule_AskUserForPassword)(int iPwdBuffSize, char *pchPwdBuff);
typedef int SSMODULECALL (*pfn_SSModule_ShowWrongPasswordNotification)(void);

typedef int SSMODULECALL (*pfn_SSModule_PauseSaving)(void);
typedef int SSMODULECALL (*pfn_SSModule_ResumeSaving)(void);

typedef int SSMODULECALL (*pfn_SSModule_SetNLSText)(int iNLSTextID, char *pchNLSText);
typedef int SSMODULECALL (*pfn_SSModule_SetCommonSetting)(int iSettingID, void *pSettingValue);

#endif
