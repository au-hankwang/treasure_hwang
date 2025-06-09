#include "pch.h"
#include "zlib.h"
#include "../bzlib/bzlib.h"


filetype::~filetype()
   {
   if (fptr==stdout) fptr=NULL;
/*   if (fptr) FATAL_ERROR;
   if (gzptr) FATAL_ERROR;   
   if (bz2ptr) FATAL_ERROR;   */
   }

void filetype::OpenStdout()
   {
   if (fptr!=NULL) FATAL_ERROR;
   fptr = stdout;
   }

bool filetype::fopen(const char *filename, const char *format, bool exitOnError)      
   {
   if (filename==NULL) FATAL_ERROR;
/*
   if (strstr(filename,".bz2")!=NULL)
      {
      printf("\nSorry but I can't read .bz2 format. Please switch to .gz(%s)\n",filename);
      FATAL_ERROR;
      }
*/
   nullOutput = false;
   if (strstr(filename,".bz2")!=NULL)
      {
      textMode = format[1]=='t';
      if (format[0]=='w')
         bz2ptr = BZ2_bzopen(filename, "wb");
      else if (format[0]=='r')
         bz2ptr = BZ2_bzopen(filename, "rb");
      else
         FATAL_ERROR;
      if (bz2ptr == NULL && exitOnError)
         {
         printf("\nError opening %s (%s)\n",filename,format);
         FATAL_ERROR;
         }
      return bz2ptr == NULL;
      }

   if (strstr(filename,".gz")!=NULL)
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

   fptr = ::fopen(filename, format);
   if (fptr == NULL && exitOnError)
      {
      printf("\nError opening '%s' (%s)\n",filename,format);
      FATAL_ERROR;
      }
   return fptr == NULL;
   }

void filetype::fprintf(const char *fmt, ...)
   {
   va_list args;
   char buffer[1024];
   
   if (nullOutput) return;
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
   if (nullOutput) return;

   if (bz2ptr)
      BZ2_bzwrite(bz2ptr, (void*)txt, strlen(txt));
   else if (gzptr)
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
   if (nullOutput) return;
   if (fptr==stdout) fptr=NULL;
   if (bz2ptr)
      BZ2_bzclose(bz2ptr);
   else if (gzptr)
      gzclose(gzptr);
   else if (fptr)
      ::fclose(fptr);
   else
      FATAL_ERROR;
   fptr  = NULL;
   gzptr = NULL;
   bz2ptr = NULL;
   }

int filetype::fread(void *buffer, int size, int count)
   {
   if (bz2ptr){
      int bzerror;
      int value = BZ2_bzRead (&bzerror, bz2ptr, buffer, size*count);
      if (bzerror!=BZ_OK && bzerror!=BZ_STREAM_END)
         return 0;
      return value / size;
      }
   else if (gzptr){
      int value= gzread(gzptr, buffer, size*count)/size;
      return value;
      }
   else if (fptr)
      return ::fread(buffer, size, count, fptr);
   else
      FATAL_ERROR;
   return 0;
   }

int filetype::fwrite(const void *buffer, int size, int count)
   {
   if (nullOutput) return 0;
   if (bz2ptr)
      {
      int bzerror;
      BZ2_bzWrite (&bzerror, bz2ptr, (void *)buffer, size*count);
      if (bzerror==BZ_OK) return count;
      else return 0;
      }
   else if (gzptr)
      return gzwrite(gzptr, (void *)buffer, size*count)/size;
   else if (fptr)
      return ::fwrite(buffer, size, count, fptr);
   else
      FATAL_ERROR;
   return 0;
   }

char *filetype::fgets(char *buffer, int size)
   {
   if (bz2ptr)
      {
      int len=0;
      int bzerror;
      while (len<size-1 && 1==BZ2_bzRead(&bzerror, bz2ptr, buffer+len, 1))
         {
         if (bzerror!=BZ_OK) break;
         if (buffer[len]=='\n')
            {
            if (len>1 && textMode && buffer[len-1]=='\r')
               {
               buffer[len-1] = '\n';
               len--;
               }
            buffer[len+1] = 0;
            return buffer;
            }
         len++;
         }
      if (len==0) return NULL;
      buffer[len+1] = 0;
      return buffer;

      if (strlen(buffer)<=0)
         return NULL;
      return buffer;
      }
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

