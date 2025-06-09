#include <stdio.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <functional>

#ifndef _MSC_VER
  #include <cvmx.h>
  #define __int64  int64_t
  #define __uint64 uint64_t
#endif



#define MINIMUM(a,b) ( (a<b)? a :b)
#define MAXIMUM(a,b) ( (a>b)? a :b)
#define FATAL_ERROR DebugBreakFunc(__FILE__,__LINE__)
void DebugBreakFunc(const char *module, int line)
   {
   printf("Fatal error at line %d of %s\n",line, module);
   exit(-1);
   }


template<class T> class array
   {
private:
   void CopyConstruct(int index, const T &val)
      {
      new ((void *)&arr[index]) T(val);  // call the copy constructor, don't actually allocate memory
      }
   void Destruct(int index, T*_arr)
      {
//      _arr[index].~T();      
      (_arr+index)->~T();      // this fixes an internal compiler error on g++
      }   

protected:
   T * arr;
   const void *external_mem;
   size_t top, allocated;

public:
   array(void *_external_mem=NULL, size_t external_size=0) : external_mem(_external_mem)
      {
      top = 0;
      if (external_mem)
         {
         arr = (T *)external_mem;
         allocated = external_size/sizeof(T);
         }
      else
         {
         arr = NULL;      
         allocated = 0;
         }
      }
   array(size_t n, const T &val = T(), void *_external_mem=NULL, size_t external_size=0) : external_mem(_external_mem)
      {
      top = 0;
      if (external_mem)
         {
         arr = (T *)external_mem;
         allocated = external_size/sizeof(T);
         }
      else
         {
         arr = NULL;      
         allocated = 0;
         }
      resize(n, val);
      }  

   array(const array &right) : external_mem(NULL)  // copy constructor
      {
      arr = NULL;
      top = 0;
      allocated = 0;
      reserve(right.top);
      for (int i=0; i<right.top; i++)
         CopyConstruct(i, right.arr[i]);
      top = right.top;
      }
   void resize(size_t n, const T &val = T())
      {
      int i;
      if (allocated < n)
         reserve(n);
      for (i=top; i<n; i++)
         CopyConstruct(i, val);
      for (i=n; i<top; i++)
         Destruct(i, arr);
      top = n;
      }
   ~array()
      {
      clear();
      if (arr != external_mem)
         {
         if (arr && allocated==0) FATAL_ERROR;
         delete [](char *)arr;  // typecast to keep destructor from being called twice
         }
      arr=NULL;
      }
   void UnAllocate()
      {
      clear();
      if (arr != external_mem)
         delete [](char *)arr;  // typecast to keep destructor from being called twice
      arr=NULL;
      }
   void reserve(size_t n)
      {
      if (n <= allocated) return;
      if (n < top) FATAL_ERROR;
      T * oldarr = arr;
      int i;
            
      allocated = n;
      arr = (T*)new char[allocated * sizeof(T)];
      if (arr==NULL) FATAL_ERROR;      

      for (i=0; i<top; i++)
         {
         CopyConstruct(i, oldarr[i]);
         Destruct(i, oldarr);
         }
      if (oldarr != external_mem)
         delete [](char *)oldarr;
      oldarr = NULL;
      }
   const T &operator[](size_t index) const
      {
      if (index>=top) FATAL_ERROR;
      
      return arr[index];
      }
   T &operator[](size_t index)
      {
      if (index>=top) FATAL_ERROR;
      
      return arr[index];
      }
   int size() const  // use a signed int instead of size_t to prevent 0-1 >0
      {
      return top;
      }
   int capacity() const
      {
      return allocated;
      }
   void clear()
      {
      for (int i=0; i<top; i++)
         Destruct(i, arr);
      top = 0;
      }
   // push is tricky because val could be an element in this array, after a relocation it would be invalid
   void push(const T &val)
      {
      if (top >= allocated)
         {
         T * oldarr = arr;
         int i;

         if (sizeof(T) <= 64)
            allocated = MAXIMUM(allocated*2, 16);
         else
            allocated = MAXIMUM(allocated*2, 1);
         
         arr = (T*)new char[allocated * sizeof(T)];
         if (arr==NULL) FATAL_ERROR;      
         
         for (i=0; i<top; i++)
            CopyConstruct(i, oldarr[i]);            

         CopyConstruct(top, val);  // must do this before calling destructors
         
         for (i=0; i<top; i++)
            Destruct(i, oldarr);
         if (oldarr != external_mem)
            delete [](char *)oldarr;
         oldarr = NULL;
         }
      else
         CopyConstruct(top, val);
      top++;
      }
   void push_back(const T &val)
      {
      push(val);
      }
   const T &back() const
      {
      if (top==0) FATAL_ERROR;
      return arr[top-1];
      }
   T &back()
      {
      if (top==0) FATAL_ERROR;
      return arr[top-1];
      }
   const T &front() const
      {
      if (top==0) FATAL_ERROR;
      return arr[0];
      }
   T &front()
      {
      if (top==0) FATAL_ERROR;
      return arr[0];
      }
   T *begin()
      {
      return arr + 0;
      }
   T *end()
      {
      return arr + top;
      }
   bool pop(T &ob)
      {
      if (top==0) return false;
      ob = back();
      top--;
      return true;
      }
/*   T pop()      // be careful may be slow
      {
      T temp = back();
      top--;
      Destruct(top, arr);
      return temp;
      }
*/   void pop_back()
      {
      if (top==0) FATAL_ERROR;
      top--;
      Destruct(top, arr);
      }

   // this is necessary because the compiler will change fooarray=5 into fooarray = array<footype>(5)
   void operator=(const int);  // will result in linker error! Fix the code.
   array &operator=(const array &right)
      {
      int i;
      if (this == &right)
         return *this;
      if (allocated < right.top)
         reserve(right.top);
      for (i=0; i<MINIMUM(top, right.top); i++)
         arr[i] = right.arr[i];
      for (i=top; i<right.top; i++)
         CopyConstruct(i, right.arr[i]);
      for (i=right.top; i<top; i++)
         Destruct(i, arr);
      top = right.top;
      return *this;
      }
   array &operator+=(const array &right)
      {
      //Insert(top, right);
      for (int i=0; i<right.top; i++)
         push(right.arr[i]);
      return *this;
      }
   void Sort()
      {
      std::sort(begin(), end());
      }   
   };



void SetBits(unsigned char *output, int &bit, int num, unsigned value)
   {
   int i;

   for (i=0; i<num; i++)
      {
      if ((bit&7) ==0)
         output[bit>>3] = 0;
      if ((value>>i)&1)
         output[bit>>3] |= 1<<(bit%8);
      bit++;
      }
   }
void SetRBits(unsigned char *output, int &bit, int num, unsigned value)
   {
   int i;

   for (i=num-1; i>=0; i--)
      {
      if ((bit&7) ==0)
         output[bit>>3] = 0;
      if ((value>>i)&1)
         output[bit>>3] |= 1<<(bit%8);
      bit++;
      }
   }



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
   void Transform() // this will convert lengths into codes
      {
      int code, i;
      array <int> blcount(16, 0);  // from the spec I know I can never have a code longer than 15b
      array <int> nextcode(16, 0);

      int sum=0, nonzeros=0;
      for (i=0; i<num; i++)
         if (huffs[i].len)
            {
            sum += 1<<(20-huffs[i].len);
            nonzeros++;
            }
//      if (nonzeros>1 && sum!=(1<<20))
//         FATAL_ERROR;
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
      }
   void ComputeLengths()
      {
      array <hcodetype> temp = huffs;
      array <inodetype> inodes;
      int i, t;
      for (i=0; i<temp.size(); i++)
         {
         temp[i].code = i;
         if (temp[i].occurence>=2)
            temp[i].occurence = temp[i].occurence>>1;
         }
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
      }
   hcodetype &operator[](int index) 
      {
      return huffs[index];
      }
   };


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


// this file computes the huffman codes and writes the output to disk
void pass2(const int blocklen, unsigned *blocks, unsigned char *output, int &bit, bool final)
   {
   huffmantype litHuff(288), distHuff(32);
   int i;
   
   for (i=0; i<blocklen; i++)
      {
      unsigned b = blocks[i];
      unsigned len = b>>16;
      unsigned dist = b&0xffff;

      if (len==0)
         litHuff[dist].occurence++;
      else{
         int k;
         for (k=0; k<29; k++)
            {
            if (lengths[k].len == len)
               break;
            if (lengths[k].len > len)
               {k--; break;}
            }
         litHuff[257+k].occurence++;
         for (k=0; k<30; k++)
            {
            if (distances[k].len == dist)
               break;
            if (distances[k].len > dist)
               {k--; break;}
            }
         if (k>=30) k=29;
         distHuff[k].occurence++;
         }
      }
   litHuff[256].occurence++;
   distHuff[0].occurence++;    // this prevents having 1 code and trying to make zero length
   distHuff[2].occurence++;    // it reduced compression a negligable amount for large block
   // now that I know occurences I can set lengths
   litHuff.ComputeLengths();
   distHuff.ComputeLengths();
   
   
   // now compute the cost before building the code to see if fixed huffman is better
   int dynamiccost = 0;
   int fixedcost = 0;
   for (i=0; i<litHuff.Size(); i++)
      {
      if (i<=143)      fixedcost += 8 * litHuff[i].occurence;
      else if (i<=255) fixedcost += 9 * litHuff[i].occurence;
      else if (i<=279) fixedcost += 7 * litHuff[i].occurence;
      else             fixedcost += 8 * litHuff[i].occurence;
      dynamiccost += litHuff[i].len * litHuff[i].occurence;
      // this assertion can never happen in hardware because the blocksize is less than fibonacci sum
      // in the sample code for Yen this may occur due to the much larger blocksize
      if (litHuff[i].len >15 && litHuff[i].occurence) 
         FATAL_ERROR;
      }
   for (i=0; i<distHuff.Size(); i++)
      {
      fixedcost   += 5 * distHuff[i].occurence;
      dynamiccost += distHuff[i].len * distHuff[i].occurence;
      if (distHuff[i].len >15 && distHuff[i].occurence) 
         FATAL_ERROR;
      }
   fixedcost -=10;  // this is to compensate for the bogus distance occurances I added to prevent 0 length codes
   dynamiccost += 700;  // this models the overhead for send the codes out
   
   // check if no compression would be better.
   // for now ignore
/*   if ()
      {
      SetBits(output, bit, 1, final);
      SetBits(output, bit, 2, 0);  // no compression
      }
   else */
   if (fixedcost < dynamiccost)
      {
      SetBits(output, bit, 1, final);
      SetBits(output, bit, 2, 1);  // fixed huffman
      
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
      }
   else{
      SetBits(output, bit, 1, final);
      SetBits(output, bit, 2, 2);  // dynamic huffman
      
      const int oldbit = bit;
      array <int> codes;
      array <int> lengths;
      int last=-1;
      for (i=0; i<litHuff.Size() && i<=285; i++) {
         lengths.push(litHuff[i].len);
//         printf("\nLit  %d -> %d",i,litHuff[i].len);
         }
      for (i=0; i<distHuff.Size() && i<=29; i++) {
         lengths.push(distHuff[i].len);
//         printf("\nDist %d -> %d",i,distHuff[i].len);
         }
      for (i=0; i<lengths.size(); )
         {
         if (lengths[i]==0)
            {
            int k;
            codes.push(0 + (1<<8));  // this isn't optimal but matches hardware
            i++;
            for (k=0; k+i<lengths.size(); k++)
               if (lengths[k+i]!=0) 
                  break;
               
            if (k>138){
               codes.push(18 + (138<<8));
               i += 138;
               k -= 138;
               }
            if (k>=11){
               codes.push(18 + (k<<8));
               i += k;
               }
            else if (k>=7){
               codes.push(17 + (k<<8));
               i += k;
               }
            else if (k>=3){
               codes.push(16 + (k<<8));
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
      int hlit=286, hdist=30, hclen=19;
      
      // To make hardware easier I will use the following static encoding. It's simple and reasonably good (within 20-30 bits of optimal)
      for (i=0; i<=12; i++)
         codelens[i].len=4;
      for (; i<=18; i++)
         codelens[i].len=5;
      codelens.Transform();
      
      SetBits(output, bit, 5, hlit-257);
      SetBits(output, bit, 5, hdist-1);
      SetBits(output, bit, 4, hclen-4);
      
      for (i=0; i<hclen; i++) {
         SetBits(output, bit, 3, codelens[codeindirection[i]].len);
         if (codelens[codeindirection[i]].len >7)
            FATAL_ERROR;
         }
      
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
         }
      }
      
   litHuff.Transform();
   distHuff.Transform();
   
   for (i=0; i<blocklen; i++)
      {
      unsigned b = blocks[i];
      unsigned len = b>>16;
      unsigned dist = b&0xffff;
      if (len==0)
         SetRBits(output, bit, litHuff[dist].len, litHuff[dist].code);
      else
         {
         int k;
         for (k=0; k<29; k++)
            {
            if (lengths[k].len == len)
               break;
            if (lengths[k].len > len)
               {k--; break;}
            }
         
         SetRBits(output, bit, litHuff[257+k].len, litHuff[257+k].code);
         SetBits(output, bit, lengths[k].extra, len - lengths[k].len);
         for (k=0; k<30; k++)
            {
            if (distances[k].len == dist)
               break;
            if (distances[k].len > dist)
               {k--; break;}
            }
         if (k==30) k=29;
         SetRBits(output, bit, distHuff[k].len, distHuff[k].code);
         SetBits(output, bit, distances[k].extra, dist - distances[k].len);
         }
      }
   SetRBits(output, bit, litHuff[256].len, litHuff[256].code);
   }




void*   operator new[] (size_t size)
   {
#ifndef __NO_LC__
   return malloc(size);
#else
   return NULL;
#endif
   }
void      operator delete[] (void* ptr)
   {
#ifndef __NO_LC__
   free(ptr);
#endif
   }

