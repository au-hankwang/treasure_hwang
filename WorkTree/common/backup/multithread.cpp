/* Copyright (c) 2000-2001 Carlson Engineering Inc., 719B Fort Worth Texas 76106. All rights reserved.
   Copyright (c) 2000-2001 Compaq Computer Corporation. All rights reserved. as per aggreement #4660000596 */

#include "pch.h"
#include "multithread.h"


void thread_handler(void *param)
   {
   MultiThreadtype *base = (MultiThreadtype *)param;
   int num;
   
   EnterCriticalSection(&base->CS_general);
   num = base->aliveCount;
   base->aliveCount++;
   LeaveCriticalSection(&base->CS_general);

   base = (MultiThreadtype *)param;
   base->Func(num);

   EnterCriticalSection(&base->CS_general);
   base->deadCount++;
   LeaveCriticalSection(&base->CS_general);   
   }

#ifdef _MSC_VER
static unsigned int __stdcall raw_thread_handler(void *param)
   {      
   SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_LOWEST);
//   printf("Spawning thread");
   thread_handler(param);
   return 0;
   }
#else 
extern "C"{
   static void *raw_thread_handler(void *param)
      {
//      printf("Inside thread handler\n");
      thread_handler(param);
      return NULL;
      }
   }
#endif


#ifdef _MSC_VER
void MultiThreadtype::Launch(int numberOfThreads)
   {
   unsigned hThread=0;

   aliveCount = 0;
   deadCount = 0;
   
   totalNumberOfThreads = numberOfThreads;
   Prolog();
   for (int i=0; i<numberOfThreads-1; i++)
      if (0==_beginthreadex(NULL, 0, raw_thread_handler, this, 0, &hThread)) // use default security,stack
         {
         int error = errno;
         FATAL_ERROR;
         }

   thread_handler(this);

   // this will wait until all the spawned threads have terminated
   bool done=false;
   while(true)
      {      
      EnterCriticalSection(&CS_general);
      if (deadCount == totalNumberOfThreads)
         done = true;
      LeaveCriticalSection(&CS_general);
      if (!done)
         Sleep(10);
       else
         break;
      }

   Epilog();
   }

#else

void MultiThreadtype::Launch(int numberOfThreads)
   {
   array<pthread_t> threads(numberOfThreads);

   if (numberOfThreads<1) FATAL_ERROR;
   aliveCount = 0;
   deadCount = 0;
   
   totalNumberOfThreads = numberOfThreads;
   Prolog();
   for (int i=0; i<numberOfThreads-1; i++)
      pthread_create(&threads[i], 0, raw_thread_handler, (void*)this);

   thread_handler(this);

   for (int i=0; i<numberOfThreads-1; i++)
      pthread_join(threads[i], NULL);
   
   Epilog();
   }
#endif
