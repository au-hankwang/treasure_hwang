#ifndef __TEMPLATE_H_INCLUDED__
#define __TEMPLATE_H_INCLUDED__

#include <algorithm>
#include <functional>

#define MINIMUM(a,b) ( ((a)<(b)) ? (a) : (b))
#define MAXIMUM(a,b) ( ((a)>(b)) ? (a) : (b))
#define ROUND_TO_INT(x) ((x)>=0.0?(int)((x)+0.5):(int)((x)-0.5))


template<class T> void SWAP(T &a, T &b)
   {
   T temp = a;
   a = b;
   b = temp;
   }			 

inline unsigned int crc32(const void *mem, int len);

// IMPORTANT: remember that if you use the std::sort your predicate must adhere to a 
// 'strict weak ordering'. Any crashes inside std::sort on your predicate are most likely
// because of this

#define array DCarray
// this is just a spiffier version of std::vector.
template<class T> class array
   {
private:
   void CopyConstruct(size_t index, const T &val)
      {
      if (arr==NULL) FATAL_ERROR;
      new ((void *)&arr[index]) T(val);  // call the copy constructor, don't actually allocate memory
      }
   void Destruct(size_t index, T*_arr)
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
      for (size_t i=0; i<right.top; i++)
         CopyConstruct(i, right.arr[i]);
      top = right.top;
      }
   void resize(size_t n, const T &val = T())
      {
      size_t i;
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
      allocated = 0;
      }
   void reserve(size_t n)
      {
      if (n <= allocated) return;
      if (n < top) FATAL_ERROR;
      T * oldarr = arr;
      size_t i;
            
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
      if (index>=top){
         FATAL_ERROR;
         }

      return arr[index];
      }
   T &operator[](size_t index)
      {
      if (index>=top) 
         FATAL_ERROR;

      return arr[index];
      }
   int size() const  // use a signed int instead of size_t to prevent 0-1 >0
      {
      return (int)top;
      }
   int capacity() const
      {
      return (int)allocated;
      }
   void clear()
      {
      for (size_t i=0; i<top; i++)
         Destruct(i, arr);
      top = 0;
      }
   // push is tricky because val could be an element in this array, after a relocation it would be invalid
   void push(const T &val)
      {
      if (top >= allocated)
         {
         T * oldarr = arr;
         size_t i;

         if (sizeof(T) <= 64)
            allocated = MAXIMUM(allocated*2, 16);
         else
            allocated = MAXIMUM(allocated*2, 1);
         
         arr = (T*)new char[allocated * sizeof(T)];
         if (arr==NULL) 
            {
            allocated = MAXIMUM(top*4/3, 16);
            arr = (T*)new char[allocated * sizeof(T)];
            if (arr==NULL) FATAL_ERROR;      
            }
         
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
   const T *begin() const
      {
      return arr + 0;
      }
   T *end()
      {
      return arr + top;
      }
   const T *end() const
      {
      return arr + top;
      }
   bool pop(T &ob)
      {
      if (top==0) return false;
      ob = back();
      top--;
      Destruct(top, arr);
      return true;
      }
   bool pop()
      {
      if (top==0) return false;
      top--;
      Destruct(top, arr);
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
      size_t i;
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
      for (size_t i=0; i<right.top; i++)
         push(right.arr[i]);
      return *this;
      }
   bool operator==(const array &right)
      {
      if (!(size()==right.size())) return false;
      for (size_t i=0; i<top; i++)
         if (!(arr[i]==right.arr[i]))
            return false;
      return true;
      }
   void Insert(int before, const array &right)
      {
      if (before <0 || (unsigned)before >top) FATAL_ERROR;

      reserve(top + right.top);
      for (__int64 i=top-1; i>=before; i--)         
         {
         if (i + right.top >= top)
            CopyConstruct(i+right.top, arr[i]);
         else
            arr[i + right.top] = arr[i];
         }
      for (size_t k=0; k<right.top; k++)
         {
         if (k + before >= top)
            CopyConstruct(k+before, right.arr[k]);
         else
            arr[k + before] = right.arr[k];
         }
      top += right.top;
      }
   void Insert(int before, const T &right)
      {
      __int64 i;
      if (before <0 || (unsigned)before >top) FATAL_ERROR;

      reserve(top + 1);
      CopyConstruct(top, arr[top-1]);
      for (i=top-1; i>before; i--)         
         {
         arr[i] = arr[i-1];
         }
      arr[before] = right;
      top++;
      }
   void RemoveFast(size_t index)  // performs an erase in O(1) time but changes order of elements >= index
      {
      if (index >=top) FATAL_ERROR;
      arr[index] = arr[top-1];
      pop_back();
      }   
   void Remove(size_t index)  // performs an erase in O(N) time but it doesn't reorder
      {
      if (index >=top) FATAL_ERROR;
      for (size_t i=index; i<top-1; i++)
         arr[i] = arr[i+1];
      pop_back();
      }
   void RemoveFrontEntries(int num) // effectively a fast pull()
      {
      if (num>top || num<0) FATAL_ERROR;
      for (int i=num; i<top; i++)
         arr[i-num] = arr[i];
      top -= num;
      }
   void Sort()
      {
      std::sort(begin(), end());
      }
   void StableSort()
      {
      std::stable_sort(begin(), end());   // this is slower but removes variation when elements are equal
      }
/*
   int BinarySearchIndex(const T &value) const // will return -1 if the item isn't in the sorted list
      {
      const T *ptr;
      ptr = std::lower_bound(begin(), end(), value);
      if (ptr==end()) return -1;
      if ( *ptr < value)
         return -1;
      if (value < *ptr)
         return -1;
      return ptr-begin();
      }*/
   int BinarySearchIndex(const T &value, size_t min=0, size_t max=0) const // will return -1 if the item isn't in the sorted list, min/max are sub ranges in array inclusive
      {
      const T *ptr;
      if (max==0 && min==0)
         max = top-1;
      if (min<0 || min>=top || max<0 || max>=top || max<=min)
         FATAL_ERROR;
      ptr = std::lower_bound(&arr[min], &arr[max], value);
      if (ptr==end()) return -1;
      if ( *ptr < value)
         return -1;
      if (value < *ptr)
         return -1;
      return ptr-begin();
      }
   bool IsPresent(const T &val) const  // perform a linear search for the element
      {
      for (__int64 i=(__int64)top-1; i>=0; i--)
         {
         if (arr[i]==val)
            return true;
         }
      return false;
      }
   int LinearSearch(const T &val) const  // perform a linear search for the element(-1 for not found)
      {
      for (__int64 i=(__int64)top-1; i>=0; i--)
         {
         if (arr[i]==val)
            return i;
         }
      return -1;
      }
   void RemoveDuplicates()   // array will be unordered after this
      {
      Sort();
      for (__int64 i=(__int64)top-2; i>=0; i--)
         {
         if (arr[i+1] == arr[i])
            RemoveFast(i+1);
         }
      }
   // this function takes two sorted arrays and returns an array of all the common elements. O(N) complexity
   void CommonContents(array <T> &result, const array <T> &right) const
      {
      int l, r;

      for (l = r=0; l<this->size() && r<right.size(); )
         {
         if (operator[](l) == right[r])
            {
            result.push(right[r]);
            l++;
            r++;
            }
         else if (operator[](l) < right[r])
            l++;
         else
            r++;
         }
      }
   // this function takes two sorted arrays deletes occurence of right in this. O(N) complexity
   void RemoveCommonElements(const array <T> &right)
      {
      int l, r;
      array <int> deleted;

      for (l = r=0; l<this->size() && r<right.size(); )
         {
         if (operator[](l) == right[r])
            {
            deleted.push(l);
            l++;
            r++;
            }
         else if (operator[](l) < right[r])
            l++;
         else
            r++;
         }
      for (int i=deleted.size()-1; i>=0; i--)
         {
         RemoveFast(deleted[i]);
         }
      }

   // this function takes two sorted arrays and returns an array of all the elements in right not it self. O(N) complexity
   void AdditionalContents(array <T> &result, const array <T> &right) const
      {
      int l, r;

      for (l = r=0; l<this->size() && r<right.size(); )
         {
         if (operator[](l) == right[r])
            {            
            l++;
            r++;
            }
         else if (operator[](l) < right[r])
            l++;
         else
            {
            result.push(right[r]);
            r++;
            }
         }
      for (; r<right.size(); r++)
         result.push(right[r]);
      }
   };


using namespace std;  // might as well g++ and decxx do it implicitly

// can be used as a predicate function in stl std::sort to sort a container of pointers
template<class T> struct indirect_lessthan : std::binary_function<T, T, bool>
   {
   bool operator()(const T &x, const T &y) const
      {
      return (*x < *y);
      }
   };


template <class T> class heaptype
   {
   T *arr;
   size_t allocated,len;
   const void *external_mem;
public:
   
   heaptype(void *_external_mem=NULL, size_t external_size=0) : external_mem(_external_mem)
      {
      arr=(T*)_external_mem; 
      allocated=len=0;
      if (external_mem)
         allocated = external_size / sizeof(T);
      }
   heaptype(int initialsize) 
      {
      arr=NULL;
      external_mem = NULL;
      allocated=initialsize;
      arr= new T[allocated+1];
      if (arr==NULL)
         FATAL_ERROR;
         
      size=0;
      }
   heaptype (const heaptype &right) { //copy constructor
      external_mem = NULL;
      arr = NULL;
      operator=(right);
      }
   heaptype &operator=(const heaptype &right)
      {
      len = right.len;
      if (len >= allocated) {
         if (external_mem!=arr)
            delete arr;
         allocated = len;
         arr = new T[allocated+1];
         }
      if (arr==NULL)
         FATAL_ERROR;
      for (int i=1; i<=len; i++)
         arr[i] = right.arr[i];
      return *this;
      }
   ~heaptype(){
      if (arr!=external_mem) delete []arr;
      }
   T &operator[](int index) {
      if (index >= len) FATAL_ERROR;
      return(arr[index+1]);
      }
   __int64 size() const {
      return(len);
      }
   void clear(){
      len=0;
      }
   
   void push(const T &data)
      {
      size_t i;
            
      if (len+1>=allocated)
         {
         T *oldarr=arr;
         allocated=MAXIMUM(allocated*2, 64);
         arr= new T[allocated];
         if (arr==NULL)
            FATAL_ERROR;
         if (oldarr!=NULL)
            {
            for (i=1; i<=len; i++)
               arr[i]=oldarr[i];
            if (oldarr!=external_mem)
               delete []oldarr;
            }
         }
      len++;
      i=len;
      while (i>1 && !(arr[i/2] < data) )
         {
         arr[i]=arr[i/2];
         i=i/2;
         }
      arr[i]=data;
      }
   
   // this gets the element with the smallest key.
   int pop(T &data)
      {
      if (len<=0) return(false);  
      data=arr[1];
      arr[1]=arr[len];
      len--;
      heapify(1);
      return(true);
      }
   // this gets the element with the smallest key.
   int pop()
      {
      if (len<=0) return(false);  
      arr[1]=arr[len];
      len--;
      heapify(1);
      return(true);
      }
   
private:
   void heapify(int i)
      {
      int l=i*2, r=i*2+1, largest;
      
      if (l<=len && !(arr[i] < arr[l]))
         largest=l;
      else largest=i;
      
      if (r<=len && ! (arr[largest] < arr[r]) )
         largest=r;
      if (largest!=i)
         {
         T temp;
         temp=arr[i];
         arr[i]=arr[largest];
         arr[largest]=temp;
         heapify(largest);
         }
      }     
   };                                                        


// simpler and less flexible than STL template but it accesses memory in a block
// and doesn't depend on linked lists. Note this won't call any destructors when pop an item
template <class T> class queuetype
   {                                                                                        
protected:
   int top,bottom;
   array<T> queue;

public:         
   queuetype(size_t startsize) : queue(startsize)
      {
      bottom=top=0;
      }
   queuetype() : queue(16)
      {
      bottom=top=0;
      }
   
   void clear(){bottom=top=0;}
   
   int size()
      {
      if (top==bottom) return 0;
      if (top>bottom) return (top-bottom);
      else return (queue.size() - bottom + top);
      }
   
   T &operator[](size_t index)
      {
      if (bottom<top && index >= top-bottom)
         FATAL_ERROR;
      if (bottom>=top && index >= queue.size()-bottom+top)
         FATAL_ERROR;
      if (index+bottom < queue.size())
         return queue[index+bottom];
      else
         return queue[index+bottom-queue.size()];
      }
   const T &operator[](size_t index) const
      {
      if (bottom<top && index >= top-bottom)
         FATAL_ERROR;
      if (bottom>=top && index >= queue.size()-bottom+top)
         FATAL_ERROR;
      if (index+bottom < queue.size())
         return queue[index+bottom];
      else
         return queue[index+bottom-queue.size()];
      }

   void push(const T &ob)
      {
      queue[top]=ob;
      top++;
      if (top>=queue.size())
         top=0;
      
      if (top==bottom)
         { // must grow queue
         int delta = queue.size();
         int i;
         queue.resize(queue.size() + delta);
         for (i=0; i<delta; i++)
            queue[queue.size()-1-i] = queue[queue.size()-1-i-delta];
         bottom+=delta;
         }    
      if (bottom>=queue.size())
         FATAL_ERROR;
      }
   
   // this pulls item off the bottom of the queue (ie queue like behavior)
   bool pull(T &ob) // return false if empty
      {
      if (top==bottom) return(false);
      ob=queue[bottom];
      bottom++;
      if (bottom>=queue.size())
         bottom=0;
      return(true);
      }   
   
   // this pulls items off the top of the queue (ie stack like behavior)
   bool pop(T &ob) // return false if empty
      {
      if (top==bottom) return(false);
      top--;
      if (top<0)
         top=queue.size()-1;
      ob=queue[top];
      return(true);
      }   
   // this pulls items off the top of the queue (ie stack like behavior)
   bool pop() // return false if empty
      {
      if (top==bottom) return(false);
      top--;
      if (top<0)
         top=queue.size()-1;
      return(true);
      }   
   // this deletes an item in the middle, very expensive operation!
   void Remove(size_t index)
      {
      int i;

      if (bottom<top)
         {
         if (index >= top-bottom) FATAL_ERROR;
         for (i=index+bottom; i>bottom; i--)
            queue[i] = queue[i-1];
         bottom++;
         }
      else
         {
         if (index >= queue.size()-bottom+top)
            FATAL_ERROR;
         if (index+bottom < queue.size())
            {
            for (i=bottom+index; i>bottom; i--)
               queue[i] = queue[i-1];
            bottom++;
            if (bottom>=queue.size())
               bottom=0;
            }
         else
            {
            for (i=index+bottom-queue.size(); i<top; i++)
               queue[i] = queue[i+1];
            top--;
            }
         }
      }   
   };    




#define HASH_ITERATIONS 32
template <class KEY, class PAYLOAD> class hashtype
   {
   struct entrytype{   
      KEY key;
      PAYLOAD payload;
      } *table;
   unsigned allocated;
   int *bitfield;    // packed bit array telling whether entry is used or not

private:
   bool IsPresent(int index) const
      {
      return 0 != (bitfield[index>>5] & (1<< (index&0x1F))) ;
      }
   void SetPresent(int index)
      {
      bitfield[index>>5] |= (1<< (index&0x1F));
      }
   
public:
   hashtype(int startsize=256)
      {
      Initialize(startsize);
      }
   ~hashtype() 
      {
      delete []table;
      table = NULL;
      delete []bitfield;
      bitfield = NULL;
      allocated = 0;
      }
   void Initialize(int startsize)
      {
      startsize |=1;
      startsize -=2;
      allocated = startsize;

      table = new entrytype[allocated];
      if (table==NULL) 
         {
         int attempt = sizeof(entrytype) * allocated;
         FATAL_ERROR;
         }
      
      bitfield = new int[allocated/32 + 1];
      if (bitfield==NULL) FATAL_ERROR;
      memset(bitfield, 0, (allocated/32 +1) * sizeof(int));
      }
   hashtype(const hashtype &right)
      {
      allocated = right.allocated;

      table = new entrytype[allocated];
      if (table==NULL) FATAL_ERROR;
      bitfield = new int[allocated/32 + 1];
      if (bitfield==NULL) FATAL_ERROR;
      
      for (size_t i=0; i<allocated; i++)
         table[i] = right.table[i];
      
      memcpy(bitfield, right.bitfield, (allocated/32 +1) * sizeof(int));
      }
   hashtype &operator=(const hashtype &right)
      {
      if (this == &right)
         return *this;
      if (allocated != right.allocated)
         {
         delete []table;
         table = NULL;
         delete []bitfield;
         bitfield = NULL;
         allocated = right.allocated;
         table = new entrytype[allocated];
         bitfield = new int[allocated/32 + 1];
         }
      if (table==NULL) FATAL_ERROR;
      
      for (size_t i=0; i<allocated; i++)
         table[i] = right.table[i];
      memcpy(bitfield, right.bitfield, (allocated/32 +1) * sizeof(int));
      return *this;
      }
   // this is necessary because the compiler will change fooarray=5 into fooarray = array<footype>(5)
   void operator=(const int);  // will result in linker error! Fix the code.

   void clear()
      {
      memset(bitfield, 0, (allocated/32 +1) * sizeof(int));
      }
   void UnAllocate()
      {
      delete []table;
      table = NULL;
      delete []bitfield;
      bitfield = NULL;
      Initialize(16);
      }
   void StartDump(int &cnt) const 
      {
      cnt = 0;
      }
   bool DumpKeys(int &cnt, KEY &key, PAYLOAD &payload) const // returns false if no more keys not seen
      {      
      for (cnt; (unsigned)cnt<allocated; cnt++)
         if (IsPresent(cnt))
            {
            key     = table[cnt].key;
            payload = table[cnt].payload;
            cnt++;
            return true;
            }
      return false;
      }

   PAYLOAD &operator[](KEY key)
      {
      int index = Ordinal(key);

      if (index<0)  // must grow table
         {
         TAtype ta("Growing hash table -> hashtype<> operator[]");
         hashtype newh(allocated*2);
         int elements=0;         
         
         for (unsigned i=0; i<allocated; i++)
            if (IsPresent(i))
               {
               newh[table[i].key] = table[i].payload;               
               elements++;
               }
//         printf("\nHash was %.1f%% efficient. New table will be %d (%d bytes)",100.0*elements/allocated, newh.allocated, newh.allocated*sizeof(entrytype));
            
         delete []table;
         delete []bitfield;
            
         table         = newh.table;
         bitfield      = newh.bitfield;
         allocated     = newh.allocated;
         newh.table    = NULL;   // prevent newh destructor from clobbering the table
         newh.bitfield = NULL;
         return operator[](key);
         }
      
      if (!IsPresent(index))
         {
         SetPresent(index);         
         table[index].key = key;
         }
      return table[index].payload;
      }
   
   // if key is already present then I replace the payload, not the key
   void Add(KEY key, const PAYLOAD &payload)
      {
      operator[](key) = payload;
      }

   bool Check(KEY key, PAYLOAD &payload) const // return false if key not found
      {
      int index = Ordinal(key);
      
      if (index >=0 && IsPresent(index))
         {
         payload = table[index].payload;
         return true;
         }
      return false;
      }

   int size()
      {
      return allocated;
      }
   // the hash functions I use are h1=k mod M; h2= k mod (m-2); where m is the hashsize
   // this will return a unique ordinal for this key such that 0<=ordinal<size. 
   // if key isn't present and hash depth is exceed then will return -1
   int Ordinal(KEY key) const
      {
      unsigned int hashindex,start; 
      __int64 k1,k2;
      int i;      

      k1 = (__int64)key;
      k2 = (k1 >> 16) - k1;
      
      k2=k2%(allocated/HASH_ITERATIONS+1);  // I want to keep k2 small enough so I don't have to worry about it aliasing to the same spot
      k2|=1;
      
      i=0;      // note I use double hashing to minimize primary,secondary clustering
      hashindex=(__uint64)k1 %allocated;
      start=hashindex;
      while (IsPresent(hashindex))
         { 
         if (key == table[hashindex].key)
            return hashindex;
         if (i>HASH_ITERATIONS)
            return -1;
         i++;	 
         hashindex=(__uint64)(k1+i*k2) % (allocated-2);
         if (hashindex==start) k2=1;
         }
      return hashindex;
      }
   };


struct sha160digesttype {
   unsigned a, b, c, d, e;
   bool operator==(const sha160digesttype&right) const
      {
      if (a!=right.a) return false;
      if (b!=right.b) return false;
      if (c!=right.c) return false;
      if (d!=right.d) return false;
      if (e!=right.e) return false;
      return true;
      }
   void Clear()
      {
      a = b = c = d = e =0;
      }
   __uint64 md1() const
      {
      return a | ((__uint64)b<<32);
      }
   __uint64 md2() const
      {
      return c | ((__uint64)d<<32);
      }
   };


inline void sha(const void *mem, int memsize, sha160digesttype &digest)
   {
//   TAtype ta("SHA-1()");
   unsigned int w[16];
   unsigned int a=0x67452301, b=0xefcdab89, c=0x98badcfe, d=0x10325476, e=0xc3d2e1f0;
   int i;

   while (memsize>0)
      {
      memset(w, 0, sizeof(w));
      memcpy(w, mem, MINIMUM(64, memsize));
#ifdef __SUN 
      for (i=0; i<16; i++)
         {  // need to convert to big endian for sun machines
         unsigned char a;
         a = ((unsigned char *)(w+i))[0];
         ((unsigned char *)(w+i))[0] = ((unsigned char *)(w+i))[3];
         ((unsigned char *)(w+i))[3] = a;
         a = ((unsigned char *)(w+i))[1];
         ((unsigned char *)(w+i))[1] = ((unsigned char *)(w+i))[2];
         ((unsigned char *)(w+i))[2] = a;
         }
#endif

      memsize -= 64;
      mem = (char *)mem + 64;

      unsigned temp, nlf, k, wt;

      for (i=0; i<80; i++)
         {
         if (i<20){
            nlf = (b&c)|(~b&d);
            k   = 0x5a827999;
            }
         else if (i<40){
            nlf = b ^ c ^ d;
            k   = 0x6ed9eba1;
            }
         else if (i<60){
            nlf = (b&c)|(c&d)|(b&d);
            k   = 0x8f1bbcdc;
            }
         else{
            nlf = b ^ c ^ d;
            k   = 0xca62c1d6;
            }
         if (i<16)
            wt = w[i];
         else
            {
            wt = w[(i-3)&15] ^ w[(i-8)&15] ^ w[(i-14)&15] ^ w[(i)&15];
            wt = (wt<<1) | (wt>>31);
            w[i&15] = wt;
            }
         temp = ((a<<5) | (a>>(32-5))) + nlf + e + wt + k;
         e = d;
         d = c;
         c = (b<<30) | (b>>2);
         b = a;
         a = temp;
         }
      }
   digest.a = a;
   digest.b = b;
   digest.c = c;
   digest.d = d;
   digest.e = e;
   }





// this is a specialization of hashtype to handle text strings
// when a record is added the text pointer must be persistant.
// I'll store only the pointer not the text itself
// My destructor will in no way free the memory associated with the pointer
class texthashkeytype{
public:
   sha160digesttype digest;

   operator __int64()  
      {
      return digest.md1();
      }
   bool operator==(const texthashkeytype &right)
      {
//      if (md1==right.md1 && md2==right.md2)
//         if (strcmp(txt,right.txt)!=0)
//            FATAL_ERROR;
      return digest==right.digest;
      }
   texthashkeytype(const char *_txt=NULL)
      {
      if (_txt==NULL)
         digest.Clear();
      else
         {
         sha(_txt, (int)strlen(_txt), digest);
//         strcpy(txt, _txt);
         }
      }
   };
   
template <class PAYLOAD> class texthashtype : public hashtype<texthashkeytype, PAYLOAD>
   {
public:
   bool Check(const char *txt, PAYLOAD &payload) const  // return false if key isn't present
      {
      texthashkeytype tk(txt);
      return hashtype<texthashkeytype, PAYLOAD>::Check(tk, payload);
      }
   bool Check(const char *txt) const // return false if key isn't present
      {
      texthashkeytype tk(txt);
      PAYLOAD dummy;
      return hashtype<texthashkeytype, PAYLOAD>::Check(tk, dummy);
      }
   void Add(const char *txt, const PAYLOAD &payload)
      {
      texthashkeytype tk(txt);      
      hashtype<texthashkeytype, PAYLOAD>::Add(tk, payload);
      }
   bool DumpKeys(int &cnt, PAYLOAD &payload)  // returns false if no more keys not seen      
      {
      texthashkeytype tk;
      bool ret = hashtype<texthashkeytype, PAYLOAD>::DumpKeys(cnt, tk, payload);
      return ret;
      }
   PAYLOAD &operator[](const char *key)
      {
      return hashtype<texthashkeytype, PAYLOAD>::operator[](key);
      }
   };


// this class impliments a dynamic linked list
template <class T> class listtype{
private:
   class listnodetype{
   public:
      T ob;
      listnodetype *next;
      listnodetype (const T &data, listnodetype *nextptr) {ob=data; next=nextptr;}
      };

   listnodetype *head;   
   int listsize;

   void debug_check() // This checks that the list is correct! 
      {
      listnodetype *current;
      int count=0;          
      current=head;
      while (current!=NULL)
         {
         current=current->next;
         count++;
         }
      if (count!=listsize) FATAL_ERROR;
      }

public:
   listtype() {head=NULL; listsize=0;}
   ~listtype(){clear();}
   listtype (const listtype &original)  //copy constructor
      { // Unfortunately this will invert the order of everything copied
      listnodetype *current=original.head;
      printf("\nCopy constructor");
      head=NULL;
      listsize=0;
      while (current!=NULL)
         {          
         push(current->ob);
         current=current->next;
         }
      }
   listtype &operator=(const listtype &original)
      { // Unfortunately this will invert the order of everything assigned
      listnodetype *current=original.head;
      
      if (listsize!=0 || head!=NULL) clear(); // free memory before overwrite
      head=NULL;
      listsize=0;
      while (current!=NULL)
         {
         push(current->ob);
         current=current->next;
         }      
      return (*this);
      }
   
   int size(){return(listsize);}
   void push(const T &ob)
      {
      listnodetype *temp;
      temp=new listnodetype(ob,head);
      head=temp;
      listsize++;
      }    
   int pop(T &ob)  // returns false if list is empty
      {
      listnodetype *temp;
      if (head==NULL) return(false);
      ob=head->ob;
      temp=head;
      head=head->next;
      listsize--; 
      delete temp;  // free memory
      return(true);
      } 
   T &operator[](int index)
      {
      listnodetype *current,*temp;
      
      index=listsize-index-1;
      if (index<0 || index>=listsize) FATAL_ERROR;
      current=head;
      do{
         index--;       
         temp=current;
         current=current->next;
         }while (index>=0);
      return(temp->ob);
      }
   void clear()
      {
      listnodetype *current=head,*temp;
      while (current!=NULL)
         {
         temp=current;
         current=current->next;
         delete temp;
         listsize--;
         }
      head=NULL;
      if (listsize!=0) FATAL_ERROR;
      }
   void sort() // This is a bubble sort which at O(n^2) is not efficient for large lists 
      {
      int done;
      listnodetype *current,**previous,*temp;
      
      if (head==NULL) return;
      do{
         done=true;
         current=head;
         previous=&head;
         while (current->next!=NULL)
            {
            if (current->ob < current->next->ob)
               { // do a swap
               done=false;
               *previous=current->next;
               temp=current->next->next;
               current->next->next=current;
               current->next=temp;
               current=*previous;
               }
            else 
               {
               previous=&(current->next);
               current=current->next;
               }
            }	 
         }while (!done);
      }
   };
   


class connectiontype{

private:   
   array<int> table;
   
public:
   connectiontype(int _max_objects) : table(_max_objects)
      {
      Reset();
      }
   connectiontype(void *_external_mem=NULL, size_t external_size=0) : table(_external_mem, external_size)
      {}
   
   void Reset()
      {
      for (int i=0; i<table.size(); i++)
         table[i] = i;
      }
   // returns the size of the max id +1
   int size() const
      {
      return table.size();
      }
   // returns the size of the max id +1
   void resize(int _max_objects)
      {
      int oldsize = table.size();
      
      table.resize(_max_objects);
      for (int i=oldsize; i<_max_objects; i++)
         table[i] =  i;
      }
   // adds an entry to end. Will return handle to entry
   int push()
      {
      int ret = table.size();
      table.push(ret);
      return ret;
      }
   bool Merge(int a, int b)  // returns true if different nets
      {
      int i, expanse=MAXIMUM(a,b);
      if (a<0 || b<0) FATAL_ERROR;
      
      if (expanse >= table.size())
         {
         for (i=table.size(); i<=expanse; i++)
            table.push(i);   // grow table
         }

      i = a;      
      while (table[i] != i)   // this will find the head
         i = table[i];
      table[table[a]] = i;
      table[a] = i;
      
      i = b;      
      while (table[i] != i)   // this will find the head
         i = table[i];
      table[table[b]] = i;
      table[b] = i;

      if (table[a] != table[b])
         {
         if (table[a] < table[b])
            table[table[b]] = table[a];
         else
            table[table[a]] = table[b];
         return true;
         }
      return false;
      }
   
/*   bool IsSemaphore(int a) const
      {
      return table[a]<0;
      }
   void MakeSemaphore(int a)
      {
      table[a]= -1;
      }*/

   bool IsSame(int a, int b)
      {
      int i, expanse=MAXIMUM(a,b);
      if (a<0 || b<0) FATAL_ERROR;
      
      if (expanse >= table.size())
         {
         for (i=table.size(); i<=expanse; i++)
            table.push(i);   // grow table
         }

      i = a;      
      while (table[i] != i)   // this will find the head
         i = table[i];
      table[table[a]] = i;
      table[a] = i;
      
      i = b;      
      while (table[i] != i)   // this will find the head
         i = table[i];
      table[table[b]] = i;
      table[b] = i;

      return table[a] == table[b];      
      }   
   void MakeLoop()  // this will make a circular loop for all connections. 
                    // IsSame(), UniqueNets(), Merge() are now invalid
      {
      int i;
      
      for (i=0; i<table.size(); i++)
         {
         if (table[i]!=i){
            int s = table[i];
            table[i] = table[s];
            table[s] = i;
            }
         }
      }
   int Raw(int a) const
      {
      return table[a];
      }
   int Handle(int a) const  // only valid until another successful Merge happens
      {
      int i;
      if (a<0 || a>= table.size()) FATAL_ERROR;
      
      
      i = a;      
      while (table[i] != i)   // this will find the head
         {
         i = table[i];
         }
      
      // override const. Making this assign doesn't change the connection relationship, just improves performance
      if (table[a]!=i)
         ((array<int> &)table)[a] = i;
      return i;
      }
   int UniqueNets() const
      {
      int i, count=0;

      for (i=0; i<table.size(); i++)
         if (table[i]==i) 
            count++;
      return count;
      }
   };


// this will allow N bit integers
template <int N> class bitfieldtype{
private:
#define words ((N+63)>>6)
   __uint64 data[words];

public:
   bitfieldtype()
      {
      for (int i=0; i<words; i++)
         data[i]=0;
      }
   bitfieldtype(const __uint64 right)
      {
      operator=(right);
      }
   __int64 Int64() const
      {
      return data[0];
      }
   bitfieldtype &operator=(const __uint64 right)
      {
      data[0] = right;
      
      for (int i=1; i<words; i++)
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
      for (int i=0; i<words; i++)
         ret.data[i] = ~data[i];
      return ret;
      }
   bitfieldtype operator+(const bitfieldtype &right) const
      {
      bitfieldtype ret;
      int carry=0;
      for (int i=0; i<words; i++)
         {
         ret.data[i] = data[i] + right.data[i] + carry;
         const __uint64 cin = (data[i] ^ right.data[i] ^ ret.data[i]);
         carry = ((cin & data[i]) | (cin & right.data[i]) | (data[i] & right.data[i])) >>63;
         }
      return ret;
      }
   bitfieldtype operator-(const bitfieldtype &right) const
      {
      bitfieldtype ret;
      int carry=1;
      for (int i=0; i<words; i++)
         {
         ret.data[i] = data[i] + (~right.data[i]) + carry;
         const __uint64 cin = (data[i] ^ (~right.data[i]) ^ ret.data[i]);
         carry = ((cin & data[i]) | (cin & (~right.data[i])) | (data[i] & (~right.data[i]))) >>63;
         }
      return ret;
      }
   bitfieldtype operator|(const bitfieldtype &right) const
      {
      bitfieldtype ret;
      for (int i=0; i<words; i++)
         ret.data[i] = data[i] | right.data[i];
      return ret;
      }
   bitfieldtype operator&(const bitfieldtype &right) const
      {
      bitfieldtype ret;
      for (int i=0; i<words; i++)
         ret.data[i] = data[i] & right.data[i];
      return ret;
      }
   bitfieldtype operator^(const bitfieldtype &right) const
      {
      bitfieldtype ret;
      for (int i=0; i<words; i++)
         ret.data[i] = data[i] ^ right.data[i];
      return ret;
      }
   bitfieldtype &operator|=(const bitfieldtype &right)
      {      
      for (int i=0; i<words; i++)
         data[i] |= right.data[i];
      return *this;
      }
   bitfieldtype &operator&=(const bitfieldtype &right)
      {      
      for (int i=0; i<words; i++)
         data[i] &= right.data[i];
      return *this;
      }
   bitfieldtype operator<<(int left) const
      {
      if (left<0) FATAL_ERROR;
      bitfieldtype ret;
      __uint64 carry=0;
      int bit=left&0x3f, offset=left>>6;
      if (offset>=words)
         return ret;

      for (int i=0; i<words - offset; i++)
         {
         __uint64 nextcarry = data[i] >> (64 - bit);
         ret.data[i+offset] = (data[i]<<bit) | carry;
         carry = bit ? nextcarry :0;
         }
      return ret;
      }
   bitfieldtype operator>>(int right) const
      {
      if (right<0) FATAL_ERROR;
      bitfieldtype ret;
      __uint64 carry=0;
      int bit=right&0x3f, offset=right>>6;
      if (offset>=words)
         return ret;

      for (int i=words-1; i>=offset; i--)
         {
         __uint64 nextcarry = data[i] << (64 - bit);
         ret.data[i-offset] = (data[i]>>bit) | carry;
         carry = bit ? nextcarry :0;
         }
      return ret;
      }
   bitfieldtype operator*(const bitfieldtype &right) const
      {
      bitfieldtype ret = 0;
      int i;

      for (i=N-1; i>=0; i--)
         {
         ret = ret << 1;
         if (GetBit(i))
            ret = ret + right;
         }
      return ret;
      }
   bool operator==(const bitfieldtype &right) const
      {
      int i;
      for (i=0; i<words; i++)
         if (data[i] != right.data[i])
            return false;
      return true;
      }
   bool operator!=(const bitfieldtype &right) const
      {
      return ! operator==(right);
      }
   bool operator<(const bitfieldtype &right) const
      {
      int i;
      for (i=words-1; i>=0; i--)
         {
         if (data[i] < right.data[i])
            return true;
         if (data[i] > right.data[i])
            return false;
         }
      return false;
      }
   bool operator<=(const bitfieldtype &right) const
      {
      int i;
      for (i=words-1; i>=0; i--)
         {
         if (data[i] < right.data[i])
            return true;
         if (data[i] > right.data[i])
            return false;
         }
      return true;
      }
   bool operator>(const bitfieldtype &right) const
      {
      int i;
      for (i=words-1; i>=0; i--)
         {
         if (data[i] > right.data[i])
            return true;
         if (data[i] < right.data[i])
            return false;
         }
      return false;
      }
   bool operator>=(const bitfieldtype &right) const
      {
      int i;
      for (i=words-1; i>=0; i--)
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
      int i, count=0;
      
      for (i=0; i<N; i++)
         {
         if (GetBit(i))
            {
            if (count==index)
               return i;
            count++;
            }
         }
      return -1;   // index >= PopCount
      }
   int Msb() const 
      {
      int i;
      
      for (i=N-1; i>=0; i--)
         {
         if (GetBit(i))
            return i;
         }
      return -1;
      }
   int Lsb() const 
      {
      int i;
      
      for (i=0; i<N; i++)
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
      if (bit <0 || bit >= N) FATAL_ERROR;
      data[bit>>6] |= (__uint64)1<<(bit&63);
      }
   void ClearBit(const int bit)
      {
      if (bit <0 || bit >= N) FATAL_ERROR;
      data[bit>>6] &=  ~((__uint64)1<<(bit&63));
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
      if (bit <0 || bit >= N) FATAL_ERROR;
      return 0 != (data[bit>>6] &  ((__uint64)1<<(bit&63)));
      }
//   operator __uint64() const
//      {
//      return data[0];
//      }   
   char *Print(char *buffer, int nibble=0) const
      {
      char *chptr=buffer;
      int i;
      
      nibble *=4;
      for (i=words-1; i>0 && data[i]==0 && nibble==0; i--)   // skip over leading zeros
         ;

      for (; i>=0; i--)
         for (int k=64-4; k>=0; k-=4)
            {
            unsigned a = (data[i]>>k) &0xF;
            *chptr = a + (a>9 ? 'A'-10 : '0');
            chptr++;
            if (nibble && (k%nibble)==0 && (i!=0 || k!=0))
               {
               *chptr='_'; chptr++;
               }
            }
      *chptr = 0;
      return buffer;
      }
   int DestructivePopCount()  // will destroy data but be fast (64 bit version)
      {
      __uint64 a, b;
      int i, count;
      for (i=0; i<words; i++)
         {         
         a = data[i] & 0x5555555555555555;
         b = data[i] & 0xAAAAAAAAAAAAAAAA;
         b = b>>1;
         data[i] = a + b;
         }
      for (i=0; i<words; i++)
         {         
         a = data[i] & 0x3333333333333333;
         b = data[i] & 0xCCCCCCCCCCCCCCCC;
         b = b>>2;
         data[i] = a + b;
         }
      for (i=0; i<words; i++)
         {         
         a = data[i] & 0x0F0F0F0F0F0F0F0F;
         b = data[i] & 0xF0F0F0F0F0F0F0F0;
         b = b>>4;
         data[i] = a + b;
         }
      for (i=0; i<words; i++)
         {         
         a = data[i] & 0x00FF00FF00FF00FF;
         b = data[i] & 0xFF00FF00FF00FF00;
         b = b>>8;
         data[i] = a + b;
         }
      for (i=0; i<words; i++)
         {         
         a = data[i] & 0x0000FFFF0000FFFF;
         b = data[i] & 0xFFFF0000FFFF0000;
         b = b>>16;
         data[i] = a + b;
         }
      for (i=0; i<words; i++)
         {         
         a = data[i] & 0x00000000FFFFFFFF;
         b = data[i] & 0xFFFFFFFF00000000;
         b = b>>32;
         data[i] = a + b;
         }
      count = 0;
      for (i=0; i<words; i++)
         {
         count += data[i];
         }
      return count;
      }
#undef words
   };


// reasonably good random number generator from numerical recipes
// random number returned has a period on the order of 2^18
// I return a double instead of float to remove a boundary condition
// returns a number 0.0<= x <1.0 Watch out converting output to float 
// can cause rounding up to 1.0
inline double random(int &seed)  // initially set seed to anything except MASK value, zero is preferred
   {
   const int IA   =      16807;
   const int IM   = 2147483647;
   const int IQ   =     127773;
   const int IR   =       2836;
   const int MASK =  123459876;
   int k;
   double answer;

   seed ^= MASK;
   k = seed / IQ;
   seed = IA * (seed - k *IQ) - IR*k;
   if (seed <0) seed += IM;

   answer = (1.0/IM) * seed;
   seed ^= MASK;
   if (answer < 0.0 || answer >=1.0) FATAL_ERROR;
   return answer;
   }

inline unsigned int crc32(const void *mem, int len)
   {
   unsigned int accum=0xFFFFFFFF;
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

inline unsigned short crc16(const void *mem, int len)
   {
   unsigned short accum=0xFFFF;
   const unsigned char *buffer = (const unsigned char *)mem;
   int i,k;

   for (i=0; i<len; i++)
      {
      accum ^= buffer[i];
      for (k=0; k<8; k++)
         {
         if (accum&1)            
            accum = (accum>>1)^0xA001;
         else
            accum = accum>>1;
         }
      }
   return accum;
   }

inline double Round(double x, double rnd_amount)
   {
   int k = (int)((x + rnd_amount/2.0) / rnd_amount);
   
   return k * rnd_amount;
   }

// this function will count the number of '1' in the input
inline int PopCount(__uint64 x)
   {
   unsigned high=x,low=x>>32;
   unsigned a,b,c,d;
   
   a = high & 0x55555555;
   c = low  & 0x55555555;
   b = high & 0xAAAAAAAA;
   d = low  & 0xAAAAAAAA;
   b = b>>1;
   d = d>>1;
   high = a + b;
   low  = c + d;

   a = high & 0x33333333;
   c = low  & 0x33333333;
   b = high & 0xCCCCCCCC;
   d = low  & 0xCCCCCCCC;
   b = b>>2;
   d = d>>2;
   high = a + b;
   low  = c + d;
   
   a = high & 0x0F0F0F0F;
   c = low  & 0x0F0F0F0F;
   b = high & 0xF0F0F0F0;   
   d = low  & 0xF0F0F0F0;
   b = b>>4;
   d = d>>4;
   high = a + b;
   low  = c + d;

   a = high & 0x00FF00FF;
   c = low  & 0x00FF00FF;
   b = high & 0xFF00FF00;   
   d = low  & 0xFF00FF00;
   b = b>>8;
   d = d>>8;
   high = a + b;
   low  = c + d;

   a = high & 0x0000FFFF;
   c = low  & 0x0000FFFF;
   b = high & 0xFFFF0000;   
   d = low  & 0xFFFF0000;
   b = b>>16;
   d = d>>16;
   high = a + b;
   low  = c + d;

   return high+low;
   }

// this function will count the number of '1' in the input
inline int PopCount(unsigned int x)
   {
   unsigned a,b;
   
   a = x & 0x55555555;
   b = x & 0xAAAAAAAA;
   b = b>>1;
   x = a + b;

   a = x & 0x33333333;
   b = x & 0xCCCCCCCC;
   b = b>>2;
   x = a + b;
   
   a = x & 0x0F0F0F0F;
   b = x & 0xF0F0F0F0;   
   b = b>>4;
   x = a + b;

   a = x & 0x00FF00FF;
   b = x & 0xFF00FF00;   
   b = b>>8;
   x = a + b;

   a = x & 0x0000FFFF;
   b = x & 0xFFFF0000;   
   b = b>>16;
   x = a + b;

   return x;
   }

// this function will count the number of '1' in the input
inline int PopCount(unsigned short x)
   {
   unsigned short a,b;
   
   a = x & 0x5555;
   b = x & 0xAAAA;
   b = b>>1;
   x = a + b;

   a = x & 0x3333;
   b = x & 0xCCCC;
   b = b>>2;
   x = a + b;
   
   a = x & 0x0F0F;
   b = x & 0xF0F0;   
   b = b>>4;
   x = a + b;

   a = x & 0x00FF;
   b = x & 0xFF00;   
   b = b>>8;
   x = a + b;

   return x;
   }

inline char *strupr(char *txt)  // convert a string to uppercase
   {
   char *chptr=txt;
   while (*chptr != 0)
      {
      if (*chptr >='a' && *chptr <='z')
         *chptr += 'A' - 'a';
      chptr++;
      }
   return txt;
   }

inline char *strlwr(char *txt)  // convert a string to lowercase
   {
   char *chptr=txt;
   while (*chptr != 0)
      {
      if (*chptr >='A' && *chptr <='Z')
         *chptr += 'a' - 'A';
      chptr++;
      }
   return txt;
   }

inline int stricmp(const char *a, const char *b)  // compare case insensitive
   {
   do{
      char aa = *a, bb = *b;
      if (aa>='a' && aa<='z')
         aa += 'A' - 'a';
      if (bb>='a' && bb<='z')
         bb += 'A' - 'a';
      if (aa < bb)
         return -1;
      if (aa > bb)
         return +1;
      a++;
      b++;
      if (*a ==0 && *b !=0)
         return -1;
      if (*a !=0 && *b ==0)
         return +1;
      }while (*a != 0 && *b != 0);
   return 0;
   }

inline int strnicmp(const char *a, const char *b, int len)  // compare case insensitive
   {
   do{
      len--;
      char aa = *a, bb = *b;
      if (aa>='a' && aa<='z')
         aa += 'A' - 'a';
      if (bb>='a' && bb<='z')
         bb += 'A' - 'a';
      if (aa < bb)
         return -1;
      if (aa > bb)
         return +1;
      a++;
      b++;
      if (len==0) return 0;
      if (*a ==0 && *b !=0)
         return -1;
      if (*a !=0 && *b ==0)
         return +1;
      }while (*a != 0 && *b != 0);
   return 0;
   }

inline int strcmpxx(const char *a, const char *b)  // string compare that understands numbers
   {
   do{
      if (*a <'0' || *a >'9' || *b <'0' || *b >'9')
         {
         if (*a < *b) return -1;
         if (*a > *b) return +1;
         a++;
         b++;
         }
      else
         {
         int numa=0, numb=0;
         int adigits=0, bdigits=0;

         while (*a>='0' && *a<='9')
            {
            numa = numa*10 + *a-'0';
            adigits++;
            a++;
            }
         while (*b>='0' && *b<='9')
            {
            numb = numb*10 + *b-'0';
            bdigits++;
            b++;
            }
         if (numa < numb) return -1;
         if (numa > numb) return +1;
         if (adigits < bdigits) return -1;
         if (adigits > bdigits) return +1;
         }
      }while (*a != 0 || *b != 0);
   return 0;
   }

// will return number of matching characters between 2 strings
inline int strcorrelation(const char *a, const char *b)
   {
   int count = 0;
   while (*a!=0 && *b!=0 && *a==*b)
      {
      count++;
      a++;
      b++;
      }
   return count;
   }

inline char *strrstr(char *txt, const char *subtext)
   {
   char *chptr=txt, *lastptr;
   
   while ( (chptr=strstr(chptr,subtext))!=NULL)
      {
      lastptr = chptr;
      chptr++;
      }     
   return(lastptr);   
   }
inline const char *strrstr(const char *txt, const char *subtext)
   {
   const char *chptr=txt, *lastptr;
   
   while ( (chptr=strstr(chptr,subtext))!=NULL)
      {
      lastptr = chptr;
      chptr++;
      }     
   return(lastptr);   
   }

inline const char *Int64ToString(__uint64 x, char *buffer)
   {
   int i;
   char *chptr;
   for (chptr=buffer, i=15; i>=0; i--)
      {
      if (chptr!=buffer || (x>>(i*4)))
         {
         *chptr = (x>>(i*4))&0xF;
         if (*chptr<10)
            *chptr += '0';
         else
            *chptr += 'A'-10;
         chptr++;
         }
      }
   if (chptr==buffer)
      {*chptr='0'; chptr++;}
   *chptr=0; // null terminate
   return buffer;
   }

inline int new_atoi(const char *txt)
   {
   bool negative=false;
   int number=0;

   if (txt[0]=='-')
      {negative = true; txt++;}
   else if (txt[0]<'0' || txt[0]>'9')
      FATAL_ERROR;         // string isn't an integer

   while (txt[0]>='0' && txt[0]<='9')
      {
      if (number>(0x7fffffff/10)) FATAL_ERROR; // overflow
      number*=10;
      number+=txt[0] - '0';
      txt++;
      }
   if (negative)
      number = -number;
   
   return number;
   }
#define atoi new_atoi

inline __int64 atoint64(const char *txt)
   {
   bool negative=false;
   __int64 number=0;

   if (txt[0]=='-')
      {negative = true; txt++;}
   else if (txt[0]<'0' || txt[0]>'9')
      FATAL_ERROR;         // string isn't an integer
   
   while (txt[0]>='0' && txt[0]<='9')
      {
#ifdef _MSC_VER
      if (number>(0x7fffffffffffffff/10)) FATAL_ERROR; // overflow
#else
      if (number>(0x7fffffffffffffffLL/10)) FATAL_ERROR; // overflow
#endif
      number*=10;
      number+=txt[0] - '0';
      txt++;
      }
   if (negative)
      number = -number;
   
   return number;
   }

inline unsigned __int64 atoint64hex(const char *txt)
   {
   unsigned __int64 number=0, digit;
   bool found=false;
      
   while(true)
      {
      if (txt[0]>='0' && txt[0]<='9')
         digit = txt[0]-'0';
      else if (txt[0]>='a' && txt[0]<='f')
         digit = txt[0]-'a'+10;
      else if (txt[0]>='A' && txt[0]<='F')
         digit = txt[0]-'A'+10;
      else break;

      found = true;
      number = (number<<4) + digit;
      txt++;
      }
   if (!found) FATAL_ERROR;
   
   return number;
   }


// will determine the an edge of right triangle given hypotenuse and other edge.
inline int hypotenuse(int diagonal, int edge)
   {
   static int lastdiagonal, lastedge, lastanswer;

   if (diagonal == lastdiagonal && lastedge == edge)
      return lastanswer;

   lastdiagonal = diagonal;
   lastedge     = edge;
   lastanswer   = 1 + (int)sqrt((double)diagonal*diagonal - edge*edge);
   return lastanswer;
   }


class stringPooltype{
private:
   array<const char *> stringPools;
   char *pool;
   int poolsize;
   texthashtype <const char *> h;
   CRITICAL_SECTION CS_stringPool;

public:

   stringPooltype()
     {
       pool     = NULL;
       poolsize = 0;
       InitializeCriticalSection(&CS_stringPool);
     }
   ~stringPooltype()
      {
      clear();
      DeleteCriticalSection(&CS_stringPool);
      }

   void clear()
      {
      for (int i=0; i<stringPools.size(); i++)
         {
         delete [](char *)stringPools[i];  // typecast away const
         stringPools[i]=NULL;
         }
      stringPools.clear();
      h.clear();
      pool = NULL;
      poolsize = 0;
      }
   const char *sprintf(char *fmt, ...)
      {
      va_list args;
      char buffer[4096];
      
      va_start(args, fmt);
      int count = _vsnprintf(buffer, sizeof(buffer), fmt, args);
      va_end(args);
      if (strlen(buffer)>= sizeof(buffer)) FATAL_ERROR;
      return NewString(buffer);
      };
   const char *NewString(const char *txt)
      {
      int len = (int)strlen(txt)+1;
      const char *ptr=NULL;
      
      if (h.Check(txt, ptr))
         return ptr;           // already seen this string before, send copy
      EnterCriticalSection(&CS_stringPool);
      if (poolsize < len)
         {
         poolsize = 4096*32-64;
         pool = new char[poolsize];
         if (pool==NULL) 
            FATAL_ERROR;
         stringPools.push_back(pool);  // remember so we can do a delete[] on exit      
         }
      char *ret = pool;
      strcpy(pool, txt);
      poolsize -= len;
      pool     += len;
      LeaveCriticalSection(&CS_stringPool);
      h.Add(ret, ret);
      return ret;
      }
   };


struct real_pcre;                 
typedef struct real_pcre pcre;
class wildcardtype{
   mutable bool needsCompile, caseSensitive;
   mutable array <char> allPatterns;   // will be like (pat1)|(pat2)...
   texthashtype<bool> simple;
   mutable pcre *code;
public:

   wildcardtype(bool _caseSensitive=false)
      {
      caseSensitive = _caseSensitive;
      needsCompile = false;
      code = NULL;
      }
   ~wildcardtype()
      {
      if (code!=NULL)
         free(code);
      code=NULL;
      }
   void Add(const char *pattern)
      {
      if (strchr(pattern,'\\')==NULL && strchr(pattern,'.')==NULL && strchr(pattern,'*')==NULL && strchr(pattern,'+')==NULL && strchr(pattern,'[')==NULL && strchr(pattern,'?')==NULL)
         {// this is a performance optimization, not needed for correctness
         simple.Add(pattern, true);
         return;
         }

      if (allPatterns.size()!=0)
         allPatterns.push('|');
      allPatterns.push('(');

      int i, len=(int)strlen(pattern);
      if (len<=0) FATAL_ERROR;
      for (i=0; i<len; i++)
         allPatterns.push(pattern[i]);
      allPatterns.push('\\');
      allPatterns.push('0');
      allPatterns.push(')');
      needsCompile=true;
      }
   void Remove(const char *pattern)
      {
      if (strchr(pattern,'\\')==NULL && strchr(pattern,'.')==NULL && strchr(pattern,'*')==NULL && strchr(pattern,'+')==NULL && strchr(pattern,'[')==NULL && strchr(pattern,'?')==NULL)
         {// this is a performance optimization, not needed for correctness
         simple.Add(pattern, false);
         return;
         }
      else 
         FATAL_ERROR; // I don't support negative general patterns
      }
   bool Check(const char *string) const;  // lives in common.cpp, return true if match
   };


#endif
