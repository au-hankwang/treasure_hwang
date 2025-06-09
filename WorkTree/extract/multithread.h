#ifndef __INCLUDE_MULTITHREAD_H_INCLUDED__
#define __INCLUDE_MULTITHREAD_H_INCLUDED__


#ifdef _MSC_VER
  const int DEFAULT_NUMBER_OF_THREADS = 2;
#else 
  const int DEFAULT_NUMBER_OF_THREADS = 1;
#endif

class MultiThreadtype{
  friend void thread_handler(void *param);

private:   
   int aliveCount, deadCount;
   int syncCount;                // will count the number of threads waiting at synch point
#ifdef _MSC_VER
   int syncIteration;            // a unique number for this synchronization event
#else
   pthread_cond_t  condition;
   pthread_mutex_t mutex;
#endif

protected:   
   CRITICAL_SECTION CS_general;
   int totalNumberOfThreads;
   virtual void Func(const int threadnum)=0; // the parameters for the function should be member variables of the derived class
   virtual void Prolog()               // called before Func. Only 1 thread at this point
      {}
   virtual void Epilog()               // called after Func. Only 1 thread at this point
      {}
#ifdef _MSC_VER
   void SynchronizeEveryoneHere()
      {
      int initialIteration;
      EnterCriticalSection(&CS_general);
      syncCount++;
      initialIteration = syncIteration;
      if (syncCount >= totalNumberOfThreads)
         {
         syncIteration++;         
         syncCount= 0;  // reset for next time
         }
      LeaveCriticalSection(&CS_general);
      while (syncIteration == initialIteration)
         Sleep(0);
      }
#else
   void SynchronizeEveryoneHere()
      {
      pthread_mutex_lock(&mutex);
      syncCount++;
      if (syncCount==totalNumberOfThreads-deadCount)
         {
         syncCount=0;  // reset for next time
         pthread_cond_broadcast(&condition);   // I'm the last thread so release everyone
         }
      else
         pthread_cond_wait(&condition, &mutex);
      pthread_mutex_unlock(&mutex);
      }
                
#endif

private:      
   void Constructor()
      {
      InitializeCriticalSection(&CS_general);
#ifndef _MSC_VER
      pthread_cond_init(&condition, NULL);
      pthread_mutex_init(&mutex, NULL);
#endif
      syncCount = 0;
      }

protected:
   void Lock()
      {
      EnterCriticalSection(&CS_general);
      }
   void UnLock()
      {
      LeaveCriticalSection(&CS_general);
      }

public:      
   void Launch(int numberOfThreads=DEFAULT_NUMBER_OF_THREADS);
   
   ~MultiThreadtype()
      {
      DeleteCriticalSection(&CS_general);
      }
   MultiThreadtype()
      {
      Constructor();
      }
   MultiThreadtype(const MultiThreadtype &right) // copy constructor
      {
      // don't overwrite mutexes
      Constructor();
      }
   MultiThreadtype &operator=(const MultiThreadtype &right)
      {
      return *this;
      }
   };



#endif
