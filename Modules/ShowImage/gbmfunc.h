#ifndef _GBM_FUNC_
#define _GBM_FUNC_

#include "gbm.h"

BOOL IsGbmAvailable(void);
BOOL LoadBitmap(HWND hwnd, const CHAR *szFn, const CHAR *szOpt, GBM *gbm, GBMRGB *gbmrgb, BYTE **ppbData);
int MakeBitmap(HWND hwnd, const GBM *gbm, const GBMRGB *gbmrgb, const BYTE *pbData, HBITMAP *phbm);

#endif

