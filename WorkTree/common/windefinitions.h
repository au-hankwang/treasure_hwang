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

typedef unsigned int uint; 
typedef __int64 int64_t; 


#pragma warning (disable: 4244)  // warning C4244: '=' : conversion from 'double' to 'float', possible loss of data
#pragma warning (disable: 4305)  // warning C4305: '=' : truncation from 'const double' to 'float'
#pragma warning (disable: 4786)  // symbol greater than 255 character, STL does this like crazy, no problem
#pragma warning (disable: 4355)  // warning C4355: 'this' : used in base member initializer list
#pragma warning (disable: 4018)  // warning C4018: '<' : signed/unsigned mismatch
#pragma warning (disable: 4800)  // warning C4800: 'unsigned __int64' : forcing value to bool 'true' or 'false' (performance warning)
#pragma warning (disable: 4996)  // Gets rid of vsprintf warning
#pragma warning (disable: 4748)	 // /GS can not protect parameters and local variables from local buffer overrun because optimizations are disabled in function
#pragma warning (disable: 4267)	 // conversion from 'size_t' to 'type', possible loss of data
#pragma warning (disable: 26495) // Always initialize a member variable(type.6)
//#pragma warning (disable: 6031)	 // 


typedef unsigned __int64 __uint64;
typedef unsigned int   __uint32;
typedef unsigned short __uint16;
typedef unsigned char  __uint8;

/*inline double log2(double n)
{  
    // log(n)/log(2) is log2.  
    return log( n ) / log( 2.0 );  
}*/

#endif
