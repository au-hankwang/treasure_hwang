/* Copyright (c) 2000-2001 Carlson Engineering Inc., 719B Fort Worth Texas 76106. All rights reserved.
   Copyright (c) 2000-2001 Compaq Computer Corporation. All rights reserved. as per aggreement #4660000596 */

#ifndef __DEBUG_H_INCLUDED__ 
#define __DEBUG_H_INCLUDED__



#if defined (_DEBUG) && defined(_MSC_VER)


#undef CreateFont
#undef CreateFontIndirect

#define CreatePen(a,b,c) CarlsonCreatePen(__FILE__,__LINE__,a,b,c)
#define CreateSolidBrush(a) CarlsonCreateSolidBrush(__FILE__,__LINE__,a)
#define CreateBrushIndirect(a) CarlsonCreateBrushIndirect(__FILE__,__LINE__,a)
#define CreateHatchBrush(a,b) CarlsonCreateHatchBrush(__FILE__,__LINE__,a,b)
#define CreateFont(a,b,c,d,e,f,g,h,i,j,k,l,m,n) CarlsonCreateFont(__FILE__,__LINE__,a,b,c,d,e,f,g,h,i,j,k,l,m,n)
#define CreateFontIndirect(a) CarlsonCreateFontIndirect(__FILE__,__LINE__,a)
#define CreateCompatibleBitmap(a,b,c) CarlsonCreateCompatibleBitmap(__FILE__,__LINE__,a,b,c)
#define CreateDIBitmap(a,b,c,d,e,f) CarlsonCreateDIBitmap(__FILE__,__LINE__,a,b,c,d,e,f)
#define CreateBitmap(a,b,c,d,e) CarlsonCreateBitmap(__FILE__,__LINE__,a,b,c,d,e)
#define CreateBitmapIndirect(a) CarlsonCreateBitmapIndirect(__FILE__,__LINE__,a)
#define CreateDIBSection(a,b,c,d,e,f) CarlsonCreateDIBSection(__FILE__,__LINE__,a,b,c,d,e,f)
#define CreateCompatibleDC(a) CarlsonCreateCompatibleDC(__FILE__,__LINE__,a)

#define DeleteObject CarlsonDeleteObject
#define DeleteDC CarlsonDeleteDC


HPEN CarlsonCreatePen(char *module, int line, int fnPenStyle, int nWidth, COLORREF crColor);
HFONT CarlsonCreateFont(char *module, int line, int nHeight, int nWidth, int nEscapement, int nOrientation, int fnWeight, DWORD fdwItalic,
                        DWORD fdwUnderline, DWORD fdwStrikeOut, DWORD fdwCharSet, DWORD fdwOutputPrecision, 
                        DWORD fdwClipPrecision, DWORD fdwQuality, DWORD fdwPitchAndFamily, LPCTSTR lpszFace);
HFONT CarlsonCreateFontIndirect(char *module, int line, CONST LOGFONT *lplf);
HBRUSH CarlsonCreateSolidBrush(char *module, int line, COLORREF color);
HBRUSH CarlsonCreateBrushIndirect(char *module, int line, CONST LOGBRUSH *lplb);
HBRUSH CarlsonCreateHatchBrush(char *module, int line, int fnStyle, COLORREF color);
HBITMAP CarlsonCreateCompatibleBitmap(char *module, int line, HDC hdc, int nWidth, int nHeight);
HBITMAP CarlsonCreateDIBitmap(char *module, int line, HDC hdc, CONST BITMAPINFOHEADER *lpbmih, DWORD fdwInit, CONST VOID *lpbInit, CONST BITMAPINFO *lpbmi, UINT fuUsage);
HBITMAP CarlsonCreateBitmap(char *module, int line, int nWidth, int nHeight, UINT cPlanes, UINT cBitsPerPel, CONST VOID *lpvBits);
HBITMAP CarlsonCreateDIBSection(char *module, int line, HDC hdc, CONST BITMAPINFO *pbmi, UINT iUsage, VOID **ppvBits, HANDLE hSection, DWORD dwOffset);
HBITMAP CarlsonCreateBitmapIndirect(char *module, int line, const BITMAP *bm);
HDC CarlsonCreateCompatibleDC(char *module, int line, HDC _hDC);
bool CarlsonDeleteObject(HGDIOBJ hObject);
bool CarlsonDeleteDC(HDC hDC);
bool LeakCheckFreeObject(HANDLE hObject);

#else

#define LeakCheckFreeObject


#endif     // #ifdef DEBUG


#ifndef _MSC_VER
  inline void ResourceCheckerInit(){}
  inline void ResourceCheckOnExit(const char *module){}
  inline void PrintDebugReport(const char *){}
#else
  void ResourceCheckerInit();
  void ResourceCheckOnExit(const char *module);
  void PrintDebugReport(const char *);  // this will log the message to a file and then walk the stack
#endif


#endif
