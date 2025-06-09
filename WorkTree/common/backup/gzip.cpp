#include "pch.h"
#include "template.h"
#include "zlib.h"


#pragma optimize("",off)

filetype::~filetype()
   {
   if (fptr==stdout) fptr=NULL;
   if (fptr) FATAL_ERROR;
   if (gzptr) FATAL_ERROR;   
   }

void filetype::OpenStdout()
   {
   if (fptr!=NULL) FATAL_ERROR;
   fptr = stdout;
   }

bool filetype::fopen(const char *filename, const char *format, bool exitOnError)      
   {
   if (strstr(filename,".gz")==NULL)
      {
      fptr = ::fopen(filename, format);
      if (fptr == NULL && exitOnError)
         {
         printf("\nError opening '%s' (%s)\n",filename,format);
         FATAL_ERROR;
         }
      return fptr == NULL;
      }
   else
      {
      textMode = format[1]=='t';
      if (format[0]=='w')
         gzptr = gzopen(filename, "wb6");
      else if (format[0]=='r')
         gzptr = gzopen(filename, "rb6");
      else
         FATAL_ERROR;
      if (gzptr == NULL && exitOnError)
         {
         printf("\nError opening %s (%s)\n",filename,format);
         FATAL_ERROR;
         }
      return gzptr == NULL;
      }
   }

void filetype::fprintf(const char *fmt, ...)
   {
   va_list args;
   char buffer[1024];
   
   va_start(args, fmt);
   int count = vsprintf(buffer, fmt, args);
   va_end(args);
   if (strlen(buffer) >= sizeof(buffer))
      FATAL_ERROR;

   if (textMode)
      {
      char *last = buffer, *chptr;
      while ((chptr=strchr(last,'\n')) !=NULL)
         {
         *chptr=0;
         fputs(last);
         fputs("\r\n");
         last = chptr+1;
         }
      fputs(last);
      }
   else
      fputs(buffer);
   }

void filetype::fputs(const char *txt)
   {
   if (gzptr)
      gzputs(gzptr, txt);
   else if (fptr==stdout)
	  puts(txt);
   else if (fptr)
      ::fputs(txt, fptr);
   else
      FATAL_ERROR;
   }

void filetype::fclose()
   {
   if (fptr==stdout) fptr=NULL;
   if (gzptr)
      gzclose(gzptr);
   else if (fptr)
      ::fclose(fptr);
   else
      FATAL_ERROR;
   fptr  = NULL;
   gzptr = NULL;
   }

int filetype::fread(void *buffer, int size, int count)
   {
   if (gzptr)
      return gzread(gzptr, buffer, size*count)/size;
   else if (fptr)
      return ::fread(buffer, size, count, fptr);
   else
      FATAL_ERROR;
   return 0;
   }

int filetype::fwrite(const void *buffer, int size, int count)
   {
   if (gzptr)
      return gzwrite(gzptr, (void *)buffer, size*count)/size;
   else if (fptr)
      return ::fwrite(buffer, size, count, fptr);
   else
      FATAL_ERROR;
   return 0;
   }

char *filetype::fgets(char *buffer, int size)
   {
   if (gzptr)
      {
      gzgets(gzptr, buffer, size);
      if (textMode)
         {
         char *chptr = buffer;
         while ((chptr=strstr(buffer, "\r\n")))
            {
            memmove(chptr+1, chptr+2, strlen(chptr)-1);
            *chptr = '\n';
            }
         }
      if (strlen(buffer)<=0)
         return NULL;
      return buffer;
      }
   else if (fptr)
      return ::fgets(buffer, size, fptr);
   else
      FATAL_ERROR;
   return 0;
   }

