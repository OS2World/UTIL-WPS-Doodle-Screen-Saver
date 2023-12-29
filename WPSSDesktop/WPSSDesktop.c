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

#define WPSSDesktop_Class_Source
#define M_WPSSDesktop_Class_Source

#define INCL_WIN
#define INCL_DOS
#define INCL_GPIPRIMITIVES
#define INCL_ERRORS
#include <os2.h>

#define _ULS_CALLCONV_
#define CALLCONV _System
#include <unidef.h>                    // Unicode API
#include <uconv.h>                     // Unicode API (codepage conversion)

#include "WPSSDesktop.ih"              /* implementation header */

#include <string.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>

#include "SSCore.h"
#include "WPSSDesktop-Resources.h"
#include "WPSSDesktop-HelpIDs.h"

#include "MSGX.h" // NLS support

// Some min/max settings
#define MIN_SAVER_TIMEOUT   1
#define MAX_SAVER_TIMEOUT   180

#define MIN_DPMS_TIMEOUT    0
#define MAX_DPMS_TIMEOUT    180

// Preview area
#define PREVIEW_CLASS       "ScreenSaverPreviewAreaWindowClass"

// Some private messages
#define WM_LANGUAGECHANGED (WM_USER+0xd00d)

// Typedefs for window-private data of settings pages

typedef struct Page1UserData_s
{
  WPSSDesktop *Desktop;

  int  bPageSetUp; // TRUE if the page has been set up correctly

  // Startup configuration (used for Undo button)
  int  bUndoEnabled;
  int  iUndoActivationTime;
  int  bUndoWakeByKeyboardOnly;

  int  bUndoUseDPMSStandbyState;
  int  iUndoDPMSStandbyStateTimeout;
  int  bUndoUseDPMSSuspendState;
  int  iUndoDPMSSuspendStateTimeout;
  int  bUndoUseDPMSOffState;
  int  iUndoDPMSOffStateTimeout;

} Page1UserData_t, *Page1UserData_p;

typedef struct Page2UserData_s
{
  WPSSDesktop *Desktop;

  int  bPageSetUp; // TRUE if the page has been set up correctly

  // Startup configuration (used for Undo button)
  int  bUndoPasswordProtected;
  char achUndoEncryptedPassword[SSCORE_MAXPASSWORDLEN];
  int  bUndoDelayedPasswordProtection;
  int  iUndoDelayedPasswordProtectionTime;
  int  bUndoStartAtStartup;

} Page2UserData_t, *Page2UserData_p;

typedef struct Page3UserData_s
{
  WPSSDesktop *Desktop;

  int  bPageSetUp; // TRUE if the page has been set up correctly

  ULONG ulMaxModules;
  PSSMODULEDESC pModuleDescArray;

  int  iPreviewMsgCounter; // Number of Start/Stop preview commands pending
  int  bPreviewRunning;    // Is preview running?

  // Startup configuration (used for Undo button)
  char achUndoSaverDLLFileName[CCHMAXPATHCOMP];
} Page3UserData_t, *Page3UserData_p;

// We'll have three dialog procs
MRESULT EXPENTRY fnwpScreenSaverSettingsPage1(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2); // General & DPMS settings
MRESULT EXPENTRY fnwpScreenSaverSettingsPage2(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2); // Password settings
MRESULT EXPENTRY fnwpScreenSaverSettingsPage3(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2); // Saver modules

static void EnableDisablePage1Controls(HWND hwnd);
static void EnableDisablePage2Controls(HWND hwnd);

//#define DEBUG_LOGGING
#ifdef DEBUG_LOGGING
void AddLog(char *pchMsg)
{
  FILE *hFile;

  hFile = fopen("c:\\WPSSDesktop.log", "at+");
  if (hFile)
  {
    fprintf(hFile, "%s", pchMsg);
    fclose(hFile);
  }
}
#else
#define AddLog(p)
#endif

/*
 *   Non-Method function prototypes
 */


/***************** GLOBAL/STATIC (NON-INSTANCE) DATA SECTION ******************
*****                                                                     *****
*****    This data shouldn't be changed by instance methods or it will    *****
*****    effect all instances!  Any variables that are specific (unique   *****
*****    values) for each instance of this object should be declared as   *****
*****  instance data or dynamically allocated and stored as window data.  *****
*****                                                                     *****
*****      This global data should be declared as class instance data     *****
*****    if it will change after initialization.  In this case, it will   *****
*****                  be accessed through class methods.                 *****
*****                                                                     *****
******************************************************************************/


HMODULE hmodThisDLL;
int     bWindowClassRegistered = 0;
int     bFirstOpen = TRUE;
char    achHomeDirectory[1024];
char    achLocalHelpFileName[1024];
char    achFontToUse[128];

HWND    hwndSettingsPage1 = NULLHANDLE;
HWND    hwndSettingsPage2 = NULLHANDLE;
HWND    hwndSettingsPage3 = NULLHANDLE;
HWND    hwndSettingsNotebook = NULLHANDLE;
ULONG   ulSettingsPage1ID, ulSettingsPage2ID, ulSettingsPage3ID;
PFNWP   pfnOldNotebookWindowProc;

static void internal_CloseNLSFile(FILE *hfNLSFile)
{
  if (hfNLSFile)
    fclose(hfNLSFile);
}

static int internal_FileExists(char *pchFileName)
{
  HDIR hdirFindHandle = HDIR_CREATE;
  FILEFINDBUF3 FindBuffer;
  ULONG ulResultBufLen = sizeof(FILEFINDBUF3);
  ULONG ulFindCount = 1;
  APIRET rc;

//  AddLog("[internal_FileExists] : [");
//  AddLog(pchFileName);
//  AddLog("]\n");

  rc = DosFindFirst(pchFileName,
		    &hdirFindHandle,
                    FILE_NORMAL,
		    &FindBuffer,
		    ulResultBufLen,
		    &ulFindCount,
		    FIL_STANDARD);
  DosFindClose(hdirFindHandle);
//  if (rc==NO_ERROR)
//    AddLog("[internal_FileExists] : rc is NO_ERROR, returning TRUE!\n");
//  else
//    AddLog("[internal_FileExists] : rc is some ERROR, returning FALSE!\n");

  return (rc==NO_ERROR);
}

static int  internal_GetRealLocaleNameFromLanguageCode(char *pchLanguageCode,
                                                       char *pchRealLocaleName,
                                                       int iRealLocaleNameBufSize)
{
  int rc;
  UconvObject ucoTemp;
  LocaleObject loTemp;
  UniChar *pucLanguageCode;
  char    *pchFrom, *pchTo;
  size_t   iFromCount;
  UniChar *pucTo, *pucFrom;
  size_t   iToCount;
  size_t   iNonIdentical;
  int iLanguageCodeLen;

  // The default result is empty string:
  pchRealLocaleName[0] = 0; // Empty string

  AddLog("[internal_GetRealLocaleName] : In: [");
  AddLog(pchLanguageCode);
  AddLog("]\n");

  // Now, we have to create an unicode string from the locale name
  // we have here. For this, we allocate memory for the UCS string!
  iLanguageCodeLen = strlen(pchLanguageCode);
  pucLanguageCode = (UniChar *) malloc(sizeof(UniChar) * iLanguageCodeLen*4+4);
  if (!pucLanguageCode)
  {
    AddLog("[internal_GetRealLocaleName] : Not enough memory!\n");
    // Not enough memory!
    return 0;
  }

  // Create unicode convert object
  rc = UniCreateUconvObject(L"", &ucoTemp);
  if (rc!=ULS_SUCCESS)
  {
    AddLog("[internal_GetRealLocaleName] : Could not create convert object!\n");
    // Could not create convert object!
    return 0;
  }

  // Convert language code string to unicode string
  pchFrom = pchLanguageCode;
  iFromCount = strlen(pchLanguageCode)+1;
  pucTo = pucLanguageCode;
  iToCount = iLanguageCodeLen*4;

  if (iFromCount>1)
  {
    // It's not an empty string, so convert it!
    rc = UniUconvToUcs(ucoTemp,
                       (void**)&pchFrom,
                       &iFromCount,
                       &pucTo,
                       &iToCount,
                       &iNonIdentical);

    if (rc!=ULS_SUCCESS)
    {
      // Could not convert language code to UCS string!
      AddLog("[internal_GetRealLocaleName] : Could not convert language code to UCS string!\n");
      UniFreeUconvObject(ucoTemp);
      free(pucLanguageCode);
      return 0;
    }
  } else
  {
    // It's an empty string, no need to convert it, simply make an
    // emty unicode string as a result.
    pucTo[0] = 0;
  }

  // Okay, we have the language code in UCS (or empty UCS string)!
  // Try to open the locale object of it!
  rc = UniCreateLocaleObject(UNI_UCS_STRING_POINTER,
                             pucLanguageCode,
                             &loTemp);
  if (rc!=ULS_SUCCESS)
  {
    // Could not create locale object of this name!
    AddLog("[internal_GetRealLocaleName] : Could not create locale object of this name!\n");
    UniFreeUconvObject(ucoTemp);
    free(pucLanguageCode);
    return 0;
  }

  // Query the real locale name! (like en_US or nl_NL)
  pucTo = NULL;
  rc = UniQueryLocaleItem(loTemp,
                          LOCI_sName,
                          &pucFrom);
  if ((rc!=ULS_SUCCESS) || (!pucFrom))
  {
    // Could not get real locale name!
    AddLog("[internal_GetRealLocaleName] : Could not get real locale name!\n");
    UniFreeLocaleObject(loTemp);
    UniFreeUconvObject(ucoTemp);
    free(pucLanguageCode);
    return 0;
  }

  // Convert it from UCS to current codepage
  // pucFrom is set up there
  iFromCount = UniStrlen(pucFrom)+1;
  pchTo = pchRealLocaleName;
  iToCount = iRealLocaleNameBufSize;
  rc = UniUconvFromUcs(ucoTemp,
                       &pucFrom,
                       &iFromCount,
                       (void**)&pchTo,
                       &iToCount,
                       &iNonIdentical);

  // Free real locale name got from Uni API
  UniFreeMem(pucFrom);

  // Free other resources
  UniFreeLocaleObject(loTemp);
  UniFreeUconvObject(ucoTemp);
  free(pucLanguageCode);

  AddLog("[internal_GetRealLocaleName] : Out: [");
  AddLog(pchRealLocaleName);
  AddLog("]\n");

  return (rc == ULS_SUCCESS);
}

static void internal_SetNLSHelpFilename(WPSSDesktop *somSelf)
{
  WPSSDesktopData *somThis = WPSSDesktopGetData(somSelf);
  char achRealLocaleName[128];
  char *pchLang;
  ULONG rc;
  int i;

  AddLog("[internal_SetNLSHelpFilename] : Enter!\n");

  // Try to get a help file name
  rc = 0; // Not found the help file yet.

  // Get the real locale name from the language code
  internal_GetRealLocaleNameFromLanguageCode(_achLanguage, achRealLocaleName, sizeof(achRealLocaleName));

  // Get language code
  if (achRealLocaleName[0]!=0)
  {
    // Aaah, there is a language set!
    // Try that one!

    AddLog("[internal_SetNLSHelpFilename] : Trying the language which is set by the user!\n");

    pchLang = (char *) malloc(1024);
    if (!pchLang)
    {
      // Not enough memory, so we won't do the long
      // method of searching for every combination of
      // the language string, but only for the string itself!

      // Assemble NLS file name
      sprintf(achLocalHelpFileName, "%sLang\\ss_%s.HLP", achHomeDirectory, achRealLocaleName);
      AddLog("[internal_SetNLSHelpFilename] : Trying to use help file: [");
      AddLog(achLocalHelpFileName);
      AddLog("] by achLocalLanguage setting (in Not enough memory branch)!\n");
      rc = internal_FileExists(achLocalHelpFileName);
    } else
    {
      // Fine, we can start trying a lot of filenames!
      sprintf(pchLang, "%s", achRealLocaleName);

      do {
	// Assemble NLS file name
	sprintf(achLocalHelpFileName, "%sLang\\ss_%s.HLP", achHomeDirectory, pchLang);
	AddLog("[internal_SetNLSHelpFilename] : Trying to use help file: [");
	AddLog(achLocalHelpFileName);
	AddLog("] by achLocalLanguage setting! (in Loop)\n");

	rc = internal_FileExists(achLocalHelpFileName);

	// Make pchLang shorter, until the next underscore!
	i = strlen(pchLang)-1;
	while ((i>=0) && (pchLang[i]!='_'))
	  i--;
	if (i<0) i = 0;
	pchLang[i] = 0;

      } while ((pchLang[0]) && (!rc));

      free(pchLang); pchLang = NULL;
    }
  }

  if (!rc)
  {
    AddLog("[internal_SetNLSHelpFilename] : No language set, or could not open the required one -> LANG!\n");

    // No language set, or could not open it, so use LANG variable!
    rc = DosScanEnv("LANG", &pchLang);
    if ((rc!=NO_ERROR) || (!pchLang))
    {
      AddLog("[internal_SetNLSHelpFilename] : Could not query LANG env. var., will use 'en'\n");
      pchLang = "en"; // Default
    }

    // Assemble NLS file name
    sprintf(achLocalHelpFileName, "%sLang\\ss_%s.HLP", achHomeDirectory, pchLang);
    AddLog("[internal_SetNLSHelpFilename] : Trying to use help file: [");
    AddLog(achLocalHelpFileName);
    AddLog("] by LANG env. variable!\n");

    rc = internal_FileExists(achLocalHelpFileName);
    if (!rc)
    {
      AddLog("[internal_SetNLSHelpFilename] : Could not open it, trying the default!\n");
      pchLang = "en"; // Default

      // Assemble NLS file name
      sprintf(achLocalHelpFileName, "%sLang\\ss_%s.HLP", achHomeDirectory, pchLang);
      rc = internal_FileExists(achLocalHelpFileName);
    }
  }

  if (!rc)
  {
    AddLog("[internal_SetNLSHelpFilename] : Result : No help file!\n");
    strcpy(achLocalHelpFileName, "");
  }
#ifdef DEBUG_LOGGING
  else
  {
    AddLog("[internal_SetNLSHelpFilename] : Result : Help file is [");
    AddLog(achLocalHelpFileName);
    AddLog("]\n");
  }
#endif
}


static FILE *internal_OpenNLSFile(WPSSDesktop *somSelf)
{
  WPSSDesktopData *somThis = WPSSDesktopGetData(somSelf);
  char achRealLocaleName[128];
  char *pchFileName, *pchLang;
  ULONG rc;
  int i;
  FILE *hfNLSFile;

  hfNLSFile = NULLHANDLE;

  pchFileName = (char *) malloc(1024);
  if (pchFileName)
  {
    // Get the real locale name from the language code
    internal_GetRealLocaleNameFromLanguageCode(_achLanguage, achRealLocaleName, sizeof(achRealLocaleName));

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
	sprintf(pchFileName, "%sLang\\ss_%s.msg", achHomeDirectory, achRealLocaleName);
	AddLog("[internal_OpenNLSFile] : Trying to open NLS file: [");
	AddLog(pchFileName);
	AddLog("] by achLocalLanguage setting (in Not enough memory branch)!\n");

	hfNLSFile = fopenMessageFile(pchFileName);
      } else
      {
	// Fine, we can start trying a lot of filenames!
	sprintf(pchLang, "%s", achRealLocaleName);

	do {
	  // Assemble NLS file name
	  sprintf(pchFileName, "%sLang\\ss_%s.msg", achHomeDirectory, pchLang);
	  AddLog("[internal_OpenNLSFile] : Trying to open NLS file: [");
	  AddLog(pchFileName);
	  AddLog("] by achLocalLanguage setting! (in Loop)\n");

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

    if (!hfNLSFile)
    {
      // No language set, or could not open it, so use LANG variable!
      strcpy(achRealLocaleName, ""); // Make local language setting empty!

      rc = DosScanEnv("LANG", &pchLang);
      if ((rc!=NO_ERROR) || (!pchLang))
      {
        AddLog("[internal_OpenNLSFile] : Could not query LANG env. var., will use 'en'\n");
        pchLang = "en"; // Default
      }

      // Assemble NLS file name
      sprintf(pchFileName, "%sLang\\ss_%s.msg", achHomeDirectory, pchLang);
      AddLog("[internal_OpenNLSFile] : Trying to open NLS file: [");
      AddLog(pchFileName);
      AddLog("] by LANG env. variable!\n");

      hfNLSFile = fopenMessageFile(pchFileName);

      if (!hfNLSFile)
      {
	AddLog("[internal_OpenNLSFile] : Could not open it, trying the default!\n");
	pchLang = "en"; // Default

	// Assemble NLS file name
	sprintf(pchFileName, "%sLang\\ss_%s.msg", achHomeDirectory, pchLang);
	hfNLSFile = fopenMessageFile(pchFileName);
      }
    }
    free(pchFileName);
  }

  AddLog("[internal_OpenNLSFile] : Done.\n");

  return hfNLSFile;
}

static void internal_SendNLSTextToSSCore(WPSSDesktop *somSelf, FILE *hfNLSFile)
{
  WPSSDesktopData *somThis = WPSSDesktopGetData(somSelf);
  char achRealLocaleName[128];
  char *pchTemp;
  int i;

  // Get the real locale name from the language code
  internal_GetRealLocaleNameFromLanguageCode(_achLanguage, achRealLocaleName, sizeof(achRealLocaleName));

  // Send NLS strings to SSCore!
  pchTemp = (char *) malloc(1024);
  if ((hfNLSFile) && (pchTemp))
  {
    AddLog("[internal_SendNLSTextToSSCore] : Memory alloced.\n");

    for (i=0; i<SSCORE_NLSTEXT_MAX+1; i++)
    {
      switch (i)
      {
	case SSCORE_NLSTEXT_LANGUAGECODE:
	  SSCore_SetNLSText(i, achRealLocaleName);
	  break;
	case SSCORE_NLSTEXT_PWDPROT_ASK_TITLE:
          if (sprintmsg(pchTemp, hfNLSFile, "MOD_0000"))
	    SSCore_SetNLSText(i, pchTemp);
	  else
            SSCore_SetNLSText(i, NULL);
	  break;
	case SSCORE_NLSTEXT_PWDPROT_ASK_TEXT:
	  if (sprintmsg(pchTemp, hfNLSFile, "MOD_0001"))
	    SSCore_SetNLSText(i, pchTemp);
          else
            SSCore_SetNLSText(i, NULL);
	  break;
	case SSCORE_NLSTEXT_PWDPROT_ASK_OKBUTTON:
	  if (sprintmsg(pchTemp, hfNLSFile, "MOD_0002"))
	    SSCore_SetNLSText(i, pchTemp);
          else
            SSCore_SetNLSText(i, NULL);
	  break;
	case SSCORE_NLSTEXT_PWDPROT_ASK_CANCELBUTTON:
	  if (sprintmsg(pchTemp, hfNLSFile, "MOD_0003"))
	    SSCore_SetNLSText(i, pchTemp);
          else
            SSCore_SetNLSText(i, NULL);
	  break;
	case SSCORE_NLSTEXT_PWDPROT_WRONG_TITLE:
	  if (sprintmsg(pchTemp, hfNLSFile, "MOD_0004"))
	    SSCore_SetNLSText(i, pchTemp);
          else
            SSCore_SetNLSText(i, NULL);
	  break;
	case SSCORE_NLSTEXT_PWDPROT_WRONG_TEXT:
	  if (sprintmsg(pchTemp, hfNLSFile, "MOD_0005"))
	    SSCore_SetNLSText(i, pchTemp);
          else
            SSCore_SetNLSText(i, NULL);
	  break;
	case SSCORE_NLSTEXT_PWDPROT_WRONG_OKBUTTON:
	  if (sprintmsg(pchTemp, hfNLSFile, "MOD_0006"))
	    SSCore_SetNLSText(i, pchTemp);
          else
            SSCore_SetNLSText(i, NULL);
	  break;

	case SSCORE_NLSTEXT_FONTTOUSE:
    SSCore_SetNLSText(i, achFontToUse);
    break;

	default:
	  SSCore_SetNLSText(i, NULL);
          break;
      }
    }

    AddLog("[internal_SendNLSTextToSSCore] : For loop done.\n");

  } else
  {
    AddLog("[internal_SendNLSTextToSSCore] : No NLS file or not enough memory!\n");

    // Not enough memory, or no NLS file!
    for (i=0; i<SSCORE_NLSTEXT_MAX+1; i++)
    {
      if (i==SSCORE_NLSTEXT_LANGUAGECODE)
	SSCore_SetNLSText(i, achRealLocaleName);
      else
        SSCore_SetNLSText(i, NULL);
    }
  }
}

static void internal_wpDisplayHelp(WPSSDesktop *somSelf, int iHelpID)
{
  internal_SetNLSHelpFilename(somSelf);

  // If there is a help file, then:
  if (achLocalHelpFileName[0])
  {
    AddLog("[internal_wpDisplayHelp] : Displaying help from [");
    AddLog(achLocalHelpFileName);
    AddLog("]\n");
    _wpDisplayHelp(somSelf, iHelpID, achLocalHelpFileName);
  }
#ifdef DEBUG_LOGGING
  else
    AddLog("[internal_wpDisplayHelp] : Fuck, no help file!\n");
#endif
}

static void internal_NotifyWindowsAboutLanguageChange(FILE *hfNLSFile)
{
  int bNotebookOpened;

  bNotebookOpened = FALSE;

  // Settings windows
  if (hwndSettingsPage1)
  {
    WinSendMsg(hwndSettingsPage1, WM_LANGUAGECHANGED, (MPARAM) 0, (MPARAM) 0);
    bNotebookOpened = TRUE;
  }

  if (hwndSettingsPage2)
  {
    WinSendMsg(hwndSettingsPage2, WM_LANGUAGECHANGED, (MPARAM) 0, (MPARAM) 0);
    bNotebookOpened = TRUE;
  }

  if (hwndSettingsPage3)
  {
    WinSendMsg(hwndSettingsPage3, WM_LANGUAGECHANGED, (MPARAM) 0, (MPARAM) 0);
    bNotebookOpened = TRUE;
  }

  // Notebook controls
  if ((bNotebookOpened) && (hwndSettingsNotebook))
  {
    BOOKPAGEINFO  bpi;
    char         *pchTemp, *pchText;

    // TAB text!
    if (hfNLSFile)
    {
      pchText = pchTemp = (char *) malloc(1024);
    } else
      pchTemp = NULL;
    if (pchTemp)
    {
      if (!sprintmsg(pchText, hfNLSFile, "COM_0000"))
        pchText = (PSZ) "S~creen Saver";
    } else
    {
      pchText = (PSZ) "S~creen Saver";
    }

    WinSendMsg(hwndSettingsNotebook,
	       BKM_SETTABTEXT,
	       (MPARAM) ulSettingsPage1ID,
	       (MPARAM) pchText);

    if (pchTemp)
      free(pchTemp);


    // Page 1:
    memset(&bpi, 0, sizeof(bpi));
    bpi.cb = sizeof(bpi);
    bpi.fl = BFA_MINORTABTEXT;

    if (hfNLSFile)
    {
      bpi.pszMinorTab = pchTemp = (char *) malloc(1024);
    } else
      pchTemp = NULL;
    if (pchTemp)
    {
      if (!sprintmsg(bpi.pszMinorTab, hfNLSFile, "COM_0001"))
        bpi.pszMinorTab = (PSZ) "General Screen Saver settings";
    } else
    {
      bpi.pszMinorTab = (PSZ) "General Screen Saver settings";
    }

    bpi.cbMinorTab = strlen(bpi.pszMinorTab);
    WinSendMsg(hwndSettingsNotebook,
	       BKM_SETPAGEINFO,
	       (MPARAM) ulSettingsPage1ID,
	       (MPARAM) &bpi);

    if (pchTemp)
      free(pchTemp);

    // Page 2:
    memset(&bpi, 0, sizeof(bpi));
    bpi.cb = sizeof(bpi);
    bpi.fl = BFA_MINORTABTEXT;

    if (hfNLSFile)
    {
      bpi.pszMinorTab = pchTemp = (char *) malloc(1024);
    } else
      pchTemp = NULL;
    if (pchTemp)
    {
      if (!sprintmsg(bpi.pszMinorTab, hfNLSFile, "COM_0002"))
        bpi.pszMinorTab = (PSZ) "Password protection";
    } else
    {
      bpi.pszMinorTab = (PSZ) "Password protection";
    }

    bpi.cbMinorTab = strlen(bpi.pszMinorTab);
    WinSendMsg(hwndSettingsNotebook,
	       BKM_SETPAGEINFO,
	       (MPARAM) ulSettingsPage2ID,
	       (MPARAM) &bpi);

    if (pchTemp)
      free(pchTemp);

    // Page 3:
    memset(&bpi, 0, sizeof(bpi));
    bpi.cb = sizeof(bpi);
    bpi.fl = BFA_MINORTABTEXT;

    if (hfNLSFile)
    {
      bpi.pszMinorTab = pchTemp = (char *) malloc(1024);
    } else
      pchTemp = NULL;
    if (pchTemp)
    {
      if (!sprintmsg(bpi.pszMinorTab, hfNLSFile, "COM_0003"))
        bpi.pszMinorTab = (PSZ) "Screen Saver modules";
    } else
    {
      bpi.pszMinorTab = (PSZ) "Screen Saver modules";
    }

    bpi.cbMinorTab = strlen(bpi.pszMinorTab);
    WinSendMsg(hwndSettingsNotebook,
	       BKM_SETPAGEINFO,
	       (MPARAM) ulSettingsPage3ID,
	       (MPARAM) &bpi);

    if (pchTemp)
      free(pchTemp);

    // Redraw changes
    WinSendMsg(hwndSettingsNotebook,
	       BKM_INVALIDATETABS,
	       (MPARAM) 0,
               (MPARAM) 0);
    WinInvalidateRect(hwndSettingsNotebook, NULL, TRUE);
  }
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
  } else
  {
    // Startup!
    // Save DLL Module handle
    hmodThisDLL = hmod;
  }
  return 1;
}

/*************************  INSTANCE METHODS SECTION  *************************
*****                                                                     *****
*****              Do not put any code in this section unless             *****
*****                   it is an object INSTANCE method                   *****
*****                                                                     *****
******************************************************************************/
#undef SOM_CurrentClass
#define SOM_CurrentClass SOMInstance

SOM_Scope BOOL SOMLINK wpssdesktop_wpssAddScreenSaver3Page(WPSSDesktop *somSelf,
                                                           HWND hwndNotebook)
{
  ULONG         ulPageID;
  PAGEINFO      pi;
  BOOKPAGEINFO  bpi;
  FILE         *hfNLSFile;
  char          szText[128];

  AddLog("[wpssdesktop_wpssAddScreenSaver3Page] : Enter\n");

  // Don't be confused by the fact that this function is AddScreenSaver3Page,
  // but we add DLG_SCREENSAVERSETTINGSPAGE1 with function fnwpScrenSaverSettingsPage1!
  // It's because in my taste, page-1 is the main page, Page-2 is the second (minor) page,
  // but in WPS, we add pages from right to left, so minor pages first, then at the end
  // the major page. So, I had to exchange the two functions.

  // The AddScreenSaver3Page, which is called after the AddScreenSaver2Page, will
  // add the major tab. But I treat the major tab to be page-1. Ok? Ok. :)

  hfNLSFile = internal_OpenNLSFile(somSelf);

  memset(&pi, 0, sizeof(PAGEINFO));

  pi.cb = sizeof(pi);
  pi.hwndPage = NULLHANDLE;
  pi.usPageStyleFlags = BKA_MAJOR | BKA_STATUSTEXTON;
  pi.usPageInsertFlags = BKA_FIRST;
  pi.usSettingsFlags = SETTINGS_PAGE_NUMBERS;
  pi.pfnwp = fnwpScreenSaverSettingsPage1;
  pi.resid = hmodThisDLL;
  pi.dlgid = DLG_SCREENSAVERSETTINGSPAGE1;

  if (!hfNLSFile || !sprintmsg(szText, hfNLSFile, "COM_0000"))
    pi.pszName = (PSZ) "S~creen Saver";
  else
    pi.pszName = szText;

  pi.pCreateParams = somSelf;
  pi.pszHelpLibraryName = achLocalHelpFileName;
  pi.idDefaultHelpPanel = HELPID_PAGE1_GENERALSETTINGS;

  ulPageID = _wpInsertSettingsPage(somSelf, hwndNotebook, &pi);

  if (ulPageID==0)
  {
    AddLog("[wpssdesktop_wpssAddScreenSaver3Page] : Could not insert page!\n");
    // Could not insert new page!
    return SETTINGS_PAGE_REMOVED;
  }

  // Hack the 'page info' to show the minor tab title in the notebook context menu
  // This is taken from XWorkplace!
  memset(&bpi, 0, sizeof(bpi));
  bpi.cb = sizeof(bpi);
  bpi.fl = BFA_MINORTABTEXT;

  if (!hfNLSFile || !sprintmsg(szText, hfNLSFile, "COM_0001"))
    bpi.pszMinorTab = (PSZ) "General Screen Saver settings";
  else
    bpi.pszMinorTab = szText;

  bpi.cbMinorTab = strlen(bpi.pszMinorTab);
  WinSendMsg(hwndNotebook, BKM_SETPAGEINFO, (MPARAM) ulPageID, (MPARAM) &bpi);

  // Save notebook handle so the text can be changed lated on the fly!
  hwndSettingsNotebook = hwndNotebook;
  ulSettingsPage1ID = ulPageID;

  internal_CloseNLSFile(hfNLSFile);

  AddLog("[wpssdesktop_wpssAddScreenSaver3Page] : Leave\n");

  // Ok, new page inserted!
  return ulPageID;
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpssAddScreenSaver2Page(WPSSDesktop *somSelf,
                                                           HWND hwndNotebook)
{
  ULONG         ulPageID;
  PAGEINFO      pi;
  BOOKPAGEINFO  bpi;
  FILE         *hfNLSFile;
  char          szText[128];

  AddLog("[wpssdesktop_wpssAddScreenSaver2Page] : Enter\n");

  // See notes in wpssAddScreenSaver3Page about which function adds which page!

  memset(&pi, 0, sizeof(PAGEINFO));

  pi.cb = sizeof(pi);
  pi.hwndPage = NULLHANDLE;
  pi.usPageStyleFlags = BKA_MINOR | BKA_STATUSTEXTON;
  pi.usPageInsertFlags = BKA_FIRST;
  pi.usSettingsFlags = SETTINGS_PAGE_NUMBERS;
  pi.pfnwp = fnwpScreenSaverSettingsPage2;
  pi.resid = hmodThisDLL;
  pi.dlgid = DLG_SCREENSAVERSETTINGSPAGE2;
  pi.pszName = NULL;
  pi.pCreateParams = somSelf;

  pi.pszHelpLibraryName = achLocalHelpFileName;
  pi.idDefaultHelpPanel = HELPID_PAGE2_PASSWORDPROTECTION;

  ulPageID = _wpInsertSettingsPage(somSelf, hwndNotebook, &pi);
  if (ulPageID==0)
  {
    AddLog("[wpssdesktop_wpssAddScreenSaver2Page] : Could not insert page!\n");
    // Could not insert new page!
    return SETTINGS_PAGE_REMOVED;
  }

  // Hack the 'page info' to show the minor tab title in the notebook context menu
  // This is taken from XWorkplace!
  memset(&bpi, 0, sizeof(bpi));
  bpi.cb = sizeof(bpi);
  bpi.fl = BFA_MINORTABTEXT;

  if (!(hfNLSFile = internal_OpenNLSFile(somSelf)) ||
      !sprintmsg(szText, hfNLSFile, "COM_0002"))
    bpi.pszMinorTab = (PSZ) "Password protection";
  else
    bpi.pszMinorTab = szText;

  bpi.cbMinorTab = strlen(bpi.pszMinorTab);
  WinSendMsg(hwndNotebook, BKM_SETPAGEINFO, (MPARAM) ulPageID, (MPARAM) &bpi);

  // Save pageID for later use!
  hwndSettingsNotebook = hwndNotebook;
  ulSettingsPage2ID = ulPageID;

  internal_CloseNLSFile(hfNLSFile);

  AddLog("[wpssdesktop_wpssAddScreenSaver2Page] : Leave\n");

  // Ok, new page inserted!
  return ulPageID;
}


SOM_Scope BOOL SOMLINK wpssdesktop_wpssAddScreenSaver1Page(WPSSDesktop *somSelf,
                                                           HWND hwndNotebook)
{
  ULONG         ulPageID;
  PAGEINFO      pi;
  BOOKPAGEINFO  bpi;
  FILE         *hfNLSFile;
  char          szText[128];

  AddLog("[wpssdesktop_wpssAddScreenSaver1Page] : Enter\n");

  // See notes in wpssAddScreenSaver3Page about which function adds which page!

  memset(&pi, 0, sizeof(PAGEINFO));

  pi.cb = sizeof(pi);
  pi.hwndPage = NULLHANDLE;
  pi.usPageStyleFlags = BKA_MINOR | BKA_STATUSTEXTON;
  pi.usPageInsertFlags = BKA_FIRST;
  pi.usSettingsFlags = SETTINGS_PAGE_NUMBERS;
  pi.pfnwp = fnwpScreenSaverSettingsPage3;
  pi.resid = hmodThisDLL;
  pi.dlgid = DLG_SCREENSAVERSETTINGSPAGE3;
  pi.pszName = NULL;//"S~creen Saver";
  pi.pCreateParams = somSelf;

  pi.pszHelpLibraryName = achLocalHelpFileName;
  pi.idDefaultHelpPanel = HELPID_PAGE3_SAVERMODULES;

  ulPageID = _wpInsertSettingsPage(somSelf, hwndNotebook, &pi);
  if (ulPageID==0)
  {
    AddLog("[wpssdesktop_wpssAddScreenSaver1Page] : Could not insert page!\n");
    // Could not insert new page!
    return SETTINGS_PAGE_REMOVED;
  }

  // Hack the 'page info' to show the minor tab title in the notebook context menu
  // This is taken from XWorkplace!
  memset(&bpi, 0, sizeof(bpi));
  bpi.cb = sizeof(bpi);
  bpi.fl = BFA_MINORTABTEXT;

  if (!(hfNLSFile = internal_OpenNLSFile(somSelf)) ||
      !sprintmsg(szText, hfNLSFile, "COM_0003"))
    bpi.pszMinorTab = (PSZ) "Screen Saver modules";
  else
    bpi.pszMinorTab = szText;

  bpi.cbMinorTab = strlen(bpi.pszMinorTab);
  WinSendMsg(hwndNotebook,
             BKM_SETPAGEINFO,
             (MPARAM) ulPageID,
             (MPARAM) &bpi);

  // Save pageID for later use!
  hwndSettingsNotebook = hwndNotebook;
  ulSettingsPage3ID = ulPageID;

  internal_CloseNLSFile(hfNLSFile);

  AddLog("[wpssdesktop_wpssAddScreenSaver1Page] : Leave\n");

  // Ok, new page inserted!
  return ulPageID;
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssStartScreenSaverNow(WPSSDesktop *somSelf, BOOL bDontDelayPasswordProtection)
{
  SSCore_StartSavingNow(bDontDelayPasswordProtection);
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssStartScreenSaverModulePreview(WPSSDesktop *somSelf, PSZ pszSaverDLLFileName, HWND hwndClientArea)
{
#ifdef DEBUG_LOGGING
  int rc;

  AddLog("[wpssStartScreenSaverModulePreview]\n");
  rc = SSCore_StartModulePreview(pszSaverDLLFileName, hwndClientArea);
  if (rc!=SSCORE_NOERROR)
  {
    char achTemp[128];
    sprintf(achTemp,"StartModulePreview SSCore error: rc=0x%x\n", rc);
    AddLog(achTemp);
  }
#else

  SSCore_StartModulePreview(pszSaverDLLFileName, hwndClientArea);

#endif
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssConfigureScreenSaverModule(WPSSDesktop *somSelf, PSZ pszSaverDLLFileName, HWND hwndOwnerWindow)
{
  SSCore_ConfigureModule(pszSaverDLLFileName, hwndOwnerWindow);
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssStopScreenSaverModulePreview(WPSSDesktop *somSelf, HWND hwndClientArea)
{
  AddLog("[wpssStopScreenSaverModulePreview]\n");
  SSCore_StopModulePreview(hwndClientArea);
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetScreenSaverEnabled(WPSSDesktop *somSelf, BOOL bEnabled)
{
  SSCoreSettings_t CurrentSettings;

  AddLog("[SetScreenSaverEnabled] : Called!\n");
  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    CurrentSettings.bEnabled = bEnabled;
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpssQueryScreenSaverEnabled(WPSSDesktop *somSelf)
{
  SSCoreSettings_t CurrentSettings;

  CurrentSettings.bEnabled = FALSE;
  SSCore_GetCurrentSettings(&CurrentSettings);

  return CurrentSettings.bEnabled;
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetScreenSaverTimeout(WPSSDesktop *somSelf, ULONG ulTimeoutInmsec)
{
  SSCoreSettings_t CurrentSettings;

  AddLog("[SetScreenSaverTimeout] : Called!\n");
  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    CurrentSettings.iActivationTime = (int) ulTimeoutInmsec;
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope ULONG SOMLINK wpssdesktop_wpssQueryScreenSaverTimeout(WPSSDesktop *somSelf)
{
  SSCoreSettings_t CurrentSettings;

  CurrentSettings.iActivationTime = -1;
  SSCore_GetCurrentSettings(&CurrentSettings);

  return (ULONG) (CurrentSettings.iActivationTime);
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetAutoStartAtStartup(WPSSDesktop *somSelf, BOOL bAutoStart)
{
  WPSSDesktopData *somThis = WPSSDesktopGetData(somSelf);

  _bAutoStartAtStartup = bAutoStart;
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpssQueryAutoStartAtStartup(WPSSDesktop *somSelf)
{
  WPSSDesktopData *somThis = WPSSDesktopGetData(somSelf);

  return _bAutoStartAtStartup;
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssEncryptScreenSaverPassword(WPSSDesktop *somSelf, PSZ pszPassword)
{
  SSCore_EncryptPassword(pszPassword);
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetScreenSaverPasswordProtected(WPSSDesktop *somSelf, BOOL bPasswordProtected)
{
  SSCoreSettings_t CurrentSettings;

  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    CurrentSettings.bPasswordProtected = bPasswordProtected;
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpssQueryScreenSaverPasswordProtected(WPSSDesktop *somSelf)
{
  SSCoreSettings_t CurrentSettings;

  CurrentSettings.bPasswordProtected = 0;
  SSCore_GetCurrentSettings(&CurrentSettings);

  return CurrentSettings.bPasswordProtected;
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetScreenSaverEncryptedPassword(WPSSDesktop *somSelf, PSZ pszEncryptedPassword)
{
  SSCoreSettings_t CurrentSettings;

  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    strncpy(CurrentSettings.achEncryptedPassword, pszEncryptedPassword, sizeof(CurrentSettings.achEncryptedPassword));
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssQueryScreenSaverEncryptedPassword(WPSSDesktop *somSelf, PSZ pszEncryptedPasswordBuf, ULONG ulEncryptedPasswordBufSize)
{
  SSCoreSettings_t CurrentSettings;

  SSCore_GetCurrentSettings(&CurrentSettings);

  strncpy(pszEncryptedPasswordBuf, CurrentSettings.achEncryptedPassword, ulEncryptedPasswordBufSize);
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetDelayedPasswordProtection(WPSSDesktop *somSelf, BOOL bDelayedPasswordProtection)
{
  SSCoreSettings_t CurrentSettings;

  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    CurrentSettings.bUseDelayedPasswordProtection = bDelayedPasswordProtection;
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpssQueryDelayedPasswordProtection(WPSSDesktop *somSelf)
{
  SSCoreSettings_t CurrentSettings;

  SSCore_GetCurrentSettings(&CurrentSettings);

  return CurrentSettings.bUseDelayedPasswordProtection;
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetDelayedPasswordProtectionTimeout(WPSSDesktop *somSelf, ULONG ulTimeoutInmsec)
{
  SSCoreSettings_t CurrentSettings;

  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    CurrentSettings.iPasswordDelayTime = ulTimeoutInmsec;
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope ULONG SOMLINK wpssdesktop_wpssQueryDelayedPasswordProtectionTimeout(WPSSDesktop *somSelf)
{
  SSCoreSettings_t CurrentSettings;

  SSCore_GetCurrentSettings(&CurrentSettings);

  return CurrentSettings.iPasswordDelayTime;
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetScreenSaverModule(WPSSDesktop *somSelf, PSZ pszSaverDLLFileName)
{
  SSCoreSettings_t CurrentSettings;

  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    strncpy(CurrentSettings.achModuleFileName, pszSaverDLLFileName, sizeof(CurrentSettings.achModuleFileName));
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssQueryScreenSaverModule(WPSSDesktop *somSelf, PSZ pszSaverDLLFileNameBuf, ULONG ulSaverDLLFileNameBufSize)
{
  SSCoreSettings_t CurrentSettings;

  SSCore_GetCurrentSettings(&CurrentSettings);

  strncpy(pszSaverDLLFileNameBuf, CurrentSettings.achModuleFileName, ulSaverDLLFileNameBufSize);
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetScreenSaverPreviewEnabled(WPSSDesktop *somSelf, BOOL bEnabled)
{
  WPSSDesktopData *somThis = WPSSDesktopGetData(somSelf);

  _bPreviewEnabled = bEnabled;
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpssQueryScreenSaverPreviewEnabled(WPSSDesktop *somSelf)
{
  WPSSDesktopData *somThis = WPSSDesktopGetData(somSelf);

  return _bPreviewEnabled;
}

SOM_Scope ULONG SOMLINK wpssdesktop_wpssQueryNumOfAvailableScreenSaverModules(WPSSDesktop *somSelf)
{
  SSCoreModuleList_p pList, pTemp;
  ULONG ulResult;

  if (SSCore_GetListOfModules(&pList)!=SSCORE_NOERROR)
    return 0;

  // Ok, got the list, count the number of elements!
  ulResult = 0;
  pTemp = pList;
  while (pTemp)
  {
    ulResult++;
    pTemp = pTemp->pNext;
  }

  SSCore_FreeListOfModules(pList);
  return ulResult;
}

SOM_Scope ULONG SOMLINK wpssdesktop_wpssQueryInfoAboutAvailableScreenSaverModules(WPSSDesktop *somSelf, 
                                                                                  ULONG ulMaxModules,
                                                                                  PSSMODULEDESC pModuleDescArray)
{
  SSCoreModuleList_p pList, pTemp;
  ULONG ulResult;

  AddLog("[WPSSDesktop] : QIAASSM: Calling SSCore_GetListOfModules()\n");

  if (SSCore_GetListOfModules(&pList)!=SSCORE_NOERROR)
  {
    AddLog("[WPSSDesktop] : QIAASSM: SSCore_GetListOfModules() returned error, returning 0\n");
    return 0;
  }

  AddLog("[WPSSDesktop] : QIAASSM: Processing the result\n");

  // Ok, got the list, count the number of elements!
  ulResult = 0;
  pTemp = pList;
  while ((pTemp) && (ulResult<ulMaxModules))
  {
    // Fill fields
    strncpy(pModuleDescArray[ulResult].achSaverDLLFileName,
            pTemp->achModuleFileName,
            sizeof(pModuleDescArray[ulResult].achSaverDLLFileName));
    pModuleDescArray[ulResult].lVersionMajor = pTemp->ModuleDesc.iVersionMajor;
    pModuleDescArray[ulResult].lVersionMinor = pTemp->ModuleDesc.iVersionMinor;
    strncpy(pModuleDescArray[ulResult].achModuleName,
            pTemp->ModuleDesc.achModuleName,
            sizeof(pModuleDescArray[ulResult].achModuleName));
    strncpy(pModuleDescArray[ulResult].achModuleDesc,
            pTemp->ModuleDesc.achModuleDesc,
            sizeof(pModuleDescArray[ulResult].achModuleDesc));
    pModuleDescArray[ulResult].bConfigurable = pTemp->ModuleDesc.bConfigurable;
    pModuleDescArray[ulResult].bSupportsPasswordProtection = pTemp->ModuleDesc.bSupportsPasswordProtection;

    // Go for next one
    ulResult++;
    pTemp = pTemp->pNext;
  }

  AddLog("[WPSSDesktop] : QIAASSM: Calling SSCore_FreeListOfModules()\n");

  SSCore_FreeListOfModules(pList);

  AddLog("[WPSSDesktop] : QIAASSM: Done, returning.\n");
  return ulResult;
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetUseDPMSStandby(WPSSDesktop *somSelf, BOOL bEnabled)
{
  SSCoreSettings_t CurrentSettings;

  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    CurrentSettings.bUseDPMSStandbyState = bEnabled;
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpssQueryUseDPMSStandby(WPSSDesktop *somSelf)
{
  SSCoreSettings_t CurrentSettings;

  SSCore_GetCurrentSettings(&CurrentSettings);

  return CurrentSettings.bUseDPMSStandbyState;
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetDPMSStandbyTimeout(WPSSDesktop *somSelf, ULONG ulTimeoutInmsec)
{
  SSCoreSettings_t CurrentSettings;

  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    CurrentSettings.iDPMSStandbyTime = ulTimeoutInmsec;
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope ULONG SOMLINK wpssdesktop_wpssQueryDPMSStandbyTimeout(WPSSDesktop *somSelf)
{
  SSCoreSettings_t CurrentSettings;

  SSCore_GetCurrentSettings(&CurrentSettings);

  return CurrentSettings.iDPMSStandbyTime;
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetUseDPMSSuspend(WPSSDesktop *somSelf, BOOL bEnabled)
{
  SSCoreSettings_t CurrentSettings;

  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    CurrentSettings.bUseDPMSSuspendState = bEnabled;
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpssQueryUseDPMSSuspend(WPSSDesktop *somSelf)
{
  SSCoreSettings_t CurrentSettings;

  SSCore_GetCurrentSettings(&CurrentSettings);

  return CurrentSettings.bUseDPMSSuspendState;
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetDPMSSuspendTimeout(WPSSDesktop *somSelf, ULONG ulTimeoutInmsec)
{
  SSCoreSettings_t CurrentSettings;

  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    CurrentSettings.iDPMSSuspendTime = ulTimeoutInmsec;
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope ULONG SOMLINK wpssdesktop_wpssQueryDPMSSuspendTimeout(WPSSDesktop *somSelf)
{
  SSCoreSettings_t CurrentSettings;

  SSCore_GetCurrentSettings(&CurrentSettings);

  return CurrentSettings.iDPMSSuspendTime;
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetUseDPMSOff(WPSSDesktop *somSelf, BOOL bEnabled)
{
  SSCoreSettings_t CurrentSettings;

  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    CurrentSettings.bUseDPMSOffState = bEnabled;
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpssQueryUseDPMSOff(WPSSDesktop *somSelf)
{
  SSCoreSettings_t CurrentSettings;

  SSCore_GetCurrentSettings(&CurrentSettings);

  return CurrentSettings.bUseDPMSOffState;
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetDPMSOffTimeout(WPSSDesktop *somSelf, ULONG ulTimeoutInmsec)
{
  SSCoreSettings_t CurrentSettings;

  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    CurrentSettings.iDPMSOffTime = ulTimeoutInmsec;
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope ULONG SOMLINK wpssdesktop_wpssQueryDPMSOffTimeout(WPSSDesktop *somSelf)
{
  SSCoreSettings_t CurrentSettings;

  SSCore_GetCurrentSettings(&CurrentSettings);

  return CurrentSettings.iDPMSOffTime;
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetUseCurrentSecurityPassword(WPSSDesktop *somSelf, BOOL bEnabled)
{
  SSCoreSettings_t CurrentSettings;

  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    CurrentSettings.bUseCurrentSecurityPassword = bEnabled;
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpssQueryUseCurrentSecurityPassword(WPSSDesktop *somSelf)
{
  SSCoreSettings_t CurrentSettings;

  SSCore_GetCurrentSettings(&CurrentSettings);

  return CurrentSettings.bUseCurrentSecurityPassword;
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetWakeByKeyboardOnly(WPSSDesktop *somSelf, BOOL bEnabled)
{
  SSCoreSettings_t CurrentSettings;

  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    CurrentSettings.bWakeByKeyboardOnly = bEnabled;
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpssQueryWakeByKeyboardOnly(WPSSDesktop *somSelf)
{
  SSCoreSettings_t CurrentSettings;

  SSCore_GetCurrentSettings(&CurrentSettings);

  return CurrentSettings.bWakeByKeyboardOnly;
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetFirstKeyEventGoesToPwdWindow(WPSSDesktop *somSelf, BOOL bEnabled)
{
  SSCoreSettings_t CurrentSettings;

  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    CurrentSettings.bFirstKeyEventGoesToPwdWindow = bEnabled;
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpssQueryFirstKeyEventGoesToPwdWindow(WPSSDesktop *somSelf)
{
  SSCoreSettings_t CurrentSettings;

  SSCore_GetCurrentSettings(&CurrentSettings);

  return CurrentSettings.bFirstKeyEventGoesToPwdWindow;
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetDisableVIOPopupsWhileSaving(WPSSDesktop *somSelf, BOOL bVIOPopupsDisabled)
{
  SSCoreSettings_t CurrentSettings;

  if (SSCore_GetCurrentSettings(&CurrentSettings)==SSCORE_NOERROR)
  {
    CurrentSettings.bDisableVIOPopups = bVIOPopupsDisabled;
    SSCore_SetCurrentSettings(&CurrentSettings);
  }
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpssQueryDisableVIOPopupsWhileSaving(WPSSDesktop *somSelf)
{
  SSCoreSettings_t CurrentSettings;

  SSCore_GetCurrentSettings(&CurrentSettings);

  return CurrentSettings.bDisableVIOPopups;
}

// ---- Overriden functions ----

SOM_Scope BOOL SOMLINK wpssdesktop_wpSaveState(WPSSDesktop *somSelf)
{
  WPSSDesktopData *somThis = WPSSDesktopGetData(somSelf);

  // Save our state
  if (_wpIsCurrentDesktop(somSelf))
  {
    AddLog("[WPSSDesktop] : wpSaveState : I'm the desktop object, saving my state.\n");

    _wpSaveLong(somSelf, "WPSSDesktop", 1, _bPreviewEnabled);
    _wpSaveLong(somSelf, "WPSSDesktop", 2, _bAutoStartAtStartup);
    _wpSaveString(somSelf, "WPSSDesktop", 3, _achLanguage);
  }

  // Let the old desktop restore its state!
  return parent_wpSaveState(somSelf);

}

SOM_Scope BOOL SOMLINK wpssdesktop_wpRestoreState(WPSSDesktop *somSelf, ULONG ulReserved)
{
  WPSSDesktopData *somThis = WPSSDesktopGetData(somSelf);
  BOOL bParentRC;

  // Let the old desktop restore its state!
  bParentRC = parent_wpRestoreState(somSelf,ulReserved);

  if (bParentRC)
  {
    // Make sure that the old desktop lockup thing is not turned on!
    // So, if we're the desktop, send setup string to Desktop to turn off
    // the autolockup and the lockuponstartup!

    // It was tried to call the wpSetAutoLockup() directly, but the Desktop
    // object somehow doesn't like that, that setting did not stick...
    if (_wpIsCurrentDesktop(somSelf))
    {
      AddLog("[WPSSDesktop] : wpRestoreState : I'm the desktop object, calling setup.\n");
      _wpSetup(somSelf, "AUTOLOCKUP=NO;LOCKUPONSTARTUP=NO");
    }
  }

  // Then restore our internal state
  if (_wpIsCurrentDesktop(somSelf))
  {
    ULONG ulValue;
    FILE *hfNLSFile;

    AddLog("[WPSSDesktop] : wpRestoreState : I'm the desktop object, restoring my state.\n");
    ulValue = sizeof(_achLanguage);
    _wpRestoreString(somSelf, "WPSSDesktop", 3, _achLanguage, &ulValue);
    _wpRestoreLong(somSelf, "WPSSDesktop", 2, &_bAutoStartAtStartup);
    _wpRestoreLong(somSelf, "WPSSDesktop", 1, &_bPreviewEnabled);

    hfNLSFile = internal_OpenNLSFile(somSelf);
    // Initialize SSCore with language setting strings
    internal_SendNLSTextToSSCore(somSelf, hfNLSFile);
    // Set initial help filename
    internal_SetNLSHelpFilename(somSelf);
    internal_CloseNLSFile(hfNLSFile);
  }

  return bParentRC;
}

/*
 *
 *  METHOD: wpAddDesktopLockup?Page                        ( ) PRIVATE
 *                                                         (X) PUBLIC
 *  DESCRIPTION:
 *
 *
 */

SOM_Scope BOOL SOMLINK wpssdesktop_wpAddDesktopLockup1Page(WPSSDesktop *somSelf, HWND hwndNotebook)
{
  // Replace original Lockup-1 page with our first screen saver page
  return wpssdesktop_wpssAddScreenSaver1Page(somSelf, hwndNotebook);
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpAddDesktopLockup2Page(WPSSDesktop *somSelf, HWND hwndNotebook)
{
  // Replace original Lockup-2 page with our second screen saver page
  return wpssdesktop_wpssAddScreenSaver2Page(somSelf, hwndNotebook);
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpAddDesktopLockup3Page(WPSSDesktop *somSelf, HWND hwndNotebook)
{
  // Replace original Lockup-3 page with our third screen saver page
  return wpssdesktop_wpssAddScreenSaver3Page(somSelf, hwndNotebook);
}

SOM_Scope VOID SOMLINK wpssdesktop_wpSetAutoLockup(WPSSDesktop *somSelf, BOOL bAutoLockup)
{
  if (_wpIsCurrentDesktop(somSelf))
  {
    // Hm, somebody wants to set autolockup on the desktop.
    // We always set it to OFF, and turn on/off the screen saver as the autolockup
    // is wanted to be turned on/off.
    AddLog("[WPSSDesktop] : wpSetAutoLockup\n");
    parent_wpSetAutoLockup(somSelf, FALSE);
    wpssdesktop_wpssSetScreenSaverEnabled(somSelf, bAutoLockup);
  } else
  {
    parent_wpSetAutoLockup(somSelf, bAutoLockup);
  }
}

SOM_Scope VOID SOMLINK wpssdesktop_wpSetLockupOnStartup(WPSSDesktop *somSelf, BOOL bLockupOnStartup)
{
  if (_wpIsCurrentDesktop(somSelf))
  {
    // Hm, somebody wants to set LockupOnStartup on the desktop.
    // We always set it to OFF!
    AddLog("[WPSSDesktop] : wpLockOnStartup\n");
    parent_wpSetLockupOnStartup(somSelf, FALSE);
    // Instead, we route it to our own function, setting auto-start saver at startup.
    wpssdesktop_wpssSetAutoStartAtStartup(somSelf, bLockupOnStartup);
  } else
  {
    parent_wpSetLockupOnStartup(somSelf, bLockupOnStartup);
  }
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpSetup(WPSSDesktop *somSelf, PSZ pszSetupString)
{
  BOOL rc;
  BOOL bIsCurrentDesktop, bNeedAutoLockupOff, bNeedLockOnStartupOff;
  CHAR szValue[10];
  ULONG cbBuffer;

  // The desktop got a setup string.
  // Make it sure that the setup strings will not be used to
  // turn on autolockup or lockuponstartup!

  AddLog("[WPSSDesktop] : wpSetup\n");
  AddLog(" String: ["); AddLog(pszSetupString); AddLog("]\n");

  bNeedAutoLockupOff = FALSE;
  bNeedLockOnStartupOff = FALSE;
  bIsCurrentDesktop = _wpIsCurrentDesktop(somSelf);

  if (bIsCurrentDesktop)
  {
    // If the user passes AUTOLOCKUP=YES or LOCKUPONSTARTUP=YES, then we
    // will pass these values with NO after the call!
    memset(szValue, 0, sizeof(szValue));
    cbBuffer = sizeof(szValue);
    if (_wpScanSetupString(somSelf, pszSetupString, "AUTOLOCKUP",
                           szValue, &cbBuffer))
    {
      if (!stricmp(szValue, "YES"))
      {
	AddLog("[WPSSDesktop] : wpSetup : AUTOLOCKUP=YES!\n");
        bNeedAutoLockupOff = TRUE;
      }
    }

    memset(szValue, 0, sizeof(szValue));
    cbBuffer = sizeof(szValue);
    if (_wpScanSetupString(somSelf, pszSetupString, "LOCKUPONSTARTUP",
                           szValue, &cbBuffer))
    {
      if (!stricmp(szValue, "YES"))
      {
	AddLog("[WPSSDesktop] : wpSetup : LOCKUPONSTARTUP=YES!\n");
        bNeedLockOnStartupOff = TRUE;

        // We route it to our own function, setting auto-start saver at startup.
        wpssdesktop_wpssSetAutoStartAtStartup(somSelf, TRUE);
      } else
      {
        // We route it to our own function, setting auto-start saver at startup.
        wpssdesktop_wpssSetAutoStartAtStartup(somSelf, FALSE);
      }
    }
  }

  rc = parent_wpSetup(somSelf, pszSetupString);

  if (bIsCurrentDesktop)
  {
    if (bNeedAutoLockupOff)
      parent_wpSetup(somSelf, "AUTOLOCKUP=NO");
    if (bNeedLockOnStartupOff)
      parent_wpSetup(somSelf, "LOCKUPONSTARTUP=NO");
  }

  return (rc);
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpMenuItemSelected(WPSSDesktop *somSelf,
                                                      HWND hwndFrame,
                                                      ULONG MenuId)
{
  if (_wpIsCurrentDesktop(somSelf))
  {
    // Which of our menu items was selected?
    switch( MenuId )
    {
      case WPMENUID_LOCKUP:
	// It's the LockUp Now menuitem, call the screensaver instead of the lockup!
	AddLog("[WPSSDesktop] : wpMenuItemSelected : WPMENUID_LOCKUP\n");
        wpssdesktop_wpssStartScreenSaverNow(somSelf, TRUE);
        break;

      default:
        return parent_wpMenuItemSelected(somSelf, hwndFrame, MenuId);
        break;
    }
    return TRUE; // we processed it
  } else
    return parent_wpMenuItemSelected(somSelf, hwndFrame, MenuId);
}

SOM_Scope HWND SOMLINK wpssdesktop_wpOpen(WPSSDesktop *somSelf,
                                          HWND hwndCnr,
                                          ULONG ulView,
                                          ULONG param)
{
  WPSSDesktopData *somThis = WPSSDesktopGetData(somSelf);
  HWND hwndResult;

  hwndResult = parent_wpOpen(somSelf, hwndCnr, ulView, param);

  if (bFirstOpen)
  {
    AddLog("[WPSSDesktop] : wpOpen : It was the first open!\n");

    bFirstOpen = FALSE;

    if (_bAutoStartAtStartup)
    {
      AddLog("[WPSSDesktop] : wpOpen : Autostart is ON!\n");
      if (wpssdesktop_wpssQueryScreenSaverPasswordProtected(somSelf))
      {
        AddLog("[WPSSDesktop] : wpOpen : PwdProt is ON -> Start saver!\n");
        wpssdesktop_wpssStartScreenSaverNow(somSelf, TRUE);
      }
#ifdef DEBUG_LOGGING
      else
        AddLog("[WPSSDesktop] : wpOpen : PwdProt is OFF!\n");
    }
    else
    {
      AddLog("[WPSSDesktop] : wpOpen : Autostart is OFF!\n");
#endif
    }

    AddLog("[WPSSDesktop] : wpOpen : End of processing of first open.\n");
  }

  return hwndResult;
}


/**************************  CLASS METHODS SECTION  ***************************
*****                                                                     *****
*****              Do not put any code in this section unless             *****
*****                     it is an object CLASS method                    *****
*****                                                                     *****
******************************************************************************/
#undef SOM_CurrentClass
#define SOM_CurrentClass SOMMeta

SOM_Scope BOOL SOMLINK wpssdesktopM_wpssclsInitializeScreenSaver(M_WPSSDesktop *somSelf)
{
  int rc;
  ULONG ulValue;

  AddLog("[wpssdesktopM_wpssclsInitializeScreenSaver] : Enter\n");

  ulValue = PrfQueryProfileString(HINI_PROFILE,
                                  "SSaver",
                                  "HomePath",
				  "",
                                  achHomeDirectory,
                                  sizeof(achHomeDirectory));

  if ((ulValue==0) || (achHomeDirectory[0]==0))
  {
    // Could not get home directory, try the old way!

    // Get boot drive, and assemble the Screen Saver home directory from that!
    rc = DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, &ulValue, sizeof(ulValue));
    if (rc!=NO_ERROR)
    {
      // Could not query boot drive!
      ulValue = 3; // Defaults to C:
    }
    sprintf(achHomeDirectory, "%c:\\os2\\apps\\SSaver\\",'@'+ulValue);
  } else
  {
    // Got home directory, make sure it has the trailing '\'
    ulValue=strlen(achHomeDirectory);
    if (achHomeDirectory[ulValue-1]!='\\')
    {
      achHomeDirectory[ulValue]='\\';
      achHomeDirectory[ulValue+1]=0;
    }
  }

  AddLog("[wpssclsInitializeScreenSaver] : Home dir: [");
  AddLog(achHomeDirectory);
  AddLog("]\n");

  // use the window text font in effect at startup for the entire session
  PrfQueryProfileString(HINI_USER, "PM_SystemFonts", "WindowText",
                        "9.WarpSans", achFontToUse, sizeof(achFontToUse));

  // Initialize Screen Saver
  rc = SSCore_Initialize(WinQueryAnchorBlock(HWND_DESKTOP), achHomeDirectory);
#ifdef DEBUG_LOGGING
  if (rc!=SSCORE_NOERROR)
  {
    AddLog("[wpssclsInitializeScreenSaver] : Could not initialize SSCore!\n");
  }
#endif

  AddLog("[wpssdesktopM_wpssclsInitializeScreenSaver] : Leave\n");

  return (rc==SSCORE_NOERROR);
}

SOM_Scope BOOL SOMLINK wpssdesktopM_wpssclsUninitializeScreenSaver(M_WPSSDesktop *somSelf)
{
  AddLog("[wpssdesktopM_wpssclsUninitializeScreenSaver] : Calling sscore_uninitialize\n");
  return (SSCore_Uninitialize()==SSCORE_NOERROR);
}

SOM_Scope void SOMLINK wpssdesktopM_wpclsInitData(M_WPSSDesktop *somSelf)
{
  // Initialize parent
  parent_wpclsInitData(somSelf);

  // Initialize screen saver
  wpssdesktopM_wpssclsInitializeScreenSaver(somSelf);
}

SOM_Scope void SOMLINK wpssdesktopM_wpclsUnInitData(M_WPSSDesktop *somSelf)
{
  // Uninitialize screen saver
  wpssdesktopM_wpssclsUninitializeScreenSaver(somSelf);

  // Uninit parent
  parent_wpclsUnInitData(somSelf);
}

SOM_Scope PSZ SOMLINK wpssdesktopM_wpclsQueryTitle(M_WPSSDesktop *somSelf)
{
  return("WPSSDesktop");
}

SOM_Scope BOOL SOMLINK wpssdesktopM_wpclsQuerySettingsPageSize(M_WPSSDesktop *somSelf, PSIZEL pSizl)
{
  return  parent_wpclsQuerySettingsPageSize(somSelf, pSizl);
}


/**************************  ORDINARY CODE SECTION  ***************************
*****                                                                     *****
*****                  Any non-method code should go here.                *****
*****                                                                     *****
******************************************************************************/
#undef SOM_CurrentClass

static void StartPage3Preview(HWND hwnd)
{
  Page3UserData_p pUserData;
  long lID;
  pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (pUserData)
  {
    // Stop old module, if running
    wpssdesktop_wpssStopScreenSaverModulePreview(pUserData->Desktop,
                                                 WinWindowFromID(hwnd, ID_PREVIEWAREA));
    lID = (long) WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_QUERYSELECTION, (MPARAM) LIT_CURSOR, (MPARAM) NULL);
    if (lID!=LIT_NONE)
    {
      lID = (long) WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_QUERYITEMHANDLE, (MPARAM) lID, (MPARAM) NULL);
      if ((lID<0) || lID>=pUserData->ulMaxModules)
      {
        // Something is wrong, the handle, which should tell the index in the
        // module desc array, points out of the array!
        lID = LIT_NONE;
      }
    }
    if (lID!=LIT_NONE)
    {
      AddLog("Start preview: ["); AddLog(pUserData->pModuleDescArray[lID].achSaverDLLFileName); AddLog("]\n");
      // Disable some controls for the time it is starting
      WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE), FALSE);
      WinEnableWindow(WinWindowFromID(hwnd, PB_STARTMODULE), FALSE);
      WinEnableWindow(WinWindowFromID(hwnd, LB_MODULELIST), FALSE);
      WinEnableWindow(WinWindowFromID(hwnd, CB_SHOWPREVIEW), FALSE);

      // Start the preview!
      wpssdesktop_wpssStartScreenSaverModulePreview(pUserData->Desktop,
                                                    pUserData->pModuleDescArray[lID].achSaverDLLFileName,
                                                    WinWindowFromID(hwnd, ID_PREVIEWAREA));
      pUserData->iPreviewMsgCounter++;
    }
  }
}
static void StopPage3Preview(HWND hwnd)
{
  Page3UserData_p pUserData;
  pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (pUserData)
  {
    // Disable some controls for the time it is stopping
    WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE), FALSE);
    WinEnableWindow(WinWindowFromID(hwnd, PB_STARTMODULE), FALSE);
    WinEnableWindow(WinWindowFromID(hwnd, LB_MODULELIST), FALSE);
    WinEnableWindow(WinWindowFromID(hwnd, CB_SHOWPREVIEW), FALSE);

    // Stop the preview!
    wpssdesktop_wpssStopScreenSaverModulePreview(pUserData->Desktop,
                                                 WinWindowFromID(hwnd, ID_PREVIEWAREA));

    pUserData->iPreviewMsgCounter++;
  }
}

static void UseSelectedModule(HWND hwnd)
{
  Page3UserData_p pUserData;
  long lID;
  pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (pUserData)
  {
    lID = (long) WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_QUERYSELECTION, (MPARAM) LIT_CURSOR, (MPARAM) NULL);
    if (lID!=LIT_NONE)
    {
      lID = (long) WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_QUERYITEMHANDLE, (MPARAM) lID, (MPARAM) NULL);
      if ((lID<0) || lID>=pUserData->ulMaxModules)
      {
        // Something is wrong, the handle, which should tell the index in the
        // module desc array, points out of the array!
        lID = LIT_NONE;
      }
    }
    if (lID!=LIT_NONE)
    {
      wpssdesktop_wpssSetScreenSaverModule(pUserData->Desktop,
                                           pUserData->pModuleDescArray[lID].achSaverDLLFileName);
    }
  }
}
static void ConfigureSelectedModule(HWND hwnd)
{
  Page3UserData_p pUserData;
  long lID;
  pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (pUserData)
  {
    lID = (long) WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_QUERYSELECTION, (MPARAM) LIT_CURSOR, (MPARAM) NULL);
    if (lID!=LIT_NONE)
    {
      lID = (long) WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_QUERYITEMHANDLE, (MPARAM) lID, (MPARAM) NULL);
      if ((lID<0) || lID>=pUserData->ulMaxModules)
      {
        // Something is wrong, the handle, which should tell the index in the
        // module desc array, points out of the array!
        lID = LIT_NONE;
      }
    }
    if (lID!=LIT_NONE)
    {
      // Disable some controls for the time the module is being configured
      WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE), FALSE);
      WinEnableWindow(WinWindowFromID(hwnd, PB_STARTMODULE), FALSE);
      WinEnableWindow(WinWindowFromID(hwnd, LB_MODULELIST), FALSE);
      WinEnableWindow(WinWindowFromID(hwnd, CB_SHOWPREVIEW), FALSE);

      wpssdesktop_wpssConfigureScreenSaverModule(pUserData->Desktop,
                                                 pUserData->pModuleDescArray[lID].achSaverDLLFileName,
                                                 hwnd);

      // Enable the disabled controls
      WinEnableWindow(WinWindowFromID(hwnd, PB_STARTMODULE), TRUE);
      WinEnableWindow(WinWindowFromID(hwnd, LB_MODULELIST), TRUE);
      WinEnableWindow(WinWindowFromID(hwnd, CB_SHOWPREVIEW), TRUE);
      WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE), pUserData->pModuleDescArray[lID].bConfigurable);
    }
  }
}
static void ChangeSaverPassword(HWND hwnd)
{
  Page1UserData_p pUserData;
  char achTemp[SSCORE_MAXPASSWORDLEN], achTemp2[SSCORE_MAXPASSWORDLEN];
  FILE *hfNLSFile;

  pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (pUserData)
  {
    hfNLSFile = internal_OpenNLSFile(pUserData->Desktop);

    WinQueryDlgItemText(hwnd, EF_PASSWORD, sizeof(achTemp2), achTemp2);
    WinQueryDlgItemText(hwnd, EF_PASSWORD2, sizeof(achTemp), achTemp);
    if (strcmp(achTemp, achTemp2))
    {
      // The two passwords are not the same!
      char *pchTemp1, *pchTemp2;
      pchTemp1 = (char *) malloc(1024);
      pchTemp2 = (char *) malloc(1024);
      if ((pchTemp1) && (pchTemp2) && (hfNLSFile) &&
          (sprintmsg(pchTemp1, hfNLSFile, "PG2_0018")) &&
          (sprintmsg(pchTemp2, hfNLSFile, "PG2_0017"))
         )
        WinMessageBox(HWND_DESKTOP, hwnd,
                      pchTemp1,
                      pchTemp2, 0,
                      MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);

      else
        WinMessageBox(HWND_DESKTOP, hwnd,
                      "The two passwords you've entered do not match!",
                      "Password mismatch", 0,
                      MB_OK | MB_MOVEABLE | MB_ICONEXCLAMATION);
      if (pchTemp1)
        free(pchTemp1);
      if (pchTemp2)
        free(pchTemp2);
    } else
    {
      // The two encrypted passwords are the same, fine.
      wpssdesktop_wpssSetScreenSaverEncryptedPassword(pUserData->Desktop,
                                                      achTemp);
      // Disable CHANGE button!
      WinEnableWindow(WinWindowFromID(hwnd, PB_CHANGE), FALSE);
    }

    internal_CloseNLSFile(hfNLSFile);
  }
}
MRESULT EXPENTRY PreviewWindowProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  char *pchPreviewAreaText;
  HWND hwndParent;

  switch( msg )
  {
    case WM_PAINT:
      {
	HPS    hpsBP;
	RECTL  rc;
	Page3UserData_p pUserData;
	FILE *hfNLSFile;

        // If we're here, then there must be a settings page 3, we can use that!
	pUserData = (Page3UserData_p) WinQueryWindowULong(hwndSettingsPage3, QWL_USER);
	if (pUserData)
	  hfNLSFile = internal_OpenNLSFile(pUserData->Desktop);
	else
	  hfNLSFile = NULLHANDLE;

        hpsBP = WinBeginPaint(hwnd, 0L, &rc);
        WinQueryWindowRect(hwnd, &rc);
        if (!hfNLSFile)
        {
          pchPreviewAreaText = "No preview";
          WinDrawText(hpsBP,
                      strlen(pchPreviewAreaText), pchPreviewAreaText,
                      &rc,
                      SYSCLR_WINDOWTEXT, // Foreground color
                      SYSCLR_WINDOW,     // Background color
                      DT_ERASERECT | DT_CENTER | DT_VCENTER);
        } else
	{
          pchPreviewAreaText = (char *) malloc(1024);
          if (pchPreviewAreaText)
          {
	    if (!sprintmsg(pchPreviewAreaText, hfNLSFile, "PG3_0015"))
              sprintf(pchPreviewAreaText, "No preview");

            WinDrawText(hpsBP,
                        strlen(pchPreviewAreaText), pchPreviewAreaText,
                        &rc,
                        SYSCLR_WINDOWTEXT, // Foreground color
                        SYSCLR_WINDOW,     // Background color
                        DT_ERASERECT | DT_CENTER | DT_VCENTER);
            free(pchPreviewAreaText);
	  }
	}

	internal_CloseNLSFile(hfNLSFile);

	WinEndPaint(hpsBP);
	break;
      }

    // This should not be needed, because SSCore always
    // posts these to the parent of the preview window,
    // but it's always good to have seatbelts. :)
    case WM_SSCORENOTIFY_PREVIEWSTARTED:
    case WM_SSCORENOTIFY_PREVIEWSTOPPED:
      AddLog("[PreviewWndProc] : SSCORENOTIFY! Forwarding to parent!\n");
      hwndParent = WinQueryWindow(hwnd, QW_PARENT);
      return WinSendMsg(hwndParent, msg, mp1, mp2);

    default:
      /*
      {
        char achTemp[128];
        sprintf(achTemp,"[PreviewWndProc] : Unprocessed: 0x%x\n", msg);
        AddLog(achTemp);
      }
      */
      break;
  }
  return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}

static int internal_IsLocaleObject(PDRAGINFO pdiDragInfo)
{
  PDRAGITEM pditDragItem;
  char achTemp[256];
  char achTemp2[128];
  int rc;

  // Get drag info!
  rc = DrgAccessDraginfo(pdiDragInfo);
  if (!rc)
    return FALSE;

  // Check if we're being got only one object!
  if (pdiDragInfo->cditem>1)
  {
    DrgFreeDraginfo(pdiDragInfo);
    // We support it, but not in this form!
    return FALSE;
  }

  // Now, get what kind of item they want to drop on us!
  pditDragItem = DrgQueryDragitemPtr(pdiDragInfo, 0);
  if (!pditDragItem)
  {
    DrgFreeDraginfo(pdiDragInfo);
    // We support it, but not in this form!
    return FALSE;
  }

  if ((DrgVerifyType(pditDragItem, DRT_TEXT)) &&
      (DrgVerifyRMF(pditDragItem, "DRM_OS2FILE", "DRF_BITMAP")))
  {
    // Seems to be the fine format.
    // Let's check some more stuffs to be able to see if it's from locale object or not!

    DrgQueryStrName(pditDragItem->hstrContainerName,
                    sizeof(achTemp),
                    achTemp);
    if (!strcmp(achTemp, "."))
    {
      // Container is OK!

      // Now check if target name conforms to Locale format!
      DrgQueryStrName(pditDragItem->hstrTargetName,
                      sizeof(achTemp),
                      achTemp);

      /*
       * The old method was this: We checked for filename, if
       * it conforms to locale format id.
       * New method: we query the Unicode api if a locale object
       * of that given name exists. (as of v1.4)

      if ((strlen(achTemp)<6) ||
          (achTemp[2]!='_'))
      {
        // Wrong target name format!
        DrgFreeDraginfo(pdiDragInfo);
        // We support it, but not in this form!
        return FALSE;
      }
      // Good target name format, something like en_US.IBM850!
      DrgFreeDraginfo(pdiDragInfo);
      return TRUE;
      */

      rc = internal_GetRealLocaleNameFromLanguageCode(achTemp, achTemp2, sizeof(achTemp2));
      DrgFreeDraginfo(pdiDragInfo);
      // So, we accept the drag (or not)
      return (rc);
    } else
    {
      // Container is not ok, it's not from Locale object!
      DrgFreeDraginfo(pdiDragInfo);
      return FALSE;
    }
  } else
  {
    // Not a format we accept.
    DrgFreeDraginfo(pdiDragInfo);
    return FALSE;
  }
}

static void internal_SetLanguageFromLocaleObject(WPSSDesktop *somSelf, PDRAGINFO pdiDragInfo)
{
  WPSSDesktopData *somThis = WPSSDesktopGetData(somSelf);
  PDRAGITEM pditDragItem;
  int rc;
  char achTemp[256];
  /*  int i;  // Was used for old method */
  FILE *hfNLSFile;

  // Get drag info!
  rc = DrgAccessDraginfo(pdiDragInfo);
  if (!rc)
    return;

  pditDragItem = DrgQueryDragitemPtr(pdiDragInfo, 0);
  if (!pditDragItem)
  {
    DrgFreeDraginfo(pdiDragInfo);
    return;
  }

  // Get target name! (Language string)
  DrgQueryStrName(pditDragItem->hstrTargetName,
                  sizeof(achTemp),
                  achTemp);

  AddLog("[internal_SetLanguageFromLocaleObject] : Changing language!\n");
  AddLog("[internal_SetLanguageFromLocaleObject] : Dropped: [");
  AddLog(achTemp);
  AddLog("]\n");

  /*
   * Old method:

  // We now have something like:
  // hu_HU.IBM852
  // or
  // nl_NL_EURO.IBM850
  // We have to replace the first '.' with a zero
  // to get the locale name.
  i=0;
  while (achTemp[i])
  {
    if (achTemp[i]=='.')
    {
      achTemp[i] = 0;
      break;
    }
    i++;
  }

  */

  AddLog("[internal_SetLanguageFromLocaleObject] : New language is: [");
  AddLog(achTemp);
  AddLog("]\n");

  DrgFreeDraginfo(pdiDragInfo);

  // Let the changes take effect!
  strcpy(_achLanguage, achTemp);

  hfNLSFile = internal_OpenNLSFile(somSelf);  // Open new NLS file
  internal_SendNLSTextToSSCore(somSelf, hfNLSFile); // Send new nls text to core
  internal_SetNLSHelpFilename(somSelf);             // Set new help filename
  internal_NotifyWindowsAboutLanguageChange(hfNLSFile); // Change window texts
  internal_CloseNLSFile(hfNLSFile); // Close old file
}

MRESULT EXPENTRY fnwpNLSEnabledNotebookWindowProc(HWND hwnd, ULONG msg,
						  MPARAM mp1, MPARAM mp2)
{
  ULONG ulCurrPageID;
  if (msg==WM_HELP)
  {
    ulCurrPageID = (ULONG) WinSendMsg(hwnd, BKM_QUERYPAGEID,
				      (MPARAM) 0,
				      MPFROM2SHORT(BKA_TOP, 0));
    if (ulCurrPageID == ulSettingsPage1ID)
    {
      Page1UserData_p pUserData;
      pUserData = (Page1UserData_p) WinQueryWindowULong(hwndSettingsPage1, QWL_USER);
      if (pUserData)
	// Show help
	internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE1_GENERALSETTINGS);
      return (MRESULT) TRUE;
    }
    if (ulCurrPageID == ulSettingsPage2ID)
    {
      Page2UserData_p pUserData;
      pUserData = (Page2UserData_p) WinQueryWindowULong(hwndSettingsPage2, QWL_USER);
      if (pUserData)
	// Show help
	internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE2_PASSWORDPROTECTION);
      return (MRESULT) TRUE;
    }
    if (ulCurrPageID == ulSettingsPage3ID)
    {
      Page3UserData_p pUserData;
      pUserData = (Page3UserData_p) WinQueryWindowULong(hwndSettingsPage3, QWL_USER);
      if (pUserData)
	// Show help
	internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE3_SAVERMODULES);
      return (MRESULT) TRUE;
    }
  }
  return pfnOldNotebookWindowProc(hwnd, msg, mp1, mp2);
}

static void SubclassNotebookWindowForNLS()
{
  if (WinQueryWindowPtr(hwndSettingsNotebook, QWP_PFNWP)!=fnwpNLSEnabledNotebookWindowProc)
  {
    // Not yet subclassed, subclass it!
    pfnOldNotebookWindowProc = WinSubclassWindow(hwndSettingsNotebook,
						 fnwpNLSEnabledNotebookWindowProc);
  }
}


MRESULT EXPENTRY fnwpNLSEnabledControlWindowProc(HWND hwnd, ULONG msg,
                                                 MPARAM mp1, MPARAM mp2)
{
  PFNWP pfnwpOldProc;

  pfnwpOldProc = (PFNWP) WinQueryWindowULong(hwnd, QWL_USER);
  if (!pfnwpOldProc)
    pfnwpOldProc = WinDefWindowProc;

  // Handling Drag'n'Drop of Locale objects!
  if (msg == DM_DRAGOVER)
  {
    if (internal_IsLocaleObject((PDRAGINFO) mp1))
    {
      // So, we accept the drag!
      return MRFROM2SHORT(DOR_DROP, DO_COPY);
    } else
    {
      // Not a locale object, let the control handle it!
      return pfnwpOldProc(hwnd, msg, mp1, mp2);
    }
  }

  if (msg == DM_DROP)
  {
    if (internal_IsLocaleObject((PDRAGINFO) mp1))
    {
      HWND hwndParent;
      Page1UserData_p pUserData;

      // Locale object!
      hwndParent = WinQueryWindow(hwnd, QW_PARENT);
      pUserData = (Page1UserData_p) WinQueryWindowULong(hwndParent, QWL_USER);

      // Actually, the parent can be all three pages, but we only
      // want the Desktop handle from it, an it's the first entry at every
      // pageuserdata structure.
      if (pUserData)
	internal_SetLanguageFromLocaleObject(pUserData->Desktop, (PDRAGINFO) mp1);

      return MRFROM2SHORT(DOR_DROP, DO_COPY);
    } else
    {
      // Not a locale object, let the control handle it!
      return pfnwpOldProc(hwnd, msg, mp1, mp2);
    }
  }

  // Not a message we're interested in, so pass it to the original msg handler.
  return pfnwpOldProc(hwnd, msg, mp1, mp2);
}

static void SubclassChildWindowsForNLS(HWND hwnd)
{
  HWND hwndChild;
  PFNWP pfnwpOldProc;
  HENUM henum;

  henum = WinBeginEnumWindows(hwnd);

  while ((hwndChild = WinGetNextWindow(henum))!=NULLHANDLE)
  {
    pfnwpOldProc = WinSubclassWindow(hwndChild, fnwpNLSEnabledControlWindowProc);
    WinSetWindowULong(hwndChild, QWL_USER, (ULONG) pfnwpOldProc);
  }

  WinEndEnumWindows(henum);
}

/*****************************************************************************/
/*****************************************************************************/

static LONG GetStaticTextWidth(HWND hStatic)
{
  HPS     hps;
  char    text[256];
  POINTL  aptl[TXTBOX_BOTTOMRIGHT]; // this is 3 POINTLs

  WinQueryWindowText(hStatic, sizeof(text), text);

  hps = WinGetPS(hStatic);
  GpiQueryTextBox(hps, strlen(text), text, TXTBOX_BOTTOMRIGHT, aptl);
  WinReleasePS(hps);

  return aptl[TXTBOX_TOPRIGHT].x - aptl[TXTBOX_TOPLEFT].x;
}

/*****************************************************************************/

static void SetPage3ModuleInfo(HWND hwnd)
{
  long  lID;
  FILE *hfNLSFile;
  Page3UserData_p pUserData;
  char  szText[256];

  pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (!pUserData)
    return;

  hfNLSFile = internal_OpenNLSFile(pUserData->Desktop);

  lID = (long) WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_QUERYSELECTION,
                                 (MPARAM) LIT_CURSOR, (MPARAM) NULL);
  if (lID!=LIT_NONE)
  {
    lID = (long) WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_QUERYITEMHANDLE, (MPARAM) lID, (MPARAM) NULL);
    if ((lID<0) || lID>=pUserData->ulMaxModules)
    {
      // Something is wrong, the handle, which should tell the index in the
      // module desc array, points out of the array!
      lID = LIT_NONE;
    }
  }

  if (lID==LIT_NONE)
  {
    // Either there is no module selected, or something went wrong.
    WinSetDlgItemText(hwnd, GB_MODULEINFORMATION, "");

    if (!hfNLSFile || !sprintmsg(szText, hfNLSFile, "PG3_0014"))
      strcpy(szText, "No module selected!\nPlease select a screen saver module!");
    WinSetDlgItemText(hwnd, MLE_SELECTEDMODULEDESCRIPTION, szText);

    WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE), FALSE);
    StopPage3Preview(hwnd);
    internal_CloseNLSFile(hfNLSFile);
    return;
  }

  // combine the name and version to form the groupbox heading
  sprintf(szText, "%s v%d.%02d",
          pUserData->pModuleDescArray[lID].achModuleName,
          pUserData->pModuleDescArray[lID].lVersionMajor,
          pUserData->pModuleDescArray[lID].lVersionMinor);
  WinSetDlgItemText(hwnd, GB_MODULEINFORMATION, szText);

  WinSetDlgItemText(hwnd, MLE_SELECTEDMODULEDESCRIPTION,
                    pUserData->pModuleDescArray[lID].achModuleDesc);

  WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE),
                  pUserData->pModuleDescArray[lID].bConfigurable);

  if (WinQueryButtonCheckstate(hwnd, CB_SHOWPREVIEW))
  {
    if (WinIsWindowVisible(hwnd))
      StartPage3Preview(hwnd);
    else
      StopPage3Preview(hwnd);
  }

  internal_CloseNLSFile(hfNLSFile);
}

/*****************************************************************************/

static void SetPage1Text(HWND hwnd)
{
  HWND hParent;
  FILE *hfNLSFile;
  char *pchTemp;
  Page1UserData_p pUserData;

  if (!(pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER)) ||
      !(hfNLSFile = internal_OpenNLSFile(pUserData->Desktop)) ||
      !(pchTemp = (char *) malloc(1024)))
    return;

  // Okay, read strings and change control texts!

  // First the Undo/Default/Help buttons!  During init, they're
  // children of the dialog; later they become children of the notebook
  hParent = WinWindowFromID(hwnd, PB_NOTEBOOK_PG1_HELP) ? hwnd : hwndSettingsNotebook;
  if (sprintmsg(pchTemp, hfNLSFile, "COM_0004"))
  {
    WinSetDlgItemText(hParent, PB_NOTEBOOK_PG1_HELP, pchTemp);
    WinSendDlgItemMsg(hParent, PB_NOTEBOOK_PG1_HELP, BM_AUTOSIZE, 0, 0);
  }
  if (sprintmsg(pchTemp, hfNLSFile, "COM_0005"))
  {
    WinSetDlgItemText(hParent, PB_NOTEBOOK_PG1_DEFAULT, pchTemp);
    WinSendDlgItemMsg(hParent, PB_NOTEBOOK_PG1_DEFAULT, BM_AUTOSIZE, 0, 0);
  }
  if (sprintmsg(pchTemp, hfNLSFile, "COM_0006"))
  {
    WinSetDlgItemText(hParent, PB_NOTEBOOK_PG1_UNDO, pchTemp);
    WinSendDlgItemMsg(hParent, PB_NOTEBOOK_PG1_UNDO, BM_AUTOSIZE, 0, 0);
  }

  // Then the controls
  if (sprintmsg(pchTemp, hfNLSFile, "PG1_0003"))
    WinSetDlgItemText(hwnd, GB_GENERALSETTINGS, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG1_0004"))
    WinSetDlgItemText(hwnd, CB_ENABLED, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG1_0005"))
    WinSetDlgItemText(hwnd, ST_STARTSAVERAFTER, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG1_0006"))
    WinSetDlgItemText(hwnd, ST_MINUTESOFINACTIVITY, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG1_0007"))
    WinSetDlgItemText(hwnd, CB_WAKEUPBYKEYBOARDONLY, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG1_0008"))
    WinSetDlgItemText(hwnd, CB_DISABLEVIOPOPUPSWHILESAVING, pchTemp);

  if (sprintmsg(pchTemp, hfNLSFile, "PG1_0013"))
    WinSetDlgItemText(hwnd, GB_DPMSSETTINGS, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG1_0014"))
    WinSetDlgItemText(hwnd, CB_USEDPMSSTANDBYSTATE, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG1_0015"))
    WinSetDlgItemText(hwnd, ST_SWITCHTOSTANDBYSTATEAFTER, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG1_0016"))
    WinSetDlgItemText(hwnd, CB_USEDPMSSUSPENDSTATE, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG1_0017"))
    WinSetDlgItemText(hwnd, ST_SWITCHTOSUSPENDSTATEAFTER, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG1_0018"))
    WinSetDlgItemText(hwnd, CB_USEDPMSOFFSTATE, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG1_0019"))
    WinSetDlgItemText(hwnd, ST_SWITCHTOOFFSTATEAFTER, pchTemp);

  free(pchTemp);
  internal_CloseNLSFile(hfNLSFile);
  WinInvalidateRect(hwndSettingsNotebook, NULL, TRUE);
}

/*****************************************************************************/

static void SetPage2Text(HWND hwnd)
{
  HWND hParent;
  FILE *hfNLSFile;
  char *pchTemp;
  Page2UserData_p pUserData;

  if (!(pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER)) ||
      !(hfNLSFile = internal_OpenNLSFile(pUserData->Desktop)) ||
      !(pchTemp = (char *) malloc(1024)))
    return;

  // Okay, read strings and change control texts!

  // First the Undo/Default/Help buttons!
  hParent = WinWindowFromID(hwnd, PB_NOTEBOOK_PG2_HELP) ? hwnd : hwndSettingsNotebook;

  if (sprintmsg(pchTemp, hfNLSFile, "COM_0004"))
  {
    WinSetDlgItemText(hParent, PB_NOTEBOOK_PG2_HELP, pchTemp);
    WinSendDlgItemMsg(hParent, PB_NOTEBOOK_PG2_HELP, BM_AUTOSIZE, 0, 0);
  }
  if (sprintmsg(pchTemp, hfNLSFile, "COM_0005"))
  {
    WinSetDlgItemText(hParent, PB_NOTEBOOK_PG2_DEFAULT, pchTemp);
    WinSendDlgItemMsg(hParent, PB_NOTEBOOK_PG2_DEFAULT, BM_AUTOSIZE, 0, 0);
  }
  if (sprintmsg(pchTemp, hfNLSFile, "COM_0006"))
  {
    WinSetDlgItemText(hParent, PB_NOTEBOOK_PG2_UNDO, pchTemp);
    WinSendDlgItemMsg(hParent, PB_NOTEBOOK_PG2_UNDO, BM_AUTOSIZE, 0, 0);
  }

  if (sprintmsg(pchTemp, hfNLSFile, "PG2_0007"))
    WinSetDlgItemText(hwnd, GB_PASSWORDPROTECTION, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG2_0008"))
    WinSetDlgItemText(hwnd, CB_USEPASSWORDPROTECTION, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG2_0009"))
    WinSetDlgItemText(hwnd, ST_TYPEYOURPASSWORD, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG2_0010"))
  {
    WinSetDlgItemText(hwnd, ST_PASSWORD, pchTemp);
    WinSetDlgItemText(hwnd, ST_PASSWORDFORVERIFICATION, pchTemp);
  }
  if (sprintmsg(pchTemp, hfNLSFile, "PG2_0011"))
    WinSetDlgItemText(hwnd, ST_FORVERIFICATION, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG2_0012"))
    WinSetDlgItemText(hwnd, PB_CHANGE, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG2_0013"))
    WinSetDlgItemText(hwnd, CB_DELAYPASSWORDPROTECTION, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG2_0014"))
    WinSetDlgItemText(hwnd, ST_ASKPASSWORDONLYAFTER, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG2_0015"))
    WinSetDlgItemText(hwnd, ST_MINUTESOFSAVING, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG2_0016"))
    WinSetDlgItemText(hwnd, CB_STARTSAVERMODULEONSTARTUP, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG2_0019"))
    WinSetDlgItemText(hwnd, RB_USEPASSWORDOFCURRENTSECURITYUSER, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG2_0020"))
    WinSetDlgItemText(hwnd, RB_USETHISPASSWORD, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG2_0022"))
    WinSetDlgItemText(hwnd, CB_MAKETHEFIRSTKEYPRESSTHEFIRSTKEYOFTHEPASSWORD, pchTemp);

  free(pchTemp);
  internal_CloseNLSFile(hfNLSFile);

  // Also, we have to set the dynamic text, too, so:
  EnableDisablePage2Controls(hwnd);
  WinInvalidateRect(hwndSettingsNotebook, NULL, TRUE);
}

/*****************************************************************************/

static void SetPage3Text(HWND hwnd)
{
  HWND hParent;
  FILE *hfNLSFile;
  char *pchTemp;
  Page3UserData_p pUserData;

  if (!(pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER)) ||
      !(hfNLSFile = internal_OpenNLSFile(pUserData->Desktop)) ||
      !(pchTemp = (char *) malloc(1024)))
    return;

  // Okay, read strings and change control texts!

  // First the Undo/Default/Help buttons!
  hParent = WinWindowFromID(hwnd, PB_NOTEBOOK_PG3_HELP) ? hwnd : hwndSettingsNotebook;
  if (sprintmsg(pchTemp, hfNLSFile, "COM_0004"))
  {
    WinSetDlgItemText(hParent, PB_NOTEBOOK_PG3_HELP, pchTemp);
    WinSendDlgItemMsg(hParent, PB_NOTEBOOK_PG3_HELP, BM_AUTOSIZE, 0, 0);
  }
  if (sprintmsg(pchTemp, hfNLSFile, "COM_0005"))
  {
    WinSetDlgItemText(hParent, PB_NOTEBOOK_PG3_DEFAULT, pchTemp);
    WinSendDlgItemMsg(hParent, PB_NOTEBOOK_PG3_DEFAULT, BM_AUTOSIZE, 0, 0);
  }
  if (sprintmsg(pchTemp, hfNLSFile, "COM_0006"))
  {
    WinSetDlgItemText(hParent, PB_NOTEBOOK_PG3_UNDO, pchTemp);
    WinSendDlgItemMsg(hParent, PB_NOTEBOOK_PG3_UNDO, BM_AUTOSIZE, 0, 0);
  }

  // Then the controls
  if (sprintmsg(pchTemp, hfNLSFile, "PG3_0003"))
    WinSetDlgItemText(hwnd, GB_SCREENSAVERMODULES, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG3_0004"))
    WinSetDlgItemText(hwnd, CB_SHOWPREVIEW, pchTemp);

  WinSetDlgItemText(hwnd, GB_MODULEINFORMATION, "");
  if (sprintmsg(pchTemp, hfNLSFile, "PG3_0012"))
    WinSetDlgItemText(hwnd, PB_STARTMODULE, pchTemp);
  if (sprintmsg(pchTemp, hfNLSFile, "PG3_0013"))
    WinSetDlgItemText(hwnd, PB_CONFIGURE, pchTemp);

  free(pchTemp);
  internal_CloseNLSFile(hfNLSFile);

  // Also, we have to set the dynamic text, too, so:
  SetPage3ModuleInfo(hwnd);
  WinInvalidateRect(hwndSettingsNotebook, NULL, TRUE);
}

/*****************************************************************************/

static void ResizePage1Controls(HWND hwnd)
{
  LONG    tmp;
  LONG    xS;
  LONG    xR;
  LONG    cxL;
  LONG    cxR;
  HWND    hCtl;
  POINTL  ptl = {4, 8};
  SWP     swpLtxt;
  SWP     swpRtxt;
  SWP     swpSpin;

  // get the width & height in pixels of an average char 
  WinMapDlgPoints(hwnd, &ptl, 1, TRUE);

  // timeout
  hCtl = WinWindowFromID(hwnd, ST_STARTSAVERAFTER);
  WinQueryWindowPos(hCtl, &swpLtxt);
  cxL = GetStaticTextWidth(hCtl);
  WinSetWindowPos(hCtl, 0, 0, 0, cxL, swpLtxt.cy, SWP_SIZE);

  hCtl = WinWindowFromID(hwnd, SPB_TIMEOUT);
  WinQueryWindowPos(hCtl, &swpSpin);
  xS = swpLtxt.x + cxL + ptl.x;
  WinSetWindowPos(hCtl, 0, xS, swpSpin.y, 0, 0, SWP_MOVE);

  hCtl = WinWindowFromID(hwnd, ST_MINUTESOFINACTIVITY);
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_MINUTESOFINACTIVITY), &swpRtxt);
  xR = xS + swpSpin.cx + ptl.x;
  cxR = swpRtxt.x + swpRtxt.cx - xR;
  WinSetWindowPos(hCtl, 0, xR, swpRtxt.y, cxR, swpRtxt.cy, SWP_SIZE | SWP_MOVE);

  // make the text to the left of the DPMS spinbuttons a uniform width;
  // the text to the right will be resized below to use all remaining space
  cxL = 0;
  tmp = GetStaticTextWidth(WinWindowFromID(hwnd, ST_SWITCHTOSTANDBYSTATEAFTER));
  cxL = tmp > cxL ? tmp : cxL;
  tmp = GetStaticTextWidth(WinWindowFromID(hwnd, ST_SWITCHTOSUSPENDSTATEAFTER));
  cxL = tmp > cxL ? tmp : cxL;
  tmp = GetStaticTextWidth(WinWindowFromID(hwnd, ST_SWITCHTOOFFSTATEAFTER));
  cxL = tmp > cxL ? tmp : cxL;

  // standby timeout
  hCtl = WinWindowFromID(hwnd, ST_SWITCHTOSTANDBYSTATEAFTER);
  WinQueryWindowPos(hCtl, &swpLtxt);
  WinSetWindowPos(hCtl, 0, 0, 0, cxL, swpLtxt.cy, SWP_SIZE);

  hCtl = WinWindowFromID(hwnd, SPB_STANDBYTIMEOUT);
  WinQueryWindowPos(hCtl, &swpSpin);
  xS = swpLtxt.x + cxL + ptl.x;
  WinSetWindowPos(hCtl, 0, xS, swpLtxt.y, 0, 0, SWP_MOVE);

  hCtl = WinWindowFromID(hwnd, ST_STANDBYMINUTESTEXT);
  WinQueryWindowPos(hCtl, &swpRtxt);
  xR = xS + swpSpin.cx + ptl.x;
  cxR = swpRtxt.x + swpRtxt.cx - xR;
  WinSetWindowPos(hCtl, 0, xR, swpLtxt.y, cxR, swpLtxt.cy, SWP_SIZE | SWP_MOVE);

  // at this point, the x and cx values for all controls are known and can be reused;
  // we only need to call WinQueryWindowPos() once per line to get its y & cy values

  // suspend timeout
  hCtl = WinWindowFromID(hwnd, ST_SWITCHTOSUSPENDSTATEAFTER);
  WinQueryWindowPos(hCtl, &swpLtxt);
  WinSetWindowPos(hCtl, 0, 0, 0, cxL, swpLtxt.cy, SWP_SIZE);

  hCtl = WinWindowFromID(hwnd, SPB_SUSPENDTIMEOUT);
  WinSetWindowPos(hCtl, 0, xS, swpLtxt.y, 0, 0, SWP_MOVE);

  hCtl = WinWindowFromID(hwnd, ST_SUSPENDMINUTESTEXT);
  WinSetWindowPos(hCtl, 0, xR, swpLtxt.y, cxR, swpLtxt.cy, SWP_SIZE | SWP_MOVE);

  // off timeout
  hCtl = WinWindowFromID(hwnd, ST_SWITCHTOOFFSTATEAFTER);
  WinQueryWindowPos(hCtl, &swpLtxt);
  WinSetWindowPos(hCtl, 0, 0, 0, cxL, swpLtxt.cy, SWP_SIZE);

  hCtl = WinWindowFromID(hwnd, SPB_OFFTIMEOUT);
  WinSetWindowPos(hCtl, 0, xS, swpLtxt.y, 0, 0, SWP_MOVE);

  hCtl = WinWindowFromID(hwnd, ST_OFFMINUTESTEXT);
  WinSetWindowPos(hCtl, 0, xR, swpLtxt.y, cxR, swpLtxt.cy, SWP_SIZE | SWP_MOVE);

  return;
}

/*****************************************************************************/

static void ResizePage2Controls(HWND hwnd)
{
  LONG    x;
  LONG    cx;
  HWND    hCtl;
  POINTL  ptl = {4, 8}; // avg character size in dlg units
  SWP     swpLtxt;
  SWP     swpRtxt;
  SWP     swpSpin;

  // get the width & height in pixels of an average char 
  WinMapDlgPoints(hwnd, &ptl, 1, TRUE);

  // timeout
  hCtl = WinWindowFromID(hwnd, ST_ASKPASSWORDONLYAFTER);
  WinQueryWindowPos(hCtl, &swpLtxt);
  cx = GetStaticTextWidth(hCtl);
  WinSetWindowPos(hCtl, 0, 0, 0, cx, swpLtxt.cy, SWP_SIZE);

  hCtl = WinWindowFromID(hwnd, SPB_PWDDELAYMIN);
  WinQueryWindowPos(hCtl, &swpSpin);
  x = swpLtxt.x + cx + ptl.x;
  WinSetWindowPos(hCtl, 0, x, swpSpin.y, 0, 0, SWP_MOVE);

  hCtl = WinWindowFromID(hwnd, ST_MINUTESOFSAVING);
  WinQueryWindowPos(hCtl, &swpRtxt);
  x += swpSpin.cx + ptl.x;
  cx = swpRtxt.x + swpRtxt.cx - x;
  WinSetWindowPos(hCtl, 0, x, swpRtxt.y, cx, swpRtxt.cy, SWP_SIZE | SWP_MOVE);

  return;
}

/*****************************************************************************/

static void SetPage1ControlLimits(HWND hwnd)
{
  WinSendDlgItemMsg(hwnd, SPB_TIMEOUT,
                    SPBM_SETLIMITS,
                    (MPARAM) MAX_SAVER_TIMEOUT,
                    (MPARAM) MIN_SAVER_TIMEOUT);

  WinSendDlgItemMsg(hwnd, SPB_STANDBYTIMEOUT,
                    SPBM_SETLIMITS,
                    (MPARAM) MAX_DPMS_TIMEOUT,
                    (MPARAM) MIN_DPMS_TIMEOUT);

  WinSendDlgItemMsg(hwnd, SPB_SUSPENDTIMEOUT,
                    SPBM_SETLIMITS,
                    (MPARAM) MAX_DPMS_TIMEOUT,
                    (MPARAM) MIN_DPMS_TIMEOUT);

  WinSendDlgItemMsg(hwnd, SPB_OFFTIMEOUT,
                    SPBM_SETLIMITS,
                    (MPARAM) MAX_DPMS_TIMEOUT,
                    (MPARAM) MIN_DPMS_TIMEOUT);
}

static void SetPage2ControlLimits(HWND hwnd)
{
  WinSendDlgItemMsg(hwnd, SPB_PWDDELAYMIN,
                    SPBM_SETLIMITS,
                    (MPARAM) MAX_SAVER_TIMEOUT,
                    (MPARAM) MIN_SAVER_TIMEOUT);

  WinSendDlgItemMsg(hwnd, EF_PASSWORD,
                    EM_SETTEXTLIMIT,
                    (MPARAM) SSCORE_MAXPASSWORDLEN,
                    (MPARAM) NULL);

  WinSendDlgItemMsg(hwnd, EF_PASSWORD2,
                    EM_SETTEXTLIMIT,
                    (MPARAM) SSCORE_MAXPASSWORDLEN,
                    (MPARAM) NULL);
}

/*****************************************************************************/

static void SetPage1ControlValues(HWND hwnd)
{
  Page1UserData_p pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);

  if (!pUserData)
    return;

  WinCheckButton(hwnd, CB_ENABLED, wpssdesktop_wpssQueryScreenSaverEnabled(pUserData->Desktop));
  WinSendDlgItemMsg(hwnd, SPB_TIMEOUT, SPBM_SETCURRENTVALUE,
          (MPARAM) (wpssdesktop_wpssQueryScreenSaverTimeout(pUserData->Desktop)/60000), 0);
  WinCheckButton(hwnd, CB_WAKEUPBYKEYBOARDONLY, wpssdesktop_wpssQueryWakeByKeyboardOnly(pUserData->Desktop));
  WinCheckButton(hwnd, CB_DISABLEVIOPOPUPSWHILESAVING, wpssdesktop_wpssQueryDisableVIOPopupsWhileSaving(pUserData->Desktop));

  WinCheckButton(hwnd, CB_USEDPMSSTANDBYSTATE,
                 wpssdesktop_wpssQueryUseDPMSStandby(pUserData->Desktop));
  WinSendDlgItemMsg(hwnd, SPB_STANDBYTIMEOUT, SPBM_SETCURRENTVALUE,
          (MPARAM) (wpssdesktop_wpssQueryDPMSStandbyTimeout(pUserData->Desktop)/60000), 0);

  WinCheckButton(hwnd, CB_USEDPMSSUSPENDSTATE,
                 wpssdesktop_wpssQueryUseDPMSSuspend(pUserData->Desktop));
  WinSendDlgItemMsg(hwnd, SPB_SUSPENDTIMEOUT, SPBM_SETCURRENTVALUE,
          (MPARAM) (wpssdesktop_wpssQueryDPMSSuspendTimeout(pUserData->Desktop)/60000), 0);

  WinCheckButton(hwnd, CB_USEDPMSOFFSTATE,
                 wpssdesktop_wpssQueryUseDPMSOff(pUserData->Desktop));
  WinSendDlgItemMsg(hwnd, SPB_OFFTIMEOUT, SPBM_SETCURRENTVALUE,
          (MPARAM) (wpssdesktop_wpssQueryDPMSOffTimeout(pUserData->Desktop)/60000), 0);

  EnableDisablePage1Controls(hwnd);
}

/*****************************************************************************/

static void SetPage2ControlValues(HWND hwnd)
{
  Page2UserData_p pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  char achTemp[SSCORE_MAXPASSWORDLEN];

  if (!pUserData)
    return;

  WinCheckButton(hwnd, CB_USEPASSWORDPROTECTION, wpssdesktop_wpssQueryScreenSaverPasswordProtected(pUserData->Desktop));
  wpssdesktop_wpssQueryScreenSaverEncryptedPassword(pUserData->Desktop, achTemp, sizeof(achTemp));
  WinSetDlgItemText(hwnd, EF_PASSWORD, achTemp);
  WinSetDlgItemText(hwnd, EF_PASSWORD2, achTemp);
  WinCheckButton(hwnd, CB_DELAYPASSWORDPROTECTION, wpssdesktop_wpssQueryDelayedPasswordProtection(pUserData->Desktop));
  if (wpssdesktop_wpssQueryUseCurrentSecurityPassword(pUserData->Desktop))
  {
    WinCheckButton(hwnd, RB_USEPASSWORDOFCURRENTSECURITYUSER, TRUE);
    WinCheckButton(hwnd, RB_USETHISPASSWORD, FALSE);
  } else
  {
    WinCheckButton(hwnd, RB_USEPASSWORDOFCURRENTSECURITYUSER, FALSE);
    WinCheckButton(hwnd, RB_USETHISPASSWORD, TRUE);
  }
  WinSendDlgItemMsg(hwnd, SPB_PWDDELAYMIN, SPBM_SETCURRENTVALUE,
          (MPARAM) (wpssdesktop_wpssQueryDelayedPasswordProtectionTimeout(pUserData->Desktop)/60000),
          (MPARAM) NULL);
  WinCheckButton(hwnd, CB_MAKETHEFIRSTKEYPRESSTHEFIRSTKEYOFTHEPASSWORD, wpssdesktop_wpssQueryFirstKeyEventGoesToPwdWindow(pUserData->Desktop));
  WinCheckButton(hwnd, CB_STARTSAVERMODULEONSTARTUP, wpssdesktop_wpssQueryAutoStartAtStartup(pUserData->Desktop));

  EnableDisablePage2Controls(hwnd);
}

/*****************************************************************************/

static void SetPage3ControlValues(HWND hwnd, int bStartOrStopPreview)
{
  Page3UserData_p pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);

  char achCurrModule[CCHMAXPATHCOMP];
  ULONG ulTemp, ulCount;
  long lID, lIDToSelect;

  if (!pUserData)
    return;
  
  // Get current saver DLL module!
  wpssdesktop_wpssQueryScreenSaverModule(pUserData->Desktop,
                                         achCurrModule, sizeof(achCurrModule));
  AddLog("[SetPage3ControlValues] : Current saver module: [");
  AddLog(achCurrModule); AddLog("]\n");

  // Fill module list with available modules
  lIDToSelect = LIT_NONE;
  WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_DELETEALL, (MPARAM) NULL, (MPARAM) NULL);

  for (ulTemp=0; ulTemp<pUserData->ulMaxModules; ulTemp++)
  {
    lID = (long) WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_INSERTITEM,
                                   (MPARAM) LIT_SORTASCENDING,
                                   (MPARAM) (pUserData->pModuleDescArray[ulTemp].achModuleName));
    WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_SETITEMHANDLE,
                      (MPARAM) lID, (MPARAM) ulTemp);


    // This is the currently selected screen saver!
    // We might save the item ID, so we can select that item later.
    // The problem is that we're inserting items in sorted mode, so
    // item indexes will not be valid. So, we save item handle here, and
    // search for item with that handle later!
    if (!strcmp(pUserData->pModuleDescArray[ulTemp].achSaverDLLFileName, achCurrModule))
    {
      AddLog("[SetPage3ControlValues] : Found module!\n");
      lIDToSelect = ulTemp;
    }
  }

  if (lIDToSelect != LIT_NONE)
  {
    // Go through the items in the list, and select the one with lIDToSelect handle!
    ulCount = (ULONG) WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_QUERYITEMCOUNT, (MPARAM) NULL, (MPARAM) NULL);
    for (ulTemp = 0; ulTemp<ulCount; ulTemp++)
    {
      if ((ULONG)WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_QUERYITEMHANDLE, (MPARAM) ulTemp, (MPARAM) NULL) == lIDToSelect)
      {
        WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_SELECTITEM, (MPARAM) ulTemp, (MPARAM) TRUE);
        break;
      }
    }
  }

  ulTemp = wpssdesktop_wpssQueryScreenSaverPreviewEnabled(pUserData->Desktop);
  WinCheckButton(hwnd, CB_SHOWPREVIEW, ulTemp);

  if (bStartOrStopPreview)
  {
    if (ulTemp)
      StartPage3Preview(hwnd);
    else
      StopPage3Preview(hwnd);
  }

  // Ok, now set up module info!
  SetPage3ModuleInfo(hwnd);
}

/*****************************************************************************/

static void EnableDisablePage1Controls(HWND hwnd)
{
  SSCoreInfo_t  SSCoreInfo;
  int           bStandbyEnabled, bSuspendEnabled, bOffEnabled;
  BOOL          bOK;
  BOOL          bEnabled;
  Page1UserData_p pUserData;
  char         *pchTemp;
  char         *pNext;
  FILE         *hfNLSFile;

  pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (!pUserData)
    return;

  // Enable/Disable elements of first groupbox
  bEnabled = WinQueryButtonCheckstate(hwnd, CB_ENABLED);
  WinEnableWindow(WinWindowFromID(hwnd, ST_STARTSAVERAFTER), bEnabled);
  WinEnableWindow(WinWindowFromID(hwnd, SPB_TIMEOUT), bEnabled);
  WinEnableWindow(WinWindowFromID(hwnd, ST_MINUTESOFINACTIVITY), bEnabled);
  WinEnableWindow(WinWindowFromID(hwnd, CB_WAKEUPBYKEYBOARDONLY), bEnabled);
  WinEnableWindow(WinWindowFromID(hwnd, CB_DISABLEVIOPOPUPSWHILESAVING), bEnabled);

  // Get DPMS capabilities!
  if (SSCore_GetInfo(&SSCoreInfo, sizeof(SSCoreInfo))!=SSCORE_NOERROR)
    SSCoreInfo.iDPMSCapabilities = SSCORE_DPMSCAPS_NODPMS;

  // Get current settings!
  bStandbyEnabled = wpssdesktop_wpssQueryUseDPMSStandby(pUserData->Desktop);
  bSuspendEnabled = wpssdesktop_wpssQueryUseDPMSSuspend(pUserData->Desktop);
  bOffEnabled     = wpssdesktop_wpssQueryUseDPMSOff(pUserData->Desktop);

  // Enable/Disable Standby checkbox and spinbutton
  bOK = bEnabled &&
        (SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_STANDBY) ? TRUE : FALSE; 
  WinEnableWindow(WinWindowFromID(hwnd, CB_USEDPMSSTANDBYSTATE), bOK);
  WinShowWindow(WinWindowFromID(hwnd, ST_SWITCHTOSTANDBYSTATEAFTER), 
                bOK && bStandbyEnabled);
  WinShowWindow(WinWindowFromID(hwnd, SPB_STANDBYTIMEOUT),
                bOK && bStandbyEnabled);
  WinShowWindow(WinWindowFromID(hwnd, ST_STANDBYMINUTESTEXT),
                bOK && bStandbyEnabled);

  // Enable/Disable Suspend checkbox and spinbutton
  bOK = bEnabled &&
        (SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_SUSPEND) ? TRUE : FALSE; 
  WinEnableWindow(WinWindowFromID(hwnd, CB_USEDPMSSUSPENDSTATE), bOK);
  WinShowWindow(WinWindowFromID(hwnd, ST_SWITCHTOSUSPENDSTATEAFTER),
                 bOK && bSuspendEnabled);
  WinShowWindow(WinWindowFromID(hwnd, SPB_SUSPENDTIMEOUT),
                 bOK && bSuspendEnabled);
  WinShowWindow(WinWindowFromID(hwnd, ST_SUSPENDMINUTESTEXT),
                 bOK && bSuspendEnabled);

  // Enable/Disable Off checkbox and spinbutton
  bOK = bEnabled &&
        (SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_OFF) ? TRUE : FALSE; 
  WinEnableWindow(WinWindowFromID(hwnd, CB_USEDPMSOFFSTATE), bOK);
  WinShowWindow(WinWindowFromID(hwnd, ST_SWITCHTOOFFSTATEAFTER),
                bOK && bOffEnabled);
  WinShowWindow(WinWindowFromID(hwnd, SPB_OFFTIMEOUT),
                bOK && bOffEnabled);
  WinShowWindow(WinWindowFromID(hwnd, ST_OFFMINUTESTEXT),
                bOK && bOffEnabled);

  // Now update texts to reflect the order of states!
  hfNLSFile = internal_OpenNLSFile(pUserData->Desktop);
  if (hfNLSFile)
    pchTemp = (char *) malloc(1024);
  else
    pchTemp = NULL;

  if ((pchTemp) && (sprintmsg(pchTemp, hfNLSFile, "PG1_0020")))
    pNext = pchTemp;
  else
    pNext = "minute(s) of saving";

  if ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_STANDBY) &&
      bStandbyEnabled)
  {
    WinSetDlgItemText(hwnd, ST_STANDBYMINUTESTEXT, pNext);

    if ((pchTemp) && (sprintmsg(pchTemp, hfNLSFile, "PG1_0021")))
      pNext = pchTemp;
    else
      pNext = "minute(s) of Standby state";
  }

  if ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_SUSPEND) &&
      bSuspendEnabled)
  {
    WinSetDlgItemText(hwnd, ST_SUSPENDMINUTESTEXT, pNext);

    if ((pchTemp) && (sprintmsg(pchTemp, hfNLSFile, "PG1_0022")))
      pNext = pchTemp;
    else
      pNext = "minute(s) of Standby state";
  }

  if ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_OFF) &&
      bOffEnabled)
    WinSetDlgItemText(hwnd, ST_OFFMINUTESTEXT, pNext);

  internal_CloseNLSFile(hfNLSFile);

  if (pchTemp)
    free(pchTemp);
}

/*****************************************************************************/

static void EnableDisablePage2Controls(HWND hwnd)
{
  SSCoreInfo_t SSCoreInfo;
  int   bValue1, bValue2, bValue3, bEnabled;

  // Get info if Security/2 is installed or not!
  if (SSCore_GetInfo(&SSCoreInfo, sizeof(SSCoreInfo))!=SSCORE_NOERROR)
    SSCoreInfo.bSecurityPresent = FALSE;

  bValue1 = WinQueryButtonCheckstate(hwnd, CB_USEPASSWORDPROTECTION);
  bValue2 = WinQueryButtonCheckstate(hwnd, CB_DELAYPASSWORDPROTECTION);
  bValue3 = WinQueryButtonCheckstate(hwnd, RB_USETHISPASSWORD);

  // disable everything if DSS is not enabled
  // Note: wpssdesktop_wpssQueryScreenSaverEnabled() is used as a function call
  // not a method call, and doesn't use somSelf anyway, so a null object is OK
  bEnabled = wpssdesktop_wpssQueryScreenSaverEnabled(0);
  WinEnableWindow(WinWindowFromID(hwnd, CB_USEPASSWORDPROTECTION), bEnabled);
  bValue1 &= bEnabled;

  // Enable this only if Security/2 is present, and password protection is on
  WinEnableWindow(WinWindowFromID(hwnd, RB_USEPASSWORDOFCURRENTSECURITYUSER), bValue1 && (SSCoreInfo.bSecurityPresent));
  WinEnableWindow(WinWindowFromID(hwnd, RB_USETHISPASSWORD), bValue1);

  WinEnableWindow(WinWindowFromID(hwnd, ST_TYPEYOURPASSWORD), bValue1 && bValue3);
  WinEnableWindow(WinWindowFromID(hwnd, ST_PASSWORD), bValue1 && bValue3);
  WinEnableWindow(WinWindowFromID(hwnd, ST_PASSWORDFORVERIFICATION), bValue1 && bValue3);
  WinEnableWindow(WinWindowFromID(hwnd, ST_FORVERIFICATION), bValue1 && bValue3);
  WinEnableWindow(WinWindowFromID(hwnd, EF_PASSWORD), bValue1 && bValue3);
  WinEnableWindow(WinWindowFromID(hwnd, EF_PASSWORD2), bValue1 && bValue3);

  if ((!bValue1) || (!bValue3))
    WinEnableWindow(WinWindowFromID(hwnd, PB_CHANGE), bValue1 && bValue3);

  WinEnableWindow(WinWindowFromID(hwnd, CB_DELAYPASSWORDPROTECTION), bValue1);

  WinEnableWindow(WinWindowFromID(hwnd, ST_ASKPASSWORDONLYAFTER), bValue1 && bValue2);
  WinEnableWindow(WinWindowFromID(hwnd, SPB_PWDDELAYMIN), bValue1 && bValue2);
  WinEnableWindow(WinWindowFromID(hwnd, ST_MINUTESOFSAVING), bValue1 && bValue2);

  WinEnableWindow(WinWindowFromID(hwnd, CB_MAKETHEFIRSTKEYPRESSTHEFIRSTKEYOFTHEPASSWORD), bValue1);
  WinEnableWindow(WinWindowFromID(hwnd, CB_STARTSAVERMODULEONSTARTUP), bValue1);
}

/*****************************************************************************/

MRESULT EXPENTRY fnwpScreenSaverSettingsPage1(HWND hwnd, ULONG msg,
                                              MPARAM mp1, MPARAM mp2)
{
  Page1UserData_p pUserData;

  switch (msg)
  {
    case WM_INITDLG:
    {
      // Save window handle
      hwndSettingsPage1 = hwnd;

      // Initialize dialog window's private data
      pUserData = (Page1UserData_p) malloc(sizeof(Page1UserData_t));

      if (pUserData)
      {
        pUserData->Desktop = (WPSSDesktop *)mp2;
        pUserData->bPageSetUp = FALSE;
        WinSetWindowULong(hwnd, QWL_USER, (ULONG) pUserData);
        AddLog("[fnwpScreenSaverSettingsPage1] : WM_INITDLG : Memory allocated\n");

        // Fill Undo buffer
        pUserData->bUndoEnabled = wpssdesktop_wpssQueryScreenSaverEnabled(pUserData->Desktop);
        pUserData->iUndoActivationTime = wpssdesktop_wpssQueryScreenSaverTimeout(pUserData->Desktop);
        pUserData->bUndoWakeByKeyboardOnly = wpssdesktop_wpssQueryWakeByKeyboardOnly(pUserData->Desktop);

        pUserData->bUndoUseDPMSStandbyState = wpssdesktop_wpssQueryUseDPMSStandby(pUserData->Desktop);
        pUserData->iUndoDPMSStandbyStateTimeout = wpssdesktop_wpssQueryDPMSStandbyTimeout(pUserData->Desktop);
        pUserData->bUndoUseDPMSSuspendState = wpssdesktop_wpssQueryUseDPMSSuspend(pUserData->Desktop);
        pUserData->iUndoDPMSSuspendStateTimeout = wpssdesktop_wpssQueryDPMSSuspendTimeout(pUserData->Desktop);
        pUserData->bUndoUseDPMSOffState = wpssdesktop_wpssQueryUseDPMSOff(pUserData->Desktop);
        pUserData->iUndoDPMSOffStateTimeout = wpssdesktop_wpssQueryDPMSOffTimeout(pUserData->Desktop);
      }

      // set dialog font
      WinSetPresParam(hwnd, PP_FONTNAMESIZE, strlen(achFontToUse) + 1, achFontToUse);

      // Set text (NLS support)
      SetPage1Text(hwnd);

      // Resize controls
      ResizePage1Controls(hwnd);

      // Set the limits of the controls
      SetPage1ControlLimits(hwnd);

      // Set the initial values of controls
      SetPage1ControlValues(hwnd);

      // Subclass every child control, so it will pass DM_* messages to its
      // owner, so locale objects dropped to the controls change language!
      SubclassChildWindowsForNLS(hwnd);

      // Subclass the notebook control too. It is required so that if
      // we dynamically change the language, we can show the right help!
      SubclassNotebookWindowForNLS();

      if (pUserData)
        pUserData->bPageSetUp = TRUE;

      return (MRESULT) FALSE;
    }

    case WM_LANGUAGECHANGED:
      SetPage1Text(hwnd);
      ResizePage1Controls(hwnd);
      break;

    case WM_DESTROY:
      // Free private window memory!
      pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      WinSetWindowULong(hwnd, QWL_USER, 0);
      if (pUserData)
        free(pUserData);

      // Note that we're destroyed!
      hwndSettingsPage1 = NULLHANDLE;
      break;

    case WM_CONTROL:
    {
      pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (!pUserData)
        return (MRESULT) FALSE;

      switch (SHORT1FROMMP(mp1)) // Control window ID
      {
        case CB_ENABLED:
          if (SHORT2FROMMP(mp1)==BN_CLICKED)
          {
            wpssdesktop_wpssSetScreenSaverEnabled(pUserData->Desktop,
                                      WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
            EnableDisablePage1Controls(hwnd);
            if (hwndSettingsPage2)
              EnableDisablePage2Controls(hwndSettingsPage2);
          }
          break;

        case CB_DISABLEVIOPOPUPSWHILESAVING:
          if (SHORT2FROMMP(mp1)==BN_CLICKED)
            wpssdesktop_wpssSetDisableVIOPopupsWhileSaving(pUserData->Desktop,
                                        WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
          break;

        case CB_WAKEUPBYKEYBOARDONLY:
          if (SHORT2FROMMP(mp1)==BN_CLICKED)
            wpssdesktop_wpssSetWakeByKeyboardOnly(pUserData->Desktop,
                               WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
          break;

        case SPB_TIMEOUT:
          if (SHORT2FROMMP(mp1)==SPBN_CHANGE)
          {
            if (pUserData->bPageSetUp)
            {
              ULONG ulValue = 3; // Default, in case of problems.

              WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE,
                                (MPARAM) &ulValue, MPFROM2SHORT(0, SPBQ_ALWAYSUPDATE));
              wpssdesktop_wpssSetScreenSaverTimeout(pUserData->Desktop, ulValue*60000);
            }
          }
          break;

        case CB_USEDPMSSTANDBYSTATE:
          if (SHORT2FROMMP(mp1)==BN_CLICKED)
          {
            wpssdesktop_wpssSetUseDPMSStandby(pUserData->Desktop,
                                              WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
            EnableDisablePage1Controls(hwnd);
          }
          break;

        case CB_USEDPMSSUSPENDSTATE:
          if (SHORT2FROMMP(mp1)==BN_CLICKED)
          {
            wpssdesktop_wpssSetUseDPMSSuspend(pUserData->Desktop,
                                              WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
            EnableDisablePage1Controls(hwnd);
          }
          break;

        case CB_USEDPMSOFFSTATE:
          if (SHORT2FROMMP(mp1)==BN_CLICKED)
          {
            wpssdesktop_wpssSetUseDPMSOff(pUserData->Desktop,
                                          WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
            EnableDisablePage1Controls(hwnd);
          }
          break;

        case SPB_STANDBYTIMEOUT:
          if (SHORT2FROMMP(mp1)==SPBN_CHANGE)
          {
            if (pUserData->bPageSetUp)
            {
              ULONG ulValue = 5; // Default, in case of problems.

              WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE,
                                (MPARAM) &ulValue, MPFROM2SHORT(0, SPBQ_ALWAYSUPDATE));
              wpssdesktop_wpssSetDPMSStandbyTimeout(pUserData->Desktop, ulValue*60000);
            }
          }
          break;

        case SPB_SUSPENDTIMEOUT:
          if (SHORT2FROMMP(mp1)==SPBN_CHANGE)
          {
            if (pUserData->bPageSetUp)
            {
              ULONG ulValue = 5; // Default, in case of problems.

              WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE,
                                (MPARAM) &ulValue, MPFROM2SHORT(0, SPBQ_ALWAYSUPDATE));
              wpssdesktop_wpssSetDPMSSuspendTimeout(pUserData->Desktop, ulValue*60000);
            }
          }
          break;

        case SPB_OFFTIMEOUT:
          if (SHORT2FROMMP(mp1)==SPBN_CHANGE)
          {
            if (pUserData->bPageSetUp)
            {
              ULONG ulValue = 5; // Default, in case of problems.

              WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE,
                                (MPARAM) &ulValue, MPFROM2SHORT(0, SPBQ_ALWAYSUPDATE));
              wpssdesktop_wpssSetDPMSOffTimeout(pUserData->Desktop, ulValue*60000);
            }
          }
          break;

        default:
          break;
      }

      return (MRESULT) FALSE;
    }

    case WM_COMMAND:
    {
      pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (!pUserData)
        return (MRESULT) TRUE;

      switch (SHORT1FROMMP(mp1))
      {
        case PB_NOTEBOOK_PG1_UNDO:
          AddLog("[PB_NOTEBOOK_UNDO] : Pressed!\n");

          // Set back settings from UNDO buffer (saved at start of dialog)
          wpssdesktop_wpssSetScreenSaverEnabled(pUserData->Desktop, pUserData->bUndoEnabled);
          wpssdesktop_wpssSetScreenSaverTimeout(pUserData->Desktop, pUserData->iUndoActivationTime);
          wpssdesktop_wpssSetWakeByKeyboardOnly(pUserData->Desktop, pUserData->bUndoWakeByKeyboardOnly);

          wpssdesktop_wpssSetUseDPMSStandby(pUserData->Desktop,
                                            pUserData->bUndoUseDPMSStandbyState);
          wpssdesktop_wpssSetDPMSStandbyTimeout(pUserData->Desktop,
                                                pUserData->iUndoDPMSStandbyStateTimeout);
          wpssdesktop_wpssSetUseDPMSSuspend(pUserData->Desktop,
                                            pUserData->bUndoUseDPMSSuspendState);
          wpssdesktop_wpssSetDPMSSuspendTimeout(pUserData->Desktop,
                                                pUserData->iUndoDPMSSuspendStateTimeout);
          wpssdesktop_wpssSetUseDPMSOff(pUserData->Desktop,
                                        pUserData->bUndoUseDPMSOffState);
          wpssdesktop_wpssSetDPMSOffTimeout(pUserData->Desktop,
                                            pUserData->iUndoDPMSOffStateTimeout);

          // Set values of controls
          SetPage1ControlValues(hwnd);
          WinEnableWindow(WinWindowFromID(hwnd, PB_CHANGE), FALSE);
          break;

        case PB_NOTEBOOK_PG1_DEFAULT:
          AddLog("[PB_NOTEBOOK_DEFAULT] : Pressed!\n");

          // Set default settings
          wpssdesktop_wpssSetScreenSaverEnabled(pUserData->Desktop, FALSE);
          wpssdesktop_wpssSetScreenSaverTimeout(pUserData->Desktop, 3*60000);
          wpssdesktop_wpssSetWakeByKeyboardOnly(pUserData->Desktop, FALSE);

          wpssdesktop_wpssSetUseDPMSStandby(pUserData->Desktop, FALSE);
          wpssdesktop_wpssSetDPMSStandbyTimeout(pUserData->Desktop, 5*60000);
          wpssdesktop_wpssSetUseDPMSSuspend(pUserData->Desktop, FALSE);
          wpssdesktop_wpssSetDPMSSuspendTimeout(pUserData->Desktop, 5*60000);
          wpssdesktop_wpssSetUseDPMSOff(pUserData->Desktop, TRUE);
          wpssdesktop_wpssSetDPMSOffTimeout(pUserData->Desktop, 5*60000);

          // Set values of controls
          SetPage1ControlValues(hwnd);
          WinEnableWindow(WinWindowFromID(hwnd, PB_CHANGE), FALSE);
          break;

        case PB_NOTEBOOK_PG1_HELP:
          internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE1_GENERALSETTINGS);
          break;

        default:
          break;
      }

      return (MRESULT) TRUE;
    }

    case WM_HELP:
      pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pUserData)
        internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE1_GENERALSETTINGS);
      return (MRESULT) TRUE;

    // Handling Drag'n'Drop of Locale objects!
    case DM_DRAGOVER:
      if (internal_IsLocaleObject((PDRAGINFO) mp1))
        return MRFROM2SHORT(DOR_DROP, DO_COPY); // Fine, let it come!

      return MRFROM2SHORT(DOR_NODROPOP, 0); // We support it, but not in this form!

    case DM_DROP:
      if (internal_IsLocaleObject((PDRAGINFO) mp1))
      {
        pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
        if (pUserData)
          internal_SetLanguageFromLocaleObject(pUserData->Desktop, (PDRAGINFO) mp1);
        return MRFROM2SHORT(DOR_DROP, DO_COPY);
      }
      return MPFROM2SHORT(DOR_NODROPOP, 0);

    default:
      break;
  }

  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

/*****************************************************************************/

MRESULT EXPENTRY fnwpScreenSaverSettingsPage2(HWND hwnd, ULONG msg,
                                              MPARAM mp1, MPARAM mp2)
{
  Page2UserData_p pUserData;
  char achTemp[SSCORE_MAXPASSWORDLEN];

  switch (msg)
  {
    case WM_INITDLG:
    {
      // Save window handle
      hwndSettingsPage2 = hwnd;

      // Initialize dialog window's private data
      pUserData = (Page2UserData_p) malloc(sizeof(Page2UserData_t));

      if (pUserData)
      {
        pUserData->Desktop = (WPSSDesktop *)mp2;
        pUserData->bPageSetUp = FALSE;
        WinSetWindowULong(hwnd, QWL_USER, (ULONG) pUserData);
        AddLog("[fnwpScreenSaverSettingsPage2] : WM_INITDLG : Memory allocated\n");

        // Fill Undo buffer
        pUserData->bUndoPasswordProtected = wpssdesktop_wpssQueryScreenSaverPasswordProtected(pUserData->Desktop);
        wpssdesktop_wpssQueryScreenSaverEncryptedPassword(pUserData->Desktop,
                          pUserData->achUndoEncryptedPassword,
                          sizeof(pUserData->achUndoEncryptedPassword));
        pUserData->bUndoStartAtStartup = wpssdesktop_wpssQueryAutoStartAtStartup(pUserData->Desktop);
        pUserData->bUndoDelayedPasswordProtection = wpssdesktop_wpssQueryDelayedPasswordProtection(pUserData->Desktop);
        pUserData->iUndoDelayedPasswordProtectionTime = wpssdesktop_wpssQueryDelayedPasswordProtectionTimeout(pUserData->Desktop);
      }

      // set dialog font
      WinSetPresParam(hwnd, PP_FONTNAMESIZE, strlen(achFontToUse) + 1, achFontToUse);

      // Set text (NLS support)
      SetPage2Text(hwnd);

      // Resize controls
      ResizePage2Controls(hwnd);

      // Set the limits of the controls
      SetPage2ControlLimits(hwnd);

      // Set the initial values of controls
      SetPage2ControlValues(hwnd);

      // Subclass every child control, so it will pass DM_* messages to its
      // owner, so locale objects dropped to the controls change language!
      SubclassChildWindowsForNLS(hwnd);

      // Subclass the notebook control too. It is required so that if
      // we dinamically change the language, we can show the right help!
      SubclassNotebookWindowForNLS();

      WinEnableWindow(WinWindowFromID(hwnd, PB_CHANGE), FALSE);

      if (pUserData)
        pUserData->bPageSetUp = TRUE;

      return (MRESULT) FALSE;
    }

    case WM_LANGUAGECHANGED:
      SetPage2Text(hwnd);
      ResizePage2Controls(hwnd);
      break;

    case WM_DESTROY:
      // Free private window memory!
      pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      WinSetWindowULong(hwnd, QWL_USER, 0);
      if (pUserData)
        free(pUserData);

      // Note that we're destroyed!
      hwndSettingsPage2 = NULLHANDLE;
      break;

    case WM_SHOW:
      if (SHORT1FROMMP(mp1))
        EnableDisablePage2Controls(hwnd);
      return (MRESULT) FALSE;

    case WM_CONTROL:
    {
      pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (!pUserData)
        return (MRESULT) FALSE;

      switch (SHORT1FROMMP(mp1)) // Control window ID
      {
        case EF_PASSWORD:
        case EF_PASSWORD2:
          if (SHORT2FROMMP(mp1)==EN_SETFOCUS)
            WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), "");

          if (SHORT2FROMMP(mp1)==EN_KILLFOCUS)
          {
            WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1), sizeof(achTemp), achTemp);

            // Store back the encrypted password!
            wpssdesktop_wpssEncryptScreenSaverPassword(pUserData->Desktop, achTemp);
            WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achTemp);
            WinEnableWindow(WinWindowFromID(hwnd, PB_CHANGE),
                            WinQueryButtonCheckstate(hwnd, CB_USEPASSWORDPROTECTION));
          }
          break;

        case CB_USEPASSWORDPROTECTION:
          if (SHORT2FROMMP(mp1)==BN_CLICKED)
          {
            wpssdesktop_wpssSetScreenSaverPasswordProtected(pUserData->Desktop,
                                     WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
            EnableDisablePage2Controls(hwnd);
          }
          break;

        case CB_MAKETHEFIRSTKEYPRESSTHEFIRSTKEYOFTHEPASSWORD:
          if (SHORT2FROMMP(mp1)==BN_CLICKED)
            wpssdesktop_wpssSetFirstKeyEventGoesToPwdWindow(pUserData->Desktop,
                                           WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
          break;

        case CB_STARTSAVERMODULEONSTARTUP:
          if (SHORT2FROMMP(mp1)==BN_CLICKED)
            wpssdesktop_wpssSetAutoStartAtStartup(pUserData->Desktop,
                                          WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
          break;

        case CB_DELAYPASSWORDPROTECTION:
          if (SHORT2FROMMP(mp1)==BN_CLICKED)
          {
            wpssdesktop_wpssSetDelayedPasswordProtection(pUserData->Desktop,
                                         WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
            EnableDisablePage2Controls(hwnd);
          }
          break;

        case RB_USEPASSWORDOFCURRENTSECURITYUSER:
          if (SHORT2FROMMP(mp1)==BN_CLICKED)
          {
            wpssdesktop_wpssSetUseCurrentSecurityPassword(pUserData->Desktop, TRUE);
            EnableDisablePage2Controls(hwnd);
          }
          break;

        case RB_USETHISPASSWORD:
          if (SHORT2FROMMP(mp1)==BN_CLICKED)
          {
            wpssdesktop_wpssSetUseCurrentSecurityPassword(pUserData->Desktop, FALSE);
            EnableDisablePage2Controls(hwnd);
          }
          break;

        case SPB_PWDDELAYMIN:
          if (SHORT2FROMMP(mp1)==SPBN_CHANGE)
          {
            if (pUserData->bPageSetUp)
            {
              ULONG ulValue = 1; // Default, in case of problems.

              WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE,
                                (MPARAM) &ulValue, MPFROM2SHORT(0, SPBQ_ALWAYSUPDATE));
              wpssdesktop_wpssSetDelayedPasswordProtectionTimeout(pUserData->Desktop,
                                                                  ulValue*60000);
            }
          }
          break;

        default:
          break;
      }

      return (MRESULT) FALSE;
    }

    case WM_COMMAND:
    {
      pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (!pUserData)
        return (MRESULT) TRUE;

      switch (SHORT1FROMMP(mp1))
      {
        case PB_CHANGE:
          ChangeSaverPassword(hwnd);
          break;

        case PB_NOTEBOOK_PG2_UNDO:
          AddLog("[PB_NOTEBOOK_UNDO] : Pressed!\n");

          // Set back settings from UNDO buffer (saved at start of dialog)
          wpssdesktop_wpssSetScreenSaverPasswordProtected(pUserData->Desktop, pUserData->bUndoPasswordProtected);
          wpssdesktop_wpssSetScreenSaverEncryptedPassword(pUserData->Desktop,
                                                          pUserData->achUndoEncryptedPassword);
          wpssdesktop_wpssSetAutoStartAtStartup(pUserData->Desktop, pUserData->bUndoStartAtStartup);
          wpssdesktop_wpssSetDelayedPasswordProtection(pUserData->Desktop, pUserData->bUndoDelayedPasswordProtection);
          wpssdesktop_wpssSetDelayedPasswordProtectionTimeout(pUserData->Desktop, pUserData->iUndoDelayedPasswordProtectionTime);

          // Set values of controls
          SetPage2ControlValues(hwnd);
          WinEnableWindow(WinWindowFromID(hwnd, PB_CHANGE), FALSE);
          break;

        case PB_NOTEBOOK_PG2_DEFAULT:
          AddLog("[PB_NOTEBOOK_DEFAULT] : Pressed!\n");

          // Set default settings
          wpssdesktop_wpssSetScreenSaverPasswordProtected(pUserData->Desktop, FALSE);
          wpssdesktop_wpssSetScreenSaverEncryptedPassword(pUserData->Desktop, "");
          wpssdesktop_wpssSetAutoStartAtStartup(pUserData->Desktop, FALSE);
          wpssdesktop_wpssSetDelayedPasswordProtection(pUserData->Desktop, TRUE);
          wpssdesktop_wpssSetDelayedPasswordProtectionTimeout(pUserData->Desktop, 5*60000);
          wpssdesktop_wpssSetFirstKeyEventGoesToPwdWindow(pUserData->Desktop, FALSE);

          // Set values of controls
          SetPage2ControlValues(hwnd);
          WinEnableWindow(WinWindowFromID(hwnd, PB_CHANGE), FALSE);
          break;

        case PB_NOTEBOOK_PG2_HELP:
          internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE2_PASSWORDPROTECTION);
          break;

        default:
          break;
      }

      return (MRESULT) TRUE;
    }

    case WM_HELP:
      pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pUserData)
        internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE2_PASSWORDPROTECTION);
      return (MRESULT) TRUE;

    // Handling Drag'n'Drop of Locale objects!
    case DM_DRAGOVER:
      if (internal_IsLocaleObject((PDRAGINFO) mp1))
        return MRFROM2SHORT(DOR_DROP, DO_COPY); // Fine, let it come!

      return MRFROM2SHORT(DOR_NODROPOP, 0); // We support it, but not in this form!

    case DM_DROP:
      if (internal_IsLocaleObject((PDRAGINFO) mp1))
      {
        pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
        if (pUserData)
          internal_SetLanguageFromLocaleObject(pUserData->Desktop, (PDRAGINFO) mp1);
        return MRFROM2SHORT(DOR_DROP, DO_COPY);
      }
      return MPFROM2SHORT(DOR_NODROPOP, 0);

    default:
      break;
  }

  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

/*****************************************************************************/


MRESULT EXPENTRY fnwpScreenSaverSettingsPage3(HWND hwnd, ULONG msg,
                                              MPARAM mp1, MPARAM mp2)
{
  Page3UserData_p pUserData;
  HWND hwndPreview;
  long lID;

  switch (msg)
  {
    case WM_INITDLG:
    {
      SWP   swp;

      // Save window handle
      hwndSettingsPage3 = hwnd;

      // Initialize dialog window's private data
      pUserData = (Page3UserData_p) malloc(sizeof(Page3UserData_t));
      if (pUserData)
      {
        AddLog("[fnwpScreenSaverSettingsPage3] : WM_INITDLG : Memory allocated\n");

        WinSetWindowULong(hwnd, QWL_USER, (ULONG) pUserData);

        pUserData->Desktop = (WPSSDesktop *)mp2;
        pUserData->bPageSetUp = FALSE;

        pUserData->iPreviewMsgCounter = 0; // Number of Start/Stop preview commands pending
        pUserData->bPreviewRunning = 0;    // Is preview running?

        /* Querying the list of available modules may take a while */
        WinSetPointer(HWND_DESKTOP, WinQuerySysPointer(HWND_DESKTOP, SPTR_WAIT, FALSE));
        pUserData->ulMaxModules =
              wpssdesktop_wpssQueryNumOfAvailableScreenSaverModules(pUserData->Desktop);
        WinSetPointer(HWND_DESKTOP, WinQuerySysPointer(HWND_DESKTOP, SPTR_ARROW, FALSE));

#ifdef DEBUG_LOGGING
        {
          char achTemp[128];
          sprintf(achTemp, "ulMaxModules = %d\n", pUserData->ulMaxModules);
          AddLog(achTemp);
        }
#endif
        pUserData->pModuleDescArray =
              (PSSMODULEDESC) malloc(pUserData->ulMaxModules * sizeof(SSMODULEDESC));
        if (pUserData->pModuleDescArray)
        {
          WinSetPointer(HWND_DESKTOP, WinQuerySysPointer(HWND_DESKTOP, SPTR_WAIT, FALSE));
          wpssdesktop_wpssQueryInfoAboutAvailableScreenSaverModules(pUserData->Desktop,
                                                                    pUserData->ulMaxModules,
                                                                    pUserData->pModuleDescArray);
          WinSetPointer(HWND_DESKTOP, WinQuerySysPointer(HWND_DESKTOP, SPTR_ARROW, FALSE));
        }
        else
          pUserData->ulMaxModules = 0;

        // Fill Undo buffer
        wpssdesktop_wpssQueryScreenSaverModule(pUserData->Desktop,
                                               pUserData->achUndoSaverDLLFileName,
                                               sizeof(pUserData->achUndoSaverDLLFileName));
      }

      // set dialog font
      WinSetPresParam(hwnd, PP_FONTNAMESIZE, strlen(achFontToUse) + 1, achFontToUse);

      // Create special child window
      if (!bWindowClassRegistered)
      {
        WinRegisterClass(WinQueryAnchorBlock(hwnd), (PSZ) PREVIEW_CLASS,
                         (PFNWP) PreviewWindowProc,
                         CS_SIZEREDRAW | CS_CLIPCHILDREN,
                         16); // Window data
        bWindowClassRegistered = TRUE;
      }

      // use the coordinates of the dummy text field
      // to position the preview window
      // the text field which in turn is aligned with the listbox
      WinQueryWindowPos(WinWindowFromID(hwnd, ST_PREVIEWPOSITION), &swp);
      WinCreateWindow(hwnd, PREVIEW_CLASS, "Preview area", WS_VISIBLE,
                      swp.x, swp.y, swp.cx, swp.cy,
                      hwnd, swp.hwndInsertBehind, ID_PREVIEWAREA, NULL, NULL);

      WinShowWindow(WinWindowFromID(hwnd, ST_PREVIEWPOSITION), FALSE);

      // Set Page3 text (NLS support)
      SetPage3Text(hwnd);

      // Set the initial values of controls, and
      // also start the preview if it's checked!
      SetPage3ControlValues(hwnd, TRUE);

      // Subclass every child control, so it will pass DM_* messages
      // to its owner, so locale objects dropped to the controls
      // will change language!
      SubclassChildWindowsForNLS(hwnd);

      // Subclass the notebook control too. It is required so that if
      // we dinamically change the language, we can show the right help!
      SubclassNotebookWindowForNLS();

      if (pUserData)
        pUserData->bPageSetUp = TRUE;

      WinShowWindow(hwnd, TRUE);
      return (MRESULT) FALSE;
    }

    case WM_LANGUAGECHANGED:
    {
      int iPreviewState;

      AddLog("WM_LANGUAGECHANGED] : Getting pUserData\n");
      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (!pUserData)
        return (MRESULT) 0;

      AddLog("WM_LANGUAGECHANGED] : Query old preview state\n");
      iPreviewState = wpssdesktop_wpssQueryScreenSaverPreviewEnabled(pUserData->Desktop);

      AddLog("WM_LANGUAGECHANGED] : Stop preview if running\n");
      // Stop preview if it is running.
      if (iPreviewState)
      {
        AddLog("WM_LANGUAGECHANGED] : Yes, was running, so stop it!\n");
        AddLog(" - 1\n");
        WinCheckButton(hwnd, CB_SHOWPREVIEW, FALSE);
        AddLog(" - 2\n");
        wpssdesktop_wpssSetScreenSaverPreviewEnabled(pUserData->Desktop,
                                                     FALSE);
        // AddLog(" - 3\n");
        StopPage3Preview(hwnd);
        AddLog(" - 4\n");
      }

      AddLog("WM_LANGUAGECHANGED] : Destroy old strings from available modules\n");

      // Destroy old strings from available modules
      if (pUserData->pModuleDescArray)
        free(pUserData->pModuleDescArray);
      pUserData->pModuleDescArray = NULL;
      pUserData->ulMaxModules = 0;

      AddLog("WM_LANGUAGECHANGED] : Get number of modules available\n");

      // Re-load strings (using the new language!)
      WinSetPointer(HWND_DESKTOP, WinQuerySysPointer(HWND_DESKTOP, SPTR_WAIT, FALSE));
      pUserData->ulMaxModules =
        wpssdesktop_wpssQueryNumOfAvailableScreenSaverModules(pUserData->Desktop);
      WinSetPointer(HWND_DESKTOP, WinQuerySysPointer(HWND_DESKTOP, SPTR_ARROW, FALSE));

      AddLog("WM_LANGUAGECHANGED] : malloc()\n");

      pUserData->pModuleDescArray =
                (PSSMODULEDESC) malloc(pUserData->ulMaxModules * sizeof(SSMODULEDESC));
      if (pUserData->pModuleDescArray)
      {
        AddLog("WM_LANGUAGECHANGED] : Get list of modules\n");
        WinSetPointer(HWND_DESKTOP,
                      WinQuerySysPointer(HWND_DESKTOP, SPTR_WAIT, FALSE));
        wpssdesktop_wpssQueryInfoAboutAvailableScreenSaverModules(pUserData->Desktop,
                                                                  pUserData->ulMaxModules,
                                                                  pUserData->pModuleDescArray);
        WinSetPointer(HWND_DESKTOP,
                      WinQuerySysPointer(HWND_DESKTOP, SPTR_ARROW, FALSE));
      }
      else
        pUserData->ulMaxModules = 0;

      AddLog("WM_LANGUAGECHANGED] : SetPage3Text()\n");
      SetPage3Text(hwnd);

      AddLog("WM_LANGUAGECHANGED] : SetPage3ControlValues()\n");
      SetPage3ControlValues(hwnd, FALSE);

      // Restart preview if it was running before!
      if (iPreviewState)
      {
        AddLog("WM_LANGUAGECHANGED] : Restart preview!\n");
        wpssdesktop_wpssSetScreenSaverPreviewEnabled(pUserData->Desktop, TRUE);
        WinCheckButton(hwnd, CB_SHOWPREVIEW, TRUE);
        StartPage3Preview(hwnd);
      }

      AddLog("WM_LANGUAGECHANGED] : Done.\n");
      return (MRESULT) 0;
    }

    case WM_SSCORENOTIFY_PREVIEWSTARTED:
      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (!pUserData)
        return (MRESULT) 0;

      AddLog("[WM_PREVIEWSTARTED] : Done.\n");
      if (pUserData->iPreviewMsgCounter>0)
        pUserData->iPreviewMsgCounter--;
      pUserData->bPreviewRunning = 1;

      // Enable the disabled controls
      WinEnableWindow(WinWindowFromID(hwnd, PB_STARTMODULE), TRUE);
      WinEnableWindow(WinWindowFromID(hwnd, LB_MODULELIST), TRUE);
      WinEnableWindow(WinWindowFromID(hwnd, CB_SHOWPREVIEW), TRUE);

      lID = (long) WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_QUERYSELECTION, (MPARAM) LIT_CURSOR, (MPARAM) NULL);
      if (lID!=LIT_NONE)
      {
        lID = (long) WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_QUERYITEMHANDLE,
                                       (MPARAM) lID, (MPARAM) NULL);
        if ((lID<0) || lID>=pUserData->ulMaxModules)
        {
          // Something is wrong, the handle, which should tell the index in the
          // module desc array, points out of the array!
          lID = LIT_NONE;
        }
      }

      if (lID!=LIT_NONE)
        WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE),
                        pUserData->pModuleDescArray[lID].bConfigurable);
      else
        WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE), FALSE);
      
      return (MRESULT) 0;

    case WM_SSCORENOTIFY_PREVIEWSTOPPED:
      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (!pUserData)
        return (MRESULT) 0;

      AddLog("[WM_PREVIEWSTOPPED] : Done.\n");

      if (pUserData->iPreviewMsgCounter>0)
        pUserData->iPreviewMsgCounter--;
      pUserData->bPreviewRunning = 0;

      // Enable the disabled controls
      WinEnableWindow(WinWindowFromID(hwnd, PB_STARTMODULE), TRUE);
      WinEnableWindow(WinWindowFromID(hwnd, LB_MODULELIST), TRUE);
      WinEnableWindow(WinWindowFromID(hwnd, CB_SHOWPREVIEW), TRUE);

      lID = (long) WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_QUERYSELECTION, (MPARAM) LIT_CURSOR, (MPARAM) NULL);
      if (lID!=LIT_NONE)
      {
        lID = (long) WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_QUERYITEMHANDLE, (MPARAM) lID, (MPARAM) NULL);
        if ((lID<0) || lID>=pUserData->ulMaxModules)
        {
          // Something is wrong, the handle, which should tell the index in the
          // module desc array, points out of the array!
          lID = LIT_NONE;
        }
      }
      if (lID!=LIT_NONE)
        WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE),
                        pUserData->pModuleDescArray[lID].bConfigurable);
      else
        WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE), FALSE);
      
      return (MRESULT) 0;

    case WM_DESTROY:
      AddLog("[WM_DESTROY] : Stop preview!\n");

      // Make sure the preview will not go on!
      StopPage3Preview(hwnd);

      // Wait for this event to be processed!
      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pUserData)
      {
        HAB hab;
        QMSG qmsg;

        hab = WinQueryAnchorBlock(hwnd);

        AddLog("[WM_DESTROY] : Starting loop\n");

#ifdef DEBUG_LOGGING
        if (pUserData->iPreviewMsgCounter<=0)
          AddLog("[WM_DESTROY] : iPreviewMsgCounter is ZERO!!!\n");
        else
          AddLog("[WM_DESTROY] : iPreviewMsgCounter is not zero!\n");
#endif

        // Process messages until we get notification from preview window that
        // it has been stopped.
        if (pUserData->iPreviewMsgCounter>0)
        {
          AddLog("[WM_DESTROY] : Going into loop to wait for preview to stop!\n");

          while (WinGetMsg(hab, &qmsg, 0, 0, 0))
          {
            WinDispatchMsg(hab, &qmsg);
            if (pUserData->iPreviewMsgCounter<=0)
            {
              AddLog("[WM_DESTROY] : iPreviewMsgCounter became 0!\n");
              break;
            }
          }
          AddLog("[WM_DESTROY] : Preview stopped.\n");
        }
      }
      AddLog("[WM_DESTROY] : Destroying preview area child!\n");

      // Destroy preview area child window, created in WM_INITDLG!
      hwndPreview = WinWindowFromID(hwnd, ID_PREVIEWAREA);
      if ((hwndPreview) && (hwndPreview!=HWND_DESKTOP))
        WinDestroyWindow(hwndPreview);

      // Free private window memory!
      WinSetWindowULong(hwnd, QWL_USER, 0);
      if (pUserData)
      {
        if (pUserData->pModuleDescArray)
          free(pUserData->pModuleDescArray);
        free(pUserData);
      }

      // Note that we're destroyed!
      hwndSettingsPage3 = NULLHANDLE;

      AddLog("[WM_DESTROY] : Done!\n");
      break;

      // The dialog window size/pos is changed, or the dialog window is
      // being shown or hidden! Don't let the time-consuming screen saver
      // preview run when the dialog window is hidden!
    case WM_WINDOWPOSCHANGED:
    {
      PSWP pswpNew;
      pswpNew = (PSWP) mp1;

      if (pswpNew->fl & SWP_SHOW)
      {
        // The dialog window is about to be shown!
        if (WinQueryButtonCheckstate(hwnd, CB_SHOWPREVIEW))
        {
          // Restart screen saver preview!
          AddLog("[fnwpScreenSaverSettingsPage3] : WM_WINDOWPOSCHANGED : Starting preview\n");
          StartPage3Preview(hwnd);
        }
      }
      else
      if (pswpNew->fl & SWP_HIDE)
      {
        // The dialog window is about to be hidden!
        if (WinQueryButtonCheckstate(hwnd, CB_SHOWPREVIEW))
        {
          // Stop the running screen saver preview!
          AddLog("[fnwpScreenSaverSettingsPage3] : WM_WINDOWPOSCHANGED : Stopping preview\n");
          StopPage3Preview(hwnd);
        }
      }

      break;
    }

    case WM_CONTROL:
    {
      switch (SHORT1FROMMP(mp1)) // Control window ID
      {
        case LB_MODULELIST:
          if ((SHORT2FROMMP(mp1)==LN_SELECT) || (SHORT2FROMMP(mp1)==LN_ENTER))
          {
            SetPage3ModuleInfo(hwnd);
            UseSelectedModule(hwnd);
          }
          break;

        case CB_SHOWPREVIEW:
          if (SHORT2FROMMP(mp1)==BN_CLICKED)
          {
            pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
            if (!pUserData)
              break;

            // The button control has been clicked
            if (WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)))
            {
              wpssdesktop_wpssSetScreenSaverPreviewEnabled(pUserData->Desktop, TRUE);
              StartPage3Preview(hwnd);
            }
            else
            {
              wpssdesktop_wpssSetScreenSaverPreviewEnabled(pUserData->Desktop, FALSE);
              StopPage3Preview(hwnd);
            }
          }
          break;
      }
      return (MRESULT) FALSE;
    }

    case WM_COMMAND:
    {
      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (!pUserData)
        return (MRESULT) TRUE;

      switch (SHORT1FROMMP(mp1))
      {
        case PB_STARTMODULE:
          wpssdesktop_wpssStartScreenSaverNow(pUserData->Desktop, FALSE);
          break;

        case PB_CONFIGURE:
          ConfigureSelectedModule(hwnd);

          // Restart the preview if the user configured the module, so
          // the preview will show the new setting!
          if (WinQueryButtonCheckstate(hwnd, CB_SHOWPREVIEW))
          {
            StopPage3Preview(hwnd);
            StartPage3Preview(hwnd);
          }
          break;

        case PB_NOTEBOOK_PG3_UNDO:
          // Set back settings from UNDO buffer (saved at start of dialog)
          wpssdesktop_wpssSetScreenSaverModule(pUserData->Desktop,
                                               pUserData->achUndoSaverDLLFileName);
          // Set values of controls
          SetPage3ControlValues(hwnd, TRUE);
          break;

        case PB_NOTEBOOK_PG3_DEFAULT:
          // Set default settings
          wpssdesktop_wpssSetScreenSaverModule(pUserData->Desktop, "");

          // Set values of controls
          SetPage3ControlValues(hwnd, TRUE);
          break;

        case PB_NOTEBOOK_PG3_HELP:
          internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE3_SAVERMODULES);
          break;
      }

      return (MRESULT) TRUE;
    }

    case WM_HELP:
      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pUserData)
        internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE3_SAVERMODULES);
      return (MRESULT) TRUE;

    // Handling Drag'n'Drop of Locale objects!
    case DM_DRAGOVER:
      if (internal_IsLocaleObject((PDRAGINFO) mp1))
        return MRFROM2SHORT(DOR_DROP, DO_COPY); // Fine, let it come!
      return MRFROM2SHORT(DOR_NODROPOP, 0); // We support it, but not in this form!

    case DM_DROP:
      if (internal_IsLocaleObject((PDRAGINFO) mp1))
      {
        pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
        if (pUserData) {
          internal_SetLanguageFromLocaleObject(pUserData->Desktop, (PDRAGINFO) mp1);
          return MRFROM2SHORT(DOR_DROP, DO_COPY);
        }
      }
      return MPFROM2SHORT(DOR_NODROPOP, 0);
  }

  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

/*****************************************************************************/

