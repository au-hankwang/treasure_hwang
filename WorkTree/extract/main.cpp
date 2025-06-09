
#include "pch.h"
#include "gzip.h"

#ifdef _MSC_VER
 #pragma optimize("",off)
#endif

stringPooltype stringPool;

#ifdef WINGUI

// global variables for Windows GUI
HINSTANCE hInst;
bool isWINNT;
bool mainIsReadyToQuit;
HANDLE shutdown_semaphore;
CRITICAL_SECTION CS_global_init;

void WindowsInit()
   {
   INITCOMMONCONTROLSEX initcommon;
   initcommon.dwSize=sizeof(initcommon);
   initcommon.dwICC = ICC_WIN95_CLASSES|ICC_INTERNET_CLASSES|ICC_USEREX_CLASSES|ICC_DATE_CLASSES;
   if (!InitCommonControlsEx(&initcommon))
      {
      MessageBox(NULL,"You comctl32.dll is outdated.\nCarlson Engineering can supply this for you or\nyou can install Internet Explorer.","FATAL_ERROR",MB_OK|MB_ICONERROR| MB_SETFOREGROUND);
      exit(-1);
      }   

   OSVERSIONINFO osvi;
   osvi.dwOSVersionInfoSize = sizeof(osvi);
   GetVersionEx(&osvi);
   isWINNT = osvi.dwPlatformId==VER_PLATFORM_WIN32_NT;  // find out if I'm on an NT or win 95 machine
   }

filetype logfp;

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
   {   
   hInst = hInstance;   
   int argc=0;
   char **argv=NULL;
   
   InitializeCriticalSection(&CS_global_init);
   WindowsInit();
   shutdown_semaphore=CreateSemaphore(NULL, 0, 1, NULL);
//   ResourceCheckerInit();
   WindowsConsoleInit();
   
   SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL); // this makes our program place nice with other window app's
   
   int retCode = Work(argc, argv);
   TAtype::PrintSummary(false);
   mainIsReadyToQuit = true;
   WaitForSingleObject(shutdown_semaphore,INFINITE);
   Cleanup();
   Sleep(500);  // give all the other threads a chance to destruct before calling the leak checker
   shutdown(retCode);
   return retCode;
   }

void DebugBreakFunc(const char *module, int line)
   {
   FatalErrorMessage("\nFatal error in module %s line %d\n",module,line);
   }

#else

filetype logfp;
int main(int argc, char *argv[])
    {
//    setvbuf(stdout, NULL, _IOLBF, 0);
//    ResourceCheckerInit();
    /*
int seed=0,i;
for (i=0; i<32768; i++)
   {
   if (i%32==0)
      printf("\n");
   printf("%d,",(unsigned)(random(seed)*4.0e9));
   }
return 0;
    */
    
    /*
int i;
unsigned lfsr=1, poly=0x0003;

for (i=0; i<32768+10; i++)
   {
   printf("i=%5d lfsr=%4x\n",i,lfsr);
   if (lfsr&0x4000)
      lfsr = (lfsr<<1) ^ poly;
   else
      lfsr = (lfsr<<1);
   lfsr = lfsr & 0x7fff;
   if (lfsr==1)
      printf("");
   }
return 0;
*/
/*
float entropy = 0.0;
int i;
for (i=0; i<90; i++)
    entropy += sqrt(i*2000.0/800.0)/500.0;
printf("%.1f\n",entropy);
return 0;
*/

#ifdef _MSC_VER
    logfp.fopen("log.txt","wt");
#else
    logfp.fopen("log.txt.gz","wt");
#endif
    int retCode = Work(argc, argv);
    TAtype::PrintSummary(false);
    
//    ResourceCheckOnExit("main");
    shutdown(retCode);
    return retCode;
    }

void new_puts(const char *buffer)
   {
#ifdef _MSC_VER
   OutputDebugString(buffer);
#endif
   fputs(buffer, stdout);         
   if (logfp.IsOpen())
      logfp.fputs(buffer);
   }
void DebugBreakFunc(const char *module, int line)
   {
   FatalErrorMessage("\nFatal error in module %s line %d\n",module,line);
   }
#endif

void shutdown(int exitcode)
   {
   printf("\n");
   if (logfp.IsOpen())
      logfp.fclose();
   exit(exitcode);
   }


void FatalErrorMessage(const char *fmt, ...)
   {
   va_list args;
   char buffer[1024];
   
   va_start(args, fmt);
   int count = vsprintf(buffer, fmt, args);
   va_end(args);
   
#ifdef _MSC_VER   
//   MessageBeep(MB_ICONEXCLAMATION);   
//   MessageBox(NULL, buffer + (buffer[0]=='\n' ? 1:0), "Fatal Error",MB_OK|MB_ICONEXCLAMATION|MB_TASKMODAL| MB_SETFOREGROUND);
   fcloseall();
   DebugBreak();
#else
   printf("\a\a%s\n",buffer);
   int a = *(int *)0;  // dereference a NULL pointer to force a core dump
   printf("%d",a);
#endif
   shutdown(-1);
   };



