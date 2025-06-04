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

struct spicetype{
   int type;
   float v, t, delay;
   };

void Evaluate(const vector<spicetype>& records)
   {
   double vt, mobility, error;
   double bestvt=-1, bestmobility, besterror;
   int i;

   for (vt = 0.100; vt < .350; vt += 0.001)
      {
      vector<double> fit;
      mobility = 0.0, error=0.0;
      for (i = 0; i < records.size(); i++)
         {
         const spicetype& s = records[i];
         fit.push_back((s.v-vt)*(s.v-vt)/s.v);
         mobility += (s.delay * fit.back()) / records.size();
         }
      for (i = 0; i < records.size(); i++)
         {
         const spicetype& s = records[i];
         error += fabs(fit[i] - ((mobility) / s.delay));
         }
      if (bestvt<0 || besterror>fabs(error)) {
         bestvt = vt;
         bestmobility = mobility;
         besterror = fabs(error);
         }
      }
/*   printf("%s temp=%2.0f vt=%.3f mobility=%.3e error=%.3e\n", records[0].type == 0 ? "elvt4" :
      records[0].type == 1 ? "elvt6" :
      records[0].type == 2 ? "ulvt4" :
      records[0].type == 3 ? "ulvt6" : "Unknown",
      records[0].t, bestvt, bestmobility, besterror);*/
   printf("%s temp=%2.0f vt=%.3f mobility=%.2e\n", records[0].type == 0 ? "elvt4" :
      records[0].type == 1 ? "elvt6" :
      records[0].type == 2 ? "ulvt4" :
      records[0].type == 3 ? "ulvt6" : "Unknown",
      records[0].t, bestvt, bestmobility);
   }

void VtProcess()
   {
   char buffer[1024], *chptr;
   spicetype s;
   vector<spicetype> records;
   FILE* fptr = fopen("d:\\junk\\xnode_evaluation.csv","rt");
   if (fptr == NULL) FATAL_ERROR;

   while (NULL != fgets(buffer, sizeof(buffer) - 1, fptr))
      {
      s.type = 0;
      chptr = strstr(buffer, "elvt4,");
      if (chptr == NULL) {
         s.type = 2;
         chptr = strstr(buffer, "ulvt4,");
         }
      if (chptr == NULL) {
         s.type = 1;
         chptr = strstr(buffer, "elvt6,");
         }
      if (chptr == NULL) {
         s.type = 3;
         chptr = strstr(buffer, "ulvt6,");
         }
      
      if (chptr != NULL) {
         s.v = atof(chptr + 6);
         chptr = strchr(chptr + 6, ',');
         s.t = atof(chptr + 1);
         chptr = strchr(chptr + 1, ',');
         s.delay = atof(chptr + 1);
//         printf("%d %.2f %.1f %.2e\n", s.type, s.v, s.t, s.delay);
         if (records.size() != 0 && records.back().t != s.t)
            {
            Evaluate(records);
            records.clear();
            }
         records.push_back(s);
         }
      }
   }


struct asktype {
   unsigned __int64 mask, address;
   int depth;
   asktype()
      {
      mask = ~0;
      address = 0;
      depth = 0;
      }
   void Print(){
      int i;
      for (i = 0; i < 64; i++) {
         if ((mask >> i) & 1)
            printf("x");
         else
            printf("%c", (address >> i) & 1 ? '1' : '0');
         }
      }
   void SetBit(int depth)
      {
      unsigned __int64 x;
      x = 1;
      x = x << depth;
      x = ~x;
      address &= x;
      mask &= x;
      address |= ~x;
      }
   void ClearBit(int depth)
      {
      unsigned __int64 x;
      x = 1;
      x = x << depth;
      x = ~x;
      address &= x;
      mask &= x;
      }
   bool operator==(const asktype& right) const
      {
      return (address | mask | right.mask) == (right.address | mask | right.mask);
      }
   void Random()
      {
      static int seed = 1;
      for (int i = 0; i < 4; i++) {
         unsigned __int64 foo = random(seed) * ((__int64)1 << 20);
         address = foo + (address << 16);
         }
      mask = 0;
      }
   };


void Discovery()
   {
   vector<asktype> chips;
   vector<asktype> stack;
   asktype current;
   int seed = 1;
   int i;
   int found = 0, searches=0, timeouts=0;

   for (i = 0; i < 132; i++)
      {
      chips.push_back(asktype());
      chips.back().Random();
      printf("\nasic %3d: ",i);
      chips.back().Print();
      }
   stack.push_back(asktype());
   while (stack.size() > 0)
      {
      current = stack.back();
      stack.pop_back();

      asktype left = current, right = current;
      left.ClearBit(current.depth);
      right.SetBit(current.depth);
      left.depth++;
      right.depth++;
      int leftresult = 0, rightresult = 0;
      for (i = 0; i < chips.size(); i++) {
         leftresult += chips[i] == left ? 1 : 0;
         rightresult += chips[i] == right ? 1 : 0;
         }
      if (leftresult == 0) timeouts++;
      else if (leftresult == 1) found++;
      else if (leftresult > 1)
         stack.push_back(left);
      if (rightresult == 0) timeouts++;
      else if (rightresult == 1) found++;
      else if (rightresult > 1)
         stack.push_back(right);
      printf("\nLeft: "); left.Print(); printf(":: %d", leftresult);
      printf("\nRight:"); right.Print(); printf(":: %d", rightresult);
      searches += 2;
      }
   printf("\nFound %d chips, %d searches required, %d timeouts\n", found, searches, timeouts);
   }


void WriteSdc()
   {
   const char* hierarchy[67] = {
   "f0_0___pipe_clkgen",
   "f1_1___pipe_clkgen",
   "f2_2___pipe_clkgen",
   "f3_3___pipe_clkgen",
   "f4_4___pipe_clkgen",
   "f5_5___pipe_clkgen",
   "f6_6___pipe_clkgen",
   "f7_7___pipe_clkgen",
   "f8_8___pipe_clkgen",
   "f9_9___pipe_clkgen",
   "f9_10___pipe_clkgen",
   "f9_11___pipe_clkgen",
   "f9_12___pipe_clkgen",
   "f9_13___pipe_clkgen",
   "f9_14___pipe_clkgen",
   "f9_15___pipe_clkgen",
   "f9_16___pipe_clkgen",
   "f9_17___pipe_clkgen",
   "f9_18___pipe_clkgen",
   "f9_19___pipe_clkgen",
   "f9_20___pipe_clkgen",
   "f9_21___pipe_clkgen",
   "f9_22___pipe_clkgen",
   "f9_23___pipe_clkgen",
   "f24_24___pipe_clkgen",
   "f25_25___pipe_clkgen",
   "f26_26___pipe_clkgen",
   "f27_27___pipe_clkgen",
   "f27_28___pipe_clkgen",
   "f27_29___pipe_clkgen",
   "f27_30___pipe_clkgen",
   "f31_31___pipe_clkgen",
   "f32_32___pipe_clkgen",
   "f33_33___pipe_clkgen",

   "s34_34___pipe_clkgen",
   "s35_35___pipe_clkgen",
   "s36_36___pipe_clkgen",
   "s37_37___pipe_clkgen",
   "s38_38___pipe_clkgen",
   "s39_39___pipe_clkgen",
   "s40_40___pipe_clkgen",
   "s41_41___pipe_clkgen",
   "s42_42___pipe_clkgen",
   "s42_43___pipe_clkgen",
   "s42_44___pipe_clkgen",
   "s42_45___pipe_clkgen",
   "s42_46___pipe_clkgen",
   "s42_47___pipe_clkgen",
   "s42_48___pipe_clkgen",
   "s42_49___pipe_clkgen",
   "s42_50___pipe_clkgen",
   "s42_51___pipe_clkgen",
   "s42_52___pipe_clkgen",
   "s42_53___pipe_clkgen",
   "s42_54___pipe_clkgen",
   "s42_55___pipe_clkgen",
   "s56_56___pipe_clkgen",
   "s57_57___pipe_clkgen",
   "s58_58___pipe_clkgen",
   "s59_59___pipe_clkgen",
   "s59_60___pipe_clkgen",
   "s59_61___pipe_clkgen",
   "s62_62___pipe_clkgen",
   "s63_63___pipe_clkgen",
   "s64_64___pipe_clkgen",
   "s65_65___pipe_clkgen",
   "s66_66___pipe_clkgen" };

   int p, i, e0, e1, e2;
   char filename[64];
//   for (p = 0; p < 8; p++)
   for (p = 4; p <8; p++)
      {
      const char* label[] = { "0(2p)","1(3p)", "2(4p)", "3(5p)", "4(4p)", "5(6p)", "6(8p)", "7(10p)" };
      sprintf(filename, "z:\\junk\\miner_harness_%s.sdc",label[p]);
      FILE* fptr = fopen(filename, "wt");
      int phase = p >= 4 ? (p-4 + 2) * 2 : (p + 2);
      
      for (i = 66; i >= 0; i--)
         {
         e0 = (-2 - i * 2 + phase * 256) % (phase * 2);
         e1 = e0 + 1;
         e2 = e1 + phase;
         fprintf(fptr, "\ncreate_generated_clock -name fast_clk_%-3d -edges {%2d %2d %2d}  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_mux_fast1/X}]", i * 2 + 1, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
         e0 = (-1 - i * 2 + phase * 256) % (phase * 2);
         e1 = e0 + 1;
         e2 = e1 + phase;
         fprintf(fptr, "\ncreate_generated_clock -name fast_clk_%-3d -edges {%2d %2d %2d}  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_mux_fast0/X}]", i * 2 + 0, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
         }
      fprintf(fptr, "\n");
      for (i = 66; i >= 0; i--)
         {
         e0 = (0 + phase - i * 2 + phase * 256) % (phase * 4);
         e1 = (e0 + 1) % (phase * 4);
         e2 = (e1 + phase * 2) % (phase * 4);
         fprintf(fptr, "\ncreate_generated_clock -name odd_slow_clk_%-3d -edges {%2d %2d %2d}  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_slow1/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
         }
      fprintf(fptr, "\n");
      for (i = 66; i >= 0; i--)
         {
         e0 = (0 - i * 2 + phase * 256) % (phase * 4);
         e1 = (e0 + 1) % (phase * 4);
         e2 = (e1 + phase * 2) % (phase * 4);
         fprintf(fptr, "\ncreate_generated_clock -name even_slow_clk_%-3d -edges {%2d %2d %2d}  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_slow0/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
         }
      fprintf(fptr, "\n");
      for (i = 66; i >= 0; i--)
         {
         e0 = (0 + phase - (i - 3) * 2 + phase * 256) % (phase * 4);
         e1 = (e0 + 1) % (phase * 4);
         e2 = (e1 + phase * 2) % (phase * 4);
         fprintf(fptr, "\ncreate_generated_clock -name odd_slow_minus3_clk_%-3d -edges {%2d %2d %2d}  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_minus1/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
         }
      fprintf(fptr, "\n");
      for (i = 66; i >= 0; i--)
         {
         e0 = (0 - (i - 3) * 2 + phase * 256) % (phase * 4);
         e1 = (e0 + 1) % (phase * 4);
         e2 = (e1 + phase * 2) % (phase * 4);
         fprintf(fptr, "\ncreate_generated_clock -name even_slow_minus3_clk_%-3d -edges {%2d %2d %2d}  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_minus0/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
         }
      fprintf(fptr, "\n");
      fclose(fptr);
      }
   }

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






