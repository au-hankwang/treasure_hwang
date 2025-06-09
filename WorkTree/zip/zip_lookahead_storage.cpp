#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/timeb.h>
#include <time.h>
#include <math.h>
#if !defined(_MSC_VER) && !defined(__REF_MODEL_RUN__)
  #include "tb.h"
  #include "message.h"
  #include "zip_mon__tw.h"
  #include "cn_assert.h"
#endif
#ifdef _MSC_VER
  typedef unsigned __uint32;
  typedef unsigned uint;
  typedef unsigned __int64 __uint64;
  typedef unsigned __int64 uint64_t;
  typedef __int64 int64_t;
  typedef int CRITICAL_SECTION;
  void InitializeCriticalSection(int *a){};
  void DeleteCriticalSection(int *a){};
  void LeaveCriticalSection(int *a){};
  void EnterCriticalSection(int *a){};
#endif


#include "common.h"
#include "template.h"
#include "zip.h"
#ifdef __REF_MODEL_RUN__
#include "cvm_assert.h"
#endif

#ifdef _MSC_VER
// msvc doesn't support assert with a comment.    
   #undef ASSERT
   #define ASSERT MSC_ASSERT
   inline void ASSERT(bool condition, const char *comment=NULL)
      {
      if (!condition)
         {
         FATAL_ERROR;
         }
      }
#endif

int GetBitsMSB(reftype &ref, int num) 
   { // -1 on error
   int ret=0;

   if(ref.reloading_dyn_context) {
      ASSERT(!ref.c.s.get_dynamic);
      for (int i=0; i<num; i++) {
         if (ref.dyn_huff_bit >= ref.c.s.dyn_huff_bits) return(-1);
         ret = (ret<<1) | ((ref.c.s.dyn_huff_bytes[ref.dyn_huff_bit/8]>>(ref.dyn_huff_bit%8))&1);
         ref.dyn_huff_bit++;
         }
      }
   else {
      for (int i=0; i<num; i++) {
         int bit;
         if(ref.c.s.num_extra_bits) {
            bit = (ref.c.s.extra_bits >> ref.c.s.num_extra_bits) & 1;
            ref.c.s.num_extra_bits --;
            }
         else {
            if (ref.bit/8 >= ref.input.size())
               return -1;
            bit = (ref.input[ref.bit/8]>>(7-(ref.bit%8)))&1;
            ref.bit++;
            }
         if(ref.c.s.get_dynamic) {
            // save the bits in the dynamic header away in the context
            int byte = ref.c.s.dyn_huff_bits/8;
            int bitpos = ref.c.s.dyn_huff_bits & 0x7;
            ref.c.s.dyn_huff_bits++;
            int new_byte;
            if(!bitpos)
               new_byte = bit;
            else
               new_byte = ref.c.s.dyn_huff_bytes[byte] | (bit << bitpos);
            ref.c.s.dyn_huff_bytes[byte] = new_byte;
            }
         ret = (ret<<1) | bit;
         ref.total_bits_read ++;
         }
      }
   return ret;
   }


int GetBits(reftype &ref, int num) { // -1 on error
   int ret=0;

   if(ref.reloading_dyn_context) {
      ASSERT(!ref.c.s.get_dynamic);
      for (int i=0; i<num; i++) {
         if (ref.dyn_huff_bit >= ref.c.s.dyn_huff_bits) return(-1);
         ret = ret | (((ref.c.s.dyn_huff_bytes[ref.dyn_huff_bit/8]>>(ref.dyn_huff_bit%8))&1) << i);
         ref.dyn_huff_bit++;
      }
   }
   else {
      for (int i=0; i<num; i++) {
         int bit;
         if(ref.c.s.num_extra_bits) {
            bit = ref.c.s.extra_bits & 1;
            ref.c.s.extra_bits >>= 1;
            ref.c.s.num_extra_bits --;
         }
         else {
            if (ref.bit/8 >= ref.input.size())
               return -1;
            bit = (ref.input[ref.bit/8]>>(ref.bit%8))&1;
            ref.bit++;
         }
         if(ref.c.s.get_dynamic) {
            // save the bits in the dynamic header away in the context
            int byte = ref.c.s.dyn_huff_bits/8;
            int bitpos = ref.c.s.dyn_huff_bits & 0x7;
            ref.c.s.dyn_huff_bits++;
            int new_byte;
            if(!bitpos)
               new_byte = bit;
            else
               new_byte = ref.c.s.dyn_huff_bytes[byte] | (bit << bitpos);
            ref.c.s.dyn_huff_bytes[byte] = new_byte;
         }
         ret = ret | (bit << i);
         ref.total_bits_read ++;
      }
   }
   return ret;
}

int GetRBits(reftype &ref, int num) { // -1 on error
   int ret1 = GetBits(ref, num);
   if(ret1 < 0) return(ret1);
   int ret = 0;
   for (int i=0; i<num; i++) {
      int bit = (ret1 >> (num-(i+1))) & 1;
      ret |= bit << i;
   }
   return(ret);
}


void SetBitsMSB(array <unsigned char> &output, int &bit, int num, unsigned value)
   {
   int i;

   for (i=num-1; i>=0; i--)
      {
      if ((bit&7) ==0)
         output.push(0);
      if ((value>>i)&1)
         output.back() = output.back() | (0x80>>(bit%8));
      bit++;
      }
   }

// this assumes output has been zeroed previously
void SetBits(array <unsigned char> &output, int &bit, int num, unsigned value)
   {
   int i;

   for (i=0; i<num; i++)
      {
      if ((bit&7) ==0)
         output.push(0);
      if ((value>>i)&1)
         output.back() = output.back() | (1<<(bit%8));
      bit++;
      }
   }
void SetRBits(array <unsigned char> &output, int &bit, int num, unsigned value)
   {
   int i;

   for (i=num-1; i>=0; i--)
      {
      if ((bit&7) ==0)
         output.push(0);
      if ((value>>i)&1)
         output.back() = output.back() | (1<<(bit%8));
      bit++;
      }
   }


struct codetype{
   int extra;
   uint len;
   };
codetype lengths[29]={
   {0, 3},  {0, 4},  {0, 5},  {0, 6}, {0,7}, {0,8}, {0,9}, {0,10},
   {1,11},  {1,13},  {1,15},  {1,17},
   {2,19},  {2,23},  {2,27},  {2,31},
   {3,35},  {3,43},  {3,51},  {3,59},
   {4,67},  {4,83},  {4,99},  {4,115},
   {5,131}, {5,163}, {5,195}, {5,227},
   {0, 258}};

codetype distances[30]={
   {0, 1},  {0, 2},  {0, 3},  {0, 4}, 
   {1, 5},  {1, 7},  
   {2, 9},  {2,13},
   {3,17},  {3,25},
   {4,33},  {4,49},
   {5,65},  {5,97},
   {6,129}, {6,193},
   {7,257}, {7,385},
   {8,513}, {8,769},
   {9,1025}, {9,1537},
   {10,2049}, {10,3073},
   {11,4097}, {11,6145},
   {12,8193}, {12,12289},
   {13,16385}, {13,24577}};


void unzip_byte_init(reftype &ref, int inputlen, bool bof, bool eof, bool nodynamicstop) {
   ASSERT(ref.decompression);
   ref.bit = 0;
   ref.nblocks = 0;
   ref.input_so_far = 0;
   ref.inputlen = inputlen;
   ref.nblocksp = 0;
   ref.lmatchb = 0;
   ref.litHuff = NULL;
   ref.distHuff = NULL;
   ASSERT(eof || !nodynamicstop);
   ref.nodynamicstop = nodynamicstop;
   ref.reloading_dyn_context = false;
   ref.eof = eof;
   ref.unzipdone = false;
   ref.historyneeded = false;
   if(bof) {
      ref.c.s.finalblock = false;
      ref.c.s.blockdone = true;
      ref.c.s.blockstarted = false;
      ref.c.s.get_dynamic = false;
      ref.c.s.dyn_huff_bits = 0;
      ref.c.s.num_buffed_bytes_out = 0;
      ref.c.s.total_bytes_out = 0;
      ref.c.s.total_bytes_sent_out = 0;
      ref.c.s.found_eof = false;
      ref.c.s.crc_out = ref.adler_crc_in;
      ref.c.s.addler_out = ref.adler_crc_in;
      ref.total_bits_read = 0;
      ref.c.s.num_extra_bits = 0;
      ref.c.s.num_extra_bytes = 0;
      ref.c.s.dummy1 = ZIP_C_DUMMY1_VAL;
      ref.c.s.dummy2 = ZIP_C_DUMMY2_VAL;
      ref.c.s.dummy3 = ZIP_C_DUMMY3_VAL;
   }
}

void unzip_byte_with_byte(reftype &ref, unsigned char byte, const array <unsigned char> *reference, bool printit) {
   ref.input.push(byte);
   unzip_byte(ref, reference, printit);
}

void unzip_byte(reftype &ref, const array <unsigned char> *reference, bool printit) {

   ASSERT(ref.decompression);
   ref.input_so_far ++;
   ASSERT(ref.input.size() >= ref.input_so_far);

   if((ref.input_so_far == ref.inputlen) && ref.eof) {
      unzip_finishit(ref, reference, printit);
   }
   else {
      // we are not at the end -> process until we run outta data
      while(!ref.unzipdone &&
            // note the special case -> buffer more for the dynamic huffman header since we process it atomically
            ((ref.input_so_far - (ref.c.s.get_dynamic ?  UNZIP_BYTES_BUFFERED_AHEAD_DH : UNZIP_BYTES_BUFFERED_AHEAD)) >= (ref.bit / 8))
            ) {
         unzip_byte_buffered(ref, reference, printit);
      }
   }
}

void unzip_finishit(reftype &ref, const array <unsigned char> *reference, bool printit) {
   ASSERT(ref.input_so_far == ref.inputlen);
   ASSERT(ref.inputlen == ref.input.size());
#if 0
   if((ref.inputlen < 3) && printit)
         printf("\nZIP: You asked me to unzip a file with only %d bytes", ref.inputlen);
#endif
   //
   // we are at the end, so finish the processing as much as possible
   //
   // finish each input character
   //
   while(!ref.unzipdone) {
      unzip_byte_buffered(ref, reference, printit);
   }
}


void unzip_byte_buffered(reftype &ref, const array <unsigned char> *reference, bool printit) 
   {
   ASSERT(!ref.unzipdone);
   ASSERT(ref.error_out == 1);
   ASSERT((ref.output_bytes_left >= UNZIP_DYNAMIC_STOP_SIZE) || ref.nodynamicstop);
   ASSERT(ref.historybytes || !ref.historyneeded, "ZIP: decompression required history, but no history bytes provided");


   if (ref.lzs)
      { // lzs has no state so this is pretty simple
      if (ref.litHuff==NULL)
         LzsHuffman(ref); // initialize cam for lzs
      int h = ref.output.size() + ref.c.s.num_buffed_bytes_out;

      printf("bit=%d input[bit/8]=%x\n",ref.bit,ref.input[ref.bit/8]);
      int code = ref.litHuff->FetchCode(ref);
      printf("bit=%d Code=%d h=%d\n",ref.bit,code,h);
      if(code == -1) {
         ref.error_out = 4;
         ref.error_position = 200007; // stupid unique identifier to find where error happened in the code
         ref.unzipdone = true;
         }
      else if (code<256) {
#ifndef __REF_MODEL_RUN__
         ref.matches.push(matchtype(code));
#endif
         unzip_byte_out(ref, code, reference);
         }
      else if(code == 256) {
         int t = GetBitsMSB(ref, 7);
         printf(" t=%d",t);
         if(t == -1) {
            ref.error_out = 4;
            ref.error_position = 200009; // stupid unique identifier to find where error happened in the code
            ref.unzipdone = true;
            }
         if (t==0) // EOB
            {
            int throw_bits = (ref.total_bits_read&7) ? (8 - (ref.total_bits_read&7)) : 0;
            if (throw_bits!=0){
               int ret = GetBits(ref, throw_bits); // skip to next byte
               ASSERT(ret >= 0);
               }
            ref.nblocks++;
            if (ref.input.size()*8 <= ref.bit)
               ref.unzipdone = true;  // lzs doesn't have an eof indicator, just see if we're out of input data
            }
         else  // string with distance 2-127 bytes
            {
            int distance = t, length=0;
            if(h < distance) {
               ref.error_out = 7;
               ref.error_position = 100012; // stupid unique identifier to find where error happened in the code
               ref.unzipdone = true;
               return;
               }
            code = ref.distHuff->FetchCode(ref);
            if(code == -1) {
               ref.error_out = 4;
               ref.error_position = 200005; // stupid unique identifier to find where error happened in the code
               ref.unzipdone = true;
               return;
               }
            else if (code==0)
               FATAL_ERROR;
            else if (code==1)
               {  // this is the code that I've seen 8b of all 1's
               length = 23;
#ifndef __REF_MODEL_RUN__
               ref.matches.push(matchtype(23, distance));
#endif      
               do {
                  t = GetBitsMSB(ref, 4);
                  length += t;
                  if(code == -1) {
                     ref.error_out = 4;
                     ref.error_position = 200014; // stupid unique identifier to find where error happened in the code
                     ref.unzipdone = true;
                     }
#ifndef __REF_MODEL_RUN__
               if (t!=0)
                  ref.matches.push(matchtype(t, distance)); // rtl will split these up into multiple strings with same distance
#endif      
                  } while (t==0xf);
               }
            else
               {
               length = code;
#ifndef __REF_MODEL_RUN__
               ref.matches.push(matchtype(length, distance));
#endif      
               }
            //         if(printit) printf("(%d,%d)",len,distance);
            for (int i=0; i<length; i++) {
               unsigned char byte;
               if(distance <= ref.c.s.num_buffed_bytes_out) {
                  // nab from buffer
                  byte = ref.c.s.buffed_bytes_out[ref.c.s.num_buffed_bytes_out-distance];
                  }
               else
                  // nab from output
                  byte = ref.output[h-distance];
               unzip_byte_out(ref, byte, reference);
               h++;
               ASSERT(h == ref.output.size() + ref.c.s.num_buffed_bytes_out);
               }
            }
         }
      else if(code == 257) 
         {
         int t = GetBitsMSB(ref, 11);
//         printf(" t=%d",t);
         if(t == -1) {
            ref.error_out = 4;
            ref.error_position = 200019; // stupid unique identifier to find where error happened in the code
            ref.unzipdone = true;
            }
         int distance = t, length=0;
         if(h < distance) {
            ref.error_out = 7;
            ref.error_position = 100012; // stupid unique identifier to find where error happened in the code
            ref.unzipdone = true;
            printf("Error h=%d distance=%d\n",h,distance);
            return;
            }
         code = ref.distHuff->FetchCode(ref);
         if(code == -1) {
            ref.error_out = 4;
            ref.error_position = 200015; // stupid unique identifier to find where error happened in the code
            ref.unzipdone = true;
            return;
            }
         else if (code==0)
            FATAL_ERROR;
         else if (code==1)
            {  // this is the code that I've seen 8b of all 1's
            length = 23;
#ifndef __REF_MODEL_RUN__
               ref.matches.push(matchtype(23, distance));
#endif      
            do {
               t = GetBitsMSB(ref, 4);
               length += t;
               if(code == -1) {
                  ref.error_out = 4;
                  ref.error_position = 200014; // stupid unique identifier to find where error happened in the code
                  ref.unzipdone = true;
                  }
#ifndef __REF_MODEL_RUN__
               if (t!=0)
                  ref.matches.push(matchtype(t, distance)); // rtl will split these up into multiple strings with same distance
#endif      
               } while (t==0xf);
            }
         else{
            length = code;
#ifndef __REF_MODEL_RUN__
            ref.matches.push(matchtype(length, distance));
#endif
            }
         //         if(printit) printf("(%d,%d)",len,distance);
         for (int i=0; i<length; i++) 
            {
            unsigned char byte;
            if(distance <= ref.c.s.num_buffed_bytes_out) {
               // nab from buffer
               byte = ref.c.s.buffed_bytes_out[ref.c.s.num_buffed_bytes_out-distance];
               }
            else
               // nab from output
               byte = ref.output[h-distance];
            unzip_byte_out(ref, byte, reference);
            h++;
            ASSERT(h == ref.output.size() + ref.c.s.num_buffed_bytes_out);
            }
         }
      else
         ASSERT(1);
      if (ref.bit/8 == ref.input.size())
         ref.unzipdone = true;


      if(ref.unzipdone) 
         {
         // free up the things allocated for the byte-by-byte code
         if(ref.distHuff != NULL) {
            delete ref.distHuff;
            ref.distHuff = NULL;
            }
         if(ref.litHuff != NULL) {
            delete ref.litHuff;
            ref.litHuff = NULL;
            }
         ref.c.s.found_eof = true; // no real eof in lzs, this is just to make some checkers happy
         }
      if(ref.unzipdone || !(ref.nodynamicstop || (ref.output_bytes_left >= UNZIP_DYNAMIC_STOP_SIZE)) ) 
         {
         unzip_finish_run(ref, true/*doit*/, reference, printit);
         }

      return;
      }

   
   if(ref.c.s.blockdone) 
      {
      // load up the final and type bits to start processing the next block

      ref.c.s.blockdone = false;
      ref.c.s.blockstarted = true;

      ref.c.s.finalblock = GetBits(ref, 1)==1;
      if (ref.c.s.finalblock) {
         if(printit) printf("ZIP: Block #%d(final), startbyte %d ", ref.nblocks, ref.output.size() + ref.c.s.num_buffed_bytes_out - ref.historybytes);
         }
      else {
         if(printit) printf("ZIP: Block #%d, startbyte %d ", ref.nblocks, ref.output.size() + ref.c.s.num_buffed_bytes_out - ref.historybytes);
         }
      ref.nblocks++;
      int a = GetBits(ref, 2);
      if(a == -1) {
         ref.error_out = 4;
         ref.error_position = 100000; // stupid unique identifier to find where error happened in the code
         ref.unzipdone = true;
         }
      else {
         ref.c.s.unzipblocktype = (ZIP_BLOCK_TYPES) a;
         if(ref.c.s.unzipblocktype == ZIP_BLOCK_RESERVED) {
            if(printit) printf(" ERROR\n");
            ref.error_out = 5;
            ref.error_position = 100001; // stupid unique identifier to find where error happened in the code
            ref.unzipdone = true;
            }
         else if(ref.c.s.unzipblocktype == ZIP_BLOCK_NOCOMPRESS) {
            if(printit) printf(" no compression\n");
            if(ref.total_bits_read & 7) {
               int throw_bits = 8 - (ref.total_bits_read&7);
               int ret = GetBits(ref, throw_bits); // skip to next byte
               ASSERT(ret >= 0);
               }
            ref.c.s.nc_len = -1;
            ref.c.s.nc_nlen = -1;
            }
         else if(ref.c.s.unzipblocktype == ZIP_BLOCK_FIXED_HUFFMAN) {
            if(printit) printf(" fixed huffman\n");
            StaticHuffman(ref);
            }
         else {
            ASSERT(ref.c.s.unzipblocktype == ZIP_BLOCK_DYNAMIC_HUFFMAN);
            if(printit) printf(" Dynamic huffman\n");
            ref.c.s.get_dynamic = true;
            }
         }

      return;
      }

   // we are processing a block

   if(ref.c.s.unzipblocktype == ZIP_BLOCK_NOCOMPRESS) {

      if(ref.c.s.nc_len == -1) {
         // grab LEN
         ref.c.s.nc_len = GetBits(ref, 16);
         if(ref.c.s.nc_len == -1) {
            ref.error_out = 4;
            ref.error_position = 100002; // stupid unique identifier to find where error happened in the code
            ref.unzipdone = true;
            }
         }
      else if(ref.c.s.nc_nlen == -1) {
         // grab NLEN
         ref.c.s.nc_nlen = GetBits(ref, 16);
         if(ref.c.s.nc_nlen == -1) {
            ref.error_out = 4;
            ref.error_position = 100003; // stupid unique identifier to find where error happened in the code
            ref.unzipdone = true;
            }
         else if(ref.c.s.nc_len != ((~ref.c.s.nc_nlen)&0xffff)) {
            ref.error_out = 6;
            ref.error_position = 100004; // stupid unique identifier to find where error happened in the code
            ref.unzipdone = true;
            }
         else if(!ref.c.s.nc_len) {
            ref.c.s.blockdone = true;
            if(ref.c.s.finalblock) {
               ref.unzipdone = true;
               ref.c.s.found_eof = true;
               }
            }
         }
      else {
         // process a byte
         ASSERT(ref.c.s.nc_len > 0);
         int byte = GetBits(ref, 8);
         if (byte < 0) {
            ref.error_out = 4;
            ref.error_position = 100005; // stupid unique identifier to find where error happened in the code
            ref.unzipdone = true;
            }
         else {
            unzip_byte_out(ref, byte, reference);
            ref.c.s.nc_len --;
            if(!ref.c.s.nc_len) {
               ref.c.s.blockdone = true;
               if(ref.c.s.finalblock) {
                  ref.unzipdone = true;
                  ref.c.s.found_eof = true;
                  }
               }
            }
         }

      }
   else {

      if(ref.c.s.get_dynamic) {
         ASSERT(!ref.reloading_dyn_context);
         int res = DynamicHuffman(ref);
         if(res) {
            ref.error_out = res;
            // ref.error_position = 100006; // stupid unique identifier to find where error happened in the code
            ref.unzipdone = true;
            }
         ref.c.s.get_dynamic = false;
         }
      else {
         int code = ref.litHuff->FetchCode(ref);
         // if(printit) printf("\nZIP: Code = 0x%x, %c",code,(code<128 && code>=32) ? code : ' ');
         if(code == -1) {
            ref.error_out = 4;
            ref.error_position = 100007; // stupid unique identifier to find where error happened in the code
            ref.unzipdone = true;
            }
         else if (code<256) {
#ifndef __REF_MODEL_RUN__
            ref.matches.push(matchtype(code));
#endif
            unzip_byte_out(ref, code, reference);
            }
         else if(code == 256) {
            ref.c.s.blockdone = true;
            ref.c.s.dyn_huff_bits = 0;
            if(ref.c.s.finalblock) {
               ref.unzipdone = true;
               ref.c.s.found_eof = true;
               }
            }
         else if (code>285) {
            ref.error_out = 7;
            ref.error_position = 100008; // stupid unique identifier to find where error happened in the code
            ref.unzipdone = true;
            }
         else {
            ASSERT((code>=257) && (code<=285));
            int t = GetBits(ref, lengths[code-257].extra);
            if(t == -1) {
               ref.error_out = 4;
               ref.error_position = 100009; // stupid unique identifier to find where error happened in the code
               ref.unzipdone = true;
               }
            else {
               int len = lengths[code-257].len + t;
               int dcode = ref.distHuff->FetchCode(ref);
               if(dcode >29 || dcode<0) {
                  ref.error_out = 7;
                  ref.error_position = 100010; // stupid unique identifier to find where error happened in the code
                  ref.unzipdone = true;
                  }
               else {
                  int t = GetBits(ref, distances[dcode].extra);
                  if(t == -1) {
                     ref.error_out = 4;
                     ref.error_position = 100011; // stupid unique identifier to find where error happened in the code
                     ref.unzipdone = true;
                     }
                  else {
                     int distance = distances[dcode].len + t;
                     int h = ref.output.size() + ref.c.s.num_buffed_bytes_out;
                     if(h < distance) {
                        ref.error_out = 7;
                        ref.error_position = 100012; // stupid unique identifier to find where error happened in the code
                        ref.unzipdone = true;
                        }
                     else {
#ifndef __REF_MODEL_RUN__
                        ref.matches.push(matchtype(len, distance));
#endif
                        //         if(printit) printf("(%d,%d)",len,distance);
                        for (int i=0; i<len; i++) {
                           unsigned char byte;
                           if(distance <= ref.c.s.num_buffed_bytes_out) {
                              // nab from buffer
                              byte = ref.c.s.buffed_bytes_out[ref.c.s.num_buffed_bytes_out-distance];
                              }
                           else
                              // nab from output
                              byte = ref.output[h-distance];
                           unzip_byte_out(ref, byte, reference);
                           h++;
                           ASSERT(h == ref.output.size() + ref.c.s.num_buffed_bytes_out);
                           }
                        }
                     }
                  }
               }
            }
         }

      if(ref.c.s.blockdone) 
         {
         // free up the things allocated for the byte-by-byte code
         if(ref.distHuff != NULL) {
            delete ref.distHuff;
            ref.distHuff = NULL;
            }
         if(ref.litHuff != NULL) {
            delete ref.litHuff;
            ref.litHuff = NULL;
            }
         }
      }

   if(ref.unzipdone || !(ref.nodynamicstop || (ref.output_bytes_left >= UNZIP_DYNAMIC_STOP_SIZE)) ) 
      {
      unzip_finish_run(ref, true/*doit*/, reference, printit);
      }
   }






void unzip_finish_run(reftype &ref, bool doit, const array <unsigned char> *reference, bool printit) {
   if(!ref.unzipdone || doit) {
      if ((ref.error_out!=1) && printit)
         printf("ZIP: Unzip returned with error of %d, position of %d\n",ref.error_out,ref.error_position);
      if(ref.distHuff != NULL) {
         delete ref.distHuff;
         ref.distHuff = NULL;
      }
      if(ref.litHuff != NULL) {
         delete ref.litHuff;
         ref.litHuff = NULL;
      }
      if(ref.unzipdone) { // error or found_eof
         // NO CONTEXT SAVE
         if(ref.error_out==1) {
            ASSERT(ref.c.s.found_eof);
            unzip_flush_out(ref);
            if (reference!=NULL && ref.c.s.total_bytes_sent_out != reference->size()) FATAL_ERROR;
         }
         ref.bytes_in = ref.inputlen;
         ASSERT(ref.inputlen >= ref.c.s.num_extra_bytes);
         ref.eof_out = true;
      }
      else if(!(ref.nodynamicstop || (ref.output_bytes_left >= UNZIP_DYNAMIC_STOP_SIZE))) { // we are dynamic stopping
         // CONTEXT SAVE
         ASSERT(ref.error_out==1);
         ASSERT(!ref.c.s.found_eof);
         ref.bytes_in = (ref.bit + 7) / 8;
         if(ref.bytes_in < ref.c.s.num_extra_bytes) ref.bytes_in = ref.c.s.num_extra_bytes;
         ref.eof_out = false;
         ref.error_out = 3;
      }
      else { // finished all of input, haven't found eof
         // CONTEXT SAVE
         ASSERT(ref.error_out==1);
         ASSERT(!ref.c.s.found_eof);
         if(ref.eof) {
            ref.error_position = 103457; // stupid unique identifier to find where error happened in the code
            ref.error_out = 4;
         }
         ref.bytes_in = ref.inputlen;
         ASSERT(ref.inputlen >= ref.c.s.num_extra_bytes);
         ref.eof_out = false;
      }
      ref.bytes_in -= ref.c.s.num_extra_bytes;
      ASSERT(ref.bytes_in >= 0);
      ref.bytes_out = ref.output.size() - ref.historybytes;
      ref.unzipdone = true;
   }
}

void unzip(reftype &ref, const array <unsigned char> *reference) {
   int inputsize = ref.input.size();
   unzip_byte_init(ref, inputsize, true/*bof*/, true/*eof*/, true/*nodynamicstop*/);
   
   do {
      unzip_byte(ref, reference);
   } while(!ref.unzipdone);
}

int DynamicHuffman(reftype &ref) {
   ASSERT(ref.c.s.unzipblocktype == ZIP_BLOCK_DYNAMIC_HUFFMAN);
   int i;

   int hlit = GetBits(ref, 5);
   if(hlit < 0) {
      ref.error_position = 100100; // stupid unique identifier to find where error happened in the code
      return(4);
   }
   hlit += 257;
   ASSERT(ref.litHuff == NULL);
   ref.litHuff = new huffmantype(hlit);
   ASSERT(ref.litHuff != NULL);

   int hdist = GetBits(ref, 5);
   if(hdist < 0) {
      ref.error_position = 100101; // stupid unique identifier to find where error happened in the code
      return(4);
   }
   hdist += 1;
   ASSERT(ref.distHuff == NULL);
   ref.distHuff = new huffmantype(hdist);
   ASSERT(ref.distHuff != NULL);

   int hclen = GetBits(ref, 4);
   if(hclen < 0) {
      ref.error_position = 100102; // stupid unique identifier to find where error happened in the code
      return(4);
   }
   hclen += 4;

   huffmantype codelens(19);

   char codeindirection[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
   for(i = 0; i < hclen; i++) {
      int g = GetBits(ref, 3);
      if(g < 0) {
         ref.error_position = 100103; // stupid unique identifier to find where error happened in the code
         return(4);
      }
      codelens[codeindirection[i]].len = g;
   }
   if (codelens.Transform())
	  return 8;

#ifndef __REF_MODEL_RUN__
   decompblocktype db;
   for (i=0; i<19; i++) {
      db.alphacodes[i].code = codelens[i].code;
      db.alphacodes[i].len  = codelens[i].len;
   }
#endif

   int last = -1;
   int spillover = 0;
   for(i = 0; i < hlit; ) {
      spillover = 0;
      int code = codelens.FetchCode(ref);
      if (code < 0) {
         ref.error_position = 100104; // stupid unique identifier to find where error happened in the code
         return(4);
		 }
      if (codelens[code].len==0) FATAL_ERROR;
      if (code<=15) {
         last = (*ref.litHuff)[i].len = code;
         i++;
		 }
      else if (code==16) {
         int t = GetBits(ref, 2);
         if(t < 0) {
            ref.error_position = 100105; // stupid unique identifier to find where error happened in the code
            return(4);
			}
		 if (i==0){
            return(8);
			}
			
         int l = 3 + t;
         int k;
         for (k=0; k<l && i<hlit; k++,i++)
            last = (*ref.litHuff)[i].len = (*ref.litHuff)[i-1].len;
         spillover = l-k;
		 }
      else if (code==17) {
         int t = GetBits(ref, 3);
         if(t < 0) {
            ref.error_position = 100106; // stupid unique identifier to find where error happened in the code
            return(4);
			}
         int l = 3 + t;
         int k;
         for (k=0; k<l && i<hlit; k++,i++)
            last = (*ref.litHuff)[i].len = 0;
         spillover = l-k;
		 }
      else if (code==18) {
         int t = GetBits(ref, 7);
         if(t < 0) {
            ref.error_position = 100107; // stupid unique identifier to find where error happened in the code
            return(4);
			}
         int l = 11 + t;
         int k;
         for (k=0; k<l && i<hlit; k++,i++)
            last = (*ref.litHuff)[i].len = 0;
         spillover = l-k;
		 }
   }
//   for (i=0; i<hlit; i++)
//      if(printit) printf("\nZIP: LitHuff %d %d",i,litHuff[i].len);
   for (i=0; i<spillover; i++){
	  if (i >= (*ref.distHuff).Size())
		 return 8;
      (*ref.distHuff)[i].len = last;
	  }

   for(i = spillover; i < hdist; ) {
      spillover = 0;
      int code = codelens.FetchCode(ref);
      if (code < 0) {
         ref.error_position = 100108; // stupid unique identifier to find where error happened in the code
         return(4);
      }
      else if (code<=15) {
         last = (*ref.distHuff)[i].len = code;
         i++;
      }
      else if (code==16) {
         int t = GetBits(ref, 2);
         if(t < 0) {
            ref.error_position = 100109; // stupid unique identifier to find where error happened in the code
            return(4);
         }
         int l = 3 + t;
         int k;
         for (k=0; k<l && i<hdist; k++,i++)
            (*ref.distHuff)[i].len = last;
         spillover = l-k;
      }
      else if (code==17) {
         int t = GetBits(ref, 3);
         if(t < 0) {
            ref.error_position = 100110; // stupid unique identifier to find where error happened in the code
            return(4);
         }
         int l = 3 + t;
         int k;
         for (k=0; k<l && i<hdist; k++,i++)
            last = (*ref.distHuff)[i].len = 0;
         spillover = l-k;
      }
      else if (code==18) {
         int t = GetBits(ref, 7);
         if(t < 0) {
            ref.error_position = 100111; // stupid unique identifier to find where error happened in the code
            return(4);
         }
         int l = 11 + t;
         int k;
         for (k=0; k<l && i<hdist; k++,i++)
            last = (*ref.distHuff)[i].len = 0;
         spillover = l-k;
      }
   }
   if(spillover) {
      ref.error_position = 100112; // stupid unique identifier to find where error happened in the code
      return(8);
   }

   if (ref.litHuff->Transform())
	  return 7;
   if (ref.distHuff->Transform())
	  return 7;

#ifndef __REF_MODEL_RUN__
   for (i=0; i<hlit; i++)
      db.recodes.push(recodetype((*ref.litHuff)[i].code, (*ref.litHuff)[i].len));
   for (i=hlit; i<288; i++)
      db.recodes.push(recodetype(0, 0));
   for (i=0; i<hdist; i++)
      db.recodes.push(recodetype((*ref.distHuff)[i].code, (*ref.distHuff)[i].len));
   for (i=hdist; i<32; i++)
      db.recodes.push(recodetype(0, 0));

   ref.decompblocks.push(db);
#endif
   return(0);
}

void StaticHuffman(reftype &ref) {
   ASSERT(ref.c.s.unzipblocktype == ZIP_BLOCK_FIXED_HUFFMAN);
   ASSERT(ref.litHuff == NULL);
   ref.litHuff = new huffmantype(288);
   ASSERT(ref.litHuff != NULL);
   ASSERT(ref.distHuff == NULL);
   ref.distHuff = new huffmantype(32);
   ASSERT(ref.distHuff != NULL);
   int i;
   for (i=0; i<=143; i++)
      (*(ref.litHuff))[i].len = 8;
   for (i=144; i<=255; i++)
      (*(ref.litHuff))[i].len = 9;
   for (i=256; i<=279; i++)
      (*(ref.litHuff))[i].len = 7;
   for (i=280; i<=287; i++)
      (*(ref.litHuff))[i].len = 8;
   for (i=0; i<=31; i++)
      (*(ref.distHuff))[i].len = 5;
   ref.litHuff->Transform();
   ref.distHuff->Transform();
#ifndef __REF_MODEL_RUN__
   decompblocktype db;
   for (i=0; i<288; i++)
      db.recodes.push(recodetype((*ref.litHuff)[i].code, (*ref.litHuff)[i].len));
   for (i=0; i<32; i++)
      db.recodes.push(recodetype((*ref.distHuff)[i].code, (*ref.distHuff)[i].len));
   ref.decompblocks.push(db);
#endif
}

void LzsHuffman(reftype &ref) {
   ASSERT(ref.lzs);
   ASSERT(ref.litHuff == NULL);
//   ref.litHuff = new huffmantype(258);
   ref.litHuff = new huffmantype(288);
   ASSERT(ref.litHuff != NULL);
   ASSERT(ref.distHuff == NULL);
//   ref.distHuff = new huffmantype(7);
   ref.distHuff = new huffmantype(32);
   ASSERT(ref.distHuff != NULL);
   int i;
   for (i=0; i<256; i++)
      {
      (*(ref.litHuff))[i].len = 9;
      (*(ref.litHuff))[i].code = i;
      }
   (*(ref.litHuff))[256].len = 2;         // 2+7 The 7 extra bits are 0=eob, 1-127=string distance
   (*(ref.litHuff))[256].code = 0x3;
   (*(ref.litHuff))[257].len = 2;         // 2+11 The 11 extra bits are distance 128-2048
   (*(ref.litHuff))[257].code = 0x2;
   for (i=258; i<288; i++)
      {
      (*(ref.litHuff))[i].len = 0;
      (*(ref.litHuff))[i].code = 0;
      }

// lzs did a sorry job with the length code. The can't entirely contained in the cam, lengths 2-22 will be in the cam, the rest will require iteration
   (*(ref.distHuff))[0].len = 0;
   (*(ref.distHuff))[0].code = 0x0;
   (*(ref.distHuff))[1].len = 8;
   (*(ref.distHuff))[1].code = 0xff;

   (*(ref.distHuff))[2].len = 2;
   (*(ref.distHuff))[2].code = 0x0; // 2
   (*(ref.distHuff))[3].len = 2;
   (*(ref.distHuff))[3].code = 0x1; // 3
   (*(ref.distHuff))[4].len = 2;
   (*(ref.distHuff))[4].code = 0x2; // 4
   (*(ref.distHuff))[5].len = 4;
   (*(ref.distHuff))[5].code = 0xc; // 5
   (*(ref.distHuff))[6].len = 4;
   (*(ref.distHuff))[6].code = 0xd; // 6
   (*(ref.distHuff))[7].len = 4;
   (*(ref.distHuff))[7].code = 0xe; // 7
   for (i=8; i<=22; i++)
      {
      (*(ref.distHuff))[i].len = 8;
      (*(ref.distHuff))[i].code = 0xf0 | (i-8);
//      (*(ref.distHuff))[i].code = 0xf | ((i-8)<<4);
      }
   for (; i<32; i++)
      {
      (*(ref.distHuff))[i].len = 0;
      (*(ref.distHuff))[i].code = 0;
      }


#ifndef __REF_MODEL_RUN__
   decompblocktype db;
   for (i=0; i<ref.litHuff->num; i++)
      db.recodes.push(recodetype((*ref.litHuff)[i].code, (*ref.litHuff)[i].len));
   for (i=0; i<ref.distHuff->num; i++)
      db.recodes.push(recodetype((*ref.distHuff)[i].code, (*ref.distHuff)[i].len));
   ref.decompblocks.push(db);
#endif
   }
	



void unzip_adlercrc32(reftype &ref, unsigned char byte) {
   int s1=ref.c.s.addler_out&0xffff, s2=(ref.c.s.addler_out>>16)&0xffff;
   s1 += byte;
   s2 += s1;
   if (s1>=65521) s1 -= 65521;
   if (s2>=65521) s2 -= 65521;
   if (s1>=65521) s1 -= 65521;
   if (s2>=65521) s2 -= 65521;
   ref.c.s.addler_out = s1 | (s2<<16);
   itu_crc32(ref.c.s.crc_out, byte);
}

// returns the position of the byte in a virtual output stream
void unzip_byte_out(reftype &ref, unsigned char byte, const array <unsigned char> *reference) {
   unzip_adlercrc32(ref, byte);
   if (reference!=NULL && byte != (*reference)[ref.c.s.total_bytes_out]) FATAL_ERROR;
   ref.c.s.total_bytes_out ++;
   ASSERT(ref.c.s.num_buffed_bytes_out < UNZIP_BUFFED_BYTES);
   ref.c.s.buffed_bytes_out[ref.c.s.num_buffed_bytes_out] = byte;
   ref.c.s.num_buffed_bytes_out++;
   if(ref.c.s.num_buffed_bytes_out == UNZIP_BUFFED_BYTES)
      unzip_flush_out(ref);
}

void unzip_flush_out(reftype &ref) {
   for(int i = 0; i < ref.c.s.num_buffed_bytes_out; i++) {
      unsigned char byte = ref.c.s.buffed_bytes_out[i];
      ref.output.push(byte);
      ref.output_bytes_left --;
      ref.c.s.total_bytes_sent_out ++;
   }
   ref.c.s.num_buffed_bytes_out = 0;
   ASSERT(ref.c.s.total_bytes_out == ref.c.s.total_bytes_sent_out);
   ASSERT((ref.output_bytes_left >= 0) || ref.nodynamicstop);
   ASSERT(ref.output.size() - ref.historybytes <= ref.c.s.total_bytes_out);
}

void unzip_restore(reftype &ref) {
   ref.total_bits_read = ref.c.s.total_bits_read;
   ASSERT((ref.c.s.dummy1 == ZIP_C_DUMMY1_VAL) && (ref.c.s.dummy2 == ZIP_C_DUMMY2_VAL) && (ref.c.s.dummy3 == ZIP_C_DUMMY3_VAL),
         "ZIP: Reloaded context state (for decompression) is corrupted");
   // ASSERT(!ref.unzipdone, "ZIP: Reloaded context state (for decompression) is bad");
   // restore all necessary context from the ref.c structure
   if(!ref.c.s.blockdone) {
      if(ref.c.s.unzipblocktype == ZIP_BLOCK_FIXED_HUFFMAN) {
         ASSERT(!ref.c.s.get_dynamic, "ZIP: Reloaded context state (for decompression) is corrupted");
         StaticHuffman(ref);
      }
      else if(ref.c.s.unzipblocktype == ZIP_BLOCK_DYNAMIC_HUFFMAN) {
         if(!ref.c.s.get_dynamic) {
            ref.reloading_dyn_context = true;
            ref.dyn_huff_bit = 0;
            int ret = DynamicHuffman(ref);
            ref.reloading_dyn_context = false;
            ASSERT((ref.dyn_huff_bit == ref.c.s.dyn_huff_bits) && !ret, "ZIP: Reloaded context state (for decompression) is corrupted");
         }
      }
      else {
         ASSERT(!ref.c.s.get_dynamic, "ZIP: Reloaded context state (for decompression) is corrupted");
         ASSERT(ref.c.s.unzipblocktype == ZIP_BLOCK_NOCOMPRESS, "ZIP: Reloaded context state (for decompression) is corrupted");
      }
   }
   else {
      ASSERT(!ref.c.s.get_dynamic, "ZIP: Reloaded context state (for decompression) is corrupted");
   }
   ASSERT(!ref.c.s.found_eof, "ZIP:Reloaded unzip context is corrupt - found eof");
   ASSERT(ref.c.s.num_buffed_bytes_out < UNZIP_BUFFED_BYTES,
         "ZIP:Reloaded unzip context is corrupt - num_buffed_bytes_out too large");
   ASSERT(ref.c.s.num_extra_bytes + ((ref.c.s.dyn_huff_bits+7)/8) <= (MAX_DYN_HUFF_BYTES+UNZIP_BYTES_BUFFERED_AHEAD+1),
         "ZIP:Reloaded unzip context is corrupt - num_extra_bytes too large");
   ASSERT(ref.c.s.total_bytes_sent_out + ref.c.s.num_buffed_bytes_out == ref.c.s.total_bytes_out, "ZIP:Reloaded unzip context is corrupt - bad byte counts");
   ASSERT(!ref.input.size());
   ref.inputlen += ref.c.s.num_extra_bytes;
   ASSERT(!ref.input_so_far);
   ref.input_so_far += ref.c.s.num_extra_bytes;
   unsigned char *starting_address = &(ref.c.s.extra_bytes[UNZIP_BYTES_BUFFERED_AHEAD+1]) - ref.c.s.num_extra_bytes;
   for(int i = 0; i < ref.c.s.num_extra_bytes; i++) {
      ref.input.push(starting_address[i]);
   }
   ref.historyneeded = (ref.c.s.total_bytes_sent_out != 0);
}

void unzip_save(reftype &ref) {
   ref.c.s.total_bits_read = ref.total_bits_read;
   ASSERT(!ref.bit || !ref.c.s.num_extra_bits);
   int bits_used = ref.bit & 0x7;
   if(bits_used) {
      ref.c.s.num_extra_bits = 8 - bits_used;
      ref.c.s.extra_bits = ref.input[ref.bit/8] >> bits_used;
   }
   ASSERT(!ref.nodynamicstop); // to be saving, you must have had to opportunity to dynamic stop
   if(ref.error_out != 3) { // if we did not dynamic stop
      // save all input bytes
      int bytes_used = (ref.bit + 7) / 8;
      ref.c.s.num_extra_bytes = ref.inputlen - bytes_used;
      ASSERT(ref.c.s.num_extra_bytes + ((ref.c.s.dyn_huff_bits+7)/8) <= (MAX_DYN_HUFF_BYTES+UNZIP_BYTES_BUFFERED_AHEAD+1));
      unsigned char *starting_address = &(ref.c.s.extra_bytes[UNZIP_BYTES_BUFFERED_AHEAD+1]) - ref.c.s.num_extra_bytes;
      for(int i = bytes_used; i < ref.inputlen; i++) {
         starting_address[i-bytes_used] = ref.input[i];
      }
   }
   else { // we did dynamic stop
      ASSERT(!(ref.output_bytes_left >= UNZIP_DYNAMIC_STOP_SIZE));
      int bytes_used = (ref.bit + 7) / 8;
      // check if we used less than we restored
      if(bytes_used < ref.c.s.num_extra_bytes) {
         // only save those input bytes that are absolutely necessary - the remainder of those we restored
         unsigned char *starting_address = &(ref.c.s.extra_bytes[UNZIP_BYTES_BUFFERED_AHEAD+1]) - ref.c.s.num_extra_bytes;
         for(int i = bytes_used; i < ref.c.s.num_extra_bytes; i++) {
            ASSERT(starting_address[i] == ref.input[i]);
         }
         ref.c.s.num_extra_bytes -= bytes_used;
      }
      else
         ref.c.s.num_extra_bytes = 0;
   }
   // all the necessary context is in the ref.c structure now
}

void BlockStuff(array <uint64_t> &recodes, array <unsigned char> &temp, int &tempbit)
   {
   temp.push(0); // push a bunch of zeroes onto the array so I don't have to be careful with where the array ends
   temp.push(0);
   temp.push(0);
   temp.push(0);
   temp.push(0);
   temp.push(0);
   recodes.push(temp[0] | ((__int64)temp[1]<<8) | ((__int64)temp[2]<<16) | ((__int64)temp[3]<<24) | ((__int64)temp[4]<<32) | ((__int64)temp[5]<<40) | ((__int64)temp[6]<<48) | ((__int64)tempbit<<56));
   temp.clear();
   tempbit = 0;
   }

ZIP_BLOCK_TYPES OutputBlock(reftype &ref, array <unsigned char> &output, int &bit, int rawsize, array <matchtype> &matches, blocktype *blockp, bool final, bool force_fixed, bool force_dynamic, bool lzs, const unsigned char *allow_nocompression, int matchbundles, bool printit=false);
ZIP_BLOCK_TYPES OutputBlock(reftype &ref, array <unsigned char> &output, int &bit, int rawsize, array <matchtype> &matches, blocktype *blockp, bool final, bool force_fixed, bool force_dynamic, bool lzs, const unsigned char *allow_nocompression, int matchbundles, bool printit) {
   huffmantype litHuff(288), distHuff(32);
   bool block_present = (blockp != NULL);
   blocktype &block = *blockp;
   int i;
   ZIP_BLOCK_TYPES result = ZIP_BLOCK_RESERVED;

#ifndef __REF_MODEL_RUN__
//   printit=true;
#endif

  
   if(printit) printf("\nZIP:  Outputblock");
   for (i=0; i<matches.size(); i++)
      {
      const matchtype &m = matches[i];
      if (m.literal)
         litHuff[m.value_len].occurence++;
      else{
         int k;
         for (k=0; k<29; k++)
            {
            if (lengths[k].len == m.value_len)
               break;
            if (lengths[k].len > m.value_len)
               {k--; break;}
            }
         litHuff[257+k].occurence++;
         for (k=0; k<30; k++)
            {
            if (distances[k].len == m.distance)
               break;
            if (distances[k].len > m.distance)
               {k--; break;}
            }
         if (k>=30) k=29;
         distHuff[k].occurence++;
         }
      }
   litHuff[256].occurence++;
   distHuff[0].occurence++;    // this prevents having 1 code and trying to make zero length
   distHuff[2].occurence++;    // it reduced compression a negligable amount for large block
   
   int fixedcost = 0;
   int dynamiccost = 0;
   for (i=0; i<litHuff.Size(); i++)
      {
      if (i<=143) fixedcost += 8 * litHuff[i].occurence;
      else if (i<=255) fixedcost += 9 * litHuff[i].occurence;
      else if (i<=279) fixedcost += 7 * litHuff[i].occurence;
      else fixedcost += 8 * litHuff[i].occurence;
      }
   for (i=0; i<distHuff.Size(); i++)
      fixedcost   += 5 * distHuff[i].occurence;
   
   fixedcost -=10; // this compensates for the 2 added distance codes that won't really appear in the output

   
   // we need to have 2583 or less symbols to ensure a code length <=15  
   int litcount= -1;
   for (i=0; i<litHuff.Size(); i++)
      litcount += litHuff[i].occurence;
   int distcount= -2;
   for (i=0; i<distHuff.Size(); i++)
      distcount += distHuff[i].occurence;

   int litdivamount = litcount<2560   ? 1 : 
                      litcount<5120   ? 2 : 
                      litcount<10240  ? 4 : 8;

   int distdivamount= distcount<2560  ? 1 : 
                      distcount<5120  ? 2 : 4;

//   printf("litcount=%d distcount=%d litdivamount=%d distdivamount=%d",litcount,distcount,litdivamount,distdivamount);
   for (i=0; i<litHuff.Size(); i++)
      {
      int &o = litHuff[i].occurence;
      o = (o/litdivamount) | (o%litdivamount!=0);
      }
   for (i=0; i<distHuff.Size(); i++)
      {
      int &o = distHuff[i].occurence;
      o = (o/distdivamount) | (o%distdivamount!=0);
      }
   
   // now that I know occurences I can set lengths
   litHuff.ComputeLengths();
   distHuff.ComputeLengths();
   
   // now compute the cost before building the code to see if fixed huffman is better
   for (i=0; i<litHuff.Size(); i++)
      {
      dynamiccost += litHuff[i].len * litHuff[i].occurence * litdivamount;
      if(block_present) {
         block.frequencies[i] = litHuff[i].occurence;
         block.lengths[i] = litHuff[i].len;
         }
      if (litHuff[i].len >15 && litHuff[i].occurence && !ref.force_fixed)
         FATAL_ERROR;
      }
   if(block_present) {
      for (; i<288; i++)
         block.frequencies[i] = block.lengths[i] = 0;
      }
   for (i=0; i<distHuff.Size(); i++)
      {
      dynamiccost += distHuff[i].len * distHuff[i].occurence * distdivamount;
      if(block_present) {
         block.frequencies[i+288] = distHuff[i].occurence;
         block.lengths[i+288] = distHuff[i].len;
         }
      if (distHuff[i].len >15 && distHuff[i].occurence && !ref.force_fixed) 
         FATAL_ERROR;
      }
   if(block_present) {
      for (; i<32; i++)
         block.frequencies[i+288] = block.lengths[i+288] = 0;
      }
   //   if (allow_nocompression && dynamiccost > rawsize*8+16 && rawsize<=65535)
   // I don't care about making optimum files, rather I want to randomly create no compress blocks. 
   // This should make nocompress happen 50% of time and doesn't require random number generator
   if (allow_nocompression && !ref.lzs && ((dynamiccost%7)%3)==1 && rawsize<=65535)
      {
//      printf("Rawsize=0x%X\n",rawsize);
      SetBits(output, bit, 1, final);
      SetBits(output, bit, 2, 0);  // no compression
      for (i=0; (bit&7); i++)
         SetBits(output, bit, 1, 0);   // pad up to nearest byte
      SetBits(output, bit, 8, rawsize&0xff);
      SetBits(output, bit, 8, (rawsize&0xff00)>>8);
      SetBits(output, bit, 8, ~rawsize&0xff);
      SetBits(output, bit, 8, ~(rawsize&0xff00)>>8);
      for (i=0; i<rawsize; i++)
         SetBits(output, bit, 8, allow_nocompression[i]);
      result = ZIP_BLOCK_NOCOMPRESS;
      return(result);
      }
   else
      {
      // I'm going to go through and assume dynamic huffman block.
      // When the block header is complete I'll know the true size and if it's bigger than a
      // fixed encoding I'll reset bit
      const int preheaderbit = bit;
      const array <unsigned char> preheaderoutput(output);
      SetBits(output, bit, 1, final);
      SetBits(output, bit, 2, 2);  // dynamic huffman
      
      array <int> codes;
      array <int> lengths;
      int last=-1;
      for (i=0; i<litHuff.Size() && i<=285; i++) {
         lengths.push(litHuff[i].len);
         //         if(printit) printf("\nZIP: Lit  %d -> %d",i,litHuff[i].len);
         }
      for (i=0; i<distHuff.Size() && i<=29; i++) {
         lengths.push(distHuff[i].len);
         //         if(printit) printf("\nZIP: Dist %d -> %d",i,distHuff[i].len);
         }
      for (i=0; i<lengths.size(); )
         {
         if (lengths[i]==0)
            {
            int k;
            for (k=0; k+i<lengths.size(); k++)
               if (lengths[k+i]!=0) break;
               
               if (k>138){
                  codes.push(18 + (138<<8));
                  i += 138;
                  k -= 138;
                  }
               if (k>=11){
                  codes.push(18 + (k<<8));
                  i += k;
                  }
               else if (k>=3){
                  codes.push(17 + (k<<8));
                  i += k;
                  }
               else if (k>0){
                  codes.push(0 + (k<<8));
                  i += k;
                  }
               last = 0;
            }
         else if (lengths[i] == last)
            {
            int k;
            for (k=0; k<6 && k+i<lengths.size(); k++)
               if (lengths[k+i]!=last) break;
               last = lengths[i];
               if (k>=3){
                  codes.push(16 + (k<<8));
                  i += k;
                  }
               else if (k==0)
                  FATAL_ERROR;
               else {
                  codes.push(lengths[i] + (k<<8));
                  i += k;
                  }
            }
         else {
            codes.push(lengths[i]+ (1<<8));
            last = lengths[i];
            i++;
            }
         }
      if (i>lengths.size()) FATAL_ERROR;
    
      huffmantype codelens(19);
      char codeindirection[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
      //      int hlit=MINIMUM(286, litHuff.Size()), hdist=MINIMUM(29, distHuff.Size()), hclen=19;
	  // hdist must be 30 or less to prevent a zlib assertion
      int hlit=286, hdist=30, hclen=19;
      
      /*      for (i=0; i<codes.size(); i++)
      codelens[codes[i]&0xff].occurence++;
      for (i=0; i<codes.size(); i++)
      { // this prevents us from exceeding a codelength of 7 
      if (codelens[codes[i]&0xff].occurence)
      codelens[codes[i]&0xff].occurence = codelens[codes[i]&0xff].occurence/4 +1;
      }
      codelens.ComputeLengths();*/
      
      
//      for (i=0; i<codes.size(); i++){
//         printf("i=%d code =%X\n",i, codes[i]);
//         }
      
      
      // To make hardware easier I will use the following static encoding. It's simple and reasonably good (within 20-30 bits of optimal)
      codelens[0].len=3;
      codelens[1].len=7;
      codelens[2].len=6;
      codelens[3].len=4;
      codelens[4].len=4;
      codelens[5].len=4;
      codelens[6].len=4;
      codelens[7].len=4;
      codelens[8].len=4;
      codelens[9].len=4;
      codelens[10].len=4;
      codelens[11].len=4;
      codelens[12].len=6;
      codelens[13].len=7;
      codelens[14].len=7;
      codelens[15].len=7;
      codelens[16].len=3;
      codelens[17].len=4;
      codelens[18].len=4;
      codelens.Transform();
      
      SetBits(output, bit, 5, hlit-257);
      SetBits(output, bit, 5, hdist-1);
      SetBits(output, bit, 4, hclen-4);
      
      for (i=0; i<hclen; i++) {
         SetBits(output, bit, 3, codelens[codeindirection[i]].len);
         if (codelens[codeindirection[i]].len >7)
            FATAL_ERROR;
         }
      
      if(block_present) {
         block.alphalens.push((final ? 0xfded : 0xfdec) | (16<<16));
         block.alphalens.push(0x8e47 | (16<<16));
         block.alphalens.push(0x4924 | (16<<16));
         block.alphalens.push(0xbcd2 | (16<<16));
         block.alphalens.push(0x3ff  | (10<<16));
         }
      dynamiccost+=74;
      for (i=0; i<codes.size(); i++)
         {         
         int c = codes[i]&0xff;
         SetRBits(output, bit, codelens[c].len, codelens[c].code);
         if (c == 16)
            SetBits(output, bit, 2, (codes[i]>>8)-3);
         else if (c == 17)
            SetBits(output, bit, 3, (codes[i]>>8)-3);
         else if (c == 18)
            SetBits(output, bit, 7, (codes[i]>>8)-11);
         else {
            int k;
            for (k=1; k<(codes[i]>>8); k++)
               SetRBits(output, bit, codelens[c].len, codelens[c].code);
            }
         
         array<unsigned char> temp;
         int tempbit=0;
         SetRBits(temp, tempbit, codelens[c].len, codelens[c].code);
         dynamiccost+=codelens[c].len;
         if (c == 16){
            SetBits(temp, tempbit, 2, (codes[i]>>8)-3);
            dynamiccost+=2;
            }
         else if (c == 17){
            SetBits(temp, tempbit, 3, (codes[i]>>8)-3);
            dynamiccost+=3;
            }
         else if (c == 18){
            SetBits(temp, tempbit, 7, (codes[i]>>8)-11);
            dynamiccost+=7;
            }
         else {                                   
            int k;
            for (k=1; k<(codes[i]>>8); k++){
               SetRBits(temp, tempbit, codelens[c].len, codelens[c].code);
               dynamiccost+=codelens[c].len;
               }            
            }
         
         temp.push(0);
         if(block_present) {
            block.alphalens.push(temp[0] | (temp[1]<<8)  | (tempbit<<16));
            }
         }
	  if(block_present) {
		 block.dynamiccost = dynamiccost;
		 block.fixedcost = fixedcost;
		 }
      
      if (lzs)
         {
         bit = preheaderbit; // back out dynamic header
         output = preheaderoutput;
         
         for (i=0; i<=256; i++) {
            litHuff[i].len = 9;
            litHuff[i].code = i;
            }
         litHuff[256].len = 9;
         litHuff[256].code = 0x180;
         litHuff[257].len = 2;
         litHuff[257].code = 0x3;
         litHuff[258].len = 2;
         litHuff[258].code = 0x2;
         for (i=259; i<=287; i++)
            litHuff[i].len = 0;
         i=0;
         distHuff[i].len  = 2;
         distHuff[i].code = 0; i++;
         distHuff[i].len  = 2;
         distHuff[i].code = 1; i++;
         distHuff[i].len  = 2;
         distHuff[i].code = 2; i++;
         distHuff[i].len  = 4;
         distHuff[i].code = 0xc; i++;
         distHuff[i].len  = 4;
         distHuff[i].code = 0xd; i++;
         distHuff[i].len  = 4;
         distHuff[i].code = 0xe; i++;
         distHuff[i].len  = 4;
         distHuff[i].code = 0xf; i++;
         for (; i<=31; i++)
            distHuff[i].len = 0;
         if(printit) printf("ZIP: LZS Huffman selected, dynamiccost=%d, fixedcost=%d\n", dynamiccost, fixedcost);
         result = ZIP_BLOCK_FIXED_HUFFMAN;
         }
      else if ((force_dynamic || dynamiccost < fixedcost) && !force_fixed) {
         if(printit) printf("ZIP: Dynamic Huffman selected, dynamiccost=%d, fixedcost=%d\n", dynamiccost, fixedcost);
         result = ZIP_BLOCK_DYNAMIC_HUFFMAN;
         litHuff.Transform();
         distHuff.Transform();
         }
      else {
         bit = preheaderbit; // back out dynamic header
         output = preheaderoutput;

         SetBits(output, bit, 1, final);
         SetBits(output, bit, 2, 1);  // fixed huffman
         if(block_present) {
            block.alphalens.clear();
            block.alphalens.push((final ? 0x3 : 0x2) | (3<<16));
            }
         
         for (i=0; i<=143; i++)
            litHuff[i].len = 8;
         for (i=144; i<=255; i++)
            litHuff[i].len = 9;
         for (i=256; i<=279; i++)
            litHuff[i].len = 7;
         for (i=280; i<=287; i++)
            litHuff[i].len = 8;
         for (i=0; i<=31; i++)
            distHuff[i].len = 5;
         if(printit) printf("ZIP: Static Huffman selected, dynamiccost=%d, fixedcost=%d\n", dynamiccost, fixedcost);
         result = ZIP_BLOCK_FIXED_HUFFMAN;
         litHuff.Transform();
         distHuff.Transform();
         }
      }
      
   if(block_present) {
      for (i=0; i<litHuff.Size(); i++){
         block.codes[i] = litHuff[i].code;
         block.lengths[i] = litHuff[i].len;
         }
      for (i=286; i<288; i++)
         block.codes[i] = block.lengths[i] = 0;
      for (i=0; i<distHuff.Size(); i++){
         block.codes[i+288]   = distHuff[i].code;
         block.lengths[i+288] = distHuff[i].len;
         }
      for (; i<32; i++){
         block.codes[i+288] = 0;
         block.lengths[i+288] = 0;
         }
      }
   
int throw_bits=-1; // remember this for lzs

   if (lzs)
      {
      for (i=0; i<matches.size(); i++)
         {
         if (allow_nocompression) // I want to randomly throw in some eob symbols when this is used for decomp stimulus generation
            {
            if ((uint64_t)i*fixedcost % 31 ==1){
               SetBitsMSB(output, bit, 9, 0x180);  // end of block marker
               throw_bits = (bit&7) ? (8 - (bit&7)) : 0;
               SetBitsMSB(output, bit, throw_bits, 0);  // pad to nearest byte
               }
            }

	 
         const matchtype &m = matches[i];
         if (m.literal){
            SetBitsMSB(output, bit, 1, 0);
            SetBitsMSB(output, bit, 8, m.value_len);
//            printf("%2d lit %d '%c'\n", i, m.value_len, m.value_len);
            }
         else{
            SetBitsMSB(output, bit, 1, 1);
            if (m.distance < 127)
               SetBitsMSB(output, bit, 8, 0x80 | m.distance);
            else if (m.distance <= 2047)
               SetBitsMSB(output, bit, 12, m.distance);
            else 
               FATAL_ERROR;
            int len = m.value_len;
            if (len<=4)
               SetBitsMSB(output, bit, 2, len-2);
            else if (len<=7)
               SetBitsMSB(output, bit, 4, 0xC | (len-5));
            else {
               len -=8;
               SetBitsMSB(output, bit, 4, 0xf);
               while (len>=15)
                  {
                  SetBitsMSB(output, bit, 4, 0xf);
                  len -=15;
                  }
               SetBitsMSB(output, bit, 4, len);
               }
//            printf("%2d string %d %d\n", i, m.distance, m.value_len);
            }
         }
      SetBitsMSB(output, bit, 9, 0x180);  // end of block marker
      
      throw_bits = (bit&7) ? (8 - (bit&7)) : 0;
      SetBitsMSB(output, bit, throw_bits, 0);  // pad to nearest byte
//      printf("End of block padd with %d bits\n",throw_bits);
      }
   else 
      {  // normal deflate
      for (i=0; i<matches.size(); i++)
         {
         const matchtype &m = matches[i];
         if (m.literal)
            SetRBits(output, bit, litHuff[m.value_len].len, litHuff[m.value_len].code);
         else
            {
            int k;
            for (k=0; k<29; k++)
               {
               if (lengths[k].len == m.value_len)
                  break;
               if (lengths[k].len > m.value_len)
                  {k--; break;}
               }
            if (k<0) FATAL_ERROR;

            SetRBits(output, bit, litHuff[257+k].len, litHuff[257+k].code);
            SetBits(output, bit, lengths[k].extra, m.value_len - lengths[k].len);
            for (k=0; k<30; k++)
               {
               if (distances[k].len == m.distance)
                  break;
               if (distances[k].len > m.distance)
                  {k--; break;}
               }
            if (k==30) k=29;
            SetRBits(output, bit, distHuff[k].len, distHuff[k].code);
            SetBits(output, bit, distances[k].extra, m.distance - distances[k].len);
            }
         }
      SetRBits(output, bit, litHuff[256].len, litHuff[256].code);
      }
   
// this code is to collect raw output codes to compare against RTL
   if (lzs && block_present)
      {
      bool skippedlast=false;
      array<unsigned char> temp;
      int tempbit=0;
      for (i=0; i<matches.size(); i++)
         {
         const matchtype &m = matches[i];
         if (m.literal){
            SetRBits(temp, tempbit, 1, 0);
            SetRBits(temp, tempbit, 8, m.value_len);
            }
         else{
            SetRBits(temp, tempbit, 1, 1);
            if (m.distance < 127)
               SetRBits(temp, tempbit, 8, 0x80 | m.distance);
            else if (m.distance <= 2047)
               SetRBits(temp, tempbit, 12, m.distance);
            else 
               FATAL_ERROR;
            int len = m.value_len;
            if (len<=4){
               SetRBits(temp, tempbit, 2, len-2);
               }
            else if (len<=7){
               SetRBits(temp, tempbit, 4, 0xC | (len-5));
               }
            else if (len==8) {
               SetRBits(temp, tempbit, 4, 0xf);
               SetRBits(temp, tempbit, 4, 0x0);
               }
            else {
               len -=8;
               SetRBits(temp, tempbit, 4, 0xf);
               BlockStuff(block.recodes, temp, tempbit);
               while (len>=15)
                  {
                  SetRBits(temp, tempbit, 4, 0xf);
                  len -=15;
                  if (len!=0)
                     BlockStuff(block.recodes, temp, tempbit);
                  }
               SetRBits(temp, tempbit, 4, len);
               }
            skippedlast = false;
            }
         if (i==matches.size()-1 || !m.literal || !matches[i+1].literal || skippedlast)
            {
            BlockStuff(block.recodes, temp, tempbit);
            skippedlast = false;
            }
         else
            skippedlast = true;
         }
      SetRBits(temp, tempbit, 9, 0x180);  // end of block marker
      if (throw_bits>0)
         SetRBits(temp,tempbit, throw_bits, 0);
      BlockStuff(block.recodes, temp, tempbit);

      }
   else if (block_present)
      { // normal deflate
      bool skippedlast=false;
      array<unsigned char> temp;
      int tempbit=0;
      for (i=0; i<matches.size(); i++)
         {
         // for debug      if (block.recodes.size()==0x3fb)
         //  if(printit) printf(" ");
         const matchtype &m = matches[i];
         if (m.literal)
            SetRBits(temp, tempbit, litHuff[m.value_len].len, litHuff[m.value_len].code);
         else
            {
            int k;
            for (k=0; k<29; k++)
               {
               if (lengths[k].len == m.value_len)
                  break;
               if (lengths[k].len > m.value_len)
                  {k--; break;}
               }

            SetRBits(temp, tempbit, litHuff[257+k].len, litHuff[257+k].code);
            SetBits(temp, tempbit, lengths[k].extra, m.value_len - lengths[k].len);
            for (k=0; k<30; k++)
               {
               if (distances[k].len == m.distance)
                  break;
               if (distances[k].len > m.distance)
                  {k--; break;}
               }
            if (k==30) k=29;
            SetRBits(temp, tempbit, distHuff[k].len, distHuff[k].code);
            SetBits(temp, tempbit, distances[k].extra, m.distance - distances[k].len);
            }
         if (i==matches.size()-1 || !m.literal || !matches[i+1].literal || skippedlast)
            {
            BlockStuff(block.recodes, temp, tempbit);
            skippedlast = false;
            }
         else
            skippedlast = true;
         }
      SetRBits(temp, tempbit, litHuff[256].len, litHuff[256].code);
      BlockStuff(block.recodes, temp, tempbit);
      }


   return(result);
   }




inline unsigned int crcx(const void *mem, int len)
   {
   unsigned int accum=0;
   const unsigned char *buffer = (const unsigned char *)mem;
   int i,k;

   for (i=0; i<len; i++)
      {
      accum ^= buffer[i];
      for (k=0; k<8; k++)
         {
         if (accum& 0x80000000)
            accum = (accum<<1)^0x04c11db7;
         else
            accum = accum<<1;
         }
      }
   return accum;
   }

void zip_byte_init(reftype &ref, int inputlen) {
   ASSERT(!ref.decompression);
   ref.bit = 0;
   ref.z_i = 0;
   ref.startblock = 0;
   ref.lmatches = new array <matchtype>;
   ref.zhash = new zhashtype;
   ref.searches = 0;
   ref.totalsearches = 0;
   ref.search_cycles = 0;
   ref.nblocks = 0;
   ref.matchbundles = 0;
   ref.lastlen = 0;
   ref.skipto = -1;
   ref.completelyskip = -1;
   ref.output.clear();
   SetBits(ref.output, ref.bit, ref.extra_len_in, ref.extra_in);
   ref.inputlen = inputlen;
   ref.input_so_far = 0;
   ref.c.s.addler_out = ref.adler_crc_in;
   ref.c.s.crc_out = ref.adler_crc_in;
   ref.nblocksp = 0;
   ref.lmatchb = 0;
}

void zip_byte_with_byte(reftype &ref, unsigned char byte, bool printit) {
   ref.input.push(byte);
   zip_byte(ref, printit);
}

void zip_byte(reftype &ref, bool printit) {
   ASSERT(!ref.decompression);

   ref.input_so_far ++;
   ASSERT(ref.input.size() >= ref.input_so_far);

   if(ref.input_so_far == ref.inputlen) {

      ASSERT(ref.inputlen == ref.input.size());
      // we are at the end, so finish the processing

      // finish each input character
      do {
         zip_byte_buffered(ref, printit);
      } while(ref.z_i != ref.inputlen);

      // clean up at the end
      while (ref.lmatches->size()!=0){
         // there are some degenerate cases where I'll have a single string left over. It then gets forced to
         // be a block all to it's self
         if(printit) printf("ZIP: Block #%d %d bytes.\n",ref.nblocks,ref.z_i-ref.startblock);
         array<matchtype> deferred;
         if (ref.matchbundles>(ref.allow_nocompression ? BLOCKSIZE/16 :BLOCKSIZE) && (!ref.force_fixed || !ref.lzs_compression_features)) {
            deferred.push(ref.lmatches->back());
            ref.lmatches->pop_back();
         }
#ifdef __REF_MODEL_RUN__
         blocktype *tblockp = NULL;
         ASSERT(ref.nblocksp < MAX_BLOCKS_PENDING);
         ref.block_data[ref.nblocksp].bmatchb = ref.matchbundles - ref.lmatchb;
         ref.lmatchb = ref.matchbundles;
         ref.block_data[ref.nblocksp].force_fixed = ref.force_fixed;
         int bit_before = ref.bit;
         bool final = deferred.size()==0 && ref.eof;
         ZIP_BLOCK_TYPES t = OutputBlock(ref, ref.output, ref.bit, 1+ref.z_i-ref.startblock, *(ref.lmatches), tblockp, final, ref.force_fixed, ref.force_dynamic, ref.lzs, (ref.allow_nocompression && ref.startblock<ref.input.size()) ? &(ref.input[ref.startblock]) : NULL, ref.matchbundles, printit);
         ref.block_data[ref.nblocksp].output_bits = ref.bit - bit_before;
         if(final) {
            int roundup = (8 - (ref.bit & 0x7)) & 0x7;
            ref.block_data[ref.nblocksp].output_bits += roundup;
         }
         ref.block_data[ref.nblocksp].type = t;
         ref.nblocksp ++;
#else
         ref.matches += *(ref.lmatches);
         ref.blocks.push(blocktype());
         blocktype *tblockp = &(ref.blocks.back());
         bool final = deferred.size()==0 && ref.eof;
         (void) OutputBlock(ref, ref.output, ref.bit, 1+ref.z_i-ref.startblock, *(ref.lmatches), tblockp, final, ref.force_fixed, ref.force_dynamic, ref.lzs, (ref.allow_nocompression && ref.startblock<ref.input.size()) ? &(ref.input[ref.startblock]) : NULL, ref.matchbundles, printit);
#endif
         ref.lmatches->clear();
         *(ref.lmatches) = deferred;
         if (deferred.size()==0)
            ref.matchbundles=0;
         else if (deferred[0].literal)
            ref.matchbundles=1;
         else
            ref.matchbundles=2;
         deferred.clear();
//         ref.search_cycles/=2;
         //   if(printit) printf("ZIP: %d (%.2f/Byte) searches needed. %d (%.2f/Byte) total searches needed.  %.2fb/cycle needed.\n", searches, searches*1.0/(i-startblock), totalsearches, totalsearches*1.0/(i-startblock), (i-startblock)*8.0/search_cycles);
//         if(printit) 
//            printf("ZIP: Hash widget: %.2fb/cycle, History widget: %d (%.2f/Byte)total searches needed. %.2fb/cycle.\n",
//               (ref.z_i-ref.startblock)*8.0/ref.zhash->cycle, ref.totalsearches, ref.totalsearches*1.0/(ref.z_i-ref.startblock),
//               (ref.z_i-ref.startblock)*8.0/ref.search_cycles);
         ref.zhash->cycle = 0;
         ref.zhash->totalbanks[0]=ref.zhash->totalbanks[1]=ref.zhash->totalbanks[2]=ref.zhash->totalbanks[3]=0;
         
         ref.searches=0;
         ref.totalsearches=0;
//         ref.search_cycles=0;
         ref.startblock = ref.z_i+1;
         ref.nblocks++;
      }

      ref.exbits_out = 0;
      ref.bytes_out = ref.output.size();
      if(ref.eof) {
         ref.bytes_out = ref.output.size();
         ref.exnum_out = 0;
      }
      else {
         ref.exnum_out = ref.bit & 0x7;
         if(ref.exnum_out) {
            ref.bytes_out --;
            ref.exbits_out = ref.output[ref.bytes_out];
         }
      }

      // clean up the stuff we dynamically allocated for the per-byte stuff
      delete ref.lmatches;
      ref.lmatches = NULL;
      delete ref.zhash;
      ref.zhash = NULL;
      
      if(printit) printf("ZIP: %d -> %d compression ratio = %.2f\n",(ref.inputlen-ref.historybytes), ref.output.size(), 1.0*(ref.inputlen-ref.historybytes)/ref.output.size());
   }
   else if(ref.input_so_far >= ZIP_BYTES_BUFFERED_AHEAD) {

      // we are not at the end, but have enough data buffered -> process a single byte
      zip_byte_buffered(ref, printit);
   }
}


struct searchtype{
   int len, dist, backup, search_cycles;
   searchtype()
      {
      len = 0;
      dist = 0;
      backup = 0;
      search_cycles=0;
      }
   };


void zip_byte_buffered(reftype &ref, bool printit) 
   {
   int i;
   // handle the history case
   if(ref.z_i < ref.historybytes) {
#ifndef __REF_MODEL_RUN__
      if (ref.historybytes >= ref.inputlen) {
         printf("ZIP: Inputlen=%d historybytes=%d", ref.inputlen, ref.historybytes);
         FATAL_ERROR;
         }
#endif
      array <int> odd, even;
      ref.zhash->Check(ref.input, ref.z_i, ref, true, odd, even);
      ref.zhash->Add(ref.input, ref.z_i, ref);
      ref.z_i ++;
      return;
      }

   //   printf("z_i=%d, skipto=%d, complete=%d\n", ref.z_i, ref.skipto, ref.completelyskip);

   if (ref.storage && ref.z_i>=ref.skipto)
      {
      int i, k;
      array <searchtype> searches;
      
      for (i=ref.z_i; i<ref.inputlen && i<ref.z_i+100000; i++)
         {
         array <int> odd, even;
         array <int> possibles;
         int maxlen=0;
         ref.zhash->Check(ref.input, i, ref, false, odd, even);
         ref.zhash->Add(  ref.input, i, ref);
//         if (odd [lazy].size()>1)  odd[lazy].resize(1);
//         if (even[lazy].size()>1) even[lazy].resize(1);
         possibles += odd;
         possibles += even;
         searches.push(searchtype());
         searches.back().search_cycles = MAXIMUM(odd.size(), even.size());

         if (i==572)
            printf("");

         for (k=0; k<possibles.size(); k++) 
            {
            int l, backup=0;
            int location = (possibles[k] & 0x7fff) | (i&0xffff8000);
            if (location >= i)
               location -= 32768;
            int dist = i - location;

            while (i>0 && location-backup>0 && ((location-backup)&0x7))
               {
               if (ref.input[i-1-backup] == ref.input[location-1-backup])
                  {
                  backup++;
                  }
               else break;
               }
            for (l=0; l<4088 && (l<258 || ref.lzs) && location>=i-32768 && location<i && location>=0 && i+l<ref.inputlen && ref.input[i+l]==ref.input[location+l]; l++)
               ;
            if ((l > maxlen) || (l >= maxlen && dist < searches.back().dist))
               {
               maxlen = l;
               searches.back().len = l;
               searches.back().dist = dist;
               searches.back().backup = 0;
               }
            if (backup && searches.size()>backup)
               {
               searchtype &ss = searches[searches.size()-1-backup];
               if (ss.len < l+backup || (ss.len<=l+backup && ss.dist>dist))
                  {
                  ss.len  = l+backup;
                  ss.dist = dist;
                  ss.backup = backup;
                  }

               }
            }
         }
      for (i=0; i<searches.size() && ref.matchbundles < BLOCKSIZE; )
         {
         searchtype &s = searches[i];
         int best   = i,
             maxlen = s.len>3 ? s.len :0,
             dist   = s.dist;

         // now look forward and see if by waiting we can find a better match
         for (k=i+1; k<searches.size() && k<i+60; k++)
            {
            searchtype &sk = searches[k];

            int backup = 0;
            bool better = sk.len+backup>3 && maxlen<=3;
            if (k-best <1 && (sk.len >  maxlen + (k-best) - backup))
               better = true;
            if (k-best <1 && (sk.len >= maxlen + (k-best) - backup && sk.dist<dist))
               better = true;
//            better = better || (sk.len >  maxlen + (k-best-backup) - backup) && ;
//            better = better || (sk.len >= maxlen + (k-best-backup) - backup && sk.dist<dist);
            if (better){
               maxlen    = MINIMUM(258, sk.len +backup);
               dist      = sk.dist;
               best      = k - backup;
               }
            }
         if (maxlen<3) maxlen=0;
         if (best<i) FATAL_ERROR;
         for (k=i; k<best-3; k++)
            {
            searchtype &sk = searches[k];
            if (sk.len>=4)
               {
               maxlen = MINIMUM(best-k, sk.len);
               dist = sk.dist;
               best = k;
               break;
               }
            }
         if (!maxlen) best++;
         for (k=i; k<best; k++)
            {
            searchtype &sk = searches[k];

            ref.lmatches->push(matchtype(ref.input[ref.z_i+k])); // output literal
            ref.matchbundles++;
            ref.search_cycles += sk.search_cycles;
            }
         if (maxlen)
            {
//            printf("%5d %3d %5d\n",best+ref.z_i,maxlen,dist);
            if (maxlen<3) FATAL_ERROR;
            ref.search_cycles += 6 + (maxlen-12)/2;
            ref.lmatches->push(matchtype(maxlen, dist));
            ref.matchbundles += (ref.matchbundles&1)+2;
            if (best+maxlen<=i) FATAL_ERROR;
            i = best + maxlen;
            }
         else
            i = best;
         }
      ref.completelyskip = ref.z_i+i;
      ref.skipto         = ref.z_i+i;
      }
   else 
   if (ref.z_i<ref.completelyskip)
      ;  // when doing lazy evaulation I already did the hash add so I can just skip down to crc
   else if (ref.z_i<ref.skipto) {
      array <int> odd, even;
      //      printf("Adding %d\n",ref.z_i);
      ref.zhash->Check(ref.input, ref.z_i, ref, false, odd, even);
      ref.zhash->Add(ref.input, ref.z_i, ref);
      }
   else if (ref.z_i>=ref.skipto) 
      {
      array <int> possibles;
      int starti=ref.z_i, num=0, lazy=0, bestentry=-1, maxlen=0, dist=-1, l, k, location;

      while (ref.z_i + lazy<ref.inputlen && lazy<(ref.compression_fastest?1:3)) 
         {
         array <int> odd, even;
//         if (ref.z_i>=332)
//            printf("");

         ref.zhash->Check(ref.input, ref.z_i+lazy, ref, false, odd, even);
         ref.zhash->Add(  ref.input, ref.z_i+lazy, ref);

         for (k=0; k<odd.size(); k++)
            possibles.push(odd[k] | (lazy<<16));
         for (k=0; k<even.size(); k++)
            possibles.push(even[k] | (lazy<<16));
//	 printf("loc=%x lazy=%d odd[0]=%x odd[1]=%x odd[2]=%x even[0]=%x even[1]=%x even[2]=%x\n",ref.z_i,lazy,
//	      odd.size()>0  ? odd[0] :-1,odd.size()>1  ? odd[1] :-1,odd.size()>2  ? odd[2]:-1,
//	      even.size()>0 ? even[0]:-1,even.size()>1 ? even[1]:-1,even.size()>2 ? even[2]:-1);
         lazy++;
         num += MAXIMUM(odd.size(), even.size());
         if (odd.size()==0 && even.size()==0) break;

         // this models the logic which turns off lazy evaluation near the end of the file so I don't overshoot the end
         /*         if (((ref.z_i+lazy+0)&0xfffffffe) == (ref.inputlen&0xfffffffe) ||
         ((ref.z_i+lazy+2)&0xfffffffe) == (ref.inputlen&0xfffffffe) ||
         ((ref.z_i+lazy+4)&0xfffffffe) == (ref.inputlen&0xfffffffe))
         break;*/
         }
      ref.completelyskip = ref.z_i + lazy;

      for (k=0; k<possibles.size(); k++) {
         int entry = possibles[k]>>16;
         if (entry>=lazy) FATAL_ERROR;
         int i = starti + entry;
         location = (possibles[k] & 0x7fff) | (i&0xffff8000);
         if (location >= i)
            location -= 32768;

         while (entry>=1 && location>0 && (location&0x7))
            {
            if (ref.input[i-1] == ref.input[location-1])
               {
               entry--;
               i--;
               location--;
               }
            else break;
            }
         int lzs_stop = (location&7) ? (4096 - (location&7)) : (4096-8);
         for (l=0; l<lzs_stop && (l<258 || ref.lzs) && location>=i-32768 && location<i && location>=0 && i+l<ref.inputlen && ref.input[i+l]==ref.input[location+l]; l++)
            ;

         if (i-location >= 31744) 
            l=0;          // for imp reasons I can't use entire 32K history window
         if (i-location >= 7680 && ref.storage)
            l=0;          
         if (i-location >= 2048 && ref.lzs) 
            l=0;          // lzs can't handle windows more than 2K
         if (l==3 && entry==2)
            l=0;  // this is to match rtl
         bool better = bestentry<0;
         better = better || (l>maxlen+entry-bestentry);
         better = better || (l>=maxlen+entry-bestentry && (i-location)<dist);

         if (better) {
            maxlen    = l;
            dist      = i - location;
            bestentry = entry;
            }
//	 printf("starti=%x k=%d entry=%d l=%d location=%d maxlen=%d\n",starti,k,entry,l,location,maxlen);
         }
//      if (bestentry<0 || maxlen<2 || (maxlen<3 && !ref.lzs) || (maxlen<4 && !(ref.force_fixed || ref.lzs)))
      if (bestentry<0 || maxlen<=2 || (maxlen<4 && (!ref.force_fixed || !ref.lzs_compression_features)))  // erase me
         {
         for (k=0; k<lazy; k++) {
            ref.lmatches->push(matchtype(ref.input[k+starti]));
            ref.matchbundles++;
            }            
         ref.skipto = starti+k;
         }
      else {
         for (k=0; k<bestentry; k++) {
            ref.lmatches->push(matchtype(ref.input[k+starti]));
            ref.matchbundles++;
            }
         printf("%5d %3d %5d\n",k+starti,maxlen,dist);
         ref.lmatches->push(matchtype(maxlen, dist));
         ref.matchbundles += (ref.matchbundles&1)+2;
/*         for (k=bestentry+maxlen; k<lazy; k++)  // this handles the case where lazy=3, maxlen=2, hash stuff has already been pushed
            {
            ref.lmatches->push(matchtype(ref.input[k+starti]));
            ref.matchbundles++;
            maxlen++; 
            }*/

         ref.skipto = starti+bestentry+maxlen;
         }
      }


   if ((ref.matchbundles >= (ref.allow_nocompression ? BLOCKSIZE/16: BLOCKSIZE)) && !ref.force_fixed && (ref.z_i+1 < ref.inputlen) && (ref.skipto < ref.inputlen)) {
//      if(printit) printf("ZIP: Block #%d %d bytes.\n",ref.nblocks,ref.z_i-ref.startblock);
      array<matchtype> deferred;
      int k, lastlegalbundle=0;

      for (k=0, ref.matchbundles=0; k<ref.lmatches->size(); k++) {
         if ((*ref.lmatches)[k].literal)
            ref.matchbundles++;
         else
            ref.matchbundles += (ref.matchbundles&1)+2;
         if (ref.matchbundles > (ref.allow_nocompression ? BLOCKSIZE/16 :BLOCKSIZE))
            deferred.push((*ref.lmatches)[k]);
         else
            lastlegalbundle = ref.matchbundles;
         }
      for (k=0; k<deferred.size(); k++)
         ref.lmatches->pop_back();
      ref.matchbundles = lastlegalbundle;
#ifdef __REF_MODEL_RUN__
      blocktype *tblockp = NULL;
      ASSERT(ref.nblocksp < MAX_BLOCKS_PENDING);
      ref.block_data[ref.nblocksp].bmatchb = ref.matchbundles - ref.lmatchb;
      ref.lmatchb = ref.matchbundles;
      ref.block_data[ref.nblocksp].force_fixed = ref.force_fixed;
      int bit_before = ref.bit;
      ZIP_BLOCK_TYPES t = OutputBlock(ref, ref.output, ref.bit, 1+ref.z_i-ref.startblock, *(ref.lmatches), tblockp, false, ref.force_fixed, ref.force_dynamic, ref.lzs, ref.allow_nocompression ? &(ref.input[ref.startblock]) : NULL, ref.matchbundles, printit);
      ref.block_data[ref.nblocksp].output_bits = ref.bit - bit_before;
      ref.block_data[ref.nblocksp].type = t;
      ref.nblocksp ++;
#else
      ref.matches += *(ref.lmatches); 
      ref.blocks.push(blocktype());
      blocktype *tblockp = &(ref.blocks.back());
      (void) OutputBlock(ref, ref.output, ref.bit, 1+ref.z_i-ref.startblock, *(ref.lmatches), tblockp, false, ref.force_fixed, ref.force_dynamic, ref.lzs, ref.allow_nocompression ? &(ref.input[ref.startblock]) : NULL, ref.matchbundles, printit);
#endif
      ref.lmatches->clear();
      *(ref.lmatches) = deferred;
      ref.matchbundles=0;
      for (i=0; i<deferred.size(); i++) {
         if (deferred[i].literal)
            ref.matchbundles++;
         else
            ref.matchbundles += (ref.matchbundles&1)+2;
         }
      deferred.clear();
//      ref.search_cycles/=2;
//      if(printit) 
//         printf("ZIP: Hash widget: %.2fb/cycle, History widget: %d (%.2f/Byte) total searches needed. %.2fb/cycle.\n",
//         (ref.z_i-ref.startblock)*8.0/ref.zhash->cycle, ref.totalsearches, ref.totalsearches*1.0/(ref.z_i-ref.startblock),
//         (ref.z_i-ref.startblock)*8.0/ref.search_cycles);
      ref.zhash->cycle = 0;
      ref.zhash->totalbanks[0]=ref.zhash->totalbanks[1]=ref.zhash->totalbanks[2]=ref.zhash->totalbanks[3]=0;
//      ref.searches=0;
      ref.totalsearches=0;
//      ref.search_cycles=0;
      ref.startblock = ref.z_i+1;
      ref.nblocks++;
      }

   // calc addler
   int s1=ref.c.s.addler_out&0xffff, s2=(ref.c.s.addler_out>>16)&0xffff;
   s1 += ref.input[ref.z_i];
   s2 += s1;
   if (s1>=65521) s1 -= 65521;
   if (s2>=65521) s2 -= 65521;
   if (s1>=65521) s1 -= 65521;
   if (s2>=65521) s2 -= 65521;
   ref.c.s.addler_out = s1 | (s2<<16);

   // calc CRC32
   itu_crc32(ref.c.s.crc_out, ref.input[ref.z_i]);

   ref.z_i ++;
   }







void zip(reftype &ref) {
   int inputsize = ref.input.size();
   
   if (ref.lzs)
      ref.force_fixed = true;
   if (ref.force_fixed)
      ref.force_dynamic = false;
   if (ref.lzs && ref.extra_len_in!=0)
      FATAL_ERROR;  // lzs doesn't need or support extra bits 
   
   zip_byte_init(ref, inputsize);
   for(int i = 0; i < inputsize; i++) zip_byte(ref);
}


#ifndef __REF_MODEL_RUN__

void DebugBreakFunc(const char *module, int line)
   {
   printf("\nFatal error in module %s line %d\n", module, line);
#ifdef _MSC_VER
   *(int *)0 = 0;    // this will get the debugger's attention
#else
   ASSERT(0);
#endif
   }

#endif // __REF_MODEL_RUN__


