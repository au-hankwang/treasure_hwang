/* Copyright (c) 2000-2001 Carlson Engineering Inc., 719B Fort Worth Texas 76106. All rights reserved.
   Copyright (c) 2000-2001 Compaq Computer Corporation. All rights reserved. as per aggreement #4660000596 */

#ifndef __COMMON_H_INCLUDED__
#define __COMMON_H_INCLUDED__

#define FILENAME_SIZE 64
const int MAX_INT = 0x7fffffff;
const int MIN_INT = 0x80000000;

#define FATAL_ERROR DebugBreakFunc(__FILE__,__LINE__)
void DebugBreakFunc(const char *module, int line);
void FatalErrorMessage(const char *fmt, ...);
void shutdown(int exitCode=0);
void WindowsConsoleInit();
int main(int argc, char *argv[]);
int Work(int argc, char *argv[]);
void Cleanup();


void new_puts(const char *buffer);

inline void cprintf(char *fmt, ...) // Go only to console not to gui
   {
   va_list args;
   char buffer[4096];
   
   va_start(args, fmt);
   int count = _vsnprintf(buffer, sizeof(buffer), fmt, args);
   va_end(args);
   if (strlen(buffer)>= sizeof(buffer)) FATAL_ERROR;
   puts(buffer);
   };



#define printf new_printf
#define puts   new_puts

inline void printf(char *fmt, ...) // make printf call my GUI console instead of STDOUT
   {
   va_list args;
   char buffer[4096];
   
   va_start(args, fmt);
   int count = _vsnprintf(buffer, sizeof(buffer), fmt, args);
   va_end(args);
   if (strlen(buffer)>= sizeof(buffer)) FATAL_ERROR;
   puts(buffer);
   };







// this is my time accounting type. I'll keep track how many times and for how long this object is alive
class TAtype
   {
private:
   int start, startm;
   const char *tag;

public:
   TAtype (const char *_tag) : tag(_tag)
      {
      if (tag==NULL) return;
      struct timeb timeptr;
      ftime(&timeptr);
      start  = timeptr.time;
      startm = timeptr.millitm;
//      printf("%s",tag);
      }
   ~TAtype ()
      {
      Destruct();
      }
   void Destruct()
      {
      if (tag==NULL) return;

      int elapsed;
      struct timeb timeptr;
      ftime(&timeptr);
      elapsed = (timeptr.time - start)*1000 + (timeptr.millitm - startm);
      
      Account(tag, elapsed);
      tag = NULL;
      }
   static void Account(const char *tag, int elapsed);
   static void PrintSummary(bool concise);
   static void Clear();
   };






#endif //__COMMON_H_INCLUDED__
