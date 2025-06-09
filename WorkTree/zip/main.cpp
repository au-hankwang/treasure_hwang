#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

//#include "z:/n3/verif/zip/common.h"
//#include "z:/n3/verif/zip/template.h"
//#include "z:/n3/verif/zip/zip.h"
//#include "../common/zlib.h"
#pragma optimize("", off)

#ifdef _MSC_VER
  typedef unsigned __uint32;
  typedef unsigned uint32_t;
  typedef unsigned uint;
  typedef unsigned __int64 __uint64;
  typedef unsigned __int64 uint64_t;
  typedef __int64 int64_t;
#endif



#include "pch.h"
#include "common.h"
#include "template.h"
#include "zip.h"
#include "zlib.h"	


int ZEXPORT compress3 (unsigned char *dest, unsigned long *destLen, unsigned char *source, unsigned long sourceLen, unsigned long dictionarylen)
   {
   z_stream stream;
   int err;
   
   stream.next_in = (Bytef*)(source+dictionarylen);
   stream.avail_in = (uInt)sourceLen - dictionarylen;
#ifdef MAXSEG_64K
   /* Check for source > 64K on 16-bit machine: */
   if ((uLong)stream.avail_in != sourceLen) return Z_BUF_ERROR;
#endif
   stream.next_out = dest;
   stream.avail_out = (uInt)*destLen;
   if ((uLong)stream.avail_out != *destLen) return Z_BUF_ERROR;
   
   stream.zalloc = (alloc_func)0;
   stream.zfree = (free_func)0;
   stream.opaque = (voidpf)0;
   
   err = deflateInit(&stream, 9); // 6=default, 9=best, 1=worst
   if (err != Z_OK) 
      return err;
   
   if (dictionarylen!=0){
      if (dictionarylen>33000)
         printf("Bad dictionary");
      err = deflateSetDictionary(&stream, source, dictionarylen);
      if (err != Z_OK) 
         return err;
      }

   err = deflate(&stream, Z_FINISH);
   if (err != Z_STREAM_END) {
      deflateEnd(&stream);
      return err == Z_OK ? Z_BUF_ERROR : err;
      }
   *destLen = stream.total_out;
   
   err = deflateEnd(&stream);
   return err;
   }


void Verify(const reftype &zipref)
   {
   array <unsigned char> zlib_input(zipref.output.size()+6);
   array <unsigned char> zlib_output(zipref.input.size()+1000);
   unsigned long zlib_outputlen = zipref.input.size()+1000;
   unsigned long zlib_inputlen  = zipref.output.size();
   int i;

   // prepend the zlib header(rfc 1950)
   zlib_input[0]=0x78;
   zlib_input[1]=0x9c;
   
   memcpy(&zlib_input[2], &zipref.output[0], zipref.output.size()); 
   zlib_inputlen+=2;
   
   zlib_input[zlib_inputlen++] = zipref.c.s.addler_out>>24;
   zlib_input[zlib_inputlen++] = zipref.c.s.addler_out>>16;
   zlib_input[zlib_inputlen++] = zipref.c.s.addler_out>>8;
   zlib_input[zlib_inputlen++] = zipref.c.s.addler_out;


//   printf("\n");
//   for (int i=0; i<zlib_inputlen; i++)
//      printf("%2x ",zlib_input[i]);

   reftype z2;
   z2.decompression = true;
   z2.input = zipref.output;
   unzip(z2);

   if (zipref.input.size() == z2.output.size())
      {
      for (i=0; i<zipref.input.size(); i++)
         {
         if (zipref.input[i]!=z2.output[i])
            {
            printf("\nzip is broke!Mismatch at byte %d",i);
//            FATAL_ERROR;
            break;
            }
         }
      }
   else
      printf("\nOcteon decompress returned different input");

   int status = uncompress(&zlib_output[0], &zlib_outputlen, &zlib_input[0], zlib_inputlen);
   if (status==Z_MEM_ERROR){
      printf("\nuncompressed returned Z_MEM_ERROR");
      FATAL_ERROR;
      }
   else if (status==Z_BUF_ERROR){
      printf("\nuncompressed returned Z_BUF_ERROR");
      FATAL_ERROR;
      }
   else if (status==Z_DATA_ERROR){
      printf("\nuncompressed returned Z_DATA_ERROR");
//      FATAL_ERROR;
      }
   else if (status!=Z_OK){
      printf("\nuncompressed did NOT return Z_OK");
      FATAL_ERROR;
      }
   else 
      {
      if (zlib_outputlen != zipref.input.size()){
         printf("\nzip is broke!\n Length mismatch, %d!=%d.",zlib_outputlen, zipref.input.size());
         FATAL_ERROR;
         }
      for (i=0; i<zipref.input.size(); i++){
         if (zipref.input[i]!=zlib_output[i])
            {
            printf("\nzip is broke!\nMismatch at byte %d",i);
            FATAL_ERROR;
            break;
            }
         }
      }

   }


const char *GetFileFromList(const char *benchmark)
   {
   const char *dir="d:\\caveo\\zip\\corpus\\";
   const char *canterbury[]={"alice29.txt", "asyoulik.txt", "cp.html", "fields.c", "grammar.lsp", "kennedy.xls", "Icet10.txt", "plrabn12.txt", "ptt5", "sum", "xargs.1", NULL};
   const char *cash[]={"zeroes.txt", "random.txt", "kennedy.xls", "E.coli", "world192.txt", "storagedata_2_to_1.dat", NULL};
   const char *f5[]={"msoffice.html", "1024k.html", "msnbc.html", NULL};
   const char *calgary[]={"bib", "book1", "book2", "geo", "news", "obj1", "obj2", "paper1", "paper2", "paper3", "paper4", "paper5", "paper6", "pic", "progc", "progl", "progp", "trans", NULL};
   const char *foundry[]={"ABC.com.htm", "amazon.htm", "cnet.htm", "cnn.htm", "facebook.htm", "google_search.htm", "high_compressible.txt", "lab.jpg", "MSN.com.htm", "nbc.htm", "Yahoo!.htm", "youtube.htm", NULL};
   const char *silesia[]={"dickens", "mozilla", "mr", "nci", "oofice", "osdb", "reymont", "samba", "sao", "webster", "xml", "x-ray", NULL};
   const char *pathological[]={"1440B_4Bpattern.hex", "match_2.txt", "match_3.txt", "match_4.txt", "match_5.txt", "match_6.txt", "match_7.txt", "match_8.txt", "match_9.txt", "match_10.txt", "match_11.txt", "match_12.txt",  NULL};
   const char *bluecoat[]={"16740333.bin", "251a340a.bin", "2b7cc8f0.bin", "31ec48d8.bin", "320ae8cc.bin", "4c0e583b.bin", "578d0607.bin", "61979bff.bin", "79ec04fe.bin", "9fecd3b3.bin", "a292873b.bin", "b50390e3.bin", "d27b472f.bin", "dca01ff2.bin", "f1faaa15.bin", "all.bin", NULL};
   const char *all[256];

   const char **files = stricmp("canterbury",benchmark)==0 ? canterbury :
                        stricmp("calgary",benchmark)==0 ? calgary :
                        stricmp("cash",benchmark)==0 ? cash : 
                        stricmp("f5",benchmark)==0 ? f5 : 
                        stricmp("foundry",benchmark)==0 ? foundry : 
                        stricmp("silesia",benchmark)==0 ? silesia :
                        stricmp("pathological",benchmark)==0 ? pathological : 
                        stricmp("bluecoat",benchmark)==0 ? bluecoat : 
                        stricmp("all",benchmark)==0 ? all : NULL;

   static char filename[256];
   static int f=0;

   if (files==NULL)
      {
      if (f!=0) 
         {
         f=0;
         return NULL;
         }
      f++;
      return benchmark;
      }
   if (files[f]==NULL)
      {
      f=0;
      return NULL;
      }
   

   strcpy(filename, dir);
   strcat(filename, files[f]);
   f++;
   return filename;
   }



void Deflate(int packet, const char *benchmark, int history=0)
   {
   int i;
   int totalout=0, ztotalout=0;
   int totalin=0;
   int worst=0;
   array <int> lens;
   array <unsigned char> worstinput;
   const char *filename=NULL;

//   FILE *dptr = fopen("corpus.c","wt");
   FILE *dptr=NULL;

   printf("Packet size is %d with %d bytes of history for %s\n",packet,history,benchmark);
   
   
   int f=0;
   while ((filename=GetFileFromList(benchmark))!=NULL)
	  {
	  reftype zipref;
      magicheadertype magicheader;
	  array <unsigned char> &input = zipref.input;
	  array <unsigned char> &compressed =  zipref.output;
	  unsigned char buffer[1024];
	  int len;
	  int zlib=0, cavium=0;

	  FILE *fptr = fopen(filename,"rb");
	  if (fptr==NULL) FATAL_ERROR;

      zipref.decompression = false;
      long originalpos = ftell(fptr);
      if (1==fread(&magicheader, sizeof(magicheader), 1, fptr))
         {
         if (magicheader.initials[0]=='D' && magicheader.initials[1]=='a' && magicheader.initials[2]=='c' && magicheader.initials[3]=='!')
            {
            zipref.force_fixed   = magicheader.options&1;
            zipref.force_dynamic = magicheader.options&2;
            zipref.compression_fastest = magicheader.options&4;
            zipref.benchmarking  = magicheader.options&8;
            zipref.historybytes  = magicheader.historybytes;
            zipref.adler_crc_in  = magicheader.adler_crc_in;
            zipref.extra_len_in  = magicheader.extra_in&0x7;
            zipref.extra_in      = magicheader.extra_len_in&0x7f;
            zipref.storage       = magicheader.options&16;
            zipref.lzs           = magicheader.options&32;
            if (zipref.force_fixed)
               zipref.force_dynamic = false;  // these two are mutually exclusive
            printf("This file had a magic header. Options=0x%x historybytes=0x%x adler/crc in=0x%x %d extrabits %s%s%s\n",(unsigned)magicheader.options,(unsigned)magicheader.historybytes,(unsigned)magicheader.adler_crc_in,zipref.extra_len_in, zipref.lzs ?"LZS ":"", zipref.storage ?"STORAGE ":"", zipref.compression_fastest ?"SPEED ":"");
            }
         else 
            fseek(fptr, originalpos, SEEK_SET);
         }
      else 
         fseek(fptr, originalpos, SEEK_SET);
      do{
         len  = fread(buffer, 1, sizeof(buffer), fptr);
         i=0;
         if (zipref.decompression)
            {
            for (i=0; i<len && zipref.output.size()<zipref.historybytes; i++)
               zipref.output.push(buffer[i]);
            }
         for (; i<len; i++)
            zipref.input.push(buffer[i]);
         } while (len==sizeof(buffer));
//         } while (len==sizeof(buffer) && zipref.input.size()<1000000);  // erase me
      fclose(fptr);


/*
	  do{
		 len  = fread(buffer, 1, sizeof(buffer), fptr);
		 for (i=0; i<len; i++)
			input.push(buffer[i]);
		 if (input.size()>250000) break; // erase me
		 } while (len==sizeof(buffer));
	  fclose(fptr);
*/	  
      if (dptr!=NULL){
         fprintf(dptr,"CVMX_SHARED_HW char data%d[%d] = {", f, input.size());
         for (i=0; i<input.size(); i++)
            {
            if (i%128==0) fprintf(dptr, "\n");
            fprintf(dptr, "0x%x%c",input[i], i<input.size()-1 ? ',' :' ');
            }
         fprintf(dptr,"};\n");
         }
//	  lens.push(input.size());

	  zipref.decompression = false;
//    zipref.force_fixed = false;
      zipref.compression_fastest = false;
      zipref.storage = false;
      zipref.lzs = false;
      zipref.lzs_compression_features = true;


	  if (packet<=0)
		 {
         zipref.adler_crc_in = 1;
		 zip(zipref);
		 totalin  += input.size();
		 totalout += compressed.size()+6;
		 cavium    = compressed.size()+6;

		 
		 array<char> zliboutput(input.size());
		 array<unsigned char> dest(input.size()+100);
		 unsigned long destlen=dest.size();
		 compress3(&dest[0], &destlen, &input[0], input.size(), 0);
//		 compress(&dest[0], &destlen, &input[0], input.size());
		 zlib = destlen;
		 ztotalout += destlen;

//         printf("\n");
//         for (i=0; i<destlen; i++)
//            printf("%2x ",dest[i]);
//         printf("\n");
//         for (i=0; i<zipref.output.size(); i++)
//            printf("%2x ",zipref.output[i]);

//         Verify(zipref);
         }
	  else {
		 int k;
		 for (i=0; i<input.size(); )
			{
			reftype z2;
			z2.decompression = false;
			z2.force_fixed = true;
			for (k=MAXIMUM(0, i-history); k<i; k++)
			   {
			   z2.input.push(input[k]);
			   z2.historybytes++;
			   }
			for (k=0; k<packet && i<input.size(); k++,i++)
			   {
			   z2.input.push(input[i]);
               totalin++;
			   }
            
            zip(z2);
            const int best = z2.output.size()+6;
			totalout += best;
			cavium   += best;

			array<char> zliboutput(z2.input.size()+100);
			array<unsigned char> dest(z2.input.size()+100);
			unsigned long destlen=dest.size();

			compress3(&dest[0], &destlen, &z2.input[0], z2.input.size(), z2.historybytes);
			ztotalout += destlen;
			zlib += destlen;

			if ((signed)z2.output.size() - (signed)destlen > worst)
			   {
			   worst = z2.output.size() - destlen;
			   worstinput = z2.input;
			   }
			}
		 }
      char *chptr, *last;
      chptr = (char *)filename;
      while ((chptr=strchr(chptr, '\\'))!=NULL)
         {
         chptr++;
         last = chptr;
         }
      printf("%20s Cavium/Zlib->%7d/%7d %5.2f/%5.2f %.3f\n", last, cavium, zlib, 1.0*input.size()/cavium, 1.0*input.size()/zlib, (float)cavium/zlib);
//      printf("%20s Cavium->%7d %5.2f\n", last, cavium, 1.0*input.size()/cavium);
      f++;
	  }
/*
   if (dptr!=NULL){
      fprintf(dptr, "char *corpus_ptrs[] = {");
      for (f=0; files[f]!=NULL; f++)
         {
         fprintf(dptr, "data%d,",f);
         }
      fprintf(dptr, "NULL};\n");
      fprintf(dptr, "char *corpus_labels[] = {");
      for (f=0; files[f]!=NULL; f++)
         {
         fprintf(dptr, "\"%s\",",files[f]);
         }
      fprintf(dptr, "NULL};\n");
      fprintf(dptr, "int corpus_lens[] = {");
      for (f=0; files[f]!=NULL; f++)
         {
         fprintf(dptr, "%d,",lens[f]);
         }
      fprintf(dptr, "0};\n");
      }*/
printf("\n");
//   printf("%20s Cavium/Zlib->%7d/%7d %5.2f/%5.2f %.3f\n\n", "Total", totalout, ztotalout, 1.0*totalin/totalout, 1.0*totalin/ztotalout, (float)totalout/ztotalout);
   
/*
   printf("Worst packet delta=%d\n",worst);
   printf("worst[%d] = {", worstinput.size());
   for (i=0; i<worstinput.size(); i++)
	  printf("0x%x,",worstinput[i]);
*/  
   }


void Worst()
   {
   unsigned char worst[256] = {0x74,0x74,0x74,0x74,0x67,0x63,0x67,0x63,0x74,0x61,0x74,0x67,0x74,0x74,0x67,0x67,0x63,0x61,0x61,0x74,0x61,0x74,0x74,0x67,0x61,0x74,0x67,0x61,0x61,0x67,0x61,0x74,0x67,0x67,0x63,0x67,0x74,0x63,0x74,0x67,0x63,0x63,0x67,0x63,0x67,0x74,0x67,0x61,0x61,0x67,0x61,0x74,0x74,0x67,0x63,0x63,0x67,0x61,0x61,0x67,0x74,0x67,0x67,0x61,0x74,0x67,0x67,0x74,0x61,0x61,0x74,0x67,0x61,0x74,0x63,0x63,0x67,0x63,0x74,0x67,0x74,0x74,0x63,0x61,0x61,0x61,0x67,0x74,0x67,0x61,0x61,0x61,0x61,0x61,0x74,0x67,0x67,0x63,0x67,0x61,0x61,0x61,0x61,0x63,0x67,0x63,0x63,0x63,0x74,0x67,0x67,0x63,0x63,0x74,0x74,0x63,0x74,0x61,0x74,0x61,0x67,0x63,0x63,0x61,0x63,0x74,0x61,0x74,0x74,0x61,0x74,0x63,0x61,0x67,0x63,0x63,0x67,0x63,0x74,0x67,0x63,0x63,0x67,0x74,0x74,0x67,0x67,0x74,0x61,0x63,0x74,0x67,0x63,0x67,0x63,0x67,0x67,0x61,0x74,0x61,0x74,0x67,0x67,0x74,0x67,0x63,0x67,0x67,0x67,0x63,0x61,0x61,0x74,0x67,0x61,0x63,0x67,0x74,0x74,0x61,0x63,0x61,0x67,0x63,0x74,0x67,0x63,0x63,0x67,0x67,0x74,0x67,0x74,0x63,0x74,0x74,0x74,0x67,0x63,0x74,0x67,0x61,0x74,0x63,0x74,0x67,0x63,0x74,0x61,0x63,0x67,0x74,0x61,0x63,0x63,0x63,0x74,0x63,0x74,0x63,0x61,0x74,0x67,0x67,0x61,0x61,0x67,0x74,0x74,0x61,0x67,0x67,0x61,0x67,0x74,0x63,0x74,0x67,0x61,0x63,0x61,0x74,0x67,0x67,0x74,0x74,0x61,0x61,0x61,0x67,0x74,0x74,0x74,0x61,0x74,0x67};
   int i;
   reftype zipref;
   array <unsigned char> &input = zipref.input;
   array <unsigned char> &compressed =  zipref.output;
	  
   for (i=0; i<256; i++)
	  input.push(worst[i]);

   zipref.force_dynamic = false;
   zip(zipref);
	  
   array<char> zliboutput(input.size()+100);
   array<unsigned char> dest(input.size());
   unsigned long destlen=dest.size();
   compress(&dest[0], &destlen, &input[0], input.size());

   printf("Cavium %d Zlib %d\n",compressed.size(), destlen);
   }

void Inflate(const char *filename)
   {
   int i;
   reftype ref;
   array <unsigned char> &input = ref.input;
   array <unsigned char> &compressed =  ref.output;
   magicheadertype magicheader;
   FILE *fptr = fopen(filename,"rb");
   char buffer[65536];
   int len;

   if (fptr==NULL) FATAL_ERROR;
   if (strstr(filename,".gz")!=NULL){
      ref.decompression = true;
      ref.lzs = false;
      fread(buffer, 10, 1, fptr);
      unsigned char flag = buffer[3];

      if (flag&8){ // skip over filename
         while (fread(buffer, 1, 1, fptr)==1)
            if (buffer[0]==0) break;
         }
      if (flag&16){ // skip over filename
         while (fread(buffer, 1, 1, fptr)==1)
            if (buffer[0]==0) break;
         }
      }
   long originalpos = ftell(fptr);
   if (1==fread(&magicheader, sizeof(magicheader), 1, fptr))
      {
      if (magicheader.initials[0]=='D' && magicheader.initials[1]=='a' && magicheader.initials[2]=='c' && magicheader.initials[3]=='!')
         {
         ref.force_fixed   = magicheader.options&1;
         ref.force_dynamic = magicheader.options&2;
         ref.compression_fastest = magicheader.options&4;
         ref.benchmarking  = magicheader.options&8;
         ref.historybytes  = magicheader.historybytes;
         ref.adler_crc_in  = magicheader.adler_crc_in;
         ref.extra_len_in  = magicheader.extra_in&0x7;
         ref.extra_in      = magicheader.extra_len_in&0x7f;
         ref.storage       = magicheader.options&16;
         ref.lzs           = magicheader.options&32;
         ref.syncflush     = magicheader.options&64;
         if (ref.force_fixed)
            ref.force_dynamic = false;  // these two are mutually exclusive
         printf("This file had a magic header. Options=0x%x historybytes=0x%x adler/crc in=0x%x %d extrabits %s%s%s EOF=%d\n",(unsigned)magicheader.options,(unsigned)magicheader.historybytes,(unsigned)magicheader.adler_crc_in,ref.extra_len_in, ref.lzs ?"LZS ":"", ref.storage ?"STORAGE ":"", ref.compression_fastest ?"SPEED ":"",ref.eof);
         }
      else 
         fseek(fptr, originalpos, SEEK_SET);
      }
   else 
      fseek(fptr, originalpos, SEEK_SET);
   do{
      len  = fread(buffer, 1, sizeof(buffer), fptr);
      i=0;
      if (ref.decompression){
         for (i=0; i<len && ref.output.size()<ref.historybytes; i++)
            ref.output.push(buffer[i]);
         }
      for (; i<len; i++)
         ref.input.push(buffer[i]);
      } while (len==sizeof(buffer) && ref.input.size()<100000);

   do{
      len  = fread(buffer, 1, sizeof(buffer), fptr);
      for (i=0; i<len; i++)
         input.push(buffer[i]);
      } while (len==sizeof(buffer));
   fclose(fptr);
	     
   unzip(ref);
   printf("Foo input=%d output=%d\n",ref.input.size(), ref.output.size());
   }




void Lzs_unzip(const char *filename)
   {
   reftype ref;
   int i, len;
   char buffer[1024];
   
/*
   FILE *fptr = fopen(filename,"rb");
   if (fptr==NULL) FATAL_ERROR;

   do{
      
      len  = fread(buffer, 1, sizeof(buffer), fptr);

      if (len>sizeof(magicheadertype) && ref.input.size()==0 && buffer[0]=='D' && buffer[1]=='a' && buffer[2]=='c' && buffer[3]=='!')
         {
         for (i=sizeof(magicheadertype); i<len; i++)
            ref.input.push(buffer[i]);
         }
      else
         {
         for (i=0; i<len; i++)
            ref.input.push(buffer[i]);
         }
      } while (len==sizeof(buffer));
   fclose(fptr);
   fptr=NULL;
*/
   unsigned char foo[48]={0x5b,0x2a,0x40,0x01,0xf0,0x7a,0xa7,0x09,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfa,
                          0x00,0x18,0xad,0xb2,0xff,0x98,0x4f,0xff,0xff,0xff,0xff,0xb1,0x76,0x57,0xf0,0x1e,
                          0xa9,0xc2,0x7f,0xff,0xff,0xff,0xff,0xf8,0xc5,0x63,0xe8,0x51,0x8e,0xf5,0xc2,0x7f};
   for (i=0; i<48; i++)
      ref.input.push(foo[i]);




   ref.lzs = true;
   ref.decompression = true;

   unzip(ref);
   for (i=0; i<ref.output.size(); i++)
      {
      if (i%16==0)
         printf("\n");
      printf("%2X ",ref.output[i]);

      }

   }


void Lzs(const char *benchmark)
   {
   const char *filename;
   int f=0;

   while ((filename=GetFileFromList(benchmark))!=NULL)
	  {
      reftype zipref, zipref2;
      array <unsigned char> &input = zipref.input;
      array <unsigned char> &compressed =  zipref.output;
      array <unsigned char> original;
      int winlzs_size;
      char buffer[1024], *chptr, *last;
      int len, i;
      int zlib=0, cavium=0;
      FILE *fptr = fopen(filename,"rb");
      if (fptr==NULL) FATAL_ERROR;

      do{
         len  = fread(buffer, 1, sizeof(buffer), fptr);

         if (len>sizeof(magicheadertype) && original.size()==0 && buffer[0]=='D' && buffer[1]=='a' && buffer[2]=='c' && buffer[3]=='!')
            {
            for (i=sizeof(magicheadertype); i<len; i++)
               original.push(buffer[i]);
            }
         else
            {
            for (i=0; i<len; i++)
               original.push(buffer[i]);
            }
         } while (len==sizeof(buffer));
      fclose(fptr);
      fptr=NULL;

      winlzs_size = 0;
      strcpy(buffer, filename);
      chptr=strchr(buffer, '.');
      if (chptr!=NULL)
         strcpy(chptr,".lzs");
      else
         strcat(buffer,".lzs");
      fptr = fopen(buffer, "rb");
      if (fptr!=NULL)
         {
         do{
            len  = fread(buffer, 1, sizeof(buffer), fptr);
            winlzs_size += len;
            } while (len==sizeof(buffer));
         fclose(fptr);
         fptr=NULL;
         }


//      original.resize(80000);
      chptr = (char *)filename;
      while ((chptr=strchr(chptr, '\\'))!=NULL)
         {
         chptr++;
         last = chptr;
         }
      printf("lzs compress of file %-20s",last);

      zipref.decompression = false;
      zipref.compression_fastest = true;
      zipref.storage = false;
      zipref.lzs = true; // erase me
      zipref.input = original;
      zip(zipref);
      printf(" %7d-> %7d %5.1f %.3f (%.0f%%)\n",original.size(), zipref.output.size(), 1.0*input.size()/zipref.output.size(), 1.0*zipref.output.size()/winlzs_size, 100.0*zipref.searches/zipref.input.size());
/*      int total=0;
      for (i=0; i<zipref.string_distribution.size(); i++)
         total += zipref.string_distribution[i];
      for (i=0; i<zipref.string_distribution.size(); i++)
         printf("%4.1f ",zipref.string_distribution[i]*100.0/total);
      printf("\n");*/
      
      zipref2.decompression = true;
      zipref2.lzs = true;
      zipref2.input = zipref.output;
      unzip(zipref2, &original);
      for (i=0; i<original.size(); i++)
         {
         if (i< zipref2.output.size() && original[i]!=zipref2.output[i])
            {
            printf("I have a output mismatch at byte %d, 0x%x!=0x%x\n",i,original[i],zipref2.output[i]);
            break;
            }
         }
      if (zipref2.output.size()!=original.size())
         printf("I have a output size mismatch in lzs decompression %d!=%d\n", zipref2.output.size(), original.size());
      }
   }


void MakeWorstCase(int len)
   {
   char filename[256];
   sprintf(filename,"match_%d.txt",len);
   FILE *fptr = fopen(filename, "wb");
   array <unsigned char> text;
   array <bool> alphabet(256, false);
   int i, k;


   printf("Making %s\n",filename);
   for (i=0; i<256; i++)
      text.push(i);

   while(text.size()<100000)
      {
      unsigned char previous = text.back();
      array <int> frequency(256, 0);

      for (i=0; i<alphabet.size(); i++)
         alphabet[i]=false;
      for (i=MAXIMUM(0, text.size()-32768); i<text.size()-1; i++)
         {
         if (text[i]==previous)
            alphabet[text[i+1]] = true;
         frequency[text[i]]++;
         }
      alphabet[previous] = true;
      int best=-1, f;
      for (i=text.size()-1; i>=MAXIMUM(0, text.size()-30000); i--)
         {
         if (!alphabet[text[i]])
            {
            if (best<0 || f>frequency[text[i]])
               {
               best = i;
               f = frequency[text[i]];
               }
            }
         if (best>=0 && i<text.size()-2048)
            break;
         }
      if (best<0)
         {
         printf("I couldn't find a solution at location %d\n",text.size());
         text.push(text.size() &0xff);
         }
      else
         {
         // found a position I can copy in with no unwanted matches
         for (k=0; k<len; k++)
            {
            text.push(text[best+k]);
            }
         }
      }
   fwrite(&text[0], 1, text.size(), fptr);
   fclose(fptr);
   }



void ContextDump(char *filename);

int main(int argc, char *argv[])
   { 
   ContextDump("z:\\pan\\good_ctx.dump");
   ContextDump("z:\\pan\\stuck_ctx.dump");
   return 0;
   ContextDump("z:\\pan\\bad_ctx.txt");
   ContextDump("z:\\pan\\good_context");
   ContextDump("z:\\pan\\good_ctx\\zip-tracker-stuck.ctx.1.dat");
   ContextDump("z:\\pan\\good_ctx\\zip-tracker-stuck.ctx.1.dat 2");
   ContextDump("z:\\pan\\stuck_ctx\\zip-tracker-stuck.ctx.0.dat");
   int i;
   for (i=2; i<=13; i++)
      {
      char buffer[256];
      sprintf(buffer, "z:\\pan\\stuck_ctx\\zip-tracker-stuck.ctx.0.dat %d",i);
      ContextDump(buffer);
      }
   return 0;


//   Inflate("z:\\junk\\brocade_compressed.gz");
/*   MakeWorstCase(2);
   MakeWorstCase(3);
   MakeWorstCase(4);
   MakeWorstCase(5);
   MakeWorstCase(6);
   MakeWorstCase(7);
   MakeWorstCase(8);
   MakeWorstCase(9);
   MakeWorstCase(10);
   MakeWorstCase(11);
   MakeWorstCase(12);
   return 0;*/
/*   FILE *fptr = fopen("z:\\foo.txt","wb");
   int i, k, seed=5;
   char buffer[4097];
   for (k=0; k<10; k++)
      {
      memset(buffer,k,sizeof(buffer));
//      for (i=0; i<sizeof(buffer); i++)
//         buffer[i] = random(seed)*256.0;
      fwrite(buffer,sizeof(buffer),1,fptr);
      }
   fclose(fptr);
   return 0;
*/
/*
//   FILE *fptr = fopen("z:\\cpfoo1.txt.gz","rb");
   reftype zipref;
   reftype unzipref;
   int i;

   zipref.force_dynamic = true;
   zipref.decompression = false;
   zipref.input.push('D');
   zipref.input.push('a');
   zipref.input.push('v');
   zipref.input.push('i');
   zipref.input.push('d');
   zipref.input.push(' ');
   zipref.input.push('i');
   zipref.input.push('s');
   zipref.input.push(' ');
   zipref.input.push('a');
   zipref.input.push('v');
   zipref.input.push('i');
   zipref.input.push('d');
   zipref.input.push('.');

   zip(zipref);

   for (i=0; i<zipref.output.size(); i++)
	  {
	  printf("%-2d 0x%x\n",i,zipref.output[i]);
	  unzipref.input.push(zipref.output[i]);
	  }
   for (i=0; i<zipref.output.size(); i++)
	  {
	  printf("0x%x,",zipref.output[i]);
	  }
   printf("\n");

   unzipref.decompression = true;
   unzipref.decompression = true;
   unzip(unzipref);
   return 0;

*/

//   Deflate(8192, "Canterbury", 0);

//   unsigned int accum=0xf6a702;
/*
   unsigned int accum=0xffff0001;

   int s1=accum&0xffff, s2=(accum>>16)&0xffff;
   s1 += 0;
   s2 += s1;
   if (s1>=65521) s1 -= 65521;
   if (s2>=65521) s2 -= 65521;
   if (s1>=65521) s1 -= 65521;
   if (s2>=65521) s2 -= 65521;
   accum = s1 | (s2<<16);
   printf("accum=%x\n",accum);
//   return 0;


   for (int i=0; i!=-1; i++)
      {
      accum=i;
      //      itu_crc32(accum, 0);
      int s1=accum&0xffff, s2=(accum>>16)&0xffff;
      s1 += 0;
      s2 += s1;
      if (s1>=65521) s1 -= 65521;
      if (s2>=65521) s2 -= 65521;
      if (s1>=65521) s1 -= 65521;
      if (s2>=65521) s2 -= 65521;
      accum = s1 | (s2<<16);

      if (accum==1)
         printf("accum=%x",i);
      }
   itu_crc32(accum, 0);
   printf("accum=%x\n",accum);
*/
   
//   Deflate(-1, "d:\\junk\\part-00000");
//   Deflate(-1, "z:\\junk\\hadoop1.bin");
//   Deflate(-1, "z:\\junk\\hadoop2.bin");
// return 0;

//   Deflate(-1, "z:\\o63\\verif\\zip\\sim\\basic\\foo3.txt");
//   Deflate(-1, "d:\\caveo\\zip\\corpus\\E.coli");
//   Deflate(-1, "d:\\caveo\\zip\\corpus\\alice29.txt");

   Deflate(-1, "pathological");
   Deflate(-1, "f5", 0);
   Deflate(-1, "canterbury", 0);
   Deflate(-1, "calgary", 0);
   Deflate(-1, "cash", 0);
   Deflate(-1, "foundry", 0);
   return 0;


//   Inflate("z:\\foo.txt.gz");
   Lzs_unzip("");
   return 0;

   Lzs("silesia");
   return 0;
//   char buffer[256];
//   gets(buffer);
   Lzs("f5");
//   Lzs("canterbury");
//   Lzs("calgary");
//   Lzs("cash");
//   Lzs("foundry");
//   return 0;

//   Lzs("d:\\caveo\\zip\\corpus\\asyoulik.txt");
//   Lzs("d:\\caveo\\zip\\corpus\\alice29.txt");
//   Lzs("d:\\caveo\\zip\\corpus\\hello.txt");
//   Lzs("canterbury");
   return 0;

   const int history=8192;
   Deflate(-1, "f5", 0);
   Deflate(-1, "canterbury", 0);
   Deflate(-1, "calgary", 0);
   Deflate(-1, "cash", 0);
   Deflate(-1, "foundry", 0);
//   Deflate(16*1024, "f5", history);
//   Deflate(8*1024, "f5", history);
//   Deflate(4*1024, "f5", history);


/*
   Deflate(256, "cash");
   Deflate(512, "cash");
   Deflate(1024, "cash");
   Deflate(1500, "cash");
   
   Deflate(256, "cash",8192);
   Deflate(512, "cash",8192);
   Deflate(1024, "cash",8192);
   Deflate(1500, "cash",8192);
   Deflate(256, "cash",16384);
   Deflate(512, "cash",16384);
   Deflate(1024, "cash",16384);
   Deflate(1500, "cash",16384);
   Deflate(256, "cash",32000);
   Deflate(512, "cash",32000);
   Deflate(1024, "cash",32000);
   Deflate(1500, "cash",32000);
*/
   return 0;
   }


#if 0
int main(int argc, char *argv[])
   {
   reftype zipref, unzipref;
   
   array <unsigned char> &input = zipref.input;
   array <unsigned char> &compressed =  zipref.output;
   unsigned char buffer[1024];
   magicheadertype magicheader;
   int i, len;

/*   for (i=0; i<32; i++){
      unsigned accum = ~(1<<i);
      printf("foo->%x",accum);
      itu_crc32(accum,0);
      itu_crc32(accum,0);
      itu_crc32(accum,0);
      itu_crc32(accum,0);
      accum = ~accum;
      printf("(in[%d] ? 32'h%x : 32'h0) ^\n",i,accum);
      }*/


//   const char *filename = "z:\\1024k.html";
//   const char *filename = "z:\\msnbc.html";
//   const char *filename = "z:\\msoffice.html";
//   const char *filename = "z:\\isscc.tif";
//   const char *filename = "z:foobar.txt.gz";
//   const char *filename = "z:foo.txt.gz";
   const char *filename = "bible.txt";
//   const char *filename = "E.coli";
//   const char *filename = "kennedy.xls";
//   const char *filename = "world192.txt";
   printf("Opening %s\n",filename);
   FILE *fptr = fopen(filename,"rb");
   if (fptr==NULL) FATAL_ERROR;

   zipref.decompression = false;
   zipref.force_fixed = false;
   zipref.force_dynamic = false;
   zipref.historybytes = 0;
   if (1==fread(&magicheader, sizeof(magicheader), 1, fptr))
      {
      if (magicheader.initials[0]=='D' && magicheader.initials[1]=='a' && magicheader.initials[2]=='c' && magicheader.initials[3]=='!')
         {
         zipref.force_fixed   = magicheader.options&1;
         zipref.force_dynamic = magicheader.options&2;
         zipref.historybytes  = magicheader.historybytes;
         printf("This file had a magic header. Options=0x%x historybytes=0x%x\n",(unsigned)magicheader.options,(unsigned)magicheader.historybytes);
         }
      else 
         fseek(fptr, 0, SEEK_SET);  // return to start of file
      }
   else
      fseek(fptr, 0, SEEK_SET);  // return to start of file

   do{
      len  = fread(buffer, 1, sizeof(buffer), fptr);
      for (i=0; i<len; i++)
         input.push(buffer[i]);
//      if (input.size()>5000) break;
      } while (len==sizeof(buffer));
   fclose(fptr);

   if (strstr(filename,".gz")==NULL){   
      zip(zipref);
      
      printf("Compressed size is %d bytes, %.2fX",zipref.output.size(), 1.0*input.size()/zipref.output.size());
      
    input.resize(100000);
      
    array<int> packet;
    int l;
    packet.push(256);
    packet.push(512);
    packet.push(1024);
    packet.push(1500);
    for (l=0; l<packet.size(); l++)
	{
	int k;
	int out=0;
	for (i=0; i<zipref.input.size(); )
	    {
	    reftype z2;
	    
//	    for (k=MAXIMUM(0, i-16000); k<i; k++)
//		z2.input.push(zipref.input[k]);
//	    z2.historybytes = zipref.input.size();
	    for (k=0; k<packet[l] && i<zipref.input.size(); k++,i++)
		{
		z2.input.push(zipref.input[i]);
		}
	    z2.decompression = false;
	    z2.force_fixed = false;
	    z2.force_dynamic = false;
	    z2.historybytes = 0;
	    if (z2.input.size()>0){
		array<unsigned char> dest(z2.input.size());
		unsigned long destlen=dest.size();
		compress(&dest[0], &destlen, &z2.input[0], z2.input.size());
		out+=destlen;
		}
	    }
	printf("\n%d ZlibCompressed size is %d bytes, %.2fX",packet[l],out, 1.0*input.size()/out);
	}


    
/*    
    for (l=0; l<packet.size(); l++)
	{
	int k;
	int out=0;
	for (i=0; i<zipref.input.size(); )
	    {
	    reftype z2;
	    
	    for (k=MAXIMUM(0, i-16000); k<i; k++)
		z2.input.push(zipref.input[k]);
	    z2.historybytes = z2.input.size();
	    for (k=0; k<packet[l] && i<zipref.input.size(); k++,i++)
		{
		z2.input.push(zipref.input[i]);
		}
	    z2.decompression = false;
	    z2.force_fixed = false;
	    z2.force_dynamic = false;
	    if (z2.input.size()>0){
		zip(z2);
		out+=z2.output.size();
		}
	    }
	printf("\n%d Compressed size is %d bytes, %.2fX",packet[l],out, 1.0*input.size()/out);
	}*/
    for (l=0; l<packet.size(); l++)
	{
	int k;
	int out=0;
	for (i=0; i<zipref.input.size(); )
	    {
	    reftype z2;
	    
//	    for (k=MAXIMUM(0, i-16000); k<i; k++)
//		z2.input.push(zipref.input[k]);
//	    z2.historybytes = zipref.input.size();
	    for (k=0; k<packet[l] && i<zipref.input.size(); k++,i++)
		{
		z2.input.push(zipref.input[i]);
		}
	    z2.decompression = false;
	    z2.force_fixed = false;
	    z2.force_dynamic = false;
	    z2.historybytes = 0;
	    if (z2.input.size()>0){
		zip(z2);
		out+=MINIMUM(z2.input.size(), z2.output.size());
		}
	    }
	printf("\n%d Compressed size is %d bytes, %.2fX",packet[l],out, 1.0*input.size()/out);
	}
      
      return 0;
      
      unsigned char * zlib_output  = (unsigned char *)malloc(input.size()+1000);
      unsigned char * zlib_input   = (unsigned char *)malloc(compressed.size()+6);
      unsigned long zlib_inputlen  = compressed.size();
      unsigned long zlib_outputlen = input.size();
      gzipheadertype gzipheader;
      
      // prepend the zlib header(rfc 1950)
      zlib_input[0]=0x68;
      zlib_input[1]=0xde;
      
      memcpy(zlib_input+2, &compressed[0], compressed.size()); 
      zlib_inputlen+=2;
      
      fptr = fopen("z:\\ref2.gz","wb");
      gzipheader.id1    = 0x1f;
      gzipheader.id2    = 0x8b;
      gzipheader.cm     = 0x08;
      gzipheader.flags  = 0x0;
      gzipheader.mtime  = 0x0;
      gzipheader.xfl    = 0x2;
      gzipheader.os     = 0xff;
      fwrite(&gzipheader, sizeof(gzipheader), 1, fptr);
      fwrite(zlib_input+2, zlib_inputlen-2, 1, fptr);
      fwrite(&zipref.crc_out, sizeof(zipref.crc_out), 1, fptr);
      fwrite(&zlib_outputlen, sizeof(zlib_outputlen), 1, fptr);
      fclose(fptr);

      zlib_input[zlib_inputlen++] = zipref.addler_out>>24;
      zlib_input[zlib_inputlen++] = zipref.addler_out>>16;
      zlib_input[zlib_inputlen++] = zipref.addler_out>>8;
      zlib_input[zlib_inputlen++] = zipref.addler_out;

      if (zipref.historybytes==0){
         int status = uncompress(zlib_output, &zlib_outputlen, zlib_input, zlib_inputlen+2);
         if (status==Z_MEM_ERROR){
            printf("\nuncompressed returned Z_MEM_ERROR");
            FATAL_ERROR;
            }
         else if (status==Z_BUF_ERROR){
            printf("\nuncompressed returned Z_BUF_ERROR");
            FATAL_ERROR;
            }
         else if (status==Z_DATA_ERROR){
            printf("\nuncompressed returned Z_DATA_ERROR");
//            FATAL_ERROR;
            }
         else if (status!=Z_OK){
            printf("\nuncompressed did NOT return Z_OK");
            FATAL_ERROR;
            }
         else 
            {
            if (zlib_outputlen != input.size()){
               printf("\nzip is broke!\n Length mismatch, %d!=%d.",zlib_outputlen, input.size());
               FATAL_ERROR;
               }
            for (i=0; i<input.size(); i++){
               if (input[i]!=zlib_output[i])
                  {
                  printf("\nzip is broke!\nMismatch at byte %d",i);
                  FATAL_ERROR;
                  break;
                  }
               }
            }
         }
      unzipref.decompression = true;
      unzipref.input = zipref.output;
      unzip(unzipref, &input);
      }
   else
      {
      unzipref.decompression = true;
      unsigned char flag = input[3];
      
      i=10;
      if (flag&8){ // skip over filename
         printf("Original filename in .gz file is %s\n",&input[i]);
         for (; i<input.size() && input[i]!=0; i++)
            ;
         i++;
         }
      if (flag&16){
         printf("Comment in .gz file is %s\n",&input[i]);
         for (; i<input.size() && input[i]!=0; i++)
            ;
         i++;
         }
         
      // whack off the last 8 bytes, its isize and crc
      for (; i<zipref.input.size()-8; i++)
         unzipref.input.push(zipref.input[i]);   // skip over gzip header
      unzipref.adler_crc_in = 1;
      
      unzip(unzipref, NULL);

      
      unsigned char * zlib_output  = (unsigned char *)malloc(1000000);
      unsigned long zlib_outputlen = 1000000;
      unsigned char * zlib_input   = (unsigned char *)malloc(unzipref.input.size()+6);
      unsigned long zlib_inputlen  = unzipref.input.size();
      // prepend the zlib header(rfc 1950)
      zlib_input[0]=0x68;
      zlib_input[1]=0xde;
      
      memcpy(zlib_input+2, &unzipref.input[0], unzipref.input.size()); 
      zlib_inputlen+=2;

      zlib_input[zlib_inputlen++] = unzipref.addler_out>>24;
      zlib_input[zlib_inputlen++] = unzipref.addler_out>>16;
      zlib_input[zlib_inputlen++] = unzipref.addler_out>>8;
      zlib_input[zlib_inputlen++] = unzipref.addler_out;
      
      int status = uncompress(zlib_output, &zlib_outputlen, zlib_input, zlib_inputlen+2);
      if (status==Z_MEM_ERROR){
         printf("\nuncompressed returned Z_MEM_ERROR");
         FATAL_ERROR;
         }
      else if (status==Z_BUF_ERROR){
         printf("\nuncompressed returned Z_BUF_ERROR");
         FATAL_ERROR;
         }
      else if (status==Z_DATA_ERROR){
         printf("\nuncompressed returned Z_DATA_ERROR");
         FATAL_ERROR;
         }
      else if (status!=Z_OK){
         printf("\nuncompressed did NOT return Z_OK");
         FATAL_ERROR;
         }
      else 
         {
         for (i=0; i<unzipref.output.size(); i++){
            if (unzipref.output[i]!=zlib_output[i])
               {
               printf("\nzip is broke!\nMismatch at byte %d",i);
               FATAL_ERROR;
               break;
               }
            }
         }
      FILE *dest = fopen("Foo.txt","wb");
      fwrite(zlib_output, 1, zlib_outputlen, dest);
      fclose(fptr);
      unsigned char buffer[100000];
      dest = fopen("z:\\foo.txt","rb");
      
      fread(buffer, 1, sizeof(buffer), dest);
      unsigned crc1=0, crc2=0;
      for (i=0; i<zlib_outputlen; i++)
         {
         itu_crc32(crc1, buffer[i]);
         itu_crc32(crc2, zlib_output[i]);
         if (zlib_output[i]!=buffer[i])
            printf("i=%d %x %x\n", i, (unsigned)zlib_output[i], (unsigned)buffer[i]);

         }
      }
           

   printf("\n");
   return 0;
   }
#endif


#undef puts
void new_puts(const char *buffer)
   {
#ifdef _MSC_VER
   OutputDebugString(buffer);
#endif
   fputs(buffer, stdout);
   }


/*
Packet size is -1 with 0 bytes of history for pathological
         match_2.txt Cavium/Zlib->  96348/  96060  1.04/ 1.04 1.003 1.$b/cycle
         match_3.txt Cavium/Zlib->  93292/  42392  1.07/ 2.36 2.201 1.$b/cycle
         match_4.txt Cavium/Zlib->  36905/  32227  2.71/ 3.10 1.145 1.$b/cycle
         match_5.txt Cavium/Zlib->  41333/  25682  2.42/ 3.89 1.609 1.$b/cycle
         match_6.txt Cavium/Zlib->  27811/  21142  3.60/ 4.73 1.315 1.$b/cycle
         match_7.txt Cavium/Zlib->  29353/  18058  3.41/ 5.54 1.625 1.$b/cycle
         match_8.txt Cavium/Zlib->  21882/  15740  4.57/ 6.35 1.390 1.$b/cycle
         match_9.txt Cavium/Zlib->  23173/  13973  4.32/ 7.16 1.658 1.$b/cycle
        match_10.txt Cavium/Zlib->  16538/  12603  6.05/ 7.94 1.312 1.$b/cycle
        match_11.txt Cavium/Zlib->  19582/  12515  5.11/ 7.99 1.565 1.$b/cycle
        match_12.txt Cavium/Zlib->  16801/  11412  5.95/ 8.76 1.472 1.$b/cycle

Packet size is -1 with 0 bytes of history for f5
       msoffice.html Cavium/Zlib->  10884/   8873  8.14/ 9.98 1.227 1.$b/cycle
          1024k.html Cavium/Zlib->   4421/   4389 22.70/22.86 1.007 1.$b/cycle
          msnbc.html Cavium/Zlib->  13085/  11080  5.25/ 6.20 1.181 1.$b/cycle

Packet size is -1 with 0 bytes of history for canterbury
         alice29.txt Cavium/Zlib->  42173/  36601  2.38/ 2.74 1.152 1.$b/cycle
        asyoulik.txt Cavium/Zlib->  44443/  39709  2.26/ 2.53 1.119 1.$b/cycle
             cp.html Cavium/Zlib->   8766/   7940  2.81/ 3.10 1.104 1.$b/cycle
            fields.c Cavium/Zlib->   3662/   3157  3.07/ 3.56 1.160 1.$b/cycle
         grammar.lsp Cavium/Zlib->   1355/   1222  2.75/ 3.05 1.109 1.$b/cycle
         kennedy.xls Cavium/Zlib->  20710/  21207  4.85/ 4.73 0.977 1.$b/cycle
          Icet10.txt Cavium/Zlib->  39752/  34966  2.52/ 2.87 1.137 1.$b/cycle
        plrabn12.txt Cavium/Zlib->  46936/  42273  2.14/ 2.37 1.110 1.$b/cycle
                ptt5 Cavium/Zlib->   6920/   6475 14.50/15.50 1.069 1.$b/cycle
                 sum Cavium/Zlib->  14376/  12838  2.66/ 2.98 1.120 1.$b/cycle
             xargs.1 Cavium/Zlib->   1862/   1736  2.27/ 2.43 1.073 1.$b/cycle

Packet size is -1 with 0 bytes of history for calgary
                 bib Cavium/Zlib->  37387/  31869  2.68/ 3.15 1.173 1.$b/cycle
               book1 Cavium/Zlib->  46634/  42095  2.15/ 2.38 1.108 1.$b/cycle
               book2 Cavium/Zlib->  40706/  36123  2.47/ 2.78 1.127 1.$b/cycle
                 geo Cavium/Zlib->  69609/  66944  1.44/ 1.50 1.040 1.$b/cycle
                news Cavium/Zlib->  44360/  40385  2.26/ 2.48 1.098 1.$b/cycle
                obj1 Cavium/Zlib->  11235/  10317  1.91/ 2.08 1.089 1.$b/cycle
                obj2 Cavium/Zlib->  43055/  38846  2.33/ 2.58 1.108 1.$b/cycle
              paper1 Cavium/Zlib->  21050/  18524  2.53/ 2.87 1.136 1.$b/cycle
              paper2 Cavium/Zlib->  33611/  29665  2.45/ 2.77 1.133 1.$b/cycle
              paper3 Cavium/Zlib->  19968/  18055  2.33/ 2.58 1.106 1.$b/cycle
              paper4 Cavium/Zlib->   5932/   5515  2.24/ 2.41 1.076 1.$b/cycle
              paper5 Cavium/Zlib->   5377/   4976  2.22/ 2.40 1.081 1.$b/cycle
              paper6 Cavium/Zlib->  14909/  13280  2.56/ 2.87 1.123 1.$b/cycle
                 pic Cavium/Zlib->   6920/   6475 14.50/15.50 1.069 1.$b/cycle
               progc Cavium/Zlib->  15284/  13410  2.60/ 2.97 1.140 1.$b/cycle
               progl Cavium/Zlib->  19119/  16146  3.75/ 4.44 1.184 1.$b/cycle
               progp Cavium/Zlib->  13250/  11168  3.73/ 4.42 1.186 1.$b/cycle
               trans Cavium/Zlib->  23004/  18916  4.07/ 4.95 1.216 1.$b/cycle

Packet size is -1 with 0 bytes of history for cash
          zeroes.txt Cavium/Zlib->    128/    119 784.00/843.29 1.076 1.$b/cycle
          random.txt Cavium/Zlib-> 100796/ 100393  1.00/ 1.00 1.004 1.$b/cycle
         kennedy.xls Cavium/Zlib->  20710/  21207  4.85/ 4.73 0.977 1.$b/cycle
              E.coli Cavium/Zlib->  32949/  29030  3.05/ 3.46 1.135 1.$b/cycle
        world192.txt Cavium/Zlib->  38987/  34539  2.57/ 2.91 1.129 1.$b/cycle
storagedata_2_to_1.dat Cavium/Zlib->  48422/  44678  2.07/ 2.25 1.084 1.$b/cycle

Packet size is -1 with 0 bytes of history for foundry
         ABC.com.htm Cavium/Zlib->   8141/   7215  3.89/ 4.39 1.128 1.$b/cycle
          amazon.htm Cavium/Zlib->  20313/  17207  4.94/ 5.83 1.181 1.$b/cycle
            cnet.htm Cavium/Zlib->  22304/  19184  4.11/ 4.78 1.163 1.$b/cycle
             cnn.htm Cavium/Zlib->  24108/  20650  4.02/ 4.69 1.167 1.$b/cycle
        facebook.htm Cavium/Zlib->   7146/   6458  3.33/ 3.68 1.107 1.$b/cycle
   google_search.htm Cavium/Zlib->   7852/   7069  3.31/ 3.68 1.111 1.$b/cycle
high_compressible.txt Cavium/Zlib->    460/    418 218.16/240.08 1.100 1.$b/cycle
             lab.jpg Cavium/Zlib->  32706/  32011  1.50/ 1.54 1.022 1.$b/cycle
         MSN.com.htm Cavium/Zlib->  16892/  14830  3.05/ 3.47 1.139 1.$b/cycle
             nbc.htm Cavium/Zlib->  16096/  13129  6.23/ 7.64 1.226 1.$b/cycle
          Yahoo!.htm Cavium/Zlib->  30835/  25933  3.25/ 3.87 1.189 1.$b/cycle
         youtube.htm Cavium/Zlib->  15652/  12903  4.71/ 5.72 1.213 1.$b/cycle
*/