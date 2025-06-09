#ifndef __ZIP_H_INCLUDED__
#define __ZIP_H_INCLUDED__

#include "template.h"
#include <assert.h>


struct hsh_bundletype{
   int valid;
   int even[3];
   int odd[3];
   hsh_bundletype(int _valid, int _e0, int _e1, int _e2, int _o0, int _o1, int _o2) : valid(_valid)
      {
      even[0] = _e0 & 0x7fff;
      even[1] = _e1 & 0x7fff;
      even[2] = _e2 & 0x7fff;
      odd[0]  = _o0 & 0x7fff;
      odd[1]  = _o1 & 0x7fff;
      odd[2]  = _o2 & 0x7fff;
      }
   };

 struct matchtype{
   unsigned literal :1;
   unsigned value_len :12;
   unsigned distance : 16;
   matchtype(unsigned char _literal) : literal(true), value_len(_literal), distance(0)
      {}
   matchtype(unsigned _len, unsigned _distance) : literal(false), value_len(_len), distance(_distance)
      {}
   };

struct blocktype{
   int frequencies[320];
   int lengths[320];
   int codes[320];
   int dynamiccost, fixedcost;
   array <unsigned> alphalens;   // bits 0-15 is code, 16-19 is bit length
   array <uint64_t> recodes;     // bits 0-55 is code, 56-63 is bit length
   };

struct recodetype{
   unsigned code;
   unsigned len;
   recodetype()
      {}
   recodetype (unsigned _code, unsigned _len) : code(_code), len(_len)
      {}
   };

struct decompblocktype{  // this structure only exists for dynamic blocks
   recodetype alphacodes[19];
   array <recodetype> recodes;     // bits 0-55 is code, 56-63 is bit length
   };


class zhashtype;
class huffmantype;

#define BLOCKSIZE 0x3ff8 // this needs to be less than 4096 due to the occurances in inode tree being 14b
#define BLOCKSIZE_FIFO 0x800 // the 2K compress FIFO

typedef enum {
   ZIP_BLOCK_NOCOMPRESS = 0,
   ZIP_BLOCK_FIXED_HUFFMAN = 1,
   ZIP_BLOCK_DYNAMIC_HUFFMAN = 2,
   ZIP_BLOCK_RESERVED = 3
} ZIP_BLOCK_TYPES;

struct decomp_monitortype{
   int cycles;
   int s_num, i_num, o_num, h_num, s_total, block_num, oldstate;
   bool done, partial_decomp;

   int64_t context[192];
   
   decomp_monitortype()
      {
      done = false;
      partial_decomp = false;
      s_num=0;
      i_num=0;
      o_num=0;
      h_num=0;
      s_total=0;
      block_num=0;
      memset(context, 0, sizeof(context));
      }
   };


typedef union {

   struct {

#define ZIP_C_DUMMY1_VAL 0x3476991
      int dummy1;
      int dummy1_array[99];

      // these fields are both decompression and compression
      uint32_t crc_out;
      uint32_t addler_out;

      // these fields are decompression only
      uint32_t total_bits_read;
      int dyn_huff_bits;
      int extra_bits;
      int num_extra_bits;
      int num_extra_bytes;
      bool finalblock;
      bool blockdone;
      bool blockstarted;
      bool get_dynamic;
      ZIP_BLOCK_TYPES unzipblocktype;
      int nc_len;
      int nc_nlen;
      int total_bytes_out;
      int total_bytes_sent_out;
      bool found_eof;
#define UNZIP_BUFFED_BYTES 8
      int num_buffed_bytes_out;
      unsigned char buffed_bytes_out[UNZIP_BUFFED_BYTES];

#define MAX_DYN_HUFF_BYTES 320 // real max is ~2200 bits - but make it a round number
#define UNZIP_BYTES_BUFFERED_AHEAD 16
#define UNZIP_BYTES_BUFFERED_AHEAD_DH (UNZIP_BYTES_BUFFERED_AHEAD + MAX_DYN_HUFF_BYTES)
#define UNZIP_DYNAMIC_STOP_SIZE 400 // could be anywhere from 512 to ~260 + UNZIP_BUFFED_BYTES
      unsigned char dyn_huff_bytes[MAX_DYN_HUFF_BYTES];
      // note that extra_bytes isn't really an array -> it backs up and
      // shares space with the dyn_huff_bytes array ... but only when
      // those bytes in dyn_huff_bytes are not needed
      unsigned char extra_bytes[UNZIP_BYTES_BUFFERED_AHEAD+1];

#define ZIP_C_DUMMY2_VAL 0x3442991
      int dummy2;
      int dummy2_array[99];

#define ZIP_C_DUMMY3_VAL 0x3442471
      int dummy3;

   } s;

#define DECOMPRESSION_CONTEXT_WORDS 192
   uint64_t u64[DECOMPRESSION_CONTEXT_WORDS];

} DECOMPRESSION_CONTEXT;

struct reftype {

   DECOMPRESSION_CONTEXT c;

   array <unsigned char> input;
   array <unsigned char> output;
   array <int> input_byte_xref;       // this will be the minimum byte # from the input side required to produce this output byte
   int inputlen;

   // these fields are compression only
#ifndef __REF_MODEL_RUN__
   array <hsh_bundletype> bundles;
   array <blocktype> blocks;
#endif
   bool lzs_compression_features;  // flag for o63 pass2 functionality(storage mode + lzs)
   bool force_fixed;
   bool force_dynamic;
   bool compression_fastest;
   bool storage;
   bool lzs;
   bool syncflush;
   unsigned extra_in;
   unsigned extra_len_in;
   bool allow_nocompression;  // this is for decomp generation only. Hardware can't do this
   bool benchmarking;          // this is used in the testbench to disable in/out dead cycles
   const char *filename;       // this is a convenient place to pass the input filename to monitor
   int exnum_out;
   int exbits_out;
#define ZIP_BYTES_BUFFERED_AHEAD 260 // Real number may be 258
   int startblock;
   array <matchtype> *lmatches;
   zhashtype *zhash;
   int searches;
   int totalsearches;
   int search_cycles;
   int matchbundles;
   int lastlen;
   int skipto;
   int completelyskip;
   int z_i;
#define MAX_BLOCKS_PENDING 3
   int nblocksp;
   struct {
      int bmatchb;
      int output_bits;
      bool force_fixed;
      ZIP_BLOCK_TYPES type;
   } block_data[MAX_BLOCKS_PENDING];
   int lmatchb;

   // these fields are decompression only
   array <decompblocktype> decompblocks;
   bool eof_out;
   bool reloading_dyn_context;  // dynamic Huffman state is being reloaded from the context
   int dyn_huff_bit;
   int bytes_in;
   int output_bytes_left;
   bool nodynamicstop;
   bool unzipdone;
   bool historyneeded;
   uint32_t total_bits_read;
   huffmantype *litHuff;
   huffmantype *distHuff;

   // these fields are both decompression and compression
   bool decompression;
   array <matchtype> matches;
   unsigned adler_crc_in;
   unsigned error_out;
   int error_position;
   bool eof;
   int bytes_out;
   unsigned short historybytes;
   int bit;
   int nblocks;
   int input_so_far;
   int lastliteral;
   int backup_hash;
   decomp_monitortype dm;
   char m_debug_name[0x40];

   reftype() {
      if(sizeof(DECOMPRESSION_CONTEXT) != (DECOMPRESSION_CONTEXT_WORDS*sizeof(uint64_t))) FATAL_ERROR;
	  m_debug_name[0] = 0;
      lzs_compression_features = false;
      decompression = false;
      force_fixed = false;
      force_dynamic = false;
      storage       = false;
      lzs           = false;
      allow_nocompression = false;
      syncflush     = false;
      adler_crc_in = 0;
      c.s.addler_out = 0;
      c.s.crc_out = 0;
      extra_in = 0;
      extra_len_in = 0;
      eof = true;
      historybytes = 0;
      error_out = 1;
      error_position = 0;
      lastliteral = -1;
      backup_hash = 0;
      benchmarking=false;
      compression_fastest = false;
      filename = "";
   }
};

int GetBitsMSB(reftype &ref, int num);
int GetBits(reftype &ref, int num);
int GetRBits(reftype &ref, int num);
void SetBitsMSB(array <unsigned char> &output, int &bit, int num, unsigned value);
void SetBits(array <unsigned char> &output, int &bit, int num, unsigned value);
void SetRBits(array <unsigned char> &output, int &bit, int num, unsigned value);



#define zip_crc(a)(((a&0x1)      ?0xd678d:0)^((a&0x2)      ?0xacf1a:0)^((a&0x4)      ?0x48383:0)^((a&0x8)      ?0x90706:0)^((a&0x10)      ?0x20e0c:0)^((a&0x20)      ?0x501af:0)^((a&0x40)      ?0xa035e:0)^((a&0x80)      ?0x406bc:0)^\
                   ((a&0x100)    ?0xd9ab7:0)^((a&0x200)    ?0xa28d9:0)^((a&0x400)    ?0x54c05:0)^((a&0x800)    ?0xa980a:0)^((a&0x1000)    ?0x42da3:0)^((a&0x2000)    ?0x946f1:0)^((a&0x4000)    ?0x39055:0)^((a&0x8000)    ?0x63d1d:0)^\
                   ((a&0x10000)  ?0x8ac87:0)^((a&0x20000)  ?0x1590e:0)^((a&0x40000)  ?0x2b21c:0)^((a&0x80000)  ?0x56438:0)^((a&0x100000)  ?0xac870:0)^((a&0x200000)  ?0x590e0:0)^((a&0x400000)  ?0xb21c0:0)^((a&0x800000)  ?0x64380:0)^\
                   ((a&0x1000000)?0x9c1dc:0)^((a&0x2000000)?0x29e0f:0)^((a&0x4000000)?0x421a9:0)^((a&0x8000000)?0x84352:0)^((a&0x10000000)?0x19b13:0)^((a&0x20000000)?0x33626:0)^((a&0x40000000)?0x66c4c:0)^((a&0x80000000)?0xcd898:0))

  const int fifo = 6;
  const int mask = 0xfff;
  const int faststorage_drop_threshold=4; // worst case hash performance = 12/drop_threshold *4b/cycle,  12b/cycle
  const int storage_drop_threshold=5;     // worst case hash performance = 12/drop_threshold *4b/cycle , 10b/cycle
  const int fastest_drop_threshold=6;     // worst case hash performance = 12/drop_threshold *4b/cycle , 8b/cycle
  const int slow_drop_threshold=7;        // worst case hash performance = 12/drop_threshold *4b/cycle , 6.5b/cycle
class zhashtype{
public:
   array<array <unsigned> >hash;
   int banks[8], cycle;
   int lasttriple, lastloc;
   int totalbanks[8];
   array<int> queue;
   array<int> last_odd, last_even;
  
   void Check(const array <unsigned char> &input, int location, reftype &ref, bool squash_valid, array<int> &odd, array<int> &even)
      {      
      unsigned char c0=input[location+0];
      unsigned char c1=input.size()<=(location+1) ? 0 : input[location+1];
      unsigned char c2=input.size()<=(location+2) ? 0 : input[location+2];
      unsigned char c3=input.size()<=(location+3) ? 0 : input[location+3];


      if (ref.lzs && !ref.storage && ref.lzs_compression_features) // lzs+speed<2 allow 2B strings, otherwise lzs has 3B strings
         {
         c2=0;
         c3=0;
         }
      else if ((ref.lzs || ref.force_fixed) && ref.lzs_compression_features)
         c3 = 0;

      int triple = (c0<<16)|(c1<<8)|(c2<<0);
      int quad   = (c0<<24)|(c1<<16)|(c2<<8)|(c3<<0);
      int crc    = zip_crc(quad);
      int index  = crc & mask;
      int speed  = (ref.storage ? 2:0) + (ref.compression_fastest ? 1:0); 
      uint tag   = (crc>>12) & 0x3f;
      int b = index&7;
      int f, i;
      array <int> bankcounts(8, 0);
      const int drop_threshold = (ref.storage&ref.compression_fastest) ? faststorage_drop_threshold : ref.storage ? storage_drop_threshold : ref.compression_fastest ? fastest_drop_threshold : slow_drop_threshold;
      
      if (ref.lzs_compression_features)
         { // o63 pass2
         for (i=0; i<queue.size(); i++)
            {
            if (queue[i]>=0)
               bankcounts[queue[i]]++;
            }
         bankcounts[b]++;
	 bool drop=false;
         for (i=0; i<bankcounts.size(); i++)
            {
            if (bankcounts[i]> drop_threshold)
	       drop = true;
	    }
	 if (drop)
	    {
            if ((location-1)&0x10)
               odd.push((location-1)&0x7fff);
            else
               even.push((location-1)&0x7fff);
            }
//         printf("location=%x quad=%x, crc=%x, b=%d, bankcount[b]=%d drop=%s lzs=%d fixed=%d\n",location,quad,crc,b,bankcounts[b],drop?"true":"false",ref.lzs,ref.force_fixed);
	 }
      else
         { // o63 pass1, 90nm, 130nm parts
         if (lasttriple == triple && location-lastloc<=4)
            {
            if (location-lastloc!=1){
               if ((location&0x3f)==0)
                  odd.push((location+63)&0x7fff);   // goofy case to match implementation
               else if ((location-1)&8)
                  odd.push((location-1)&0x7fff);
               else
                  even.push((location-1)&0x7fff);
               }
            }
         else if (lasttriple == triple)
            {
            lastloc = location-1;
            }
         else {
            lasttriple = triple;
            lastloc = location;
            }
	}

      if (!odd.size() && !even.size()){
         banks[b]++;
         totalbanks[b]++;
         while (banks[0] >= fifo || banks[1] >= fifo || banks[2] >= fifo || banks[3] >= fifo || banks[4] >= fifo || banks[5] >= fifo || banks[6] >= fifo || banks[7] >= fifo)
            {
            banks[0] = MAXIMUM(banks[0]-1, 0);
            banks[1] = MAXIMUM(banks[1]-1, 0);
            banks[2] = MAXIMUM(banks[2]-1, 0);
            banks[3] = MAXIMUM(banks[3]-1, 0);
            banks[4] = MAXIMUM(banks[4]-1, 0);
            banks[5] = MAXIMUM(banks[5]-1, 0);
            banks[6] = MAXIMUM(banks[6]-1, 0);
            banks[7] = MAXIMUM(banks[7]-1, 0);
            cycle+=2;
            }
         
         int evennum=0, oddnum=0;
         for (f=0; f<fifo; f++)
            {
            array <unsigned> &h = hash[f];
            if (h[index]>>16 == tag)
               {
               int pos = (h[index] & 0x7fff) | (location&0xffff8000);
               if (pos >= location)
                  location -= 32768;

               if (ref.lzs_compression_features && speed==3 && (location&1))
	          ;
               else if ((h[index]&0x10) && oddnum<1 && ref.lzs_compression_features && speed==3) {
                  odd.push(h[index]&0x7fff);
                  oddnum++;
                  }
               else if (!(h[index]&0x10) && evennum<1 && ref.lzs_compression_features && speed==3) {
                  even.push(h[index]&0x7fff);
                  evennum++;
                  }
	       else if ((h[index]&0x10) && oddnum<3-speed && ref.lzs_compression_features) {
                  odd.push(h[index]&0x7fff);
                  oddnum++;
                  }
               else if (!(h[index]&0x10) && evennum<3-speed && ref.lzs_compression_features) {
                  even.push(h[index]&0x7fff);
                  evennum++;
                  }
               else if ((h[index]&8) && oddnum<3 && !ref.lzs_compression_features) {
                  odd.push(h[index]&0x7fff);
                  oddnum++;
                  }
               else if (!(h[index]&8) && evennum<3 && !ref.lzs_compression_features) {
                  even.push(h[index]&0x7fff);
                  evennum++;
                  }
               }
            }
         }
      if (odd.size() || even.size())
         ref.searches+=odd.size() + even.size();
      int valid=0;
      if (even.size()==1) valid=4;
      if (even.size()==2) valid=6;
      if (even.size()==3) valid=7;
      if (odd.size()==1)  valid|=32;
      if (odd.size()==2)  valid|=48;
      if (odd.size()==3)  valid|=56;
      if (squash_valid) valid=0;
#ifndef __REF_MODEL_RUN__
      ref.bundles.push(hsh_bundletype(valid, even.size()>=1 ? even[0]:0, even.size()>=2 ? even[1]:0, even.size()>=3 ? even[2]:0, odd.size()>=1 ? odd[0]:0, odd.size()>=2 ? odd[1]:0, odd.size()>=3 ? odd[2]:0));
#endif
      last_odd  = odd;
      last_even = even;
      return;
      }
   void Add(const array <unsigned char> &input, int location)
   { reftype ref;
	 ref.lzs = ref.lzs_compression_features = ref.storage = 0;	// unnecessary
	 Add(input, location, ref);
   }
   void Add(const array <unsigned char> &input, int location, reftype &ref)
      {
      unsigned char c0=input[location+0];
      unsigned char c1=input.size()<=(location+1) ? 0 : input[location+1];
      unsigned char c2=input.size()<=(location+2) ? 0 : input[location+2];
      unsigned char c3=input.size()<=(location+3) ? 0 : input[location+3];

      if (ref.lzs && !ref.storage && ref.lzs_compression_features) // lzs+speed<2 allow 2B strings, otherwise lzs has 3B strings
         {
         c2=0;
         c3=0;
         }
      else if ((ref.lzs || ref.force_fixed) && ref.lzs_compression_features)
         c3 = 0;
      int triple = (c0<<16)|(c1<<8)|(c2<<0);
      int quad   = (c0<<24)|(c1<<16)|(c2<<8)|(c3<<0);
      int crc   = zip_crc(quad);
      int index = crc & mask;
      int tag   = (crc>>12) & 0x3f;
      int b = index&7;
      array <int> bankcounts(8, 0);
      int i;
      const int drop_threshold = (ref.storage&ref.compression_fastest) ? faststorage_drop_threshold : ref.storage ? storage_drop_threshold : ref.compression_fastest ? fastest_drop_threshold : slow_drop_threshold;
      
      for (i=0; i<queue.size(); i++)
         {
         if (queue[i]>=0)
            bankcounts[queue[i]]++;
         }
      bankcounts[b]++;
      for (i=queue.size()-1; i>0; i--)
         queue[i] = queue[i-1];
      queue[0] = b;
      for (i=0; i<bankcounts.size(); i++)
         {
         if (bankcounts[i]> drop_threshold && ref.lzs_compression_features)
            {
            queue[0]= -1;
            return;
            }
         }
      if (location-lastloc<=4 && location-lastloc>1 && lasttriple == triple && !ref.lzs_compression_features)
         {
         queue[0]= -1;  
         return;
         }
      for (int f=fifo-1; f>=1; f--)
         {
         hash[f][index] = hash[f-1][index];
         }
      hash[0][index] = (location&0xffff) | (tag<<16);
      }
   zhashtype() : hash(fifo)
      {
      int f, i;
      for (f=0; f<fifo; f++){
         hash[f].resize(mask+1);
         for (i=0; i<mask+1; i++)
            hash[f][i] = 0;
         }

      banks[0] = banks[1] = banks[2] = banks[3] = banks[4] = banks[5] = banks[6] = banks[7] = 0;
      totalbanks[0] = totalbanks[1] = totalbanks[2] = totalbanks[3] = totalbanks[4] = totalbanks[5] = totalbanks[6] = totalbanks[7] = 0;
      cycle = 0;
      lasttriple = 0; 
      lastloc = -100;
      queue.resize(12, -1);
      }
   };





struct hcodetype{
   int len, code, occurence;
   hcodetype() : len(0), code(0), occurence(0)
      {}
   bool operator<(const hcodetype &right) const
      {
      if (occurence < right.occurence) return true;
      if (occurence > right.occurence) return false;
      if (code < right.code) return true;
      return false;
      }
   };

struct inodetype{
   int child0, child1;
   int occurance;
   inodetype(int _child0, int _child1, int _occurance) : child0(_child0), child1(_child1), occurance(_occurance)
      {}
   };


class huffmantype{
public:   
   array <hcodetype> huffs;
   const int num;

public:   
   huffmantype(int _num) : num(_num)
      {
      int i;
      for (i=0; i<num; i++)
         huffs.push(hcodetype());
      }
   int Size()
      {
      return num;
      }
   bool Transform() // this will convert lengths into codes	(true for error)
      {
      int code, i;
      array <int> blcount(16, 0);  // from the spec I know I can never have a code longer than 15b
      array <int> nextcode(16, 0);
     
      int sum=0, total_codes=0;
      for (i=0; i<num; i++)
         if (huffs[i].len)
            {
            sum += 1<<(20-huffs[i].len);
	    total_codes++;
            }
      if (sum!=(1<<20) && total_codes>1)
         return true;	 // this verifies that the codes are complete with no gaps or duplicates
      for (i=0; i<num; i++)
         blcount[huffs[i].len]++;
      
      code = 0;
      blcount[0] = 0;
      for (i = 1; i <= 15; i++) {
         code = (code + blcount[i-1]) << 1;
         nextcode[i] = code;
         }
      for (i=0; i<num; i++)
         {
         if (huffs[i].len)
            {
            huffs[i].code = nextcode[huffs[i].len];
            nextcode[huffs[i].len]++;
			if (huffs[i].code>>huffs[i].len)
			   return true;		// I overflowed my codes. This must be a set of bogus codes
            }
         }
      for (i=0; i<num; i++)
         {
         char s[20];
         int k;
         hcodetype &h = huffs[i];
         for (k=0; k<h.len; k++)
            s[h.len-k-1] = ((h.code>>k)&1) ? '1' : '0';
         s[k]=0;
//         if (h.len)
//            printf("\nCodelengths 0x%x -> %d %s %x", i, h.len, s, h.code);
         }
      for (i=0; i<num; i++)
         {
         int k, code, len;
         
         for (len=1; len<=huffs[i].len; len++)
            {
            code = huffs[i].code>>(huffs[i].len-len);
            
            for (k=0; k<num; k++)
               {
               if (huffs[k].len == len && huffs[k].code==code && k!=i && huffs[i].len)
                  FATAL_ERROR;
               }
            }
         }
	  return false;
      }
   void ComputeLengths()
      {
      array <hcodetype> temp = huffs;
      array <inodetype> inodes;
      int i, t;
      for (i=0; i<temp.size(); i++)
         temp[i].code = i;
      temp.Sort();
      for (t=0; temp[t].occurence==0; t++)
         temp[t].len=0;      // skip over zero
      
      if (t+2 > num) FATAL_ERROR;  // I need at least 2 entries
      i=0;
      while (t<num || i<inodes.size()-1)
         {
         unsigned t0 = (t+0)<num  ? temp[t].occurence  : 0xffffffff;
         unsigned t1 = (t+1)<num  ? temp[t+1].occurence  : 0xffffffff;
         unsigned i0 = (i+0)<inodes.size() ? inodes[i+0].occurance : 0xffffffff;
         unsigned i1 = (i+1)<inodes.size() ? inodes[i+1].occurance : 0xffffffff;

         if (t1 < i0)
            {
            inodes.push(inodetype(t, t+1, t0+t1));
            t+=2;
            }
         else if (i1 < t0)
            {
            inodes.push(inodetype(i|0x1000, (i+1)|0x1000, i0+i1));
            i+=2;
            }
         else 
            {
            inodes.push(inodetype(i|0x1000, t, i0+t0));
            i++;
            t++;
            }
         }
      // now a reverse pass assigning tree depths(code lengths) to everyone. 
      inodes.back().occurance=0;
      for (i=inodes.size()-1; i>=0; i--)
         {
         inodetype &ii = inodes[i];
         if (ii.child0 & 0x1000)
            inodes[ii.child0 &0xfff].occurance = ii.occurance+1;
         else
            huffs[temp[ii.child0].code].len = ii.occurance+1;
         if (ii.child1 & 0x1000)
            inodes[ii.child1 &0xfff].occurance = ii.occurance+1;
         else
            huffs[temp[ii.child1].code].len = ii.occurance+1;
         }
      int sum=0;
      for (i=0; i<num; i++)
         if (huffs[i].len)
            sum += 1<<(20-huffs[i].len);
//      if (sum!=(1<<20))
//         FATAL_ERROR;
      }
		// copied from zip.cpp to make pass1.xx work
   static int GetBitsMSB(reftype &ref, int num) 
   { // -1 on error
   int ret=0;

   if(ref.reloading_dyn_context) {
      assert(!ref.c.s.get_dynamic);
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



   int FetchCode(reftype &ref) const
      {
      int len, k, code;
      for (len=1, code=0; len<20; len++)
         {
         int b = ref.lzs ? GetBitsMSB(ref, 1) : GetRBits(ref, 1);
         if (b==-1)
            return -1;

         code = (code<<1) + b;

         for (k=0; k<num; k++)
            {
            if (huffs[k].len == len && huffs[k].code==code)
               return k;
            }
         }
      return -1;
      }
   void SortOccurences()
      {
      }
   hcodetype &operator[](int index) 
      {
      return huffs[index];
      }
   };

struct magicheadertype{
   char initials[4];           // Will be Dac!
   unsigned char options;     // [0]=force_fixed, [1]=force_dynamic, [2]=fast, [3]=benchmarking
   unsigned short historybytes; // number of bytes for predefined dictionary
   unsigned adler_crc_in;
   unsigned char extra_in;
   unsigned char extra_len_in;
   };


#pragma pack(1)
struct gzipheadertype{
   unsigned char id1;      // 0x1f
   unsigned char id2;      // 0x8b
   unsigned char cm;       // 0x08
   unsigned char flags;    // 0x00 (no extra fields, no filename, no comment)
   unsigned long mtime;    // 0x00 (no time recorded)
   unsigned char xfl;      // 0x02 (max compression)
   unsigned char os;       // 0x00 (msdos)
   };
#pragma pack(8)


inline void itu_crc32(unsigned int &accum, unsigned char byte)
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


// Global functions and variables

void unzip_byte_init(reftype &ref, int inputlen, bool bof, bool eof, bool nodynamicstop);
void unzip_byte_with_byte(reftype &ref, unsigned char byte, const array <unsigned char> *reference=NULL, bool printit=false);
void unzip_byte(reftype &ref, const array <unsigned char> *reference=NULL, bool printit=false);
void unzip_byte_buffered(reftype &ref, const array <unsigned char> *reference=NULL, bool printit=false);
void unzip_old(reftype &ref, const array <unsigned char> *reference=NULL);
void unzip(reftype &ref, const array <unsigned char> *reference=NULL);
void unzip_adlercrc32(reftype &ref, unsigned char byte);
void unzip_restore(reftype &ref);
void unzip_save(reftype &ref);
void unzip_byte_out(reftype &ref, unsigned char byte, const array <unsigned char> *reference);
void unzip_flush_out(reftype &ref);
void unzip_finish_run(reftype &ref, bool doit, const array <unsigned char> *reference, bool printit);
void unzip_finishit(reftype &ref, const array <unsigned char> *reference, bool printit);
void StaticHuffman(reftype &ref);
void LzsHuffman(reftype &ref);
int DynamicHuffman(reftype &ref);
void zip_byte_init(reftype &ref, int inputlen);
void zip_byte_with_byte(reftype &ref, unsigned char byte, bool printit=false);
void zip_byte(reftype &ref, bool printit=false);
void zip_byte_buffered(reftype &ref, bool printit=false);
void zip(reftype &ref);
void zip_old(reftype &ref);


#endif
