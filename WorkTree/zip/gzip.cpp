#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef _MSC_VER
#include <cvmx.h>
#define __int64  int64_t
#define __uint64 uint64_t
#endif



const int INPUT_SIZE    = 1048576;
const int HASH_ENTRIES  = 8192;
const int DICT_SIZE     = 32767;
//const int BLOCK_SIZE    = 8192;
const int BLOCK_SIZE    = 32768;
const int OUTPUT_SIZE   = BLOCK_SIZE*2;

#ifdef _MSC_VER
  typedef unsigned __int64 __uint64;
#endif

#define FATAL_ERROR DebugBreakFunc(__FILE__,__LINE__)


unsigned char  input[INPUT_SIZE+3];
unsigned       blocks[BLOCK_SIZE];
__uint64       hashtable[HASH_ENTRIES];
unsigned char  output[OUTPUT_SIZE];


void itu_crc32(unsigned int &accum, unsigned char byte);
unsigned crc32(unsigned data);
unsigned CLZ(__uint64 data);

void pass2(const int blocklen, unsigned *blocks, unsigned char *output, int &bit, bool final);
void DebugBreakFunc(const char *module, int line);

// this functions finds matches
void pass1(int &pos, const int len, int &blocklen)
   {
   blocklen = 0;
   int i;
   unsigned nextquad, hashindex;
   __uint64 hashvalue, newhashvalue;

   nextquad = *(unsigned *)(input+pos);
   hashindex = crc32(nextquad) & (HASH_ENTRIES-1);   
   // prefetch hashvalue = hashtable[hashindex]
   for (; pos<len && blocklen<BLOCK_SIZE; )
      {
      // this should be an unaligned load
      hashvalue = hashtable[hashindex];
      newhashvalue = (hashvalue<<16) | (pos&0x7fff);
      hashtable[hashindex] = newhashvalue;
      
      nextquad = *(unsigned *)(input+pos+1);
      hashindex = crc32(nextquad) & (HASH_ENTRIES-1);
      // prefetch hashvalue = hashtable[hashindex]

      // now I have 4 potential matches that I need to evaluate
      // I will do this using a 64b unaligned load. I compare with an XOR
      // and then use the CLZ op to determine 0-8 matches
      
      int matchlen = 0;
      int best, candidates;
      for (candidates=0; candidates<4; candidates++)
         {
         int location = (unsigned short)hashvalue | (pos&0xffff8000);
         if (location>pos)
            location -= 32768;
         __uint64 *start = (__uint64*)(input + location);
         __uint64 *ref   = (__uint64*)(input + pos);
         hashvalue = hashvalue>>16;
         if (location!=pos && location!=0)
            {
            for (i=0; i<258; )
               {
               int compare = CLZ( *start ^ *ref) >>3;
               start++;
               ref++;
               i += compare;
               if (compare!=8)
                  break;
               }
            if (i>matchlen){
               best = location;
               matchlen = i;
               }
            }
         }
      if (matchlen+pos > len)
         matchlen = len - pos;
      if (matchlen>=4 && pos-best < DICT_SIZE)
         {
         if (matchlen>258) 
            matchlen = 258;
         best = (pos - best) & 0xfffff;
         blocks[blocklen] = (matchlen<<16) | best;
         blocklen++;
         
         for (i=1; i<matchlen; i++){
            pos++;
            hashvalue = hashtable[hashindex];
            newhashvalue = (hashvalue<<16) | (pos&0x7fff);
            hashtable[hashindex] = newhashvalue;
            nextquad = *(unsigned *)(input+pos+1);
            hashindex = crc32(nextquad) & (HASH_ENTRIES-1);
            }         
         pos++;
         }
      else
         {
         blocks[blocklen] = input[pos];
         blocklen++;
         pos++;
         }
      }
   }




#pragma pack(1)
struct headertype{
   unsigned char id1;
   unsigned char id2;
   unsigned char cm;
   unsigned char flag;
   unsigned mtime;
   unsigned char xfl;
   unsigned char os;
   };

struct trailertype{
   unsigned char crc[4];
   unsigned char isize[4];
   };




void zip(const char *filename)
   {
   char buffer[256];
   strcpy(buffer, filename);
   strcat(buffer, ".gz");
   FILE *src = fopen(filename, "rb");   
   FILE *dest = fopen(buffer, "wb");
   unsigned crc = 0xffffffff;
   unsigned isize=0, osize=0;
   int i;
   __int64 starttime, endtime;
   if (src==NULL || dest==NULL)
      {
      printf("I couldn't open %s or %s!\n", filename, buffer);
      return;
      }

   headertype header;
   header.id1   = 0x1f;
   header.id2   = 0x8b;
   header.cm    = 8;
   header.flag  = 0; // no extra fields like the filename
   header.mtime = 0; // no modification time
   header.xfl   = 2; // maximum compression
   header.os    = 0; // win32 

   CVMX_MT_CRC_POLYNOMIAL(0xedb88320);

   if (sizeof(header)!=10) FATAL_ERROR;
   fwrite(&header, sizeof(header), 1, dest);

   memset(hashtable, 0, sizeof(hashtable));
   int len, pos=0, blocklen, bit=0;
   while(true)
      {
      len=fread(input, 1, INPUT_SIZE, src);

      CVMX_MF_CYCLE(starttime);
      __uint64 *ptr = (__uint64 *)input;
      CVMX_MT_CRC_IV(crc);
      for (i=0; i<len; i+=8)
         {
         CVMX_MT_CRC_DWORD(*ptr);
         ptr++;
         }
      CVMX_MT_CRC_LEN(len&7);
      CVMX_MT_CRC_VAR(*ptr);
      CVMX_MF_CRC_IV(crc);
      isize+=len;
      CVMX_MF_CYCLE(endtime);
//      return;
      printf("CRC complete. %d bytes input %d cycles.\n", len, endtime-starttime);

      int startpos = pos;
      int endpos = pos + len;
      do{
         blocklen = 0;
         
         CVMX_MF_CYCLE(starttime);
         pass1(pos, endpos, blocklen);
         CVMX_MF_CYCLE(endtime);
         printf("Pass1 complete. %d bytes input %d blocks generated %d cycles.\n",pos-startpos, blocklen, endtime-starttime);
         
         bool final = pos>=endpos && len!=INPUT_SIZE;

         CVMX_MF_CYCLE(starttime);
         pass2(blocklen, blocks, output, bit, final);
         CVMX_MF_CYCLE(endtime);
         printf("Pass2 complete. %d cycles.\n",endtime-starttime);
         if (final)
            bit+=7;   // pad to next byte
         fwrite(output, bit>>3, 1, dest);
         osize += bit>>3;
         output[0] = output[bit>>3];
         bit = bit&7;
         } while (pos < len);
      
      if (len!=INPUT_SIZE)
         break;
      pos = 32768;
      memmove(input, input+len-32768, 32768);
      }
   
   trailertype trailer;

   crc = ~crc;
   trailer.crc[0] = crc;
   trailer.crc[1] = crc>>8;
   trailer.crc[2] = crc>>16;
   trailer.crc[3] = crc>>24;
   trailer.isize[0] = isize;
   trailer.isize[1] = isize>>8;
   trailer.isize[2] = isize>>16;
   trailer.isize[3] = isize>>24;
   fwrite(&trailer,   sizeof(trailer),   1, dest);
   fclose(src);
   fclose(dest);

   printf("isize=%d osize=%d ratio=%.2f\n",isize,osize,(float)isize/osize);
   }



int main(int argc, char *argv[])
   {
   int i;

   argc=2;
#ifdef _MSC_VER
   argv[1] = "z:foo.txt";
#else
   argv[1] = "foo.txt";
#endif
   for (i=1; i<argc; i++)
      {
      zip(argv[i]);
      }
   return 0;
   }


#ifdef _MSC_VER

// I reverse the functionality of this so it will do the right thing on a LE machine
unsigned CLZ(__uint64 data)
   {
   int i;
   
   for (i=0; i<64; i++)
      {
      if ((data>>i) &1)
         break;
      }
   return i;
   }


void itu_crc32(unsigned int &accum, unsigned char byte)
   {
   int i;

   accum = ~accum;
   accum = accum ^ byte;
   for (i=0; i<8; i++)
      {
      if (accum & 1)
         accum = (accum >>1) ^ 0xedb88320;
      else
         accum = accum>>1;
      }
   accum = ~accum;
   }

#define zip_crc(a)(((a&0x1)?0xd678d:0)^((a&0x2)?0xacf1a:0)^((a&0x4)?0x48383:0)^((a&0x8)?0x90706:0)^((a&0x10)?0x20e0c:0)^((a&0x20)?0x501af:0)^((a&0x40)?0xa035e:0)^((a&0x80)?0x406bc:0)^\
                   ((a&0x100)?0xd9ab7:0)^((a&0x200)?0xa28d9:0)^((a&0x400)?0x54c05:0)^((a&0x800)?0xa980a:0)^((a&0x1000)?0x42da3:0)^((a&0x2000)?0x946f1:0)^((a&0x4000)?0x39055:0)^((a&0x8000)?0x63d1d:0)^\
                   ((a&0x10000)?0x8ac87:0)^((a&0x20000)?0x1590e:0)^((a&0x40000)?0x2b21c:0)^((a&0x80000)?0x56438:0)^((a&0x100000)?0xac870:0)^((a&0x200000)?0x590e0:0)^((a&0x400000)?0xb21c0:0)^((a&0x800000)?0x64380:0))

unsigned int crc32(unsigned data)
   {
   return zip_crc(data);
   }

#else
// now for the octane equivalents


unsigned CLZ(__uint64 data)
   {
   int i;
   
   for (i=63; i>=0; i--)
      {
      if ((data>>i) &1)
         break;
      }
   return 63-i;
   }


void itu_crc32(unsigned int &accum, unsigned char byte)
   {
   int i;

   accum = ~accum;
   accum = accum ^ byte;
   for (i=0; i<8; i++)
      {
      if (accum & 1)
         accum = (accum >>1) ^ 0xedb88320;
      else
         accum = accum>>1;
      }
   accum = ~accum;
   }

#define zip_crc(a)(((a&0x1)?0xd678d:0)^((a&0x2)?0xacf1a:0)^((a&0x4)?0x48383:0)^((a&0x8)?0x90706:0)^((a&0x10)?0x20e0c:0)^((a&0x20)?0x501af:0)^((a&0x40)?0xa035e:0)^((a&0x80)?0x406bc:0)^\
                   ((a&0x100)?0xd9ab7:0)^((a&0x200)?0xa28d9:0)^((a&0x400)?0x54c05:0)^((a&0x800)?0xa980a:0)^((a&0x1000)?0x42da3:0)^((a&0x2000)?0x946f1:0)^((a&0x4000)?0x39055:0)^((a&0x8000)?0x63d1d:0)^\
                   ((a&0x10000)?0x8ac87:0)^((a&0x20000)?0x1590e:0)^((a&0x40000)?0x2b21c:0)^((a&0x80000)?0x56438:0)^((a&0x100000)?0xac870:0)^((a&0x200000)?0x590e0:0)^((a&0x400000)?0xb21c0:0)^((a&0x800000)?0x64380:0))

unsigned int crc32(unsigned data)
   {
   return zip_crc(data);
   }

#endif
