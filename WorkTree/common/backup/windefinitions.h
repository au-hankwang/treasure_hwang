/* Copyright (c) 2000-2001 Carlson Engineering Inc., 719B Fort Worth Texas 76106. All rights reserved.
   Copyright (c) 2000-2001 Compaq Computer Corporation. All rights reserved. as per aggreement #4660000596 */

#ifdef _MSC_VER

#include <process.h>
#include <tchar.h>

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <windowsx.h>
#include <winsock2.h>
#include <shellapi.h>
#include <commdlg.h>
#include <commctrl.h>


#define DEVICECONTEXT HDC




#pragma warning (disable: 4244)  // warning C4244: '=' : conversion from 'double' to 'float', possible loss of data
#pragma warning (disable: 4305)  // warning C4305: '=' : truncation from 'const double' to 'float'
#pragma warning (disable: 4786)  // symbol greater than 255 character, STL does this like crazy, no problem
#pragma warning (disable: 4355)  // warning C4355: 'this' : used in base member initializer list
#pragma warning (disable: 4018)  // warning C4018: '<' : signed/unsigned mismatch
#pragma warning (disable: 4800)  // warning C4800: 'unsigned __int64' : forcing value to bool 'true' or 'false' (performance warning)

typedef unsigned __int64 __uint64;


#endif
