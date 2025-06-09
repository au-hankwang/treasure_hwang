#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

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


__int64 ReadWord(FILE *fptr)
   {
   unsigned char in;
   unsigned __int64 data;
   int i;

   data=0;
   for (i=0; i<8; i++)
      {
      fread(&in, 1, 1, fptr);
      data = (data<<8) | in;
      }
   return data;
   }

  
void ContextDump(char *filename)
   {
   FILE *fptr = fopen(filename, "rb");
   unsigned __int64 data;
   int i;

   for (i=0; i<=0x12; i+=2)
      {
      data = ReadWord(fptr);
      printf("%3x: %16I64x: ",i,data);
      if (i==0x0) printf("current=%x %x, mbz=%x, pos=%x ",(data>>8)&0xffffffff,(data>>40)&0xffffffff, (data>>3)&0x1f, data&0x7);
      if (i==0x2) printf("bigstring=%x remaining=%x fifo1=%7x fifo0=%7x ",(data>>61),(data>>52)&0x1ff, (data>>26)&0x3ffffff, data&0x3ffffff);
      if (i==0x4) printf("fifo3=%7x fifo2=%7x ",(data>>26)&0x3ffffff, data&0x3ffffff);
      if (i==0x6) printf("fifo5=%7x fifo4=%7x ",(data>>26)&0x3ffffff, data&0x3ffffff);
      if (i==0x8) printf("fifo7=%7x fifo6=%7x ",(data>>26)&0x3ffffff, data&0x3ffffff);
      if (i==0xa) printf("fifo9=%7x fifo8=%7x ",(data>>26)&0x3ffffff, data&0x3ffffff);
      if (i==0xc) printf("crc=%7x addler=%8x ",(data>>32)&0xffffffff, data&0xffffffff);
      if (i==0xe)
         {
         printf("count=%x, mbz=%x, final_block=%d, state=%x ",(data>>16)&0xffff, (data>>5)&0x7ff, (data>>4)&1, data&0xf);
         data = data>>32;
         printf("mbz=%x, error_code=%x, mbz=%d lastlength=%d, hclen=%x hdist=%x hlit=%x",data>>29, (data>>20)&0x1ff, (data>>19)&0x1, (data>>15)&0xf, (data>>10)&0x1f, (data>>5)&0x1f, (data>>0)&0x1f);
         }
      if (i==0x10)
         {
         printf("mbz=%x, stringpair=%x ",(data>>27)&0x1f, data&0x7ffffff);
         data = data>>32;
         printf("mbz=%x, bits_infligh=%x, pos_1a=%d, dflag=%x running=%x",data>>21, (data>>9)&0xfff, (data>>2)&0x7f, (data>>1)&0x1, (data>>0)&0x1);
         }
      if (i==0x12)
         {
         printf("mbz=%x, error_code=%x ",(data>>9)&0x7fffff, data&0x1ff);
         data = data>>32;
         printf("total_bit_read=%x",data);
         }
      printf("\n");
      }



   fclose(fptr);
   }
