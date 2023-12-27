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
#include <time.h>
#define INCL_DOS
#define INCL_DOSMISC
#define INCL_WIN
#define INCL_GPI
#define INCL_ERRORS
#include <os2.h>
#include "SSModule.h"

#include "Random-Resources.h"
#include "MSGX.h" // NLS support

#define FOREIGN_MODULE_MUTEX_TIMEOUT   100

HMODULE hmodOurDLLHandle;
int     bRunning;
int     bPaused;
int     bFirstKeyGoesToPwdWindow = 0;
int     bWMINITDLGDone;

char   *apchNLSText[SSMODULE_NLSTEXT_MAX+1];
HMTX    hmtxUseNLSTextArray;

//#define DEBUG_LOGGING
#ifdef DEBUG_LOGGING
void AddLog(char *pchMsg)
{
  FILE *hFile;

  hFile = fopen("Random.log", "at+");
  if (hFile)
  {
    fprintf(hFile, "%s", pchMsg);
    fclose(hFile);
  }
}
#endif

// The foreign module
HMODULE hmodForeignDLL = NULLHANDLE;
// Function pointers for foreign module
pfn_SSModule_GetModuleDesc  pfnGetModuleDesc = NULL;
pfn_SSModule_Configure      pfnConfigure = NULL;
pfn_SSModule_StartSaving    pfnStartSaving = NULL;
pfn_SSModule_StopSaving     pfnStopSaving = NULL;
pfn_SSModule_AskUserForPassword  pfnAskUserForPassword = NULL;
pfn_SSModule_ShowWrongPasswordNotification pfnShowWrongPasswordNotification = NULL;
pfn_SSModule_PauseSaving    pfnPauseSaving = NULL;
pfn_SSModule_ResumeSaving   pfnResumeSaving = NULL;
pfn_SSModule_SetNLSText     pfnSetNLSText = NULL;
pfn_SSModule_SetCommonSetting    pfnSetCommonSetting = NULL;
HMTX hmtxUseForeignModule;

// Configuration of randomizer module, with defaults
int bUseAllAvailableModules = TRUE;
int bChangeRandomly = TRUE;
int iChangeTime = 1; // in minutes
int iNumOfModules = 0;
char **ppchModules = NULL;

int iCurrModuleNum = 0;
char achCurrentHomeDirectory[1024];
int bCurrentPreviewMode;
HWND hwndCurrentParent;

typedef struct ConfigDlgInitParams_s
{
  USHORT          cbSize; // Required to be able to pass this structure to WM_INITDLG
  char           *pchHomeDirectory;
} ConfigDlgInitParams_t, *ConfigDlgInitParams_p;

typedef struct ModuleList_s
{
  char achFileName[1024];
  char achModuleName[SSMODULE_MAXNAMELEN];
  void *pNext;
} ModuleList_t, *ModuleList_p;

#define STATE_NOTRUNNING  0
#define STATE_STARTUP     1
#define STATE_RUNNING     2

TID  tidChanger;
int  bChangerThreadShutdownReq = 0; // Flag to notify changer thread to die
int  iChangerThreadState = STATE_NOTRUNNING;

static void internal_LoadConfig(char *pchHomeDirectory)
{
  char achFileName[1024];
  char *pchHomeEnvVar;
  FILE *hFile;
  int i;

  iCurrModuleNum = 0;

  // Destroy old configuration!
  bUseAllAvailableModules = TRUE;
  bChangeRandomly = TRUE;
  iChangeTime = 1;
  if (ppchModules)
  {
    for (i=0; i<iNumOfModules; i++)
    {
      if (ppchModules[i])
      {
        free(ppchModules[i]);
        ppchModules[i] = NULL;
      }
    }
    free(ppchModules); ppchModules = NULL;
  }
  iNumOfModules = 0;
  

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
    strncat(achFileName, ".dssaver\\Random.CFG", sizeof(achFileName));
    // Try to open that config file!
    hFile = fopen(achFileName, "rt");
  }

  if (!hFile)
  {
    // If we could not open a config file in the HOME directory, or there was
    // no HOME directory, then try to use the DSSaver Global home directory!
    snprintf(achFileName, sizeof(achFileName), "%sRandom.CFG", pchHomeDirectory);
    hFile = fopen(achFileName, "rt");
    if (!hFile)
      return; // Could not open file!
  }

  i = fscanf(hFile, "%d %d %d %d\n",
             &(bUseAllAvailableModules),
             &(bChangeRandomly),
             &(iChangeTime),
             &(iNumOfModules));

  if (i!=4)
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_LoadConfig] : Invalid config file, using defaults!\n");
#endif
    bUseAllAvailableModules = TRUE;
    bChangeRandomly = TRUE;
    iChangeTime = 1;
    iNumOfModules = 0;
    // The pointer array is already free'd.
    return;
  }

  if (iNumOfModules>0)
  {
    ppchModules = (char **) malloc(sizeof(char *) * iNumOfModules);

    if (ppchModules)
    {
      for (i=0; i<iNumOfModules; i++)
      {
        ppchModules[i] = (char *) malloc(512);
        if (ppchModules[i])
          fscanf(hFile, "%s\n", ppchModules[i]);
      }
    } else
    {
      iNumOfModules = 0; // Error!
      bUseAllAvailableModules = TRUE;
    }
  } else
  {
    // Make sure we'll have modules...
    bUseAllAvailableModules = TRUE;
  }

  fclose(hFile);

#ifdef DEBUG_LOGGING
  {
    char achTemp[128];
    AddLog("[internal_LoadConfig] : Cfg file loaded.\n");
    sprintf(achTemp, "%d %d %d %d\n",
            (bUseAllAvailableModules),
            (bChangeRandomly),
            (iChangeTime),
            (iNumOfModules));
    AddLog(achTemp);
  }
#endif
}

static void internal_SaveConfig(char *pchHomeDirectory)
{
  char achFileName[1024];
  char *pchHomeEnvVar;
  FILE *hFile;
  int i;

#ifdef DEBUG_LOGGING
  {
    char achTemp[128];
    sprintf(achTemp, "%d", iNumOfModules);
    AddLog("[SaveConfig] : iNumOfModules is ");
    AddLog(achTemp);
    AddLog("\n");
  }
#endif

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
    strncat(achFileName, "\\Random.CFG", sizeof(achFileName));
    // Try to open that config file!
    hFile = fopen(achFileName, "wt");
  }

  if (!hFile)
  {
    // If we could not open a config file in the HOME directory, or there was
    // no HOME directory, then try to use the DSSaver Global home directory!
    snprintf(achFileName, sizeof(achFileName), "%sRandom.CFG", pchHomeDirectory);
    hFile = fopen(achFileName, "wt");
    if (!hFile)
      return; // Could not open file!
  }

  fprintf(hFile, "%d %d %d %d\n",
          (bUseAllAvailableModules),
          (bChangeRandomly),
          (iChangeTime),
          (iNumOfModules));

  for (i=0; i<iNumOfModules; i++)
  {
    fprintf(hFile, "%s\n", ppchModules[i]);
  }

  fclose(hFile);
}

static void internal_FreeModule()
{
#ifdef DEBUG_LOGGING
  AddLog("[internal_FreeModule] : Enter\n");
#endif

  if (hmodForeignDLL)
  {

    if (pfnSetNLSText)
    {
      // Free its NLS texts, if it supports it!
      int i;

#ifdef DEBUG_LOGGING
      AddLog("[internal_FreeModule] : Free NLS texts\n");
#endif

      for (i=0; i<SSMODULE_NLSTEXT_MAX+1; i++)
        pfnSetNLSText(i, NULL);
    }

#ifdef DEBUG_LOGGING
    AddLog("[internal_FreeModule] : Unload DLL and NULLize function pointers\n");
    {
      char achTemp[64];
      AddLog("[internal_FreeModule] : Unloading HMOD : ");
      sprintf(achTemp, "0x%x\n", hmodForeignDLL);
      AddLog(achTemp);
    }
#endif

    DosFreeModule(hmodForeignDLL); hmodForeignDLL = NULLHANDLE;

    pfnGetModuleDesc = NULL;
    pfnConfigure = NULL;
    pfnStartSaving = NULL;
    pfnStopSaving = NULL;
    pfnAskUserForPassword = NULL;
    pfnShowWrongPasswordNotification = NULL;
    pfnPauseSaving = NULL;
    pfnResumeSaving = NULL;
    pfnSetNLSText = NULL;
    pfnSetCommonSetting = NULL;
  }

#ifdef DEBUG_LOGGING
  AddLog("[internal_FreeModule] : Leave\n");
#endif

}

static ModuleList_p internal_GetListOfModules(int bOnlyFromConfig)
{
  ModuleList_p pTemp, pLast, pResult;
  char achSearchPattern[CCHMAXPATHCOMP+15];
  char achBasePath[CCHMAXPATHCOMP+15];  /* place of directory where we are */
  char achDSSBasePath[CCHMAXPATHCOMP+15]; /* parent of directory where we are */
  char achFileNamePath[CCHMAXPATHCOMP+15];
  char achFailure[32];
  HDIR hdirFindHandle;
  FILEFINDBUF3 FindBuffer;
  ULONG ulResultBufLen;
  ULONG ulFindCount;
  APIRET rc, rcfind;
  HMODULE hmodDLL;
  int bWasLoaded;
  int i;

#ifdef DEBUG_LOGGING
  AddLog("[internal_GetListOfModules] : Enter\n");
#endif

  pResult = NULL;
  pLast = NULL;

  // If the caller is interested only in the modules in the config file,
  // make a list from that!
  if (bOnlyFromConfig)
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_GetListOfModules] : Only from config\n");
#endif

    for (i=0; i<iNumOfModules; i++)
    {
      pTemp = (ModuleList_p) malloc(sizeof(ModuleList_t));
      if (pTemp)
      {
	strcpy(pTemp->achFileName, ppchModules[i]);
        strcpy(pTemp->achModuleName, "");
        pTemp->pNext = NULL;
        if (pLast)
        {
          pLast->pNext = pTemp;
          pLast = pTemp;
        } else
        {
          pResult = pLast = pTemp;
        }
      }
    }
#ifdef DEBUG_LOGGING
    AddLog("[internal_GetListOfModules] : Only from config -> Return\n");
#endif

    return pResult;
  }

#ifdef DEBUG_LOGGING
  AddLog("[internal_GetListOfModules] : Look for available DLL files\n");
#endif

  // Otherwise, get the available DLLs!
  hdirFindHandle = HDIR_CREATE;
  ulResultBufLen = sizeof(FindBuffer);
  ulFindCount = 1;

  // Look for the DLLs next to ourselves!
  // For this, get the folder where we're!
  rc = DosQueryModuleName(hmodOurDLLHandle,
                          sizeof(achBasePath),
                          achBasePath);
  if (rc!=NO_ERROR)
  {
    // Eeek, something is wrong.

    // Have some kind of default then:
    snprintf(achBasePath, sizeof(achBasePath), "%sModules", achCurrentHomeDirectory);
  } else
  {
    // Fine! We have the fully qualified path!

#ifdef DEBUG_LOGGING
    AddLog("[internal_GetListOfModules] : Our module is [");
    AddLog(achBasePath);
    AddLog("]\n");
#endif


    // Search for the base directory then!
    i = strlen(achBasePath)-1;
    while ((i>=0) && (achBasePath[i]!='\\'))
      i--;

    if (i>=0)
    {
      achBasePath[i] = 0;
    } else
    {
      achBasePath[0] = 0;
    }

#ifdef DEBUG_LOGGING
    AddLog("[internal_GetListOfModules] : Base folder is [");
    AddLog(achBasePath);
    AddLog("]\n");
#endif
  }

  // Assemble the search path from this base directory!
  snprintf(achSearchPattern, sizeof(achSearchPattern), "%s\\*.DLL", achBasePath);

  // Create the DSSaver Base Path
  strcpy(achDSSBasePath, achBasePath);
  // Search for the parent directory then!
  i = strlen(achDSSBasePath)-1;
  while ((i>=0) && (achDSSBasePath[i]!='\\'))
    i--;

  if (i>=0)
  {
    achDSSBasePath[i+1] = 0;
  } else
  {
    achDSSBasePath[0] = 0;
  }
#ifdef DEBUG_LOGGING
    AddLog("[internal_GetListOfModules] : DSS Home folder seems to be [");
    AddLog(achDSSBasePath);
    AddLog("]\n");
#endif


#ifdef DEBUG_LOGGING
  AddLog("[internal_GetListOfModules] : Using searchpattern of [");
  AddLog(achSearchPattern);
  AddLog("]\n");
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
    do {
      sprintf(achFileNamePath, "%s\\%s", achBasePath, FindBuffer.achName);

#ifdef DEBUG_LOGGING
      AddLog("[internal_GetListOfModules] : Loading ");
      AddLog(achFileNamePath);
      AddLog("\n");
#endif

      rc = DosQueryModuleHandle(achFileNamePath, &hmodDLL);
      if (rc==NO_ERROR)
      {
        // This DLL is already in memory, no need to re-load it!
        bWasLoaded = 0;
      } else
      {
        // Try to open this DLL
        rc = DosLoadModule(achFailure, sizeof(achFailure),
                           achFileNamePath,
                           &hmodDLL);
        if (rc==NO_ERROR)
          bWasLoaded = 1;
      }

      if (rc==NO_ERROR)
      {
        // Cool, DLL could be loaded!
        PFN pfn1, pfn2, pfn3, pfn4, pfnID, pfnNLS;

#ifdef DEBUG_LOGGING
        {
          char achTemp[64];
          AddLog("[internal_GetListOfModules] : Loaded, HMOD is ");
          sprintf(achTemp, "0x%x\n", hmodDLL);
          AddLog(achTemp);
        }
        AddLog("[internal_GetListOfModules] : Check exported functions\n");
#endif

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
                              "Randomizer_ID",
                              &pfnID);
        if (rc!=NO_ERROR) pfnID = NULL;

        rc = DosQueryProcAddr(hmodDLL,
			      0,
                              "SSModule_SetNLSText",
                              &pfnNLS);
        if (rc!=NO_ERROR) pfnNLS = NULL;


        if ((pfn1) && (pfn2) && (pfn3) && (pfn4) && (!pfnID))
        {
          pfn_SSModule_GetModuleDesc pfnGetModuleDesc;
          pfn_SSModule_SetNLSText    pfnSetNLSText;
          SSModuleDesc_t ModuleDesc;

          // Good, it had all the required functions, so it seems
          // to be a valid screensaver module, and what is even
          // better, it's sure that we haven't found ourselves as
          // a module (pfnID!!).

          // Set NLS text of the module if it support it!
          pfnSetNLSText = pfnNLS;
          if (pfnSetNLSText)
          {
            int i;

#ifdef DEBUG_LOGGING
            AddLog("[internal_GetListOfModules] : Setting NLS texts...\n");
#endif
            if (DosRequestMutexSem(hmtxUseNLSTextArray, SEM_INDEFINITE_WAIT)==NO_ERROR)
            {
              for (i=0; i<SSMODULE_NLSTEXT_MAX+1; i++)
                pfnSetNLSText(i, apchNLSText[i]);

#ifdef DEBUG_LOGGING
              for (i=0; i<SSMODULE_NLSTEXT_MAX+1; i++)
              {
                if (apchNLSText[i])
                {
                  AddLog("[internal_GetListOfModules] : NLS text : ");
                  AddLog(apchNLSText[i]);
                  AddLog("\n");
                }
              }
#endif


              DosReleaseMutexSem(hmtxUseNLSTextArray);
            }
          }

#ifdef DEBUG_LOGGING
          AddLog("[internal_GetListOfModules] : Get module desc...\n");
#endif

          pfnGetModuleDesc = pfn1;
          pfnGetModuleDesc(&ModuleDesc, achDSSBasePath);

          // Make it possible for the module to clean NLS texts!
          if (pfnSetNLSText)
          {
            int i;

#ifdef DEBUG_LOGGING
            AddLog("[internal_GetListOfModules] : Clearing NLS texts...\n");
#endif
            for (i=0; i<SSMODULE_NLSTEXT_MAX+1; i++)
              pfnSetNLSText(i, NULL);
          }

          pTemp = (ModuleList_p) malloc(sizeof(ModuleList_t));
          if (pTemp)
          {
            strcpy(pTemp->achFileName, FindBuffer.achName);
            strcpy(pTemp->achModuleName, ModuleDesc.achModuleName);
            pTemp->pNext = NULL;
            if (pLast)
            {
              pLast->pNext = pTemp;
              pLast = pTemp;
            } else
            {
              pResult = pLast = pTemp;
            }
          }
        }

#ifdef DEBUG_LOGGING
        AddLog("[internal_GetListOfModules] : Free DLL\n");
        {
          char achTemp[64];
          AddLog("[internal_GetListOfModules] : Unloading HMOD : ");
          sprintf(achTemp, "0x%x\n", hmodDLL);
          AddLog(achTemp);
        }
#endif

        if (bWasLoaded)
        {
          // We don't use the DLL anymore.
          DosFreeModule(hmodDLL);
          bWasLoaded = 0;
        }
      }
#ifdef DEBUG_LOGGING
      else
        AddLog("[internal_GetListOfModules] : Could not load DLL\n");
#endif


      ulFindCount = 1; // Reset find count

      rcfind = DosFindNext( hdirFindHandle,  // Find handle
                            &FindBuffer,     // Result buffer
                            ulResultBufLen,  // Result buffer length
                            &ulFindCount);   // Number of entries to find
    } while (rcfind == NO_ERROR);

#ifdef DEBUG_LOGGING
    AddLog("[internal_GetListOfModules] : End of search\n");
#endif

    // Close directory search handle
    rcfind = DosFindClose(hdirFindHandle);
  }

#ifdef DEBUG_LOGGING
  AddLog("[internal_GetListOfModules] : Returning with results\n");
#endif


  return pResult;
}

static void internal_FreeListOfModules(ModuleList_p pModuleList)
{
  ModuleList_p pTemp;

#ifdef DEBUG_LOGGING
  AddLog("[internal_FreeListOfModules] : Enter\n");
#endif

  while (pModuleList)
  {
    pTemp = pModuleList;
    pModuleList = pModuleList->pNext;
    free(pTemp);
  }

#ifdef DEBUG_LOGGING
  AddLog("[internal_FreeListOfModules] : Leave\n");
#endif

}


static int internal_GetNumOfModules()
{
  int iResult = 0;
  ModuleList_p pModuleList, pTemp;

#ifdef DEBUG_LOGGING
  AddLog("[internal_GetNumModules] : Enter\n");
#endif

  // If not all modules, then simply return the number of modules in config!
  if (!bUseAllAvailableModules)
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_GetNumModules] : Shortcut return (from config file)\n");
#endif

    return iNumOfModules;
  }

  // Otherwise, count the number of available DLLs!
  pModuleList = internal_GetListOfModules(FALSE);

  pTemp = pModuleList;
  while (pTemp)
  {
    iResult++;
    pTemp = pTemp->pNext;
  }

  internal_FreeListOfModules(pModuleList);

#ifdef DEBUG_LOGGING
  AddLog("[internal_GetNumModules] : Leave\n");
#endif

  return iResult;
}

static void internal_LoadCurrModule()
{
  ModuleList_p pModuleList, pTemp;
  int iCount = 0;
  HMODULE hmodDLL;
  char achFileNamePath[CCHMAXPATHCOMP+15];
  char achBasePath[CCHMAXPATHCOMP+15];
  char achFailure[32];
  APIRET rc;

#ifdef DEBUG_LOGGING
  AddLog("[internal_LoadCurrModule] : Enter\n");
#endif

  // Free old module, we'll load a new instead!
  internal_FreeModule();

  // Get list of available modules
  if (bUseAllAvailableModules)
    pModuleList = internal_GetListOfModules(FALSE);
  else
    pModuleList = internal_GetListOfModules(TRUE);

#ifdef DEBUG_LOGGING
  AddLog("[internal_LoadCurrModule] : Find module entry in list\n");
#endif

  // Find the current module entry in the list
  iCount = 0;
  pTemp = pModuleList;
  while ((pTemp) && (iCount < iCurrModuleNum))
  {
    pTemp = pTemp->pNext;
    iCount++;
  }

  // If found, then load that module!
  if (pTemp)
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_LoadCurrModule] : Found it, so load it\n");
#endif

    // Load the given module!

    // For this, get the folder where we're!
    rc = DosQueryModuleName(hmodOurDLLHandle,
                            sizeof(achBasePath),
                            achBasePath);
    if (rc!=NO_ERROR)
    {
      // Eeek, something is wrong.

      // Have some kind of default then:
      snprintf(achBasePath, sizeof(achBasePath), "%sModules", achCurrentHomeDirectory);
    } else
    {
      // Fine! We have the fully qualified path!
      int i;

      // Search for the base directory then!
      i = strlen(achBasePath)-1;
      while ((i>=0) && (achBasePath[i]!='\\'))
        i--;

      if (i>=0)
      {
        achBasePath[i] = 0;
      } else
      {
        achBasePath[0] = 0;
      }

    }

    // Try to open this DLL
    sprintf(achFileNamePath, "%s\\%s",achBasePath, pTemp->achFileName);

#ifdef DEBUG_LOGGING
    AddLog("[internal_LoadCurrModule] : Loading DLL: ");
    AddLog(achFileNamePath);
    AddLog("\n");
#endif


    rc = DosLoadModule(achFailure, sizeof(achFailure),
                         achFileNamePath,
                         &hmodDLL);

    if (rc==NO_ERROR)
    {
      // Cool, DLL could be loaded!
      PFN pfn1, pfn2, pfn3, pfn4;


#ifdef DEBUG_LOGGING
        {
          char achTemp[64];
          AddLog("[internal_LoadCurrModule] : Loaded, HMOD is ");
          sprintf(achTemp, "0x%x\n", hmodDLL);
          AddLog(achTemp);
        }
#endif

#ifdef DEBUG_LOGGING
      AddLog("[internal_LoadCurrModule] : Query function pointers of DLL\n");
#endif

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


      if ((pfn1) && (pfn2) && (pfn3) && (pfn4))
      {
        // Good, it had all the required functions, so it seems
        // to be a valid screensaver module, it's not we!

        // Found the one we wanted!

        hmodForeignDLL = hmodDLL;

        pfnGetModuleDesc = pfn1;
        pfnConfigure = pfn2;
        pfnStartSaving = pfn3;
        pfnStopSaving = pfn4;


#ifdef DEBUG_LOGGING
        AddLog("[internal_LoadCurrModule] : Query optional function pointers of DLL\n");
#endif

        // Also query optional functions
        rc = DosQueryProcAddr(hmodDLL,
                              0,
                              "SSModule_AskUserForPassword",
                              &pfn1);
        if (rc!=NO_ERROR) pfn1 = NULL;
        pfnAskUserForPassword = pfn1;

        rc = DosQueryProcAddr(hmodDLL,
                              0,
                              "SSModule_ShowWrongPasswordNotification",
                              &pfn1);
        if (rc!=NO_ERROR) pfn1 = NULL;
        pfnShowWrongPasswordNotification = pfn1;

        rc = DosQueryProcAddr(hmodDLL,
                              0,
                              "SSModule_PauseSaving",
                              &pfn1);
        if (rc!=NO_ERROR) pfn1 = NULL;
        pfnPauseSaving = pfn1;

        rc = DosQueryProcAddr(hmodDLL,
                              0,
                              "SSModule_ResumeSaving",
                              &pfn1);
        if (rc!=NO_ERROR) pfn1 = NULL;
        pfnResumeSaving = pfn1;

        rc = DosQueryProcAddr(hmodDLL,
                              0,
                              "SSModule_SetNLSText",
                              &pfn1);
        if (rc!=NO_ERROR) pfn1 = NULL;
        pfnSetNLSText = pfn1;

        rc = DosQueryProcAddr(hmodDLL,
                              0,
                              "SSModule_SetCommonSetting",
                              &pfn1);
        if (rc!=NO_ERROR) pfn1 = NULL;
        pfnSetCommonSetting = pfn1;

        if (pfnSetNLSText)
        {
          // Tell it the NLS text, if it supports it!
          int i;

#ifdef DEBUG_LOGGING
          AddLog("[internal_LoadCurrModule] : Tell NLS strings\n");
#endif

          if (DosRequestMutexSem(hmtxUseNLSTextArray, SEM_INDEFINITE_WAIT)==NO_ERROR)
          {

            for (i=0; i<SSMODULE_NLSTEXT_MAX+1; i++)
              pfnSetNLSText(i, apchNLSText[i]);

            DosReleaseMutexSem(hmtxUseNLSTextArray);
          }
        }

        if (pfnSetCommonSetting)
        {
#ifdef DEBUG_LOGGING
          AddLog("[internal_GetListOfModules] : Tell common module settings...\n");
#endif

          // Tell the module the common settings we know about:
          pfnSetCommonSetting(SSMODULE_COMMONSETTING_FIRSTKEYGOESTOPWDWINDOW,
                              (void *) bFirstKeyGoesToPwdWindow);
          // ...
          // No more common settings yet.
        }

#ifdef DEBUG_LOGGING
        AddLog("[internal_GetListOfModules] : Ok, module is loaded and configured.\n");
#endif

        // Yippiiieee! Done!
      } else
      {
#ifdef DEBUG_LOGGING
        AddLog("[internal_GetListOfModules] : Bad module, unload it!\n");
        {
          char achTemp[64];
          AddLog("[internal_GetListOfModules] : Unloading HMOD : ");
          sprintf(achTemp, "0x%x\n", hmodDLL);
          AddLog(achTemp);
        }
#endif

        // Not a good module!
        DosFreeModule(hmodDLL);
      }
    } else
    {
      // Could not load DLL!
#ifdef DEBUG_LOGGING
      AddLog("[internal_GetListOfModules] : Could not load DLL.\n");
#endif

    }
  }

#ifdef DEBUG_LOGGING
  AddLog("[internal_GetListOfModules] : Free list of modules.\n");
#endif

  internal_FreeListOfModules(pModuleList);

#ifdef DEBUG_LOGGING
  AddLog("[internal_GetListOfModules] : Leave.\n");
#endif

}

static void internal_LoadNextModule()
{
  int iNumOfModulesNow;

#ifdef DEBUG_LOGGING
  AddLog("[internal_LoadNextModule] : Enter.\n");
#endif

  internal_FreeModule();

#ifdef DEBUG_LOGGING
  AddLog("[internal_LoadNextModule] : Getting number of modules\n");
#endif

  iNumOfModulesNow = internal_GetNumOfModules();

  if (iNumOfModulesNow<=0) return;

#ifdef DEBUG_LOGGING
  AddLog("[internal_LoadNextModule] : Setting new module number\n");
#endif

  if (bChangeRandomly)
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_LoadNextModule] : In random mode, getting next module.\n");
#endif

    iCurrModuleNum = rand() % iNumOfModulesNow;
  } else
  {
#ifdef DEBUG_LOGGING
    AddLog("[internal_LoadNextModule] : In sequential mode, getting next module.\n");
#endif

    iCurrModuleNum++;
    iCurrModuleNum = iCurrModuleNum % iNumOfModulesNow;
  }

#ifdef DEBUG_LOGGING
  AddLog("[internal_LoadNextModule] : Loading current module\n");
#endif

  // Now that we know the current module number, try
  // to load that!
  internal_LoadCurrModule();

#ifdef DEBUG_LOGGING
  AddLog("[internal_LoadNextModule] : Leave.\n");
#endif

}

void fnChangerThread(void *p)
{
  ULONG ulTimeStart, ulTimeNow, ulTimePrev;

#ifdef DEBUG_LOGGING
  AddLog("[fnChangerThread] : Started!\n");
#endif

  srand(time(NULL));

  if (DosRequestMutexSem(hmtxUseForeignModule, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {
#ifdef DEBUG_LOGGING
    AddLog("[fnChangerThread] : Loading first module\n");
#endif
    iCurrModuleNum=-1;
    internal_LoadNextModule();

#ifdef DEBUG_LOGGING
    AddLog("[fnChangerThread] : Calling module's startsaving...\n");
#endif

    if (pfnStartSaving)
      pfnStartSaving(hwndCurrentParent,
                     achCurrentHomeDirectory,
                     bCurrentPreviewMode);

    DosReleaseMutexSem(hmtxUseForeignModule);
  }

#ifdef DEBUG_LOGGING
  AddLog("[fnChangerThread] : Entering main loop...\n");
#endif

  bChangerThreadShutdownReq = 0;
  iChangerThreadState = STATE_RUNNING;

  DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &(ulTimeStart), 4 );
  ulTimePrev = ulTimeStart;
  while (!bChangerThreadShutdownReq)
  {
    DosSleep(32);
    DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &(ulTimeNow), 4 );
    if (ulTimeNow < ulTimeStart)
    {
      /* The counter has been turned around! */
      /* Not a good solution, but a quick hack: */
      ulTimeStart = ulTimeNow;
    }
    ulTimePrev = ulTimeNow;
    if (ulTimeNow-ulTimeStart >= iChangeTime * 60 * 1000) // iChangeTime is in minutes
    {
#ifdef DEBUG_LOGGING
      AddLog("[fnChangerThread] : Changing module!\n");
#endif
      ulTimeStart = ulTimeNow;

      // Change module!
      if (DosRequestMutexSem(hmtxUseForeignModule, SEM_INDEFINITE_WAIT)==NO_ERROR)
      {
        // Only change modules if we're not paused!
        // (no need to change modules while the screen is turned off...)
        if (!bPaused)
        {
#ifdef DEBUG_LOGGING
          AddLog("[fnChangerThread] : Stopping old module...\n");
#endif
          if (pfnStopSaving)
            pfnStopSaving();

#ifdef DEBUG_LOGGING
          AddLog("[fnChangerThread] : Loading next module...\n");
#endif
          internal_LoadNextModule();

#ifdef DEBUG_LOGGING
          AddLog("[fnChangerThread] : Starting new module...\n");
#endif
          if (pfnStartSaving)
            pfnStartSaving(hwndCurrentParent,
                           achCurrentHomeDirectory,
                           bCurrentPreviewMode);
#ifdef DEBUG_LOGGING
          AddLog("[fnChangerThread] : Module changed.\n");
#endif

        }
        DosReleaseMutexSem(hmtxUseForeignModule);
      }
#ifdef DEBUG_LOGGING
      AddLog("[fnChangerThread] : Change done.\n");
#endif
    }
  };

#ifdef DEBUG_LOGGING
  AddLog("[fnChangerThread] : Out of loop!\n");
#endif

  if (DosRequestMutexSem(hmtxUseForeignModule, SEM_INDEFINITE_WAIT)==NO_ERROR)
  {

#ifdef DEBUG_LOGGING
    AddLog("[fnChangerThread] : Stopping foreign module.\n");
#endif

    if (pfnStopSaving)
      pfnStopSaving();

#ifdef DEBUG_LOGGING
    AddLog("[fnChangerThread] : Free module.\n");
#endif

    // Free current module!
    internal_FreeModule();

    DosReleaseMutexSem(hmtxUseForeignModule);
  }

#ifdef DEBUG_LOGGING
    AddLog("[fnChangerThread] : Leave.\n");
#endif

  iChangerThreadState = STATE_NOTRUNNING;
  _endthread();
}

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

// Temp checkbox to get size
#define ID_TEMPCHECKBOX     5010

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
        sprintf(pchFileName, "%sModules\\Random\\%s.msg", pchHomeDirectory, achRealLocaleName);
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
          sprintf(pchFileName, "%sModules\\Random\\%s.msg", pchHomeDirectory, pchLang);
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

    // Groupbox-1
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0002"))
      WinSetDlgItemText(hwnd, GB_CHANGINGMODULES, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0003"))
      WinSetDlgItemText(hwnd, ST_GOFORNEXTMODULEAFTER, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0004"))
      WinSetDlgItemText(hwnd, ST_MINUTES, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0005"))
      WinSetDlgItemText(hwnd, CB_USERANDOMMODULEORDER, achTemp);

    // Groupbox-2
    if (sprintmsg(achTemp, hfNLSFile, "CFG_0006"))
      WinSetDlgItemText(hwnd, GB_MODULESTOUSE, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0007"))
      WinSetDlgItemText(hwnd, RB_USEALLAVAILABLEMODULES, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0008"))
      WinSetDlgItemText(hwnd, RB_USEMODULESINTHISLIST, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0009"))
      WinSetDlgItemText(hwnd, PB_UP, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0010"))
      WinSetDlgItemText(hwnd, PB_DN, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0011"))
      WinSetDlgItemText(hwnd, PB_ADD, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0012"))
      WinSetDlgItemText(hwnd, PB_ADDALL, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0013"))
      WinSetDlgItemText(hwnd, PB_REMOVEALL, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0014"))
      WinSetDlgItemText(hwnd, PB_REMOVE, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0015"))
      WinSetDlgItemText(hwnd, PB_OK, achTemp);

    if (sprintmsg(achTemp, hfNLSFile, "CFG_0016"))
      WinSetDlgItemText(hwnd, PB_CANCEL, achTemp);

    internal_CloseNLSFile(hfNLSFile);
  }
}

static void internal_AdjustConfigDialogControls(HWND hwnd)
{
  SWP swpTemp, swpTemp2, swpTemp3, swpWindow;
  ULONG CXDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
  ULONG CYDLGFRAME = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
  int staticheight, radioheight, gbheight;
  int iDisabledWindowUpdate=0;
  int iCX, iCY, iCenterButtonWidth, iBottomButtonsWidth, iUpDnButtonWidth;
  int iCBCX, iCBCY; // Checkbox extra sizes
  int iRBCX, iRBCY; // RadioButton extra sizes

  if (WinIsWindowVisible(hwnd))
  {
    WinEnableWindowUpdate(hwnd, FALSE);
    iDisabledWindowUpdate = 1;
  }

  // Get window size!
  WinQueryWindowPos(hwnd, &swpWindow);

  // Query static height in pixels.
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_MINUTES), &swpTemp);
  staticheight = swpTemp.cy;

  // Query radio button height in pixels.
  WinQueryWindowPos(WinWindowFromID(hwnd, RB_USEALLAVAILABLEMODULES), &swpTemp);
  radioheight = swpTemp.cy;

  // Get extra sizes
  internal_GetCheckboxExtraSize(hwnd, CB_USERANDOMMODULEORDER, &iCBCX, &iCBCY);
  internal_GetRadioButtonExtraSize(hwnd, RB_USEALLAVAILABLEMODULES, &iRBCX, &iRBCY);

  // Let's see how wide the window will be!
  internal_GetStaticTextSize(hwnd, PB_REMOVEALL, &iCX, &iCY);
  iCenterButtonWidth = iCX;
  internal_GetStaticTextSize(hwnd, PB_REMOVE, &iCX, &iCY);
  if (iCX>iCenterButtonWidth) iCenterButtonWidth = iCX;
  internal_GetStaticTextSize(hwnd, PB_ADD, &iCX, &iCY);
  if (iCX>iCenterButtonWidth) iCenterButtonWidth = iCX;
  internal_GetStaticTextSize(hwnd, PB_ADDALL, &iCX, &iCY);
  if (iCX>iCenterButtonWidth) iCenterButtonWidth = iCX;

  // Calculate width of Ok and Cancel buttons!
  internal_GetStaticTextSize(hwnd, PB_CANCEL, &iCX, &iCY);
  iBottomButtonsWidth = iCX;
  internal_GetStaticTextSize(hwnd, PB_OK, &iCX, &iCY);
  if (iBottomButtonsWidth<iCX) iBottomButtonsWidth = iCX;

  // Calculate width of Up and Down buttons!
  internal_GetStaticTextSize(hwnd, PB_DN, &iCX, &iCY);
  iUpDnButtonWidth = iCX;
  internal_GetStaticTextSize(hwnd, PB_UP, &iCX, &iCY);
  if (iUpDnButtonWidth<iCX) iUpDnButtonWidth = iCX;


  // Assemble window width:
  swpWindow.cx = iCenterButtonWidth + 6*CXDLGFRAME; // Pushbutton size
  swpWindow.cx += 2*(CXDLGFRAME + 2*(iBottomButtonsWidth + 6*CXDLGFRAME) + CXDLGFRAME); // Two listboxes plus area around them
  swpWindow.cx += CXDLGFRAME + iUpDnButtonWidth + 6*CXDLGFRAME; // Up/Dn pushbuttons
  swpWindow.cx += 6*CXDLGFRAME; // Groupbox plus frame, plus margins

  // Now the two pushbuttons
  WinSetWindowPos(WinWindowFromID(hwnd, PB_OK), HWND_TOP,
                  (swpWindow.cx/2 - (iBottomButtonsWidth + 6*CXDLGFRAME))/2,
                  CYDLGFRAME*2,
		  iBottomButtonsWidth + 6*CXDLGFRAME,
		  iCY + 3*CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);

  WinSetWindowPos(WinWindowFromID(hwnd, PB_CANCEL), HWND_TOP,
                  swpWindow.cx/2 + (swpWindow.cx/2 - (iBottomButtonsWidth + 6*CXDLGFRAME))/2,
                  CYDLGFRAME*2,
		  iBottomButtonsWidth + 6*CXDLGFRAME,
		  iCY + 3*CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);

  // First set the "Modules to use" groupbox
  gbheight =
    CYDLGFRAME + // frame
    CYDLGFRAME + // Margin
    (iCY + 3*CYDLGFRAME) + // pushbutton
    CYDLGFRAME + // Margin
    (iCY + 3*CYDLGFRAME) + // pushbutton
    CYDLGFRAME + // Margin
    (iCY + 3*CYDLGFRAME) + // pushbutton
    CYDLGFRAME + // Margin
    (iCY + 3*CYDLGFRAME) + // pushbutton
    CYDLGFRAME + // Margin
    radioheight + // radio button
    radioheight + // radio button
    2*CYDLGFRAME + // Margin
    staticheight; // Text of groupbox

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_OK), &swpTemp);
  WinSetWindowPos(WinWindowFromID(hwnd, GB_MODULESTOUSE),
		  HWND_TOP,
                  2*CXDLGFRAME,
                  swpTemp.y + swpTemp.cy + CYDLGFRAME,
                  swpWindow.cx - 4*CXDLGFRAME,
                  gbheight,
                  SWP_MOVE | SWP_SIZE);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_MODULESTOUSE), &swpTemp);

  // Things inside this groupbox:

  // Two buttons (up n dn)
  internal_GetStaticTextSize(hwnd, PB_DN, &iCX, &iCY);
  iCX = iUpDnButtonWidth;
  WinSetWindowPos(WinWindowFromID(hwnd, PB_DN), HWND_TOP,
                  swpTemp.x + 2*CXDLGFRAME,
		  swpTemp.y + 2*CYDLGFRAME,
		  iCX + 6*CXDLGFRAME,
		  iCY + 3*CYDLGFRAME,
                  SWP_MOVE | SWP_SIZE);

  WinSetWindowPos(WinWindowFromID(hwnd, PB_UP), HWND_TOP,
                  swpTemp.x + 2*CXDLGFRAME,
		  swpTemp.y + 2*CYDLGFRAME + 3*(iCY+3*CYDLGFRAME + CYDLGFRAME),
		  iCX + 6*CXDLGFRAME,
		  iCY + 3*CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  // Radiobuttons
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_UP), &swpTemp2);
  internal_GetStaticTextSize(hwnd, RB_USEMODULESINTHISLIST, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, RB_USEMODULESINTHISLIST), HWND_TOP,
                  swpTemp2.x,
		  swpTemp2.y + swpTemp2.cy + 2*CYDLGFRAME,
                  iCX + iCBCX, iCBCY,
		  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, RB_USEMODULESINTHISLIST), &swpTemp2);
  internal_GetStaticTextSize(hwnd, RB_USEALLAVAILABLEMODULES, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, RB_USEALLAVAILABLEMODULES), HWND_TOP,
                  swpTemp2.x,
		  swpTemp2.y + swpTemp2.cy,
                  iCX + iCBCX, iCBCY,
		  SWP_MOVE | SWP_SIZE);

  // Left listbox
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_DN), &swpTemp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, PB_UP), &swpTemp3);
  iCX = 2*(iBottomButtonsWidth + 6*CXDLGFRAME);
  WinSetWindowPos(WinWindowFromID(hwnd, LB_MODULESINUSE), HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
		  swpTemp2.y,
                  iCX, swpTemp3.y + swpTemp3.cy - swpTemp2.y,
		  SWP_MOVE | SWP_SIZE);


  // The buttons between the listboxes
  WinQueryWindowPos(WinWindowFromID(hwnd, LB_MODULESINUSE), &swpTemp2);
  internal_GetStaticTextSize(hwnd, PB_REMOVEALL, &iCX, &iCY); // Widest button text
  WinSetWindowPos(WinWindowFromID(hwnd, PB_REMOVE), HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
		  swpTemp2.y,
		  iCenterButtonWidth + 6*CXDLGFRAME,
		  iCY + 3*CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_REMOVE), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_REMOVEALL), HWND_TOP,
                  swpTemp2.x,
		  swpTemp2.y + swpTemp2.cy+ CYDLGFRAME,
		  swpTemp2.cx,
		  swpTemp2.cy,
		  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_REMOVEALL), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_ADDALL), HWND_TOP,
                  swpTemp2.x,
		  swpTemp2.y + swpTemp2.cy+ CYDLGFRAME,
		  swpTemp2.cx,
		  swpTemp2.cy,
		  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_ADDALL), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, PB_ADD), HWND_TOP,
                  swpTemp2.x,
		  swpTemp2.y + swpTemp2.cy+ CYDLGFRAME,
		  swpTemp2.cx,
		  swpTemp2.cy,
		  SWP_MOVE | SWP_SIZE);

  WinQueryWindowPos(WinWindowFromID(hwnd, PB_REMOVEALL), &swpTemp2);
  WinQueryWindowPos(WinWindowFromID(hwnd, LB_MODULESINUSE), &swpTemp3);
  WinSetWindowPos(WinWindowFromID(hwnd, LB_ALLMODULES), HWND_TOP,
                  swpTemp2.x + swpTemp2.cx+ CXDLGFRAME,
		  swpTemp3.y,
		  swpTemp3.cx,
		  swpTemp3.cy,
		  SWP_MOVE | SWP_SIZE);

  // Then the "Changing modules" groupbox
  gbheight =
    CYDLGFRAME + // frame
    CYDLGFRAME + // Margin
    radioheight + // radiobutton
    CYDLGFRAME + // Margin
    (staticheight + CYDLGFRAME) + // spinbutton
    CYDLGFRAME + // Margin
    staticheight; // Text of groupbox

  WinQueryWindowPos(WinWindowFromID(hwnd, GB_MODULESTOUSE), &swpTemp);
  WinSetWindowPos(WinWindowFromID(hwnd, GB_CHANGINGMODULES),
		  HWND_TOP,
                  2*CXDLGFRAME,
                  swpTemp.y + swpTemp.cy + CYDLGFRAME,
                  swpWindow.cx - 4*CXDLGFRAME,
                  gbheight,
                  SWP_MOVE | SWP_SIZE);
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_CHANGINGMODULES), &swpTemp);

  // Things inside this groupbox:

  // checkbox
  internal_GetStaticTextSize(hwnd, CB_USERANDOMMODULEORDER, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, CB_USERANDOMMODULEORDER), HWND_TOP,
                  swpTemp.x + 2*CXDLGFRAME,
		  swpTemp.y + 2*CYDLGFRAME,
                  iCX+iCBCX, iCBCY,
                  SWP_MOVE | SWP_SIZE);

  // Static text
  WinQueryWindowPos(WinWindowFromID(hwnd, CB_USERANDOMMODULEORDER), &swpTemp2);
  internal_GetStaticTextSize(hwnd, ST_GOFORNEXTMODULEAFTER, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_GOFORNEXTMODULEAFTER), HWND_TOP,
                  swpTemp2.x,
		  swpTemp2.y + swpTemp2.cy + 2*CYDLGFRAME,
		  iCX, iCY,
		  SWP_MOVE | SWP_SIZE);

  // Spinbox
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_GOFORNEXTMODULEAFTER), &swpTemp2);
  WinSetWindowPos(WinWindowFromID(hwnd, SPB_NEXTMODULETIME), HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
		  swpTemp2.y - CYDLGFRAME/2,
                  CXDLGFRAME*12,
                  staticheight + CYDLGFRAME,
		  SWP_MOVE | SWP_SIZE);

  // static text
  WinQueryWindowPos(WinWindowFromID(hwnd, ST_GOFORNEXTMODULEAFTER), &swpTemp3);
  WinQueryWindowPos(WinWindowFromID(hwnd, SPB_NEXTMODULETIME), &swpTemp2);
  internal_GetStaticTextSize(hwnd, ST_MINUTES, &iCX, &iCY);
  WinSetWindowPos(WinWindowFromID(hwnd, ST_MINUTES),
                  HWND_TOP,
                  swpTemp2.x + swpTemp2.cx + CXDLGFRAME,
                  swpTemp3.y,
                  iCX, iCY,
                  SWP_MOVE | SWP_SIZE);

  // Set window size so the contents will fit perfectly!
  WinQueryWindowPos(WinWindowFromID(hwnd, GB_MODULESTOUSE), &swpTemp);
  swpWindow.cy = swpTemp.cy + swpTemp.y + CYDLGFRAME + gbheight + CYDLGFRAME + CYDLGFRAME + WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);

  WinSetWindowPos(hwnd, HWND_TOP,
                  0, 0,
                  swpWindow.cx, swpWindow.cy,
                  SWP_SIZE);
  // That's all!
  if (iDisabledWindowUpdate)
    WinEnableWindowUpdate(hwnd, TRUE);
}

static void internal_SetConfigDialogInitialControlValues(HWND hwnd, ModuleList_p pModuleList)
{
  ModuleList_p pTemp;
  SHORT sItemIndex;
  int i;

  // Set control limits
  WinSendDlgItemMsg(hwnd, SPB_NEXTMODULETIME,
                    SPBM_SETLIMITS,
                    (MPARAM) 60, // Maximum 60 minutes
                    (MPARAM) 1); // Minimum 1 minute

  // Set control values
  WinCheckButton(hwnd, CB_USERANDOMMODULEORDER, bChangeRandomly);

  WinSendDlgItemMsg(hwnd, SPB_NEXTMODULETIME, SPBM_SETCURRENTVALUE,
		    (MPARAM) (iChangeTime),
		    (MPARAM) NULL);

  // Empty both listboxes
  WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);
  WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_DELETEALL, (MPARAM) 0, (MPARAM) 0);

  // Fill allmodules listbox with all the available modules
  pTemp = pModuleList;
  while (pTemp)
  {
    sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_INSERTITEM,
					   (MPARAM) LIT_SORTASCENDING,
					   (MPARAM) (pTemp->achModuleName));
    WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_SETITEMHANDLE,
		      (MPARAM) sItemIndex,
		      (MPARAM) pTemp);
    pTemp = pTemp->pNext;
  }

  // Move modules from available to in-use
  if (ppchModules)
    for (i=0; i<iNumOfModules; i++)
    {
      pTemp = pModuleList;
      while ((pTemp) && (strcmp(pTemp->achFileName, ppchModules[i])))
	pTemp = pTemp->pNext;
      if (pTemp)
      {
	// Found in the right listbox!
	// Move it to the left then!
	sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_SEARCHSTRING,
					       MPFROM2SHORT(0, LIT_FIRST),
					       pTemp->achModuleName);

	if ((sItemIndex!=LIT_NONE) && (sItemIndex!=LIT_ERROR))
	{
          // Remove from left box
	  WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_DELETEITEM,
			    (MPARAM) sItemIndex,
			    (MPARAM) 0);
	  // Add to right box
	  sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_INSERTITEM,
						 (MPARAM) LIT_END,
						 (MPARAM) (pTemp->achModuleName));
	  WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_SETITEMHANDLE,
			    (MPARAM) sItemIndex,
                            (MPARAM) pTemp);
	}
      }
    }

  // Enable/disable controls
  WinSendDlgItemMsg(hwnd, RB_USEALLAVAILABLEMODULES,
		    BM_SETCHECK,
		    (MRESULT) (bUseAllAvailableModules),
		    (MRESULT) 0);

  WinSendDlgItemMsg(hwnd, RB_USEMODULESINTHISLIST,
		    BM_SETCHECK,
		    (MRESULT) (!bUseAllAvailableModules),
		    (MRESULT) 0);

  // Disable other controls
  WinEnableWindow(WinWindowFromID(hwnd, LB_MODULESINUSE), !bUseAllAvailableModules);
  WinEnableWindow(WinWindowFromID(hwnd, PB_ADD), !bUseAllAvailableModules);
  WinEnableWindow(WinWindowFromID(hwnd, PB_ADDALL), !bUseAllAvailableModules);
  WinEnableWindow(WinWindowFromID(hwnd, PB_REMOVE), !bUseAllAvailableModules);
  WinEnableWindow(WinWindowFromID(hwnd, PB_REMOVEALL), !bUseAllAvailableModules);
  WinEnableWindow(WinWindowFromID(hwnd, LB_ALLMODULES), !bUseAllAvailableModules);
  WinEnableWindow(WinWindowFromID(hwnd, PB_UP), !bUseAllAvailableModules);
  WinEnableWindow(WinWindowFromID(hwnd, PB_DN), !bUseAllAvailableModules);

}

MRESULT EXPENTRY fnConfigDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  ConfigDlgInitParams_p pCfgDlgInit;
  HWND hwndOwner;
  SWP swpDlg, swpOwner;
  POINTL ptlTemp;
  ModuleList_p pModuleList;
  SHORT sItemIndex, sMaxItemIndex;

  switch (msg)
  {
    case WM_INITDLG:

      bWMINITDLGDone = 0;

      // Get init parameter
      pCfgDlgInit = (ConfigDlgInitParams_p) mp2;

      /* This window initialization may take a while, because it queries all available */
      /* modules, so we'll show wait pointer for this time */
      WinSetPointer(HWND_DESKTOP,
                    WinQuerySysPointer(HWND_DESKTOP, SPTR_WAIT, FALSE));

      // Get list of available modules
      pModuleList = internal_GetListOfModules(FALSE); // All the available modules
      WinSetWindowULong(hwnd, QWL_USER, (ULONG) pModuleList);

      // Set dialog control texts and fonts based on
      // current language
      internal_SetPageFont(hwnd);
      internal_SetConfigDialogText(hwnd, pCfgDlgInit->pchHomeDirectory);

      // Arrange controls, and resize dialog window
      internal_AdjustConfigDialogControls(hwnd);

      // Set control values and limits
      internal_SetConfigDialogInitialControlValues(hwnd, pModuleList);

      // Set initial position of dialog window and show it
      WinQueryWindowPos(hwnd, &swpDlg);
      hwndOwner = WinQueryWindow(hwnd, QW_OWNER);
      WinQueryWindowPos(hwndOwner, &swpOwner);
      ptlTemp.x = swpOwner.x; ptlTemp.y = swpOwner.y;
      WinMapWindowPoints(WinQueryWindow(hwndOwner, QW_PARENT),
			 HWND_DESKTOP,
			 &ptlTemp,
                         1);
      WinSetWindowPos(hwnd, HWND_TOP,
		      ptlTemp.x + (swpOwner.cx - swpDlg.cx)/2,
		      ptlTemp.y + (swpOwner.cy - swpDlg.cy)/2,
		      0, 0,
                      SWP_MOVE | SWP_SHOW);

      WinSetPointer(HWND_DESKTOP,
                    WinQuerySysPointer(HWND_DESKTOP, SPTR_ARROW, FALSE));

      bWMINITDLGDone = 1;

      return (MRESULT) FALSE;

    case WM_DESTROY:

      // Recreate module list
      if (ppchModules)
      {
	int i;

#ifdef DEBUG_LOGGING
	AddLog("[WM_DESTROY] : Deleting old ppchModules\n");
#endif

	for (i=0; i<iNumOfModules; i++)
	  if (ppchModules[i])
	    free(ppchModules[i]);
        free(ppchModules); ppchModules = NULL;
      }
      // Get number of modules in left side
      sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_QUERYITEMCOUNT,
					     (MPARAM) 0,
					     (MPARAM) 0);
      iNumOfModules = 0;
      if (sItemIndex>0)
      {
#ifdef DEBUG_LOGGING
	AddLog("[WM_DESTROY] : Recreating ppchModules\n");
#endif
	ppchModules = (char **) malloc(sItemIndex * sizeof(char *));
	if (ppchModules)
	{

#ifdef DEBUG_LOGGING
	  AddLog("[WM_DESTROY] : Going through list\n");
#endif

	  while (sItemIndex>0)
	  {
	    ModuleList_p pTemp;
	    // Get its handle
	    pTemp = (ModuleList_p) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_QUERYITEMHANDLE,
						     (MPARAM) 0, (MPARAM) 0);
	    // Remove from MODULESINUSE
	    WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_DELETEITEM,
			      (MPARAM) 0, (MPARAM) 0);
	    ppchModules[iNumOfModules] = (char *) malloc(strlen(pTemp->achFileName)+1);
	    if (ppchModules[iNumOfModules])
	    {
              strcpy(ppchModules[iNumOfModules], pTemp->achFileName);
	      iNumOfModules++;

#ifdef DEBUG_LOGGING
	      AddLog("[WM_DESTROY] : New modules: [");
	      AddLog(pTemp->achFileName);
              AddLog("]\n");
#endif

	    }

	    // Get again the number of modules in left side
	    sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_QUERYITEMCOUNT,
						   (MPARAM) 0,
						   (MPARAM) 0);
	  }
	}
      }

      // Free list of available modules
      pModuleList = (ModuleList_p) WinQueryWindowULong(hwnd, QWL_USER);
      internal_FreeListOfModules(pModuleList);

#ifdef DEBUG_LOGGING
      {
	char achTemp[128];
        sprintf(achTemp, "%d", iNumOfModules);
	AddLog("[WM_DESTROY] : iNumOfModules is ");
	AddLog(achTemp);
	AddLog("\n");
      }
#endif

      break;

    case WM_COMMAND:
      switch (SHORT1FROMMP(mp2)) {
	case CMDSRC_PUSHBUTTON:           // ---- A WM_COMMAND from a pushbutton ------
	  switch (SHORT1FROMMP(mp1)) {
	    case PB_ADD:
              // Get current selected item in ALLMODULES
	      sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_QUERYSELECTION,
						     (MPARAM) LIT_CURSOR,
						     (MPARAM) 0);
	      if (sItemIndex!=LIT_NONE)
	      {
		ModuleList_p pTemp;
                // Get its handle
		pTemp = (ModuleList_p) WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_QUERYITEMHANDLE,
							 (MPARAM) sItemIndex, (MPARAM) 0);
                // Remove from ALLMODULES
		WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_DELETEITEM,
				  (MPARAM) sItemIndex, (MPARAM) 0);
                // Add to MODULESINUSE
		sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_INSERTITEM,
						       (MPARAM) LIT_END,
						       (MPARAM) (pTemp->achModuleName));
		// Set handle
		WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_SETITEMHANDLE,
				  (MPARAM) sItemIndex, (MPARAM) pTemp);
                // Select it as current
		WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_SELECTITEM,
                                  (MPARAM) sItemIndex, (MPARAM) TRUE);
	      }
              return (MRESULT) TRUE;
	    case PB_ADDALL:
	      // Get number of modules in right side
	      sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_QUERYITEMCOUNT,
						     (MPARAM) 0,
						     (MPARAM) 0);
              while (sItemIndex>0)
	      {
		ModuleList_p pTemp;
                // Get its handle
		pTemp = (ModuleList_p) WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_QUERYITEMHANDLE,
							 (MPARAM) 0, (MPARAM) 0);
                // Remove from ALLMODULES
		WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_DELETEITEM,
				  (MPARAM) 0, (MPARAM) 0);
                // Add to MODULESINUSE
		sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_INSERTITEM,
						       (MPARAM) LIT_END,
						       (MPARAM) (pTemp->achModuleName));
		// Set handle
		WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_SETITEMHANDLE,
				  (MPARAM) sItemIndex, (MPARAM) pTemp);
                // Select it as current
		WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_SELECTITEM,
				  (MPARAM) sItemIndex, (MPARAM) TRUE);

                // Get again the number of modules in left side
		sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_QUERYITEMCOUNT,
						       (MPARAM) 0,
						       (MPARAM) 0);
	      }
              return (MRESULT) TRUE;
	    case PB_REMOVE:
              // Get current selected item in MODULESINUSE
	      sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_QUERYSELECTION,
						     (MPARAM) LIT_CURSOR,
						     (MPARAM) 0);
	      if (sItemIndex!=LIT_NONE)
	      {
		ModuleList_p pTemp;
                // Get its handle
		pTemp = (ModuleList_p) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_QUERYITEMHANDLE,
							 (MPARAM) sItemIndex, (MPARAM) 0);
                // Remove from MODULESINUSE
		WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_DELETEITEM,
				  (MPARAM) sItemIndex, (MPARAM) 0);
                // Add to ALLMODULES
		sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_INSERTITEM,
						       (MPARAM) LIT_SORTASCENDING,
						       (MPARAM) (pTemp->achModuleName));
		// Set handle
		WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_SETITEMHANDLE,
				  (MPARAM) sItemIndex, (MPARAM) pTemp);
                // Select it as current
		WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_SELECTITEM,
                                  (MPARAM) sItemIndex, (MPARAM) TRUE);
	      }
              return (MRESULT) TRUE;
	    case PB_REMOVEALL:
	      // Get number of modules in left side
	      sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_QUERYITEMCOUNT,
						     (MPARAM) 0,
						     (MPARAM) 0);
              while (sItemIndex>0)
	      {
		ModuleList_p pTemp;
                // Get its handle
		pTemp = (ModuleList_p) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_QUERYITEMHANDLE,
							 (MPARAM) 0, (MPARAM) 0);
                // Remove from MODULESINUSE
		WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_DELETEITEM,
				  (MPARAM) 0, (MPARAM) 0);
                // Add to ALLMODULES
		sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_INSERTITEM,
						       (MPARAM) LIT_SORTASCENDING,
						       (MPARAM) (pTemp->achModuleName));
		// Set handle
		WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_SETITEMHANDLE,
				  (MPARAM) sItemIndex, (MPARAM) pTemp);
                // Select it as current
		WinSendDlgItemMsg(hwnd, LB_ALLMODULES, LM_SELECTITEM,
				  (MPARAM) sItemIndex, (MPARAM) TRUE);

                // Get again the number of modules in left side
		sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_QUERYITEMCOUNT,
						       (MPARAM) 0,
						       (MPARAM) 0);
	      }
	      return (MRESULT) TRUE;
	    case PB_UP:
              // Get current selected item in MODULESINUSE
	      sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_QUERYSELECTION,
						     (MPARAM) LIT_CURSOR,
						     (MPARAM) 0);
	      if ((sItemIndex!=LIT_NONE) && (sItemIndex>0))
	      {
		ModuleList_p pTemp;
                SHORT sPos;
                // Get its handle
		pTemp = (ModuleList_p) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_QUERYITEMHANDLE,
							 (MPARAM) sItemIndex, (MPARAM) 0);
                // Remove from MODULESINUSE
		WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_DELETEITEM,
				  (MPARAM) sItemIndex, (MPARAM) 0);
                sPos = sItemIndex-1;
                // Add to MODULESINUSE, new position
		sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_INSERTITEM,
						       (MPARAM) (sPos),
						       (MPARAM) (pTemp->achModuleName));
		// Set handle
		WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_SETITEMHANDLE,
				  (MPARAM) sItemIndex, (MPARAM) pTemp);
                // Select it as current
		WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_SELECTITEM,
                                  (MPARAM) sItemIndex, (MPARAM) TRUE);
	      }
              return (MRESULT) TRUE;
	    case PB_DN:
              // Get current selected item in MODULESINUSE
	      sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_QUERYSELECTION,
						     (MPARAM) LIT_CURSOR,
						     (MPARAM) 0);
	      // Get max
	      sMaxItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_QUERYITEMCOUNT,
							(MPARAM) LIT_CURSOR,
							(MPARAM) 0);
              sMaxItemIndex--;

	      if ((sItemIndex!=LIT_NONE) && (sItemIndex<sMaxItemIndex))
	      {
		ModuleList_p pTemp;
                SHORT sPos;
                // Get its handle
		pTemp = (ModuleList_p) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_QUERYITEMHANDLE,
							 (MPARAM) sItemIndex, (MPARAM) 0);
                // Remove from MODULESINUSE
		WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_DELETEITEM,
				  (MPARAM) sItemIndex, (MPARAM) 0);
                sPos = sItemIndex+1;
                // Add to MODULESINUSE, new position
		sItemIndex = (SHORT) WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_INSERTITEM,
						       (MPARAM) (sPos),
						       (MPARAM) (pTemp->achModuleName));
		// Set handle
		WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_SETITEMHANDLE,
				  (MPARAM) sItemIndex, (MPARAM) pTemp);
                // Select it as current
		WinSendDlgItemMsg(hwnd, LB_MODULESINUSE, LM_SELECTITEM,
                                  (MPARAM) sItemIndex, (MPARAM) TRUE);
	      }
	      return (MRESULT) TRUE;

	    case PB_OK:
	      WinDismissDlg(hwnd, PB_OK);
              return (MRESULT) 0;

	    default:
              break;
	  }
	  break;
        default:
          break;
      }
      break;

    case WM_CONTROL:
      switch (SHORT1FROMMP(mp1))
      {
	case CB_USERANDOMMODULEORDER:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
	    // The button control has been clicked
	    bChangeRandomly = WinQueryButtonCheckstate(hwnd, SHORT1FROMMP(mp1));
	  }
	  break;

	case SPB_NEXTMODULETIME:
	  if ((SHORT2FROMMP(mp1)==SPBN_CHANGE) && (bWMINITDLGDone))
	  {
            // The spin-button control value has been changed
	    ULONG ulValue;

	    ulValue = 1; // Default, in case of problems.
	    WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1),
			      SPBM_QUERYVALUE,
			      (MPARAM) &ulValue,
			      MPFROM2SHORT(0, SPBQ_ALWAYSUPDATE));
            iChangeTime = ulValue;
          }
          break;

	case RB_USEMODULESINTHISLIST:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
	    // Selected this radio button!

	    bUseAllAvailableModules = FALSE; // Change config

	    // Enable/Disable other controls
	    WinEnableWindow(WinWindowFromID(hwnd, LB_MODULESINUSE), !bUseAllAvailableModules);
	    WinEnableWindow(WinWindowFromID(hwnd, PB_ADD), !bUseAllAvailableModules);
	    WinEnableWindow(WinWindowFromID(hwnd, PB_ADDALL), !bUseAllAvailableModules);
	    WinEnableWindow(WinWindowFromID(hwnd, PB_REMOVE), !bUseAllAvailableModules);
	    WinEnableWindow(WinWindowFromID(hwnd, PB_REMOVEALL), !bUseAllAvailableModules);
	    WinEnableWindow(WinWindowFromID(hwnd, LB_ALLMODULES), !bUseAllAvailableModules);
            WinEnableWindow(WinWindowFromID(hwnd, PB_UP), !bUseAllAvailableModules);
            WinEnableWindow(WinWindowFromID(hwnd, PB_DN), !bUseAllAvailableModules);

	    return (MRESULT) 0;
	  }
	  break;
	case RB_USEALLAVAILABLEMODULES:
	  if ((SHORT2FROMMP(mp1)==BN_CLICKED) || (SHORT2FROMMP(mp1)==BN_DBLCLICKED))
	  {
	    // Selected this radio button!

	    bUseAllAvailableModules = TRUE; // Change config

	    // Enable/Disable other controls
	    WinEnableWindow(WinWindowFromID(hwnd, LB_MODULESINUSE), !bUseAllAvailableModules);
	    WinEnableWindow(WinWindowFromID(hwnd, PB_ADD), !bUseAllAvailableModules);
	    WinEnableWindow(WinWindowFromID(hwnd, PB_ADDALL), !bUseAllAvailableModules);
	    WinEnableWindow(WinWindowFromID(hwnd, PB_REMOVE), !bUseAllAvailableModules);
	    WinEnableWindow(WinWindowFromID(hwnd, PB_REMOVEALL), !bUseAllAvailableModules);
	    WinEnableWindow(WinWindowFromID(hwnd, LB_ALLMODULES), !bUseAllAvailableModules);
            WinEnableWindow(WinWindowFromID(hwnd, PB_UP), !bUseAllAvailableModules);
            WinEnableWindow(WinWindowFromID(hwnd, PB_DN), !bUseAllAvailableModules);

	    return (MRESULT) 0;
	  }
	  break;

	default:
	  break;
      }
      break;

    default:
      break;
  }
  return WinDefDlgProc(hwnd, msg, mp1, mp2);
}


SSMODULEDECLSPEC int SSMODULECALL SSModule_GetModuleDesc(SSModuleDesc_p pModuleDesc, char *pchHomeDirectory)
{
  FILE *hfNLSFile;

  if (!pModuleDesc)
    return SSMODULE_ERROR_INVALIDPARAMETER;

  // Return info about module!
  pModuleDesc->iVersionMajor = 1;
  pModuleDesc->iVersionMinor = 80;
  strcpy(pModuleDesc->achModuleName, "Randomizer");
  strcpy(pModuleDesc->achModuleDesc,
         "Randomizer module, using other saver modules.\n"
         "Written by Doodle"
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

#define WM_MODULESTOPPED  0xD00F

void fnModuleStopperThread(void *pParm)
{
  SSModule_StopSaving();
         
  WinPostMsg((HWND) pParm, WM_MODULESTOPPED, (MPARAM) NULL, (MPARAM) NULL); // Notify caller that we're ready!

  _endthread();
}


SSMODULEDECLSPEC int SSMODULECALL SSModule_Configure(HWND hwndOwner, char *pchHomeDirectory)
{
  HWND hwndDlg;
  ConfigDlgInitParams_t CfgDlgInit;
  int iResult;
  HAB hab = WinQueryAnchorBlock(hwndOwner);
  QMSG qmsg;
  TID tidStopper;


#ifdef DEBUG_LOGGING
  AddLog("[SSModule_Configure] : Enter\n");
#endif

  if (bRunning)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSModule_Configure] : Stopping running module\n");
#endif

    tidStopper = _beginthread(fnModuleStopperThread,
                              0,
                              1024*1024,
                              (void *) hwndOwner);

    while (WinGetMsg(hab, &qmsg, NULL, 0, 0))
    {
      if (qmsg.msg == WM_MODULESTOPPED)
      {
        break;
      } else
        WinDispatchMsg(hab, &qmsg);
    }

    DosWaitThread(&tidStopper, DCWW_WAIT);
#ifdef DEBUG_LOGGING
    AddLog("[SSModule_Configure] : Okay, module is stopped\n");
#endif
  }

  // We get this semaphore. This will protect us while
  // pokeing with the configuration, and it will make sure that
  // noone else will poke with it while the config window is open!
  if (DosRequestMutexSem(hmtxUseForeignModule, FOREIGN_MODULE_MUTEX_TIMEOUT)!=NO_ERROR)
    return SSMODULE_ERROR_INTERNALERROR;

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_Configure] : Grabbed semaphore, loading config.\n");
#endif

  CfgDlgInit.cbSize = sizeof(CfgDlgInit);
  CfgDlgInit.pchHomeDirectory = pchHomeDirectory;
  internal_LoadConfig(pchHomeDirectory);

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_Configure] : Loading dialog window\n");
#endif

  hwndDlg = WinLoadDlg(HWND_DESKTOP, hwndOwner,
                       fnConfigDialogProc,
                       hmodOurDLLHandle,
                       DLG_CONFIGURERANDOMIZEMODULE,
                       &CfgDlgInit);

  if (!hwndDlg)
  {
    DosReleaseMutexSem(hmtxUseForeignModule);
    iResult = SSMODULE_ERROR_INTERNALERROR;
  } else
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSModule_Configure] : Processing dialog window\n");
#endif

    iResult = WinProcessDlg(hwndDlg);
    WinDestroyWindow(hwndDlg);

#ifdef DEBUG_LOGGING
    AddLog("[SSModule_Configure] : Cleaning up after the dialog window\n");
#endif

    if (iResult == PB_OK)
    {
      // User dismissed the dialog with OK button,
      // so save configuration!

#ifdef DEBUG_LOGGING
      {
	char achTemp[128];
	sprintf(achTemp, "%d", iNumOfModules);
	AddLog("[SSModule_Configure] : iNumOfModules is ");
	AddLog(achTemp);
	AddLog("\n");
      }
#endif

      internal_SaveConfig(pchHomeDirectory);
    }
    iResult = SSMODULE_NOERROR;
  }

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_Configure] : Releasing mutex\n");
#endif

  DosReleaseMutexSem(hmtxUseForeignModule);

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_Configure] : Leave\n");
#endif

  return iResult;
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_StartSaving(HWND hwndParent, char *pchHomeDirectory, int bPreviewMode)
{
  srand(time(NULL));

  // This is called when we should start the saving.
  // Return error if already running, start the saver thread otherwise!

  if (bRunning)
    return SSMODULE_ERROR_ALREADYRUNNING;

  if (DosRequestMutexSem(hmtxUseForeignModule, FOREIGN_MODULE_MUTEX_TIMEOUT)!=NO_ERROR)
    return SSMODULE_ERROR_INTERNALERROR;

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_StartSaving] : Set up\n");
#endif

  bPaused = FALSE;

  // Save parameters for later use!
  snprintf(achCurrentHomeDirectory, sizeof(achCurrentHomeDirectory),
           "%s", pchHomeDirectory);
  bCurrentPreviewMode = bPreviewMode;
  hwndCurrentParent = hwndParent;

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_StartSaving] : Load Config\n");
#endif

  internal_LoadConfig(pchHomeDirectory);

  // Fine, screen saver started and running!
  bRunning = TRUE;

  // Now start up a new thread which will take care of changing modules
  // while running!

#ifdef DEBUG_LOGGING
  AddLog("[StartSaving] : Firing up changer thread\n");
#endif

  iChangerThreadState = STATE_STARTUP;
  tidChanger = _beginthread(fnChangerThread,
                            0,
                            1024*1024,
                            NULL);
  DosReleaseMutexSem(hmtxUseForeignModule);

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_StartSaving] : Wait for changer thread to be initialized\n");
#endif

  // Wait for thread to initialize
  while (iChangerThreadState == STATE_STARTUP) DosSleep(32);

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_StartSaving] : Leave\n");
#endif

  if (iChangerThreadState == STATE_RUNNING)
    return SSMODULE_NOERROR;
  else
  {
    // Wait for thread to really stop
    DosWaitThread(&tidChanger, DCWW_WAIT);
    return SSMODULE_ERROR_INTERNALERROR;
  }
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
  AddLog("[SSModule_StopSaving] : Stopping changer thread.\n");
#endif

  // Stop the changer thread first!
  while (iChangerThreadState == STATE_STARTUP) DosSleep(32);
  if (iChangerThreadState!=STATE_NOTRUNNING)
  {
    bChangerThreadShutdownReq = 1;
    while (iChangerThreadState != STATE_NOTRUNNING) DosSleep(32);
    // Wait for the thread to really die!
    DosWaitThread(&tidChanger, DCWW_WAIT);
  }

  // Ok, screensaver stopped.
  bRunning = FALSE;

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_StopSaving] : All done!\n");
#endif

  return SSMODULE_NOERROR;
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_AskUserForPassword(int iPwdBuffSize, char *pchPwdBuff)
{
  int rc;

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_AskUserForPassword] : Enter\n");
#endif

  // This is called when we should ask the password from the user.
  if (!bRunning)
    return SSMODULE_ERROR_NOTRUNNING;

  if ((iPwdBuffSize<=0) || (!pchPwdBuff))
    return SSMODULE_ERROR_INVALIDPARAMETER;

  if (DosRequestMutexSem(hmtxUseForeignModule, FOREIGN_MODULE_MUTEX_TIMEOUT)!=NO_ERROR)
    return SSMODULE_ERROR_INTERNALERROR;

  if (pfnAskUserForPassword)
    rc = pfnAskUserForPassword(iPwdBuffSize, pchPwdBuff);
  else
    rc = SSMODULE_ERROR_NOTSUPPORTED;

  DosReleaseMutexSem(hmtxUseForeignModule);

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_AskUserForPassword] : Leave\n");
#endif

  return rc;
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_ShowWrongPasswordNotification(void)
{
  int rc;

  // This is called if the user has entered a wrong password, so we should
  // notify the user about this, in a way that fits the best to our screensaver.

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_ShowWrongPasswordNotification] : Enter\n");
#endif


  if (!bRunning)
    return SSMODULE_ERROR_NOTRUNNING;

  if (DosRequestMutexSem(hmtxUseForeignModule, FOREIGN_MODULE_MUTEX_TIMEOUT)!=NO_ERROR)
    return SSMODULE_ERROR_INTERNALERROR;

  if (pfnShowWrongPasswordNotification)
    rc = pfnShowWrongPasswordNotification();
  else
    rc = SSMODULE_ERROR_NOTSUPPORTED;

  DosReleaseMutexSem(hmtxUseForeignModule);

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_ShowWrongPasswordNotification] : Leave\n");
#endif

  return rc;
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_PauseSaving(void)
{
  int rc;

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_PauseSaving] : Enter\n");
#endif

  if (!bRunning)
    return SSMODULE_ERROR_NOTRUNNING;

  if (DosRequestMutexSem(hmtxUseForeignModule, FOREIGN_MODULE_MUTEX_TIMEOUT)!=NO_ERROR)
    return SSMODULE_ERROR_INTERNALERROR;

  if (pfnPauseSaving)
    rc = pfnPauseSaving();
  else
    rc = SSMODULE_ERROR_NOTSUPPORTED;

  bPaused = TRUE;

  DosReleaseMutexSem(hmtxUseForeignModule);

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_PauseSaving] : Leave\n");
#endif

  return rc;
}

SSMODULEDECLSPEC int SSMODULECALL SSModule_ResumeSaving(void)
{
  int rc;

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_ResumeSaving] : Enter\n");
#endif

  if (!bRunning)
    return SSMODULE_ERROR_NOTRUNNING;

  if (DosRequestMutexSem(hmtxUseForeignModule, FOREIGN_MODULE_MUTEX_TIMEOUT)!=NO_ERROR)
    return SSMODULE_ERROR_INTERNALERROR;

  if (pfnResumeSaving)
    rc = pfnResumeSaving();
  else
    rc = SSMODULE_ERROR_NOTSUPPORTED;

  bPaused = FALSE;

  DosReleaseMutexSem(hmtxUseForeignModule);

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_PauseSaving] : Leave\n");
#endif

  return rc;
}


SSMODULEDECLSPEC int SSMODULECALL SSModule_SetNLSText(int iNLSTextID, char *pchNLSText)
{

  if ((iNLSTextID<0) ||
      (iNLSTextID>SSMODULE_NLSTEXT_MAX))
    return SSMODULE_ERROR_INVALIDPARAMETER;

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_SetNLSText] : Enter\n");
#endif

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

#ifdef DEBUG_LOGGING
  AddLog("[SSModule_SetNLSText] : Leave\n");
#endif

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

// This is a special export of this module so it can identify itself when needed.
SSMODULEDECLSPEC int SSMODULECALL Randomizer_ID(int iDummy)
{
  return 1;
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

    // Destroy old configuration!
    if (ppchModules)
    {
      for (i=0; i<iNumOfModules; i++)
      {
        if (ppchModules[i])
        {
          free(ppchModules[i]);
          ppchModules[i] = NULL;
        }
      }
      free(ppchModules); ppchModules = NULL;
    }
    DosCloseMutexSem(hmtxUseForeignModule);
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

    // Create semaphore to serialize access to foreign module DLL
    DosCreateMutexSem(NULL, &hmtxUseForeignModule, 0, FALSE);
  }
  return 1;
}

