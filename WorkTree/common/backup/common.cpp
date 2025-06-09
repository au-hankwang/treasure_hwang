#include "pch.h"
#include "../regex/pcre.h"

#ifdef _MSC_VER
 #pragma optimize("",off)
#endif


bool wildcardtype::Check(const char *string) const// returns true if there is a match
   {
   TAtype ta("wildcardtype::Check");
   if (simple.Check(string))
	  return true;
   if (needsCompile)
	  {
	  const char *errptr;
	  int erroffset;
	  if (code!=NULL)
		 free(code);
	  allPatterns.push(0);
	  code = pcre_compile(&allPatterns[0], (caseSensitive? 0 : PCRE_CASELESS), &errptr, &erroffset, NULL);
	  allPatterns.pop();
	  if (code==NULL)
		 FATAL_ERROR;	// we had a bad regex
	  needsCompile=false;
	  }
   if (code==NULL)
	  return false;
   int result = pcre_exec(code, NULL, string, strlen(string)+1, 0, 0, NULL, 0);
   if (result==0)
	  return true;
   if (result!=PCRE_ERROR_NOMATCH)
      FATAL_ERROR;
   return false;
   }




struct time_accounttype{
   char tag[100];
   __int64 total, calls;   // total is in milliseconds
   time_accounttype()
      {
      tag[0]=0;
      total=0;
      calls=0;
      }
   };

static array <time_accounttype> accounts;

void TAtype::Account(const char *tag, int elapsed)
   {
   static bool inited=false;
   static CRITICAL_SECTION CS_timeaccount;
   int i;
   
   if (!inited)
      {
      inited = true;
      InitializeCriticalSection(&CS_timeaccount);
      }

   EnterCriticalSection(&CS_timeaccount);

   for (i=accounts.size()-1; i>=0; i--)
      if (strcmp(accounts[i].tag, tag)==0)
         {
         accounts[i].calls++;
         accounts[i].total+= elapsed;
         break;
         }

   if (i<0)
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
   
   if (accounts.size()==0) return;
   printf("\n\n______________________________________________________________________________________");
   
   for (i=0; i<1 || (i<accounts.size() && !concise); i++)
      {
      char buf2[64]="";
      if (accounts[i].calls > 1000000000)
         sprintf(buf2," %5.1fB iterations.",accounts[i].calls/1000000000.0);
      else if (accounts[i].calls > 1000000)
         sprintf(buf2," %5.1fM iterations.",accounts[i].calls/1000000.0);
      else if (accounts[i].calls > 10000)
         sprintf(buf2," %5.1fK iterations.",accounts[i].calls/1000.0);
      else if (accounts[i].calls!=1)
         sprintf(buf2," %6d iterations.",accounts[i].calls);
      printf("\n%-50s %7.1f seconds %s",accounts[i].tag,accounts[i].total/1000.0,buf2);
      }
   printf("\n");
   }



