#ifndef _MSC_VER

// Here are the macrox I expect to be defined for each platform
// NT                _MSC_VER
// Linux Alpha g++   __UNIX   __LINUX                 __alpha
// Linux Alpha cxx   __UNIX   __LINUX  __DECCXX_VER   __alpha
// Linux x86 g++     __UNIX   __LINUX
// DUNIX Alpha       __UNIX            __DECCXX_VER   __alpha


#include <pthread.h>
#include <unistd.h> 

#define UNIX
#define __UNIX



#ifdef __alpha
  #define __uint64 unsigned long
  #define __int64  long
#else
  #define __uint64 unsigned long long int
  #define __int64  long long int
#endif

#define _vsnprintf vsnprintf
#pragma GCC diagnostic ignored "-Wwrite-strings"

typedef pthread_mutex_t CRITICAL_SECTION;

inline void InitializeCriticalSection(CRITICAL_SECTION *cs)
   {
     //pthread_mutexattr_t attr;   
     //pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
     //pthread_mutex_init(cs, &attr);   
     pthread_mutex_init(cs, NULL);   
   }

inline void EnterCriticalSection(CRITICAL_SECTION *cs)
   {
     pthread_mutex_lock(cs);
   }

inline void LeaveCriticalSection(CRITICAL_SECTION *cs)
   {
     pthread_mutex_unlock(cs);
   }

inline void DeleteCriticalSection(CRITICAL_SECTION *cs)
   {
     pthread_mutex_destroy(cs);
   }

inline void Beep(int dwFreq, int dwDuration)
   {
   printf("\a");
   }

inline void DebugBreak()
   {
   fputs("Forcing stack dump!\a\n",stdout);
   *(char *)NULL = 0;  // cause a core dump
   }

inline void OutputDebugString(const char *string)
   {
   fputs(string, stdout);
   }

#endif

