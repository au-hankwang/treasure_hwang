#include "pch.h"
#include "helper.h"
#include "multithread.h"
#ifndef MARIE
#include "test.h"
#endif

void Test();
void DataProcessing() {}
void MinerDebug();
void Electrical_Sim();
void Electrical_Sim2();
void Eval_Simulator_Reference();
void Fpga();
void WriteSdc();
int MakeSdc();
void Discovery();
void AdderWork();
void VtProcess();

int main(int argc, char* argv[])
   {
   TAtype ta("main()");
   bool abbreviated = false, shmoo=false, turbo=false;

   if (argc==2 && (strstr(argv[1], "regen") != NULL || strstr(argv[1], "Regen") != NULL || strstr(argv[1], "REGEN") != NULL)) 
      {
      DataProcessing();
      return 0;
      }
   if (argc == 2 && (strstr(argv[1], "simple") != NULL || strstr(argv[1], "Simple") != NULL || strstr(argv[1], "SIMPLE") != NULL)) 
      {
      testtype t(2); // use USB
      const char* ver = t.usb.Version();
//      t.Simple();
      return 0;
      }
   if (argc == 2 && (strstr(argv[1], "quick") != NULL || strstr(argv[1], "Quick") != NULL || strstr(argv[1], "QUICK") != NULL)) {
      abbreviated = true;
      }
   if (argc == 2 && (strstr(argv[1], "turbo") != NULL || strstr(argv[1], "Turbo") != NULL || strstr(argv[1], "TURBO") != NULL)) {
      turbo = true;
      }
   if (argc == 2 && (strstr(argv[1], "shmoo") != NULL || strstr(argv[1], "Shmoo") != NULL || strstr(argv[1], "SHMOO") != NULL)) {
      shmoo = true;
      }

   testtype t(2); // use USB
//   shmoo = true;

   t.usb.OLED_Display("Test\n");
   t.usb.SetVoltage(S_CORE, 200);
   t.usb.SetVoltage(S_IO, 1500);
   t.usb.SetVoltage(S_SCV, 750);
   t.usb.SetVoltage(S_RO, 300);
   t.usb.ZeroADC();
   t.usb.SetVoltage(S_RO, 300);


   const char* ver = t.usb.Version();
   if (t.usb.version == 0)
      t.Characterization(abbreviated, shmoo, turbo);
//      t.TuneChip(abbreviated, shmoo, turbo);
   else if (t.usb.version == 2)
      t.HashSystem();
   else
      FATAL_ERROR;
   ta.PrintSummary(false);
   return 0;
   }



static FILE* logptr = NULL;

void ChangeLogfile(const char* filename)
   {
   if (logptr != NULL) fclose(logptr);
   logptr = fopen(filename, "at");
   if (logptr == NULL)
      {
      printf("I couldn't open %s for appending!\n",filename); 
      logptr = fopen("log.txt", "at");
      if (logptr==NULL)
         FATAL_ERROR;
      }
   }

void new_puts(const char* buffer)
   {
#ifdef _MSC_VER
/*
   wchar_t wide[1024];
   int i;
   for (i = 0; buffer[i] != 0; i++)
      wide[i] = buffer[i];
   wide[i] = 0;
   OutputDebugString(wide);*/
   OutputDebugString(buffer);
#endif
   if (logptr == NULL)
      {
      logptr = fopen("log.txt", "at");
      if (logptr == NULL) 
         { printf("I couldn't open log.txt for appending!\n"); FATAL_ERROR; }
      }
   fputs(buffer, stdout);
   fputs(buffer, logptr);
   }

void DebugBreakFunc(const char* module, int line)
   {
   FatalErrorMessage("\nFatal error in module %s line %d\n", module, line);
   }

void shutdown(int exitcode)
   {
   printf("\n");
   exit(exitcode);
   }


void FatalErrorMessage(const char* fmt, ...)
   {
   va_list args;
   char buffer[1024];

   va_start(args, fmt);
   int count = vsprintf(buffer, fmt, args);
   va_end(args);

#ifdef _MSC_VER   
   //   MessageBeep(MB_ICONEXCLAMATION);   
   //   MessageBox(NULL, buffer + (buffer[0]=='\n' ? 1:0), "Fatal Error",MB_OK|MB_ICONEXCLAMATION|MB_TASKMODAL| MB_SETFOREGROUND);
   printf("%s\n", buffer);
   DebugBreak();
#else
   printf("%s\n", buffer);
#endif
   shutdown(-1);
   };



struct time_accounttype {
   char tag[100];
   __int64 total, calls;   // total is in milliseconds
   time_accounttype()
      {
      tag[0] = 0;
      total = 0;
      calls = 0;
      }
   };

static vector <time_accounttype> accounts;

void TAtype::Account(const char* tag, int elapsed)
   {
   static bool inited = false;
   static CRITICAL_SECTION CS_timeaccount;
   int i;

   if (!inited)
      {
      inited = true;
      InitializeCriticalSection(&CS_timeaccount);
      }

   EnterCriticalSection(&CS_timeaccount);

   for (i = accounts.size() - 1; i >= 0; i--)
      if (strcmp(accounts[i].tag, tag) == 0)
         {
         accounts[i].calls++;
         accounts[i].total += elapsed;
         break;
         }

   if (i < 0)
      {
      time_accounttype temp;
      strcpy(temp.tag, tag);
      temp.calls = 1;
      temp.total = elapsed;
      accounts.push_back(temp);
      }

   LeaveCriticalSection(&CS_timeaccount);
   }


void TAtype::PrintSummary(bool concise)
   {
   int i;

   if (accounts.size() == 0) return;
   printf("\n\n______________________________________________________________________________________");

   for (i = 0; i < 1 || (i < accounts.size() && !concise); i++)
      {
      char buf2[64] = "";
      if (accounts[i].calls > 1000000000)
         sprintf(buf2, " %5.1fB iterations.", accounts[i].calls / 1000000000.0);
      else if (accounts[i].calls > 1000000)
         sprintf(buf2, " %5.1fM iterations.", accounts[i].calls / 1000000.0);
      else if (accounts[i].calls > 10000)
         sprintf(buf2, " %5.1fK iterations.", accounts[i].calls / 1000.0);
      else if (accounts[i].calls != 1)
         sprintf(buf2, " %6lld iterations.", accounts[i].calls);
      printf("\n%-50s %7.1f seconds %s", accounts[i].tag, accounts[i].total / 1000.0, buf2);
      }
   printf("\n");
   }



