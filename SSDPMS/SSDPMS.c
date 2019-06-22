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
#include <conio.h>

#define INCL_DOS
#define INCL_WIN
#define INCL_WINSHELLDATA
#define INCL_ERRORS
#include <os2.h>

#include "SSDPMS.h"
#include "pmapi.h"
#include "pm_help.h"
#include "snap/graphics.h"

int        bInitialized;
int        bUseDirectVGAAccess;

int        iCurrentDPMSState = SSDPMS_STATE_ON;
GA_devCtx *SNAPDeviceContext = NULL;

//#define DEBUG_LOGGING
#ifdef DEBUG_LOGGING
void AddLog(char *pchMsg)
{
  FILE *hFile;

  hFile = fopen("c:\\ssdpms.log", "at+");
  if (hFile)
  {
    fprintf(hFile, "%s", pchMsg);
    fclose(hFile);
  }
}
#endif

/**************************************************************************
 * The following two functions and their company are derived from the
 * dssblack.zip package of Lesha Bogdanow, which in turn was derived from
 * the Blackout screen saver by Staffan Ulfberg, which was derived from
 * the console drivers of Linux...
 */

/* Structure holding original VGA register settings */
typedef struct VGARegisters_s
{
  unsigned char  SeqCtrlIndex;          /* Sequencer Index reg.   */
  unsigned char  CrtCtrlIndex;          /* CRT-Contr. Index reg.  */
  unsigned char  CrtMiscIO;             /* Miscellaneous register */
  unsigned char  HorizontalTotal;       /* CRT-Controller:00h */
  unsigned char  HorizDisplayEnd;       /* CRT-Controller:01h */
  unsigned char  StartHorizRetrace;     /* CRT-Controller:04h */
  unsigned char  EndHorizRetrace;       /* CRT-Controller:05h */
  unsigned char  Overflow;              /* CRT-Controller:07h */
  unsigned char  StartVertRetrace;      /* CRT-Controller:10h */
  unsigned char  EndVertRetrace;        /* CRT-Controller:11h */
  unsigned char  ModeControl;           /* CRT-Controller:17h */
  unsigned char  ClockingMode;          /* Seq-Controller:01h */
} VGARegisters_t;

/* The place to store the original VGA registers */
static VGARegisters_t SavedVGARegisters;

#define video_port_reg  (0x3d4)         /* CRT Controller Index - color emulation */
#define video_port_val  (0x3d5)         /* CRT Controller Data Register - color emulation */
#define seq_port_reg    (0x3c4)         /* Sequencer register select port */
#define seq_port_val    (0x3c5)         /* Sequencer register value port  */
#define video_misc_rd   (0x3cc)         /* Video misc. read port          */
#define video_misc_wr   (0x3c2)         /* Video misc. write port         */

#define port_first      (0x3c2)
#define port_last       (0x3d5)

// This comes from SNAP's pm.lib
extern ushort _PM_gdt;

static void Get_PM_gdt()
{
  static ulong    inLen;          /* Must not cross 64Kb boundary!    */
  static ulong    outLen;         /* Must not cross 64Kb boundary!    */
  static ulong    parms[3];       /* Must not cross 64Kb boundary!    */
  static ulong    result[4];      /* Must not cross 64Kb boundary!    */
  HFILE           hSDDHelp;
  ULONG           rc;

  rc = DosOpen(PMHELP_NAME,&hSDDHelp,&result[0],0,0,
               FILE_OPEN, OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE,
               NULL);
  if (rc==NO_ERROR)
  {
    rc = DosDevIOCtl(hSDDHelp,PMHELP_IOCTL,PMHELP_GETGDT32,
                &parms, inLen = sizeof(parms), &inLen,
                &result, outLen = sizeof(result), &outLen);
    DosClose(hSDDHelp);
    if (rc == NO_ERROR)
      _PM_gdt = result[0];
  }
}


/* routine to blank a vesa screen */
int vesa_blank(void)
{
  int iOldIOPL;

  if (!_PM_gdt)
    Get_PM_gdt();

  if (!_PM_gdt)
    return 0;

  iOldIOPL = PM_setIOPL(3);

  /* save original values of VGA controller registers */

  SavedVGARegisters.SeqCtrlIndex = inp(seq_port_reg);
  SavedVGARegisters.CrtCtrlIndex = inp(video_port_reg);
  SavedVGARegisters.CrtMiscIO = inp(video_misc_rd);


  outp(video_port_reg, 0x00);          /* HorizontalTotal */
  SavedVGARegisters.HorizontalTotal = inp(video_port_val);
  outp(video_port_reg, 0x01);          /* HorizDisplayEnd */
  SavedVGARegisters.HorizDisplayEnd = inp(video_port_val);
  outp(video_port_reg, 0x04);          /* StartHorizRetrace */
  SavedVGARegisters.StartHorizRetrace = inp(video_port_val);
  outp(video_port_reg, 0x05);          /* EndHorizRetrace */
  SavedVGARegisters.EndHorizRetrace = inp(video_port_val);


  outp(video_port_reg, 0x07);                  /* Overflow */
  SavedVGARegisters.Overflow = inp(video_port_val);
  outp(video_port_reg, 0x10);                  /* StartVertRetrace */
  SavedVGARegisters.StartVertRetrace = inp(video_port_val);
  outp(video_port_reg, 0x11);                  /* EndVertRetrace */
  SavedVGARegisters.EndVertRetrace = inp(video_port_val);
  outp(video_port_reg, 0x17);                  /* ModeControl */
  SavedVGARegisters.ModeControl = inp(video_port_val);
  outp(seq_port_reg, 0x01);                    /* ClockingMode */
  SavedVGARegisters.ClockingMode = inp(seq_port_val);
        
  /* assure that video is enabled */
  /* "0x20" is VIDEO_ENABLE_bit in register 01 of sequencer */
  outp(seq_port_reg, 0x01);
  outp(seq_port_val, SavedVGARegisters.ClockingMode | 0x20);

  /* test for vertical retrace in process.... */
  if ((SavedVGARegisters.CrtMiscIO & 0x80) == 0x80)
    outp(video_misc_wr, SavedVGARegisters.CrtMiscIO & 0xef);

  /*
   * Set <End of vertical retrace> to minimum (0) and
   * <Start of vertical Retrace> to maximum (incl. overflow)
   * Result: turn off vertical sync (VSync) pulse.
   */

  outp(video_port_reg, 0x10);          /* StartVertRetrace */
  outp(video_port_val, 0xff);          /* maximum value */
  outp(video_port_reg, 0x11);          /* EndVertRetrace */
  outp(video_port_val, 0x40);          /* minimum (bits 0..3)  */
  outp(video_port_reg, 0x07);          /* Overflow */
  /* bits 9,10 of  */
  /* vert. retrace */
  outp(video_port_val, SavedVGARegisters.Overflow | 0x84);

  /*
   * Set <End of horizontal retrace> to minimum (0) and
   *  <Start of horizontal Retrace> to maximum
   * Result: turn off horizontal sync (HSync) pulse.
   */
  outp(video_port_reg, 0x04);  /* StartHorizRetrace */
  outp(video_port_val, 0xff);  /* maximum */
  outp(video_port_reg, 0x05);  /* EndHorizRetrace */
  outp(video_port_val, 0x00);  /* minimum (0) */


  /* restore both index registers */
  outp(seq_port_reg, SavedVGARegisters.SeqCtrlIndex);
  outp(video_port_reg, SavedVGARegisters.CrtCtrlIndex);

  PM_setIOPL(iOldIOPL);
  return 1;
}

/* routine to unblank a vesa screen */
int vesa_unblank(void)
{
  int iOldIOPL;

  if (!_PM_gdt)
    Get_PM_gdt();

  if (!_PM_gdt)
    return 0;

  iOldIOPL = PM_setIOPL(3);

  /* restore original values of VGA controller registers */

  outp(video_misc_wr, SavedVGARegisters.CrtMiscIO);


  outp(video_port_reg, 0x00);  /* HorizontalTotal */
  outp(video_port_val, SavedVGARegisters.HorizontalTotal);
  outp(video_port_reg, 0x01);  /* HorizDisplayEnd */
  outp(video_port_val, SavedVGARegisters.HorizDisplayEnd);
  outp(video_port_reg, 0x04);  /* StartHorizRetrace */
  outp(video_port_val, SavedVGARegisters.StartHorizRetrace);
  outp(video_port_reg, 0x05);  /* EndHorizRetrace */
  outp(video_port_val, SavedVGARegisters.EndHorizRetrace);


  outp(video_port_reg, 0x07);          /* Overflow */
  outp(video_port_val, SavedVGARegisters.Overflow);
  outp(video_port_reg, 0x10);          /* StartVertRetrace */
  outp(video_port_val, SavedVGARegisters.StartVertRetrace);
  outp(video_port_reg, 0x11);          /* EndVertRetrace */
  outp(video_port_val, SavedVGARegisters.EndVertRetrace);
  outp(video_port_reg, 0x17);          /* ModeControl */
  outp(video_port_val, SavedVGARegisters.ModeControl);
  outp(seq_port_reg, 0x01);            /* ClockingMode */
  outp(seq_port_val, SavedVGARegisters.ClockingMode);

  /* restore index/control registers */
  outp(seq_port_reg, SavedVGARegisters.SeqCtrlIndex);
  outp(video_port_reg, SavedVGARegisters.CrtCtrlIndex);

  PM_setIOPL(iOldIOPL);
  return 1;
}

/**************************************************************************/


SSDPMSDECLSPEC int SSDPMSCALL SSDPMS_GetCapabilities(int *piCapabilities)
{
  GA_DPMSFuncs DPMSFuncs;
  N_int16      supportedStates;

  // Check parameters!
  if (!piCapabilities)
    return SSDPMS_ERROR_INVALIDPARAMETER;

  if (!bInitialized)
    return SSDPMS_ERROR_SERVICEUNAVAILABLE;

  if (bUseDirectVGAAccess)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSDPMS_GetCapabilities] : Returning capabilities for direct VGA acces.\n");
#endif
    *piCapabilities = SSDPMS_STATE_ON | SSDPMS_STATE_OFF;
    return SSDPMS_NOERROR;
  }

#ifdef DEBUG_LOGGING
  AddLog("[SSDPMS_GetCapabilities] : Calling GA_queryFunctions and DPMSdetect\n");
#endif

  // Detect if the DPMS interface is active
  DPMSFuncs.dwSize = sizeof(DPMSFuncs);
  if ((!(GA_queryFunctions(SNAPDeviceContext,
                         GA_GET_DPMSFUNCS,
                         &DPMSFuncs))) ||
      (!(DPMSFuncs.DPMSdetect(&supportedStates))) ||
      (supportedStates == 0))
  {
    // DPMS services are not available!
#ifdef DEBUG_LOGGING
    AddLog("[SSDPMS_GetCapabilities] : DPMS services are not available (SNAP)\n");
#endif

    *piCapabilities = SSDPMS_STATE_ON;
    return SSDPMS_NOERROR;
  }

  // Return supported DPMS modes

#ifdef DEBUG_LOGGING
  AddLog("[SSDPMS_GetCapabilities] : Returning supported modes\n");
#endif

  *piCapabilities = SSDPMS_STATE_ON;

  if (supportedStates & DPMS_standby)
    *piCapabilities |= SSDPMS_STATE_STANDBY;

  if (supportedStates & DPMS_suspend)
    *piCapabilities |= SSDPMS_STATE_SUSPEND;

  if (supportedStates & DPMS_off)
    *piCapabilities |= SSDPMS_STATE_OFF;

  return SSDPMS_NOERROR;
}

SSDPMSDECLSPEC int SSDPMSCALL SSDPMS_SetState(int iDPMSState)
{
  int iSupportedStates;
  GA_DPMSFuncs DPMSFuncs;
  N_int32      stateToSet;

  if (!bInitialized)
    return SSDPMS_ERROR_SERVICEUNAVAILABLE;

  if (SSDPMS_GetCapabilities(&iSupportedStates)!=SSDPMS_NOERROR)
  {
    // Eeeek, error querying capabilities!
    // State setting failed!
#ifdef DEBUG_LOGGING
    AddLog("[SSDPMS_SetState] : Could not get capabilities!\n");
#endif
    return SSDPMS_ERROR_INTERNALERROR;
  }

  if ((iSupportedStates & iDPMSState)==0)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSDPMS_SetState] : Requested state not supported!\n");
#endif
    return SSDPMS_ERROR_STATENOTSUPPORTED;
  }

  // Okay, it seems that the requested DPMS state is supported.
  // Let's try to set it, then!

  if (iCurrentDPMSState == iDPMSState)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSDPMS_SetState] : Shortcut exit.\n");
#endif

    // Shortcut, we're already in the requested state!
    return SSDPMS_NOERROR;
  }

  if (bUseDirectVGAAccess)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSDPMS_SetState] : Setting state using direct VGA access.\n");
#endif
    if (iDPMSState == SSDPMS_STATE_ON)
    {
      if (!vesa_unblank())
      {
#ifdef DEBUG_LOGGING
        AddLog("[SSDPMS_SetState] : Requested state could not be set!\n");
#endif
        return SSDPMS_ERROR_STATENOTSUPPORTED;
      }

      // Save the current state.
      iCurrentDPMSState = iDPMSState;
      return SSDPMS_NOERROR;
    } else
    if (iDPMSState == SSDPMS_STATE_OFF)
    {
      if (!vesa_blank())
      {
#ifdef DEBUG_LOGGING
        AddLog("[SSDPMS_SetState] : Requested state could not be set!\n");
#endif
        return SSDPMS_ERROR_STATENOTSUPPORTED;
      }

      // Save the current state.
      iCurrentDPMSState = iDPMSState;
      return SSDPMS_NOERROR;
    } else
    {
#ifdef DEBUG_LOGGING
      AddLog("[SSDPMS_SetState] : Requested state not supported!\n");
#endif
      return SSDPMS_ERROR_STATENOTSUPPORTED;
    }
  }

#ifdef DEBUG_LOGGING
  AddLog("[SSDPMS_SetState] : Query DPMS functions from SNAP\n");
#endif

  // Query DPMS functions from SNAP
  DPMSFuncs.dwSize = sizeof(DPMSFuncs);
  if (!GA_queryFunctions(SNAPDeviceContext,
                         GA_GET_DPMSFUNCS,
                         &DPMSFuncs))
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSDPMS_SetState] : Error querying DPMS functions from SNAP!\n");
#endif
    return SSDPMS_ERROR_INTERNALERROR;
  }

  switch (iDPMSState)
  {
    case SSDPMS_STATE_OFF:
      stateToSet = DPMS_off;
      break;
    case SSDPMS_STATE_STANDBY:
      stateToSet = DPMS_standby;
      break;
    case SSDPMS_STATE_SUSPEND:
      stateToSet = DPMS_suspend;
      break;
    case SSDPMS_STATE_ON:
    default:
      stateToSet = DPMS_on;
      break;
  }

#ifdef DEBUG_LOGGING
  AddLog("[SSDPMS_SetState] : Calling DPMSsetState()\n");
#endif

  // If all goes well, this should set dpms state on all heads of the
  // primary video device.
  DPMSFuncs.DPMSsetState(stateToSet);

#ifdef DEBUG_LOGGING
  AddLog("[SSDPMS_SetState] : Saving current state, and returning\n");
#endif

  // Save the current state.
  iCurrentDPMSState = iDPMSState;

  return SSDPMS_NOERROR;
}

SSDPMSDECLSPEC int SSDPMSCALL SSDPMS_GetState(int *piDPMSState)
{
  // Check parameters!
  if (!piDPMSState)
    return SSDPMS_ERROR_INVALIDPARAMETER;

  if (!bInitialized)
    return SSDPMS_ERROR_SERVICEUNAVAILABLE;

  // Return current DPMS state
  *piDPMSState = iCurrentDPMSState;

  return SSDPMS_NOERROR;
}


static int SNAP_Initialize()
{
  // Load SNAP driver for primary display
  // Note that we do not handle the case, when there are
  // multiple video cards in the system.
  // If that should also be handled, we should load a driver for
  // all the cards, and call DPMSsetState() for all the drivers
  // in SSDPMS_SetState().

  if ((SNAPDeviceContext = GA_loadDriver(0, false))==NULL)
  {
    // Ooops, could not load SNAP driver for primary display.
#ifdef DEBUG_LOGGING
    AddLog("[SNAP_Initialize] : Error in GA_loadDriver(0, false)!\n");
#endif
    // Something went wrong. Return error!
    return 0;
  }

  // Okay, SNAP is with us! :)
  return 1;
}

static void SNAP_Uninitialize()
{
  if (SNAPDeviceContext)
  {
    // Uninitialize SNAP driver
    GA_unloadDriver(SNAPDeviceContext);
    SNAPDeviceContext = NULL;
  }
}

SSDPMSDECLSPEC int SSDPMSCALL SSDPMS_Initialize(void)
{
  HMODULE     hModSDDGRADD;
  int         bForceDirectVGAAccess;

  if (bInitialized) return SSDPMS_NOERROR;

  // Initialize some variables first
  iCurrentDPMSState = SSDPMS_STATE_ON;
  bUseDirectVGAAccess = FALSE;

  // Query this setting from INI file!
  bForceDirectVGAAccess = PrfQueryProfileInt(HINI_PROFILE,
                                             "SSaver",
                                             "DPMS_ForceDirectVGAAccess",
                                             FALSE); // Defaults to FALSE, in case this key does not exist there.

  // First of all, check if SNAP is present!
  if (!bForceDirectVGAAccess)
  {
    if (DosQueryModuleHandle("SDDGRADD.DLL", &hModSDDGRADD) == NO_ERROR)
    {
#ifdef DEBUG_LOGGING
      AddLog("[SSDPMS_Initialize] : SNAP is installed, trying to initialize it.\n");
#endif

      // Ok, SNAP seems to be installed, let's see if we can initialize it!
      if (!SNAP_Initialize())
      {
        // Could not initialize SNAP!
        // It might be that the user doesn't have SNAP drivers, or
        // something else happened...
#ifdef DEBUG_LOGGING
        AddLog("[SSDPMS_Initialize] : Could not initialize SNAP!\n");
#endif
      } else
      {
#ifdef DEBUG_LOGGING
        AddLog("[SSDPMS_Initialize] : SNAP DPMS support initialized successfully.\n");
#endif
        bInitialized = TRUE;
      }
    } else
    {
#ifdef DEBUG_LOGGING
      AddLog("[SSDPMS_Initialize] : SNAP is not installed.\n");
#endif
    }
  }

  if (!bInitialized)
  {
    // Could not initialize SNAP, so fall back to direct VGA access!
#ifdef DEBUG_LOGGING
    AddLog("[SSDPMS_Initialize] : Will use direct VGA access.\n");
#endif

    bUseDirectVGAAccess = TRUE;
    bInitialized = TRUE;
  }

  if (bInitialized)
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSDPMS_Initialize] : Service initialized.\n");
#endif

    return SSDPMS_NOERROR;
  } else
  {
#ifdef DEBUG_LOGGING
    AddLog("[SSDPMS_Initialize] : Service could not be initialized.\n");
#endif

    return SSDPMS_ERROR_SERVICEUNAVAILABLE;
  }
}

SSDPMSDECLSPEC int SSDPMSCALL SSDPMS_Uninitialize(void)
{
  if (bInitialized)
  {
    // Uninitialize SNAP if we're using it.
    if (!bUseDirectVGAAccess)
    {
      SNAP_Uninitialize();
    }
    bInitialized = FALSE;

#ifdef DEBUG_LOGGING
    AddLog("[SSDPMS_Uninitialize] : Service uninitialized.\n");
#endif
    return SSDPMS_NOERROR;
  } else
    return SSDPMS_ERROR_SERVICEUNAVAILABLE;
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
    AddLog("[LibMain] : TERMINATE DLL (start)\n");
#endif


    // If the client disconnects, and leaves the monitor in saving mode,
    // we should set it back to ON state!
    if (iCurrentDPMSState!=SSDPMS_STATE_ON)
      SSDPMS_SetState(SSDPMS_STATE_ON);

    // Okay, now drop SNAP functions, in case the client forgot it!
    SSDPMS_Uninitialize();

#ifdef DEBUG_LOGGING
    AddLog("[LibMain] : TERMINATE DLL (done)\n");
#endif

    return 1; // Return success!
  } else
  {
    // Startup!

#ifdef DEBUG_LOGGING
    AddLog("[LibMain] : INITIALIZE DLL (start)\n");
#endif

    // Now initialize local variables
    iCurrentDPMSState = SSDPMS_STATE_ON;
    SNAPDeviceContext = NULL;
    bUseDirectVGAAccess = FALSE;
    bInitialized = FALSE;

#ifdef DEBUG_LOGGING
    AddLog("[LibMain] : INITIALIZE DLL (done)\n");
#endif

    // All done, we're completely initialized!
    return 1;
  }
}

