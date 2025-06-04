#ifndef __HELPER_H_INCLUDED__
#define __HELPER_H_INCLUDED__


#ifdef __STM32F3xx_HAL_CONF_H  // ideally this should be a STM32 macro
typedef __int64_t   __int64;
typedef __uint64_t   __uint64;
#define FATAL_ERROR DebugBreakFunc(__FILE__,__LINE__)
extern "C" void DebugBreakFunc(const char* module, int line);
#endif




#define MINIMUM(a,b) ( ((a)<(b)) ? (a) : (b))
#define MAXIMUM(a,b) ( ((a)>(b)) ? (a) : (b))

#ifndef STM32
class plottype {
   vector<char*> labels;
   vector<vector<float> > data;
   char extra_label[1024];
public:
   plottype() { extra_label[0] = 0; }
   void AddHeader(const char* txt) {
      if (strlen(txt) >= sizeof(extra_label)) FATAL_ERROR;
      strcpy(extra_label, txt);
      }
   
   void AddData(const char* label, float value)
      {
      int i;
      for (i = labels.size() - 1; i >= 0; i--)
         if (strcmp(labels[i], label) == 0) break;
      if (i < 0)
         {
         int len = strlen(label);
         labels.push_back(new char[len + 1]);
         strcpy(labels.back(), label);
         i = labels.size() - 1;
         }
      if (i >= data.size())
         data.push_back(vector<float>());
      data[i].push_back(value);
      }
   void AppendToFile(const char* filename)
      {
      int i, k;
      FILE* fptr = fopen(filename, "at");
      if (fptr == NULL)
	 {
	 printf("I couldn't open %s for append\n", filename);
	 FILE* fptr = fopen("foo.csv", "at");
	 if (fptr==NULL) FATAL_ERROR;
	 }
      if (extra_label[0])
         fprintf(fptr, "%s", extra_label);
      for (i = 0; i < labels.size(); i++)
         {
         fprintf(fptr, "%s", labels[i]);
         for (k = 0; k < data[i].size(); k++)
            fprintf(fptr, ",%.3f", data[i][k]);
         fprintf(fptr, "\n");
         }
      fclose(fptr);
      }
   };
#endif

inline unsigned int old_crc32(const void* mem, int len) {
   unsigned int accum = 0xFFFFFFFF;
   const unsigned char* buffer = (const unsigned char*)mem;
   int i, k;

   for (i = 0; i < len; i++) {
      accum ^= buffer[i];
      for (k = 0; k < 8; k++) {
         if (accum & 0x80000000)
            accum = (accum << 1) ^ 0x04c11db7;
         else
            accum = accum << 1;
         }
      }
   return accum;
   }
inline unsigned int crc32(const void* mem, int len) {
   unsigned int accum = 0xFFFFFFFF;
   const unsigned char* buffer = (const unsigned char*)mem;
   int i, k;

   for (i = 0; i < len; i++) {
      accum ^= buffer[i];
      for (k = 0; k < 8; k++) {
         if (accum & 1)
            accum = (accum >> 1) ^ 0xedb88320;
         else
            accum = accum >> 1;
         }
      }
   return accum;
   }
inline unsigned int bitreflect(unsigned int x) {
   unsigned int z = 0;
   for (int i = 0; i < 32; i++) {
      z = (z << 1) | (x & 1);
      x = x >> 1;
      }
   return z;
   }


// reasonably good random number generator from numerical recipes
// random number returned has a period on the order of 2^18
// I return a double instead of float to remove a boundary condition
// returns a number 0.0<= x <1.0 Watch out converting output to float 
// can cause rounding up to 1.0
inline double random(int& seed)  // initially set seed to anything except MASK value, zero is preferred
   {
   const int IA = 16807;
   const int IM = 2147483647;
   const int IQ = 127773;
   const int IR = 2836;
   const int MASK = 123459876;
   int k;
   double answer;

   seed ^= MASK;
   k = seed / IQ;
   seed = IA * (seed - k * IQ) - IR * k;
   if (seed < 0) seed += IM;

   answer = (1.0 / IM) * seed;
   seed ^= MASK;
   if (answer < 0.0 || answer >= 1.0) FATAL_ERROR;
   return answer;
   }
inline void randomcpy(int& seed, void* dest, int len) {
   int i;
   for (i = 0; i < len; i++)
      ((unsigned char*)dest)[i] = random(seed) * 512;
   }


// will return a random number with a gaussian distribution, mean of 0 and stdev=1. This is from numerical recipes
inline double gasdev(int& seed)
   {
   double fac, rsq, v1, v2;

   do {
      v1 = 2.0 * random(seed) - 1.0;
      v2 = 2.0 * random(seed) - 1.0;
      rsq = v1 * v1 + v2 * v2;
      } while (rsq >= 1.0 || rsq == 0.0);
      fac = sqrt(-2.0 * log(rsq) / rsq);
//      gset = v1 * fac;
//      iset = 1;
      return v2 * fac;
   }

// this function will count the number of '1' in the input
inline int PopCount(__uint64 x)
   {
   unsigned high = x, low = x >> 32;
   unsigned a, b, c, d;

   a = high & 0x55555555;
   c = low & 0x55555555;
   b = high & 0xAAAAAAAA;
   d = low & 0xAAAAAAAA;
   b = b >> 1;
   d = d >> 1;
   high = a + b;
   low = c + d;

   a = high & 0x33333333;
   c = low & 0x33333333;
   b = high & 0xCCCCCCCC;
   d = low & 0xCCCCCCCC;
   b = b >> 2;
   d = d >> 2;
   high = a + b;
   low = c + d;

   a = high & 0x0F0F0F0F;
   c = low & 0x0F0F0F0F;
   b = high & 0xF0F0F0F0;
   d = low & 0xF0F0F0F0;
   b = b >> 4;
   d = d >> 4;
   high = a + b;
   low = c + d;

   a = high & 0x00FF00FF;
   c = low & 0x00FF00FF;
   b = high & 0xFF00FF00;
   d = low & 0xFF00FF00;
   b = b >> 8;
   d = d >> 8;
   high = a + b;
   low = c + d;

   a = high & 0x0000FFFF;
   c = low & 0x0000FFFF;
   b = high & 0xFFFF0000;
   d = low & 0xFFFF0000;
   b = b >> 16;
   d = d >> 16;
   high = a + b;
   low = c + d;

   return high + low;
   }

// this function will count the number of '1' in the input
inline int PopCount(unsigned int x)
   {
   unsigned a, b;

   a = x & 0x55555555;
   b = x & 0xAAAAAAAA;
   b = b >> 1;
   x = a + b;

   a = x & 0x33333333;
   b = x & 0xCCCCCCCC;
   b = b >> 2;
   x = a + b;

   a = x & 0x0F0F0F0F;
   b = x & 0xF0F0F0F0;
   b = b >> 4;
   x = a + b;

   a = x & 0x00FF00FF;
   b = x & 0xFF00FF00;
   b = b >> 8;
   x = a + b;

   a = x & 0x0000FFFF;
   b = x & 0xFFFF0000;
   b = b >> 16;
   x = a + b;

   return x;
   }

// this function will count the number of '1' in the input
inline int PopCount(unsigned short x)
   {
   unsigned short a, b;

   a = x & 0x5555;
   b = x & 0xAAAA;
   b = b >> 1;
   x = a + b;

   a = x & 0x3333;
   b = x & 0xCCCC;
   b = b >> 2;
   x = a + b;

   a = x & 0x0F0F;
   b = x & 0xF0F0;
   b = b >> 4;
   x = a + b;

   a = x & 0x00FF;
   b = x & 0xFF00;
   b = b >> 8;
   x = a + b;

   return x;
   }

inline char* strupr(char* txt)  // convert a string to uppercase
   {
   char* chptr = txt;
   while (*chptr != 0)
      {
      if (*chptr >= 'a' && *chptr <= 'z')
         *chptr += 'A' - 'a';
      chptr++;
      }
   return txt;
   }

inline char* strlwr(char* txt)  // convert a string to lowercase
   {
   char* chptr = txt;
   while (*chptr != 0)
      {
      if (*chptr >= 'A' && *chptr <= 'Z')
         *chptr += 'a' - 'A';
      chptr++;
      }
   return txt;
   }

inline int stricmp(const char* a, const char* b)  // compare case insensitive
   {
   do {
      char aa = *a, bb = *b;
      if (aa >= 'a' && aa <= 'z')
         aa += 'A' - 'a';
      if (bb >= 'a' && bb <= 'z')
         bb += 'A' - 'a';
      if (aa < bb)
         return -1;
      if (aa > bb)
         return +1;
      a++;
      b++;
      if (*a == 0 && *b != 0)
         return -1;
      if (*a != 0 && *b == 0)
         return +1;
      } while (*a != 0 && *b != 0);
      return 0;
   }

inline int strnicmp(const char* a, const char* b, int len)  // compare case insensitive
   {
   do {
      len--;
      char aa = *a, bb = *b;
      if (aa >= 'a' && aa <= 'z')
         aa += 'A' - 'a';
      if (bb >= 'a' && bb <= 'z')
         bb += 'A' - 'a';
      if (aa < bb)
         return -1;
      if (aa > bb)
         return +1;
      a++;
      b++;
      if (len == 0) return 0;
      if (*a == 0 && *b != 0)
         return -1;
      if (*a != 0 && *b == 0)
         return +1;
      } while (*a != 0 && *b != 0);
      return 0;
   }

inline int strcmpxx(const char* a, const char* b)  // string compare that understands numbers
   {
   do {
      if (*a < '0' || *a >'9' || *b < '0' || *b >'9')
         {
         if (*a < *b) return -1;
         if (*a > *b) return +1;
         a++;
         b++;
         }
      else
         {
         int numa = 0, numb = 0;
         int adigits = 0, bdigits = 0;

         while (*a >= '0' && *a <= '9')
            {
            numa = numa * 10 + *a - '0';
            adigits++;
            a++;
            }
         while (*b >= '0' && *b <= '9')
            {
            numb = numb * 10 + *b - '0';
            bdigits++;
            b++;
            }
         if (numa < numb) return -1;
         if (numa > numb) return +1;
         if (adigits < bdigits) return -1;
         if (adigits > bdigits) return +1;
         }
      } while (*a != 0 || *b != 0);
      return 0;
   }

// will return number of matching characters between 2 strings
inline int strcorrelation(const char* a, const char* b)
   {
   int count = 0;
   while (*a != 0 && *b != 0 && *a == *b)
      {
      count++;
      a++;
      b++;
      }
   return count;
   }

inline char* strrstr(char* txt, const char* subtext)
   {
   char* chptr = txt, * lastptr=NULL;

   while ((chptr = strstr(chptr, subtext)) != NULL)
      {
      lastptr = chptr;
      chptr++;
      }
   return(lastptr);
   }
inline const char* strrstr(const char* txt, const char* subtext)
   {
   const char* chptr = txt, * lastptr=NULL;

   while ((chptr = strstr(chptr, subtext)) != NULL)
      {
      lastptr = chptr;
      chptr++;
      }
   return(lastptr);
   }

inline const char* Int64ToString(__uint64 x, char* buffer)
   {
   int i;
   char* chptr;
   for (chptr = buffer, i = 15; i >= 0; i--)
      {
      if (chptr != buffer || (x >> (i * 4)))
         {
         *chptr = (x >> (i * 4)) & 0xF;
         if (*chptr < 10)
            *chptr += '0';
         else
            *chptr += 'A' - 10;
         chptr++;
         }
      }
   if (chptr == buffer)
      {
      *chptr = '0'; chptr++;
      }
   *chptr = 0; // null terminate
   return buffer;
   }

// this will allow N bit integers
template <int N> class bitfieldtype {
private:
#define words ((N+63)>>6)
   __uint64 data[words];

public:
   bitfieldtype()
      {
      for (int i = 0; i < words; i++)
         data[i] = 0;
      }
   bitfieldtype(const __uint64 right)
      {
      operator=(right);
      }
   __int64 Int64() const
      {
      return data[0];
      }
   bitfieldtype& operator=(const __uint64 right)
      {
      data[0] = right;

      for (int i = 1; i < words; i++)
         data[i] = 0;
      return *this;
      }
   // let the default assignment operator be used instead (perhaps faster)
   /*   bitfieldtype &operator=(const bitfieldtype &right)
         {
         for (int i=0; i<words; i++)
            data[i] = right.data[i];
         return *this;
         }
   */
   bitfieldtype operator~() const
      {
      bitfieldtype ret;
      for (int i = 0; i < words; i++)
         ret.data[i] = ~data[i];
      return ret;
      }
   bitfieldtype operator+(const bitfieldtype& right) const
      {
      bitfieldtype ret;
      int carry = 0;
      for (int i = 0; i < words; i++)
         {
         ret.data[i] = data[i] + right.data[i] + carry;
         const __uint64 cin = (data[i] ^ right.data[i] ^ ret.data[i]);
         carry = ((cin & data[i]) | (cin & right.data[i]) | (data[i] & right.data[i])) >> 63;
         }
      return ret;
      }
   bitfieldtype operator-(const bitfieldtype& right) const
      {
      bitfieldtype ret;
      int carry = 1;
      for (int i = 0; i < words; i++)
         {
         ret.data[i] = data[i] + (~right.data[i]) + carry;
         const __uint64 cin = (data[i] ^ (~right.data[i]) ^ ret.data[i]);
         carry = ((cin & data[i]) | (cin & (~right.data[i])) | (data[i] & (~right.data[i]))) >> 63;
         }
      return ret;
      }
   bitfieldtype operator|(const bitfieldtype& right) const
      {
      bitfieldtype ret;
      for (int i = 0; i < words; i++)
         ret.data[i] = data[i] | right.data[i];
      return ret;
      }
   bitfieldtype operator&(const bitfieldtype& right) const
      {
      bitfieldtype ret;
      for (int i = 0; i < words; i++)
         ret.data[i] = data[i] & right.data[i];
      return ret;
      }
   bitfieldtype operator^(const bitfieldtype& right) const
      {
      bitfieldtype ret;
      for (int i = 0; i < words; i++)
         ret.data[i] = data[i] ^ right.data[i];
      return ret;
      }
   bitfieldtype& operator|=(const bitfieldtype& right)
      {
      for (int i = 0; i < words; i++)
         data[i] |= right.data[i];
      return *this;
      }
   bitfieldtype& operator&=(const bitfieldtype& right)
      {
      for (int i = 0; i < words; i++)
         data[i] &= right.data[i];
      return *this;
      }
   bitfieldtype operator<<(int left) const
      {
      if (left < 0) FATAL_ERROR;
      bitfieldtype ret;
      __uint64 carry = 0;
      int bit = left & 0x3f, offset = left >> 6;
      if (offset >= words)
         return ret;

      for (int i = 0; i < words - offset; i++)
         {
         __uint64 nextcarry = data[i] >> (64 - bit);
         ret.data[i + offset] = (data[i] << bit) | carry;
         carry = bit ? nextcarry : 0;
         }
      return ret;
      }
   bitfieldtype operator>>(int right) const
      {
      if (right < 0) FATAL_ERROR;
      bitfieldtype ret;
      __uint64 carry = 0;
      int bit = right & 0x3f, offset = right >> 6;
      if (offset >= words)
         return ret;

      for (int i = words - 1; i >= offset; i--)
         {
         __uint64 nextcarry = data[i] << (64 - bit);
         ret.data[i - offset] = (data[i] >> bit) | carry;
         carry = bit ? nextcarry : 0;
         }
      return ret;
      }
   bitfieldtype operator*(const bitfieldtype& right) const
      {
      bitfieldtype ret = 0;
      int i;

      for (i = N - 1; i >= 0; i--)
         {
         ret = ret << 1;
         if (GetBit(i))
            ret = ret + right;
         }
      return ret;
      }
   bool operator==(const bitfieldtype& right) const
      {
      int i;
      for (i = 0; i < words; i++)
         if (data[i] != right.data[i])
            return false;
      return true;
      }
   bool operator!=(const bitfieldtype& right) const
      {
      return !operator==(right);
      }
   bool operator<(const bitfieldtype& right) const
      {
      int i;
      for (i = words - 1; i >= 0; i--)
         {
         if (data[i] < right.data[i])
            return true;
         if (data[i] > right.data[i])
            return false;
         }
      return false;
      }
   bool operator<=(const bitfieldtype& right) const
      {
      int i;
      for (i = words - 1; i >= 0; i--)
         {
         if (data[i] < right.data[i])
            return true;
         if (data[i] > right.data[i])
            return false;
         }
      return true;
      }
   bool operator>(const bitfieldtype& right) const
      {
      int i;
      for (i = words - 1; i >= 0; i--)
         {
         if (data[i] > right.data[i])
            return true;
         if (data[i] < right.data[i])
            return false;
         }
      return false;
      }
   bool operator>=(const bitfieldtype& right) const
      {
      int i;
      for (i = words - 1; i >= 0; i--)
         {
         if (data[i] > right.data[i])
            return true;
         if (data[i] < right.data[i])
            return false;
         }
      return true;
      }
   int PopCount() const
      {
      bitfieldtype temp(*this);
      return temp.DestructivePopCount();
      }
   int BitLocation(int index) const // this will return the location of the index bit. ie index=0 then return location of LSB
      {
      int i, count = 0;

      for (i = 0; i < N; i++)
         {
         if (GetBit(i))
            {
            if (count == index)
               return i;
            count++;
            }
         }
      return -1;   // index >= PopCount
      }
   int Msb() const
      {
      int i;

      for (i = N - 1; i >= 0; i--)
         {
         if (GetBit(i))
            return i;
         }
      return -1;
      }
   int Lsb() const
      {
      int i;

      for (i = 0; i < N; i++)
         {
         if (GetBit(i))
            return i;
         }
      return -1;
      }
   void FlipBit(const int bit)
      {
      if (GetBit(bit))
         ClearBit(bit);
      else
         SetBit(bit);
      }

   void SetBit(const int bit)
      {
      if (bit < 0 || bit >= N) FATAL_ERROR;
      data[bit >> 6] |= (__uint64)1 << (bit & 63);
      }
   void ClearBit(const int bit)
      {
      if (bit < 0 || bit >= N) FATAL_ERROR;
      data[bit >> 6] &= ~((__uint64)1 << (bit & 63));
      }
   void MakeBit(const int bit, bool value)
      {
      if (value)
         SetBit(bit);
      else
         ClearBit(bit);
      }
   bool GetBit(const int bit) const
      {
      if (bit < 0 || bit >= N) FATAL_ERROR;
      return 0 != (data[bit >> 6] & ((__uint64)1 << (bit & 63)));
      }
   //   operator __uint64() const
   //      {
   //      return data[0];
   //      }   
   char* Print(char* buffer, int nibble = 0) const
      {
      char* chptr = buffer;
      int i;

      nibble *= 4;
      for (i = words - 1; i > 0 && data[i] == 0 && nibble == 0; i--)   // skip over leading zeros
         ;

      for (; i >= 0; i--)
         for (int k = 64 - 4; k >= 0; k -= 4)
            {
            unsigned a = (data[i] >> k) & 0xF;
            *chptr = a + (a > 9 ? 'A' - 10 : '0');
            chptr++;
            if (nibble && (k % nibble) == 0 && (i != 0 || k != 0))
               {
               *chptr = '_'; chptr++;
               }
            }
      *chptr = 0;
      return buffer;
      }
   int DestructivePopCount()  // will destroy data but be fast (64 bit version)
      {
      __uint64 a, b;
      int i, count;
      for (i = 0; i < words; i++)
         {
         a = data[i] & 0x5555555555555555;
         b = data[i] & 0xAAAAAAAAAAAAAAAA;
         b = b >> 1;
         data[i] = a + b;
         }
      for (i = 0; i < words; i++)
         {
         a = data[i] & 0x3333333333333333;
         b = data[i] & 0xCCCCCCCCCCCCCCCC;
         b = b >> 2;
         data[i] = a + b;
         }
      for (i = 0; i < words; i++)
         {
         a = data[i] & 0x0F0F0F0F0F0F0F0F;
         b = data[i] & 0xF0F0F0F0F0F0F0F0;
         b = b >> 4;
         data[i] = a + b;
         }
      for (i = 0; i < words; i++)
         {
         a = data[i] & 0x00FF00FF00FF00FF;
         b = data[i] & 0xFF00FF00FF00FF00;
         b = b >> 8;
         data[i] = a + b;
         }
      for (i = 0; i < words; i++)
         {
         a = data[i] & 0x0000FFFF0000FFFF;
         b = data[i] & 0xFFFF0000FFFF0000;
         b = b >> 16;
         data[i] = a + b;
         }
      for (i = 0; i < words; i++)
         {
         a = data[i] & 0x00000000FFFFFFFF;
         b = data[i] & 0xFFFFFFFF00000000;
         b = b >> 32;
         data[i] = a + b;
         }
      count = 0;
      for (i = 0; i < words; i++)
         {
         count += data[i];
         }
      return count;
      }
#undef words
   };

#endif
