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

#ifndef __MODULESPEC_H__
#define __MODULESPEC_H__

#include "cairo.h"
#include "cairo-os2.h"

extern char *pchModuleName;
extern char *pchModuleDesc;
extern int   iModuleVersionMajor;
extern int   iModuleVersionMinor;
extern char *pchSaverWindowClassName;

extern int bPauseDrawing;
extern int bShutdownDrawing;

void CairoDrawLoop(HWND             hwndClientWindow,
                   int              iWidth,
                   int              iHeight,
                   cairo_surface_t *pCairoSurface,
                   cairo_t         *pCairoHandle);

#endif
