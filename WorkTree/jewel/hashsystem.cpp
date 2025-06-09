#include "pch.h"
#include "pch.h"
#include "helper.h"
#include "miner.h"
#include "dvfs.h"
#include "serial.h"
#include "usb.h"
#include "test.h"
#include "characterization.h"
#include "multithread.h"



void CreateTopology();


float testtype::MinMaxTemp()
   {
   float min = 1000, max = -1000;
   int i;
   for (i = 0; i < found.size(); i++)
      {
      float odt = OnDieTemp(found[i]);
      min = MINIMUM(min, odt);
      max = MAXIMUM(max, odt);
      }
   printf("Min temp is %.1fC, max is %.1fC\n", min, max);
   return max;
   }




struct systemstattype {
   float temp, voltage;
   int hits[256];
   int true_hits;
   vector< uint32> repeats;
   systemstattype()
      {
      temp = voltage = 0.0;
      true_hits = 0;
      for (int i = 0; i < 256; i++) hits[i] = 0;
      }
   };

void testtype::SystemStatistics(const char *label)
   {
   vector<batchtype> batch;
   response_hittype hitpacket;
   vector<systemstattype> ss(132);
   int i, k;
   time_t start_time, end_time;
   const int period = 60*60;

   WriteConfig(E_HIT0, 0);
   WriteConfig(E_HIT1, 0);
   WriteConfig(E_HIT2, 0);
   WriteConfig(E_HIT0, 0);
   WriteConfig(E_HIT1, 0);
   WriteConfig(E_HIT2, 0);
   WriteConfig(E_GENERAL_TRUEHIT_COUNT, 0);
   WriteConfig(E_DIFFICULT_HIT_COUNT, 0);
   WriteConfig(E_DROPPED_HIT_COUNT, 0);
   WriteConfig(E_DROPPED_DIFFICULT_COUNT, 0);
   time(&start_time);

   for (i = 0; i < found.size(); i++) {
      batch.push_back(batchtype().Read(0, found[i], E_GENERAL_TRUEHIT_COUNT));
      batch.push_back(batchtype().Read(0, found[i], E_SUMMARY));
      }
   while (true)
      {
      time(&end_time);
      if (end_time - start_time > period) break;

      BatchConfig(batch);
      for (i = 0; i < found.size(); i++) {
         if (batch[i * 2 + 1].data & 0xf)
            {
            if (!GetHit(hitpacket, found[i])) {
               printf("Hit problem\n");
               }
            if (hitpacket.id != found[i])
               FATAL_ERROR;
            uint32 nonce = hitpacket.header.x.nonce;
            ss[i].hits[nonce &0xff]++;
            for (k = ss[i].repeats.size() - 1; k >= 0; k--)
               if (ss[i].repeats[k] == nonce) break;
            if (k >= 0) {
               printf("I found a nonce repeat\n");
               }
            ss[i].repeats.push_back(nonce);
            ss[i].true_hits = batch[i * 2 + 0].data;
            }
         }
      }
   FILE* fptr = fopen("data/systemstat.csv", "at");
   fprintf(fptr, "%s", label);
   for (i = 0; i < found.size(); i++)
      fprintf(fptr, ",%d", ss[i].true_hits);
   fprintf(fptr, "\n");
   fprintf(fptr, "%s_sum", label);
   for (i = 0; i < found.size(); i++) {
      int total = 0;
      for (k = 0; k < 256; k++)
         total += ss[i].hits[k];
      fprintf(fptr, ",%d", total);
      }
   fprintf(fptr, "\n");
   fprintf(fptr, "%s_difficult", label);
   for (i = 0; i < found.size(); i++) {
      fprintf(fptr, ",%d", ReadConfig(E_DIFFICULT_HIT_COUNT,found[i]));
      }
   fprintf(fptr, "\n");
   fprintf(fptr, "%s_difficultdrop", label);
   for (i = 0; i < found.size(); i++) {
      fprintf(fptr, ",%d", ReadConfig(E_DROPPED_DIFFICULT_COUNT, found[i]));
      }
   fprintf(fptr, "\n");
   fclose(fptr);
   fprintf(fptr, "%s_drop", label);
   for (i = 0; i < found.size(); i++) {
      fprintf(fptr, ",%d", ReadConfig(E_DROPPED_HIT_COUNT, found[i]));
      }
   fprintf(fptr, "\n");
   fclose(fptr);

   fptr = fopen("data/systemstatminer.csv", "at");
   fprintf(fptr, "%s\n",label);
   for (i = 0; i < found.size(); i++) {
      fprintf(fptr, "%d,%.1f,%1.f,%d", i, OnDieTemp(found[i]), OnDieVoltage(found[i]) * 1000.0, ss[i].true_hits);
      for (k = 0; k < 256; k++)
         fprintf(fptr, ",%d", ss[i].hits[k]);
      fprintf(fptr, "\n");
      }

   fclose(fptr);
   }

void testtype::CommunicationTest()
   {
   int i, k;
   vector<batchtype> batch;
   int seed = 0;
   uint32 data;

   printf("Testing communications\n");
   seed = 0;
   for (i = 0; i < topology.size(); i++)
      {
      topologytype& t = topology[i];
      for (k = 0; k < 7; k++) {
	 data = (__int64)(random(seed) * 1.0e10);
	 batch.push_back(batchtype().Write(t.board, t.id, E_ENGINEMASK + k, data));
	 }
      batch.push_back(batchtype().Read(t.board, t.id, E_UNIQUE));
      ReadWriteConfig(batch);
      batch.clear();
      }
   seed = 0;
   for (i = 0; i < topology.size(); i++)
      {
      topologytype& t = topology[i];
      for (k = 0; k < 7; k++) {
	 batch.push_back(batchtype().Read(t.board, t.id, E_ENGINEMASK + k));
	 }
      batch.push_back(batchtype().Read(t.board, t.id, E_UNIQUE));
      batch.push_back(batchtype().Read(t.board, t.id, E_ASICID));
      ReadWriteConfig(batch);
      for (k = 0; k < 7; k++) {
	 data = (__int64)(random(seed) * 1.0e10);
	 if (batch[k].data != data)
	    printf("I saw a data mismatch %d:%d on ENGINEMASK%d (%x!=%x)\n",t.board,t.id,k,batch[k].data,data);
	 }
      if (batch[7].data != 0x61727541)
	 printf("I saw a data mismatch %d:%d on Unique (%x!=%x)\n", t.board, t.id, batch[7].data, 0x61727541);
      if (batch[8].data != t.id)
	 printf("I saw a data mismatch %d:%d on ID (%x!=%x)\n", t.board, t.id, batch[8].data, t.id);
      batch.clear();
      }

   for (i = 0; i < topology.size(); i++)
      {
      topologytype& t = topology[i];
      for (k = 0; k < 7; k++) {
	 batch.push_back(batchtype().Write(t.board, t.id, E_ENGINEMASK + k, 0));
	 }
      }
   for (i=0; i<boards.size(); i++)
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE));
   ReadWriteConfig(batch);
   batch.clear();

   printf("\n");
   }


void testtype::PllExperiment(int b)
   {
   float v = 14.0;
   int vcosel,divider;
   boardinfotype info;

   
   if (!IsAlive(0)) printf("Aliveness failure"); 
   usb.SetFanSpeed(150);
   if (!IsAlive(0)) printf("Aliveness failure"); 
   SetVoltage(14.0);
   SetBoard(b);
   if (!IsAlive(0)) printf("Aliveness failure");

   for (divider = 8; divider >= 1; divider--)
      {
      for (vcosel = 0; vcosel < 4; vcosel++)
         {
         pll_multiplier = divider / 25.0 * (1 << 20);

/*         gpio.reset = 0;
         usb.ReadWriteGPIO(gpio);
         Delay(200);
         gpio.reset = 1;
         usb.ReadWriteGPIO(gpio);
         Delay(100);
         usb.SetBaudRate(115200);*/
         if (!IsAlive(0)) {
            printf("Aliveness failure");
            return;
            }

         WriteConfig(E_PLLFREQ, pll_multiplier*1000.0, -1);
         WriteConfig(E_PLLCONFIG, 0x12, -1);
         WriteConfig(E_PLLCONFIG, 0x1d + (0 << 13) + ((divider-1) << 10) + (vcosel << 8), -1);
         usb.VddEnable(1<<b);
         Delay(1000);
         BoardEnumerate();
         if (found.size() != 132) {
            printf("I lost communication, quitting\n");
            usb.VddEnable(0x0);
            exit(-1);
            }
         Delay(2000);
         FrequencyEstimate(1000.0);
         Delay(5000);
         usb.BoardInfo(info, true);
         printf("vcosel=%d divider=%d power=%.1fW\n",vcosel,divider,info.supply_pin);
         usb.VddEnable(0x0);
         }
      }
   }


void testtype::IRTest(int b)
   {
   float v=14.0, ir, f;
   float f_start = 200, f_end = 1600, f_incr = 200, ps_efficiency=0.95;
   int i,count;
   boardinfotype info;
   vector<float> irs, currents;

   usb.SetFanSpeed(255);
   SetVoltage(14.0);
   SetBoard(b);

   for (f = f_start; f <= f_end; f += f_incr)
      {
      Pll(f, -1);
      Delay(5000);
      usb.BoardInfo(info,true);
      ir = 0.0;
      count = 0;
      for (i = 0; i < topology.size(); i++)
         {
         topologytype& t = topology[i];
         if (t.board == b)
            {
            ir += t.Voltage(ReadConfig(E_VOLTAGE, t.id));
            count++;
            }
         }

      ir /= 3.0;
      ir = v - ir;
      float current = info.supply_pin / v* ps_efficiency;
      float r1 = 0, r2 = 0, offset1 = 0, offset2 = 0;
      if (irs.size()) {
         r1 = (ir - irs[0]) / (current - currents[0]);
         offset1 = r1 * current - ir;
         r2 = (ir - irs.back()) / (current - currents.back());
         offset2 = r1 * current - ir;
         }
      irs.push_back(ir);
      currents.push_back(current);

      printf("F=%.0fMhz, Power=%.0fW Current=%.1fA IR=%.1fmV R1=%.2f mOhm+%.2f mV R2=%.2f mOhm+%.2f mV\n", f, info.supply_pin, info.supply_pin / v, ir * 1000.0,r1*1000.0,offset1 * 1000.0,r2 * 1000.0,offset2 * 1000.0);
      }
   PrintSystem();
   }

void testtype::PerformanceSweep(int board_mask, const char *label, float experiment)
   {
   vector<topologytype> topology_1board    { topologytype(0, 0, 0), topologytype(0, 1, 1), topologytype(0, 2, 2), topologytype(1, 0, 128), topologytype(1, 1, 129), topologytype(1, 2, 130), topologytype(2, 0, 3), topologytype(2, 1, 4), topologytype(2, 2, 5), topologytype(3, 0, 131), topologytype(3, 1, 132), topologytype(3, 2, 133), topologytype(4, 0, 6), topologytype(4, 1, 7), topologytype(4, 2, 8), topologytype(5, 0, 134), topologytype(5, 1, 135), topologytype(5, 2, 136), topologytype(6, 0, 9), topologytype(6, 1, 10), topologytype(6, 2, 11), topologytype(7, 0, 137), topologytype(7, 1, 138), topologytype(7, 2, 139), topologytype(8, 0, 12), topologytype(8, 1, 13), topologytype(8, 2, 14), topologytype(9, 0, 140), topologytype(9, 1, 141), topologytype(9, 2, 142), topologytype(10, 0, 15), topologytype(10, 1, 16), topologytype(10, 2, 17), topologytype(11, 0, 143), topologytype(11, 1, 144), topologytype(11, 2, 145), topologytype(12, 0, 18), topologytype(12, 1, 19), topologytype(12, 2, 20), topologytype(13, 0, 146), topologytype(13, 1, 147), topologytype(13, 2, 148), topologytype(14, 0, 21), topologytype(14, 1, 22), topologytype(14, 2, 23), topologytype(15, 0, 149), topologytype(15, 1, 150), topologytype(15, 2, 151), topologytype(16, 0, 24), topologytype(16, 1, 25), topologytype(16, 2, 26), topologytype(17, 0, 152), topologytype(17, 1, 153), topologytype(17, 2, 154), topologytype(18, 0, 27), topologytype(18, 1, 28), topologytype(18, 2, 29), topologytype(19, 0, 155), topologytype(19, 1, 156), topologytype(19, 2, 157), topologytype(20, 0, 30), topologytype(20, 1, 31), topologytype(20, 2, 32), topologytype(21, 0, 158), topologytype(21, 1, 159), topologytype(21, 2, 160), topologytype(22, 0, 33), topologytype(22, 1, 34), topologytype(22, 2, 35), topologytype(23, 0, 161), topologytype(23, 1, 162), topologytype(23, 2, 163), topologytype(24, 0, 36), topologytype(24, 1, 37), topologytype(24, 2, 38), topologytype(25, 0, 164), topologytype(25, 1, 165), topologytype(25, 2, 166), topologytype(26, 0, 39), topologytype(26, 1, 40), topologytype(26, 2, 41), topologytype(27, 0, 167), topologytype(27, 1, 168), topologytype(27, 2, 169), topologytype(28, 0, 42), topologytype(28, 1, 43), topologytype(28, 2, 44), topologytype(29, 0, 170), topologytype(29, 1, 171), topologytype(29, 2, 172), topologytype(30, 0, 45), topologytype(30, 1, 46), topologytype(30, 2, 47), topologytype(31, 0, 173), topologytype(31, 1, 174), topologytype(31, 2, 175), topologytype(32, 0, 48), topologytype(32, 1, 49), topologytype(32, 2, 50), topologytype(33, 0, 176), topologytype(33, 1, 177), topologytype(33, 2, 178), topologytype(34, 0, 51), topologytype(34, 1, 52), topologytype(34, 2, 53), topologytype(35, 0, 179), topologytype(35, 1, 180), topologytype(35, 2, 181), topologytype(36, 0, 54), topologytype(36, 1, 55), topologytype(36, 2, 56), topologytype(37, 0, 182), topologytype(37, 1, 183), topologytype(37, 2, 184), topologytype(38, 0, 57), topologytype(38, 1, 58), topologytype(38, 2, 59), topologytype(39, 0, 185), topologytype(39, 1, 186), topologytype(39, 2, 187), topologytype(40, 0, 60), topologytype(40, 1, 61), topologytype(40, 2, 62), topologytype(41, 0, 188), topologytype(41, 1, 189), topologytype(41, 2, 190), topologytype(42, 0, 63), topologytype(42, 1, 64), topologytype(42, 2, 65), topologytype(43, 0, 191), topologytype(43, 1, 192), topologytype(43, 2, 193) };
   vector<topologytype> topology_1board_150{ topologytype(0, 0, 0), topologytype(0, 1, 1), topologytype(0, 2, 2), topologytype(1, 0, 128), topologytype(1, 1, 129), topologytype(1, 2, 130), topologytype(2, 0, 3), topologytype(2, 1, 4), topologytype(2, 2, 5), topologytype(3, 0, 131), topologytype(3, 1, 132), topologytype(3, 2, 133), topologytype(4, 0, 6), topologytype(4, 1, 7), topologytype(4, 2, 8), topologytype(5, 0, 134), topologytype(5, 1, 135), topologytype(5, 2, 136), topologytype(6, 0, 9), topologytype(6, 1, 10), topologytype(6, 2, 11), topologytype(7, 0, 137), topologytype(7, 1, 138), topologytype(7, 2, 139), topologytype(8, 0, 12), topologytype(8, 1, 13), topologytype(8, 2, 14), topologytype(9, 0, 140), topologytype(9, 1, 141), topologytype(9, 2, 142), topologytype(10, 0, 15), topologytype(10, 1, 16), topologytype(10, 2, 17), topologytype(11, 0, 143), topologytype(11, 1, 144), topologytype(11, 2, 145), topologytype(12, 0, 18), topologytype(12, 1, 19), topologytype(12, 2, 20), topologytype(13, 0, 146), topologytype(13, 1, 147), topologytype(13, 2, 148), topologytype(14, 0, 21), topologytype(14, 1, 22), topologytype(14, 2, 23), topologytype(15, 0, 149), topologytype(15, 1, 150), topologytype(15, 2, 151), topologytype(16, 0, 24), topologytype(16, 1, 25), topologytype(16, 2, 26), topologytype(17, 0, 152), topologytype(17, 1, 153), topologytype(17, 2, 154), topologytype(18, 0, 27), topologytype(18, 1, 28), topologytype(18, 2, 29), topologytype(19, 0, 155), topologytype(19, 1, 156), topologytype(19, 2, 157), topologytype(20, 0, 30), topologytype(20, 1, 31), topologytype(20, 2, 32), topologytype(21, 0, 158), topologytype(21, 1, 159), topologytype(21, 2, 160), topologytype(22, 0, 33), topologytype(22, 1, 34), topologytype(22, 2, 35), topologytype(23, 0, 161), topologytype(23, 1, 162), topologytype(23, 2, 163), topologytype(24, 0, 36), topologytype(24, 1, 37), topologytype(24, 2, 38), topologytype(25, 0, 164), topologytype(25, 1, 165), topologytype(25, 2, 166), topologytype(26, 0, 39), topologytype(26, 1, 40), topologytype(26, 2, 41), topologytype(27, 0, 167), topologytype(27, 1, 168), topologytype(27, 2, 169), topologytype(28, 0, 42), topologytype(28, 1, 43), topologytype(28, 2, 44), topologytype(29, 0, 170), topologytype(29, 1, 171), topologytype(29, 2, 172), topologytype(30, 0, 45), topologytype(30, 1, 46), topologytype(30, 2, 47), topologytype(31, 0, 173), topologytype(31, 1, 174), topologytype(31, 2, 175), topologytype(32, 0, 48), topologytype(32, 1, 49), topologytype(32, 2, 50), topologytype(33, 0, 176), topologytype(33, 1, 177), topologytype(33, 2, 178), topologytype(34, 0, 51), topologytype(34, 1, 52), topologytype(34, 2, 53), topologytype(35, 0, 179), topologytype(35, 1, 180), topologytype(35, 2, 181), topologytype(36, 0, 54), topologytype(36, 1, 55), topologytype(36, 2, 56), topologytype(37, 0, 182), topologytype(37, 1, 183), topologytype(37, 2, 184), topologytype(38, 0, 57), topologytype(38, 1, 58), topologytype(38, 2, 59), topologytype(39, 0, 185), topologytype(39, 1, 186), topologytype(39, 2, 187), topologytype(40, 0, 60), topologytype(40, 1, 61), topologytype(40, 2, 62), topologytype(41, 0, 188), topologytype(41, 1, 189), topologytype(41, 2, 190), topologytype(42, 0, 63), topologytype(42, 1, 64), topologytype(42, 2, 65), topologytype(43, 0, 191), topologytype(43, 1, 192), topologytype(43, 2, 193) , topologytype(44, 0, 66), topologytype(44, 1, 67), topologytype(44, 2, 68), topologytype(45, 0, 194), topologytype(45, 1, 195), topologytype(45, 2, 196), topologytype(46, 0, 69), topologytype(46, 1, 70), topologytype(46, 2, 71), topologytype(47, 0, 197), topologytype(47, 1, 198), topologytype(47, 2, 199), topologytype(48, 0, 72), topologytype(48, 1, 73), topologytype(48, 2, 74), topologytype(49, 0, 200), topologytype(49, 1, 201), topologytype(49, 2, 202) };
   vector<topologytype> topology_system;
   systeminfotype systeminfo;
   headertype h;
   int i, k;
   bool too_hot;

   for (k = 0; k < 3; k++) {
      if ((board_mask >> k) & 1) {
         for (i = 0; i < (zareen ? topology_1board_150.size() : topology_1board.size()); i++)
            {
            topology_system.push_back(zareen ? topology_1board_150[i] : topology_1board[i]);
            topology_system.back().board = k;
            }
         }
      }
   gpiotype gpio;
   gpio.reset = 0;
   usb.ReadWriteGPIO(gpio);
   Delay(500);
   gpio.reset = 1;
   usb.ReadWriteGPIO(gpio);
   Delay(500);
   gpio.reset = 0;
   usb.ReadWriteGPIO(gpio);
   Delay(500);
   gpio.reset = 1;
   usb.ReadWriteGPIO(gpio);
   Delay(500);
   SetBaudRate(115200);
   Delay(200);

   systeminfo.optimal_temp = 50.0;
   DescribeSystem(topology_system, systeminfo);
   for (k = 0; k < 3; k++) {
      if ((board_mask >> k) & 1) {
         printf("Board %d: ", k);
         SetBoard(k);
         SetBaudRate(115200);
//         SetBaud(1000000);
         SetBaud(500000);
         BoardEnumerate();
         if (found.size() != 132 && found.size() != 150)
            printf("This board has some missing chips(%d/132)\n", found.size());
         found.clear(); // zero this so we don't get confused with multiple boards
         Pll(25, -1);
         }
      }

   vector<batchtype> batch;
   for (i = 0; i < topology.size(); i++)
      {
      topologytype& t = topology[i];
      batch.push_back(batchtype().Write(t.board, t.id, E_VERSION_BOUND, i * 100 + ((i * 100 + 99) << 16)));
      batch.push_back(batchtype().Write(t.board, t.id, E_HASHCONFIG, (0 << 8) | (1 << 9)));
      }
   ReadWriteConfig(batch);
   batch.clear();
   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   Load(h, 0, 38, 0);

   InitialSetup();
   PrintSystem();

   plottype plots;
   float divider = 396.0 / topology.size();
//   for (float hash = 60; hash <= 180; hash += (hash==140 ? 10 : hash == 150 ? 10 : 20)) {
   for (float hash = 100; hash <= 280; hash += 20) {
      float hitrate, efficiency, avg_temp, max_temp;
      char buffer[256];
      sprintf(buffer, "data/%s_dvfs.txt", label);
      FILE* fptr = fopen(buffer, "at");

      plots.AddData("Throughput", hash / divider);
/*
      DVFS(hash / divider, false, 0, true);
      hitrate = MeasureHashRate(30 * 1000, too_hot);
      usb.BoardInfo(info, true);
      efficiency = info.supply_pin / (hash * hitrate / divider);
      sprintf(buffer, "D_%s: ", label);
      PrintSummary(fptr, buffer, efficiency);
      PrintSystem(fptr);
      plots.AddData(buffer, efficiency);

      systeminfo.optimal_temp = 50;
      DVFS(hash / divider, true, 1, true);
      hitrate = MeasureHashRate(30 * 1000, too_hot);
      usb.BoardInfo(info, true);
      efficiency = info.supply_pin / (hash * hitrate / divider);
      sprintf(buffer, "DECO_%.0f_%s: ", systeminfo.optimal_temp, label);
      PrintSummary(fptr, buffer, efficiency);
      PrintSystem(fptr);
      plots.AddData(buffer, efficiency);
*/

/*      DVFS(hash / divider, false, true);
      hitrate = MeasureHashRate(30 * 1000, too_hot, avg_temp, max_temp);
      sprintf(buffer, "D_%.0f_%s: ", systeminfo.optimal_temp, label);
      PrintSummary(fptr, buffer, efficiency);
      PrintSystem(fptr);
      plots.AddData(buffer, efficiency);
*/

//      experiment = 2;
      for (int x=experiment; x >= 0; x-=2) {
         DVFS(hash / divider, x, true);
         hitrate = MeasureHashRate(20 * 1000, too_hot, avg_temp, max_temp);
         sprintf(buffer, "DECO_%d_%s: ", x, label);
         PrintSystem();
         PrintSystem(fptr);
         PrintSummary(fptr, buffer, efficiency);
         plots.AddData(buffer, efficiency);
         }

      if (fptr!=NULL) fclose(fptr);
      }
   plots.AppendToFile("shmoo/dvfs.csv");
   MinPower();
   Delay(1000);
   }

void testtype::ReadAllVoltages(const topologytype& t)
   {
   int k;
   int channel;
   vector<batchtype> batch;

   for (channel = 0; channel < 16; channel++)
      {
      if (systeminfo.refclk == 25.0)
	 batch.push_back(batchtype().Write(t.board, t.id, E_IPCFG, 0x1)); // ipclk is 6.25Mhz
      else FATAL_ERROR;
      batch.push_back(batchtype().Write(t.board, t.id, E_DVMCFG, 0x2));
      batch.push_back(batchtype().Write(t.board, t.id, E_DVMCFG, 0x3));
      batch.push_back(batchtype().Write(t.board, t.id, E_DVMCFG, 0x1));
      batch.push_back(batchtype().Write(t.board, t.id, E_DVMCFG, 0x9 | (channel << 12)));
      batch.push_back(batchtype().Write(t.board, t.id, E_DVMCFG, 0x5));
      batch.push_back(batchtype().Read(t.board, t.id, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels

      ReadWriteConfig(batch);
      batch.clear();
      batch.push_back(batchtype().Read(t.board, t.id, E_VOLTAGE));
      float avg = 0.0;
      for (k = 0; k < 3; k++) {
	 Delay(100);
	 ReadWriteConfig(batch);
	 avg += t.Voltage(batch[0].data) / 3.0;
	 }
      printf("%d:%d channel %d = %.1fmV\n", t.board, t.id, channel, avg * 1000.0);
      batch.clear();
      }

   channel = 5; // return to core voltage channel
   batch.push_back(batchtype().Write(t.board, t.id, E_DVMCFG, 0x2));
   batch.push_back(batchtype().Write(t.board, t.id, E_DVMCFG, 0x3));
   batch.push_back(batchtype().Write(t.board, t.id, E_DVMCFG, 0x1));
   batch.push_back(batchtype().Write(t.board, t.id, E_DVMCFG, 0x9 | (channel << 12)));
   batch.push_back(batchtype().Write(t.board, t.id, E_DVMCFG, 0x5));
   batch.push_back(batchtype().Read(t.board, t.id, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
   ReadWriteConfig(batch);
   }



void testtype::HashSystem()
   {
   vector<topologytype> topology_1board{ topologytype(0, 0, 0), topologytype(0, 1, 1), topologytype(0, 2, 2), topologytype(1, 0, 128), topologytype(1, 1, 129), topologytype(1, 2, 130), topologytype(2, 0, 3), topologytype(2, 1, 4), topologytype(2, 2, 5), topologytype(3, 0, 131), topologytype(3, 1, 132), topologytype(3, 2, 133), topologytype(4, 0, 6), topologytype(4, 1, 7), topologytype(4, 2, 8), topologytype(5, 0, 134), topologytype(5, 1, 135), topologytype(5, 2, 136), topologytype(6, 0, 9), topologytype(6, 1, 10), topologytype(6, 2, 11), topologytype(7, 0, 137), topologytype(7, 1, 138), topologytype(7, 2, 139), topologytype(8, 0, 12), topologytype(8, 1, 13), topologytype(8, 2, 14), topologytype(9, 0, 140), topologytype(9, 1, 141), topologytype(9, 2, 142), topologytype(10, 0, 15), topologytype(10, 1, 16), topologytype(10, 2, 17), topologytype(11, 0, 143), topologytype(11, 1, 144), topologytype(11, 2, 145), topologytype(12, 0, 18), topologytype(12, 1, 19), topologytype(12, 2, 20), topologytype(13, 0, 146), topologytype(13, 1, 147), topologytype(13, 2, 148), topologytype(14, 0, 21), topologytype(14, 1, 22), topologytype(14, 2, 23), topologytype(15, 0, 149), topologytype(15, 1, 150), topologytype(15, 2, 151), topologytype(16, 0, 24), topologytype(16, 1, 25), topologytype(16, 2, 26), topologytype(17, 0, 152), topologytype(17, 1, 153), topologytype(17, 2, 154), topologytype(18, 0, 27), topologytype(18, 1, 28), topologytype(18, 2, 29), topologytype(19, 0, 155), topologytype(19, 1, 156), topologytype(19, 2, 157), topologytype(20, 0, 30), topologytype(20, 1, 31), topologytype(20, 2, 32), topologytype(21, 0, 158), topologytype(21, 1, 159), topologytype(21, 2, 160), topologytype(22, 0, 33), topologytype(22, 1, 34), topologytype(22, 2, 35), topologytype(23, 0, 161), topologytype(23, 1, 162), topologytype(23, 2, 163), topologytype(24, 0, 36), topologytype(24, 1, 37), topologytype(24, 2, 38), topologytype(25, 0, 164), topologytype(25, 1, 165), topologytype(25, 2, 166), topologytype(26, 0, 39), topologytype(26, 1, 40), topologytype(26, 2, 41), topologytype(27, 0, 167), topologytype(27, 1, 168), topologytype(27, 2, 169), topologytype(28, 0, 42), topologytype(28, 1, 43), topologytype(28, 2, 44), topologytype(29, 0, 170), topologytype(29, 1, 171), topologytype(29, 2, 172), topologytype(30, 0, 45), topologytype(30, 1, 46), topologytype(30, 2, 47), topologytype(31, 0, 173), topologytype(31, 1, 174), topologytype(31, 2, 175), topologytype(32, 0, 48), topologytype(32, 1, 49), topologytype(32, 2, 50), topologytype(33, 0, 176), topologytype(33, 1, 177), topologytype(33, 2, 178), topologytype(34, 0, 51), topologytype(34, 1, 52), topologytype(34, 2, 53), topologytype(35, 0, 179), topologytype(35, 1, 180), topologytype(35, 2, 181), topologytype(36, 0, 54), topologytype(36, 1, 55), topologytype(36, 2, 56), topologytype(37, 0, 182), topologytype(37, 1, 183), topologytype(37, 2, 184), topologytype(38, 0, 57), topologytype(38, 1, 58), topologytype(38, 2, 59), topologytype(39, 0, 185), topologytype(39, 1, 186), topologytype(39, 2, 187), topologytype(40, 0, 60), topologytype(40, 1, 61), topologytype(40, 2, 62), topologytype(41, 0, 188), topologytype(41, 1, 189), topologytype(41, 2, 190), topologytype(42, 0, 63), topologytype(42, 1, 64), topologytype(42, 2, 65), topologytype(43, 0, 191), topologytype(43, 1, 192), topologytype(43, 2, 193) };
   vector<topologytype> topology_1board_150{ topologytype(0, 0, 0), topologytype(0, 1, 1), topologytype(0, 2, 2), topologytype(1, 0, 128), topologytype(1, 1, 129), topologytype(1, 2, 130), topologytype(2, 0, 3), topologytype(2, 1, 4), topologytype(2, 2, 5), topologytype(3, 0, 131), topologytype(3, 1, 132), topologytype(3, 2, 133), topologytype(4, 0, 6), topologytype(4, 1, 7), topologytype(4, 2, 8), topologytype(5, 0, 134), topologytype(5, 1, 135), topologytype(5, 2, 136), topologytype(6, 0, 9), topologytype(6, 1, 10), topologytype(6, 2, 11), topologytype(7, 0, 137), topologytype(7, 1, 138), topologytype(7, 2, 139), topologytype(8, 0, 12), topologytype(8, 1, 13), topologytype(8, 2, 14), topologytype(9, 0, 140), topologytype(9, 1, 141), topologytype(9, 2, 142), topologytype(10, 0, 15), topologytype(10, 1, 16), topologytype(10, 2, 17), topologytype(11, 0, 143), topologytype(11, 1, 144), topologytype(11, 2, 145), topologytype(12, 0, 18), topologytype(12, 1, 19), topologytype(12, 2, 20), topologytype(13, 0, 146), topologytype(13, 1, 147), topologytype(13, 2, 148), topologytype(14, 0, 21), topologytype(14, 1, 22), topologytype(14, 2, 23), topologytype(15, 0, 149), topologytype(15, 1, 150), topologytype(15, 2, 151), topologytype(16, 0, 24), topologytype(16, 1, 25), topologytype(16, 2, 26), topologytype(17, 0, 152), topologytype(17, 1, 153), topologytype(17, 2, 154), topologytype(18, 0, 27), topologytype(18, 1, 28), topologytype(18, 2, 29), topologytype(19, 0, 155), topologytype(19, 1, 156), topologytype(19, 2, 157), topologytype(20, 0, 30), topologytype(20, 1, 31), topologytype(20, 2, 32), topologytype(21, 0, 158), topologytype(21, 1, 159), topologytype(21, 2, 160), topologytype(22, 0, 33), topologytype(22, 1, 34), topologytype(22, 2, 35), topologytype(23, 0, 161), topologytype(23, 1, 162), topologytype(23, 2, 163), topologytype(24, 0, 36), topologytype(24, 1, 37), topologytype(24, 2, 38), topologytype(25, 0, 164), topologytype(25, 1, 165), topologytype(25, 2, 166), topologytype(26, 0, 39), topologytype(26, 1, 40), topologytype(26, 2, 41), topologytype(27, 0, 167), topologytype(27, 1, 168), topologytype(27, 2, 169), topologytype(28, 0, 42), topologytype(28, 1, 43), topologytype(28, 2, 44), topologytype(29, 0, 170), topologytype(29, 1, 171), topologytype(29, 2, 172), topologytype(30, 0, 45), topologytype(30, 1, 46), topologytype(30, 2, 47), topologytype(31, 0, 173), topologytype(31, 1, 174), topologytype(31, 2, 175), topologytype(32, 0, 48), topologytype(32, 1, 49), topologytype(32, 2, 50), topologytype(33, 0, 176), topologytype(33, 1, 177), topologytype(33, 2, 178), topologytype(34, 0, 51), topologytype(34, 1, 52), topologytype(34, 2, 53), topologytype(35, 0, 179), topologytype(35, 1, 180), topologytype(35, 2, 181), topologytype(36, 0, 54), topologytype(36, 1, 55), topologytype(36, 2, 56), topologytype(37, 0, 182), topologytype(37, 1, 183), topologytype(37, 2, 184), topologytype(38, 0, 57), topologytype(38, 1, 58), topologytype(38, 2, 59), topologytype(39, 0, 185), topologytype(39, 1, 186), topologytype(39, 2, 187), topologytype(40, 0, 60), topologytype(40, 1, 61), topologytype(40, 2, 62), topologytype(41, 0, 188), topologytype(41, 1, 189), topologytype(41, 2, 190), topologytype(42, 0, 63), topologytype(42, 1, 64), topologytype(42, 2, 65), topologytype(43, 0, 191), topologytype(43, 1, 192), topologytype(43, 2, 193) , topologytype(44, 0, 66), topologytype(44, 1, 67), topologytype(44, 2, 68), topologytype(45, 0, 194), topologytype(45, 1, 195), topologytype(45, 2, 196), topologytype(46, 0, 69), topologytype(46, 1, 70), topologytype(46, 2, 71), topologytype(47, 0, 197), topologytype(47, 1, 198), topologytype(47, 2, 199), topologytype(48, 0, 72), topologytype(48, 1, 73), topologytype(48, 2, 74), topologytype(49, 0, 200), topologytype(49, 1, 201), topologytype(49, 2, 202) };
   vector<topologytype> topology_system;
   vector<int> boards;
   vector<char*> board_labels;
   headertype h;
   boardinfotype info;
   gpiotype gpio;   gpio.clear();
   int i, k;
   char buffer[256];

   usb.SetImmersion(true);
   SetVoltage(0.0);
   gpio.reset = 0;
   usb.ReadWriteGPIO(gpio);
   Delay(1000);
   usb.SetFanSpeed(255);
//   Delay(2000);
//   printf("Fans at 100%, power=%.1fW\n", ReadPower());

   usb.VddEnable(0x0);
   SetVoltage(12.0);
   Delay(1000);
   usb.BoardInfo(info, true);
   printf("FW is reporting boards[%c%c%c] present\n", info.boards_present & 1 ? '1' : ' ', info.boards_present & 2 ? '2' : ' ', info.boards_present & 4 ? '3' : ' ');

   if (false) {
      SetVoltage(12.0);
      usb.VddEnable(0x7);
      Delay(1000);
      }

   gpio.reset = 1;
   usb.ReadWriteGPIO(gpio);
   Delay(400);
   gpio.reset = 0;
   usb.ReadWriteGPIO(gpio);
   Delay(400);
   gpio.reset = 1;
   usb.ReadWriteGPIO(gpio);
   Delay(400);
   usb.OLED_Display("System Testing\n#");
   
   for (k = 0; k < 3; k++) {
      SetBoard(k);
      usb.SetBaudRate(115200);
      SetBaud(1000000);
      found.clear();
      if (IsAlive(0)) {
         printf("I detected an asic on board %d", k + 1);
         if (!((info.boards_present >> k) & 1))
            printf(" but FW doesn't report this board!");
         printf("\n");
         boards.push_back(k);
         BoardEnumerate();
//         std::sort(found.begin(), found.end());
//         for (i = 0; i < found.size(); i++)
//            printf("asic %d\n", found[i]);

         if (found.size() == 150 || found.size() == 149)
            zareen = true;
         }
      else {
         if (((info.boards_present >> k) & 1))
            printf("FW reports board %d but I can't communicate to asic 0!\n", k + 1);
         }
      }
   if (boards.size() == 0) {
      printf("No boards so nothing to do\n");
      usb.VddEnable(0x0);
      usb.SetFanSpeed(100);
      return;
      }
   Delay(1000);
   printf("Fans at 100%, supply@12V pll=1Ghz power=%.1fW\n", ReadPower());




//   boards.clear();
//   boards.push_back(0);
//   boards.push_back(2);
   systeminfotype sinfo;
   for (k = 0; k < boards.size(); k++) {
      for (i = 0; i < (zareen ? topology_1board_150.size() : topology_1board.size()); i++)
         {
         topology_system.push_back(zareen ? topology_1board_150[i] : topology_1board[i]);
         topology_system.back().board = boards[k];
         }
      }
   sinfo.optimal_temp = 25;
   sinfo.zareen = zareen;
   if (zareen) sinfo.min_voltage = 12.0;
   DescribeSystem(topology_system, sinfo);
   vector<batchtype> batch;
   for (i = 0; i < topology.size(); i++)
      {
      topologytype& t = topology[i];
      batch.push_back(batchtype().Write(t.board, t.id, E_VERSION_BOUND, i * 100 + ((i * 100 + 99) << 16)));
      batch.push_back(batchtype().Write(t.board, t.id, E_HASHCONFIG, (0 << 8) | (1 << 9)));
      }
   ReadWriteConfig(batch);
   batch.clear();
   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   Load(h, 0, 38, 0);
   CommunicationTest();

#ifdef _DEBUGfoo
   usb.SetFanSpeed(100);
   int seed=0,it;
   while (true) {
      for (it = 0; it < 50; it++)
	 {
	 float hitrate, avg_temp, max_temp, efficiency;
	 bool too_hot = false;

	 for (i = 0; i < topology.size(); i++)
	    {
	    const int vcosel = 2;
	    const int div1 = 1, div2 = 1; // I want the vco reasonably slow to save power but still achieve low min frequency
	    int pll_divider = (div1 + 1) * (div2 + 1);
	    pll_multiplier = pll_divider / systeminfo.refclk * (1 << 20);
	    topologytype& t = topology[i];
	    int frequency = random(seed) * 800 + 300.0;
	    batch.push_back(batchtype().Write(t.board, t.id, E_PLLOPTION, 0x10000)); // refdiv=1
	    batch.push_back(batchtype().Write(t.board, t.id, E_PLLCONFIG, 0x12)); // put pll into bypass, the fixes board issue when engaging power switch
	    batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, systeminfo.min_frequency * pll_multiplier));
	    batch.push_back(batchtype().Write(t.board, t.id, E_PLLCONFIG, 0x1d + (div2 << 13) + (div1 << 10) + (vcosel << 8))); // vcosel=0, div1=1, div2=2 (nominal vcoclk 1.6-3.2Ghz, hashclk is 800Mhz-1.6Ghz, assume vco can go lower than spec
	    batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, frequency * pll_multiplier));
	    batch.push_back(batchtype().Write(t.board, t.id, E_IPCFG, 0x1)); // ipclk is 6.25Mhz
	    batch.push_back(batchtype().Write(t.board, t.id, E_TEMPCFG, 0xd));  // turn on temp sensor
	    batch.push_back(batchtype().Write(t.board, t.id, E_DVMCFG, 0x2));
	    batch.push_back(batchtype().Write(t.board, t.id, E_DVMCFG, 0x3));
	    batch.push_back(batchtype().Write(t.board, t.id, E_DVMCFG, 0x1));
	    batch.push_back(batchtype().Write(t.board, t.id, E_DVMCFG, 0x9 | (5 << 12)));   // select channel 5 for voltage measurements
	    batch.push_back(batchtype().Write(t.board, t.id, E_DVMCFG, 0x5));
	    batch.push_back(batchtype().Write(t.board, t.id, E_HASHCONFIG, (0 << 8) | (1 << 9)));
	    batch.push_back(batchtype().Read(t.board, t.id, E_UNIQUE));
	    }
	 ReadWriteConfig(batch);
	 batch.clear();
	 CommunicationTest();

	 printf("Turning switch on, it=%d\n", it);
	 int enables = 0;
	 for (i = 0; i < boards.size(); i++)
	    enables |= 1 << boards[i];
	 usb.VddEnable(enables);
	 systeminfo.optimal_temp = 60.0;
	 for (i = 0; i < 10; i++) {
	    hitrate = MeasureHashRate(10 * 1000, too_hot, avg_temp, max_temp);   // run for 30 seconds to get a clean sample for the printsystem below
	    PrintSummary(NULL, "", efficiency);
	    CommunicationTest();
	    SetVoltage(voltage = (i % 2) ? 11.5 : 14.0);
	    usb.SetFanSpeed(0);
	    Delay(2000);
	    usb.SetFanSpeed(255);
	    Delay(2000);
	    usb.SetFanSpeed(200);
	    Delay(2000);
	    }
	 PrintSystem();
	 printf("Turning switch off, it=%d\n", it);
	 usb.VddEnable(0);
	 SetVoltage(voltage = random(seed) * 2.75 + 12);
	 }
      Delay(1000);
      CommunicationTest();
      InitialSetup();
      PrintSystem();
      int iterations = 0;
      for (it = 0; it < 2; it++) {
	 CommunicationTest();
	 if (topology.size() >= 132) { for (i = 0; i < 3; i++) ReadAllVoltages(topology[i + 129]); }
	 if (topology.size() >= 264) { for (i = 0; i < 3; i++) ReadAllVoltages(topology[i + 129 + 132]); }
	 if (topology.size() >= 396) { for (i = 0; i < 3; i++) ReadAllVoltages(topology[i + 129 + 264]); }
	 printf("iterations %d\n", it++);
	 DVFS((it % 2 == 0 ? 40 : 160) * topology.size() / 396.0, 0, true);
	 PrintSystem(NULL);
	 }
      }

#endif

   char label[256];
   printf("What is the system name? ");
   gets_s(label, sizeof(buffer));
   sprintf(buffer, "%s_all", label);
   PerformanceSweep(0x7, buffer, 0);
   MinPower();
/*
   usb.VddEnable(0x0);
   sprintf(buffer, "%s_B1", label);
   PerformanceSweep(0x1, buffer, 0);
   MinPower();
   usb.VddEnable(0x0);
   sprintf(buffer, "%s_B2", label);
   PerformanceSweep(0x2, buffer, 0);
   MinPower();
   usb.VddEnable(0x0);
   sprintf(buffer, "%s_B3", label);
   PerformanceSweep(0x4, buffer, 0);
   MinPower();
   usb.VddEnable(0x0);
   usb.SetFanSpeed(100);
*/
   return;

   PerformanceSweep(0x1, "911_build_B1", 0.0);
   PerformanceSweep(0x2, "911_build_B2", 0.0);
   PerformanceSweep(0x4, "911_build_B3", 0.0);
   MinPower();
   usb.VddEnable(0x0);
   return;
   PerformanceSweep(0x2, "tt2_B2", 60.0);
   PerformanceSweep(0x2, "tt2_B2", 70.0);
   PerformanceSweep(0x2, "tt2_B2", 80.0);
   MinPower();
   usb.VddEnable(0x0);
   PerformanceSweep(0x4, "tt2_B3", 60.0);
   PerformanceSweep(0x4, "tt2_B3", 70.0);
   PerformanceSweep(0x4, "tt2_B3", 80.0);
   MinPower();
   usb.VddEnable(0x0);

   usb.SetFanSpeed(100);
   return;


   InitialSetup();
   while (true) {
//      DVFS(140, false, true);
      DVFS(80, false, true);
      float hitrate, avg_temp, max_temp, efficiency;
      bool too_hot = false;
      hitrate = MeasureHashRate(10 * 1000, too_hot, avg_temp, max_temp);   // run for 30 seconds to get a clean sample for the printsystem below
      PrintSummary(NULL,"", efficiency);
//      PrintSystem(NULL);
      }
//   DVFS(160, false, true, 1, true);
//   PrintSystem(NULL);
   MinPower();
   usb.VddEnable(0x0);
   usb.SetFanSpeed(100);
   return;

   
   PerformanceSweep(0x6,"system_50", 50.0);
   PerformanceSweep(0x6, "system_60", 60.0);
   PerformanceSweep(0x6, "system_70", 70.0);
   SetVoltage(systeminfo.min_voltage);
   Delay(1000);
   usb.VddEnable(0x0);
   usb.SetFanSpeed(100);
   return;
   



   for (k = 0; k < boards.size(); k++) {
      printf("What shall I call board %d? ",k);
      gets_s(buffer, sizeof(buffer));
      board_labels.push_back(new char[strlen(buffer) + 1]);
      strcpy(board_labels.back(), buffer);
      }


   int all = 0;
   for (i = 0; i < boards.size(); i++)
      {
      sprintf(buffer, "%s_%.0f", board_labels[i], 60.0);
      PerformanceSweep(1 << boards[i], buffer, 60.0);
      all |= 1 << boards[i];
      usb.VddEnable(0x0);
      printf("Done with board %d\n", boards[i]);
      }
   if (boards.size() > 1) {
      sprintf(buffer, "%s_%s_%s", board_labels[0], board_labels.size()>1? board_labels[1]:"", board_labels.size() > 2 ? board_labels[2] : "");
      PerformanceSweep(all, buffer,60.0);
      }
   SetVoltage(systeminfo.min_voltage);
   Delay(1000);
   usb.VddEnable(0x0);
   usb.SetFanSpeed(100);
   return;


   for (k = 0; k < boards.size(); k++) {
      for (i = 0; i < topology_1board.size(); i++)
         {
         topology_system.push_back(topology_1board[i]);
         topology_system.back().board = boards[k];
         }
      }
   DescribeSystem(topology_system, systeminfotype());
   for (k = 0; k < boards.size(); k++) {
      printf("Board %d: ", boards[k]);
      SetBoard(boards[k]);
      usb.SetBaudRate(115200);
      SetBaud(1000000);
      BoardEnumerate();
      if (found.size() != 132)
         printf("This board has some missing chips(%d/132)\n", found.size());
      found.clear(); // zero this so we don't get confused with multiple boards
      }

   for (i = 0; i < topology.size(); i++)
      {
      topologytype& t = topology[i];
      batch.push_back(batchtype().Write(t.board, t.id, E_VERSION_BOUND, i * 100 + ((i * 100 + 99) << 16)));
      batch.push_back(batchtype().Write(t.board, t.id, E_HASHCONFIG, (0 << 8) | (1 << 9)));
      }
   ReadWriteConfig(batch);
   batch.clear();
   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   Load(h, 0, 38, 0);

   FILE* fptr = fopen("system data.txt", "at");
   DVFS(140, false, true);
   bool too_hot=false;
   float hitrate, avg_temp, max_temp, efficiency;
   hitrate = MeasureHashRate(60 * 1000, too_hot, avg_temp, max_temp);
   PrintSummary(fptr, buffer, efficiency);
   PrintSystem(fptr);
   fclose(fptr);
   return;

//   CommunicationTest();


//   InitialSetup();
//   PrintSystem();
//   PllExperiment(boards[1]);
//   IRTest(boards[0]);
//   SetVoltage(systeminfo.min_voltage);
//   Delay(1000);
//   usb.VddEnable(0x0);
//   usb.SetFanSpeed(100);
//   return;





   int index;
   float f, voltage = 12.0;
   vector<batchtype> batch_start, batch_end;
   const int period = 10000;
   const float f_start = 400, f_end = 1300, f_incr = 25;

   batch_start.push_back(batchtype().Write(0, -1, E_CLKCOUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_HIT_COUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_TRUEHIT_COUNT, 0));
   for (i = 0; i < found.size(); i++) {
      batch_end.push_back(batchtype().Read(0, found[i], E_CLKCOUNT));
      batch_end.push_back(batchtype().Read(0, found[i], E_GENERAL_HIT_COUNT));
      batch_end.push_back(batchtype().Read(0, found[i], E_GENERAL_TRUEHIT_COUNT));
      batch_end.push_back(batchtype().Read(0, found[i], E_BIST));
      }

   usb.VddEnable(0x7);

   bool first = true;
   for (voltage = 12.0; voltage <= 13.5; voltage += 0.5)
      {
      SetVoltage(voltage);
      Delay(1000);
      vector<float> zero;
      vector<vector<float> > plots;
      vector<float> power;
      bool goquick = false;
      for (f = f_start; f <= f_end; f += f_incr)
         zero.push_back(0.0);
      for (i = 0; i < 3; i++)
         plots.push_back(zero);
      for (f = f_start, index = 0; f <= f_end; f += f_incr, index++)
         {
         Pll(f, -1);
         for (i = 0; i < topology.size(); i++) {
            topologytype& t = topology[i];
            t.frequency = f;
            }
         Delay(100);
         WriteConfig(E_BIST, 0, -1);   // run bist before doing the mining experiment
         Delay(100);
         BatchConfig(batch_start);
         Delay(period * sqrt(1000 / f) * (goquick ? 0.05 : 1.0));
         BatchConfig(batch_end);
         float expected_total = 0.0, hit_total = 0.0, true_total = 0.0, bist_total = 0.0;
         for (i = 0; i < found.size(); i++) {
            float seconds = batch_end[i * 4 + 0].data * 40.0e-9;
            float expected = seconds * f * 254 * 4 / 3.0 * 1.0e6 / ((__int64)1 << 32);
            expected_total += expected;
            hit_total += batch_end[i * 4 + 1].data;
            true_total += batch_end[i * 4 + 2].data;
            bist_total += 254 - ((batch_end[i * 4 + 3].data) >> 1) & 0xff;
            }
//         plots[0][index] = bist_total / (132 * 254);
         plots[0][index] = true_total / expected_total;

//         WriteConfig(E_BIST, 0, -1);   // run bist before doing the mining experiment
         Delay(100);
         BatchConfig(batch_start);
         Delay(period * sqrt(1000 / f) * (goquick ? 0.05 : 1.0));
         BatchConfig(batch_end);

         expected_total = 0.0, hit_total = 0.0, true_total = 0.0, bist_total = 0.0;
         for (i = 0; i < found.size(); i++) {
            float seconds = batch_end[i * 4 + 0].data * 40.0e-9;
            float expected = seconds * f * 254 * 4 / 3.0 * 1.0e6 / ((__int64)1 << 32);
            expected_total += expected;
            true_total += batch_end[i * 4 + 2].data;
            }
         plots[1][index] = true_total / expected_total;
/*
         Pll(f, -1);
         for (i = 0; i < topology.size(); i++) {
            topologytype& t = topology[i];
            t.frequency = f;
            }
//         if (!goquick)
//            MinerTweak2();
//         WriteConfig(E_BIST, 0, -1);   // run bist before doing the mining experiment
         Delay(100);
         BatchConfig(batch_start);
         Delay(period * sqrt(1000 / f) * (goquick ? 0.05 : 1.0));
         BatchConfig(batch_end);

         expected_total = 0.0, hit_total = 0.0, true_total = 0.0, bist_total = 0.0;
         for (i = 0; i < found.size(); i++) {
            float seconds = batch_end[i * 4 + 0].data * 40.0e-9;
            float expected = seconds * f * 254 * 4 / 3.0 * 1.0e6 / ((__int64)1 << 32);
            expected_total += expected;
            true_total += batch_end[i * 4 + 2].data;
            }
         plots[2][index] = true_total / expected_total;
*/

         float odt = OnDieTemp(0);
         float odv = OnDieVoltage(0);
         usb.BoardInfo(info, true);
         power.push_back(info.supply_pin);
         printf("Testing PS=%.1f %.0fMhz, odv=%.0fmV odt=%.1fC Power=%.1fW, hits=%.2f%%, tweaks=%.2f%%, goquick=%d\n", voltage, f, odv*1000.0, odt, power.back(), 100.0* plots[0][index], 100.0 * plots[1][index], goquick ? 1 : 0);
         goquick = true_total == 0;
         }
      Pll(f, -1);
      Pll(MAXIMUM(100,f * 0.8), -1);
      Pll(MAXIMUM(100, f * 0.6), -1);
      Pll(MAXIMUM(100, f * 0.4), -1);
      Pll(MAXIMUM(100, f * 0.2), -1);

      FILE* fptr = fopen("data/system_paredo.csv", "at");
      if (fptr == NULL) FATAL_ERROR;
      if (first) {
         for (f = f_start, index = 0; f <= f_end; f += f_incr, index++)
            {
            fprintf(fptr, ",%.0f", f);
            }
         fprintf(fptr, "\n");
         first = false;
         }
      for (int p = 0; p < 3; p++)
         {
         if (p == 0)
            fprintf(fptr, "True%.2f", voltage);
         else if (p == 1)
            fprintf(fptr, "DVFS%.2f", voltage);
         else
            fprintf(fptr, "2nd%.2f", voltage);

         for (f = f_start, index = 0; f <= f_end; f += f_incr, index++)
            {
            fprintf(fptr, ",%.3f", plots[p][index]);
            }
         fprintf(fptr, "\n");
         }
      fclose(fptr);

      fptr = fopen("data/system_paredo_eff.csv", "at");
      if (fptr == NULL) FATAL_ERROR;
      if (first) {
         for (f = f_start, index = 0; f <= f_end; f += f_incr, index++)
            {
            fprintf(fptr, ",%.0f", f);
            }
         fprintf(fptr, "\n");
         first = false;
         }

      fprintf(fptr, "Efficiency%.2f", voltage);
      for (f = f_start, index = 0; f <= f_end; f += f_incr, index++)
         {
         float eff = power[index] / (plots[0][index] * f * 1.0e6 * 132 * 254 * 4 / 3 * 1.0e-12);
         if (plots[0][index] == 0)
            eff = 50;
         if (eff > 50) eff = 50;
         fprintf(fptr, ",%.3f", eff);
         }
      fprintf(fptr, "\n");
      fprintf(fptr, "DvfsEff%.2f", voltage);
      for (f = f_start, index = 0; f <= f_end; f += f_incr, index++)
         {
         float eff = power[index] / (plots[1][index] * f * 1.0e6 * 132 * 254 * 4 / 3 * 1.0e-12);
         if (plots[1][index] == 0)
            eff = 50;
         if (eff > 50) eff = 50;
         fprintf(fptr, ",%.3f", eff);
         }
      fprintf(fptr, "\n");
      fclose(fptr);
      }
   usb.VddEnable(0x0);
   usb.SetFanSpeed(100);
   }


