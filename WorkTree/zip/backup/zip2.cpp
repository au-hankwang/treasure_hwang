#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/timeb.h>
#include <time.h>
#include <math.h>

typedef unsigned uint;

#include "common.h"
#include "template.h"
#include "zip.h"




int GetBits(const array <unsigned char> &input, int &bit, int num)  // -1 on error
   {
   int ret=0;
   int i;

   for (i=0; i<num; i++)
      {
      if (bit/8 >= input.size())
         return -1;
      ret = ret | (((input[bit/8]>>(bit%8))&1) << i);
      bit++;
      }
   return ret;
   }

int GetRBits(const array <unsigned char> &input, int &bit, int num)  // -1 on error
   {
   int ret=0;
   int i;

   for (i=0; i<num; i++)
      {
      if (bit/8 >= input.size())
         return -1;
      ret = ret <<1;
      ret = ret | ((input[bit/8]>>(bit%8))&1);
      bit++;
      }
   return ret;
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
   unsigned len;
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


void BlockProcess(huffmantype &litHuff, huffmantype &distHuff, const array <unsigned char> &input, int &bit, array <unsigned char> &output, reftype &ref, const array <unsigned char> *reference)
   {
   int code;
   int ops=0;

   do{
      ops++;
      code = litHuff.FetchCode(input, bit);
//      printf("\nZIP: Code = 0x%x, %c",code,(code<128 && code>=32) ? code : ' ');
      if (code==-1)
         {
         ref.error_out = 4;
         return;
         }
      else if (code<256)
         {
         ref.matches.push(matchtype(code));
         
         output.push(code);
         if (reference!=NULL && output.back() != (*reference)[output.size()-1])
            FATAL_ERROR;
         }
      else if (code>285)
         {
         ref.error_out = 7;
         return;
         }
      else if (code>256)
         {
         int len = lengths[code-257].len + GetBits(input, bit, lengths[code-257].extra);
         int dcode = distHuff.FetchCode(input, bit);
         if (dcode >29)
            {
            ref.error_out = 7;
            return;
            }
         int distance = distances[dcode].len + GetBits(input, bit, distances[dcode].extra);
         int h = output.size();
         if (bit/8 >= input.size())
            {
            ref.error_out = 4;
            return;
            }

         ref.matches.push(matchtype(len, distance));
         if (h-distance<0)
            {
            ref.error_out = 7;
            return;
            }
         if (h-distance <0) 
            FATAL_ERROR;
//         printf("(%d,%d)",len,distance);
         for (int i=0; i<len; i++)
            {
            output.push(output[h-distance]); h++; 
            if (reference!=NULL && output.back() != (*reference)[output.size()-1])
               FATAL_ERROR;
            }
         }
      } while (code!=256);
   }

         
void DynamicHuffman(const array<unsigned char> &input, int &bit, array <unsigned char> &output, reftype &ref, const array <unsigned char> *reference)
   {
   int code;
   short hlit, hdist, hclen;
   huffmantype codelens(19);
   char codeindirection[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
   int i;
   struct decompblocktype b;

   hlit  = GetBits(input, bit, 5) +257;
   hdist = GetBits(input, bit, 5) +1;
   hclen = GetBits(input, bit, 4) +4;

   if (hclen==-1) {
      ref.error_out = 4;
      return;
      }

   huffmantype litHuff(hlit), distHuff(hdist);
   for (i=0; i<hclen; i++)
      codelens[codeindirection[i]].len = GetBits(input, bit, 3);
   if (bit/8>=input.size()){
      ref.error_out = 4;
      return;
      }

   codelens.Transform();
   for (i=0; i<19; i++)
      {
      b.alphacodes[i].code = codelens[i].code;
      b.alphacodes[i].len  = codelens[i].len;
      }

   int spillover=0, last=-1;
   for (i=0; i<hlit; )
      {
      code = codelens.FetchCode(input, bit);
      if (code==-1)
         {
         ref.error_out = 4;
         return;
         }
      if (codelens[code].len==0) FATAL_ERROR;
      spillover = 0;      
      if (code<=15) {
         last = litHuff[i].len = code;
         i++;
         }
      else if (code==16)
         {
		 if (i==0) 
			{
			ref.error_out = 8;
			return;
			}
         int l = 3 + GetBits(input, bit, 2);
         int k;
         for (k=0; k<l && i<hlit; k++,i++)
            last = litHuff[i].len = litHuff[i-1].len;
         spillover = l-k;
         }
      else if (code==17)
         {
         int l = 3 + GetBits(input, bit, 3);
         int k;
         for (k=0; k<l && i<hlit; k++,i++)
            last = litHuff[i].len = 0;
         spillover = l-k;
         }
      else if (code==18)
         {
         int l = 11 + GetBits(input, bit, 7);
         int k;
         for (k=0; k<l && i<hlit; k++,i++)
            last = litHuff[i].len = 0;
         spillover = l-k;
         }
      }
//   for (i=0; i<hlit; i++)
//      printf("\nZIP: LitHuff %d %d",i,litHuff[i].len);
   for (i=0; i<spillover; i++) {
	  if (i>= distHuff.Size())
		 {
         ref.error_out = 8;
         return;
		 }
      distHuff[i].len = last;
	  }
   for (; i<hdist; )
      {
      code = codelens.FetchCode(input, bit);
      if (code==-1)
         {
         ref.error_out = 4;
         return;
         }
      spillover = 0;
      if (code<=15){
         last = distHuff[i].len = code;
         i++;
         }
      else if (code==16)
         {
         int l = 3 + GetBits(input, bit, 2);
         int k;
         for (k=0; k<l && i<hdist; k++,i++)
            distHuff[i].len = last;
         spillover = l-k;
         }
      else if (code==17)
         {
         int l = 3 + GetBits(input, bit, 3);
         int k;
         for (k=0; k<l && i<hdist; k++,i++)
            last = distHuff[i].len = 0;
         spillover = l-k;
         }
      else if (code==18)
         {
         int l = 11 + GetBits(input, bit, 7);
         int k;
         for (k=0; k<l && i<hdist; k++,i++)
            last = distHuff[i].len = 0;
         spillover = l-k;
         }
      }
   if (bit/8 >= input.size())
      {
      ref.error_out = 4;
      return;
      }   
   if (spillover) {
      ref.error_out = 7;
      return;
      }
   litHuff.Transform();
   distHuff.Transform();

   for (i=0; i<hlit; i++)
      b.recodes.push(recodetype(litHuff[i].code, litHuff[i].len));
   for (i=hlit; i<288; i++)
      b.recodes.push(recodetype(0, 0));
   for (i=0; i<hdist; i++)
      b.recodes.push(recodetype(distHuff[i].code, distHuff[i].len));
   for (i=hdist; i<32; i++)
      b.recodes.push(recodetype(0, 0));

   ref.decompblocks.push(b);
   
   BlockProcess(litHuff, distHuff, input, bit, output, ref, reference);
   }


void FixedHuffman(const array<unsigned char> &input, int &bit, array <unsigned char> &output, reftype &ref, const array <unsigned char> *reference)
   {
   huffmantype litHuff(288), distHuff(32);
   int i;

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
   
   litHuff.Transform();
   distHuff.Transform();

   BlockProcess(litHuff, distHuff, input, bit, output, ref, reference);
   }



void unzip(reftype &ref, const array <unsigned char> *reference)
   {
   array <unsigned char>       &output    = ref.output;
   const array <unsigned char> &input     = ref.input;
   int bit=0;
   int blocks=0;


   if (ref.input.size()<3)
      printf("\nZIP: You're asking me to unzip a file with only %d bytes", ref.input.size());
   
   bool final;
   do {
      final = GetBits(input, bit, 1)==1;
//      if (final)
//         printf("\nZIP: Block #%d(final), startbyte %d ", blocks, output.size());
//      else
//         printf("\nZIP: Block #%d, startbyte %d ", blocks, output.size());
      blocks++;

      int a = GetBits(input, bit, 2);
      if (a==-1)
         {
         ref.error_out = 4;
         return;
         }
      if (a==0){
         printf(" no compression");
         if (bit&7)
            bit += 8 - (bit&7);   // skip to next byte
         unsigned short len =GetBits(input, bit, 16);
         unsigned short nlen=GetBits(input, bit, 16);
         int i;

         if (bit/8 >= input.size())
            {
            ref.error_out = 4;
            return;
            }
         if (len != ((~nlen)&0xffff)) {
            ref.error_out = 6;
            return;
            }
            
         for (i=0; i<len; i++)
            {
            if (bit/8 >= input.size())
               {
               ref.error_out = 4;
               return;
               }
            output.push(input[bit/8]);
            bit+=8;
            if (reference!=NULL && output.back() != (*reference)[output.size()-1])
               FATAL_ERROR;
            }
         }
      else if (a==1)
         {
         FixedHuffman(input, bit, output, ref, reference);
         }
      else if (a==2)
         {           
         DynamicHuffman(input, bit, output, ref, reference);
         }   
      else if (a==3){
         printf(" ERROR");
         ref.error_out = 5;
         return;
         }
      if (ref.error_out!=1)
         break;
      } while (!final);
   if (reference!=NULL && output.size() != reference->size())
      FATAL_ERROR;
   ref.eof_out = (ref.error_out==1); // as everything is full-file for now, this is always true
   int i;
   int s1=ref.adler_crc_in&0xffff, s2=(ref.adler_crc_in>>16)&0xffff;
   ref.crc_out= ref.adler_crc_in;
   for (i=0; i<output.size(); i++)
      {
      itu_crc32(ref.crc_out, output[i]);
      s1 += output[i];
      s2 += s1;
      if (s1>=65521)
         s1 -= 65521;
      if (s2>=65521)
         s2 -= 65521;
      if (s1>=65521)
         s1 -= 65521;
      if (s2>=65521)
         s2 -= 65521;
      }
   ref.addler_out = s1 | (s2<<16);   
   ref.total_bits_read = bit;
   if (ref.error_out!=1)
      printf("ZIP: Unzip returned with error of %d\n",ref.error_out);
   }


ZIP_BLOCK_TYPES OutputBlock(array <unsigned char> &output, int &bit, int rawsize, array <matchtype> &matches, blocktype *blockp, bool final, bool force_fixed, bool force_dynamic, const unsigned char *allow_nocompression, bool printit=true);

ZIP_BLOCK_TYPES OutputBlock(array <unsigned char> &output, int &bit, int rawsize, array <matchtype> &matches, blocktype *blockp, bool final, bool force_fixed, bool force_dynamic, const unsigned char *allow_nocompression, bool printit) {
   huffmantype litHuff(288), distHuff(32);
   bool block_present = (blockp != NULL);
   blocktype &block = *blockp;
   int i;
   ZIP_BLOCK_TYPES result = ZIP_BLOCK_RESERVED;
   
   //   if(printit) printf("\nZIP:  Outputblock");
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
         if (k==30) k=29;
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
   
   
   // we need to have 2583 or less symbols to ensure a code length <=15
   int bundles=0;
   bool lit=false;
   for (i=0; i<matches.size(); i++)
      {
      if (lit)
         {
         if (matches[i].literal)
            bundles++;
         else 
            bundles+=2;
         lit =false;
         }
      else {
         if (matches[i].literal)
            lit = true;
         else
            bundles++;
         }
      }
   if (lit) bundles++;

   int divamount = bundles<1024  ? 1 : 
                   bundles<2048  ? 2 : 
                   bundles<4096  ? 4 : 8;

   for (i=0; i<litHuff.Size(); i++)
      {
      int &o = litHuff[i].occurence;
      o = (o+divamount-1)/divamount;
      }
   for (i=0; i<distHuff.Size(); i++)
      {
      int &o = distHuff[i].occurence;
      o = (o+divamount-1)/divamount;
      }
   
   // now that I know occurences I can set lengths
   litHuff.ComputeLengths();
   distHuff.ComputeLengths();
   
   // now compute the cost before building the code to see if fixed huffman is better
   for (i=0; i<litHuff.Size(); i++)
      {
      dynamiccost += litHuff[i].len * litHuff[i].occurence;
      if(block_present) {
         block.frequencies[i] = litHuff[i].occurence;
         block.lengths[i] = litHuff[i].len;
         }
      if (litHuff[i].len >15 && litHuff[i].occurence) 
         FATAL_ERROR;
      }
   if(block_present) {
      for (; i<288; i++)
         block.frequencies[i] = block.lengths[i] = 0;
      }
   for (i=0; i<distHuff.Size(); i++)
      {
      dynamiccost += distHuff[i].len * distHuff[i].occurence;
      if(block_present) {
         block.frequencies[i+288] = distHuff[i].occurence;
         block.lengths[i+288] = distHuff[i].len;
         }
      if (distHuff[i].len >15 && distHuff[i].occurence) 
         FATAL_ERROR;
      }
   dynamiccost *= divamount;    // I earlier divided the occurances by 8 to get under the fibonacci limit
   
   if(block_present) {
      block.dynamiccost = dynamiccost;
      block.fixedcost = fixedcost;
      for (; i<32; i++)
         block.frequencies[i+288] = block.lengths[i+288] = 0;
      }
   //   if (allow_nocompression && dynamiccost > rawsize*8+16 && rawsize<=65535)
   // I don't care about making optimum files, rather I want to randomly create no compress blocks. 
   // This should make nocompress happen 50% of time and doesn't require random number generator
   if (allow_nocompression && ((dynamiccost%7)%3)==1 && rawsize<=65535)
      {
      printf("Rawsize=0x%X\n",rawsize);
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
   else{
      // I'm going to go through and assume dynamic huffman block.
      // When the block header is complete I'll know the true size and if it's bigger than a
      // fixed encoding I'll reset bit
      const int preheaderbit = bit;
      const array <unsigned char> preheaderoutput(output);
      
      
      SetBits(output, bit, 1, final);
      SetBits(output, bit, 2, 2);  // dynamic huffman
      
      const int oldbit = bit;
      array <int> codes;
      array <int> lengths;
      int last=-1;
      for (i=0; i<litHuff.Size() && i<=285; i++) {
         lengths.push(litHuff[i].len);
         //         if(printit) printf("\nZIP: Lit  %d -> %d",i,litHuff[i].len);
         }
      for (i=0; i<distHuff.Size() && i<30; i++) {
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
      int hlit=286, hdist=30, hclen=19;
      
      for (i=0; i<codes.size(); i++)
         codelens[codes[i]&0xff].occurence++;
/*      codelens.ComputeLengths();      
      bool illegal = false;
      for (i=0; i<=18; i++)
         { // this prevents us from exceeding a codelength of 7 
         illegal = illegal || codelens[i].len>7;
         }
      if (illegal){
         for (i=0; i<=18; i++)
            { // this prevents us from exceeding a codelength of 7 
            if (codelens[i].occurence)
               codelens[i].occurence = codelens[i].occurence/2 +1;
            }
         codelens.ComputeLengths();
         }


      int best=0, easy=0, pass1=0;
      for (i=0; i<=18; i++)
         {
         best += codelens[i].len * codelens[i].occurence;
         pass1 += codelens[i].occurence * (
                 (i<=12) ? 4 : 5);
         }*/
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

/*      static bool oneshot=true;
      if (!oneshot)
         {
         for (i=0; i<=18; i++)
            {
            printf("i=%d len=%d code=%x\n", i, codelens[i].len, codelens[i].code);
            }
         oneshot=true;
         }


      for (i=0; i<=18; i++)
         {
         easy += codelens[i].len * codelens[i].occurence;
         }
      if (easy-best>300 &&0)
         {
         printf("Best = %d Easy = %d, delta=%d\n",best,easy,easy-best);
         codelens.ComputeLengths();
         for (i=0; i<=18; i++)
            printf("i=%d occurences=%d len=%d\n",i,codelens[i].occurence,codelens[i].len);
         exit(-1);
         }*/
      
      // To make hardware easier I will use the following static encoding. It's simple and reasonably good (within 20-30 bits of optimal)
/*      for (i=0; i<=12; i++)
         codelens[i].len=4;
      for (; i<=18; i++)
         codelens[i].len=5;*/
      
      
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
      dynamiccost += 74;
      for (i=0; i<codes.size(); i++)
         {         
         int c = codes[i]&0xff;
         SetRBits(output, bit, codelens[c].len, codelens[c].code);
         dynamiccost += codelens[c].len;
         if (c == 16)
            {
            SetBits(output, bit, 2, (codes[i]>>8)-3);
            dynamiccost += 2;
            }
         else if (c == 17){
            SetBits(output, bit, 3, (codes[i]>>8)-3);
            dynamiccost += 3;
            }
         else if (c == 18){
            SetBits(output, bit, 7, (codes[i]>>8)-11);
            dynamiccost += 7;
            }
         else {
            int k;
            for (k=1; k<(codes[i]>>8); k++){
               SetRBits(output, bit, codelens[c].len, codelens[c].code);
               dynamiccost += codelens[c].len;
               }
            }
         
         array<unsigned char> temp;
         int tempbit=0;
         SetRBits(temp, tempbit, codelens[c].len, codelens[c].code);
         if (c == 16)
            SetBits(temp, tempbit, 2, (codes[i]>>8)-3);
         else if (c == 17)
            SetBits(temp, tempbit, 3, (codes[i]>>8)-3);
         else if (c == 18)
            SetBits(temp, tempbit, 7, (codes[i]>>8)-11);
         else {                                   
            int k;
            for (k=1; k<(codes[i]>>8); k++)
               SetRBits(temp, tempbit, codelens[c].len, codelens[c].code);
            }
                  
         temp.push(0);
         if(block_present) {
            block.alphalens.push(temp[0] | (temp[1]<<8)  | (tempbit<<16));
            }
         }
//      if(printit) printf("ZIP: Dynamic Huffman selected, cost is %d bits\n",bit-oldbit);
      result = ZIP_BLOCK_DYNAMIC_HUFFMAN;      
      
      
      if ((!force_dynamic && dynamiccost > fixedcost) || force_fixed) 
         {
         bit = preheaderbit; // back out dynamic header
         output = preheaderoutput;
         SetBits(output, bit, 1, final);
         SetBits(output, bit, 2, 1);  // fixed huffman
         if(block_present) {
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
         //      if(printit) printf("ZIP: Static Huffman selected\n");
         result = ZIP_BLOCK_FIXED_HUFFMAN;
         }
      }
      
   litHuff.Transform();
   distHuff.Transform();
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
         temp.push(0);
         temp.push(0);
         temp.push(0);
         temp.push(0);
         temp.push(0);
         temp.push(0);
         if(block_present) {
            block.recodes.push(temp[0] | (temp[1]<<8) | (temp[2]<<16) | ((__int64)temp[3]<<24) | ((__int64)temp[4]<<32) | ((__int64)temp[5]<<40) | ((__int64)temp[6]<<48) | ((__int64)tempbit<<56));
            }
         tempbit=0;
         temp.clear();
         skippedlast = false;
         }
      else
         skippedlast = true;
      }
      {
      array<unsigned char> temp;
      int tempbit=0;
      SetRBits(temp, tempbit, litHuff[256].len, litHuff[256].code);
      temp.push(0);
      temp.push(0);
      temp.push(0);
      temp.push(0);
      temp.push(0);
      temp.push(0);
      if(block_present) {
         block.recodes.push(temp[0] | (temp[1]<<8) | (temp[2]<<16) | ((__int64)temp[3]<<24) | ((__int64)temp[4]<<32) | ((__int64)temp[5]<<40) | ((__int64)temp[6]<<48) | ((__int64)tempbit<<56));
         }
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

static int evenoddhistogram[13], lengthhistogram[260];

int Cost(int len, int dist)
   {
   int ret=0, i;

   for (i=0; i<29; i++)
      if (lengths[i].len >= len) {
         ret += lengths[i].extra;
         break;
         }
   for (i=0; i<30; i++)
      if (distances[i].len >= dist) {
         ret += lengths[i].extra;
         break;
         }
   return ret;
   }





// the original, before split into per-byte routines
void zip(reftype &ref)
   {
   array <unsigned char>       &output    = ref.output;
   const array <unsigned char> &input     = ref.input;
   const int inputlen                     = ref.input.size();
   
   int bit=0, i, k, startblock=0;
   array <matchtype> matches;
   zhashtype zhash;
   int searches=0, totalsearches=0;
   int search_cycles=0;
   int blocks=0;
   int matchbundles=0;
      
   output.clear();
   SetBits(output, bit, ref.extra_len_in, ref.extra_in);
   
   
   // now start processing data   
   if (ref.historybytes >= inputlen)
      {
//      printf("ZIP: Inputlen=%d historybytes=%d", inputlen, ref.historybytes);
      FATAL_ERROR;
      }
   for (i=0; i<ref.historybytes; i++)
      {
      array <int> odd, even;
      zhash.Check(input, i, ref, true, odd, even);
      zhash.Add(input, i);
      }
   
   int skipto=-1;
   for (; i<inputlen; )
      {      
      if (i<skipto){
         array <int> odd, even;
         zhash.Check(input, i, ref, false, odd, even);
         zhash.Add(input, i);
         i++;
         }
      else if (i>=skipto) 
         {
         array <int> possibles;
         int starti=i, num=0, lazy=0, bestentry=-1, maxlen=0, dist=-1, l, k, location;
         
         while (i+lazy<inputlen && lazy<3)
            {
            array <int> odd, even;
            zhash.Check(input, i+lazy, ref, false, odd, even);
            zhash.Add(input, i+lazy);

            for (k=0; k<odd.size(); k++)
               possibles.push(odd[k] | (lazy<<16));
            for (k=0; k<even.size(); k++)
               possibles.push(even[k] | (lazy<<16));
            lazy++;
            num += MAXIMUM(odd.size(), even.size());
            if (odd.size()==0 && even.size()==0) break;
            }
         
         for (k=0; k<possibles.size(); k++)
            {
            int entry = possibles[k]>>16;
            if (entry>=lazy) FATAL_ERROR;
            i = starti + entry;
            
            location = (possibles[k] & 0x7fff) | (i&0xffff8000);
            if (location >= i)
               location -= 32768;
            for (l=0; l<258 && location>=i-32768 && location<i && location>=0 && i+l<input.size() && input[i+l]==input[location+l]; l++)
               ;
               
            if (i-location >= MAX_DIST) 
               l=0;          // for imp reasons I can't use entire 32K history window
            bool better=false;// = l>=4 && bestentry<0;
            better = better || (l>maxlen+entry-bestentry);
            better = better || (l>=maxlen+entry-bestentry && (i-location)<dist);

//            if (l<4) better=false;
               
            if (better){
               maxlen    = l;
               dist      = i - location;
               bestentry = entry;
               }
            }
         if (bestentry<0 || maxlen<4)
            {
            for (k=0; k<lazy; k++)
               {
               matches.push(matchtype(input[k+starti]));
               matchbundles++;
               }            
            skipto = starti+k;
            }
         else {
            for (k=0; k<bestentry; k++)
               {
               matches.push(matchtype(input[k+starti]));
               matchbundles++;
               }
            matches.push(matchtype(maxlen, dist));
            matchbundles += (matchbundles&1)+2;
            skipto = starti+bestentry+maxlen;
            }
         i=starti+lazy;
         }


      if (matchbundles>=(ref.allow_nocompression ? BLOCKSIZE/16 :BLOCKSIZE) && i+1<inputlen && skipto<inputlen) {
//         printf("ZIP: Block #%d %d bytes.\n",blocks,i-startblock);
         array<matchtype> deferred;
         while (matchbundles>(ref.allow_nocompression ? BLOCKSIZE/16 :BLOCKSIZE))
            {
			// I'm not proud of reiterating to calculate the correct matchbundles
            deferred.push(matches.back());
            matches.pop_back();
			matchbundles=0;
			for (k=0; k<matches.size(); k++)
			   {
			   if (matches[k].literal)
				  matchbundles++;
			   else
				  matchbundles += (matchbundles&1)+2;
			   }
            }
         ref.matches += matches; 
         ref.blocks.push(blocktype());
         OutputBlock(output, bit, i-startblock, matches, &(ref.blocks.back()), false, ref.force_fixed, ref.force_dynamic, ref.allow_nocompression ? &input[startblock] : NULL);
         matches = deferred;
		 matchbundles=0;
		 for (k=0; k<matches.size(); k++)
			{
			if (matches[k].literal)
			   matchbundles++;
			else
			   matchbundles += (matchbundles&1)+2;
			}
         deferred.clear();
         search_cycles/=2;
//         printf("ZIP: Hash widget: %.2fb/cycle, History widget: %d (%.2f/Byte) total searches needed. %.2fb/cycle.\n", (i-startblock)*8.0/zhash.cycle, totalsearches, totalsearches*1.0/(i-startblock), (i-startblock)*8.0/search_cycles);
         zhash.cycle = 0;
         zhash.totalbanks[0]=zhash.totalbanks[1]=zhash.totalbanks[2]=zhash.totalbanks[3]=0;
         
         searches=0;
         totalsearches=0;
         search_cycles=0;
         startblock = i+1;
         blocks++;
         }
      }
   while (matches.size()!=0){
      // there are some degenerate cases where I'll have a single string left over. It then gets forced to
      // be a block all to it's self
//      printf("ZIP: Block #%d %d bytes.\n",blocks,i-startblock);
      array<matchtype> deferred;
      while (matchbundles>(ref.allow_nocompression ? BLOCKSIZE/16 :BLOCKSIZE))
         {
         deferred.push(matches.back());
         matches.pop_back();
		 matchbundles=0;
		 for (k=0; k<matches.size(); k++)
			{
			if (matches[k].literal)
			   matchbundles++;
			else
			   matchbundles += (matchbundles&1)+2;
			}
         }
      ref.matches += matches;
      ref.blocks.push(blocktype());
      OutputBlock(output, bit, i-startblock, matches, &(ref.blocks.back()), deferred.size()==0 && ref.eof, ref.force_fixed, ref.force_dynamic, ref.allow_nocompression ? &input[startblock] : NULL);
      matches.clear();
      matches = deferred;
	  matchbundles=0;
	  for (k=0; k<matches.size(); k++)
		 {
		 if (matches[k].literal)
			matchbundles++;
		 else
			matchbundles += (matchbundles&1)+2;
		 }
      deferred.clear();
      search_cycles/=2;
      //   printf("ZIP: %d (%.2f/Byte) searches needed. %d (%.2f/Byte) total searches needed.  %.2fb/cycle needed.\n", searches, searches*1.0/(i-startblock), totalsearches, totalsearches*1.0/(i-startblock), (i-startblock)*8.0/search_cycles);
//      printf("ZIP: Hash widget: %.2fb/cycle, History widget: %d (%.2f/Byte)total searches needed. %.2fb/cycle.\n", (i-startblock)*8.0/zhash.cycle, totalsearches, totalsearches*1.0/(i-startblock), (i-startblock)*8.0/search_cycles);
      zhash.cycle = 0;
      zhash.totalbanks[0]=zhash.totalbanks[1]=zhash.totalbanks[2]=zhash.totalbanks[3]=0;
      
      searches=0;
      totalsearches=0;
      search_cycles=0;
      startblock = i+1;
      blocks++;
      }
   
   ref.exbits_out = 0;
   ref.bytes_out = ref.output.size();
   if(ref.eof) {
      ref.bytes_out = ref.output.size();
      ref.exnum_out = 0;
   }
   else {
      ref.exnum_out = bit & 0x7;
      if(ref.exnum_out) {
         ref.bytes_out --;
         ref.exbits_out = ref.output[ref.bytes_out];
      }
   }
   int s1=ref.adler_crc_in&0xffff, s2=(ref.adler_crc_in>>16)&0xffff;
   ref.crc_out= ref.adler_crc_in;
   for (i=ref.historybytes; i<inputlen; i++)
      {
      itu_crc32(ref.crc_out, input[i]);
      s1 += input[i];
      s2 += s1;
      if (s1>=65521)
         s1 -= 65521;
      if (s2>=65521)
         s2 -= 65521;
      if (s1>=65521)
         s1 -= 65521;
      if (s2>=65521)
         s2 -= 65521;
      }
   ref.addler_out = s1 | (s2<<16);   

//   printf("ZIP: %d -> %d compression ratio = %.2f\n",input.size(), output.size(), 1.0*input.size()/output.size());
   }

void PrintHistogram()
   {
   int total=0, i;

   for (i=0; i<13; i++)
      {
      total += evenoddhistogram[i];
      }
   for (i=0; i<13; i++)
      {
      printf("Odd/Even %2d -> %6d %.3f\n",i,evenoddhistogram[i],(float)100.0*evenoddhistogram[i]/total);
      }
   total=0;
   for (i=0; i<260; i++)
      {
      total += lengthhistogram[i];
      }
   for (i=0; i<260; i++)
      {
      printf("Length  %3d -> %6d %.3f\n",i,lengthhistogram[i],(float)100.0*lengthhistogram[i]/total);
      }
   }



void DebugBreakFunc(const char *module, int line)
   {
   printf("\nFatal error in module %s line %d\n", module, line);
#ifdef _MSC_VER
   *(int *)0 = 0;    // this will get the debugger's attention
#else
   ASSERT(0);
#endif
   }



