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

void testtype::PerformanceSweep(vector<topologytype> &topology_system, const char *label, float experiment)
   {
   systeminfotype systeminfo;
   headertype h;
   int i;
   bool too_hot;

   systeminfo.optimal_temp = 50.0;
   DescribeSystem(topology_system, systeminfo);

   vector<batchtype> batch;
   for (i = 0; i < topology.size(); i++)
      {
      topologytype& t = topology[i];
      batch.push_back(batchtype().Write(t.board, t.id, E_VERSION_BOUND, i * 100 + ((i * 100 + 99) << 16)));
      batch.push_back(batchtype().Write(t.board, t.id, E_DUTY_CYCLE, 0x80a0));
      batch.push_back(batchtype().Write(t.board, t.id, E_HASHCONFIG, 1<<16));
      batch.push_back(batchtype().Write(t.board, t.id, E_HASHCONFIG, 0));
      }
   ReadWriteConfig(batch);
   batch.clear();
   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   Load(h, 0, 38, 0);

   InitialSetup();

   float divider = 414.0 / topology.size();
   for (int experiment = 0; experiment <= 1; experiment+=1) {
      float hitrate, efficiency, avg_temp, max_temp;
      char buffer[256];
      plottype plots;
      sprintf(buffer, "data/%s_dvfs.txt", label);
      FILE* fptr = fopen(buffer, "at");

      for (float hash = 60; hash <= 220; hash += 20) {
//      for (float hash = 85; hash <= 85; hash += 30) {
         plots.AddData("Throughput", hash / divider);
         DVFS(hash / divider, experiment, true);
         if (experiment == 4)
            hitrate = MeasureHashRate2(20 * 1000, too_hot, avg_temp, max_temp);
         else
            hitrate = MeasureHashRate(20 * 1000, too_hot, avg_temp, max_temp);
         sprintf(buffer, "D_%d_%s: ", experiment,label);
         PrintSystem(fptr);
         PrintSummary(fptr, buffer, efficiency);
         plots.AddData(buffer, efficiency);
         }
      MinPower();

      if (fptr!=NULL) fclose(fptr);
      plots.AppendToFile("shmoo/dvfs.csv");
      }
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
//   vector<topologytype> topology_1board{ topologytype(0, 0, 0), topologytype(0, 1, 1), topologytype(0, 2, 2), topologytype(1, 0, 128), topologytype(1, 1, 129), topologytype(1, 2, 130), topologytype(2, 0, 3), topologytype(2, 1, 4), topologytype(2, 2, 5), topologytype(3, 0, 131), topologytype(3, 1, 132), topologytype(3, 2, 133), topologytype(4, 0, 6), topologytype(4, 1, 7), topologytype(4, 2, 8), topologytype(5, 0, 134), topologytype(5, 1, 135), topologytype(5, 2, 136), topologytype(6, 0, 9), topologytype(6, 1, 10), topologytype(6, 2, 11), topologytype(7, 0, 137), topologytype(7, 1, 138), topologytype(7, 2, 139), topologytype(8, 0, 12), topologytype(8, 1, 13), topologytype(8, 2, 14), topologytype(9, 0, 140), topologytype(9, 1, 141), topologytype(9, 2, 142), topologytype(10, 0, 15), topologytype(10, 1, 16), topologytype(10, 2, 17), topologytype(11, 0, 143), topologytype(11, 1, 144), topologytype(11, 2, 145), topologytype(12, 0, 18), topologytype(12, 1, 19), topologytype(12, 2, 20), topologytype(13, 0, 146), topologytype(13, 1, 147), topologytype(13, 2, 148), topologytype(14, 0, 21), topologytype(14, 1, 22), topologytype(14, 2, 23), topologytype(15, 0, 149), topologytype(15, 1, 150), topologytype(15, 2, 151), topologytype(16, 0, 24), topologytype(16, 1, 25), topologytype(16, 2, 26), topologytype(17, 0, 152), topologytype(17, 1, 153), topologytype(17, 2, 154), topologytype(18, 0, 27), topologytype(18, 1, 28), topologytype(18, 2, 29), topologytype(19, 0, 155), topologytype(19, 1, 156), topologytype(19, 2, 157), topologytype(20, 0, 30), topologytype(20, 1, 31), topologytype(20, 2, 32), topologytype(21, 0, 158), topologytype(21, 1, 159), topologytype(21, 2, 160), topologytype(22, 0, 33), topologytype(22, 1, 34), topologytype(22, 2, 35), topologytype(23, 0, 161), topologytype(23, 1, 162), topologytype(23, 2, 163), topologytype(24, 0, 36), topologytype(24, 1, 37), topologytype(24, 2, 38), topologytype(25, 0, 164), topologytype(25, 1, 165), topologytype(25, 2, 166), topologytype(26, 0, 39), topologytype(26, 1, 40), topologytype(26, 2, 41), topologytype(27, 0, 167), topologytype(27, 1, 168), topologytype(27, 2, 169), topologytype(28, 0, 42), topologytype(28, 1, 43), topologytype(28, 2, 44), topologytype(29, 0, 170), topologytype(29, 1, 171), topologytype(29, 2, 172), topologytype(30, 0, 45), topologytype(30, 1, 46), topologytype(30, 2, 47), topologytype(31, 0, 173), topologytype(31, 1, 174), topologytype(31, 2, 175), topologytype(32, 0, 48), topologytype(32, 1, 49), topologytype(32, 2, 50), topologytype(33, 0, 176), topologytype(33, 1, 177), topologytype(33, 2, 178), topologytype(34, 0, 51), topologytype(34, 1, 52), topologytype(34, 2, 53), topologytype(35, 0, 179), topologytype(35, 1, 180), topologytype(35, 2, 181), topologytype(36, 0, 54), topologytype(36, 1, 55), topologytype(36, 2, 56), topologytype(37, 0, 182), topologytype(37, 1, 183), topologytype(37, 2, 184), topologytype(38, 0, 57), topologytype(38, 1, 58), topologytype(38, 2, 59), topologytype(39, 0, 185), topologytype(39, 1, 186), topologytype(39, 2, 187), topologytype(40, 0, 60), topologytype(40, 1, 61), topologytype(40, 2, 62), topologytype(41, 0, 188), topologytype(41, 1, 189), topologytype(41, 2, 190), topologytype(42, 0, 63), topologytype(42, 1, 64), topologytype(42, 2, 65), topologytype(44, 0, 66), topologytype(44, 1, 67), topologytype(44, 2, 68),topologytype(43, 0, 191), topologytype(43, 1, 192), topologytype(43, 2, 193), topologytype(45, 0, 194), topologytype(45, 1, 195), topologytype(45, 2, 196) };
   vector<topologytype> topology_1board{ // fixme, missing 160,164
      topologytype(0, 0, 0), topologytype(0, 1, 1), topologytype(0, 2, 2), topologytype(1, 0, 128), topologytype(1, 1, 129), topologytype(1, 2, 130),
      topologytype(2, 0, 3), topologytype(2, 1, 4), topologytype(2, 2, 5), topologytype(3, 0, 131), topologytype(3, 1, 132), topologytype(3, 2, 133),
      topologytype(4, 0, 6), topologytype(4, 1, 7), topologytype(4, 2, 8), topologytype(5, 0, 134), topologytype(5, 1, 135), topologytype(5, 2, 136),
      topologytype(6, 0, 9), topologytype(6, 1, 10), topologytype(6, 2, 11), topologytype(7, 0, 137), topologytype(7, 1, 138), topologytype(7, 2, 139),
      topologytype(8, 0, 12), topologytype(8, 1, 13), topologytype(8, 2, 14), topologytype(9, 0, 140), topologytype(9, 1, 141), topologytype(9, 2, 142),
      topologytype(10, 0, 15), topologytype(10, 1, 16), topologytype(10, 2, 17), topologytype(11, 0, 143), topologytype(11, 1, 144), topologytype(11, 2, 145),
      topologytype(12, 0, 18), topologytype(12, 1, 19), topologytype(12, 2, 20), topologytype(13, 0, 146), topologytype(13, 1, 147), topologytype(13, 2, 148),
      topologytype(14, 0, 21), topologytype(14, 1, 22), topologytype(14, 2, 23), topologytype(15, 0, 149), topologytype(15, 1, 150), topologytype(15, 2, 151),
      topologytype(16, 0, 24), topologytype(16, 1, 25), topologytype(16, 2, 26), topologytype(17, 0, 152), topologytype(17, 1, 153), topologytype(17, 2, 154),
      topologytype(18, 0, 27), topologytype(18, 1, 28), topologytype(18, 2, 29), topologytype(19, 0, 155), topologytype(19, 1, 156), topologytype(19, 2, 157),
      topologytype(20, 0, 30), topologytype(20, 1, 31), topologytype(20, 2, 32), topologytype(21, 0, 158), topologytype(21, 1, 159), topologytype(21, 2, 160),
      topologytype(22, 0, 33), topologytype(22, 1, 34), topologytype(22, 2, 35), topologytype(23, 0, 161), topologytype(23, 1, 162), topologytype(23, 2, 163),
      topologytype(24, 0, 36), topologytype(24, 1, 37), topologytype(24, 2, 38), topologytype(25, 0, 164),  topologytype(25, 1, 165), topologytype(25, 2, 166),
      topologytype(26, 0, 39), topologytype(26, 1, 40), topologytype(26, 2, 41), topologytype(27, 0, 167), topologytype(27, 1, 168), topologytype(27, 2, 169), 
      topologytype(28, 0, 42), topologytype(28, 1, 43), topologytype(28, 2, 44), topologytype(29, 0, 170), topologytype(29, 1, 171), topologytype(29, 2, 172), 
      topologytype(30, 0, 45), topologytype(30, 1, 46), topologytype(30, 2, 47), topologytype(31, 0, 173), topologytype(31, 1, 174), topologytype(31, 2, 175), 
      topologytype(32, 0, 48), topologytype(32, 1, 49), topologytype(32, 2, 50), topologytype(33, 0, 176), topologytype(33, 1, 177), topologytype(33, 2, 178), 
      topologytype(34, 0, 51), topologytype(34, 1, 52), topologytype(34, 2, 53), topologytype(35, 0, 179), topologytype(35, 1, 180), topologytype(35, 2, 181), 
      topologytype(36, 0, 54), topologytype(36, 1, 55), topologytype(36, 2, 56), topologytype(37, 0, 182), topologytype(37, 1, 183), topologytype(37, 2, 184), 
      topologytype(38, 0, 57), topologytype(38, 1, 58), topologytype(38, 2, 59), topologytype(39, 0, 185), topologytype(39, 1, 186), topologytype(39, 2, 187), 
      topologytype(40, 0, 60), topologytype(40, 1, 61), topologytype(40, 2, 62), topologytype(41, 0, 188), topologytype(41, 1, 189), topologytype(41, 2, 190), 
      topologytype(42, 0, 63), topologytype(42, 1, 64), topologytype(42, 2, 65), topologytype(43, 0, 191), topologytype(43, 1, 192), topologytype(43, 2, 193), 
      topologytype(44, 0, 66), topologytype(44, 1, 67), topologytype(44, 2, 68), topologytype(45, 0, 194), topologytype(45, 1, 195), topologytype(45, 2, 196) }; 
   vector<topologytype> topology_system;
   vector<int> boards;
   vector<char*> board_labels;
   headertype h;
   boardinfotype info;
   gpiotype gpio;   gpio.clear();
   int i, k, invert;
   char buffer[256];

   usb.SetImmersion(false);
   SetVoltage(0.0);
   gpio.reset = 0;
   usb.ReadWriteGPIO(gpio);
   Delay(1000);
   usb.SetFanSpeed(200);

   usb.VddEnable(0x0);
   SetVoltage(12.0);
   Delay(1000);
   usb.BoardInfo(info, true);
   printf("FW is reporting boards[%c%c%c] present\n", info.boards_present & 1 ? '1' : ' ', info.boards_present & 2 ? '2' : ' ', info.boards_present & 4 ? '3' : ' ');

   usb.OLED_Display("Reset\n#");
   gpio.reset = 1;
   usb.ReadWriteGPIO(gpio);
   Delay(400);
   gpio.reset = 0;
   usb.ReadWriteGPIO(gpio);
   Delay(400);
   gpio.reset = 1;
   usb.ReadWriteGPIO(gpio);
   Delay(400);
   gpio.reset = 0;
   usb.ReadWriteGPIO(gpio);
   Delay(400);
   gpio.reset = 1;
   usb.ReadWriteGPIO(gpio);
   Delay(400);
   
   usb.OLED_Display("Scanning for asics\n#");
   for (invert = 0; invert < 8; invert++) {
      usb.VddEnable(invert<<4);
      for (k = 0; k < 3; k++) {
         SetBoard(k);
         usb.SetBaudRate(115200);
         found.clear();
         if (IsAlive(0))
            break;
         }
      if (k < 3)
         break;
      }
   if (invert == 8)
      {
      SetVoltage(0.0);
      printf("Could not communicate to any chips\n");
      return;
      }

   for (k = 0; k < 3; k++) {
      SetBoard(k);
      usb.SetBaudRate(115200);
      SetBaud(2000000);
      found.clear();
      if (IsAlive(0)) {
         printf("I detected an asic on board %d", k + 1);
         if (!((info.boards_present >> k) & 1))
            printf(" but FW doesn't report this board!");
         printf("\n");
         boards.push_back(k);
         BoardEnumerate();

         for (i = 0; i < topology_1board.size(); i++)
            {
            int j;
            for (j = found.size() - 1; j >= 0; j--)
               if (found[j] == topology_1board[i].id)
                  break;
            if (j >= 0) {  // only add asic that are actually present
               topology_system.push_back(topology_1board[i]);
               topology_system.back().board = k;
               }
            else printf("I didn't see expected asic %d\n", topology_1board[i].id);
            }
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

   systeminfotype sinfo;
   DescribeSystem(topology_system, sinfo);
   vector<batchtype> batch;
   for (i = 0; i < topology.size(); i++)
      {
      topologytype& t = topology[i];
      batch.push_back(batchtype().Write(t.board, t.id, E_VERSION_BOUND, i * 100 + ((i * 100 + 99) << 16)));
      batch.push_back(batchtype().Write(t.board, t.id, E_DUTY_CYCLE, 0x80a0));
      batch.push_back(batchtype().Write(t.board, t.id, E_HASHCONFIG, 1<<16));
      batch.push_back(batchtype().Write(t.board, t.id, E_HASHCONFIG, 0));
      }
   ReadWriteConfig(batch);
   batch.clear();
   SetBoard(1);

   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   Load(h, 0, 45, 0);
   CommunicationTest();

   if (false) {
      float hitrate, efficiency, avg_temp, max_temp;
      bool too_hot;
      int x = 0;
      InitialSetup();
      SetVoltage(12.5);
      Pll(500);
      hitrate = MeasureHashRate(10 * 1000, too_hot, avg_temp, max_temp);
      PrintSummary(NULL, "", efficiency);
      PrintSystem();

      DVFS(60, 0, true);
      Mine();
      MinPower();
      }
   if (false)
      {
      float efficiency = 0.0, avg_temp, max_temp, hitrate;
      bool too_hot;
      InitialSetup();
      DVFS(150, 4, true);
      hitrate = MeasureHashRate2(10 * 1000, too_hot, avg_temp, max_temp, 0);
      PrintSummary(NULL, "", efficiency);
      PrintSystem();
      MinPower();

      DVFS(150, 0, true);
      hitrate = MeasureHashRate(10 * 1000, too_hot, avg_temp, max_temp, 0);
      PrintSummary(NULL, "", efficiency);
//      PrintSystem();
      MinPower();

      DVFS(220, 0, true);
      hitrate = MeasureHashRate(10 * 1000, too_hot, avg_temp, max_temp, 0);
      PrintSummary(NULL, "", efficiency);
//      PrintSystem();
      MinPower();

      DVFS(220, 4, true);
      hitrate = MeasureHashRate2(10 * 1000, too_hot, avg_temp, max_temp, 0);
      PrintSummary(NULL, "", efficiency);
      //      PrintSystem();
      MinPower();
      return;

      DVFS(60, 4, true);
      hitrate = MeasureHashRate(10 * 1000, too_hot, avg_temp, max_temp, 1);
      PrintSummary(NULL, "", efficiency);
      PrintSystem();
      MinPower();
      return;
      }
   
   char label[256];
   printf("What is the system name? ");
   gets_s(label, sizeof(buffer));
   sprintf(buffer, "%s_all", label);
   PerformanceSweep(topology_system, buffer, 0);
   MinPower();

   }

