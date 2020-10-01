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
#define INCL_WINSHELLDATA
#define INCL_WINSTDDRAG
#define INCL_DOS
#define INCL_DOSMISC
#define INCL_GPIPRIMITIVES
#define INCL_ERRORS
#include <os2.h>

#define INCL_WPCLASS
#define INCL_WPFOLDER
#include <pmwp.h>

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


// Common notebook buttons of Help, Undo and Default:
#define PB_NOTEBOOK_PG1_HELP    5000
#define PB_NOTEBOOK_PG1_UNDO    5001
#define PB_NOTEBOOK_PG1_DEFAULT 5002

#define PB_NOTEBOOK_PG2_HELP    5003
#define PB_NOTEBOOK_PG2_UNDO    5004
#define PB_NOTEBOOK_PG2_DEFAULT 5005

#define PB_NOTEBOOK_PG3_HELP    5006
#define PB_NOTEBOOK_PG3_UNDO    5007
#define PB_NOTEBOOK_PG3_DEFAULT 5008

// Preview area
#define ID_PREVIEWAREA      5009
#define PREVIEW_CLASS       "ScreenSaverPreviewAreaWindowClass"

// Temp checkbox to get size
#define ID_TEMPCHECKBOX     5010

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
  int  bUndoStartAtStartup;
  int  bUndoPasswordProtected;
  char achUndoEncryptedPassword[SSCORE_MAXPASSWORDLEN];
  int  bUndoDelayedPasswordProtection;
  int  iUndoDelayedPasswordProtectionTime;
  int  bUndoWakeByKeyboardOnly;
} Page1UserData_t, *Page1UserData_p;

typedef struct Page2UserData_s
{
  WPSSDesktop *Desktop;

  int  bPageSetUp; // TRUE if the page has been set up correctly

  // Startup configuration (used for Undo button)
  int  bUndoUseDPMSStandbyState;
  int  iUndoDPMSStandbyStateTimeout;
  int  bUndoUseDPMSSuspendState;
  int  iUndoDPMSSuspendStateTimeout;
  int  bUndoUseDPMSOffState;
  int  iUndoDPMSOffStateTimeout;
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




// We'll have two dialog procs
MRESULT EXPENTRY fnwpScreenSaverSettingsPage1(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2); // General settings
MRESULT EXPENTRY fnwpScreenSaverSettingsPage2(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2); // DPMS settings
MRESULT EXPENTRY fnwpScreenSaverSettingsPage3(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2); // Saver modules

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

#ifdef DEBUG_LOGGING
//  AddLog("[internal_FileExists] : [");
//  AddLog(pchFileName);
//  AddLog("]\n");
#endif

  rc = DosFindFirst(pchFileName,
		    &hdirFindHandle,
                    FILE_NORMAL,
		    &FindBuffer,
		    ulResultBufLen,
		    &ulFindCount,
		    FIL_STANDARD);
  DosFindClose(hdirFindHandle);
#ifdef DEBUG_LOGGING
//  if (rc==NO_ERROR)
//    AddLog("[internal_FileExists] : rc is NO_ERROR, returning TRUE!\n");
//  else
//    AddLog("[internal_FileExists] : rc is some ERROR, returning FALSE!\n");
#endif

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

#ifdef DEBUG_LOGGING
  AddLog("[internal_GetRealLocaleName] : In: [");
  AddLog(pchLanguageCode);
  AddLog("]\n");
#endif

  // Now, we have to create an unicode string from the locale name
  // we have here. For this, we allocate memory for the UCS string!
  iLanguageCodeLen = strlen(pchLanguageCode);
  pucLanguageCode = (UniChar *) malloc(sizeof(UniChar) * iLanguageCodeLen*4+4);
  if (!pucLanguageCode)
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_GetRealLocaleName] : Not enough memory!\n");
#endif
    // Not enough memory!
    return 0;
  }

  // Create unicode convert object
  rc = UniCreateUconvObject(L"", &ucoTemp);
  if (rc!=ULS_SUCCESS)
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_GetRealLocaleName] : Could not create convert object!\n");
#endif
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
                       &pchFrom,
                       &iFromCount,
                       &pucTo,
                       &iToCount,
                       &iNonIdentical);

    if (rc!=ULS_SUCCESS)
    {
      // Could not convert language code to UCS string!
#ifdef DEBUG_LOGGING
      AddLog("[internal_GetRealLocaleName] : Could not convert language code to UCS string!\n");
#endif
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
#ifdef DEBUG_LOGGING
    AddLog("[internal_GetRealLocaleName] : Could not create locale object of this name!\n");
#endif
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
#ifdef DEBUG_LOGGING
    AddLog("[internal_GetRealLocaleName] : Could not get real locale name!\n");
#endif
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
                       &pchTo,
                       &iToCount,
                       &iNonIdentical);

  // Free real locale name got from Uni API
  UniFreeMem(pucFrom);

  // Free other resources
  UniFreeLocaleObject(loTemp);
  UniFreeUconvObject(ucoTemp);
  free(pucLanguageCode);

#ifdef DEBUG_LOGGING
  AddLog("[internal_GetRealLocaleName] : Out: [");
  AddLog(pchRealLocaleName);
  AddLog("]\n");
#endif

  return (rc == ULS_SUCCESS);
}

static void internal_SetNLSHelpFilename(WPSSDesktop *somSelf)
{
  WPSSDesktopData *somThis = WPSSDesktopGetData(somSelf);
  char achRealLocaleName[128];
  char *pchLang;
  ULONG rc;
  int i;

#ifdef DEBUG_LOGGING
  AddLog("[internal_SetNLSHelpFilename] : Enter!\n");
#endif

  // Try to get a help file name
  rc = 0; // Not found the help file yet.

  // Get the real locale name from the language code
  internal_GetRealLocaleNameFromLanguageCode(_achLanguage, achRealLocaleName, sizeof(achRealLocaleName));

  // Get language code
  if (achRealLocaleName[0]!=0)
  {
    // Aaah, there is a language set!
    // Try that one!

#ifdef DEBUG_LOGGING
    AddLog("[internal_SetNLSHelpFilename] : Trying the language which is set by the user!\n");
#endif

    pchLang = (char *) malloc(1024);
    if (!pchLang)
    {
      // Not enough memory, so we won't do the long
      // method of searching for every combination of
      // the language string, but only for the string itself!

      // Assemble NLS file name
      sprintf(achLocalHelpFileName, "%sLang\\ss_%s.HLP", achHomeDirectory, achRealLocaleName);
#ifdef DEBUG_LOGGING
      AddLog("[internal_SetNLSHelpFilename] : Trying to use help file: [");
      AddLog(achLocalHelpFileName);
      AddLog("] by achLocalLanguage setting (in Not enough memory branch)!\n");
#endif
      rc = internal_FileExists(achLocalHelpFileName);
    } else
    {
      // Fine, we can start trying a lot of filenames!
      sprintf(pchLang, "%s", achRealLocaleName);

      do {
	// Assemble NLS file name
	sprintf(achLocalHelpFileName, "%sLang\\ss_%s.HLP", achHomeDirectory, pchLang);
#ifdef DEBUG_LOGGING
	AddLog("[internal_SetNLSHelpFilename] : Trying to use help file: [");
	AddLog(achLocalHelpFileName);
	AddLog("] by achLocalLanguage setting! (in Loop)\n");
#endif

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
#ifdef DEBUG_LOGGING
    AddLog("[internal_SetNLSHelpFilename] : No language set, or could not open the required one -> LANG!\n");
#endif

    // No language set, or could not open it, so use LANG variable!
    rc = DosScanEnv("LANG", &pchLang);
    if ((rc!=NO_ERROR) || (!pchLang))
    {
#ifdef DEBUG_LOGGING
      AddLog("[internal_SetNLSHelpFilename] : Could not query LANG env. var., will use 'en'\n");
#endif
      pchLang = "en"; // Default
    }

    // Assemble NLS file name
    sprintf(achLocalHelpFileName, "%sLang\\ss_%s.HLP", achHomeDirectory, pchLang);
#ifdef DEBUG_LOGGING
    AddLog("[internal_SetNLSHelpFilename] : Trying to use help file: [");
    AddLog(achLocalHelpFileName);
    AddLog("] by LANG env. variable!\n");
#endif

    rc = internal_FileExists(achLocalHelpFileName);
    if (!rc)
    {
#ifdef DEBUG_LOGGING
      AddLog("[internal_SetNLSHelpFilename] : Could not open it, trying the default!\n");
#endif
      pchLang = "en"; // Default

      // Assemble NLS file name
      sprintf(achLocalHelpFileName, "%sLang\\ss_%s.HLP", achHomeDirectory, pchLang);
      rc = internal_FileExists(achLocalHelpFileName);
    }
  }

  if (!rc)
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_SetNLSHelpFilename] : Result : No help file!\n");
#endif
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
	  sprintf(pchFileName, "%sLang\\ss_%s.msg", achHomeDirectory, pchLang);
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

    if (!hfNLSFile)
    {
      // No language set, or could not open it, so use LANG variable!
      strcpy(achRealLocaleName, ""); // Make local language setting empty!

      rc = DosScanEnv("LANG", &pchLang);
      if ((rc!=NO_ERROR) || (!pchLang))
      {
#ifdef DEBUG_LOGGING
        AddLog("[internal_OpenNLSFile] : Could not query LANG env. var., will use 'en'\n");
#endif
        pchLang = "en"; // Default
      }

      // Assemble NLS file name
      sprintf(pchFileName, "%sLang\\ss_%s.msg", achHomeDirectory, pchLang);
#ifdef DEBUG_LOGGING
      AddLog("[internal_OpenNLSFile] : Trying to open NLS file: [");
      AddLog(pchFileName);
      AddLog("] by LANG env. variable!\n");
#endif

      hfNLSFile = fopenMessageFile(pchFileName);

      if (!hfNLSFile)
      {
#ifdef DEBUG_LOGGING
	AddLog("[internal_OpenNLSFile] : Could not open it, trying the default!\n");
#endif
	pchLang = "en"; // Default

	// Assemble NLS file name
	sprintf(pchFileName, "%sLang\\ss_%s.msg", achHomeDirectory, pchLang);
	hfNLSFile = fopenMessageFile(pchFileName);
      }
    }
    free(pchFileName);
  }

#ifdef DEBUG_LOGGING
  AddLog("[internal_OpenNLSFile] : Done.\n");
#endif

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
#ifdef DEBUG_LOGGING
    AddLog("[internal_SendNLSTextToSSCore] : Memory alloced.\n");
#endif

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
	  if (sprintmsg(pchTemp, hfNLSFile, "MOD_0007"))
	    SSCore_SetNLSText(i, pchTemp);
          else
            SSCore_SetNLSText(i, NULL);
	  break;
	default:
	  SSCore_SetNLSText(i, NULL);
          break;
      }
    }

#ifdef DEBUG_LOGGING
    AddLog("[internal_SendNLSTextToSSCore] : For loop done.\n");
#endif

  } else
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_SendNLSTextToSSCore] : No NLS file or not enough memory!\n");
#endif

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
#ifdef DEBUG_LOGGING
    AddLog("[internal_wpDisplayHelp] : Displaying help from [");
    AddLog(achLocalHelpFileName);
    AddLog("]\n");
#endif
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
        bpi.pszMinorTab = (PSZ) "DPMS settings";
    } else
    {
      bpi.pszMinorTab = (PSZ) "DPMS settings";
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
  ULONG ulPageID;
  PAGEINFO      pi;
  BOOKPAGEINFO  bpi;
  char *pchTemp;
  FILE *hfNLSFile;

#ifdef DEBUG_LOGGING
  AddLog("[wpssdesktop_wpssAddScreenSaver3Page] : Enter\n");
#endif

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
  if (hfNLSFile)
  {
    pi.pszName = pchTemp = (char *) malloc(1024);
  } else
    pchTemp = NULL;
  if (pchTemp)
  {
    if (!sprintmsg(pi.pszName, hfNLSFile, "COM_0000"))
      pi.pszName = "S~creen Saver";
  } else
  {
    pi.pszName = "S~creen Saver";
  }
  pi.pCreateParams = somSelf;

  pi.pszHelpLibraryName = achLocalHelpFileName;
  pi.idDefaultHelpPanel = HELPID_PAGE1_GENERAL;

  ulPageID = _wpInsertSettingsPage(somSelf, hwndNotebook, &pi);

  if (pchTemp)
    free(pchTemp);

  if (ulPageID==0)
  {
#ifdef DEBUG_LOGGING
    AddLog("[wpssdesktop_wpssAddScreenSaver3Page] : Could not insert page!\n");
#endif
    // Could not insert new page!
    return SETTINGS_PAGE_REMOVED;
  }

  // Hack the 'page info' to show the minor tab title in the notebook context menu
  // This is taken from XWorkplace!
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
  WinSendMsg(hwndNotebook,
             BKM_SETPAGEINFO,
             (MPARAM) ulPageID,
             (MPARAM) &bpi);

  if (pchTemp)
    free(pchTemp);

  // Save notebook handle so the text can be changed lated on the fly!
  hwndSettingsNotebook = hwndNotebook;
  ulSettingsPage1ID = ulPageID;

  internal_CloseNLSFile(hfNLSFile);

#ifdef DEBUG_LOGGING
  AddLog("[wpssdesktop_wpssAddScreenSaver3Page] : Leave\n");
#endif

  // Ok, new page inserted!
  return ulPageID;
}

SOM_Scope BOOL SOMLINK wpssdesktop_wpssAddScreenSaver2Page(WPSSDesktop *somSelf,
                                                           HWND hwndNotebook)
{
  ULONG ulPageID;
  PAGEINFO      pi;
  BOOKPAGEINFO  bpi;
  char *pchTemp;
  FILE *hfNLSFile;

#ifdef DEBUG_LOGGING
  AddLog("[wpssdesktop_wpssAddScreenSaver2Page] : Enter\n");
#endif

  // See notes in wpssAddScreenSaver3Page about which function adds which page!

  hfNLSFile = internal_OpenNLSFile(somSelf);

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
  pi.idDefaultHelpPanel = HELPID_PAGE2_GENERAL;

  ulPageID = _wpInsertSettingsPage(somSelf, hwndNotebook, &pi);
  if (ulPageID==0)
  {
#ifdef DEBUG_LOGGING
    AddLog("[wpssdesktop_wpssAddScreenSaver2Page] : Could not insert page!\n");
#endif
    // Could not insert new page!
    return SETTINGS_PAGE_REMOVED;
  }

  // Hack the 'page info' to show the minor tab title in the notebook context menu
  // This is taken from XWorkplace!
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
      bpi.pszMinorTab = (PSZ) "DPMS settings";
  } else
  {
    bpi.pszMinorTab = (PSZ) "DPMS settings";
  }

  bpi.cbMinorTab = strlen(bpi.pszMinorTab);
  WinSendMsg(hwndNotebook,
             BKM_SETPAGEINFO,
             (MPARAM) ulPageID,
             (MPARAM) &bpi);

  if (pchTemp)
    free(pchTemp);

  // Save pageID for later use!
  hwndSettingsNotebook = hwndNotebook;
  ulSettingsPage2ID = ulPageID;

  internal_CloseNLSFile(hfNLSFile);

#ifdef DEBUG_LOGGING
  AddLog("[wpssdesktop_wpssAddScreenSaver2Page] : Leave\n");
#endif


  // Ok, new page inserted!
  return ulPageID;
}


SOM_Scope BOOL SOMLINK wpssdesktop_wpssAddScreenSaver1Page(WPSSDesktop *somSelf,
                                                           HWND hwndNotebook)
{
  ULONG ulPageID;
  PAGEINFO      pi;
  BOOKPAGEINFO  bpi;
  char *pchTemp;
  FILE *hfNLSFile;

#ifdef DEBUG_LOGGING
  AddLog("[wpssdesktop_wpssAddScreenSaver1Page] : Enter\n");
#endif

  // See notes in wpssAddScreenSaver3Page about which function adds which page!

  hfNLSFile = internal_OpenNLSFile(somSelf);

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
  pi.idDefaultHelpPanel = HELPID_PAGE3_GENERAL;

  ulPageID = _wpInsertSettingsPage(somSelf, hwndNotebook, &pi);
  if (ulPageID==0)
  {
#ifdef DEBUG_LOGGING
    AddLog("[wpssdesktop_wpssAddScreenSaver1Page] : Could not insert page!\n");
#endif
    // Could not insert new page!
    return SETTINGS_PAGE_REMOVED;
  }

  // Hack the 'page info' to show the minor tab title in the notebook context menu
  // This is taken from XWorkplace!
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
  WinSendMsg(hwndNotebook,
             BKM_SETPAGEINFO,
             (MPARAM) ulPageID,
             (MPARAM) &bpi);

  if (pchTemp)
    free(pchTemp);

  // Save pageID for later use!
  hwndSettingsNotebook = hwndNotebook;
  ulSettingsPage3ID = ulPageID;

  internal_CloseNLSFile(hfNLSFile);

#ifdef DEBUG_LOGGING
  AddLog("[wpssdesktop_wpssAddScreenSaver1Page] : Leave\n");
#endif

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
  rc =
#endif

  SSCore_StartModulePreview(pszSaverDLLFileName, hwndClientArea);

#ifdef DEBUG_LOGGING
  if (rc!=SSCORE_NOERROR)
  {
    char achTemp[128];
    sprintf(achTemp,"StartModulePreview SSCore error: rc=0x%x\n", rc);
    AddLog(achTemp);
  }
#endif
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssConfigureScreenSaverModule(WPSSDesktop *somSelf, PSZ pszSaverDLLFileName, HWND hwndOwnerWindow)
{
  SSCore_ConfigureModule(pszSaverDLLFileName, hwndOwnerWindow);
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssStopScreenSaverModulePreview(WPSSDesktop *somSelf, HWND hwndClientArea)
{
#ifdef DEBUG_LOGGING
  AddLog("[wpssStopScreenSaverModulePreview]\n");
#endif
  SSCore_StopModulePreview(hwndClientArea);
}

SOM_Scope VOID SOMLINK wpssdesktop_wpssSetScreenSaverEnabled(WPSSDesktop *somSelf, BOOL bEnabled)
{
  SSCoreSettings_t CurrentSettings;

#ifdef DEBUG_LOGGING
  AddLog("[SetScreenSaverEnabled] : Called!\n");
#endif
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

#ifdef DEBUG_LOGGING
  AddLog("[SetScreenSaverTimeout] : Called!\n");
#endif
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

#ifdef DEBUG_LOGGING
  AddLog("[WPSSDesktop] : QIAASSM: Calling SSCore_GetListOfModules()\n");
#endif

  if (SSCore_GetListOfModules(&pList)!=SSCORE_NOERROR)
  {
#ifdef DEBUG_LOGGING
    AddLog("[WPSSDesktop] : QIAASSM: SSCore_GetListOfModules() returned error, returning 0\n");
#endif

    return 0;
  }

#ifdef DEBUG_LOGGING
  AddLog("[WPSSDesktop] : QIAASSM: Processing the result\n");
#endif

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

#ifdef DEBUG_LOGGING
  AddLog("[WPSSDesktop] : QIAASSM: Calling SSCore_FreeListOfModules()\n");
#endif

  SSCore_FreeListOfModules(pList);

#ifdef DEBUG_LOGGING
  AddLog("[WPSSDesktop] : QIAASSM: Done, returning.\n");
#endif
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
#ifdef DEBUG_LOGGING
    AddLog("[WPSSDesktop] : wpSaveState : I'm the desktop object, saving my state.\n");
#endif

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
#ifdef DEBUG_LOGGING
      AddLog("[WPSSDesktop] : wpRestoreState : I'm the desktop object, calling setup.\n");
#endif
      _wpSetup(somSelf, "AUTOLOCKUP=NO;LOCKUPONSTARTUP=NO");
    }
  }

  // Then restore our internal state
  if (_wpIsCurrentDesktop(somSelf))
  {
    ULONG ulValue;
    FILE *hfNLSFile;

#ifdef DEBUG_LOGGING
    AddLog("[WPSSDesktop] : wpRestoreState : I'm the desktop object, restoring my state.\n");
#endif
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
#ifdef DEBUG_LOGGING
    AddLog("[WPSSDesktop] : wpSetAutoLockup\n");
#endif
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
#ifdef DEBUG_LOGGING
    AddLog("[WPSSDesktop] : wpLockOnStartup\n");
#endif
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

#ifdef DEBUG_LOGGING
  AddLog("[WPSSDesktop] : wpSetup\n");
  AddLog(" String: ["); AddLog(pszSetupString); AddLog("]\n");
#endif

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
#ifdef DEBUG_LOGGING
	AddLog("[WPSSDesktop] : wpSetup : AUTOLOCKUP=YES!\n");
#endif
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
#ifdef DEBUG_LOGGING
	AddLog("[WPSSDesktop] : wpSetup : LOCKUPONSTARTUP=YES!\n");
#endif
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
#ifdef DEBUG_LOGGING
	AddLog("[WPSSDesktop] : wpMenuItemSelected : WPMENUID_LOCKUP\n");
#endif
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
#ifdef DEBUG_LOGGING
    AddLog("[WPSSDesktop] : wpOpen : It was the first open!\n");
#endif

    bFirstOpen = FALSE;

    if (_bAutoStartAtStartup)
    {
#ifdef DEBUG_LOGGING
      AddLog("[WPSSDesktop] : wpOpen : Autostart is ON!\n");
#endif
      if (wpssdesktop_wpssQueryScreenSaverPasswordProtected(somSelf))
      {
#ifdef DEBUG_LOGGING
        AddLog("[WPSSDesktop] : wpOpen : PwdProt is ON -> Start saver!\n");
#endif
        wpssdesktop_wpssStartScreenSaverNow(somSelf, TRUE);
      }
#ifdef DEBUG_LOGGING
      else
        AddLog("[WPSSDesktop] : wpOpen : PwdProt is OFF!\n");
#endif

    }
#ifdef DEBUG_LOGGING
    else
    {
      AddLog("[WPSSDesktop] : wpOpen : Autostart is OFF!\n");
    }

    AddLog("[WPSSDesktop] : wpOpen : End of processing of first open.\n");
#endif
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

#ifdef DEBUG_LOGGING
  AddLog("[wpssdesktopM_wpssclsInitializeScreenSaver] : Enter\n");
#endif

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

#ifdef DEBUG_LOGGING
  AddLog("[wpssclsInitializeScreenSaver] : Home dir: [");
  AddLog(achHomeDirectory);
  AddLog("]\n");
#endif

  // Initialize Screen Saver
  rc = SSCore_Initialize(WinQueryAnchorBlock(HWND_DESKTOP), achHomeDirectory);
#ifdef DEBUG_LOGGING
  if (rc!=SSCORE_NOERROR)
  {
    AddLog("[wpssclsInitializeScreenSaver] : Could not initialize SSCore!\n");
  }
#endif

#ifdef DEBUG_LOGGING
  AddLog("[wpssdesktopM_wpssclsInitializeScreenSaver] : Leave\n");
#endif

  return (rc==SSCORE_NOERROR);
}

SOM_Scope BOOL SOMLINK wpssdesktopM_wpssclsUninitializeScreenSaver(M_WPSSDesktop *somSelf)
{
#ifdef DEBUG_LOGGING
  AddLog("[wpssdesktopM_wpssclsUninitializeScreenSaver] : Calling sscore_uninitialize\n");
#endif
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
  BOOL rc;

  // Query default settings page size from parent
  rc = parent_wpclsQuerySettingsPageSize(somSelf, pSizl);
  if (rc)
  {
    // Modify the minimum height, if needed!
    // This 180 Dialog Units height seems to be okay in most cases.
    // (empirically...)
    if (pSizl->cy < 210)
      pSizl->cy = 210;
  }

  return rc;
}


/**************************  ORDINARY CODE SECTION  ***************************
*****                                                                     *****
*****                  Any non-method code should go here.                *****
*****                                                                     *****
******************************************************************************/
#undef SOM_CurrentClass

static void internal_GetStaticTextSize(HWND hwnd, ULONG ulID, int *piCX, int *piCY)
{
  HPS hps;
  char achTemp[512];
  POINTL aptl[TXTBOX_COUNT];

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

static void internal_GetRadioButtonExtraSize(HWND hwnd, ULONG ulID, int *piCX, int *piCY)
{
  HWND hwndTemp;
  SWP swpTemp;

  // Create temporary radiobutton with no text inside
  hwndTemp = WinCreateWindow(hwnd, WC_BUTTON,
                             "",
                             BS_RADIOBUTTON | BS_AUTOSIZE,
                             10, 10,
                             -1, -1,
                             hwnd,
                             HWND_TOP,
                             ID_TEMPCHECKBOX,
                             NULL, NULL);

  // Query textbox size
  WinQueryWindowPos(hwndTemp, &swpTemp);
  // Destroy temporary radiobutton
  WinDestroyWindow(hwndTemp);

  *piCX = swpTemp.cx+1;
  *piCY = swpTemp.cy+1;
}

static void SetPage1Font(HWND hwnd)
{
  FILE *hfNLSFile;
  char achFont[512];
  HWND hwndChild;
  HENUM henum;
  Page1UserData_p pUserData;

  pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (pUserData)
    hfNLSFile = internal_OpenNLSFile(pUserData->Desktop);
  else
    hfNLSFile = NULLHANDLE;

  // Only if we have an NLS support file opened!
  if (hfNLSFile)
  {
    if (sprintmsg(achFont, hfNLSFile, "PG1_FONT"))
    {
      // Oookay, we have the font, set the page and all controls inside it!
      WinSetPresParam(hwnd, PP_FONTNAMESIZE,
		      strlen(achFont)+1,
		      achFont);

      // Now go through all of its children, and set it there, too!
      henum = WinBeginEnumWindows(hwnd);

      while ((hwndChild = WinGetNextWindow(henum))!=NULLHANDLE)
      {
	WinSetPresParam(hwndChild, PP_FONTNAMESIZE,
			strlen(achFont)+1,
			achFont);

      }
      WinEndEnumWindows(henum);
    }
  }

  internal_CloseNLSFile(hfNLSFile);
}

static void SetPage2Font(HWND hwnd)
{
  FILE *hfNLSFile;
  char achFont[512];
  HWND hwndChild;
  HENUM henum;
  Page2UserData_p pUserData;

  pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (pUserData)
    hfNLSFile = internal_OpenNLSFile(pUserData->Desktop);
  else
    hfNLSFile = NULLHANDLE;

  // Only if we have an NLS support file opened!
  if (hfNLSFile)
  {
    if (sprintmsg(achFont, hfNLSFile, "PG2_FONT"))
    {
      // Oookay, we have the font, set the page and all controls inside it!
      WinSetPresParam(hwnd, PP_FONTNAMESIZE,
		      strlen(achFont)+1,
		      achFont);

      // Now go through all of its children, and set it there, too!
      henum = WinBeginEnumWindows(hwnd);

      while ((hwndChild = WinGetNextWindow(henum))!=NULLHANDLE)
      {
	WinSetPresParam(hwndChild, PP_FONTNAMESIZE,
			strlen(achFont)+1,
			achFont);

      }
      WinEndEnumWindows(henum);
    }
  }

  internal_CloseNLSFile(hfNLSFile);
}

static void SetPage3Font(HWND hwnd)
{
  FILE *hfNLSFile;
  char achFont[512];
  HWND hwndChild;
  HENUM henum;
  Page3UserData_p pUserData;

  pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (pUserData)
    hfNLSFile = internal_OpenNLSFile(pUserData->Desktop);
  else
    hfNLSFile = NULLHANDLE;

  // Only if we have an NLS support file opened!
  if (hfNLSFile)
  {
    if (sprintmsg(achFont, hfNLSFile, "PG3_FONT"))
    {
      // Oookay, we have the font, set the page and all controls inside it!
      WinSetPresParam(hwnd, PP_FONTNAMESIZE,
		      strlen(achFont)+1,
		      achFont);

      // Now go through all of its children, and set it there, too!
      henum = WinBeginEnumWindows(hwnd);

      while ((hwndChild = WinGetNextWindow(henum))!=NULLHANDLE)
      {
	WinSetPresParam(hwndChild, PP_FONTNAMESIZE,
			strlen(achFont)+1,
			achFont);

      }
      WinEndEnumWindows(henum);
    }
  }

  internal_CloseNLSFile(hfNLSFile);
}

static void ResizePage1Controls(HWND hwnd)
{
  SWP swpTemp, swpTemp2, swpTemp3, swpWindow;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  int staticheight, checkboxheight, buttonheight, gbheight, radiobuttonheight;
  int iCBCX, iCBCY; // Checkbox extra sizes
  int iRBCX, iRBCY; // RadioButton extra sizes
  int iCX, iCY;     // Control size
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(hwnd))
  {
    WinEnableWindowUpdate(hwnd, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Get window size!
  WinQueryWindowPos(hwnd, &swpWindow);

  // Query static height in pixels. 
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_PASSWORD), &swpTemp);
  staticheight = swpTemp.cy;

  // Query button height in pixels.
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CHANGE), &swpTemp);
  buttonheight = swpTemp.cy;

  // Query checkbox height in pixels.
  WinQueryWindowPos(WinWindowFromID(hwnd, CB_USEPASSWORDPROTECTION), &swpTemp);
  checkboxheight = swpTemp.cy;

  // Query radiobutton height in pixels.
  WinQueryWindowPos(WinWindowFromID(hwnd, RB_USEPASSWORDOFCURRENTSECURITYUSER), &swpTemp);
  radiobuttonheight = swpTemp.cy;

  // Get extra sizes
  internal_GetCheckboxExtraSize(hwnd, CB_USEPASSWORDPROTECTION, &iCBCX, &iCBCY);
  internal_GetRadioButtonExtraSize(hwnd, RB_USEPASSWORDOFCURRENTSECURITYUSER, &iRBCX, &iRBCY);

  // Get size of maybe longest control, and make that the
  // minimum size of window!
  internal_GetStaticTextSize(hwnd, RB_USEPASSWORDOFCURRENTSECURITYUSER, &iCX, &iCY);
  if (swpWindow.cx < iCX+iRBCX) swpWindow.cx = iCX+iRBCX;

  internal_GetStaticTextSize(hwnd, CB_MAKETHEFIRSTKEYPRESSTHEFIRSTKEYOFTHEPASSWORD, &iCX, &iCY);
  if (swpWindow.cx < iCX+iCBCX) swpWindow.cx = iCX+iCBCX;

  // First set the "Password protection" groupbox
  gbheight =
    CYDLGFRAME + // frame
    checkboxheight + // Checkbox
    2*CYDLGFRAME + // Margin
    checkboxheight + // Checkbox
    2*CYDLGFRAME + // Margin
    staticheight + CYDLGFRAME + // Entry field
    CYDLGFRAME + // Margin
    checkboxheight + // Checkbox
    CYDLGFRAME + // Margin
    buttonheight + // Change button
    staticheight + CYDLGFRAME + // Entry field
    CYDLGFRAME + // Margin
    staticheight + CYDLGFRAME + // Entry field
    CYDLGFRAME + // Margin
    3*staticheight +  // Info text
    CYDLGFRAME + // Margin
    radiobuttonheight + // RadioButton
    radiobuttonheight + // RadioButton
    CYDLGFRAME + // Margin
    checkboxheight + // Checkbox
    2*CYDLGFRAME + // Margin
    staticheight; // Text of groupbox

  WinSetWindowPos(WinWindowFromID(hwnd, GB_PASSWORDPROTECTION),
		  HWND_TOP,
                  2*CXDLGFRAME,
                  2*CYDLGFRAME,
                  swpWindow.cx - 4*CXDLGFRAME,
                  gbheight,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, GB_PASSWORDPROTECTION), &swpTemp);

  //Things inside this groupbox
  internal_GetStaticTextSize(hwnd, CB_STARTSAVERMODULEONSTARTUP, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, CB_STARTSAVERMODULEONSTARTUP),
                  HWND_TOP,
                  swpTemp.x + CXDLGFRAME,
                  swpTemp.y + 2*CYDLGFRAME,
                  iCBCX+iCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, CB_STARTSAVERMODULEONSTARTUP), &swpTemp2);
  internal_GetStaticTextSize(hwnd, CB_MAKETHEFIRSTKEYPRESSTHEFIRSTKEYOFTHEPASSWORD, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, CB_MAKETHEFIRSTKEYPRESSTHEFIRSTKEYOFTHEPASSWORD),
                  HWND_TOP,
                  swpTemp.x + CXDLGFRAME,
                  swpTemp2.y + swpTemp2.cy + CYDLGFRAME,
                  iCBCX+iCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, CB_MAKETHEFIRSTKEYPRESSTHEFIRSTKEYOFTHEPASSWORD), &swpTemp2);
  internal_GetStaticTextSize(hwnd, ST_ASKPASSWORDONLYAFTER, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, ST_ASKPASSWORDONLYAFTER),
                  HWND_TOP,
                  swpTemp.x + CXDLGFRAME,
                  swpTemp2.y + swpTemp2.cy + 2*CYDLGFRAME,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ASKPASSWORDONLYAFTER), &swpTemp3);
  WinSetWindowPos(WinWindowFromID(hwnd, SPB_PWDDELAYMIN),
                  HWND_TOP,
                  swpTemp3.x + swpTemp3.cx + CXDLGFRAME,
                  swpTemp3.y - CYDLGFRAME/2,
                  CXDLGFRAME*12,
                  staticheight + CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, SPB_PWDDELAYMIN), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_MINUTESOFSAVING),
                  HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
                  swpTemp3.y,
                  swpTemp.cx - 2*CXDLGFRAME - (swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME),
                  staticheight,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_ASKPASSWORDONLYAFTER), &swpTemp2);
  internal_GetStaticTextSize(hwnd, CB_DELAYPASSWORDPROTECTION, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, CB_DELAYPASSWORDPROTECTION),
                  HWND_TOP,
                  3*CXDLGFRAME,
                  swpTemp2.y + swpTemp2.cy + CYDLGFRAME,
                  iCBCX+iCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);

  // Pushbutton
  WinQueryWindowPos(WinWindowFromID(hwnd, CB_DELAYPASSWORDPROTECTION), &swpTemp3);
  internal_GetStaticTextSize(hwnd, PB_CHANGE, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, PB_CHANGE), HWND_TOP,
                  swpTemp.x + swpTemp.cx - 3*CXDLGFRAME - (6*CXDLGFRAME + iCX),
                  swpTemp3.y + swpTemp3.cy + CYDLGFRAME,
		  6*CXDLGFRAME + iCX,
		  3*CYDLGFRAME + iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_CHANGE), &swpTemp);

  internal_GetStaticTextSize(hwnd, ST_PASSWORDFORVERIFICATION, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, ST_PASSWORDFORVERIFICATION), HWND_TOP,
                  3*CXDLGFRAME + iRBCX,
                  swpTemp.y + swpTemp.cy + 2*CYDLGFRAME,
                  iCX, iCY,
		  SWP_MOVE | SWP_SIZE);
  internal_GetStaticTextSize(hwnd, ST_FORVERIFICATION, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, ST_FORVERIFICATION), HWND_TOP,
                  3*CXDLGFRAME + iRBCX,
                  swpTemp.y + swpTemp.cy + 2*CYDLGFRAME - staticheight - CYDLGFRAME,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_FORVERIFICATION), &swpTemp2);
  internal_GetStaticTextSize(hwnd, ST_PASSWORD, &iCX, &iCY);
  if (iCX > swpTemp2.cx) swpTemp2.cx = iCX;

  WinSetWindowPos(WinWindowFromID(hwnd, EF_PASSWORD2), HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME,
                  swpTemp.y + swpTemp.cy + 2*CYDLGFRAME,
                  swpWindow.cx - 4*CXDLGFRAME -
                    (swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME) - CXDLGFRAME,
                  staticheight,
                  SWP_MOVE | SWP_SIZE);

  WinSetWindowPos(WinWindowFromID(hwnd, EF_PASSWORD), HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME,
                  swpTemp.y + swpTemp.cy + 2*CYDLGFRAME + staticheight + CYDLGFRAME + CYDLGFRAME,
                  swpWindow.cx - 4*CXDLGFRAME -
                    (swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME) - CXDLGFRAME,
                  staticheight,
                  SWP_MOVE | SWP_SIZE);

  internal_GetStaticTextSize(hwnd, ST_PASSWORD, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, ST_PASSWORD), HWND_TOP,
                  3*CXDLGFRAME + iRBCX,
                  swpTemp.y + swpTemp.cy + 2*CYDLGFRAME + staticheight + CYDLGFRAME + CYDLGFRAME,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, EF_PASSWORD), &swpTemp);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_TYPEYOURPASSWORD), HWND_TOP,
                  3*CXDLGFRAME + iRBCX,
                  swpTemp.y + swpTemp.cy + CYDLGFRAME,
                  swpWindow.cx - 6*CXDLGFRAME - iRBCX,
                  staticheight*3,
                  SWP_MOVE | SWP_SIZE);

  // The two radiobuttons
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_TYPEYOURPASSWORD), &swpTemp);
  internal_GetStaticTextSize(hwnd, RB_USETHISPASSWORD, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, RB_USETHISPASSWORD), HWND_TOP,
                  2*CXDLGFRAME + CXDLGFRAME,
                  swpTemp.y + swpTemp.cy + CYDLGFRAME,
                  iRBCX + iCX, iRBCY,
                  SWP_MOVE | SWP_SIZE);
  WinQueryWindowPos(WinWindowFromID(hwnd, RB_USETHISPASSWORD), &swpTemp);
  internal_GetStaticTextSize(hwnd, RB_USEPASSWORDOFCURRENTSECURITYUSER, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, RB_USEPASSWORDOFCURRENTSECURITYUSER), HWND_TOP,
                  2*CXDLGFRAME + CXDLGFRAME,
                  swpTemp.y + swpTemp.cy,
                  iRBCX + iCX, iRBCY,
                  SWP_MOVE | SWP_SIZE);

  // The main checkbox
  WinQueryWindowPos(WinWindowFromID(hwnd, RB_USEPASSWORDOFCURRENTSECURITYUSER), &swpTemp);
  internal_GetStaticTextSize(hwnd, CB_USEPASSWORDPROTECTION, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, CB_USEPASSWORDPROTECTION), HWND_TOP,
                  2*CXDLGFRAME + CXDLGFRAME,
                  swpTemp.y + swpTemp.cy + CYDLGFRAME,
                  iCBCX + iCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);

  // Then set the "General settings" groupbox
  gbheight =
    CYDLGFRAME + // frame
    CYDLGFRAME + // Margin
    staticheight + CYDLGFRAME + // Entry field
    CYDLGFRAME + // Margin
    checkboxheight + // Checkbox
    CYDLGFRAME + // Margin
    checkboxheight + // Checkbox
    CYDLGFRAME + // Margin
    checkboxheight + // Checkbox
    CYDLGFRAME + // Margin
    staticheight; // Text of groupbox

  WinQueryWindowPos(WinWindowFromID(hwnd, GB_PASSWORDPROTECTION), &swpTemp);
  WinSetWindowPos(WinWindowFromID(hwnd, GB_GENERALSETTINGS),
                  HWND_TOP,
                  swpTemp.x,
                  swpTemp.y + swpTemp.cy + CYDLGFRAME,
                  swpWindow.cx - 4*CXDLGFRAME,
                  gbheight,
                  SWP_MOVE | SWP_SIZE);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_GENERALSETTINGS), &swpTemp);

  internal_GetStaticTextSize(hwnd, CB_DISABLEVIOPOPUPSWHILESAVING, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, CB_DISABLEVIOPOPUPSWHILESAVING),
                  HWND_TOP,
                  3*CXDLGFRAME,
                  swpTemp.y + CYDLGFRAME,
                  iCBCX + iCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, CB_DISABLEVIOPOPUPSWHILESAVING), &swpTemp3);
  internal_GetStaticTextSize(hwnd, CB_WAKEUPBYKEYBOARDONLY, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, CB_WAKEUPBYKEYBOARDONLY),
                  HWND_TOP,
                  3*CXDLGFRAME,
                  swpTemp3.y + swpTemp3.cy + 2*CYDLGFRAME,
                  iCBCX + iCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, CB_WAKEUPBYKEYBOARDONLY), &swpTemp3);
  internal_GetStaticTextSize(hwnd, ST_STARTSAVERAFTER, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, ST_STARTSAVERAFTER),
                  HWND_TOP,
                  swpTemp.x + CXDLGFRAME,
                  swpTemp3.y + swpTemp3.cy + 2*CYDLGFRAME,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_STARTSAVERAFTER), &swpTemp3);
  WinSetWindowPos(WinWindowFromID(hwnd, SPB_TIMEOUT),
                  HWND_TOP,
                  swpTemp3.x + swpTemp3.cx + CXDLGFRAME,
                  swpTemp3.y - CYDLGFRAME/2,
                  CXDLGFRAME*12,
                  staticheight + CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, SPB_TIMEOUT), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_MINUTESOFINACTIVITY),
                  HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
                  swpTemp3.y,
                  swpTemp.cx - 2*CXDLGFRAME - (swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME),
                  staticheight,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_STARTSAVERAFTER), &swpTemp2);
  internal_GetStaticTextSize(hwnd, CB_ENABLED, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, CB_ENABLED),
                  HWND_TOP,
                  3*CXDLGFRAME,
                  swpTemp2.y + swpTemp2.cy + 2*CYDLGFRAME,
                  iCBCX + iCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);


  // That's all!
  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);

}

static void ResizePage2Controls(HWND hwnd)
{
  SWP swpTemp, swpTemp2, swpTemp3, swpWindow;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  int staticheight, checkboxheight, gbheight;
  int iCBCX, iCBCY; // Checkbox extra sizes
  int iCX, iCY;     // Control size
  int iDisabledWindowUpdate=0;
  int iSpinStartPos;

  if (WinIsWindowVisible(hwnd))
  {
    WinEnableWindowUpdate(hwnd, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Get window size!
  WinQueryWindowPos(hwnd, &swpWindow);

  // Query static height in pixels. 
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SWITCHTOSTANDBYSTATEAFTER), &swpTemp);
  staticheight = swpTemp.cy;
  if (swpWindow.cx < swpTemp.cx) swpWindow.cx = swpTemp.cx;

  // Query checkbox height in pixels.
  WinQueryWindowPos(WinWindowFromID(hwnd, CB_USEDPMSOFFSTATE), &swpTemp);
  checkboxheight = swpTemp.cy;
  internal_GetCheckboxExtraSize(hwnd, CB_USEDPMSOFFSTATE, &iCBCX, &iCBCY);

  // Set the "DPMS Settings" groupbox
  gbheight =
    CYDLGFRAME + // frame
    2*CYDLGFRAME + // Margin
    staticheight + CYDLGFRAME + // Entry field
    CYDLGFRAME + // Margin
    checkboxheight + // Checkbox
    CYDLGFRAME + // Margin
    staticheight + CYDLGFRAME + // Entry field
    CYDLGFRAME + // Margin
    checkboxheight + // Checkbox
    CYDLGFRAME + // Margin
    staticheight + CYDLGFRAME + // Entry field
    CYDLGFRAME + // Margin
    checkboxheight + // Checkbox
    2*CYDLGFRAME + // Margin
    staticheight; // Text of groupbox

  WinSetWindowPos(WinWindowFromID(hwnd, GB_DPMSSETTINGS),
		  HWND_TOP,
                  2*CXDLGFRAME,
                  2*CYDLGFRAME,
                  swpWindow.cx - 4*CXDLGFRAME,
                  gbheight,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, GB_DPMSSETTINGS), &swpTemp);

  //Things inside this groupbox

  // First select the longest text of the three "Use xxx state" texts!
  iSpinStartPos = 0;
  internal_GetStaticTextSize(hwnd, ST_SWITCHTOSTANDBYSTATEAFTER, &iCX, &iCY); // Autosize!
  if (iSpinStartPos<iCX) iSpinStartPos = iCX;
  internal_GetStaticTextSize(hwnd, ST_SWITCHTOSUSPENDSTATEAFTER, &iCX, &iCY); // Autosize!
  if (iSpinStartPos<iCX) iSpinStartPos = iCX;
  internal_GetStaticTextSize(hwnd, ST_SWITCHTOOFFSTATEAFTER, &iCX, &iCY); // Autosize!
  if (iSpinStartPos<iCX) iSpinStartPos = iCX;
  iSpinStartPos += swpTemp.x + 3*CXDLGFRAME;

  // Now put there the stuffs

  internal_GetStaticTextSize(hwnd, ST_SWITCHTOOFFSTATEAFTER, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, ST_SWITCHTOOFFSTATEAFTER),
                  HWND_TOP,
                  swpTemp.x + CXDLGFRAME,
                  swpTemp.y + 3*CYDLGFRAME,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SWITCHTOOFFSTATEAFTER), &swpTemp3);
  WinSetWindowPos(WinWindowFromID(hwnd, SPB_OFFTIMEOUT),
                  HWND_TOP,
                  iSpinStartPos,
                  swpTemp3.y - CYDLGFRAME/2,
                  CXDLGFRAME * 12,
                  staticheight + CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, SPB_OFFTIMEOUT), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_OFFMINUTESTEXT),
                  HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
                  swpTemp3.y,
                  swpTemp.x + swpTemp.cx - 2*CXDLGFRAME - (swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME),
                  staticheight,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SWITCHTOOFFSTATEAFTER), &swpTemp2);
  internal_GetStaticTextSize(hwnd, CB_USEDPMSOFFSTATE, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, CB_USEDPMSOFFSTATE),
                  HWND_TOP,
                  3*CXDLGFRAME,
                  swpTemp2.y + swpTemp2.cy + CYDLGFRAME,
                  iCBCX + iCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);

  internal_GetStaticTextSize(hwnd, ST_SWITCHTOSUSPENDSTATEAFTER, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, ST_SWITCHTOSUSPENDSTATEAFTER),
                  HWND_TOP,
                  swpTemp2.x,
                  swpTemp2.y + 4*CYDLGFRAME  + checkboxheight + staticheight,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SWITCHTOSUSPENDSTATEAFTER), &swpTemp3);
  WinSetWindowPos(WinWindowFromID(hwnd, SPB_SUSPENDTIMEOUT),
                  HWND_TOP,
                  iSpinStartPos,
                  swpTemp3.y - CYDLGFRAME/2,
                  CXDLGFRAME * 12,
                  staticheight + CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, SPB_SUSPENDTIMEOUT), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_SUSPENDMINUTESTEXT),
                  HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
                  swpTemp3.y,
                  swpTemp.x + swpTemp.cx - 2*CXDLGFRAME - (swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME),
                  staticheight,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SWITCHTOSUSPENDSTATEAFTER), &swpTemp2);
  internal_GetStaticTextSize(hwnd, CB_USEDPMSSUSPENDSTATE, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, CB_USEDPMSSUSPENDSTATE),
                  HWND_TOP,
                  3*CXDLGFRAME,
                  swpTemp2.y + swpTemp2.cy + CYDLGFRAME,
                  iCBCX + iCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);

  internal_GetStaticTextSize(hwnd, ST_SWITCHTOSTANDBYSTATEAFTER, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, ST_SWITCHTOSTANDBYSTATEAFTER),
                  HWND_TOP,
                  swpTemp2.x,
                  swpTemp2.y + 4*CYDLGFRAME  + checkboxheight + staticheight,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SWITCHTOSTANDBYSTATEAFTER), &swpTemp3);
  WinSetWindowPos(WinWindowFromID(hwnd, SPB_STANDBYTIMEOUT),
                  HWND_TOP,
                  iSpinStartPos,
                  swpTemp3.y - CYDLGFRAME/2,
                  CXDLGFRAME * 12,
                  staticheight + CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, SPB_STANDBYTIMEOUT), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_STANDBYMINUTESTEXT),
                  HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
                  swpTemp3.y,
                  swpTemp.x + swpTemp.cx - 2*CXDLGFRAME - (swpTemp2.x + swpTemp2.cx + 2*CXDLGFRAME),
                  staticheight,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SWITCHTOSTANDBYSTATEAFTER), &swpTemp2);
  internal_GetStaticTextSize(hwnd, CB_USEDPMSSTANDBYSTATE, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, CB_USEDPMSSTANDBYSTATE),
                  HWND_TOP,
                  3*CXDLGFRAME,
                  swpTemp2.y + swpTemp2.cy + CYDLGFRAME,
                  iCBCX + iCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);

  // That's all!
  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);

}

static void ResizePage3Controls(HWND hwnd)
{
  SWP swpTemp, swpTemp2, swpTemp3, swpWindow;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  int staticheight, checkboxheight, buttonheight;
  int paheight, pawidth, pax, pay;
  int iCBCX, iCBCY; // Checkbox extra sizes
  int iCX, iCY;     // Control size
  int iDisabledWindowUpdate=0;

  if (WinIsWindowVisible(hwnd))
  {
    WinEnableWindowUpdate(hwnd, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Get window size!
  WinQueryWindowPos(hwnd, &swpWindow);

  // Query static height in pixels.
  internal_GetStaticTextSize(hwnd, ST_SUPPORTSPASSWORDPROTECTION, &iCX, &iCY); // Autosize!
  staticheight = iCY;
  if (swpWindow.cx < iCX*3/2) swpWindow.cx = iCX*3/2;

  // Query button height in pixels.
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_STARTMODULE), &swpTemp);
  buttonheight = swpTemp.cy;

  // Query checkbox height in pixels.
  WinQueryWindowPos(WinWindowFromID(hwnd, CB_SHOWPREVIEW), &swpTemp);
  checkboxheight = swpTemp.cy;
  internal_GetCheckboxExtraSize(hwnd, CB_SHOWPREVIEW, &iCBCX, &iCBCY);

  // Set the groupbox
  WinSetWindowPos(WinWindowFromID(hwnd, GB_SCREENSAVERMODULES), HWND_TOP,
                  2*CXDLGFRAME,
                  2*CYDLGFRAME,
                  swpWindow.cx - 4*CXDLGFRAME,
                  swpWindow.cy - 4*CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SCREENSAVERMODULES), &swpTemp);
  // First set the preview window!
  // Its width will be half the groupbox minus the margins, and
  // the height will be 3:4 to the width (like TV)
  pax = swpTemp.x + 2*CXDLGFRAME;
  pay = swpTemp.y + 2*CYDLGFRAME;
  pawidth = (swpTemp.cx - 5*CXDLGFRAME)/2;
  paheight = ((swpTemp.cx - 5*CXDLGFRAME)/2)*3/4;
  if (paheight > swpTemp.cy/2)
  {
    paheight = swpTemp.cy/2;
    pawidth = paheight*4/3;
    pax += ((swpTemp.cx - 5*CXDLGFRAME)/2 - pawidth)/2;
  }
  WinSetWindowPos(WinWindowFromID(hwnd, ID_PREVIEWAREA), HWND_TOP,
                  pax, pay,
                  pawidth, paheight,
                  SWP_MOVE | SWP_SIZE);

  // Put the "Show preview" checkbox above the preview area

  internal_GetStaticTextSize(hwnd, CB_SHOWPREVIEW, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, CB_SHOWPREVIEW), HWND_TOP,
                  swpTemp.x + 2*CXDLGFRAME,
                  pay + paheight + CYDLGFRAME,
                  iCBCX + iCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);

  // Let the listbox get the remaining space at this side!
  WinQueryWindowPos(WinWindowFromID(hwnd, CB_SHOWPREVIEW), &swpTemp3);
  WinSetWindowPos(WinWindowFromID(hwnd, LB_MODULELIST), HWND_TOP,
                  swpTemp3.x,
                  swpTemp3.y + swpTemp3.cy + CYDLGFRAME,
                  (swpTemp.cx - 5*CXDLGFRAME)/2,
                  swpTemp.cy - staticheight - 4*CYDLGFRAME - swpTemp3.cy - CYDLGFRAME - paheight,
                  SWP_MOVE | SWP_SIZE);

  // Right side
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_SCREENSAVERMODULES), &swpTemp);
  WinQueryWindowPos(WinWindowFromID(hwnd, LB_MODULELIST), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, GB_MODULEINFORMATION),
		  HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
                  swpTemp.y + 2*CYDLGFRAME,
                  (swpTemp.cx - 5*CXDLGFRAME)/2,
                  swpTemp.cy - staticheight - 2*CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, GB_MODULEINFORMATION), &swpTemp);

  // Pushbuttons
  WinSetWindowPos(WinWindowFromID(hwnd, PB_STARTMODULE),
		  HWND_TOP,
                  swpTemp.x + 2*CXDLGFRAME,
                  swpTemp.y + 2*CYDLGFRAME,
                  (swpTemp.cx - 5*CXDLGFRAME)/2,
                  buttonheight,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_STARTMODULE), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_CONFIGURE),
		  HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
                  swpTemp2.y,
                  (swpTemp.cx - 5*CXDLGFRAME)/2,
                  buttonheight,
                  SWP_MOVE | SWP_SIZE);

  // Module info statics
  internal_GetStaticTextSize(hwnd, ST_MODULENAME, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, ST_MODULENAME),
		  HWND_TOP,
                  swpTemp.x + 2*CXDLGFRAME,
                  swpTemp.y + swpTemp.cy - staticheight - CYDLGFRAME - staticheight,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_MODULENAME), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_SELECTEDMODULENAME),
		  HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
                  swpTemp2.y,
                  swpTemp.cx - 5*CXDLGFRAME - swpTemp2.cx,
                  staticheight,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_MODULENAME), &swpTemp2);
  internal_GetStaticTextSize(hwnd, ST_MODULEVERSION, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, ST_MODULEVERSION),
		  HWND_TOP,
                  swpTemp2.x,
                  swpTemp2.y - CYDLGFRAME - staticheight,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_MODULEVERSION), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_SELECTEDMODULEVERSION),
		  HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
                  swpTemp2.y,
                  swpTemp.cx - 5*CXDLGFRAME - swpTemp2.cx,
                  staticheight,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_MODULEVERSION), &swpTemp2);
  internal_GetStaticTextSize(hwnd, ST_SUPPORTSPASSWORDPROTECTION, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, ST_SUPPORTSPASSWORDPROTECTION),
		  HWND_TOP,
                  swpTemp2.x,
                  swpTemp2.y - CYDLGFRAME - staticheight,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SUPPORTSPASSWORDPROTECTION), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_SELECTEDSUPPORTSPASSWORDPROTECTION),
		  HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
                  swpTemp2.y,
                  swpTemp.cx - 5*CXDLGFRAME - swpTemp2.cx,
                  staticheight,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_SUPPORTSPASSWORDPROTECTION), &swpTemp2);
  internal_GetStaticTextSize(hwnd, ST_DESCRIPTION, &iCX, &iCY); // Autosize!
  WinSetWindowPos(WinWindowFromID(hwnd, ST_DESCRIPTION),
		  HWND_TOP,
                  swpTemp2.x,
                  swpTemp2.y - 2*CYDLGFRAME - staticheight,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, ST_DESCRIPTION), &swpTemp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_STARTMODULE), &swpTemp3);
  WinSetWindowPos(WinWindowFromID(hwnd, MLE_SELECTEDMODULEDESCRIPTION),
		  HWND_TOP,
                  swpTemp2.x,
                  swpTemp3.y + swpTemp3.cy + 2*CYDLGFRAME,
                  swpTemp.cx - 4*CXDLGFRAME,
                  swpTemp2.y - CYDLGFRAME - (swpTemp3.y + swpTemp3.cy + 2*CYDLGFRAME),
                  SWP_MOVE | SWP_SIZE);


  // That's all!
  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);

}

static void SetPage1ControlLimits(HWND hwnd)
{
  WinSendDlgItemMsg(hwnd, SPB_TIMEOUT,
                    SPBM_SETLIMITS,
                    (MPARAM) MAX_SAVER_TIMEOUT,
                    (MPARAM) MIN_SAVER_TIMEOUT);

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

static void SetPage2ControlLimits(HWND hwnd)
{
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

static void SetPage3ControlLimits(HWND hwnd)
{
  WinSendDlgItemMsg(hwnd, MLE_SELECTEDMODULEDESCRIPTION,
		    MLM_SETTEXTLIMIT,
		    (MPARAM) SSMODULE_MAXDESCLEN,
                    (MPARAM) NULL);
}

static void EnableDisablePage1Controls(HWND hwnd)
{
  SSCoreInfo_t SSCoreInfo;
  int bValue1, bValue2, bValue3;

  // Get info if Security/2 is installed or not!
  if (SSCore_GetInfo(&SSCoreInfo, sizeof(SSCoreInfo))!=SSCORE_NOERROR)
    SSCoreInfo.bSecurityPresent = FALSE;

  // Enable/Disable elements of first groupbox
  bValue1 = WinQueryButtonCheckstate(hwnd, CB_ENABLED);

  WinEnableWindow(WinWindowFromID(hwnd, ST_STARTSAVERAFTER), bValue1);
  WinEnableWindow(WinWindowFromID(hwnd, SPB_TIMEOUT), bValue1);
  WinEnableWindow(WinWindowFromID(hwnd, ST_MINUTESOFINACTIVITY), bValue1);

  WinEnableWindow(WinWindowFromID(hwnd, CB_WAKEUPBYKEYBOARDONLY), bValue1);
  WinEnableWindow(WinWindowFromID(hwnd, CB_DISABLEVIOPOPUPSWHILESAVING), bValue1);

  // Query elements for the remaining groupboxes
  bValue1 = WinQueryButtonCheckstate(hwnd, CB_USEPASSWORDPROTECTION);
  bValue2 = WinQueryButtonCheckstate(hwnd, CB_DELAYPASSWORDPROTECTION);
  bValue3 = WinQueryButtonCheckstate(hwnd, RB_USETHISPASSWORD);

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

static void EnableDisablePage2Controls(HWND hwnd)
{
  SSCoreInfo_t SSCoreInfo;
  int bStandbyEnabled, bSuspendEnabled, bOffEnabled;
  Page2UserData_p pUserData;
  char *pchTemp;
  FILE *hfNLSFile;

  pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (pUserData)
  {
    hfNLSFile = internal_OpenNLSFile(pUserData->Desktop);

    if (hfNLSFile)
      pchTemp = (char *) malloc(1024);
    else
      pchTemp = NULL;


    // Get DPMS capabilities!
    if (SSCore_GetInfo(&SSCoreInfo, sizeof(SSCoreInfo))!=SSCORE_NOERROR)
      SSCoreInfo.iDPMSCapabilities = SSCORE_DPMSCAPS_NODPMS;

    // Get current settings!
    bStandbyEnabled = wpssdesktop_wpssQueryUseDPMSStandby(pUserData->Desktop);
    bSuspendEnabled = wpssdesktop_wpssQueryUseDPMSSuspend(pUserData->Desktop);
    bOffEnabled = wpssdesktop_wpssQueryUseDPMSOff(pUserData->Desktop);

    // Enable/Disable Standby checkbox and spinbutton
    WinEnableWindow(WinWindowFromID(hwnd, CB_USEDPMSSTANDBYSTATE),
                    SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_STANDBY);

    WinShowWindow(WinWindowFromID(hwnd, ST_SWITCHTOSTANDBYSTATEAFTER),
                  ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_STANDBY) &&
                   bStandbyEnabled));
    WinShowWindow(WinWindowFromID(hwnd, SPB_STANDBYTIMEOUT),
                  ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_STANDBY) &&
                   bStandbyEnabled));
    WinShowWindow(WinWindowFromID(hwnd, ST_STANDBYMINUTESTEXT),
                  ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_STANDBY) &&
                   bStandbyEnabled));


    // Enable/Disable Suspend checkbox and spinbutton
    WinEnableWindow(WinWindowFromID(hwnd, CB_USEDPMSSUSPENDSTATE),
                    SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_SUSPEND);
    WinShowWindow(WinWindowFromID(hwnd, ST_SWITCHTOSUSPENDSTATEAFTER),
                  ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_SUSPEND) &&
                   bSuspendEnabled));
    WinShowWindow(WinWindowFromID(hwnd, SPB_SUSPENDTIMEOUT),
                  ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_SUSPEND) &&
                   bSuspendEnabled));
    WinShowWindow(WinWindowFromID(hwnd, ST_SUSPENDMINUTESTEXT),
                  ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_SUSPEND) &&
                   bSuspendEnabled));

    // Enable/Disable Off checkbox and spinbutton
    WinEnableWindow(WinWindowFromID(hwnd, CB_USEDPMSOFFSTATE),
                    SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_OFF);
    WinShowWindow(WinWindowFromID(hwnd, ST_SWITCHTOOFFSTATEAFTER),
                  ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_OFF) &&
                   bOffEnabled));
    WinShowWindow(WinWindowFromID(hwnd, SPB_OFFTIMEOUT),
                  ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_OFF) &&
                   bOffEnabled));
    WinShowWindow(WinWindowFromID(hwnd, ST_OFFMINUTESTEXT),
                  ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_OFF) &&
                   bOffEnabled));

    // Now update texts to reflect the order of states!
    // Let's set the first one!
    if ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_STANDBY) &&
        bStandbyEnabled)
    {
      // The first enabled and available mode is the standby mode!
      if ((pchTemp) && (sprintmsg(pchTemp, hfNLSFile, "PG2_0010")))
        WinSetDlgItemText(hwnd, ST_STANDBYMINUTESTEXT, pchTemp);
      else
	WinSetDlgItemText(hwnd, ST_STANDBYMINUTESTEXT, "minute(s) of saving");

      // Look for the next one!
      if ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_SUSPEND) &&
          bSuspendEnabled)
      {
	// The next one is the suspend mode!
        if ((pchTemp) && (sprintmsg(pchTemp, hfNLSFile, "PG2_0011")))
	  WinSetDlgItemText(hwnd, ST_SUSPENDMINUTESTEXT, pchTemp);
	else
	  WinSetDlgItemText(hwnd, ST_SUSPENDMINUTESTEXT, "minute(s) of Standby state");
        // Look for the next one!
        if ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_OFF) &&
            bOffEnabled)
	{
	  if ((pchTemp) && (sprintmsg(pchTemp, hfNLSFile, "PG2_0012")))
	    WinSetDlgItemText(hwnd, ST_OFFMINUTESTEXT, pchTemp);
	  else
	    WinSetDlgItemText(hwnd, ST_OFFMINUTESTEXT, "minute(s) of Suspend state");
        }
      } else
      {
        // Suspend is skipped, maybe Off mode?
        if ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_OFF) &&
            bOffEnabled)
	{
	  if ((pchTemp) && (sprintmsg(pchTemp, hfNLSFile, "PG2_0011")))
	    WinSetDlgItemText(hwnd, ST_OFFMINUTESTEXT, pchTemp);
	  else
	    WinSetDlgItemText(hwnd, ST_OFFMINUTESTEXT, "minute(s) of Standby state");
        }
      }
    }
    else
    if ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_SUSPEND) &&
        bSuspendEnabled)
    {
      // The first enabled and available mode is the Suspend mode!
      if ((pchTemp) && (sprintmsg(pchTemp, hfNLSFile, "PG2_0010")))
	WinSetDlgItemText(hwnd, ST_SUSPENDMINUTESTEXT, pchTemp);
      else
	WinSetDlgItemText(hwnd, ST_SUSPENDMINUTESTEXT, "minute(s) of saving");
      // Look for the next one!
      if ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_OFF) &&
          bOffEnabled)
      {
	// The next one is the Off mode!
	if ((pchTemp) && (sprintmsg(pchTemp, hfNLSFile, "PG2_0012")))
	  WinSetDlgItemText(hwnd, ST_OFFMINUTESTEXT, pchTemp);
	else
	  WinSetDlgItemText(hwnd, ST_OFFMINUTESTEXT, "minute(s) of Suspend state");
      }
    }
    else
    if ((SSCoreInfo.iDPMSCapabilities & SSCORE_DPMSCAPS_STATE_OFF) &&
        bOffEnabled)
    {
      // The first enabled and available mode is the Off mode!
      if ((pchTemp) && (sprintmsg(pchTemp, hfNLSFile, "PG2_0010")))
        WinSetDlgItemText(hwnd, ST_OFFMINUTESTEXT, pchTemp);
      else
        WinSetDlgItemText(hwnd, ST_OFFMINUTESTEXT, "minute(s) of saving");
    }

    internal_CloseNLSFile(hfNLSFile);
  }

  if (pchTemp)
    free(pchTemp);
}

static void SetPage1ControlValues(HWND hwnd)
{
  Page1UserData_p pUserData;
  char achTemp[SSCORE_MAXPASSWORDLEN];
  pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (pUserData)
  {
    WinCheckButton(hwnd, CB_ENABLED, wpssdesktop_wpssQueryScreenSaverEnabled(pUserData->Desktop));
    WinSendDlgItemMsg(hwnd, SPB_TIMEOUT, SPBM_SETCURRENTVALUE,
		      (MPARAM) (wpssdesktop_wpssQueryScreenSaverTimeout(pUserData->Desktop)/60000),
                      (MPARAM) NULL);
    WinCheckButton(hwnd, CB_WAKEUPBYKEYBOARDONLY, wpssdesktop_wpssQueryWakeByKeyboardOnly(pUserData->Desktop));
    WinCheckButton(hwnd, CB_DISABLEVIOPOPUPSWHILESAVING, wpssdesktop_wpssQueryDisableVIOPopupsWhileSaving(pUserData->Desktop));
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
    EnableDisablePage1Controls(hwnd);
  }
}
static void SetPage2ControlValues(HWND hwnd)
{
  Page2UserData_p pUserData;
  pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (pUserData)
  {
    WinCheckButton(hwnd, CB_USEDPMSSTANDBYSTATE, wpssdesktop_wpssQueryUseDPMSStandby(pUserData->Desktop));
    WinSendDlgItemMsg(hwnd, SPB_STANDBYTIMEOUT, SPBM_SETCURRENTVALUE,
		      (MPARAM) (wpssdesktop_wpssQueryDPMSStandbyTimeout(pUserData->Desktop)/60000),
		      (MPARAM) NULL);
    WinCheckButton(hwnd, CB_USEDPMSSUSPENDSTATE, wpssdesktop_wpssQueryUseDPMSSuspend(pUserData->Desktop));
    WinSendDlgItemMsg(hwnd, SPB_SUSPENDTIMEOUT, SPBM_SETCURRENTVALUE,
		      (MPARAM) (wpssdesktop_wpssQueryDPMSSuspendTimeout(pUserData->Desktop)/60000),
		      (MPARAM) NULL);
    WinCheckButton(hwnd, CB_USEDPMSOFFSTATE, wpssdesktop_wpssQueryUseDPMSOff(pUserData->Desktop));
    WinSendDlgItemMsg(hwnd, SPB_OFFTIMEOUT, SPBM_SETCURRENTVALUE,
		      (MPARAM) (wpssdesktop_wpssQueryDPMSOffTimeout(pUserData->Desktop)/60000),
		      (MPARAM) NULL);
    EnableDisablePage2Controls(hwnd);
  }
}
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
#ifdef DEBUG_LOGGING
      AddLog("Start preview: ["); AddLog(pUserData->pModuleDescArray[lID].achSaverDLLFileName); AddLog("]\n");
#endif
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
static void SetPage3ModuleInfo(HWND hwnd)
{
  long lID;
  Page3UserData_p pUserData;
  char achVer[128];
  FILE *hfNLSFile;

  pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (pUserData)
  {
    hfNLSFile = internal_OpenNLSFile(pUserData->Desktop);

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
    if (lID==LIT_NONE)
    {
      // Either there is no module selected, or something went wrong.
      WinSetDlgItemText(hwnd, ST_SELECTEDMODULENAME, "");
      WinSetDlgItemText(hwnd, ST_SELECTEDMODULEVERSION, "");
      WinSetDlgItemText(hwnd, ST_SELECTEDSUPPORTSPASSWORDPROTECTION, "");
      if (!hfNLSFile) // No NLS support
        WinSetDlgItemText(hwnd, MLE_SELECTEDMODULEDESCRIPTION, "No module selected!\nPlease select a screen saver module!");
      else
      {
        // There is NLS support, read text!
        char *pchTemp = (char *) malloc(1024);
        if (pchTemp)
        {
          if (sprintmsg(pchTemp, hfNLSFile, "PG3_0014"))
	    WinSetDlgItemText(hwnd, MLE_SELECTEDMODULEDESCRIPTION, pchTemp);
	  else
            WinSetDlgItemText(hwnd, MLE_SELECTEDMODULEDESCRIPTION, "No module selected!\nPlease select a screen saver module!");
          free(pchTemp);
        } else
          WinSetDlgItemText(hwnd, MLE_SELECTEDMODULEDESCRIPTION, "No module selected!\nPlease select a screen saver module!");
      }
      WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE), FALSE);
      StopPage3Preview(hwnd);
    } else
    {
      // Ok, we have the index of the selected module in lID!
      char *pchYes, *pchNo;

      WinSetDlgItemText(hwnd, ST_SELECTEDMODULENAME, pUserData->pModuleDescArray[lID].achModuleName);
      sprintf(achVer, "%d.%02d",
	      pUserData->pModuleDescArray[lID].lVersionMajor,
              pUserData->pModuleDescArray[lID].lVersionMinor);
      WinSetDlgItemText(hwnd, ST_SELECTEDMODULEVERSION, achVer);

      if (hfNLSFile)
      {
        // There is NLS support, read text!
        pchYes = (char *) malloc(1024);
        pchNo = (char *) malloc(1024);
        if (pchYes)
          sprintmsg(pchYes, hfNLSFile, "PG3_0009");
        if (pchNo)
          sprintmsg(pchNo, hfNLSFile, "PG3_0010");
      } else
      {
        pchYes = NULL;
        pchNo = NULL;
      }
      if ((pchYes) && (pchNo))
        WinSetDlgItemText(hwnd, ST_SELECTEDSUPPORTSPASSWORDPROTECTION, pUserData->pModuleDescArray[lID].bSupportsPasswordProtection?pchYes:pchNo);
      else
        WinSetDlgItemText(hwnd, ST_SELECTEDSUPPORTSPASSWORDPROTECTION, pUserData->pModuleDescArray[lID].bSupportsPasswordProtection?"Yes":"No");

      if (pchYes)
        free(pchYes);
      if (pchNo)
        free(pchNo);

      WinSetDlgItemText(hwnd, MLE_SELECTEDMODULEDESCRIPTION, pUserData->pModuleDescArray[lID].achModuleDesc);
      WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE), pUserData->pModuleDescArray[lID].bConfigurable);
      if (WinQueryButtonCheckstate(hwnd, CB_SHOWPREVIEW))
      {
	if (WinIsWindowVisible(hwnd))
	  StartPage3Preview(hwnd);
      } else
      {
	StopPage3Preview(hwnd);
      }
    }

    internal_CloseNLSFile(hfNLSFile);

  }
}
static void SetPage3ControlValues(HWND hwnd, int bStartOrStopPreview)
{
  Page3UserData_p pUserData;
  char achCurrModule[CCHMAXPATHCOMP];
  ULONG ulTemp, ulCount;
  long lID, lIDToSelect;

  pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (pUserData)
  {
    // Get current saver DLL module!
    wpssdesktop_wpssQueryScreenSaverModule(pUserData->Desktop,
                                           achCurrModule, sizeof(achCurrModule));
#ifdef DEBUG_LOGGING
    AddLog("[SetPage3ControlValues] : Current saver module: [");
    AddLog(achCurrModule); AddLog("]\n");
#endif
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
      if (!strcmp(pUserData->pModuleDescArray[ulTemp].achSaverDLLFileName,
                  achCurrModule))
      {
#ifdef DEBUG_LOGGING
        AddLog("[SetPage3ControlValues] : Found module!\n");
#endif
        // This is the currently selected screen saver!
        // We might save the item ID, so we can select that item later.
        // The problem is that we're inserting items in sorted mode, so
        // item indexes will not be valid. So, we save item handle here, and
        // search for item with that handle later!
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
    WinCheckButton(hwnd, CB_SHOWPREVIEW,
                   ulTemp = wpssdesktop_wpssQueryScreenSaverPreviewEnabled(pUserData->Desktop));
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
          (sprintmsg(pchTemp1, hfNLSFile, "PG1_0018")) &&
          (sprintmsg(pchTemp2, hfNLSFile, "PG1_0017"))
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
#ifdef DEBUG_LOGGING
      AddLog("[PreviewWndProc] : SSCORENOTIFY! Forwarding to parent!\n");
#endif
      hwndParent = WinQueryWindow(hwnd, QW_PARENT);
      return WinSendMsg(hwndParent, msg, mp1, mp2);

    default:
#ifdef DEBUG_LOGGING
      /*
      {
        char achTemp[128];
        sprintf(achTemp,"[PreviewWndProc] : Unprocessed: 0x%x\n", msg);
        AddLog(achTemp);
      }
      */
#endif
      break;
  }
  return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}

static void SetPage1Text(HWND hwnd)
{
  char *pchTemp;
  int iCX, iCY, iMaxCX;
  SWP swpNotebook, swpBtn;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  FILE *hfNLSFile;
  Page1UserData_p pUserData;

  pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (pUserData)
    hfNLSFile = internal_OpenNLSFile(pUserData->Desktop);
  else
    hfNLSFile = NULLHANDLE;

  // Only if we have an NLS support file opened!
  if (hfNLSFile)
  {
    // Allocate memory for strings we're gonna read!
    pchTemp = (char *) malloc(1024);
    if (pchTemp)
    {
      // Okay, read strings and change control texts!
      // First the Undo/Default/Help buttons!
      iMaxCX = 0;
      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0000"))
      {
        WinSetDlgItemText(hwndSettingsNotebook, PB_NOTEBOOK_PG1_HELP, pchTemp);
        WinSetDlgItemText(hwnd, PB_NOTEBOOK_PG1_HELP, pchTemp);
        internal_GetStaticTextSize(hwndSettingsNotebook, PB_NOTEBOOK_PG1_HELP, &iCX, &iCY);
        if (iMaxCX<iCX) iMaxCX = iCX;
      }

      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0001"))
      {
        WinSetDlgItemText(hwndSettingsNotebook, PB_NOTEBOOK_PG1_DEFAULT, pchTemp);
        WinSetDlgItemText(hwnd, PB_NOTEBOOK_PG1_DEFAULT, pchTemp);
        internal_GetStaticTextSize(hwndSettingsNotebook, PB_NOTEBOOK_PG1_DEFAULT, &iCX, &iCY);
        if (iMaxCX<iCX) iMaxCX = iCX;
      }

      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0002"))
      {
        WinSetDlgItemText(hwndSettingsNotebook, PB_NOTEBOOK_PG1_UNDO, pchTemp);
        WinSetDlgItemText(hwnd, PB_NOTEBOOK_PG1_UNDO, pchTemp);
        internal_GetStaticTextSize(hwndSettingsNotebook, PB_NOTEBOOK_PG1_UNDO, &iCX, &iCY);
        if (iMaxCX<iCX) iMaxCX = iCX;
      }
      // Now resize and reposition the buttons!
      WinQueryWindowPos(hwndSettingsNotebook, &swpNotebook);
      WinQueryWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG1_UNDO), &swpBtn);
      WinSetWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG1_UNDO), HWND_TOP,
                      swpNotebook.cx - 20*CXDLGFRAME - 3*iMaxCX,
                      swpBtn.y,
                      iMaxCX+6*CXDLGFRAME,
                      swpBtn.cy,
                      SWP_MOVE | SWP_SIZE);

      WinQueryWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG1_DEFAULT), &swpBtn);
      WinSetWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG1_DEFAULT), HWND_TOP,
                      swpNotebook.cx - 20*CXDLGFRAME - 3*iMaxCX +7*CXDLGFRAME+iMaxCX,
                      swpBtn.y,
                      iMaxCX+6*CXDLGFRAME,
                      swpBtn.cy,
                      SWP_MOVE | SWP_SIZE);

      WinQueryWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG1_HELP), &swpBtn);
      WinSetWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG1_HELP), HWND_TOP,
                      swpNotebook.cx - 20*CXDLGFRAME - 3*iMaxCX +14*CXDLGFRAME+2*iMaxCX,
                      swpBtn.y,
                      iMaxCX+6*CXDLGFRAME,
                      swpBtn.cy,
                      SWP_MOVE | SWP_SIZE);

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
	WinSetDlgItemText(hwnd, GB_PASSWORDPROTECTION, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0008"))
	WinSetDlgItemText(hwnd, CB_USEPASSWORDPROTECTION, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0009"))
	WinSetDlgItemText(hwnd, ST_TYPEYOURPASSWORD, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0010"))
      {
	WinSetDlgItemText(hwnd, ST_PASSWORD, pchTemp);
        WinSetDlgItemText(hwnd, ST_PASSWORDFORVERIFICATION, pchTemp);
      }
      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0011"))
        WinSetDlgItemText(hwnd, ST_FORVERIFICATION, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0012"))
        WinSetDlgItemText(hwnd, PB_CHANGE, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0013"))
        WinSetDlgItemText(hwnd, CB_DELAYPASSWORDPROTECTION, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0014"))
        WinSetDlgItemText(hwnd, ST_ASKPASSWORDONLYAFTER, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0015"))
        WinSetDlgItemText(hwnd, ST_MINUTESOFSAVING, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0016"))
        WinSetDlgItemText(hwnd, CB_STARTSAVERMODULEONSTARTUP, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0019"))
        WinSetDlgItemText(hwnd, RB_USEPASSWORDOFCURRENTSECURITYUSER, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0020"))
        WinSetDlgItemText(hwnd, RB_USETHISPASSWORD, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0021"))
	WinSetDlgItemText(hwnd, CB_WAKEUPBYKEYBOARDONLY, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0022"))
        WinSetDlgItemText(hwnd, CB_MAKETHEFIRSTKEYPRESSTHEFIRSTKEYOFTHEPASSWORD, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG1_0023"))
        WinSetDlgItemText(hwnd, CB_DISABLEVIOPOPUPSWHILESAVING, pchTemp);

      free(pchTemp);
    }

  }

  internal_CloseNLSFile(hfNLSFile);

  WinInvalidateRect(hwndSettingsNotebook, NULL, TRUE);
}

static void SetPage2Text(HWND hwnd)
{
  char *pchTemp;
  int iCX, iCY, iMaxCX;
  SWP swpNotebook, swpBtn;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  Page2UserData_p pUserData;
  FILE *hfNLSFile;

  pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (pUserData)
    hfNLSFile = internal_OpenNLSFile(pUserData->Desktop);
  else
    hfNLSFile = NULLHANDLE;

  // Only if we have an NLS support file opened!
  if (hfNLSFile)
  {
    // Allocate memory for strings we're gonna read!
    pchTemp = (char *) malloc(1024);
    if (pchTemp)
    {
      // Okay, read strings and change control texts!
      // First the Undo/Default/Help buttons!
      iMaxCX = 0;
      if (sprintmsg(pchTemp, hfNLSFile, "PG2_0000"))
      {
        WinSetDlgItemText(hwndSettingsNotebook, PB_NOTEBOOK_PG2_HELP, pchTemp);
        WinSetDlgItemText(hwnd, PB_NOTEBOOK_PG2_HELP, pchTemp);
        internal_GetStaticTextSize(hwndSettingsNotebook, PB_NOTEBOOK_PG2_HELP, &iCX, &iCY);
        if (iMaxCX<iCX) iMaxCX = iCX;
      }

      if (sprintmsg(pchTemp, hfNLSFile, "PG2_0001"))
      {
        WinSetDlgItemText(hwndSettingsNotebook, PB_NOTEBOOK_PG2_DEFAULT, pchTemp);
        WinSetDlgItemText(hwnd, PB_NOTEBOOK_PG2_DEFAULT, pchTemp);
        internal_GetStaticTextSize(hwndSettingsNotebook, PB_NOTEBOOK_PG2_DEFAULT, &iCX, &iCY);
        if (iMaxCX<iCX) iMaxCX = iCX;
      }

      if (sprintmsg(pchTemp, hfNLSFile, "PG2_0002"))
      {
        WinSetDlgItemText(hwndSettingsNotebook, PB_NOTEBOOK_PG2_UNDO, pchTemp);
        WinSetDlgItemText(hwnd, PB_NOTEBOOK_PG2_UNDO, pchTemp);
        internal_GetStaticTextSize(hwndSettingsNotebook, PB_NOTEBOOK_PG2_UNDO, &iCX, &iCY);
        if (iMaxCX<iCX) iMaxCX = iCX;
      }
      // Now resize and reposition the buttons!
      WinQueryWindowPos(hwndSettingsNotebook, &swpNotebook);
      WinQueryWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG2_UNDO), &swpBtn);
      WinSetWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG2_UNDO), HWND_TOP,
                      swpNotebook.cx - 20*CXDLGFRAME - 3*iMaxCX,
                      swpBtn.y,
                      iMaxCX+6*CXDLGFRAME,
                      swpBtn.cy,
                      SWP_MOVE | SWP_SIZE);

      WinQueryWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG2_DEFAULT), &swpBtn);
      WinSetWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG2_DEFAULT), HWND_TOP,
                      swpNotebook.cx - 20*CXDLGFRAME - 3*iMaxCX +7*CXDLGFRAME+iMaxCX,
                      swpBtn.y,
                      iMaxCX+6*CXDLGFRAME,
                      swpBtn.cy,
                      SWP_MOVE | SWP_SIZE);

      WinQueryWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG2_HELP), &swpBtn);
      WinSetWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG2_HELP), HWND_TOP,
                      swpNotebook.cx - 20*CXDLGFRAME - 3*iMaxCX +14*CXDLGFRAME+2*iMaxCX,
                      swpBtn.y,
                      iMaxCX+6*CXDLGFRAME,
                      swpBtn.cy,
                      SWP_MOVE | SWP_SIZE);

      // Then the controls
      if (sprintmsg(pchTemp, hfNLSFile, "PG2_0003"))
	WinSetDlgItemText(hwnd, GB_DPMSSETTINGS, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG2_0004"))
	WinSetDlgItemText(hwnd, CB_USEDPMSSTANDBYSTATE, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG2_0005"))
	WinSetDlgItemText(hwnd, ST_SWITCHTOSTANDBYSTATEAFTER, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG2_0006"))
	WinSetDlgItemText(hwnd, CB_USEDPMSSUSPENDSTATE, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG2_0007"))
	WinSetDlgItemText(hwnd, ST_SWITCHTOSUSPENDSTATEAFTER, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG2_0008"))
	WinSetDlgItemText(hwnd, CB_USEDPMSOFFSTATE, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG2_0009"))
	WinSetDlgItemText(hwnd, ST_SWITCHTOOFFSTATEAFTER, pchTemp);

      free(pchTemp);
    }
  }

  internal_CloseNLSFile(hfNLSFile);

  // Also, we have to set the dynamic text, too, so:
  EnableDisablePage2Controls(hwnd);
  WinInvalidateRect(hwndSettingsNotebook, NULL, TRUE);
}
static void SetPage3Text(HWND hwnd)
{
  char *pchTemp;
  int iCX, iCY, iMaxCX;
  SWP swpNotebook, swpBtn;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  Page3UserData_p pUserData;
  FILE *hfNLSFile;

  pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
  if (pUserData)
    hfNLSFile = internal_OpenNLSFile(pUserData->Desktop);
  else
    hfNLSFile = NULLHANDLE;

  // Only if we have an NLS support file opened!
  if (hfNLSFile)
  {
    // Allocate memory for strings we're gonna read!
    pchTemp = (char *) malloc(1024);
    if (pchTemp)
    {
      // Okay, read strings and change control texts!
      // First the Undo/Default/Help buttons!
      iMaxCX = 0;
      if (sprintmsg(pchTemp, hfNLSFile, "PG3_0000"))
      {
        WinSetDlgItemText(hwndSettingsNotebook, PB_NOTEBOOK_PG3_HELP, pchTemp);
        WinSetDlgItemText(hwnd, PB_NOTEBOOK_PG3_HELP, pchTemp);
        internal_GetStaticTextSize(hwndSettingsNotebook, PB_NOTEBOOK_PG3_HELP, &iCX, &iCY);
        if (iMaxCX<iCX) iMaxCX = iCX;
      }

      if (sprintmsg(pchTemp, hfNLSFile, "PG3_0001"))
      {
        WinSetDlgItemText(hwndSettingsNotebook, PB_NOTEBOOK_PG3_DEFAULT, pchTemp);
        WinSetDlgItemText(hwnd, PB_NOTEBOOK_PG3_DEFAULT, pchTemp);
        internal_GetStaticTextSize(hwndSettingsNotebook, PB_NOTEBOOK_PG3_DEFAULT, &iCX, &iCY);
        if (iMaxCX<iCX) iMaxCX = iCX;
      }

      if (sprintmsg(pchTemp, hfNLSFile, "PG3_0002"))
      {
        WinSetDlgItemText(hwndSettingsNotebook, PB_NOTEBOOK_PG3_UNDO, pchTemp);
        WinSetDlgItemText(hwnd, PB_NOTEBOOK_PG3_UNDO, pchTemp);
        internal_GetStaticTextSize(hwndSettingsNotebook, PB_NOTEBOOK_PG3_UNDO, &iCX, &iCY);
        if (iMaxCX<iCX) iMaxCX = iCX;
      }
      // Now resize and reposition the buttons!
      WinQueryWindowPos(hwndSettingsNotebook, &swpNotebook);
      WinQueryWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG3_UNDO), &swpBtn);
      WinSetWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG3_UNDO), HWND_TOP,
                      swpNotebook.cx - 20*CXDLGFRAME - 3*iMaxCX,
                      swpBtn.y,
                      iMaxCX+6*CXDLGFRAME,
                      swpBtn.cy,
                      SWP_MOVE | SWP_SIZE);

      WinQueryWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG3_DEFAULT), &swpBtn);
      WinSetWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG3_DEFAULT), HWND_TOP,
                      swpNotebook.cx - 20*CXDLGFRAME - 3*iMaxCX +7*CXDLGFRAME+iMaxCX,
                      swpBtn.y,
                      iMaxCX+6*CXDLGFRAME,
                      swpBtn.cy,
                      SWP_MOVE | SWP_SIZE);

      WinQueryWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG3_HELP), &swpBtn);
      WinSetWindowPos(WinWindowFromID(hwndSettingsNotebook, PB_NOTEBOOK_PG3_HELP), HWND_TOP,
                      swpNotebook.cx - 20*CXDLGFRAME - 3*iMaxCX +14*CXDLGFRAME+2*iMaxCX,
                      swpBtn.y,
                      iMaxCX+6*CXDLGFRAME,
                      swpBtn.cy,
                      SWP_MOVE | SWP_SIZE);

      // Then the controls
      if (sprintmsg(pchTemp, hfNLSFile, "PG3_0003"))
	WinSetDlgItemText(hwnd, GB_SCREENSAVERMODULES, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG3_0004"))
	WinSetDlgItemText(hwnd, CB_SHOWPREVIEW, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG3_0005"))
	WinSetDlgItemText(hwnd, GB_MODULEINFORMATION, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG3_0006"))
	WinSetDlgItemText(hwnd, ST_MODULENAME, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG3_0007"))
	WinSetDlgItemText(hwnd, ST_MODULEVERSION, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG3_0008"))
	WinSetDlgItemText(hwnd, ST_SUPPORTSPASSWORDPROTECTION, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG3_0011"))
	WinSetDlgItemText(hwnd, ST_DESCRIPTION, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG3_0012"))
	WinSetDlgItemText(hwnd, PB_STARTMODULE, pchTemp);
      if (sprintmsg(pchTemp, hfNLSFile, "PG3_0013"))
	WinSetDlgItemText(hwnd, PB_CONFIGURE, pchTemp);

      free(pchTemp);
    }
  }

  internal_CloseNLSFile(hfNLSFile);

  // Also, we have to set the dynamic text, too, so:
  SetPage3ModuleInfo(hwnd);
  WinInvalidateRect(hwndSettingsNotebook, NULL, TRUE);
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

#ifdef DEBUG_LOGGING
  AddLog("[internal_SetLanguageFromLocaleObject] : Changing language!\n");
  AddLog("[internal_SetLanguageFromLocaleObject] : Dropped: [");
  AddLog(achTemp);
  AddLog("]\n");
#endif

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

#ifdef DEBUG_LOGGING
  AddLog("[internal_SetLanguageFromLocaleObject] : New language is: [");
  AddLog(achTemp);
  AddLog("]\n");
#endif

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
	internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE1_GENERAL);
      return (MRESULT) TRUE;
    }
    if (ulCurrPageID == ulSettingsPage2ID)
    {
      Page2UserData_p pUserData;
      pUserData = (Page2UserData_p) WinQueryWindowULong(hwndSettingsPage2, QWL_USER);
      if (pUserData)
	// Show help
	internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE2_GENERAL);
      return (MRESULT) TRUE;
    }
    if (ulCurrPageID == ulSettingsPage3ID)
    {
      Page3UserData_p pUserData;
      pUserData = (Page3UserData_p) WinQueryWindowULong(hwndSettingsPage3, QWL_USER);
      if (pUserData)
	// Show help
	internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE3_GENERAL);
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

MRESULT EXPENTRY fnwpScreenSaverSettingsPage1(HWND hwnd, ULONG msg,
                                              MPARAM mp1, MPARAM mp2)
{
  Page1UserData_p pUserData;
  char achTemp[SSCORE_MAXPASSWORDLEN];
  switch (msg)
  {
    case WM_INITDLG:
      // Save window handle
      hwndSettingsPage1 = hwnd;
      // Initialize dialog window's private data
      pUserData = (Page1UserData_p) malloc(sizeof(Page1UserData_t));
      if (pUserData)
      {
	pUserData->Desktop = (WPSSDesktop *)mp2;
	pUserData->bPageSetUp = FALSE;
	WinSetWindowULong(hwnd, QWL_USER, (ULONG) pUserData);
#ifdef DEBUG_LOGGING
	AddLog("[fnwpScreenSaverSettingsPage1] : WM_INITDLG : Memory allocated\n");
#endif
	// Fill Undo buffer
	pUserData->bUndoEnabled = wpssdesktop_wpssQueryScreenSaverEnabled(pUserData->Desktop);
        pUserData->iUndoActivationTime = wpssdesktop_wpssQueryScreenSaverTimeout(pUserData->Desktop);
        pUserData->bUndoPasswordProtected = wpssdesktop_wpssQueryScreenSaverPasswordProtected(pUserData->Desktop);
	wpssdesktop_wpssQueryScreenSaverEncryptedPassword(pUserData->Desktop,
							  pUserData->achUndoEncryptedPassword,
                                                          sizeof(pUserData->achUndoEncryptedPassword));
        pUserData->bUndoStartAtStartup = wpssdesktop_wpssQueryAutoStartAtStartup(pUserData->Desktop);
        pUserData->bUndoDelayedPasswordProtection = wpssdesktop_wpssQueryDelayedPasswordProtection(pUserData->Desktop);
        pUserData->iUndoDelayedPasswordProtectionTime = wpssdesktop_wpssQueryDelayedPasswordProtectionTimeout(pUserData->Desktop);
        pUserData->bUndoWakeByKeyboardOnly = wpssdesktop_wpssQueryWakeByKeyboardOnly(pUserData->Desktop);
      }
      // Create Undo/Default/Help buttons in notebook common button area!
      WinCreateWindow(hwnd, WC_BUTTON, "Help",
                      WS_VISIBLE | BS_NOTEBOOKBUTTON | BS_HELP,
                      0, 0, 0, 0, hwnd, HWND_TOP,
                      PB_NOTEBOOK_PG1_HELP,
                      NULL, NULL);
      WinCreateWindow(hwnd, WC_BUTTON, "~Default",
                      WS_VISIBLE | BS_NOTEBOOKBUTTON,
                      0, 0, 0, 0, hwnd, HWND_TOP,
                      PB_NOTEBOOK_PG1_DEFAULT,
                      NULL, NULL);
      WinCreateWindow(hwnd, WC_BUTTON, "~Undo",
                      WS_VISIBLE | BS_NOTEBOOKBUTTON,
                      0, 0, 0, 0, hwnd, HWND_TOP,
                      PB_NOTEBOOK_PG1_UNDO,
		      NULL, NULL);
      // Set text (NLS support)
      SetPage1Font(hwnd);
      SetPage1Text(hwnd);
      // Resize controls
      ResizePage1Controls(hwnd);
      // Set the limits of the controls
      SetPage1ControlLimits(hwnd);
      // Set the initial values of controls
      SetPage1ControlValues(hwnd);
      // Subclass every child control, so it will pass DM_* messages
      // to its owner, so locale objects dropped to the controls
      // will change language!
      SubclassChildWindowsForNLS(hwnd);
      // Subclass the notebook control too. It is required so that if
      // we dinamically change the language, we can show the right help!
      SubclassNotebookWindowForNLS();

      WinEnableWindow(WinWindowFromID(hwnd, PB_CHANGE), FALSE);

      if (pUserData)
	pUserData->bPageSetUp = TRUE;
      return (MRESULT) TRUE;
    case WM_FORMATFRAME:
      ResizePage1Controls(hwnd);
      break;
    case WM_LANGUAGECHANGED:
      SetPage1Font(hwnd);
      SetPage1Text(hwnd);
      ResizePage1Controls(hwnd);
      break;
    case WM_DESTROY:
      // Free private window memory!
      pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pUserData)
      {
        free(pUserData);
      }
      // Note that we're destroyed!
      hwndSettingsPage1 = NULLHANDLE;

      break;
    case WM_CONTROL:
      switch (SHORT1FROMMP(mp1)) // Control window ID
      {
	case EF_PASSWORD:
	case EF_PASSWORD2:
	  if (SHORT2FROMMP(mp1)==EN_SETFOCUS)
	  {
	    WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), "");
	  }
	  if (SHORT2FROMMP(mp1)==EN_KILLFOCUS)
	  {
	    pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pUserData)
	    {
	      WinQueryDlgItemText(hwnd, SHORT1FROMMP(mp1), sizeof(achTemp), achTemp);
	      // Store back the encrypted password!
	      wpssdesktop_wpssEncryptScreenSaverPassword(pUserData->Desktop, achTemp);
	      WinSetDlgItemText(hwnd, SHORT1FROMMP(mp1), achTemp);
              WinEnableWindow(WinWindowFromID(hwnd, PB_CHANGE),
                              WinQueryButtonCheckstate(hwnd, CB_USEPASSWORDPROTECTION));
	    }
	  }
	  break;
	case CB_ENABLED:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
	    // The button control has been clicked
	    pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pUserData)
	    {
	      wpssdesktop_wpssSetScreenSaverEnabled(pUserData->Desktop,
                                                    WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
              EnableDisablePage1Controls(hwnd);
	    }
	  }
	  break;
	case CB_DISABLEVIOPOPUPSWHILESAVING:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
	    // The button control has been clicked
	    pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pUserData)
	    {
	      wpssdesktop_wpssSetDisableVIOPopupsWhileSaving(pUserData->Desktop,
                                                    WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
            }
	  }
	  break;
	case CB_WAKEUPBYKEYBOARDONLY:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
	    // The button control has been clicked
	    pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pUserData)
	    {
	      wpssdesktop_wpssSetWakeByKeyboardOnly(pUserData->Desktop,
                                                    WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
            }
	  }
	  break;
	case CB_USEPASSWORDPROTECTION:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
	    // The button control has been clicked
	    pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pUserData)
            {
	      wpssdesktop_wpssSetScreenSaverPasswordProtected(pUserData->Desktop,
                                                              WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
              EnableDisablePage1Controls(hwnd);
	    }
	  }
	  break;
        case CB_MAKETHEFIRSTKEYPRESSTHEFIRSTKEYOFTHEPASSWORD:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
	    // The button control has been clicked
	    pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pUserData)
	    {
	      wpssdesktop_wpssSetFirstKeyEventGoesToPwdWindow(pUserData->Desktop,
                                                              WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
	    }
	  }
	  break;
        case CB_STARTSAVERMODULEONSTARTUP:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
	    // The button control has been clicked
	    pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pUserData)
	    {
	      wpssdesktop_wpssSetAutoStartAtStartup(pUserData->Desktop,
                                                    WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
	    }
	  }
	  break;
	case CB_DELAYPASSWORDPROTECTION:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
	    // The button control has been clicked
	    pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pUserData)
            {
	      wpssdesktop_wpssSetDelayedPasswordProtection(pUserData->Desktop,
                                                           WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
              EnableDisablePage1Controls(hwnd);
	    }
	  }
          break;
        case RB_USEPASSWORDOFCURRENTSECURITYUSER:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
	    // The button control has been clicked
	    pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pUserData)
            {
	      wpssdesktop_wpssSetUseCurrentSecurityPassword(pUserData->Desktop,
                                                            TRUE);
              EnableDisablePage1Controls(hwnd);
	    }
	  }
	  break;
        case RB_USETHISPASSWORD:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
	    // The button control has been clicked
	    pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if (pUserData)
            {
	      wpssdesktop_wpssSetUseCurrentSecurityPassword(pUserData->Desktop,
                                                            FALSE);
              EnableDisablePage1Controls(hwnd);
	    }
	  }
	  break;
	case SPB_TIMEOUT:
	  if (SHORT2FROMMP(mp1)==SPBN_CHANGE)
	  {
            // The spin-button control value has been changed
	    pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if ((pUserData) && (pUserData->bPageSetUp))
	    {
	      ULONG ulValue;
              ulValue = 3; // Default, in case of problems.
	      WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1),
				SPBM_QUERYVALUE,
				(MPARAM) &ulValue,
				MPFROM2SHORT(0, SPBQ_ALWAYSUPDATE));
	      wpssdesktop_wpssSetScreenSaverTimeout(pUserData->Desktop,
						    ulValue*60000);
	    }
          }
          break;
	case SPB_PWDDELAYMIN:
	  if (SHORT2FROMMP(mp1)==SPBN_CHANGE)
	  {
            // The spin-button control value has been changed
	    pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if ((pUserData) && (pUserData->bPageSetUp))
	    {
	      ULONG ulValue;
              ulValue = 1; // Default, in case of problems.
	      WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1),
				SPBM_QUERYVALUE,
				(MPARAM) &ulValue,
				MPFROM2SHORT(0, SPBQ_ALWAYSUPDATE));
	      wpssdesktop_wpssSetDelayedPasswordProtectionTimeout(pUserData->Desktop,
                                                                  ulValue*60000);
	    }
          }
          break;
	default:
          break;
      }
      return (MRESULT) FALSE;
    case WM_COMMAND:
      switch SHORT1FROMMP(mp2) {
	case CMDSRC_PUSHBUTTON:           // ---- A WM_COMMAND from a pushbutton ------
	  switch (SHORT1FROMMP(mp1)) {
            case PB_CHANGE:
	      {
                ChangeSaverPassword(hwnd);
	      }
	      break;
	    case PB_NOTEBOOK_PG1_UNDO:
	      pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pUserData)
	      {
#ifdef DEBUG_LOGGING
		AddLog("[PB_NOTEBOOK_UNDO] : Pressed!\n");
#endif
		// Set back settings from UNDO buffer (saved at start of dialog)
              	wpssdesktop_wpssSetScreenSaverEnabled(pUserData->Desktop, pUserData->bUndoEnabled);
		wpssdesktop_wpssSetScreenSaverTimeout(pUserData->Desktop, pUserData->iUndoActivationTime);
                wpssdesktop_wpssSetScreenSaverPasswordProtected(pUserData->Desktop, pUserData->bUndoPasswordProtected);
		wpssdesktop_wpssSetScreenSaverEncryptedPassword(pUserData->Desktop,
                                                                pUserData->achUndoEncryptedPassword);
                wpssdesktop_wpssSetAutoStartAtStartup(pUserData->Desktop, pUserData->bUndoStartAtStartup);
                wpssdesktop_wpssSetDelayedPasswordProtection(pUserData->Desktop, pUserData->bUndoDelayedPasswordProtection);
                wpssdesktop_wpssSetDelayedPasswordProtectionTimeout(pUserData->Desktop, pUserData->iUndoDelayedPasswordProtectionTime);
                wpssdesktop_wpssSetWakeByKeyboardOnly(pUserData->Desktop, pUserData->bUndoWakeByKeyboardOnly);
		// Set values of controls
		SetPage1ControlValues(hwnd);
		WinEnableWindow(WinWindowFromID(hwnd, PB_CHANGE), FALSE);
	      }
	      break;
	    case PB_NOTEBOOK_PG1_DEFAULT:
	      pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pUserData)
	      {
#ifdef DEBUG_LOGGING
		AddLog("[PB_NOTEBOOK_DEFAULT] : Pressed!\n");
#endif
		// Set default settings
              	wpssdesktop_wpssSetScreenSaverEnabled(pUserData->Desktop, FALSE);
		wpssdesktop_wpssSetScreenSaverTimeout(pUserData->Desktop, 3*60000);
                wpssdesktop_wpssSetScreenSaverPasswordProtected(pUserData->Desktop, FALSE);
                wpssdesktop_wpssSetScreenSaverEncryptedPassword(pUserData->Desktop, "");
                wpssdesktop_wpssSetAutoStartAtStartup(pUserData->Desktop, FALSE);
                wpssdesktop_wpssSetDelayedPasswordProtection(pUserData->Desktop, TRUE);
                wpssdesktop_wpssSetDelayedPasswordProtectionTimeout(pUserData->Desktop, 5*60000);
                wpssdesktop_wpssSetWakeByKeyboardOnly(pUserData->Desktop, FALSE);
                wpssdesktop_wpssSetFirstKeyEventGoesToPwdWindow(pUserData->Desktop, FALSE);
		// Set values of controls
		SetPage1ControlValues(hwnd);
		WinEnableWindow(WinWindowFromID(hwnd, PB_CHANGE), FALSE);
	      }
	      break;
	    case PB_NOTEBOOK_PG1_HELP:
	      pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pUserData)
		// Show help
		internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE1_GENERAL);
	      return (MRESULT) TRUE;
	    default:
              break;
	  }
	  return (MRESULT) TRUE;
        default:
          break;
      }
      break;

    case WM_HELP:
      pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pUserData)
	// Show help
	internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE1_GENERAL);
      return (MRESULT) TRUE;

    // Handling Drag'n'Drop of Locale objects!
    case DM_DRAGOVER:
      {
	if (internal_IsLocaleObject((PDRAGINFO) mp1))
          return MRFROM2SHORT(DOR_DROP, DO_COPY); // Fine, let it come!
	else
	  return MRFROM2SHORT(DOR_NODROPOP, 0); // We support it, but not in this form!
      }

    case DM_DROP:
      {
	if (internal_IsLocaleObject((PDRAGINFO) mp1))
	{
	  pUserData = (Page1UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	  if (pUserData)
	    internal_SetLanguageFromLocaleObject(pUserData->Desktop, (PDRAGINFO) mp1);
	  return MRFROM2SHORT(DOR_DROP, DO_COPY);
	} else
          return MPFROM2SHORT(DOR_NODROPOP, 0);
      }

    default:
      break;
  }
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}
MRESULT EXPENTRY fnwpScreenSaverSettingsPage2(HWND hwnd, ULONG msg,
                                              MPARAM mp1, MPARAM mp2)
{
  Page2UserData_p pUserData;
  switch (msg)
  {
    case WM_INITDLG:
      // Save window handle
      hwndSettingsPage2 = hwnd;

      // Initialize dialog window's private data
      pUserData = (Page2UserData_p) malloc(sizeof(Page2UserData_t));
      if (pUserData)
      {
#ifdef DEBUG_LOGGING
	AddLog("[fnwpScreenSaverSettingsPage2] : WM_INITDLG : Memory allocated\n");
#endif
	pUserData->Desktop = (WPSSDesktop *)mp2;
        pUserData->bPageSetUp = FALSE;
	WinSetWindowULong(hwnd, QWL_USER, (ULONG) pUserData);
        // Fill Undo buffer
        pUserData->bUndoUseDPMSStandbyState = wpssdesktop_wpssQueryUseDPMSStandby(pUserData->Desktop);
        pUserData->iUndoDPMSStandbyStateTimeout = wpssdesktop_wpssQueryDPMSStandbyTimeout(pUserData->Desktop);
        pUserData->bUndoUseDPMSSuspendState = wpssdesktop_wpssQueryUseDPMSSuspend(pUserData->Desktop);
        pUserData->iUndoDPMSSuspendStateTimeout = wpssdesktop_wpssQueryDPMSSuspendTimeout(pUserData->Desktop);
        pUserData->bUndoUseDPMSOffState = wpssdesktop_wpssQueryUseDPMSOff(pUserData->Desktop);
        pUserData->iUndoDPMSOffStateTimeout = wpssdesktop_wpssQueryDPMSOffTimeout(pUserData->Desktop);
      }
      // Create Undo/Default/Help buttons in notebook common button area!
      WinCreateWindow(hwnd, WC_BUTTON, "Help",
                      WS_VISIBLE | BS_NOTEBOOKBUTTON | BS_HELP,
                      0, 0, 0, 0, hwnd, HWND_TOP,
                      PB_NOTEBOOK_PG2_HELP,
                      NULL, NULL);
      WinCreateWindow(hwnd, WC_BUTTON, "~Default",
                      WS_VISIBLE | BS_NOTEBOOKBUTTON,
                      0, 0, 0, 0, hwnd, HWND_TOP,
                      PB_NOTEBOOK_PG2_DEFAULT,
                      NULL, NULL);
      WinCreateWindow(hwnd, WC_BUTTON, "~Undo",
                      WS_VISIBLE | BS_NOTEBOOKBUTTON,
                      0, 0, 0, 0, hwnd, HWND_TOP,
                      PB_NOTEBOOK_PG2_UNDO,
                      NULL, NULL);
      // Set text (NLS support)
      SetPage2Font(hwnd);
      SetPage2Text(hwnd);
      // Resize controls
      ResizePage2Controls(hwnd);
      // Set the limits of the controls
      SetPage2ControlLimits(hwnd);
      // Set the initial values of controls
      SetPage2ControlValues(hwnd);
      // Subclass every child control, so it will pass DM_* messages
      // to its owner, so locale objects dropped to the controls
      // will change language!
      SubclassChildWindowsForNLS(hwnd);
      // Subclass the notebook control too. It is required so that if
      // we dinamically change the language, we can show the right help!
      SubclassNotebookWindowForNLS();

      if (pUserData)
        pUserData->bPageSetUp = TRUE;
      return (MRESULT) TRUE;
    case WM_FORMATFRAME:
      ResizePage2Controls(hwnd);
      break;
    case WM_LANGUAGECHANGED:
      SetPage2Font(hwnd);
      SetPage2Text(hwnd);
      ResizePage2Controls(hwnd);
      break;
    case WM_DESTROY:
      // Free private window memory!
      pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pUserData)
      {
        free(pUserData);
      }

      // Note that we're destroyed!
      hwndSettingsPage2 = NULLHANDLE;

      break;
    case WM_CONTROL:
      switch (SHORT1FROMMP(mp1)) // Control window ID
      {
	case CB_USEDPMSSTANDBYSTATE:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
          {
            // The button control has been clicked
            pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
            if (pUserData)
            {
              wpssdesktop_wpssSetUseDPMSStandby(pUserData->Desktop,
                                                WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
              EnableDisablePage2Controls(hwnd);
	    }
          }
          break;
	case CB_USEDPMSSUSPENDSTATE:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
          {
            // The button control has been clicked
            pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
            if (pUserData)
            {
              wpssdesktop_wpssSetUseDPMSSuspend(pUserData->Desktop,
                                                WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
              EnableDisablePage2Controls(hwnd);
	    }
          }
          break;
	case CB_USEDPMSOFFSTATE:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
          {
            // The button control has been clicked
            pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
            if (pUserData)
            {
              wpssdesktop_wpssSetUseDPMSOff(pUserData->Desktop,
                                            WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)));
              EnableDisablePage2Controls(hwnd);
	    }
          }
          break;
	case SPB_STANDBYTIMEOUT:
	  if (SHORT2FROMMP(mp1)==SPBN_CHANGE)
	  {
            // The spin-button control value has been changed
	    pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if ((pUserData) && (pUserData->bPageSetUp))
	    {
	      ULONG ulValue;
              ulValue = 5; // Default, in case of problems.
	      WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1),
				SPBM_QUERYVALUE,
				(MPARAM) &ulValue,
				MPFROM2SHORT(0, SPBQ_ALWAYSUPDATE));
	      wpssdesktop_wpssSetDPMSStandbyTimeout(pUserData->Desktop,
						    ulValue*60000);
	    }
          }
          break;
	case SPB_SUSPENDTIMEOUT:
	  if (SHORT2FROMMP(mp1)==SPBN_CHANGE)
	  {
            // The spin-button control value has been changed
	    pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if ((pUserData) && (pUserData->bPageSetUp))
	    {
	      ULONG ulValue;
              ulValue = 5; // Default, in case of problems.
	      WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1),
				SPBM_QUERYVALUE,
				(MPARAM) &ulValue,
				MPFROM2SHORT(0, SPBQ_ALWAYSUPDATE));
	      wpssdesktop_wpssSetDPMSSuspendTimeout(pUserData->Desktop,
						    ulValue*60000);
	    }
          }
          break;
	case SPB_OFFTIMEOUT:
	  if (SHORT2FROMMP(mp1)==SPBN_CHANGE)
	  {
            // The spin-button control value has been changed
	    pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	    if ((pUserData) && (pUserData->bPageSetUp))
	    {
	      ULONG ulValue;
              ulValue = 5; // Default, in case of problems.
	      WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1),
				SPBM_QUERYVALUE,
				(MPARAM) &ulValue,
				MPFROM2SHORT(0, SPBQ_ALWAYSUPDATE));
	      wpssdesktop_wpssSetDPMSOffTimeout(pUserData->Desktop,
                                                ulValue*60000);
	    }
          }
          break;
	default:
          break;
      }
      return (MRESULT) FALSE;
    case WM_COMMAND:
      switch SHORT1FROMMP(mp2) {
	case CMDSRC_PUSHBUTTON:           // ---- A WM_COMMAND from a pushbutton ------
	  switch (SHORT1FROMMP(mp1)) {
	    case PB_NOTEBOOK_PG2_UNDO:
	      pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pUserData)
	      {
		// Set back settings from UNDO buffer (saved at start of dialog)
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
		SetPage2ControlValues(hwnd);
	      }
	      break;
	    case PB_NOTEBOOK_PG2_DEFAULT:
	      pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pUserData)
	      {
		// Set default settings
                wpssdesktop_wpssSetUseDPMSStandby(pUserData->Desktop,
                                                  FALSE);
                wpssdesktop_wpssSetDPMSStandbyTimeout(pUserData->Desktop,
                                                      5*60000);
                wpssdesktop_wpssSetUseDPMSSuspend(pUserData->Desktop,
                                                  FALSE);
                wpssdesktop_wpssSetDPMSSuspendTimeout(pUserData->Desktop,
                                                      5*60000);
                wpssdesktop_wpssSetUseDPMSOff(pUserData->Desktop,
                                              TRUE);
                wpssdesktop_wpssSetDPMSOffTimeout(pUserData->Desktop,
                                                  5*60000);
		// Set values of controls
		SetPage2ControlValues(hwnd);
	      }
	      break;
	    case PB_NOTEBOOK_PG2_HELP:
	      pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pUserData)
		// Show help
		internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE2_GENERAL);
	      return (MRESULT) TRUE;
	    default:
              break;
	  }
          return (MRESULT) TRUE;
        default:
          break;
      }
      break;

    case WM_HELP:
      pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pUserData)
	// Show help
	internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE2_GENERAL);
      return (MRESULT) TRUE;

    // Handling Drag'n'Drop of Locale objects!
    case DM_DRAGOVER:
      {
	if (internal_IsLocaleObject((PDRAGINFO) mp1))
          return MRFROM2SHORT(DOR_DROP, DO_COPY); // Fine, let it come!
	else
	  return MRFROM2SHORT(DOR_NODROPOP, 0); // We support it, but not in this form!
      }

    case DM_DROP:
      {
	if (internal_IsLocaleObject((PDRAGINFO) mp1))
	{
	  pUserData = (Page2UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	  if (pUserData)
	    internal_SetLanguageFromLocaleObject(pUserData->Desktop, (PDRAGINFO) mp1);
	  return MRFROM2SHORT(DOR_DROP, DO_COPY);
	} else
          return MPFROM2SHORT(DOR_NODROPOP, 0);
      }

    default:
      break;
  }
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

MRESULT EXPENTRY fnwpScreenSaverSettingsPage3(HWND hwnd, ULONG msg,
                                              MPARAM mp1, MPARAM mp2)
{
  Page3UserData_p pUserData;
  HWND hwndPreview;
  long lID;

  switch (msg)
  {
    case WM_INITDLG:
      // Save window handle
      hwndSettingsPage3 = hwnd;

      // Initialize dialog window's private data
      pUserData = (Page3UserData_p) malloc(sizeof(Page3UserData_t));
      if (pUserData)
      {
#ifdef DEBUG_LOGGING
	AddLog("[fnwpScreenSaverSettingsPage3] : WM_INITDLG : Memory allocated\n");
#endif
	pUserData->Desktop = (WPSSDesktop *)mp2;
        pUserData->bPageSetUp = FALSE;

        pUserData->iPreviewMsgCounter = 0; // Number of Start/Stop preview commands pending
        pUserData->bPreviewRunning = 0;    // Is preview running?

        /* Querying the list of available modules may take a while */
        WinSetPointer(HWND_DESKTOP,
                      WinQuerySysPointer(HWND_DESKTOP, SPTR_WAIT, FALSE));
        pUserData->ulMaxModules =
          wpssdesktop_wpssQueryNumOfAvailableScreenSaverModules(pUserData->Desktop);
        WinSetPointer(HWND_DESKTOP,
                      WinQuerySysPointer(HWND_DESKTOP, SPTR_ARROW, FALSE));

#ifdef DEBUG_LOGGING
        {
          char achTemp[128];
          sprintf(achTemp, "ulMaxModules = %d\n", pUserData->ulMaxModules);
          AddLog(achTemp);
        }
#endif
        pUserData->pModuleDescArray = (PSSMODULEDESC) malloc(pUserData->ulMaxModules * sizeof(SSMODULEDESC));
        if (pUserData->pModuleDescArray)
        {
          WinSetPointer(HWND_DESKTOP,
                        WinQuerySysPointer(HWND_DESKTOP, SPTR_WAIT, FALSE));
          wpssdesktop_wpssQueryInfoAboutAvailableScreenSaverModules(pUserData->Desktop,
                                                                    pUserData->ulMaxModules,
                                                                    pUserData->pModuleDescArray);
          WinSetPointer(HWND_DESKTOP,
                        WinQuerySysPointer(HWND_DESKTOP, SPTR_ARROW, FALSE));
        } else
        {
          pUserData->ulMaxModules = 0;
        }
        WinSetWindowULong(hwnd, QWL_USER, (ULONG) pUserData);

        // Fill Undo buffer
        wpssdesktop_wpssQueryScreenSaverModule(pUserData->Desktop,
                                               pUserData->achUndoSaverDLLFileName,
                                               sizeof(pUserData->achUndoSaverDLLFileName));
      }

      // Create special child window
      // (will be resized/positioned later)
      if (!bWindowClassRegistered)
      {
        WinRegisterClass(WinQueryAnchorBlock(hwnd), (PSZ) PREVIEW_CLASS,
                         (PFNWP) PreviewWindowProc,
                         CS_SIZEREDRAW | CS_CLIPCHILDREN,
                         16); // Window data
        bWindowClassRegistered = TRUE;
      }
      WinCreateWindow(hwnd, PREVIEW_CLASS, "Preview area",
                      WS_VISIBLE,
                      50, 50, 50, 50,
                      hwnd, HWND_TOP,
                      ID_PREVIEWAREA,
                      NULL, NULL);

      // Create Undo/Default/Help buttons in notebook common button area!
      WinCreateWindow(hwnd, WC_BUTTON, "Help",
                      WS_VISIBLE | BS_NOTEBOOKBUTTON | BS_HELP,
                      0, 0, 0, 0, hwnd, HWND_TOP,
                      PB_NOTEBOOK_PG3_HELP,
                      NULL, NULL);
      WinCreateWindow(hwnd, WC_BUTTON, "~Default",
                      WS_VISIBLE | BS_NOTEBOOKBUTTON,
                      0, 0, 0, 0, hwnd, HWND_TOP,
                      PB_NOTEBOOK_PG3_DEFAULT,
                      NULL, NULL);
      WinCreateWindow(hwnd, WC_BUTTON, "~Undo",
                      WS_VISIBLE | BS_NOTEBOOKBUTTON,
                      0, 0, 0, 0, hwnd, HWND_TOP,
                      PB_NOTEBOOK_PG3_UNDO,
                      NULL, NULL);

      // Set Page3 text (NLS support)
      SetPage3Font(hwnd);
      SetPage3Text(hwnd);
      // Resize controls
      ResizePage3Controls(hwnd);
      // Set the limits of the controls
      SetPage3ControlLimits(hwnd);
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

      return (MRESULT) TRUE;

    case WM_FORMATFRAME:
      ResizePage3Controls(hwnd);
      break;

    case WM_LANGUAGECHANGED:

#ifdef DEBUG_LOGGING
      AddLog("WM_LANGUAGECHANGED] : Getting pUserData\n");
#endif
      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pUserData)
      {
        int iPreviewState;

#ifdef DEBUG_LOGGING
        AddLog("WM_LANGUAGECHANGED] : Query old preview state\n");
#endif
        iPreviewState = wpssdesktop_wpssQueryScreenSaverPreviewEnabled(pUserData->Desktop);

#ifdef DEBUG_LOGGING
        AddLog("WM_LANGUAGECHANGED] : Stop preview if running\n");
#endif
        // Stop preview if it is running.
        if (iPreviewState)
        {
#ifdef DEBUG_LOGGING
          AddLog("WM_LANGUAGECHANGED] : Yes, was running, so stop it!\n");
          AddLog(" - 1\n");
#endif
          WinCheckButton(hwnd, CB_SHOWPREVIEW, FALSE);
#ifdef DEBUG_LOGGING
          AddLog(" - 2\n");
#endif
          wpssdesktop_wpssSetScreenSaverPreviewEnabled(pUserData->Desktop,
                                                       FALSE);
#ifdef DEBUG_LOGGING
          //AddLog(" - 3\n");
#endif
          StopPage3Preview(hwnd);
#ifdef DEBUG_LOGGING
          AddLog(" - 4\n");
#endif
        }

#ifdef DEBUG_LOGGING
        AddLog("WM_LANGUAGECHANGED] : Destroy old strings from available modules\n");
#endif
        // Destroy old strings from available modules
        if (pUserData->pModuleDescArray)
          free(pUserData->pModuleDescArray);
        pUserData->pModuleDescArray = NULL;
        pUserData->ulMaxModules = 0;

#ifdef DEBUG_LOGGING
        AddLog("WM_LANGUAGECHANGED] : Get number of modules available\n");
#endif
        // Re-load strings (using the new language!)
        WinSetPointer(HWND_DESKTOP,
                      WinQuerySysPointer(HWND_DESKTOP, SPTR_WAIT, FALSE));
        pUserData->ulMaxModules =
          wpssdesktop_wpssQueryNumOfAvailableScreenSaverModules(pUserData->Desktop);
        WinSetPointer(HWND_DESKTOP,
                        WinQuerySysPointer(HWND_DESKTOP, SPTR_ARROW, FALSE));

#ifdef DEBUG_LOGGING
        AddLog("WM_LANGUAGECHANGED] : malloc()\n");
#endif

        pUserData->pModuleDescArray = (PSSMODULEDESC) malloc(pUserData->ulMaxModules * sizeof(SSMODULEDESC));
        if (pUserData->pModuleDescArray)
        {
#ifdef DEBUG_LOGGING
          AddLog("WM_LANGUAGECHANGED] : Get list of modules\n");
#endif
          WinSetPointer(HWND_DESKTOP,
                        WinQuerySysPointer(HWND_DESKTOP, SPTR_WAIT, FALSE));
          wpssdesktop_wpssQueryInfoAboutAvailableScreenSaverModules(pUserData->Desktop,
                                                                    pUserData->ulMaxModules,
                                                                    pUserData->pModuleDescArray);
          WinSetPointer(HWND_DESKTOP,
                        WinQuerySysPointer(HWND_DESKTOP, SPTR_ARROW, FALSE));
        } else
        {
          pUserData->ulMaxModules = 0;
        }

#ifdef DEBUG_LOGGING
	AddLog("WM_LANGUAGECHANGED] : SetPage3Font()\n");
#endif
	SetPage3Font(hwnd);
#ifdef DEBUG_LOGGING
	AddLog("WM_LANGUAGECHANGED] : SetPage3Text()\n");
#endif
	SetPage3Text(hwnd);
#ifdef DEBUG_LOGGING
	AddLog("WM_LANGUAGECHANGED] : SetPage3ControlValues()\n");
#endif
	SetPage3ControlValues(hwnd, FALSE);
#ifdef DEBUG_LOGGING
	AddLog("WM_LANGUAGECHANGED] : ResizePage3Controls()\n");
#endif
	ResizePage3Controls(hwnd);

        // Restart preview if it was running before!
        if (iPreviewState)
	{
#ifdef DEBUG_LOGGING
	  AddLog("WM_LANGUAGECHANGED] : Restart preview!\n");
#endif

          wpssdesktop_wpssSetScreenSaverPreviewEnabled(pUserData->Desktop,
                                                       TRUE);
          WinCheckButton(hwnd, CB_SHOWPREVIEW, TRUE);
          StartPage3Preview(hwnd);
        }
      }
#ifdef DEBUG_LOGGING
      AddLog("WM_LANGUAGECHANGED] : Done.\n");
#endif
      break;

    case WM_SSCORENOTIFY_PREVIEWSTARTED:
      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pUserData)
      {
#ifdef DEBUG_LOGGING
        AddLog("[WM_PREVIEWSTARTED] : Done.\n");
#endif
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
          lID = (long) WinSendDlgItemMsg(hwnd, LB_MODULELIST, LM_QUERYITEMHANDLE, (MPARAM) lID, (MPARAM) NULL);
          if ((lID<0) || lID>=pUserData->ulMaxModules)
          {
            // Something is wrong, the handle, which should tell the index in the
            // module desc array, points out of the array!
            lID = LIT_NONE;
          }
        }
        if (lID!=LIT_NONE)
          WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE), pUserData->pModuleDescArray[lID].bConfigurable);
        else
          WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE), FALSE);
      }
      return (MRESULT) 0;

    case WM_SSCORENOTIFY_PREVIEWSTOPPED:
      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pUserData)
      {
#ifdef DEBUG_LOGGING
        AddLog("[WM_PREVIEWSTOPPED] : Done.\n");
#endif

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
          WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE), pUserData->pModuleDescArray[lID].bConfigurable);
        else
          WinEnableWindow(WinWindowFromID(hwnd, PB_CONFIGURE), FALSE);
      }
      return (MRESULT) 0;

    case WM_CLOSE:
#ifdef DEBUG_LOGGING
      AddLog("[WM_DESTROY] : Starting loop\n");
#endif
      break;

    case WM_DESTROY:

#ifdef DEBUG_LOGGING
      AddLog("[WM_DESTROY] : Stop preview!\n");
#endif
      // Make sure the preview will not go on!
      StopPage3Preview(hwnd);

      // Wait for this event to be processed!
      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pUserData)
      {
        HAB hab;
        QMSG qmsg;

        hab = WinQueryAnchorBlock(hwnd);

#ifdef DEBUG_LOGGING
        AddLog("[WM_DESTROY] : Starting loop\n");
#endif

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
#ifdef DEBUG_LOGGING
          AddLog("[WM_DESTROY] : Going into loop to wait for preview to stop!\n");
#endif

          while (WinGetMsg(hab, &qmsg, 0, 0, 0))
          {
            WinDispatchMsg(hab, &qmsg);
            if (pUserData->iPreviewMsgCounter<=0)
            {
#ifdef DEBUG_LOGGING
              AddLog("[WM_DESTROY] : iPreviewMsgCounter became 0!\n");
#endif
              break;
            }
          }
#ifdef DEBUG_LOGGING
          AddLog("[WM_DESTROY] : Preview stopped.\n");
#endif
        }
      }
#ifdef DEBUG_LOGGING
      AddLog("[WM_DESTROY] : Destroying preview area child!\n");
#endif

      // Destroy preview area child window, created in WM_INITDLG!
      hwndPreview = WinWindowFromID(hwnd, ID_PREVIEWAREA);
      if ((hwndPreview) && (hwndPreview!=HWND_DESKTOP))
        WinDestroyWindow(hwndPreview);

      // Free private window memory!
      if (pUserData)
      {
        if (pUserData->pModuleDescArray)
          free(pUserData->pModuleDescArray);
        free(pUserData);
      }

      // Note that we're destroyed!
      hwndSettingsPage3 = NULLHANDLE;

#ifdef DEBUG_LOGGING
      AddLog("[WM_DESTROY] : Done!\n");
#endif
      break;

    case WM_WINDOWPOSCHANGED:
      // The dialog window size/pos is changed, or the dialog window
      // is being shown or hidden!
      // Don't let the time-consuming screen saver preview run,
      // when the dialog window is hidden!
      {
        PSWP pswpNew;
        pswpNew = (PSWP) mp1;

        if (pswpNew->fl & SWP_SHOW)
        {
          // The dialog window is about to be shown!
          if (WinQueryButtonCheckstate(hwnd, CB_SHOWPREVIEW))
          {
#ifdef DEBUG_LOGGING
	    AddLog("[fnwpScreenSaverSettingsPage3] : WM_WINDOWPOSCHANGED : Starting preview\n");
#endif
            // Restart screen saver preview!
            StartPage3Preview(hwnd);
	  }
	} else
	if (pswpNew->fl & SWP_HIDE)
	{
	  // The dialog window is about to be hidden!
	  if (WinQueryButtonCheckstate(hwnd, CB_SHOWPREVIEW))
	  {
#ifdef DEBUG_LOGGING
	    AddLog("[fnwpScreenSaverSettingsPage3] : WM_WINDOWPOSCHANGED : Stopping preview\n");
#endif
            // Stop the running screen saver preview!
            StopPage3Preview(hwnd);
	  }
	}
        break;
      }
    case WM_CONTROL:
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
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
	    // The button control has been clicked
	    if (WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1)))
	    {
	      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pUserData)
		wpssdesktop_wpssSetScreenSaverPreviewEnabled(pUserData->Desktop,
                                                             TRUE);

              StartPage3Preview(hwnd);
	    }
	    else
	    {
	      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pUserData)
		wpssdesktop_wpssSetScreenSaverPreviewEnabled(pUserData->Desktop,
                                                             FALSE);

              StopPage3Preview(hwnd);
	    }
	  }
	  break;
	default:
          break;
      }
      return (MRESULT) FALSE;

    case WM_COMMAND:
      switch SHORT1FROMMP(mp2) {
	case CMDSRC_PUSHBUTTON:           // ---- A WM_COMMAND from a pushbutton ------
	  switch (SHORT1FROMMP(mp1)) {
            case PB_STARTMODULE:
	      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pUserData)
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
	      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pUserData)
	      {
		// Set back settings from UNDO buffer (saved at start of dialog)
                wpssdesktop_wpssSetScreenSaverModule(pUserData->Desktop,
						     pUserData->achUndoSaverDLLFileName);

		// Set values of controls
		SetPage3ControlValues(hwnd, TRUE);
	      }
	      break;
	    case PB_NOTEBOOK_PG3_DEFAULT:
	      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pUserData)
	      {
		// Set default settings
                wpssdesktop_wpssSetScreenSaverModule(pUserData->Desktop, "");

		// Set values of controls
		SetPage3ControlValues(hwnd, TRUE);
	      }
	      break;
	    case PB_NOTEBOOK_PG3_HELP:
	      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	      if (pUserData)
		// Show help
		internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE3_GENERAL);
              return (MRESULT) TRUE;
            default:
              break;
	  }
          return (MRESULT) TRUE;
        default:
          break;
      }
      break;

    case WM_HELP:
      pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
      if (pUserData)
	// Show help
	internal_wpDisplayHelp(pUserData->Desktop, HELPID_PAGE3_GENERAL);
      return (MRESULT) TRUE;

    // Handling Drag'n'Drop of Locale objects!
    case DM_DRAGOVER:
      {
	if (internal_IsLocaleObject((PDRAGINFO) mp1))
          return MRFROM2SHORT(DOR_DROP, DO_COPY); // Fine, let it come!
	else
	  return MRFROM2SHORT(DOR_NODROPOP, 0); // We support it, but not in this form!
      }

    case DM_DROP:
      {
	if (internal_IsLocaleObject((PDRAGINFO) mp1))
	{
	  pUserData = (Page3UserData_p) WinQueryWindowULong(hwnd, QWL_USER);
	  if (pUserData)
	    internal_SetLanguageFromLocaleObject(pUserData->Desktop, (PDRAGINFO) mp1);
	  return MRFROM2SHORT(DOR_DROP, DO_COPY);
	} else
          return MPFROM2SHORT(DOR_NODROPOP, 0);
      }

    default:
      break;
  }

  return WinDefDlgProc(hwnd, msg, mp1, mp2);
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



/**************************  END WPSSDesktop.c  ******************************/
