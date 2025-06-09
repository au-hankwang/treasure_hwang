#include "pch.h"
#include "helper.h"
#include "miner.h"
#include "dvfs.h"
#include "electrical.h"
#include "multithread.h"
#include "usb.h"
#include "test.h"


void Test();
void DataProcessing();

void MinerDebug()
   {
   headertype h;
   uint32 message[16], second_message[16];
   shadigesttype digest, rtl, second;

   command_cfgtype foo;
   memset(&foo, 0, sizeof(foo));
   foo.id2 = 3;


/*
   digest.a = digest.b = digest.c = digest.d = digest.e = digest.f = digest.g = digest.h;
   memset(message, 0, sizeof(message));
   message[3] = 0x24;
   RTL2Hash(digest, message);
   return;
*/
//   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");

//   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   h.AsciiIn("0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
//   h.AsciiIn("0100000000000000000000000000000000000000000000000000000000000000000000003ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a29ab5f49ffff001d1dac2b7c");
//   h.AsciiIn("0100000000000000000000000000000000000000000000000000000000000000000000003ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a29ab5f49ffff001d9dac107c");
   //   int foo = h.ReturnZeroes();
//   return;

   bool win = h.RTL_Winner();
   return;

   
/*
   digest.a = 0;
   digest.b = 0;
   digest.c = 0;
   digest.d = 0;
   digest.e = 0;
   digest.f = 0;
   digest.g = 0;
   digest.h = 0;
*/
   message[0] = 0;
   message[1] = 0;
   message[2] = 0;
   message[3] = 0;
   message[4] = 0x80000000;
   message[5] = 0;
   message[6] = 0;
   message[7] = 0;
   message[8] = 0;
   message[9] = 0;
   message[10] = 0;
   message[11] = 0;
   message[12] = 0;
   message[13] = 0;
   message[14] = 0;
   message[15] = 640;
   rtl = digest;
   sha256ProcessBlock(digest, message);
   RTLsha256ProcessBlock(rtl, message);
   if (digest != rtl) FATAL_ERROR;

   return;

   shadigesttype d2;
   second_message[0] = digest.a;
   second_message[1] = digest.b;
   second_message[2] = digest.c;
   second_message[3] = digest.d;
   second_message[4] = digest.e;
   second_message[5] = digest.f;
   second_message[6] = digest.g;
   second_message[7] = digest.h;
   second_message[8] = 0x80000000;
   second_message[9] = 0;
   second_message[10] = 0;
   second_message[11] = 0;
   second_message[12] = 0;
   second_message[13] = 0;
   second_message[14] = 0;
   second_message[15] = 256;
   sha256ProcessBlock(second, second_message);

   }



// this expects 2 characters for each ascii character
void HexToAscii(vector<char>& in)
   {
   vector<char> out;
   int i;

   if (in.size() % 2) FATAL_ERROR;
   for (i = 0; i < in.size(); i++) {
      const char& ch = in[i];
      int digit;
      if (ch >= '0' && ch <= '9')
         digit = ch - '0';
      else if (ch >= 'a' && ch <= 'f')
         digit = ch - 'a' + 10;
      else if (ch >= 'A' && ch <= 'F')
         digit = ch - 'A' + 10;
      else break;
      if (i % 2 == 0)
         out.push_back(digit << 4);
      else
         out.back() += digit;
      }
   in = out;
   }


class grindForHittype : public MultiThreadtype {
   void Prolog() {}
   void Epilog() {}
   void Func(const int threadnum) {
      headertype h;
      h.x.nonce = 0x3f93ada7;// 0xa7ad933f;
      h.words[0] = threadnum;
      int inner, outer;

      for (outer = 0; outer < 0x7fffffff; outer++) {
         h.words[1] = outer;
         for (inner = 0; inner < 0x7fffffff; inner++)
            {
            h.words[2] = inner;
            if (h.ReturnZeroes() >= 32) {
               Lock();
               printf("Found a example, [0]=%d, [1]=%x, [2]=%x, zeroes=%d\n", threadnum, outer, inner, h.ReturnZeroes());
               UnLock();
               }
            }
         }
      }
   void Func2(const int threadnum) {
      boardmodeltype boardmodel;
      __int64 count = 0;
      int i;
      int seed = threadnum * 100 + time(NULL);
      Lock();
      boardmodel.WriteConfig(16, (2048 << 16) + (4 << 0));   // version upper/lower
      boardmodel.WriteConfig(17, 5);  // version shift
      boardmodel.WriteConfig(20, 0x8000);  // small nonce
      boardmodel.tinynonce = true;
      UnLock();
      while (true) {
         headertype input, hit;
         int ctx, difficulty, sequence;
         for (i = 0; i < 20; i++)
            input.words[i] = random(seed) * ((__int64)1 << 40);
         boardmodel.SendJob(input, 32, 0);
         for (count = 0; count < ((__int64)1 << 32); count++) {
            boardmodel.ExecuteCycle();
            if ((count & 0xffffff) == 0) {
               Lock();
               if (boardmodel.ReturnHit(hit, ctx, difficulty, sequence)) {
                  printf("\ahit found, difficulty=%d count=%I64d\n", difficulty, count);

                  FILE* fptr = fopen(boardmodel.tinynonce ? "tinynonce.csv" : "easyhits.csv", "at");
                  vector<char> input_ascii, hit_ascii;
                  input.AsciiOut(input_ascii);
                  hit.AsciiOut(hit_ascii);
                  fprintf(fptr, "%s,%s,%d,%d,%x, %I64d\n", &input_ascii[0], &hit_ascii[0], ctx, difficulty, hit.x.nonce, count);
                  fclose(fptr);
                  }
               UnLock();
               }
            }
         }
      }
   };

void Electrical_Sim();



void Gray()
   {
   int i, gray;
   vector<bool> repeats(16777216, false);
   int pattern[] = { 0,1,0,2,0,1,0 };

   for (i = 0; i < 16777216; i++)
      {
      gray = (i >> 1) ^ i;
      int inverse = gray;
      inverse ^= inverse >> 1;
      inverse ^= inverse >> 2;
      inverse ^= inverse >> 4;
      inverse ^= inverse >> 8;
      inverse ^= inverse >> 16;
//      printf("i=%5d gray=%x inverse=%d\n", i, gray, inverse);
      if (repeats[gray]) {
         printf("found a repeat\n");
         break;
         }
      if (inverse!=i){
         printf("inverse didn't work\n");
         break;
         }
   repeats[gray] = true;
      }
   }

void lfsr() {
   unsigned lfsr = -1;
   __int64 count = 0;
   do {  // 32,22,2,1
      lfsr = (lfsr << 1) | (((lfsr >> 31) ^ (lfsr >> 21) ^ (lfsr >> 1) ^ (lfsr >> 0)) &1);
      count++;
      } while (lfsr != -1);
    printf("count = % I64d\n",count);

    exit(0);
   }


int Cost(vector<int> &position, const vector<__int64> &connectivity) {
   int tension = 0;
   int i, k;
   vector<int> xref = position;

   for (i = 0; i < connectivity.size(); i++)
      xref[position[i]] = i;


   for (i=0; i< connectivity.size(); i++)
      for (k = 0; k < connectivity.size(); k++)
         {
         if ((connectivity[i] >> k) & 1)
            tension += labs(xref[i] - xref[k]);
         }
   return tension;
   }


void Raju()
   {
   vector<__int64> connectivity;
   vector<int> newposition, position, bestposition;
   int i, k;
   int last_cost, new_cost;
   bool done;

   connectivity.push_back(0x26);
   connectivity.push_back(0x11);
   connectivity.push_back(0x09);
   connectivity.push_back(0x04);
   connectivity.push_back(0x02);
   connectivity.push_back(0x01);

   for (i = 0; i < connectivity.size(); i++)
      position.push_back(i);

   while (true) {
      done = true;
      for (i = 0; i < connectivity.size(); i++)
         printf("%c \t", 'A' + position[i]);
      last_cost = Cost(position, connectivity);
      printf("total:%d\n", last_cost);

      // try a dumb greedy algorithm
      int best_cost = last_cost + 1;
      for (i = 0; i < connectivity.size() - 1; i++) {
         for (k = i + 1; k < connectivity.size(); k++)
            {
            newposition = position;
            newposition[i] = position[k];
            newposition[k] = position[i];
            new_cost = Cost(newposition, connectivity);
            if (new_cost < best_cost)
               {
               best_cost = new_cost;
               bestposition = newposition;
               }
            }
         }
      if (best_cost < last_cost) {
         position = bestposition;
         done = false;
         }
      if (done) break;
      }
   exit(-1);
   }


void Foo() {
   usbtype usb;
   char buffer[512];

   usb.Init();


/*
   for (int i = 0; i < 512; i++) 
      buffer[i] = 'A' + (i % 26);
   usb.SendData(3, buffer, 248);
   usb.CheckForError();
   for (int i = 0; i < 496; i++) {
      sprintf(buffer, "\r\ncount %d with a lot of filler.", i);
      usb.SendData(3, buffer, i);
      usb.CheckForError();
//      usb.SendData(3, buffer, strlen(buffer));
      }*/
   usb.SendData(3,"Hello world\n", 12);

   while (true)
      {
      usb.SendData(3, buffer, 0);
//      printf("gpio = % x\n", usb.ReadWriteGPIO(gpiouniontype()));
      }

   }


void Constants();
void Simulator();
void MinerExperiment();

static unsigned int __stdcall raw_thread_handler_simulator(void* param) {
   Simulator();
   return 0;
   }






