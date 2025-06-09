#ifndef __GZIP_H_INCLUDED__
#define __GZIP_H_INCLUDED__



class filetype{
protected:
   FILE *fptr;
   void *gzptr;
   void *bz2ptr;
   bool textMode;
   bool nullOutput;

public:           
   filetype()
      {
      fptr  = NULL;
      gzptr = NULL;
      bz2ptr = NULL;
      textMode = false;
      nullOutput = true;
      }
   ~filetype();
   void OpenStdout();
   void MakeNullOutput()
      {
      nullOutput = true;
      }
   bool fopen(const char *filename, const char *format, bool exitOnError=true);  // return true on error
   void fclose();
   bool IsOpen() const
      {
      return fptr!=NULL || gzptr!=NULL || bz2ptr!=NULL;
      }
   void fputs(const char *buffertxt);
   char *fgets(char *buffer, int len);
   int fread(void *buffer, int size, int count);
   int fwrite(const void *buffer, int size, int count);
   void fprintf(const char *fmt, ...);
   };


#endif
   
