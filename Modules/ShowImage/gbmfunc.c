#define INCL_DOS
#define INCL_WIN
#define INCL_ERRORS
#define INCL_GPIPRIMITIVES
#define INCL_GPI
#define INCL_GPIBITMAPS
#define INCL_DEV
#include <os2.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "gbm.h"

/* GBM _System prototypes for dynamic gbm.dll access */
static GBM_ERR      (_System * fn_Gbm_init)           (void);
static GBM_ERR      (_System * fn_Gbm_deinit)         (void);
static int          (_System * fn_Gbm_io_open)        (const char *fn, int mode);
static void         (_System * fn_Gbm_io_close)       (int fd);
static GBM_ERR      (_System * fn_Gbm_guess_filetype) (const char *fn, int *ft);
static GBM_ERR      (_System * fn_Gbm_read_header)    (const char *fn, int fd, int ft, GBM *gbm, const char *opt);
static GBM_ERR      (_System * fn_Gbm_read_palette)   (int fd, int ft, GBM *gbm, GBMRGB *gbmrgb);
static GBM_ERR      (_System * fn_Gbm_read_data)      (int fd, int ft, GBM *gbm, byte *data);
static const char * (_System * fn_Gbm_err)            (GBM_ERR rc);

/* ---------------------- */

/* GBM loader and function resolver */
static HMODULE moduleHandle = NULLHANDLE;

static BOOL init(void)
{
    // load GBM.DLL
    UCHAR  loadError[201] = "";
    APIRET rc             = 0;

    /* load the module */
    rc = DosLoadModule((PSZ)loadError,
                       sizeof(loadError)-1,
                       (PSZ)"GBM",
                       &moduleHandle);
    if (rc == 0)
    {
        // as _System exported functions
        rc |= DosQueryProcAddr(moduleHandle, 0L, "Gbm_init"          , (PFN *) &fn_Gbm_init);
        rc |= DosQueryProcAddr(moduleHandle, 0L, "Gbm_deinit"        , (PFN *) &fn_Gbm_deinit);
        rc |= DosQueryProcAddr(moduleHandle, 0L, "Gbm_io_open"       , (PFN *) &fn_Gbm_io_open);
        rc |= DosQueryProcAddr(moduleHandle, 0L, "Gbm_io_close"      , (PFN *) &fn_Gbm_io_close);
        rc |= DosQueryProcAddr(moduleHandle, 0L, "Gbm_guess_filetype", (PFN *) &fn_Gbm_guess_filetype);
        rc |= DosQueryProcAddr(moduleHandle, 0L, "Gbm_read_header"   , (PFN *) &fn_Gbm_read_header);
        rc |= DosQueryProcAddr(moduleHandle, 0L, "Gbm_read_palette"  , (PFN *) &fn_Gbm_read_palette);
        rc |= DosQueryProcAddr(moduleHandle, 0L, "Gbm_read_data"     , (PFN *) &fn_Gbm_read_data);
        rc |= DosQueryProcAddr(moduleHandle, 0L, "Gbm_err"           , (PFN *) &fn_Gbm_err);

        if (rc)
        {
            DosFreeModule(moduleHandle);
            moduleHandle = NULLHANDLE;
            return FALSE;
        }

        fn_Gbm_init();
        return TRUE;
    }
    moduleHandle = NULLHANDLE;
    return FALSE;
}

static void deinit(void)
{
    if (moduleHandle != NULLHANDLE)
    {
        fn_Gbm_deinit();
        DosFreeModule(moduleHandle);
        moduleHandle = NULLHANDLE;
    }
}

/* ---------------------- */

BOOL IsGbmAvailable(void)
{
	if (init())
	{
		deinit();
		return TRUE;
	}
	return FALSE;
}

/* ---------------------- */

BOOL LoadBitmap(HWND hwnd, const CHAR *szFn, const CHAR *szOpt, GBM *gbm, GBMRGB *gbmrgb, BYTE **ppbData) {
	GBM_ERR rc;
	int fd, ft;
	ULONG cb;
	USHORT usStride;

	if (! init())
	{
		return FALSE;
	}

	if ( fn_Gbm_guess_filetype(szFn, &ft) != GBM_ERR_OK )
		{
		#ifdef DEBUG_LOGGING
		AddLog("===GBMFUNC->LoadBitmap=== Can't deduce bitmap format from file extension: %s\n", szFn);
		#endif
		deinit();
		return ( FALSE );
		}

	if ( (fd = fn_Gbm_io_open(szFn, GBM_O_RDONLY)) == -1 )
		{
		#ifdef DEBUG_LOGGING
		AddLog("===GBMFUNC->LoadBitmap=== Can't open file: %s\n", szFn);
		#endif
		deinit();
		return ( FALSE );
		}

	if ( (rc = fn_Gbm_read_header(szFn, fd, ft, gbm, szOpt)) != GBM_ERR_OK )
		{
		fn_Gbm_io_close(fd);
		#ifdef DEBUG_LOGGING
		AddLog("===GBMFUNC->LoadBitmap=== Can't read file header of %s: %s\n", szFn, fn_Gbm_err(rc));
		#endif
		deinit();
		return ( FALSE );
		}

	/* check for color depth supported */
	switch ( gbm->bpp )
		{
		case 24:
		case 8:
		case 4:
		case 1:
			break;
		default:
 		  {
		     fn_Gbm_io_close(fd);
		     #ifdef DEBUG_LOGGING
		     AddLog("===GBMFUNC->LoadBitmap=== Color depth %d is not supported\n", gbm->bpp);
		     #endif
		     deinit();
		     return ( FALSE );
		  }
		}

	if ( (rc = fn_Gbm_read_palette(fd, ft, gbm, gbmrgb)) != GBM_ERR_OK )
		{
		fn_Gbm_io_close(fd);
		#ifdef DEBUG_LOGGING
		AddLog("===GBMFUNC->LoadBitmap=== Can't read file palette of %s: %s\n", szFn, fn_Gbm_err(rc));
		#endif
		deinit();
		return ( FALSE );
		}

	usStride = ((gbm -> w * gbm -> bpp + 31)/32) * 4;
	cb = gbm -> h * usStride;
	if ( (*ppbData = (BYTE *)malloc((int) cb)) == NULL )
		{
		fn_Gbm_io_close(fd);
		#ifdef DEBUG_LOGGING
		AddLog("===GBMFUNC->LoadBitmap=== Out of memory requesting %ld bytes\n", cb);
		#endif
		deinit();
		return ( FALSE );
		}

	if ( (rc = fn_Gbm_read_data(fd, ft, gbm, *ppbData)) != GBM_ERR_OK )
		{
		free(*ppbData);
		fn_Gbm_io_close(fd);
		#ifdef DEBUG_LOGGING
		AddLog("===GBMFUNC->LoadBitmap=== Can't read file data of %s: %s\n", szFn, fn_Gbm_err(rc));
		#endif
		deinit();
		return ( FALSE );
		}

	fn_Gbm_io_close(fd);
	deinit();

	return ( TRUE );
}

/* ---------------------- */

int MakeBitmap(HWND hwnd, const GBM *gbm, const GBMRGB *gbmrgb, const BYTE *pbData, HBITMAP *phbm) {
	HAB hab = WinQueryAnchorBlock(hwnd);
	USHORT cRGB, usCol;
	SIZEL sizl;
	HDC hdc;
	HPS hps;
	struct
		{
		BITMAPINFOHEADER2 bmp2;
		RGB2 argb2Color [0x100];
		} bm;

	/* Got the data in memory, now make bitmap */

	memset(&bm, 0, sizeof(bm));

	bm.bmp2.cbFix     = sizeof(BITMAPINFOHEADER2);
	bm.bmp2.cx        = gbm -> w;
	bm.bmp2.cy        = gbm -> h;
	bm.bmp2.cBitCount = gbm -> bpp;
	bm.bmp2.cPlanes   = 1;

	cRGB = ( (1 << gbm -> bpp) & 0x1ff );
		/* 1 -> 2, 4 -> 16, 8 -> 256, 24 -> 0 */

	for ( usCol = 0; usCol < cRGB; usCol++ )
		{
		bm.argb2Color [usCol].bRed   = gbmrgb [usCol].r;
		bm.argb2Color [usCol].bGreen = gbmrgb [usCol].g;
		bm.argb2Color [usCol].bBlue  = gbmrgb [usCol].b;
		}

	if ( (hdc = DevOpenDC(hab, OD_MEMORY, "*", 0L, (PDEVOPENDATA) NULL, (HDC) NULL)) == (HDC) NULL )
		{
		#ifdef DEBUG_LOGGING
		AddLog("===GBMFUNC->MakeBitmap=== DevOpenDC failure\n");
		#endif
		return ( 0 );
		}

	sizl.cx = bm.bmp2.cx;
	sizl.cy = bm.bmp2.cy;
	if ( (hps = GpiCreatePS(hab, hdc, &sizl, PU_PELS | GPIF_DEFAULT | GPIT_MICRO | GPIA_ASSOC)) == (HPS) NULL )
		{
		#ifdef DEBUG_LOGGING
		AddLog("===GBMFUNC->MakeBitmap=== GpiCreatePS failure\n");
		#endif
		DevCloseDC(hdc);
		return ( 0 );
		}


	if ( cRGB == 2 )
/*...shandle 1bpp case:16:*/
/*
1bpp presentation spaces have a reset or background colour.
They also have a contrast or foreground colour.
When data is mapped into a 1bpp presentation space :-
Data which is the reset colour, remains reset, and is stored as 0's.
All other data becomes contrast, and is stored as 1's.
The reset colour for 1bpp screen HPSs is white.
I want 1's in the source data to become 1's in the HPS.
We seem to have to reverse the ordering here to get the desired effect.
*/

{
static RGB2 argb2Black = { 0x00, 0x00, 0x00 };
static RGB2 argb2White = { 0xff, 0xff, 0xff };
bm.argb2Color [0] = argb2Black; /* Contrast */
bm.argb2Color [1] = argb2White; /* Reset */
}
/*...e*/

	if ( (*phbm = GpiCreateBitmap(hps, &(bm.bmp2), CBM_INIT, (BYTE *) pbData, (BITMAPINFO2 *) &(bm.bmp2))) == (HBITMAP) NULL )
		{
		#ifdef DEBUG_LOGGING
		ULONG errCode = WinGetLastError(hab);
		AddLog("===GBMFUNC->MakeBitmap=== GpiCreateBitmap failure (%lu)\n",errCode);
		#endif
		GpiDestroyPS(hps);
		DevCloseDC(hdc);
		return ( 2 );
		}

	GpiSetBitmap(hps, (HBITMAP) NULL);
	GpiDestroyPS(hps);
	DevCloseDC(hdc);

	return ( 1 );
	}

