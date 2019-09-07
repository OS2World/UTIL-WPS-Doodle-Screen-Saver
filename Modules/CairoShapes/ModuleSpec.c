/*
 * Screen Saver - Lockup Desktop replacement for OS/2, ArcaOS
 * and eComStation systems
 * Copyright (C) 2004-2008 2016,2017 Doodle contributor Dave Yeo
 *
 * Contact: doodle@dont.spam.me.scenergy.dfmk.hu dave.r.yeo@gmail.com
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
#include <float.h>
#include <math.h>
#include <time.h>
#define INCL_DOS
#define INCL_WIN
#include <os2.h>

#include "ModuleSpec.h"

#ifndef M_PI
#define M_PI 3.14159
#endif

/* ------------------------------------------------------------------------*/

/*
 * The next some lines has to be changed according to the module
 * you want to create!
 */

/* Short module name: */
char *pchModuleName = "Cairo Shapes";

/* Module description string: */
char *pchModuleDesc =
  "Shows different colorful shapes and figures.\n"
  "Using the Cairo library.\n";

/* Module version number (Major.Minor format): */
int   iModuleVersionMajor = 3;
int   iModuleVersionMinor = 10;

/* Internal name generated from module name, to have
 * private window identifier for this kind of windows: */
char *pchSaverWindowClassName = "CairoShapesWindowClass";

/* ------------------------------------------------------------------------*/

/* Some global variables, modified from outside: */
int bPauseDrawing = 0;
int bShutdownDrawing = 0;

/* And the real drawing loop: */
void CairoDrawLoop(HWND             hwndClientWindow,
                   int              iWidth,
                   int              iHeight,
                   cairo_surface_t *pCairoSurface,
                   cairo_t         *pCairoHandle)
{
  int iUniSize;
  double dXAspect, dYAspect;
  int iTemp;
  double dRandMax;
  char *pchText;
  cairo_text_extents_t CairoTextExtents;

  // Initialize random number generator
  srand(clock());

  // Set dRandMax value
  dRandMax = RAND_MAX;

  // Clear background with black
  cairo_set_source_rgb (pCairoHandle, 0, 0 ,0);
  cairo_rectangle (pCairoHandle,
                   0, 0,
                   iWidth, iHeight);
  cairo_fill(pCairoHandle);

  // Calculate screen aspect ratio
  if (iWidth<iHeight)
  {
    iUniSize = iWidth;
    dYAspect = iHeight;
    dYAspect /= iWidth;
    dXAspect = 1;
  }
  else
  {
    iUniSize = iHeight;
    dXAspect = iWidth;
    dXAspect /= iHeight;
    dYAspect = 1;
  }

  // Do the main drawing loop as long as needed!
  while (!bShutdownDrawing)
  {
    if (bPauseDrawing)
    {
      // Do not draw anything if we're paused!
      DosSleep(250);
    }
    else
    {
      // Otherwise draw something!

      // Save cairo canvas state
      cairo_save(pCairoHandle);

      // Scale canvas so we'll have a
      // normalized coordinate system of (0;0) -> (1;1)
      cairo_scale(pCairoHandle, iUniSize, iUniSize);

      // Set a random operator
      do {
      iTemp = ((int)rand())*13/RAND_MAX;
      } while (
               (iTemp==CAIRO_OPERATOR_CLEAR) ||
               (iTemp==CAIRO_OPERATOR_OVER) ||
               (iTemp==CAIRO_OPERATOR_IN) ||
               (iTemp==CAIRO_OPERATOR_OUT) ||
               (iTemp==CAIRO_OPERATOR_ATOP) ||
               (iTemp==CAIRO_OPERATOR_DEST) ||
               (iTemp==CAIRO_OPERATOR_DEST_OVER) ||
               (iTemp==CAIRO_OPERATOR_DEST_IN) ||
               (iTemp==CAIRO_OPERATOR_DEST_OUT) ||
               (iTemp==CAIRO_OPERATOR_DEST_ATOP)
              );

      cairo_set_operator(pCairoHandle, iTemp);

      // Set random color and transparency
      cairo_set_source_rgba(pCairoHandle,
                            rand()/dRandMax,
                            rand()/dRandMax,
                            rand()/dRandMax,
                            0.33+rand()/dRandMax/3);

      // Set random line width
      cairo_set_line_width(pCairoHandle, rand()/dRandMax/5);

      // Set random line cap
      cairo_set_line_cap(pCairoHandle, ((int)rand())*3/RAND_MAX);

      // Set random line join
      cairo_set_line_join(pCairoHandle, ((int)rand())*3/RAND_MAX);

      // Draw a random thing
      iTemp = ((int)rand())*5/RAND_MAX;
      switch (iTemp)
      {
        case 0:
          cairo_move_to(pCairoHandle,
                        rand()/dRandMax*dXAspect,
                        rand()/dRandMax*dYAspect);
          cairo_line_to(pCairoHandle,
                        rand()/dRandMax*dXAspect,
                        rand()/dRandMax*dYAspect);
          break;
        case 1:
          cairo_move_to(pCairoHandle,
                        rand()/dRandMax*dXAspect,
                        rand()/dRandMax*dYAspect);
          cairo_curve_to(pCairoHandle,
                         rand()/dRandMax*dXAspect,
                         rand()/dRandMax*dYAspect,
                         rand()/dRandMax*dXAspect,
                         rand()/dRandMax*dYAspect,
                         rand()/dRandMax*dXAspect,
                         rand()/dRandMax*dYAspect);
          break;
        case 2:
          cairo_arc(pCairoHandle,
                    rand()/dRandMax*dXAspect,
                    rand()/dRandMax*dYAspect,
                    rand()/dRandMax/20, // Radius
                    0, 2*M_PI); // from,to
          break;
        case 3:
          cairo_move_to(pCairoHandle,
                        rand()/dRandMax*dXAspect,
                        rand()/dRandMax*dYAspect);
          cairo_rectangle(pCairoHandle,
                          rand()/dRandMax*dXAspect,
                          rand()/dRandMax*dYAspect,
                          rand()/dRandMax*dXAspect/100,
                          rand()/dRandMax*dYAspect/100);
          break;
        case 4:
          cairo_select_font_face(pCairoHandle, "Sans",
                                 ((int)rand())*2/RAND_MAX,
                                 ((int)rand())*2/RAND_MAX);
          cairo_set_font_size(pCairoHandle,
                              rand()/dRandMax);

          // Get how bit the text will be!
          if (rand()>RAND_MAX/2)
            pchText = "OS/2";
          else
            if (rand()>RAND_MAX/2)
              pchText = "ArcaOS";
            else
              pchText = "eComStation";
          cairo_text_extents(pCairoHandle,
                             pchText,
                             &CairoTextExtents);

          // Rotate by random value
          cairo_rotate(pCairoHandle, rand()/dRandMax*2*M_PI);
          // Print the text, centered!
          cairo_move_to(pCairoHandle,
                        rand()/dRandMax*dXAspect - CairoTextExtents.width/2,
                        rand()/dRandMax*dYAspect + CairoTextExtents.height/2);
          cairo_show_text(pCairoHandle, pchText);
      }

        cairo_os2_surface_paint_window(pCairoSurface,
                                       NULL,
                                       NULL,
                                       1);

      cairo_stroke (pCairoHandle);

      // Restore canvas state to original one
      cairo_restore(pCairoHandle);
    }

    DosSleep(50); // Wait some
  }
  // Got shutdown event, so stopped drawing. That's all.
}

/* ------------------------------------------------------------------------*/
/* End of file */
