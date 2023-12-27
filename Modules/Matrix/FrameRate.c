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

/*****************************************************************************
 * Frame regulation library                                                  *
 *                                                                           *
 * Can be used to post a semaphore in a given number of times per second,    *
 * giving ticks. Some functions are exported, so it can be used to measure   *
 * normal time too, like a stopper.                                          *
 *****************************************************************************/

#include <stdlib.h>
#include <process.h>
#define INCL_DOS
#include <os2.h>
#include "FrameRate.h"

// ----------- Frame Regulation -----------------------------

TID  FrameRegulatorThread; // Frame Regulation thread ID
ULONG ulFRPollStartTime;   // Start timestamp of actual frame
float flFRPollTime;        // Time (ms) to wait for a frame
int fFRShutdown;           // Indicator of shutdown request for FRAPI
HEV hevFRBlitNeededEvent;  // Event semaphore, posted by FrameRegulator, when it's time to tick!
ULONG ulInitTimeStamp;     // Timestamp, when the FrameRegulator has been initialized. Exported, so other threads can use it!
ULONG ulCurrTimeStamp;     // Current time, since boot of OS/2, in msecs. Exported, so other threads can use it!


//---------------------------------------------------------------------
// Initialize_ulInitTimeStamp
//
// Initializes the ulInitTimeStamp variable, or in other words, queries
// the actual timestamp, and stores it in ulInitTimeStamp.
// The ulCurrTimeStamp is updated regularly, so counting the difference
// of these two, the time elapsed can be counted easily.
//---------------------------------------------------------------------

void Initialize_ulInitTimeStamp()
{
  DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &(ulInitTimeStamp), 4 );
}

//---------------------------------------------------------------------
// FrameRegulator
//
// This is the thread function for frame regulation.
// It frequently polls the actual time, and if it's time to do a tick,
// it posts a given semaphore.
//---------------------------------------------------------------------
VOID FrameRegulator(void *pParam)
{
  ULONG ulCurrTime, ulPrevPollStartTime;
  float flNextFrameTime = 0;

  ulPrevPollStartTime = 0;
  flNextFrameTime += flFRPollTime;

  ulInitTimeStamp = ulFRPollStartTime; // Save timestamp of program initialization

  while (!fFRShutdown)
  {
    // Query the actual time of system
    DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &ulCurrTime, 4 );
    if (ulPrevPollStartTime != ulFRPollStartTime)
    { // The FPS setting has been changed on the fly!
      flNextFrameTime = ulPrevPollStartTime = ulFRPollStartTime;
    }
    // Calculate the next timestamp, when we will have to post a
    // semaphore.
    flNextFrameTime += flFRPollTime;
    // Wait for this time!
    while ((!(fFRShutdown)) &&
	   (ulCurrTime < flNextFrameTime) &&
	   (ulPrevPollStartTime == ulFRPollStartTime))
    {
      DosSleep(1);
      DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &ulCurrTime, 4 );
      ulCurrTimeStamp = ulCurrTime;
    }
    // Ok, time elapsed, post the semaphore!
    DosPostEventSem( hevFRBlitNeededEvent );
  }

  _endthread();
}


//---------------------------------------------------------------------
// SetupFrameRegulation
//
// Queries the current timestamp, and calculates, how much to wait
// between the ticks. It can be called anytime, if the FPS should
// be changed.
//---------------------------------------------------------------------
void SetupFrameRegulation(float flFPS )
{
  if (flFPS==0.0) return;
  flFRPollTime = 1000.0 / flFPS;
  DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &(ulFRPollStartTime), 4 );
}

//---------------------------------------------------------------------
// StartFrameRegulation
//
// Starts frame regulation thread
//---------------------------------------------------------------------
int StartFrameRegulation(HEV hevSem, float flFPS)
{
  fFRShutdown = 0;
  SetupFrameRegulation(flFPS);
  hevFRBlitNeededEvent = hevSem; // Save semaphore handle to post
  FrameRegulatorThread = _beginthread( FrameRegulator,
				       NULL,
				       16384,
				       NULL);

  if (FrameRegulatorThread<1) return FALSE;
  // Burst the priority of regulator
  DosSetPriority(PRTYS_THREAD, PRTYC_FOREGROUNDSERVER, PRTYD_MAXIMUM, FrameRegulatorThread);
  return TRUE;
}

//---------------------------------------------------------------------
// StopFrameRegulation
//
// Signals stop to frameregulator thread, and waits until it stops.
//---------------------------------------------------------------------
void StopFrameRegulation()
{
  if (FrameRegulatorThread>=1)
  {
    fFRShutdown = 1;
    DosWaitThread(&(FrameRegulatorThread), DCWW_WAIT);
    FrameRegulatorThread = 0;
  }
}


//---------------------------------------------------------------------
// Initialize_FrameRegulation
//
// Initializes frame regulation global variables
//---------------------------------------------------------------------
int Initialize_FrameRegulation()
{
  FrameRegulatorThread = 0;
  return 1;
}

//---------------------------------------------------------------------
// Uninitialize_FrameRegulation
//
// Signals stop to frameregulator thread, and waits until it stops.
//---------------------------------------------------------------------
void Uninitialize_FrameRegulation()
{
  StopFrameRegulation();
}
