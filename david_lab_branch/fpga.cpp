#include "pch.h"
#include "helper.h"
#include "miner.h"
#include "dvfs.h"
#include "serial.h"
#include "usb.h"
#include "test.h"
#include "multithread.h"


void Fpga()
   {
   vector<topologytype> topology_fpga{ topologytype(0,0,0),topologytype(0,1,1)};
   testtype t(1, topology_fpga, systeminfotype());
   int i;
   headertype h;

   h.AsciiIn("000080208df76a159fa6df963fe397716e8a7cd73a0948c2ec9208000000000000000000938d701091a36a0bb6e663d4b30d776e4a642ad5d725c817b812d5da88fe83904f41f25f17220f17717d3900");


   t.SetBaudRate(11520);
//   t.SetBaud(1000000);
//   t.IsAlive(0);
   t.BoardEnumerate();
   t.WriteConfig(E_HASHCONFIG, 1<<16, -1);
   t.WriteConfig(E_HASHCONFIG, 0 << 16, -1);
   t.WriteConfig(E_HASHCONFIG, 0x1, -1);    // set 3 phase clocking
   t.WriteConfig(E_ENGINEMASK, 0xffffffbe, -1);
//   t.BistParedo();
   t.WriteConfig(E_HASHCONFIG, 1<<17, -1);    // enable small nonce
   t.WriteConfig(E_SMALL_NONCE, 1 << 12, -1);
   t.Pll(150);
   t.FrequencyEstimate();

   i = 0;
   while (true) {
      int bist_reg;
      t.WriteConfig(E_BIST_GOOD_SAMPLES, 0, -1);
      t.WriteConfig(E_BIST, 10, -1); // run bist, clearing failures
      for (int loops = 0; loops < 100; loops++) {
         bist_reg = t.ReadConfig(E_BIST, -1);
         if (bist_reg == 0) break;
         }
      bist_reg = t.ReadConfig(E_BIST_GOOD_SAMPLES, -1);
      if (bist_reg != 160) printf("Bist good samples=%.1f%%, i=%d\n", 100.0*bist_reg/160.0, i);
      if (i % 10000 == 0) printf("i=%d\n", i);
      i++;
      }


   t.MineTest();

   for (int phase=0; phase<8; phase++){
      t.WriteConfig(E_HASHCONFIG, (1 << 17) | phase, -1);
      printf("setting phase%d\n", phase);
      t.SingleTest(h);
      }
   return;
   t.MineTest();

   printf("Speed search for 100% -> %.2fMhz\n",t.SpeedSearch(60.0, 250.0, -1, 1.0));
   printf("Speed search for  90% -> %.2fMhz\n", t.SpeedSearch(60.0, 250.0, -1, 0.9));
   printf("Speed search for  50% -> %.2fMhz\n", t.SpeedSearch(60.0, 250.0, -1, 0.5));
   printf("Speed search for  10% -> %.2fMhz\n", t.SpeedSearch(60.0, 250.0, -1, 0.1));

   t.Pll(150);
   t.FrequencyEstimate();

   t.WriteConfig(E_HASHCONFIG, 0x0, -1);    // set 2 phase clocking
   t.WriteConfig(E_MINER_CONFIG, (0 << 16) | 1, -1);    // set 5 phase clocking
   t.WriteConfig(E_MINER_CONFIG, (6 << 16) | 2, -1);    // set 3 phase clocking
   t.MineTest();


   t.ReadHeaders("data\\block_headers.txt.gz");
   vector<headertype> pruned;
   vector<int> histogram(256, 0);
   for (i = 0; i < t.block_headers.size(); i++) {
      const headertype& h = t.block_headers[i];
      if (h.ReturnZeroes() < 40);
      else if (((h.x.nonce>>24) & 0xff) == 0)
         pruned.push_back(h);
      else if (((h.x.nonce >> 24) & 0xff) == 6)
         pruned.push_back(h);
//      if (pruned.size() > 1000) break;
      histogram[h.x.nonce >> 24]++;
      }
   for (i = 0; i < 256; i++)
      printf("blockchain nonce %3d = %5d %.1f%%\n",i,histogram[i],histogram[i]*100.0/t.block_headers.size());
   t.block_headers = pruned;
   printf("Found %d headers that are testable\n", pruned.size());
   for (int phase = 0; phase < 4; phase++) {
      vector<int> hits;
      t.WriteConfig(E_HASHCONFIG, phase + (1<<17), -1);
      t.WriteConfig(E_SMALL_NONCE, (1<<24)*(phase+2), -1);
      printf("Setting phase %d\n", phase + 1);
      t.HeaderTest(0, hits);
//      t.HeaderTest(1, hits);
//      t.HeaderTest(2, hits);
//      t.HeaderTest(3, hits);
      }
//   t.MineTest();
   }



void testtype::BistExperiment()
   {
   int i, bist_reg, loops, failures=0;
   
   Pll(100);
   WriteConfig(E_BIST_GOOD_SAMPLES, 0, -1);
   WriteConfig(E_HASHCONFIG, (1<<17) + 0x1, -1);    // set 3 phase clocking
   WriteConfig(E_SMALL_NONCE, 1<<28, -1);

   int iterations = 100;
   for (i = 0; i < 500; i++) {
      WriteConfig(E_BIST_GOOD_SAMPLES, 0, -1);
//      WriteConfig(E_BIST_RESULTS +0, 0, -1);
      WriteConfig(E_BIST, iterations, -1); // run bist, clearing failures
      for (loops = 0; loops < 100; loops++) {
         bist_reg = ReadConfig(E_BIST, -1);
         if (bist_reg == 0) break;
         }
      bist_reg = ReadConfig(E_BIST_GOOD_SAMPLES, -1);
      float hitrate = (float)bist_reg / (iterations * 8 * 2);
      if (hitrate != 1) {
         printf("i=%d Hit rate=%.1f%%\n", i, hitrate * 100.0);
         failures++;
         }
      }
   printf("failures = %d\n", failures);
   }