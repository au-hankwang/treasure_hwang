/* Copyright (c) 2000-2001 Carlson Engineering Inc., 719B Fort Worth Texas 76106. All rights reserved.
   Copyright (c) 2000-2001 Compaq Computer Corporation. All rights reserved. as per aggreement #4660000596 */

#include "pch.h"
#include <crtdbg.h>
#include <time.h>
#include "debug.h"


#undef DeleteObject
#undef CreatePen
#undef CreateSolidBrush
#undef CreateBrushIndirect
#undef CreateHatchBrush
#undef CreateCompatibleBitmap
#undef CreateDIBitmap
#undef CreateBitmap
#undef CreateDIBSection
#undef CreateBitmapIndirect
#undef CreateCompatibleDC
#undef DeleteDC
#undef LeakCheckFreeObject

struct objecttype{
   HANDLE handle;
   char *module;
   int line;
   };

static array <objecttype> objects;
static CRITICAL_SECTION CS_debug;
static bool program_exit_in_progress=false;


void AddToList(char *module, int line, HANDLE handle)
   {
   objecttype obj;

   EnterCriticalSection(&CS_debug);

   obj.handle = handle;
   obj.module = module;
   obj.line = line;   
   for (int i=objects.size()-1; i>=0; i--)
      if (objects[i].handle==NULL)
         {
         objects[i]=obj;
         break;
         }
   if (i<0)
      objects.push_back(obj);
      
   LeaveCriticalSection(&CS_debug);
   }

HBITMAP CarlsonCreateDIBitmap(char *module, int line, HDC hdc, CONST BITMAPINFOHEADER *lpbmih, DWORD fdwInit, CONST VOID *lpbInit, CONST BITMAPINFO *lpbmi, UINT fuUsage)
   {
   HBITMAP hBitmap=CreateDIBitmap(hdc, lpbmih, fdwInit, lpbInit, lpbmi, fuUsage);

   AddToList(module, line, (HANDLE)hBitmap);

   return hBitmap;
   }

HBITMAP CarlsonCreateCompatibleBitmap(char *module, int line, HDC hdc, int nWidth, int nHeight)
   {
   HBITMAP hBitmap=CreateCompatibleBitmap(hdc, nWidth, nHeight);

   AddToList(module, line, (HANDLE)hBitmap);

   return hBitmap;
   }

HBITMAP CarlsonCreateBitmap(char *module, int line, int nWidth, int nHeight, UINT cPlanes, UINT cBitsPerPel, CONST VOID *lpvBits)
   {
   HBITMAP hBitmap=CreateBitmap(nWidth, nHeight, cPlanes, cBitsPerPel, lpvBits);

   AddToList(module, line, (HANDLE)hBitmap);

   return hBitmap;
   }

HBITMAP CarlsonCreateDIBSection(char *module, int line, HDC hdc, CONST BITMAPINFO *pbmi, UINT iUsage, VOID **ppvBits, HANDLE hSection, DWORD dwOffset)
   {
   HBITMAP hBitmap=CreateDIBSection(hdc, pbmi, iUsage, ppvBits, hSection, dwOffset);

   AddToList(module, line, (HANDLE)hBitmap);

   return hBitmap;
   }

HBITMAP CarlsonCreateBitmapIndirect(char *module, int line, const BITMAP *bm)
   {
   HBITMAP hBitmap=CreateBitmapIndirect(bm);

   AddToList(module, line, (HANDLE)hBitmap);

   return hBitmap;
   }


HPEN CarlsonCreatePen(char *module, int line, int fnPenStyle, int nWidth, COLORREF crColor)
   {
   HPEN hPen=CreatePen(fnPenStyle, nWidth, crColor);

   AddToList(module, line, (HANDLE)hPen);

   return hPen;
   }

HFONT CarlsonCreateFont(char *module, int line, int nHeight, int nWidth, int nEscapement, int nOrientation, int fnWeight, DWORD fdwItalic,
                        DWORD fdwUnderline, DWORD fdwStrikeOut, DWORD fdwCharSet, DWORD fdwOutputPrecision, 
                        DWORD fdwClipPrecision, DWORD fdwQuality, DWORD fdwPitchAndFamily, LPCTSTR lpszFace)
   {
   HFONT hFont = CreateFontA(nHeight, nWidth, nEscapement, nOrientation, fnWeight, fdwItalic, fdwUnderline,
                            fdwStrikeOut, fdwCharSet, fdwOutputPrecision, fdwClipPrecision, 
                            fdwQuality, fdwPitchAndFamily, lpszFace);
   
   AddToList(module, line, (HANDLE)hFont);
   return hFont;
   }

HFONT CarlsonCreateFontIndirect(char *module, int line, CONST LOGFONT *lplf)
   {
   HFONT hFont = CreateFontIndirectA(lplf);

   AddToList(module, line, (HANDLE)hFont);
   return hFont;
   }


HBRUSH CarlsonCreateSolidBrush(char *module, int line, COLORREF color)
   {
   HBRUSH hBrush = CreateSolidBrush(color);
   
   AddToList(module, line, (HANDLE)hBrush);
   return hBrush;
   }

HBRUSH CarlsonCreateBrushIndirect(char *module, int line, CONST LOGBRUSH *lplb)
   {
   HBRUSH hBrush = CreateBrushIndirect(lplb);
   
   AddToList(module, line, (HANDLE)hBrush);
   return hBrush;
   }

HBRUSH CarlsonCreateHatchBrush(char *module, int line, int fnStyle, COLORREF color)
   {
   HBRUSH hBrush = CreateHatchBrush(fnStyle, color);
   
   AddToList(module, line, (HANDLE)hBrush);
   return hBrush;
   }

HDC CarlsonCreateCompatibleDC(char *module, int line, HDC _hDC)
   {
   HDC hDC = CreateCompatibleDC(_hDC);
   
   AddToList(module, line, (HANDLE)hDC);
   return hDC;
   }

bool LeakCheckFreeObject(HANDLE hObject)
   {
   int i;
   int found=0;

   if (program_exit_in_progress)
      return true;   // I can't predict whether a static object's destructor might call delete object after my arraytype has been destroyed

   EnterCriticalSection(&CS_debug);
   
   for (i=objects.size()-1; i>=0; i--)
      if (objects[i].handle == hObject)
         {
         objects[i].handle=NULL;
         found++;
         }
   LeaveCriticalSection(&CS_debug);

   if (found==0)
      FATAL_ERROR; // we deleted an object that doesn't exist
   if (found>1)
      FATAL_ERROR; // this should never happen
   return true;
   }

bool CarlsonDeleteObject(HGDIOBJ hObject)
   {
   if (!DeleteObject(hObject))
      {
      printf("\aERROR: A delete object failed. Possible resource leak.");
      return false;
      }
   return LeakCheckFreeObject(hObject);
   }


bool CarlsonDeleteDC(HDC hDC)
   {
   int i;

   if (!DeleteDC(hDC))
      {
      printf("\aERROR: A DeleteDC() failed.  Possible resource leak.");
      return false;
      }
   if (program_exit_in_progress)
      return true;   // I can't predict whether a static object's destructor might call delete object after my arraytype has been destroyed

   EnterCriticalSection(&CS_debug);
   
   for (i=objects.size()-1; i>=0; i--)
      if (objects[i].handle == (HANDLE)hDC)
         objects[i].handle=NULL;

   LeaveCriticalSection(&CS_debug);
   return true;
   }


int MyReportHook( int reportType, char *message, int *returnValue )
   {
   static int one_shot=false;
   FILE *fptr;
   
   if (reportType!=_CRT_WARN) return FALSE;

   if (!one_shot)
      {
      one_shot=true;
      fptr = fopen("Memory.txt","wb");      
      fputs(message, fptr);
      fclose(fptr);
      return FALSE;
      }
   else
      {
      fptr = fopen("Memory.txt","ab");
      fputs(message, fptr);
      fclose(fptr);
      return TRUE;
      }
   }


void ResourceCheckerInit()
   {
   static bool init_yet=false;

//   _CrtSetBreakAlloc(82);   // you can specify an allocation number and a breakpoint will happen

//   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF);
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   _CrtSetReportMode(_CRT_WARN,   _CRTDBG_MODE_WNDW);
   _CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_WNDW);
   _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_WNDW);
   
   if (init_yet) FATAL_ERROR;
   InitializeCriticalSection(&CS_debug);
   init_yet=true;
   }


void ResourceCheckOnExit(const char *module)
   {
   int i;
   int count=0;
   char buffer[256];
   objecttype obj;

   program_exit_in_progress=true;

   for (i=objects.size()-1; i>=0; i--)
      if (objects[i].handle!=NULL)
         {
         count++;
         obj=objects[i];
         }
      
   if (count)
      {
      sprintf(buffer,"Upon program exit I detected %d HANDLESs that had not been deleted in module %s!\nThe first culprit is in module %s line %d",count,module,obj.module,obj.line);
      MessageBox(NULL, buffer, "Resource/Memory Leak Detector",MB_OK|MB_ICONINFORMATION);
      }

// If you get a memory leak dialog box then it 'Ignore' and look for a memory.txt file
// detailing all the leaks

   _CrtSetReportHook(MyReportHook);
   }


// I copied this from MSJ.
// this will make a global function which catches unhandled exceptions 
// and generate a debug rport

class MSJExceptionHandler
   {
public:
   
   MSJExceptionHandler( );
   ~MSJExceptionHandler( );
   
   void SetLogFileName( PTSTR pszLogFileName );
   void MSJExceptionHandler::PrintDebugReport(const char *errortxt);
   
private:
   
   // entry point where control comes on an unhandled exception
   static LONG WINAPI MSJUnhandledExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo );
   
   // where report info is extracted and generated 
   static void GenerateExceptionReport( PEXCEPTION_POINTERS pExceptionInfo );
   
   // Helper functions
   static LPTSTR GetExceptionString( DWORD dwCode );
   static BOOL GetLogicalAddress(  PVOID addr, PTSTR szModule, DWORD len, DWORD& section, DWORD& offset );
   static void IntelStackWalk( PCONTEXT pContext );
   static int __cdecl _tprintf(const TCHAR * format, ...);
   
   // Variables used by the class
   static TCHAR m_szLogFileName[MAX_PATH];
   static LPTOP_LEVEL_EXCEPTION_FILTER m_previousFilter;
   static HANDLE m_hReportFile;
   };


//============================== Global Variables =============================
//
// Declare the static variables of the MSJExceptionHandler class
//
TCHAR MSJExceptionHandler::m_szLogFileName[MAX_PATH];
LPTOP_LEVEL_EXCEPTION_FILTER MSJExceptionHandler::m_previousFilter;
HANDLE MSJExceptionHandler::m_hReportFile;

MSJExceptionHandler g_MSJExceptionHandler;  // Declare global instance of class


void PrintDebugReport(const char *errortxt)  // this will log the message to a file and then walk the stack
   {
   g_MSJExceptionHandler.PrintDebugReport(errortxt);
   }


void MSJExceptionHandler::PrintDebugReport(const char *errortxt)  // this will log the message to a file and then walk the stack
   {
   m_hReportFile = CreateFile( m_szLogFileName, GENERIC_WRITE, 0, 0, OPEN_ALWAYS, FILE_FLAG_WRITE_THROUGH, 0);
   
   if ( m_hReportFile )
      {
      SetFilePointer( m_hReportFile, 0, 0, FILE_END );
      
      time_t sec;
      // Start out with a banner
      _tprintf("\n//=====================================================");
      time(&sec);
      _tprintf("\n%s", asctime(localtime(&sec)));
      _tprintf("FATAL_ERROR: %s", errortxt);
      
      CloseHandle( m_hReportFile );
      m_hReportFile = 0;
      }
   }


//============================== Class Methods =============================
//=============
// Constructor 
//=============
MSJExceptionHandler::MSJExceptionHandler( )
   {
    // Install the unhandled exception filter function
    m_previousFilter = SetUnhandledExceptionFilter(MSJUnhandledExceptionFilter);

    // Figure out what the report file will be named, and store it away
    GetModuleFileName( 0, m_szLogFileName, MAX_PATH );

    // Look for the '.' before the "EXE" extension.  Replace the extension
    // with "RPT"
    PTSTR pszDot = _tcsrchr( m_szLogFileName, _T('.') );
    if ( pszDot )
       {
        pszDot++;   // Advance past the '.'
        if ( _tcslen(pszDot) >= 3 )
            _tcscpy( pszDot, _T("crash") );   
       }
   }
//============
// Destructor 
//============
MSJExceptionHandler::~MSJExceptionHandler( )
   {
    SetUnhandledExceptionFilter( m_previousFilter );
   }
//==============================================================
// Lets user change the name of the report file to be generated 
//==============================================================
void MSJExceptionHandler::SetLogFileName( PTSTR pszLogFileName )
   {
    _tcscpy( m_szLogFileName, pszLogFileName );
   }
//===========================================================
// Entry point where control comes on an unhandled exception 
//===========================================================
LONG WINAPI MSJExceptionHandler::MSJUnhandledExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo )
   {
   m_hReportFile = CreateFile( m_szLogFileName, GENERIC_WRITE, 0, 0, OPEN_ALWAYS, FILE_FLAG_WRITE_THROUGH, 0);
   
   if ( m_hReportFile )
      {
      SetFilePointer( m_hReportFile, 0, 0, FILE_END );
      
      GenerateExceptionReport( pExceptionInfo );
      
      CloseHandle( m_hReportFile );
      m_hReportFile = 0;
      }
   
   if ( m_previousFilter )
      return m_previousFilter( pExceptionInfo );
   else
      return EXCEPTION_CONTINUE_SEARCH;
   }


//===========================================================================
// Open the report file, and write the desired information to it.  Called by 
// MSJUnhandledExceptionFilter                                               
//===========================================================================
void MSJExceptionHandler::GenerateExceptionReport(PEXCEPTION_POINTERS pExceptionInfo)
   {
   time_t sec;
   // Start out with a banner
   _tprintf("\n//=====================================================");
   time(&sec);
   _tprintf("\n%s", asctime(localtime(&sec)));

   PEXCEPTION_RECORD pExceptionRecord = pExceptionInfo->ExceptionRecord;
   
   // First print information about the type of fault
   _tprintf("\nException code: %08X %s", pExceptionRecord->ExceptionCode, GetExceptionString(pExceptionRecord->ExceptionCode));
   
   // Now print information about where the fault occured
   TCHAR szFaultingModule[MAX_PATH];
   DWORD section, offset;
   GetLogicalAddress(  pExceptionRecord->ExceptionAddress, szFaultingModule, sizeof( szFaultingModule ), section, offset );
   
   _tprintf("\nFault address:  %08X %02X:%08X %s", pExceptionRecord->ExceptionAddress, section, offset, szFaultingModule );
   
   PCONTEXT pCtx = pExceptionInfo->ContextRecord;
   
   // Show the registers
#ifdef _M_IX86  // Intel Only!
   _tprintf( _T("\nRegisters:\n") );
   
   _tprintf("\nEAX:%08X\nEBX:%08X\nECX:%08X\nEDX:%08X\nESI:%08X\nEDI:%08X", pCtx->Eax, pCtx->Ebx, pCtx->Ecx, pCtx->Edx, pCtx->Esi, pCtx->Edi);
   _tprintf("\nCS:EIP:%04X:%08X", pCtx->SegCs, pCtx->Eip );
   _tprintf("\nSS:ESP:%04X:%08X  EBP:%08X", pCtx->SegSs, pCtx->Esp, pCtx->Ebp );
   _tprintf("\nDS:%04X  ES:%04X  FS:%04X  GS:%04X", pCtx->SegDs, pCtx->SegEs, pCtx->SegFs, pCtx->SegGs );
   _tprintf("\nFlags:%08X\n", pCtx->EFlags );
   
   // Walk the stack using x86 specific code
   IntelStackWalk( pCtx );
   
#endif
   
   _tprintf("\n");
   }
//======================================================================
// Given an exception code, returns a pointer to a static string with a 
// description of the exception                                         
//======================================================================
LPTSTR MSJExceptionHandler::GetExceptionString( DWORD dwCode )
   {
#define EXCEPTION( x ) case EXCEPTION_##x: return _T(#x);
   
   switch ( dwCode )
      {   
      EXCEPTION( ACCESS_VIOLATION )
      EXCEPTION( DATATYPE_MISALIGNMENT )
      EXCEPTION( BREAKPOINT )
      EXCEPTION( SINGLE_STEP )
      EXCEPTION( ARRAY_BOUNDS_EXCEEDED )
      EXCEPTION( FLT_DENORMAL_OPERAND )
      EXCEPTION( FLT_DIVIDE_BY_ZERO )
      EXCEPTION( FLT_INEXACT_RESULT )
      EXCEPTION( FLT_INVALID_OPERATION )
      EXCEPTION( FLT_OVERFLOW )
      EXCEPTION( FLT_STACK_CHECK )
      EXCEPTION( FLT_UNDERFLOW )
      EXCEPTION( INT_DIVIDE_BY_ZERO )
      EXCEPTION( INT_OVERFLOW )
      EXCEPTION( PRIV_INSTRUCTION )
      EXCEPTION( IN_PAGE_ERROR )
      EXCEPTION( ILLEGAL_INSTRUCTION )
      EXCEPTION( NONCONTINUABLE_EXCEPTION )
      EXCEPTION( STACK_OVERFLOW )
      EXCEPTION( INVALID_DISPOSITION )
      EXCEPTION( GUARD_PAGE )
      EXCEPTION( INVALID_HANDLE )
      }
   
   // If not one of the "known" exceptions, try to get the string
   // from NTDLL.DLL's message table.
   static TCHAR szBuffer[512] = { 0 };
   
   FormatMessage(  FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE,
      GetModuleHandle("NTDLL.DLL"), dwCode, 0, szBuffer, sizeof( szBuffer ), 0 );
   
   return szBuffer;
   }
//==============================================================================
// Given a linear address, locates the module, section, and offset containing  
// that address.                                                               
//                                                                             
// Note: the szModule paramater buffer is an output buffer of length specified 
// by the len parameter (in characters!)                                       
//==============================================================================
BOOL MSJExceptionHandler::GetLogicalAddress(PVOID addr, PTSTR szModule, DWORD len, DWORD& section, DWORD& offset )
   {
   MEMORY_BASIC_INFORMATION mbi;
   
   if ( !VirtualQuery( addr, &mbi, sizeof(mbi) ) )
      return FALSE;
   
   DWORD hMod = (DWORD)mbi.AllocationBase;
   
   if ( !GetModuleFileName( (HMODULE)hMod, szModule, len ) )
      return FALSE;
   
   // Point to the DOS header in memory
   PIMAGE_DOS_HEADER pDosHdr = (PIMAGE_DOS_HEADER)hMod;
   
   // From the DOS header, find the NT (PE) header
   PIMAGE_NT_HEADERS pNtHdr = (PIMAGE_NT_HEADERS)(hMod + pDosHdr->e_lfanew);
   
   PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION( pNtHdr );
   
   DWORD rva = (DWORD)addr - hMod; // RVA is offset from module load address
   
   // Iterate through the section table, looking for the one that encompasses
   // the linear address.
   for (   unsigned i = 0;
   i < pNtHdr->FileHeader.NumberOfSections;
   i++, pSection++ )
      {
      DWORD sectionStart = pSection->VirtualAddress;
      DWORD sectionEnd = sectionStart
         + max(pSection->SizeOfRawData, pSection->Misc.VirtualSize);
      
      // Is the address in this section???
      if ( (rva >= sectionStart) && (rva <= sectionEnd) )
         {
         // Yes, address is in the section.  Calculate section and offset,
         // and store in the "section" & "offset" params, which were
         // passed by reference.
         section = i+1;
         offset = rva - sectionStart;
         return TRUE;
         }
      }
   
   return FALSE;   // Should never get here!
   }
//============================================================
// Walks the stack, and writes the results to the report file 
//============================================================
void MSJExceptionHandler::IntelStackWalk( PCONTEXT pContext )
   {
#ifdef _M_IX86
   _tprintf("\nCall stack:");
   
   _tprintf("\nAddress   Frame     Logical addr  Module");
   
   DWORD pc = pContext->Eip;
   PDWORD pFrame, pPrevFrame;
   
   pFrame = (PDWORD)pContext->Ebp;
   
   do
      {
      TCHAR szModule[MAX_PATH] = _T("");
      DWORD section = 0, offset = 0;
      
      GetLogicalAddress((PVOID)pc, szModule,sizeof(szModule),section,offset );
      
      _tprintf("\n%08X  %08X  %04X:%08X %s",pc, pFrame, section, offset, szModule );
      
      pc = pFrame[1];
      
      pPrevFrame = pFrame;
      
      pFrame = (PDWORD)pFrame[0]; // precede to next higher frame on stack
      
      if ( (DWORD)pFrame & 3 )    // Frame pointer must be aligned on a
         break;                  // DWORD boundary.  Bail if not so.
      
      if ( pFrame <= pPrevFrame )
         break;
      
      // Can two DWORDs be read from the supposed frame address?          
      if ( IsBadWritePtr(pFrame, sizeof(PVOID)*2) )
         break;
      
      } while ( 1 );
#endif
   }
//============================================================================
// Helper function that writes to the report file, and allows the user to use 
// printf style formating                                                     
//============================================================================
int __cdecl MSJExceptionHandler::_tprintf(const TCHAR * format, ...)
   {
   TCHAR szBuff[1024];
   int retValue;
   DWORD cbWritten;
   va_list argptr;
   
   va_start( argptr, format );
   retValue = wvsprintf( szBuff, format, argptr );
   va_end( argptr );
   
   WriteFile( m_hReportFile, szBuff, retValue * sizeof(TCHAR), &cbWritten, 0 );
   
   return retValue;
   }


