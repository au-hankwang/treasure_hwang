#ifndef __GZIP_H_INCLUDED__
#define __GZIP_H_INCLUDED__

#include "zlib.h"


class filetype{
protected:
   FILE *fptr;
   void *gzptr;
   bool textMode;

public:           
   filetype()
      {
      fptr  = NULL;
      gzptr = NULL;
      textMode = false;
      }
   ~filetype();
   void OpenStdout();
   bool fopen(const char *filename, const char *format, bool exitOnError=true);  // return true on error
   void fclose();
   bool IsOpen() const
      {
      return fptr!=NULL || gzptr!=NULL;
      }
   void fputs(const char *buffertxt);
   char *fgets(char *buffer, int len);
   int fread(void *buffer, int size, int count);
   int fwrite(const void *buffer, int size, int count);
   void fprintf(const char *fmt, ...);
   };


#endif
   
