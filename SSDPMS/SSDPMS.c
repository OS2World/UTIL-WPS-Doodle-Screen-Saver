/*
 * Screen Saver - Lockup Desktop replacement for OS/2 and eComStation systems
 * Copyright (C) 2004-2008 Doodle
 * Copyright (C) 2020 David Yeo
 * Panorama Support based on reference code Copyright (C) 2020 David Azarewicz
 *
 * Contact: doodle@dont.spam.me.scenergy.dfmk.hu
 * Contact: dave.r.yeo@don't.spam.me.gmail.com
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

/*****************************************************************************/

typedef ULONG (APIENTRY *PFN_DPMS)(ULONG, ULONG*);

#define Pano_DPMS_on      0
#define Pano_DPMS_standby 1
#define Pano_DPMS_suspend 2
#define Pano_DPMS_off     4

/*****************************************************************************/

int        bInitialized = 0;
int        bUseDirectVGAAccess = 0;
int        UsePano = 0;
int        UseSNAP = 0;
int        iCurrentDPMSState = SSDPMS_STATE_ON;

PFN_DPMS   DpmsAdr = 0;
GA_devCtx *SNAPDeviceContext = 0;

/*****************************************************************************/

//#define DEBUG_LOGGING
#ifdef DEBUG_LOGGING
void AddLog(char *pchMsg) /*fold00*/
{
  FILE *hFile;

  hFile = fopen("c:\\ssdpms.log", "at+");
  if (hFile)
  {
    fprintf(hFile, "%s", pchMsg);
    fclose(hFile);
  }
}
#else
#define AddLog(p)
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

static void Get_PM_gdt() /*fold00*/
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
int vesa_blank(void) /*fold00*/
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
int vesa_unblank(void) /*fold00*/
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

/*****************************************************************************/

// Routine to use Panorama

int PanoDpms(ULONG ulCmd, ULONG *pulData)
{
  ULONG rc;

  if (!DpmsAdr)
  {
    AddLog("[PanoDpms] : DpmsAdr not initialized.\n");
    return SSDPMS_ERROR_INTERNALERROR;
  }

  rc = DpmsAdr(ulCmd, pulData);
  if (rc)
  {
    AddLog("[PanoDpms] : Failed to get Panorama DPMS capabilities.\n");
    return SSDPMS_ERROR_SERVICEUNAVAILABLE;
  }

  return SSDPMS_NOERROR;
}

/*****************************************************************************/


SSDPMSDECLSPEC int SSDPMSCALL SSDPMS_GetCapabilities(int *piCapabilities) /*fold00*/
{
  GA_DPMSFuncs DPMSFuncs;
  N_int16      supportedStates;
  ULONG        Pano_supportedStates;
  ULONG        rc;

  // Check parameters!
  if (!piCapabilities)
    return SSDPMS_ERROR_INVALIDPARAMETER;

  if (!bInitialized)
    return SSDPMS_ERROR_SERVICEUNAVAILABLE;

  if (bUseDirectVGAAccess)
  {
    AddLog("[SSDPMS_GetCapabilities] : Returning capabilities for direct VGA acces.\n");
    *piCapabilities = SSDPMS_STATE_ON | SSDPMS_STATE_OFF;
    return SSDPMS_NOERROR;
  }

  if (UseSNAP)
  {
    AddLog("[SSDPMS_GetCapabilities] : Calling GA_queryFunctions and DPMSdetect\n");

    // Detect if the DPMS interface is active
    DPMSFuncs.dwSize = sizeof(DPMSFuncs);
    if (!(GA_queryFunctions(SNAPDeviceContext, GA_GET_DPMSFUNCS, &DPMSFuncs)) ||
        !(DPMSFuncs.DPMSdetect(&supportedStates)) ||
        supportedStates == 0)
    {
      // DPMS services are not available!
      AddLog("[SSDPMS_GetCapabilities] : SNAP DPMS services are not available\n");

      *piCapabilities = SSDPMS_STATE_ON;
      return SSDPMS_NOERROR;
    }

    // Return supported DPMS modes
    AddLog("[SSDPMS_GetCapabilities] : Returning supported modes\n");
    *piCapabilities = (supportedStates << 1) | SSDPMS_STATE_ON;

    return SSDPMS_NOERROR;
  }

  if (UsePano)
  {
    AddLog("[SSDPMS_GetCapabilities] : Detecting Pano DPMS.\n");

    // Detect if the DPMS interface is active
    rc = (PanoDpms(0, &Pano_supportedStates));
    if (rc)
    {
      // DPMS services are not available!
      AddLog("[SSDPMS_GetCapabilities] : Panorama DPMS services are not available\n");
      *piCapabilities = SSDPMS_STATE_ON;
      return SSDPMS_NOERROR;
    }

    // Return supported DPMS modes
    AddLog("[SSDPMS_GetCapabilities] : Returning supported modes\n");
    *piCapabilities = (Pano_supportedStates << 1) | SSDPMS_STATE_ON;

    return SSDPMS_NOERROR;
  }

  return SSDPMS_ERROR_SERVICEUNAVAILABLE;
}

SSDPMSDECLSPEC int SSDPMSCALL SSDPMS_SetState(int iDPMSState) /*fold00*/
{
  int          iSupportedStates;
  GA_DPMSFuncs DPMSFuncs;
  N_int32      stateToSet;
  int          iPano_supportedStates;
  ULONG        Pano_stateToSet;

  if (!bInitialized)
    return SSDPMS_ERROR_SERVICEUNAVAILABLE;

  if (SSDPMS_GetCapabilities(&iSupportedStates)!=SSDPMS_NOERROR)
  {
    // Eeeek, error querying capabilities! State setting failed!
    AddLog("[SSDPMS_SetState] : Could not get capabilities!\n");
    return SSDPMS_ERROR_INTERNALERROR;
  }

  if ((iSupportedStates & iDPMSState)==0)
  {
    AddLog("[SSDPMS_SetState] : Requested state not supported!\n");
    return SSDPMS_ERROR_STATENOTSUPPORTED;
  }

  // Okay, it seems that the requested DPMS state is supported.
  // Let's try to set it, then!
#if 0
  if (iCurrentDPMSState == iDPMSState)
  {
    AddLog("[SSDPMS_SetState] : Shortcut exit.\n");

    // Shortcut, we're already in the requested state!
    return SSDPMS_NOERROR;
  }
#endif // 0

  if (bUseDirectVGAAccess)
  {
    BOOL  ok;

    AddLog("[SSDPMS_SetState] : Setting state using direct VGA access.\n");

    if (iDPMSState == SSDPMS_STATE_ON)
      ok = vesa_unblank();
    else
    if (iDPMSState == SSDPMS_STATE_OFF)
      ok = vesa_blank();
    else
    {
      AddLog("[SSDPMS_SetState] : Requested state not supported!\n");
      return SSDPMS_ERROR_STATENOTSUPPORTED;
    }

    if (!ok)
    {
      AddLog("[SSDPMS_SetState] : Requested state could not be set!\n");
      return SSDPMS_ERROR_STATENOTSUPPORTED;
    }

    // Save the current state.
    iCurrentDPMSState = iDPMSState;
    return SSDPMS_NOERROR;
  }

  if (UseSNAP)
  {
    AddLog("[SSDPMS_SetState] : Query DPMS functions from SNAP\n");

    // Query DPMS functions from SNAP
    DPMSFuncs.dwSize = sizeof(DPMSFuncs);
    if (!GA_queryFunctions(SNAPDeviceContext, GA_GET_DPMSFUNCS, &DPMSFuncs))
    {
      AddLog("[SSDPMS_SetState] : Error querying DPMS functions from SNAP!\n");
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

    AddLog("[SSDPMS_SetState] : Calling DPMSsetState()\n");

    // If all goes well, this should set dpms state
    // on all heads of the primary video device.
    DPMSFuncs.DPMSsetState(stateToSet);

    // Save the current state.
    AddLog("[SSDPMS_SetState] : Saving current state, and returning\n");
    iCurrentDPMSState = iDPMSState;

    return SSDPMS_NOERROR;
  }

  if (UsePano)
  {
    if (!bInitialized)
      return SSDPMS_ERROR_SERVICEUNAVAILABLE;

    if (SSDPMS_GetCapabilities(&iPano_supportedStates)!=SSDPMS_NOERROR)
    {
      // Eeeek, error querying capabilities! State setting failed!
      AddLog("[SSDPMS_SetState] : Could not get capabilities!\n");
      return SSDPMS_ERROR_INTERNALERROR;
    }

    if ((iPano_supportedStates & iDPMSState)==0)
    {
      AddLog("[SSDPMS_SetState] : Requested state not supported!\n");
      return SSDPMS_ERROR_STATENOTSUPPORTED;
    }

  // Okay, it seems that the requested DPMS state is supported.
  // Let's try to set it, then!
#if 0
    if (iCurrentDPMSState == iDPMSState)
    {
    AddLog("[SSDPMS_SetState] : Shortcut exit.\n");

      // Shortcut, we're already in the requested state!
      return SSDPMS_NOERROR;
     }
#endif // 0

    switch (iDPMSState)
    {
      case SSDPMS_STATE_OFF:
        Pano_stateToSet = Pano_DPMS_off;
        AddLog("[SSDPMS_SetState] : Pano_stateToSet = Pano_DPMS_off.\n");
        break;

      case SSDPMS_STATE_STANDBY:
        Pano_stateToSet = Pano_DPMS_standby;
        AddLog("[SSDPMS_SetState] : Pano_stateToSet = Pano_DPMS_standby.\n");
        break;

      case SSDPMS_STATE_SUSPEND:
        Pano_stateToSet = Pano_DPMS_suspend;
        AddLog("[SSDPMS_SetState] : Pano_stateToSet = Pano_DPMS_suspend.\n");
        break;

      case SSDPMS_STATE_ON:
      default:
        Pano_stateToSet = Pano_DPMS_on;
        AddLog("[SSDPMS_SetState] : Pano_stateToSet = Pano_DPMS_on.\n");
        break;
    }

    AddLog("[SSDPMS_SetState] : Calling set state.\n");

    // If all goes well, this should set dpms state on the primary video device.
    if (PanoDpms(1, &Pano_stateToSet))
      return SSDPMS_ERROR_STATENOTSUPPORTED;

    // Save the current state.
    AddLog("[SSDPMS_SetState] : Saving current state, and returning\n");
    iCurrentDPMSState = iDPMSState;

    return SSDPMS_NOERROR;
  }

  return SSDPMS_ERROR_SERVICEUNAVAILABLE;
}

SSDPMSDECLSPEC int SSDPMSCALL SSDPMS_GetState(int *piDPMSState) /*fold00*/
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


static int SNAP_Initialize() /*fold00*/
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
    AddLog("[SSDPMS_Initialize] : Error in GA_loadDriver(0, false)!\n");

    // Something went wrong. Return error!
    return 0;
  }

  // Okay, SNAP is with us! :)
  return 1;
}

static void SNAP_Uninitialize() /*fold00*/
{
  if (SNAPDeviceContext)
  {
    // Uninitialize SNAP driver
    GA_unloadDriver(SNAPDeviceContext);
    SNAPDeviceContext = NULL;
  }
}

SSDPMSDECLSPEC int SSDPMSCALL SSDPMS_Initialize(void) /*FOLD00*/
{
  HMODULE     hmod;

  if (bInitialized)
    return SSDPMS_NOERROR;

  // Query this setting from INI file!
  // Defaults to FALSE, in case this key does not exist there.
  bUseDirectVGAAccess = PrfQueryProfileInt(HINI_USERPROFILE, "SSaver",
                                           "DPMS_ForceDirectVGAAccess", 0);
  if (bUseDirectVGAAccess)
  {
    AddLog("[SSDPMS_Initialize] : forced use of direct VGA access.\n");
    bInitialized = TRUE;
    return SSDPMS_NOERROR;
  }

  // SNAP
  if (!DosQueryModuleHandle("SDDGRADD.DLL", &hmod))
  {
    // Ok, SNAP seems to be installed, let's see if we can initialize it!
    if (!SNAP_Initialize())
    {
      AddLog("[SSDPMS_Initialize] : Could not initialize SNAP!\n");
      return SSDPMS_ERROR_SERVICEUNAVAILABLE;
    }

    AddLog("[SSDPMS_Initialize] : SNAP DPMS support initialized successfully.\n");
    UseSNAP = 1;
    bInitialized = TRUE;
    return SSDPMS_NOERROR;
  }

  // Panorama
  if (!DosQueryModuleHandle("VBE2GRAD", &hmod))
  {
    // Okay, Panorama is here.
    if (DosQueryProcAddr(hmod, 2, NULL, (PFN*)&DpmsAdr))
    {
      AddLog("[SSDPMS_Initialize] : Panorama is not available.\n");
      return SSDPMS_ERROR_SERVICEUNAVAILABLE;
    }

    AddLog("[SSDPMS_Initialize] : Panorama DPMS support initialized.\n");
    UsePano = 1;
    bInitialized = TRUE;
    return SSDPMS_NOERROR;
  }

  AddLog("[SSDPMS_Initialize] : No DPMS providers are available.\n");
  return SSDPMS_ERROR_SERVICEUNAVAILABLE;
}

SSDPMSDECLSPEC int SSDPMSCALL SSDPMS_Uninitialize(void) /*fold00*/
{
  if (bInitialized)
  {
    // Uninitialize SNAP if we're using it.
    if (UseSNAP)
      SNAP_Uninitialize();

    bInitialized = FALSE;
    AddLog("[SSDPMS_Uninitialize] : Service uninitialized.\n");
    return SSDPMS_NOERROR;
  }

  return SSDPMS_ERROR_SERVICEUNAVAILABLE;
}
 /*fold00*/

//---------------------------------------------------------------------
// LibMain
//
// This gets called at DLL initialization and termination.
//---------------------------------------------------------------------
unsigned _System LibMain(unsigned hmod, unsigned termination) /*fold00*/
{
  if (termination)
  {
    // Cleanup!
    AddLog("[LibMain] : TERMINATE DLL (start)\n");

    // If the client disconnects, and leaves the monitor in saving mode,
    // we should set it back to ON state!
    if (iCurrentDPMSState!=SSDPMS_STATE_ON)
      SSDPMS_SetState(SSDPMS_STATE_ON);

    // Okay, now drop DPMS functions, in case the client forgot it!
    SSDPMS_Uninitialize();

    AddLog("[LibMain] : TERMINATE DLL (done)\n");

    return 1; // Return success!
  }

  // Startup! - nothing to do
  AddLog("[LibMain] : INITIALIZE DLL\n");

  return 1;
}

