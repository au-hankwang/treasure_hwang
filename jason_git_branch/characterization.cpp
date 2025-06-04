#include "pch.h"
#include "helper.h"
#include "miner.h"
#include "dvfs.h"
#include "serial.h"
#include "usb.h"
#include "test.h"
#include "characterization.h"
#include "multithread.h"
#include "winsock2.h"
#pragma comment(lib, "Ws2_32.lib")     // needed for socket routines

bool NoTempControl = true;


#pragma optimize("", off)




void PrintShmooMap(const bitfieldtype <50000>& map, const char* title, const int width, const int numrows, float v_start, float v_incr, const vector<float>& frequencies, FILE* fptr)
   {
   int row, col, i;
   float v;

   fprintf(fptr, "\f%s\n", title);
   for (col = frequencies.size() - 1; col >= 0; col--)
      {
      fprintf(fptr, "%4.0f   |", frequencies[col]);

      for (row = 0; row < numrows; row++)
         {
         fprintf(fptr, "%c", map.GetBit(row * width + col) ? 'X' : ' ');
         }
      fprintf(fptr, "\n");
      }
   fprintf(fptr, "       *");
   for (row = 0, v = v_start; row < numrows; row++, v += v_incr)
      fprintf(fptr, "-");
   for (i = 0; i < 5; i++)
      {
      char buffer[64];
      fprintf(fptr, "\n        ");
      for (row = 0, v = v_start; row < numrows; row++, v += v_incr)
         {
         sprintf(buffer, "%4.3f", v);
         if (row % 5 == 0)
            fprintf(fptr, "%c", buffer[i]);
         else
            fprintf(fptr, " ");
         }
      }
   fprintf(fptr, "\n\n");
   }

void testtype::BistShmoo(const char* filename, float temp)
   {
   TAtype ta("BistShmoo");
   bool real = false;

   int v_start = 250, v_end = 350, v_incr = 10;
   float f_start = 100, f_end = 700, f_incr = 25;
   float v, f, period = 5000;
   int iterations = 32;
   const int num_miners = 238;
   int i, samples;
   char buffer[256];
   headertype h;

   boardinfotype info;
   vector<batchtype> batch_start, batch_end;
   plottype plot;

   usb.OLED_Display("Bist Shmoo");
   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   Load(h, 64, 0, -1);
   WriteConfig(E_VERSION_BOUND, 0xff0000);
   WriteConfig(E_DUTY_CYCLE, 0);
   for (i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0);
   batch_start.push_back(batchtype().Write(0, -1, E_CLKCOUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_HIT_COUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_TRUEHIT_COUNT, 0));
   batch_end.push_back(batchtype().Read(0, -1, E_CLKCOUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_GENERAL_HIT_COUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_GENERAL_TRUEHIT_COUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_BIST));

   for (f = f_start; f <= f_end; f += f_incr)
      {
      plot.AddData("Throughput(Mhz)", f);
      }

   for (int p= 0; p<= 5; p += p==1?3:1)
//   for (int p = 0; p <= 7; p += 1)
      {
      const int phase = (p % 4) + 2;
      SetVoltage(0); // reset the supply in case it tripped
      Delay(1);
      SetVoltage(v_start*0.001);

      for (v = v_start; v <= v_end; v += v_incr) {
         SetVoltage(v * 0.001);
         Delay(100);
         printf("Bist shmoo phase%d Voltage=%.0f, odv=%.1f odt=%.1f\n", p, v, OnDieVoltage() * 1000.0, OnDieTemp());
         Pll(1000.0);
         WriteConfig(E_HASHCONFIG, p+(1 << 16));
         WriteConfig(E_HASHCONFIG, p);

         for (f = f_start; f <= f_end; f += f_incr)
            {
            Pll(f * phase, -1);
            Delay(10);
            sprintf(buffer, "Bist Shmoo\n%4.0fMhz %.0fmV\n#", f * phase, v);
            usb.OLED_Display(buffer);
            WriteConfig(E_BIST_GOOD_SAMPLES, 0);
            WriteConfig(E_BIST, iterations);
            while (ReadConfig(E_BIST) != 0)
               ;
            samples = ReadConfig(E_BIST_GOOD_SAMPLES);

            float bist_hit = (float)samples / (iterations * 8 * num_miners);
            float real_hit = 0.0;
            if (real)
               {
               BatchConfig(batch_start);
               Delay(period * 700 / f);
               BatchConfig(batch_end);
               float seconds = batch_end[0].data / 25.0e6;
               float expected = seconds * f * 1.0e6 * num_miners * 4 / ((__int64)1 << 32);
               real_hit = batch_end[2].data / expected;
               sprintf(buffer, "Real%d_%.0fmv_%.0fC", p, v,temp);
               plot.AddData(buffer, real_hit);
               }
            usb.BoardInfo(info, true);
            float efficiency100 = info.core_current * v * 0.001 / (f * num_miners * 4 * 1.0e-6);
//            printf("%.0fMhz@%.0fC phase%d bist_hit=%.1f%% real_hit=%.1f%% bist_eff=%.2fJ/TH\n", f, temp,p, bist_hit*100.0, real_hit * 100.0, MINIMUM(99.99, efficiency100 / MAXIMUM(real_hit,bist_hit)));
            sprintf(buffer, "Current%d_%.0fmv_%.0fC", p, v, temp);
            plot.AddData(buffer, info.core_current);
            sprintf(buffer, "Bist%d_%.0fmv_%.0fC", p, v, temp);
            plot.AddData(buffer, bist_hit);
            sprintf(buffer, "Eff%d_%.0fmv_%.0fC", p, v, temp);
            plot.AddData(buffer, MINIMUM(99.99, efficiency100 / bist_hit));
            if (info.core_current > 70) { Pll(100); break; }
            }
         }
      }
   
   time_t seconds;
   time(&seconds);
   const struct tm* localtime_tm = localtime(&seconds);
   sprintf(buffer, "Data taken at %s", asctime(localtime_tm));
   plot.AddHeader(buffer);
   plot.AppendToFile(filename);
   }

void testtype::BistShmooTest(const char* filename, float temp)
   {
   TAtype ta("BistShmoo");
   bool real = false;

   int v_start = 250, v_end = 350, v_incr = 10;
   float f_start = 100, f_end = 700, f_incr = 25;
   float v, f, period = 5000;
   int iterations = 100;
   const int num_miners = 238;
   int i, samples;
   char buffer[256];
   headertype h;

   boardinfotype info;
   vector<batchtype> batch_start, batch_end;
   plottype plot;

   //   SetVoltage(.25);

   usb.OLED_Display("Bist Shmoo");
   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   Load(h, 64, 0, -1);
   WriteConfig(E_VERSION_BOUND, 0xff0000);
   WriteConfig(E_DUTY_CYCLE, 0);
   for (i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0);
   batch_start.push_back(batchtype().Write(0, -1, E_CLKCOUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_HIT_COUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_TRUEHIT_COUNT, 0));
   batch_end.push_back(batchtype().Read(0, -1, E_CLKCOUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_GENERAL_HIT_COUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_GENERAL_TRUEHIT_COUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_BIST));

   for (f = f_start; f <= f_end; f += f_incr)
      {
      plot.AddData("Throughput(Mhz)", f);
      }

   for (iterations = 1; iterations <= 128; iterations *= 2) {
      for (int p = 0; p <= 5; p += p == 1 ? 3 : 1)
         {
         const int phase = (p % 4) + 2;

         for (v = v_start; v <= v_end; v += v_incr) {
            SetVoltage(v * 0.001);
            Delay(100);
            printf("Voltage=%.0f, odv=%.1f odt=%.1f\n", v, OnDieVoltage() * 1000.0, OnDieTemp());
            Pll(1000.0);
            usb.OLED_Display("$");
            WriteConfig(E_HASHCONFIG, p + (1 << 16));
            WriteConfig(E_HASHCONFIG, p);

            for (f = f_start; f <= f_end; f += f_incr)
               {
               Pll(f * phase, -1);
               Delay(10);
               sprintf(buffer, "Bist Shmoo\n%.1fMhz %.0fmV\n#", f * phase, v);
               usb.OLED_Display(buffer);
               WriteConfig(E_BIST_GOOD_SAMPLES, 0);
               WriteConfig(E_BIST, iterations);
               while (ReadConfig(E_BIST) != 0)
                  ;
               samples = ReadConfig(E_BIST_GOOD_SAMPLES);

               float bist_hit = (float)samples / (iterations * 8 * num_miners);
               float real_hit = 0.0;
               if (real)
                  {
                  BatchConfig(batch_start);
                  Delay(period * 700 / f);
                  BatchConfig(batch_end);
                  float seconds = batch_end[0].data / 25.0e6;
                  float expected = seconds * f * 1.0e6 * num_miners * 4 / ((__int64)1 << 32);
                  real_hit = batch_end[2].data / expected;
                  sprintf(buffer, "Real%d_%.0fmv_%.0fC", p, v, temp);
                  plot.AddData(buffer, real_hit);
                  }
               usb.BoardInfo(info, true);
               float efficiency100 = info.core_current * v * 0.001 / (f * num_miners * 4 * 1.0e-6);
               printf("%.0fMhz phase%d bist_hit=%.1f%% real_hit=%.1f%% bist_eff=%.2fJ/TH\n", f, p, bist_hit * 100.0, real_hit * 100.0, MINIMUM(99.99, efficiency100 / MAXIMUM(real_hit, bist_hit)));
               sprintf(buffer, "Current%d_%.0fmv_%dits", p, v, iterations);
               plot.AddData(buffer, info.core_current);
               sprintf(buffer, "Bist%d_%.0fmv_%dits", p, v, iterations);
               plot.AddData(buffer, bist_hit);
               sprintf(buffer, "Eff%d_%.0fmv_%dits", p, v, iterations);
               plot.AddData(buffer, MINIMUM(99.99, efficiency100 / bist_hit));
               if (info.core_current > 70) { Pll(100); break; }
               }
            }
         }
      }

   time_t seconds;
   time(&seconds);
   const struct tm* localtime_tm = localtime(&seconds);
   sprintf(buffer, "Data taken at %s", asctime(localtime_tm));
   plot.AddHeader(buffer);
   plot.AppendToFile(filename);
   }

void testtype::OldChipMetric(metrictype& metric, float f)
   {
   TAtype ta("Old ChipMetric");
   float v, odv, odt;
   boardinfotype info;
   int i;
   headertype h;

   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   Load(h, 64, 0, -1);

   usb.OLED_Display("Old Chip Metric\n#\n@");

   int period = 5000;

   SetVoltage(0);
   Pll(0, -1);
   WriteConfig(E_VERSION_BOUND, 0xff0000);
   WriteConfig(E_DUTY_CYCLE, 0x80a0);
   for (i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0);
   usb.BoardInfo(info, true);
   odv = OnDieVoltage();

   bool quick = true;
   Pll(25, -1);
   SetVoltage(0);

   vector<batchtype> batch_start, batch_end;
   batch_start.push_back(batchtype().Write(0, -1, E_HASHCONFIG, (1 << 16)));
   batch_start.push_back(batchtype().Write(0, -1, E_HASHCONFIG, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_HASHCONFIG, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_BIST, 1));
   batch_start.push_back(batchtype().Write(0, -1, E_CLKCOUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_CLKCOUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_HIT_COUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_TRUEHIT_COUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_HASHCLK_COUNT, 0));
   batch_end.push_back(batchtype().Read(0, -1, E_CLKCOUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_GENERAL_HIT_COUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_GENERAL_TRUEHIT_COUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_HASHCLK_COUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_BIST));

   SetVoltage(0);
   Delay(100);
   metric.throughput = f;
   Pll(f * 2.0, -1);
   printf("Performance at %4.0fMhz ", f);
   WriteConfig(E_HASHCONFIG, (1 << 16));
   WriteConfig(E_HASHCONFIG, 0);

   for (v=.240; v < .350; v += 0.002) {
      SetVoltage(v);
      Delay(10);
      BatchConfig(batch_start);
      odv = OnDieVoltage();
      odt = OnDieTemp();
      Delay(period * 1000 * (quick?0.3:1.0) / f);
      usb.BoardInfo(info, true);
      BatchConfig(batch_end);
      float seconds = batch_end[0].data / 25.0e6;
      float expected = seconds * f * 1.0e6 * 238 * 4 / ((__int64)1 << 32);
      float hit = MINIMUM(1.0,batch_end[2].data / expected);
      float efficiency = info.core_current * v / (hit * f * 1.0e6 * 238 * 4 * 1.0e-12);
      if (metric.peakE == 0.0 || metric.peakE > efficiency) {
         metric.peakE = efficiency;
         metric.peakH = hit;
         metric.peakI = info.core_current;
         metric.peakV = v;
         metric.temp = odt;
         metric.phase = 0;
         }
//      printf("isl=%.0f, v=%.1f hit=%.1f%% eff=%.1fJ/TH\n", v * 1000.0, odv * 1000.0, hit * 100.0, efficiency);
      if (metric.hit50 == 0.0 && hit >= 0.50)
         metric.hit50 = v;
      if (metric.hit98 == 0.0 && hit >= 0.98) {
         metric.hit98 = v;
         break;
         }
      quick = false;
      if (hit < 0.3) {
         v += 0.008; quick = true;
         }
      else if (hit < 0.7) v += 0.003;
      }
   printf("@%3.0fC 50%% %.0fmV, 98%% %.0fmV, PeakE %.0fmV Eff=%.1f J/TH\n", metric.temp, metric.hit50 * 1000.0, metric.hit98 * 1000.0, metric.peakV * 1000.0, metric.peakE);

   Pll(25, -1);
   SetVoltage(0);
   }


void testtype::ChipMetric(metrictype& metric, float startmV, float f, int phase, int duty_cycle, bool quick)
   {
   TAtype ta("ChipMetric");
   float v;
   const int iterations = 320;
   boardinfotype info;
   int i;
   headertype h;
   vector<batchtype> batch_start, batch_end;

   SetVoltage(.25);
   Delay(10);
   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   Load(h, 64, 0, -1);
   usb.OLED_Display("Chip Metric\n#\n@");
//   usb.OLED_Display("$");

   Pll(200, -1, true);

   WriteConfig(E_VERSION_BOUND, 0xff0000);
   if (duty_cycle == 0)
      WriteConfig(E_DUTY_CYCLE, 0);
   else if (duty_cycle < 0)
      WriteConfig(E_DUTY_CYCLE, 0x8000 | (-duty_cycle + 0x80));
   else
      WriteConfig(E_DUTY_CYCLE, 0x80 | ((duty_cycle + 0x80) << 8));

   WriteConfig(E_HASHCONFIG, 1<<16);
   WriteConfig(E_HASHCONFIG, phase);
   for (i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0);
   WriteConfig(E_HASHCONFIG, phase + (1 << 16));
   WriteConfig(E_HASHCONFIG, phase);
   WriteConfig(E_BIST, 1);


   batch_start.push_back(batchtype().Write(0, -1, E_HASHCONFIG, phase + (1 << 16)));
   batch_start.push_back(batchtype().Write(0, -1, E_HASHCONFIG, phase+(1<<3)));
   batch_start.push_back(batchtype().Write(0, -1, E_BIST_GOOD_SAMPLES, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_BIST, 1));
   if (!quick) {
      batch_start.push_back(batchtype().Write(0, -1, E_BIST_RESULTS + 0, 0));
      batch_start.push_back(batchtype().Write(0, -1, E_BIST_RESULTS + 1, 0));
      batch_start.push_back(batchtype().Write(0, -1, E_BIST_RESULTS + 2, 0));
      batch_start.push_back(batchtype().Write(0, -1, E_BIST_RESULTS + 3, 0));
      batch_start.push_back(batchtype().Write(0, -1, E_BIST_RESULTS + 4, 0));
      batch_start.push_back(batchtype().Write(0, -1, E_BIST_RESULTS + 5, 0));
      batch_start.push_back(batchtype().Write(0, -1, E_BIST_RESULTS + 6, 0));
      batch_start.push_back(batchtype().Write(0, -1, E_BIST_RESULTS + 7, 0));
      }
   batch_end.push_back(batchtype().Read(0, -1, E_BIST_GOOD_SAMPLES));
   if (!quick) {
      batch_end.push_back(batchtype().Read(0, -1, E_BIST_RESULTS + 0));
      batch_end.push_back(batchtype().Read(0, -1, E_BIST_RESULTS + 1));
      batch_end.push_back(batchtype().Read(0, -1, E_BIST_RESULTS + 2));
      batch_end.push_back(batchtype().Read(0, -1, E_BIST_RESULTS + 3));
      batch_end.push_back(batchtype().Read(0, -1, E_BIST_RESULTS + 4));
      batch_end.push_back(batchtype().Read(0, -1, E_BIST_RESULTS + 5));
      batch_end.push_back(batchtype().Read(0, -1, E_BIST_RESULTS + 6));
      batch_end.push_back(batchtype().Read(0, -1, E_BIST_RESULTS + 7));
      }

   memset(&metric, 0, sizeof(metric));
   metric.throughput = f;
   metric.phase = phase;
   float mhz = f * (phase % 4 + 2);
   Pll(mhz, -1);
   Pll(mhz, -1,true);
   metric.peakE = 1000.0;
   for (v = startmV; v <= 375; v += 2)
      {
      SetVoltage(v * 0.001);
      Delay(20);
      BatchConfig(batch_start);
      WriteConfig(E_BIST, iterations);
      while (ReadConfig(E_BIST) != 0)
         ;
      BatchConfig(batch_end);
      usb.BoardInfo(info, true);
      float hit = batch_end[0].data / (float)(iterations * 8 * 238);
      float efficiency = info.core_current * v*0.001 / (f * 238 * 4 * 1.0e-6 * hit);
      int bad = 0;
      if (!quick) {
         for (i = 0; i < 8; i++)
            bad += PopCount(batch_end[i + 1].data);
         }

      if (hit >= 0.5 && metric.hit50 == 0)
         metric.hit50 = v * 0.001;
      if (hit >= 0.98 && metric.hit98 == 0)
         metric.hit98 = v * 0.001;
      if (238 - bad >= 238 * 0.5 && metric.perf50 == 0)
         metric.perf50 = v * 0.001;
      if (238 - bad >= 238 * 0.98 && metric.perf98 == 0)
         metric.perf98 = v * 0.001;
//      if (efficiency < metric.peakE) {
      if (hit>=0.975) {
         metric.peakE = efficiency;
         metric.peakH = hit;
         metric.peakV = v*0.001;
         metric.peakI = info.core_current;
         if (quick) break;
         }
      if (metric.hit98 && metric.perf98) break;
      }
   metric.temp = OnDieTemp();


   if (quick)
      printf("Phase%d througput=%.0fMhz hit50=%.3f hit98=%.3f PeakV=%.3fV PeakE=%.2fJ/TH temp=%.1fC\n", phase, f, metric.hit50, metric.hit98, metric.peakV, metric.peakE, metric.temp);
   else
      printf("Phase%d througput=%.0fMhz hit50=%.3f hit98=%.3f perfect50=%.3f perfect98=%.3f PeakV=%.3fV PeakE=%.2fJ/TH temp=%.1fC\n", phase, f, metric.hit50, metric.hit98, metric.perf50, metric.perf98, metric.peakV, metric.peakE, metric.temp);
   Pll(25, -1);
   SetVoltage(.2);
   }

void testtype::ChipMetric3(metrictype& metric, float startmV, float f, const vector<tweaktype>& tweaks, int experiment)
   {
   TAtype ta("ChipMetric3");
   float v;
   const int iterations = 320;
   boardinfotype info;
   int i;
   headertype h;
   

   SetVoltage(0);
   SetVoltage(.25);
   Delay(10);
   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   Load(h, 64, 0, -1);
   usb.OLED_Display("Chip Metric\n#\n@");
   //   usb.OLED_Display("$");

   Pll(200, -1, true);

   const unsigned duty_cycle = 0x80a0;

   WriteConfig(E_VERSION_BOUND, 0xff0000);

   for (i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0);
   WriteConfig(E_HASHCONFIG, (1 << 16));
   WriteConfig(E_HASHCONFIG, 0);
   WriteConfig(E_BIST, 1);

   memset(&metric, 0, sizeof(metric));
   metric.throughput = f;
   metric.phase = 0;
   float mhz = f * 2;
   Pll(mhz, -1);
   Pll(mhz, -1, true);
   metric.peakE = 1000.0;
   metric.peakV = 1000.0;
   tweaktype lastt;
   for (v = startmV; v <= 375; v += 2)
      {
      tweaktype t=tweaks[0];
      vector<batchtype> batch_start;

      for (i = 0; i < tweaks.size(); i++)
         if (fabs(tweaks[i].voltage- v) < fabs(t.voltage- v))
            t = tweaks[i];
//      Pll(mhz * t.pll_ratio, -1);
//      Pll(mhz * t.pll2_ratio, -1, true);
      Pll(mhz , -1);
      Pll(mhz , -1, true);
      if (experiment == 1)
         Pll(0, -1, true);

      batch_start.push_back(batchtype().Write(0, -1, E_HASHCONFIG, 0 + (1 << 16)));
      batch_start.push_back(batchtype().Write(0, -1, E_HASHCONFIG, 0));
      batch_start.push_back(batchtype().Write(0, -1, E_BIST_GOOD_SAMPLES, 0));
      batch_start.push_back(batchtype().Write(0, -1, E_BIST, 1));
      int p2count = 0;
      for (i = 0; i < 238; i++) {
         bool phase2 = (t.phase2_map[i / 32] >> (i % 32)) & 1;
         bool pll2 = (t.pll2_map[i / 32] >> (i % 32)) & 1;
         if (experiment == 1) pll2 = false;
         p2count += pll2 ? 1 : 0;
         int duty = phase2 ? duty_cycle : 0;
         if (experiment == 2 && !phase2) duty = 0x8080;
         batch_start.push_back(batchtype().Write(0, -1, E_DUTY_CYCLE, duty));
         batch_start.push_back(batchtype().Write(0, -1, E_MINER_CONFIG, (i << 16) | (phase2 ? 0 : 4) | (pll2 ? 8 : 0)));
         }
      lastt = t;

      SetVoltage(v * 0.001);
      Delay(20);
      BatchConfig(batch_start);
      for (float x = 0; x <= 0.0; x += 0.025) {
         float p2f = mhz * (1 - t.pll_ratio * x);
         float p1f = mhz * (1 + t.pll2_ratio * x);
         float total = p2f * p2count + p1f *(238 - p2count);
         Pll(p2f, -1, false);
         Pll(p1f, -1, true);
         WriteConfig(E_BIST_GOOD_SAMPLES, 0);
         WriteConfig(E_BIST, iterations);
         while (ReadConfig(E_BIST) != 0)
            ;
         int samples = ReadConfig(E_BIST_GOOD_SAMPLES);
//         Delay(50);
         usb.BoardInfo(info, true);
         float hit = samples / (float)(iterations * 8 * 238);
         float efficiency = info.core_current * v * 0.001 / (f * 238 * 4 * 1.0e-6 * hit);
//         printf("X=%.2f pll1=%.0f pll2=%.0f, hit=%.2f e=%.1f",x, mhz * (1 - t.pll_ratio * x), mhz * (1 + t.pll2_ratio * x),hit,efficiency);

         if (hit >= 0.5 && metric.hit50 == 0)
            metric.hit50 = v * 0.001;
         if (hit >= 0.98 && metric.hit98 == 0)
            metric.hit98 = v * 0.001;
         if (hit>=0.975 && (v * 0.001 < metric.peakV)) {
            metric.peakE = efficiency;
            metric.peakH = hit;
            metric.peakV = v * 0.001;
            metric.peakI = info.core_current;
            metric.spare = x;
            break;
            }
         }
      if (metric.hit98 && metric.perf98) break;
      }
   metric.temp = OnDieTemp();

   printf("PhaseZ througput=%.0fMhz hit50=%.3f hit98=%.3f PeakV=%.3fV PeakE=%.2fJ/TH temp=%.1fC spare=%.2f\n", f, metric.hit50, metric.hit98, metric.peakV, metric.peakE, metric.temp,metric.spare);
   Pll(25, -1);
   SetVoltage(.2);
   }

   
void testtype::ChipMetric2(metrictype& metric, const float f, int phase, int *duty_settings, float *speeds)
   {
   TAtype ta("ChipMetric2");
   float v;
   const int iterations = 100;
   boardinfotype info;
   int i;
   headertype h;
   vector<batchtype> batch_start, batch_end;
   vector<batchtype> real_start, real_end;
   bool pll1[238];
   vector<speedsorttype> ss;

   float division = 10;

   for (i = 0; i < 238; i++)
      ss.push_back(speedsorttype(i, speeds[i]));
   std::sort(ss.begin(), ss.end());
   
   for (i = 0; i < 238; i++) {
      pll1[ss[i].engine] = i > (238 / division);
      }
   printf("low=%.0f 1/4=%.0f median=%.0f high=%.0f \n", ss[0].speed, ss[238/4].speed, ss[118].speed, ss[237].speed);
   
   SetVoltage(.25);
   Delay(10);
   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   Load(h, 64, 0, -1);
   usb.OLED_Display("Chip Metric\n#\n@");
   //   usb.OLED_Display("$");

   Pll(200, -1, true);

   WriteConfig(E_VERSION_BOUND, 0xff0000);
   WriteConfig(E_DUTY_CYCLE, 0);
   WriteConfig(E_HASHCONFIG, 1 << 16);
   WriteConfig(E_HASHCONFIG, phase);
   for (i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0);
   WriteConfig(E_HASHCONFIG, phase + (1 << 16));
   WriteConfig(E_HASHCONFIG, phase);
   WriteConfig(E_BIST, 1);

   batch_start.push_back(batchtype().Write(0, -1, E_HASHCONFIG, phase + (1 << 16)));
   batch_start.push_back(batchtype().Write(0, -1, E_HASHCONFIG, phase));
   batch_start.push_back(batchtype().Write(0, -1, E_BIST_GOOD_SAMPLES, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_BIST, 1));

   real_start.push_back(batchtype().Write(0, -1, E_CLKCOUNT, 0));
   real_start.push_back(batchtype().Write(0, -1, E_GENERAL_HIT_COUNT, 0));
   real_start.push_back(batchtype().Write(0, -1, E_GENERAL_TRUEHIT_COUNT, 0));
   real_end.push_back(batchtype().Read(0, -1, E_CLKCOUNT));
   real_end.push_back(batchtype().Read(0, -1, E_GENERAL_HIT_COUNT));
   real_end.push_back(batchtype().Read(0, -1, E_GENERAL_TRUEHIT_COUNT));


   for (i = 0; i < 238; i++) {
      if (i != 0 && duty_settings[i] == duty_settings[i - 1])
         ;
      else if (duty_settings[i]==0)
         batch_start.push_back(batchtype().Write(0, -1, E_DUTY_CYCLE, 0));
      else if (duty_settings[i] < 0)
         batch_start.push_back(batchtype().Write(0, -1, E_DUTY_CYCLE, 0x8000 | (-duty_settings[i] + 0x80)));
      else 
         batch_start.push_back(batchtype().Write(0, -1, E_DUTY_CYCLE, 0x80 | ((duty_settings[i] + 0x80) << 8)));
      batch_start.push_back(batchtype().Write(0, -1, E_MINER_CONFIG, (i<<16)|phase|(pll1[i]?8:0)));
      }
   batch_end.push_back(batchtype().Read(0, -1, E_BIST_GOOD_SAMPLES));

   memset(&metric, 0, sizeof(metric));
   metric.throughput = f;
   metric.phase = phase;
   float mhz = f * (phase % 4 + 2);
   for (float adder = 0; adder< 70; adder += 5) {
      printf("adder=%.3f:", adder);
      Pll(mhz + adder, -1, true);
      Pll(mhz - adder * (division-1), -1, false);
      metric.peakE = 1000.0;
      for (v = 230; v <= 300; v +=1)
         {
         SetVoltage(v * 0.001);
         Delay(40);
         usb.BoardInfo(info, true);
         BatchConfig(batch_start);
         WriteConfig(E_BIST, iterations);
         while (ReadConfig(E_BIST) != 0)
            ;
         BatchConfig(batch_end);
         float hit = batch_end[0].data / (float)(iterations * 8 * 238);
         float efficiency = info.core_current * v * 0.001 / (f * 238 * 4 * 1.0e-6 * hit);

         if (efficiency < metric.peakE) {
            metric.peakE = efficiency;
            metric.peakH = hit;
            metric.peakV = v * 0.001;
            metric.peakI = info.core_current;
            }
         }
      metric.temp = OnDieTemp();
      printf("Phase%d througput=%.0fMhz hit50=%.3f hit98=%.3f PeakV=%.3fV PeakI=%.3fA PeakH=%.3f PeakE=%.2fJ/TH temp=%.1fC  ", phase, f, metric.hit50, metric.hit98, metric.peakV, metric.peakI, metric.peakH, metric.peakE, metric.temp);

      if (false)
         {
         SetVoltage(metric.peakV);
         Delay(10);
         BatchConfig(real_start);
         int period = 30000;
         Delay(period * 700 / mhz);
         BatchConfig(real_end);
         float seconds = real_end[0].data / 25.0e6;
         float expected = seconds * mhz/2 * 1.0e6 * 238 * 4 / ((__int64)1 << 32);
         float real_hit = real_end[2].data / expected;
         usb.BoardInfo(info, true);
         float efficiency = info.core_current * metric.peakV / (f * 238 * 4 * 1.0e-6 * real_hit);
         printf("Real hit = %.3f E=%.2f J/TH", real_hit, efficiency);
         }
      printf("\n");
      }
   Pll(25, -1);
   SetVoltage(.2);
   }

void testtype::DutyTest(int phase, int *settings, float *speed, bool speedonly)
   {
   const int iterations = 10;
   const float hit = 0.95;
   int i;
   headertype h;

   usb.OLED_Display("DutyCycle\n#\n@");

   Pll(500, -1);
   SetVoltage(0.29);

   WriteConfig(E_VERSION_BOUND, 0xff0000);
   WriteConfig(E_DUTY_CYCLE, 0);
   WriteConfig(E_HASHCONFIG, (1 << 15) | (1 << 16));
   WriteConfig(E_HASHCONFIG, (1 << 15) | phase);
   for (i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0xffffffff);
   const int num_engines = 1;
   WriteConfig(E_HASHCONFIG, phase | (1 << 15) | (1 << 16));
   WriteConfig(E_HASHCONFIG, phase | (1 << 15));
   WriteConfig(E_BIST, 1);

   WriteConfig(E_SPEED_INCREMENT, pll_multiplier * 5);
   WriteConfig(E_SPEED_DELAY, 1000);
   WriteConfig(E_SPEED_UPPER_BOUND, pll_multiplier * 3000);
   WriteConfig(E_BIST_THRESHOLD, (iterations - 1) * num_engines * 8 * hit);
   WriteConfig(E_HASHCONFIG, (1 << 15) | phase | (1 << 16));
   WriteConfig(E_HASHCONFIG, (1 << 15) | phase);
   WriteConfig(E_HASHCONFIG, (1 << 15) | phase);
   WriteConfig(E_BIST, 1);

   float baseline[238], average = 0.0, average_baseline = 0.0;
   for (i = 0; i < 238; i++)
      {
      WriteConfig(E_ENGINEMASK + (i / 32), 0xffffffff ^ (1 << (i & 31)));

      float best = 0;
      int bestsetting = -1;
      int start = -64, end = 32;
      if (phase < 4) end = 0;
      if (phase >= 4) start = -32;
      if (speedonly) start = end = 0;
      for (int duty = start; duty <= end; duty+=2) {
         if (duty == 0)
            WriteConfig(E_DUTY_CYCLE, 0);
         else if (duty < 0)
            WriteConfig(E_DUTY_CYCLE, 0x8000 | (-duty + 0x80));
         else
            WriteConfig(E_DUTY_CYCLE, 0x80 | ((duty + 0x80) << 8));
         WriteConfig(E_HASHCONFIG, (1 << 15) | phase);
         WriteConfig(E_BIST_GOOD_SAMPLES, 0);
         WriteConfig(E_PLLFREQ, 600.0 * pll_multiplier);
         WriteConfig(E_BIST, iterations + (1 << 31));
         while (ReadConfig(E_BIST) != 0)
            ;
         float f = ReadConfig(E_PLLFREQ) / pll_multiplier;
         if (duty == 0) baseline[i] = f;
         if (f > best) { best = f; speed[i] = f; settings[i] = duty; }
         }
      printf("Phase %d Engine %3d Base=%.0f Tuned=%.0f setting=%d\n", phase, i, baseline[i], speed[i], settings[i]);
      average_baseline += baseline[i];
      average += speed[i];
      WriteConfig(E_ENGINEMASK + (i / 32), 0xffffffff);
      }

   Pll(25, -1);
   }

void testtype::EngineMap(const char *filename, float *map, int phase, int duty_cycle)
   {
   TAtype ta("EngineMap");
   const int iterations = 10;
   const float hit = 0.95;
   int i;
   headertype h;
   vector<batchtype> batch_start, batch_end;

   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   Load(h, 64, 0, -1);
   usb.OLED_Display("EngineMap\n#\n@");

   Pll(100);
   WriteConfig(E_VERSION_BOUND, 0xff0000);
   for (i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0xffffffff);
   const int num_engines = 1;
   WriteConfig(E_HASHCONFIG, phase | (1 << 15)| (1 << 16));
   WriteConfig(E_HASHCONFIG, phase| (1 << 15));
   WriteConfig(E_BIST, 1);

   WriteConfig(E_SPEED_INCREMENT, pll_multiplier * 5);
   WriteConfig(E_SPEED_DELAY, 1000);
   WriteConfig(E_SPEED_UPPER_BOUND, pll_multiplier * 3000);
   WriteConfig(E_BIST_THRESHOLD, (iterations - 1) * num_engines * 8 * hit);
   if (duty_cycle == 0)
      WriteConfig(E_DUTY_CYCLE, 0);
   else if (duty_cycle < 0)
      WriteConfig(E_DUTY_CYCLE, 0x8000 | (-duty_cycle + 0x80));
   else
      WriteConfig(E_DUTY_CYCLE, 0x80 | ((duty_cycle + 0x80) << 8));
   WriteConfig(E_HASHCONFIG, (1 << 15) | phase | (1 << 16));
   WriteConfig(E_HASHCONFIG, (1 << 15) | phase);
   WriteConfig(E_HASHCONFIG, (1 << 15) | phase);
   WriteConfig(E_BIST, 1);

   float average=0.0;
   
   for (i = 0; i < 238; i++)
      {
      WriteConfig(E_ENGINEMASK + (i / 32), 0xffffffff ^ (1 << (i & 31)));

      WriteConfig(E_HASHCONFIG, (1 << 15) | phase);
      WriteConfig(E_BIST_GOOD_SAMPLES, 0);
      WriteConfig(E_PLLFREQ, 100.0 * pll_multiplier);
      WriteConfig(E_BIST, iterations + (1 << 31));
      while (ReadConfig(E_BIST) != 0)
         ;
      float f = ReadConfig(E_PLLFREQ) / pll_multiplier;
      map[i] = f;
      average += f;
      WriteConfig(E_ENGINEMASK + (i / 32), 0xffffffff);
      }
   average = average / 238;

   printf("Engine map for hit %.2f phase %d duty=%d temp=%.1fC, Average f=%.0f\n", hit, phase, duty_cycle,OnDieTemp(), average);
   Pll(25, -1);

   if (filename == NULL) return;
   FILE* fptr = fopen(filename, "at");
   if (fptr == NULL) { printf("I couldn't open %s for append\n",filename); return; }
   fprintf(fptr, "Engine map for hit%.2f phase %d duty=%d temp=%.1fC Average f=%.2f\n", hit, phase, duty_cycle, OnDieTemp(), average);
   for (int row = 0; row < 19; row++)
      {
      for (i = 11; i >= 6; i--) {
         fprintf(fptr, ",%.0f", map[row * 12 + i]);
         printf(" %4.0f", map[row * 12 + i]);
         }
      fprintf(fptr, ",");
      printf(" ");
      for (i = 0; i <= 5; i++) {
         fprintf(fptr, ",%.0f", map[row * 12 + i]);
         printf(" %4.0f", map[row * 12 + i]);
         }
      fprintf(fptr, "\n");
      printf("\n");
      }
   for (i = 9; i >= 5; i--) {
      fprintf(fptr, ",%.0f", map[19 * 12 + i]);
      printf(" %4.0f", map[19 * 12 + i]);
      }
   fprintf(fptr, ",,,");
   printf("           ");
   for (i = 0; i <= 4; i++) {
      fprintf(fptr, ",%.0f", map[19 * 12 + i]);
      printf(" %4.0f", map[19 * 12 + i]);
      }
   fprintf(fptr, "\n");
   printf("\n");
   fclose(fptr);
   }


void testtype::Ecurve(const char* filename, const float temp)
   {
   TAtype ta("E Curve");

   float f;
   float f_start = 100, f_end = 800, f_incr = 25;
   float vmin = 200;
   int p;
   metrictype metric;
   plottype plot;
   char buffer[256];

   SetVoltage(0); // reset the supply in case it tripped
   Delay(1);
   SetVoltage(.200);
   for (f = f_start; f <= f_end; f += f_incr)
      {
      plot.AddData("Throughput(Mhz)", f);

      float minseen = 400;
      for (p = 0; p <= 5; p += 4)
         {
         for (int duty_cycle = 0; duty_cycle >= -48; duty_cycle -= 16) {
            ChipMetric(metric, vmin, f, p, duty_cycle, true);
            sprintf(buffer, "Phase%d_%d_%.0fC", p, duty_cycle, temp);
            plot.AddData(buffer, metric.peakE);
            minseen = MINIMUM(minseen, metric.hit50 * 1000);
            minseen = MINIMUM(minseen, metric.peakV * 1000);
            if (p >= 4) break;
            }
         }
      vmin = MAXIMUM(200, minseen - 20);
      }
   time_t seconds;
   time(&seconds);
   const struct tm* localtime_tm = localtime(&seconds);
   sprintf(buffer, "Data taken at %s", asctime(localtime_tm));
   plot.AddHeader(buffer);
   plot.AppendToFile(filename);
   }

void testtype::Ecurve2(const char* filename, const float temp, const vector<tweaktype>& tweaks)
   {
   TAtype ta("E Curve");

   float f;
   float f_start = 100, f_end = 800, f_incr = 50;
   float vmin = 200;
   int p;
   metrictype metric;
   plottype plot;
   char buffer[256];

   SetVoltage(0); // reset the supply in case it tripped
   Delay(1);
   SetVoltage(.200);
   for (f = f_start; f <= f_end; f += f_incr)
      {
      plot.AddData("Throughput(Mhz)", f);

      float minseen = 400;
      for (p = 0; p <= 7; p ++)
         {
         int duty = 0;
         if (p == 1) duty = -16;
         if (p == 2) duty = -32;
         if (p == 3) duty = -48;
         if (p<=3)
            ChipMetric(metric, vmin, f, 0, duty, true);
         if (p == 4)
            ChipMetric(metric, vmin, f, 4, duty, true);
         if (p == 5)
            ChipMetric3(metric, vmin, f, tweaks,0);
         if (p == 6)
            ChipMetric3(metric, vmin, f, tweaks, 1);
         if (p == 7)
            ChipMetric3(metric, vmin, f, tweaks, 2);
         sprintf(buffer, "Phase%d_%d_%.0fC", p, duty, temp);
         plot.AddData(buffer, metric.peakE);
         minseen = MINIMUM(minseen, metric.hit50 * 1000);
         minseen = MINIMUM(minseen, metric.peakV * 1000);
         }
      vmin = MAXIMUM(200, minseen - 20);
      }
   time_t seconds;
   time(&seconds);
   const struct tm* localtime_tm = localtime(&seconds);
   sprintf(buffer, "Data taken at %s", asctime(localtime_tm));
   plot.AddHeader(buffer);
   plot.AppendToFile(filename);
   }

void testtype::ComputeTweaks(const float hit_rate,vector<tweaktype>& tweaks)
   {
   TAtype ta("ComputeTweaks");
   const int iterations = 32;
   const int num_engines = 1;
   float v;
   int i;
   vector<batchtype> batch_start, batch_end;
   vector<speedsorttype> ss;

   SetVoltage(0);
   SetVoltage(.2);
   tweaks.clear();
   usb.OLED_Display("ComputeTweaks\n#\n@");

   WriteConfig(E_DUTY_CYCLE, 0);
   for (i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0xffffffff);
   float lastf=50, average_f=0.0;
   Pll(100, -1, false, 2); // change the pll refdivider to 2. This will make the pll lock faster at the expense of resolution
   Pll(100, -1, true, 2); // change the pll refdivider to 2. This will make the pll lock faster at the expense of resolution
   for (v = 250; v < 350; v += 2) {
      float f2, f4;
      tweaktype tweak;
      memset(&tweak, 0, sizeof(tweak));
      tweak.voltage = v;
      SetVoltage(tweak.voltage * 0.001);
      Delay(100);
      WriteConfig(E_SPEED_INCREMENT, pll_multiplier * 12.5);
      WriteConfig(E_SPEED_DELAY, 400);
      WriteConfig(E_SPEED_UPPER_BOUND, pll_multiplier * 3500);
      WriteConfig(E_BIST_THRESHOLD, (iterations - 1) * num_engines * 8 * hit_rate);
      WriteConfig(E_HASHCONFIG, (1 << 16)|(1<<3));  // soft reset miners
      WriteConfig(E_HASHCONFIG, 0 | (1 << 3));
      WriteConfig(E_BIST, 1);

      float total = 0.0, nextf=2000.0;
      ss.clear();
      for (i = 0; i < 238; i++)
         {
         // do 2 phase with -32 duty cycle
         WriteConfig(E_PLLFREQ, lastf * pll_multiplier);
         WriteConfig(E_ENGINEMASK + (i / 32), 0xffffffff ^ (1 << (i & 31)));
         WriteConfig(E_DUTY_CYCLE, 0x80a0);
         WriteConfig(E_MINER_CONFIG, (i << 16) | (0<<3) | 0);  // phase 2 with main pll
         WriteConfig(E_BIST_GOOD_SAMPLES, 0);
         WriteConfig(E_BIST, iterations + (1 << 31));
         while (ReadConfig(E_BIST) != 0)
            ;
         f2 = ReadConfig(E_PLLFREQ) / pll_multiplier;

         WriteConfig(E_DUTY_CYCLE, 0x8080);
         WriteConfig(E_MINER_CONFIG, (i << 16) | (0 << 3) | 4);  // phase 4 with main pll
         WriteConfig(E_BIST_GOOD_SAMPLES, 0);
         WriteConfig(E_PLLFREQ, lastf * pll_multiplier);
         WriteConfig(E_BIST, iterations + (1 << 31));
         while (ReadConfig(E_BIST) != 0)
            ;
         f4 = ReadConfig(E_PLLFREQ) / pll_multiplier;
         WriteConfig(E_MINER_CONFIG, (i << 16) | (1 << 3) | 4);  // phase 4 with main pll
         WriteConfig(E_ENGINEMASK + (i / 32), 0xffffffff);
         if (f2 > f4)
            tweak.phase2_map[i / 32] |= 1 << (i % 32);
         nextf = MINIMUM(f2, f4);
         average_f += f4;
         ss.push_back(speedsorttype(i, MAXIMUM(f2, f4)));
         total += MAXIMUM(f2, f4);
         }
      average_f /= 238;
      if (nextf<lastf*2+100)  // weed out crazy values
         lastf = MAXIMUM(average_f/3,nextf);
      Pll(MINIMUM(1600,lastf), -1, true, 2);  // set the clocks close to where we'll be operating to get realistic IR

      std::sort(ss.begin(), ss.end());
      int b1, b2;
      float best = 0.0;
      int best_b1, best_b2;
//      for (i = 0; i < 236; i+=4)
//         printf("[%d]=%.0f [%d]=%.0f [%d]=%.0f [%d]=%.0f \n", ss[i].engine, ss[i].speed, ss[i+1].engine, ss[i + 1].speed, ss[i+2].engine, ss[i + 2].speed, ss[i+3].engine, ss[i + 3].speed);
//      printf("[%d]=%.0f [%d]=%.0f \n", ss[i].engine, ss[i].speed, ss[i + 1].engine, ss[i + 1].speed);
      for (b1 = 0; b1 < 237; b1++)
         {
         for (b2 = b1 + 1; b2 < 238; b2++)
            {
            float ab1 = ss[b1].speed * (b2 - b1);
            float ab2 = ss[b2].speed * (238-b2);
            float ta0 = 0, ta1 = 0, ta2=0;
            for (i = 0; i < b1; i++) ta0+=ss[i].speed;
            for (i = b1; i < b2; i++) ta1 += ss[i].speed;
            for (i = b2; i < 238; i++) ta2 += ss[i].speed;
            if (ab1 + ab2 > best) {
               best = ab1 + ab2;
               best_b1 = b1;
               best_b2 = b2;
//               printf("B1=%d B2=%d B1={0/%.0f} B1-B2={%.0f/%.0f} B2={%.0f/%.0f}, total={%.0f/%.0f}\n", b1, b2, ta0, ab1, ta1, ab2, ta2, ab1 + ab2, total);
               }
            }
         }
      for (i = 0; i < 8; i++)
         tweak.pll2_map[i] = 0;
      for (i = best_b2; i < 238; i++)
         tweak.pll2_map[ss[i].engine / 32] |= 1 << (ss[i].engine % 32);
      if (v == 260)
         printf("");

//      float a = (238 - best_b2) / (238 - best_b1);
//      float b = (best_b2-best_b1) / (238 - best_b1);
      float a = (float)(238 - best_b2) / (238 - 0);
      float b = (float)(best_b2) / (238 - 0);
      float k = ss[best_b2].speed / ss[best_b1].speed;
      float x = (k - 1.0) / (a + b * k);
//      tweak.pll2_ratio = 1 + a * x;
//      tweak.pll_ratio = 1 - b * x;
      tweak.pll2_ratio = a;
      tweak.pll_ratio = b;
      tweak.b1_freq = ss[best_b1].speed;
      tweak.b2_freq = ss[best_b2].speed;

      int phase2_engines = PopCount(tweak.phase2_map[0]) + PopCount(tweak.phase2_map[1]) + PopCount(tweak.phase2_map[2]) + PopCount(tweak.phase2_map[3]) + PopCount(tweak.phase2_map[4]) + PopCount(tweak.phase2_map[5]) + PopCount(tweak.phase2_map[6]) + PopCount(tweak.phase2_map[7]);
      printf("Computing tweaks hitrate=%.2f %.0fmV, avgf=%.0f phase 2 miners=%d, pll_ratio=%.3f pll2_ratio=%.3f\n", hit_rate, tweak.voltage, average_f, phase2_engines,tweak.pll_ratio, tweak.pll2_ratio);

      tweaks.push_back(tweak);
      }
   }

void testtype::ComputeTweaks2(float f, const float hit_rate, vector<tweaktype>& tweaks)
   {
   TAtype ta("ComputeTweaks2");
   const int iterations = 32;
   const int num_engines = 1;
   float v;
   int i;
   vector<batchtype> batch_start, batch_end;
   vector<speedsorttype> ss;

   SetVoltage(0);
   SetVoltage(.2);
   tweaks.clear();
   usb.OLED_Display("ComputeTweaks\n#\n@");

   WriteConfig(E_DUTY_CYCLE, 0);
   for (i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0xffffffff);
   float lastf = 50, average_f = 0.0;
   Pll(f, -1, false, 2); // change the pll refdivider to 2. This will make the pll lock faster at the expense of resolution
   Pll(f, -1, true, 2); // change the pll refdivider to 2. This will make the pll lock faster at the expense of resolution
   for (v = 250; v < 350; v += 2) {
      float f2, f4;
      tweaktype tweak;
      memset(&tweak, 0, sizeof(tweak));
      tweak.voltage = v;
      SetVoltage(tweak.voltage * 0.001);
      Delay(100);
      WriteConfig(E_SPEED_INCREMENT, pll_multiplier * 12.5);
      WriteConfig(E_SPEED_DELAY, 400);
      WriteConfig(E_SPEED_UPPER_BOUND, pll_multiplier * 3500);
      WriteConfig(E_BIST_THRESHOLD, (iterations - 1) * num_engines * 8 * hit_rate);
      WriteConfig(E_HASHCONFIG, (1 << 16) | (1 << 3));  // soft reset miners
      WriteConfig(E_HASHCONFIG, 0 | (1 << 3));
      WriteConfig(E_BIST, 1);

      float total = 0.0, nextf = 2000.0;
      ss.clear();
      for (i = 0; i < 238; i++)
         {
         // do 2 phase with -32 duty cycle
         WriteConfig(E_PLLFREQ, lastf * pll_multiplier);
         WriteConfig(E_ENGINEMASK + (i / 32), 0xffffffff ^ (1 << (i & 31)));
         WriteConfig(E_DUTY_CYCLE, 0x80a0);
         WriteConfig(E_MINER_CONFIG, (i << 16) | (0 << 3) | 0);  // phase 2 with main pll
         WriteConfig(E_BIST_GOOD_SAMPLES, 0);
         WriteConfig(E_BIST, iterations + (1 << 31));
         while (ReadConfig(E_BIST) != 0)
            ;
         f2 = ReadConfig(E_PLLFREQ) / pll_multiplier;

         WriteConfig(E_DUTY_CYCLE, 0x8080);
         WriteConfig(E_MINER_CONFIG, (i << 16) | (0 << 3) | 4);  // phase 4 with main pll
         WriteConfig(E_BIST_GOOD_SAMPLES, 0);
         WriteConfig(E_PLLFREQ, lastf * pll_multiplier);
         WriteConfig(E_BIST, iterations + (1 << 31));
         while (ReadConfig(E_BIST) != 0)
            ;
         f4 = ReadConfig(E_PLLFREQ) / pll_multiplier;
         WriteConfig(E_MINER_CONFIG, (i << 16) | (1 << 3) | 4);  // phase 4 with main pll
         WriteConfig(E_ENGINEMASK + (i / 32), 0xffffffff);
         if (f2 > f4)
            tweak.phase2_map[i / 32] |= 1 << (i % 32);
         nextf = MINIMUM(f2, f4);
         average_f += f4;
         ss.push_back(speedsorttype(i, MAXIMUM(f2, f4)));
         total += MAXIMUM(f2, f4);
         }
      average_f /= 238;
      if (nextf < lastf * 2 + 100)  // weed out crazy values
         lastf = MAXIMUM(average_f / 3, nextf);
      Pll(MINIMUM(1600, lastf), -1, true, 2);  // set the clocks close to where we'll be operating to get realistic IR

      std::sort(ss.begin(), ss.end());
      int b1, b2;
      float best = 0.0;
      int best_b1, best_b2;
      //      for (i = 0; i < 236; i+=4)
      //         printf("[%d]=%.0f [%d]=%.0f [%d]=%.0f [%d]=%.0f \n", ss[i].engine, ss[i].speed, ss[i+1].engine, ss[i + 1].speed, ss[i+2].engine, ss[i + 2].speed, ss[i+3].engine, ss[i + 3].speed);
      //      printf("[%d]=%.0f [%d]=%.0f \n", ss[i].engine, ss[i].speed, ss[i + 1].engine, ss[i + 1].speed);
      for (b1 = 0; b1 < 237; b1++)
         {
         for (b2 = b1 + 1; b2 < 238; b2++)
            {
            float ab1 = ss[b1].speed * (b2 - b1);
            float ab2 = ss[b2].speed * (238 - b2);
            float ta0 = 0, ta1 = 0, ta2 = 0;
            for (i = 0; i < b1; i++) ta0 += ss[i].speed;
            for (i = b1; i < b2; i++) ta1 += ss[i].speed;
            for (i = b2; i < 238; i++) ta2 += ss[i].speed;
            if (ab1 + ab2 > best) {
               best = ab1 + ab2;
               best_b1 = b1;
               best_b2 = b2;
               //               printf("B1=%d B2=%d B1={0/%.0f} B1-B2={%.0f/%.0f} B2={%.0f/%.0f}, total={%.0f/%.0f}\n", b1, b2, ta0, ab1, ta1, ab2, ta2, ab1 + ab2, total);
               }
            }
         }
      for (i = 0; i < 8; i++)
         tweak.pll2_map[i] = 0;
      for (i = best_b2; i < 238; i++)
         tweak.pll2_map[ss[i].engine / 32] |= 1 << (ss[i].engine % 32);
      if (v == 260)
         printf("");

      //      float a = (238 - best_b2) / (238 - best_b1);
      //      float b = (best_b2-best_b1) / (238 - best_b1);
      float a = (float)(238 - best_b2) / (238 - 0);
      float b = (float)(best_b2) / (238 - 0);
      float k = ss[best_b2].speed / ss[best_b1].speed;
      float x = (k - 1.0) / (a + b * k);
      //      tweak.pll2_ratio = 1 + a * x;
      //      tweak.pll_ratio = 1 - b * x;
      tweak.pll2_ratio = a;
      tweak.pll_ratio = b;
      tweak.b1_freq = ss[best_b1].speed;
      tweak.b2_freq = ss[best_b2].speed;

      int phase2_engines = PopCount(tweak.phase2_map[0]) + PopCount(tweak.phase2_map[1]) + PopCount(tweak.phase2_map[2]) + PopCount(tweak.phase2_map[3]) + PopCount(tweak.phase2_map[4]) + PopCount(tweak.phase2_map[5]) + PopCount(tweak.phase2_map[6]) + PopCount(tweak.phase2_map[7]);
      printf("Computing tweaks hitrate=%.2f %.0fmV, avgf=%.0f phase 2 miners=%d, pll_ratio=%.3f pll2_ratio=%.3f\n", hit_rate, tweak.voltage, average_f, phase2_engines, tweak.pll_ratio, tweak.pll2_ratio);

      tweaks.push_back(tweak);
      }
   }


void testtype::Shahriar()
   {
   TAtype ta("Shahriar");
   bool hires = false;

   int i ;
   headertype h;

   vector<float> frequencies;
   vector<vector<float> > hits;
   vector<vector<float> > current;
   vector<batchtype> batch_start, batch_end;

   usb.OLED_Display("Shahriar\n#\n@");
   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   
   SetVoltage(.31);
   Pll(1400);
   FrequencyEstimate();
   WriteConfig(E_VERSION_BOUND, 0xff0000);
   WriteConfig(E_DUTY_CYCLE, 0);
   for (i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0);

   WriteConfig(E_HASHCONFIG, 4|(1<<16)); // 4 phase
   WriteConfig(E_HASHCONFIG, 4); // 4 phase

   Load(h, 0, 64, 0);
   response_hittype rsppacket;
   int x;
   while (((x = ReadConfig(E_SUMMARY, -1)) &7)!=0)
      if (!GetHit(rsppacket,0, -1)) 
         printf("hit error\n");

   WriteConfig(-1, E_CLKCOUNT, 0);
   WriteConfig(-1, E_GENERAL_HIT_COUNT, 0);
   WriteConfig(-1, E_GENERAL_TRUEHIT_COUNT, 0);
   WriteConfig(-1, E_DIFFICULT_HIT_COUNT, 0);

   Load(h, 0, 64, 0);

   while ((ReadConfig(E_SUMMARY, -1) & 7) != 0)
      {
      GetHit(rsppacket,0, -1);
      printf("found a hit, difficulty=%d\n",rsppacket.nbits);
      }

   ReadHeaders("data\\block_headers.txt.gz");
   vector<headertype> pruned;
   vector<int> histogram(256, 0);
   for (i = 0; i < block_headers.size(); i++) {
      const headertype& h = block_headers[i];
      if (h.ReturnZeroes() < 40);
      else if (((h.x.nonce >> 24) & 0xff) < 238)
         pruned.push_back(h);
      histogram[h.x.nonce >> 24]++;
      }
   for (i = 0; i < 256; i++)
      printf("blockchain nonce %3d = %5d %.1f%%\n", i, histogram[i], histogram[i] * 100.0 / block_headers.size());
   block_headers = pruned;
   printf("Found %d headers that are testable\n", pruned.size());
   for (int phase = 0; phase < 4; phase++) {
      vector<int> hits;
      WriteConfig(E_HASHCONFIG, phase + (1 << 17), -1);
      WriteConfig(E_SMALL_NONCE, (1 << 24) * (phase + 2), -1);
      printf("Setting phase %d\n", phase + 1);
      HeaderTest(0, hits);
      }

   }


void testtype::Shmoo(const char* filename, float temp)
   {
   TAtype ta("Shmoo");
   bool hires = false;

   int v_start = 230, v_end = 330, v_incr = 10;
   float f_start = 400, f_end = 3000, f_incr = hires ? 10 : 30;
   float v, f, period = hires ? 5000 : 1500, odt, odv;
   int i, r, index;
   headertype h;

   vector<float> frequencies;
   vector<vector<float> > hits;
   vector<vector<float> > current;
   boardinfotype info;
   vector<batchtype> batch_start, batch_end;

   usb.OLED_Display("Shmoo\n#\n@");
   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   Load(h, 64, 0, -1);
   WriteConfig(E_VERSION_BOUND, 0xff0000);
   WriteConfig(E_DUTY_CYCLE, 0);
   WriteConfig(E_HASHCONFIG, (1 << 8) | (1 << 9));
   for (i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0);

   batch_start.push_back(batchtype().Write(0, -1, E_CLKCOUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_HIT_COUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_TRUEHIT_COUNT, 0));
   batch_end.push_back(batchtype().Read(0, -1, E_CLKCOUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_GENERAL_HIT_COUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_GENERAL_TRUEHIT_COUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_BIST));

   WriteConfig(E_HASHCONFIG, 1); // 3 phase

   bool go_quick = false;
   for (v = v_start; v <= v_end; v += v_incr) {
      SetVoltage(v * 0.001);
      Delay(100);
      printf("Voltage=%.0f, odv=%.1f odt=%.1f\n", v, OnDieVoltage() * 1000.0, OnDieTemp());
      frequencies.clear();
      vector<float> crow;
      vector<float> hrow;

      for (f = f_end; f >= f_start; f -= f_incr)
         {
         frequencies.push_back(f);
         hrow.push_back(0.0);
         crow.push_back(0.0);
         }
      int perfect_count = 0;
      float trim = 0.0;
      bool lastoneperfect;
      for (f = f_start, index = hrow.size() - 1; f <= f_end; f += f_incr, index--)
         {
         Pll(f, -1);
         SetVoltage(v * 0.001);

         Delay(5);
         BatchConfig(batch_start);
         int repeats = 0;
         float raw_percentage = 0.0, percentage = 0.0;
         while (true) {
            repeats++;
            Delay(period * 700 / f * (go_quick ? 0.25 : 1.0));
            odv = OnDieVoltage();
            odt = OnDieTemp();
            usb.BoardInfo(info, true);
            BatchConfig(batch_end);
            float seconds = batch_end[0].data / 25.0e6;
            float expected = seconds * f * 1.0e6 * 254 * 4 / 3.0 / ((__int64)1 << 32);
            raw_percentage = batch_end[2].data / expected;
            percentage = (float)batch_end[2].data / MAXIMUM(batch_end[1].data, expected * 0.9);
            if (repeats >= 1 && percentage > 0.9 && batch_end[1].data == batch_end[2].data)
               {
               percentage = 1.0;
               break;
               }
            if (repeats > 1 && percentage > 0.9 && batch_end[1].data <= batch_end[2].data * 1.015) {
               percentage = 1.0;
               break;
               }
            if (batch_end[2].data == 0) break;
            if (percentage < 0.05 && repeats >= 5) break;
            if (repeats >= 5) break;
            if (info.core_current > 65.0) break;
            }
         lastoneperfect = percentage >= 1.0;
         printf("f=%4.0f repeats=%2d percentage=%5.1f%% true/gen=%5.1f/%4d odv=%.1fmV odt=%.1fC %.1fA\n", f, repeats, raw_percentage * 100.0, percentage * 100.0, batch_end[1].data, odv * 1000.0, odt, info.core_current);
         hrow[index] = MINIMUM(1.0, percentage);
         crow[index] = info.core_current;
         if (info.core_current > 65.0)
            {
            Pll(f_start, -1);
            for (index--; index >= 0; index--)
               {
               hrow[index] = 1.0;
               crow[index] = 100.0; // pick a large value since we won't actually be measuring this
               }
            break;
            }
         }
      go_quick = lastoneperfect;
      hits.push_back(hrow);
      current.push_back(crow);
      Pll(f_start, -1);
      Delay(100);
      }
   FILE* fptr = fopen(filename, "rt");
   bool first_time = false;
   if (fptr == NULL) first_time = true;
   else fclose(fptr);

   fptr = fopen(filename, "at");
   if (fptr == NULL) {
      printf("I couldn't open %s for writing\n", filename); FATAL_ERROR;
      }
   if (first_time) {
      fprintf(fptr, "Temp_ISL");
      for (i = 0; i < frequencies.size(); i++)
         fprintf(fptr, ",%.1f", frequencies[i]);
      }

   for (r = 0; r < hits.size(); r++) {
      fprintf(fptr, "\n%.0f_%d_hits", temp, v_start + v_incr * r);
      for (i = 0; i < hits[r].size(); i++)
         fprintf(fptr, ",%.3f", hits[r][i]);
      }
   fprintf(fptr, "\n");
   for (r = 0; r < current.size(); r++) {
      fprintf(fptr, "\n%.0f_%d_current", temp, v_start + v_incr * r);
      for (i = 0; i < current[r].size(); i++)
         fprintf(fptr, ",%.3f", current[r][i]);
      }
   fprintf(fptr, "\n");
   fprintf(fptr, "\n");
   fclose(fptr);
   }

void testtype::Shmoo_Setting(const char* filename, float temp)
   {
   TAtype ta("Shmoo_Setting");

   int v_start = 230, v_end = 330, v_incr=1;
   float f_start = 400, f_end = 1600, f_incr = 200;
   float v, f, period=1000, odt, odv;
   int s, s_start = 0, s_end = 64, s_incr = 1;
   int i;
   headertype h;

   vector<float> frequencies;
   vector<vector<float> > hits;
   vector<vector<float> > current;
   boardinfotype info;
   vector<batchtype> batch_start, batch_end;

   usb.SetVoltage(S_SCV, 810);
   usb.OLED_Display("Shmoo_setting\n#\n@");
   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   Load(h, 64, 0, -1);
   WriteConfig(E_VERSION_BOUND, 0xff0000);
   WriteConfig(E_DUTY_CYCLE, 0);
   WriteConfig(E_HASHCONFIG, (1 << 8) | (1 << 9));
   for (i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0);
   for (i = 0; i < 8; i++)
      WriteConfig(E_CLOCK_RETARD + i, 0x0);

   WriteConfig(E_HASHCONFIG, (0 << 8) | (1 << 9));
   for (i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0);

   batch_start.push_back(batchtype().Write(0, -1, E_CLKCOUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_HIT_COUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_TRUEHIT_COUNT, 0));
   batch_end.push_back(batchtype().Read(0, -1, E_CLKCOUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_GENERAL_HIT_COUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_GENERAL_TRUEHIT_COUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_BIST));

   FILE* fptr = fopen(filename, "rt");
   bool first_time = false;
   if (fptr == NULL) first_time = true;
   else fclose(fptr);

   fptr = fopen(filename, "at");
   if (fptr == NULL) {
      printf("I couldn't open %s for writing\n", filename); FATAL_ERROR;
      }
   if (first_time) {
      fprintf(fptr, "Voltage");
      for (s = s_start; s <= s_end; s += (s >= 24 ? s_incr * 4 : s >= 8 ? s_incr * 2 : s_incr))
         fprintf(fptr, ",%d", s);
      }

   for (f = f_start; f <= f_end; f += f_incr)
      {
      Pll(f, -1);
      fprintf(fptr, "\n%.0f_%.0f_hits", temp, f);
      v = v_start;
      for (s = s_start; s <= s_end; s += (s >= 24 ? s_incr * 4 : s>=8 ? s_incr*2 : s_incr)) {
         if (s<0)
            WriteConfig(E_HASHCONFIG, (1 << 8) | (0 << 9));
         else
            WriteConfig(E_HASHCONFIG, (1 << 8) | (1 << 9));
         WriteConfig(E_DUTY_CYCLE, labs(s) | (1 << 17) | (0 << 18));
         WriteConfig(E_DUTY_CYCLE, labs(s) | (1 << 17) | (1 << 18));
         bool good = false;
         for (; v <= v_end; v += v_incr) {
            SetVoltage(v * 0.001);
            Delay(100);
            BatchConfig(batch_start);
            for (int tries = 0; tries < 4; tries++) {
               Delay(period * 700 / f);
               odv = OnDieVoltage();
               odt = OnDieTemp();
               usb.BoardInfo(info, true);
               BatchConfig(batch_end);
               if (batch_end[2].data == 0) break;
               }
            float seconds = batch_end[0].data / 25.0e6;
            float expected = seconds * f * 1.0e6 * 254 * 4 / 3.0 / ((__int64)1 << 32);
            float raw_percentage = batch_end[2].data / expected;
            float percentage = (float)batch_end[2].data / MAXIMUM(batch_end[1].data, expected * 0.8);
            printf("f=%.0f v=%.0f, hit=%.1f\n", f, v, percentage * 100.0);
            if (percentage >= 0.97)
               {
               good = true;
               break;
               }
            if (percentage <= 0.0) v += 4;
            else if (percentage <= 0.5) v += 2;
            else if (percentage <= 0.8) v += 1;
            }
         
         printf("temp=%.0f F=%4.0fMhz S=%3d Voltage=%3.0f, odv=%5.1f odt=%5.1f\n", temp,f, s, good?v:0.0, OnDieVoltage() * 1000.0, OnDieTemp());
         fprintf(fptr, ",%.1f", good ? v:0.0);
         v -= 10;
         }
      }
   fprintf(fptr, "\n");
   fprintf(fptr, "\n");
   fclose(fptr);
   }








bool testtype::IpTest(datatype& data)
   {
   TAtype ta("IPTest");
   bool reject = false;
   int i;
   boardinfotype info;


   usb.OLED_Display("IP test\n#\n@");
   WriteConfig(E_TESTCLKOUT, (2 << 0) | (2 << 2));  // hashclk /4
   if ((int)(refclk/1.0e6) == 25)
      WriteConfig(E_IPCFG, 0x1); // ipclk is 6.25Mhz
   else if ((int)(refclk / 1.0e6) == 40)
      WriteConfig(E_IPCFG, 0x2); // ipclk is 5.0Mhz
   else if ((int)(refclk / 1.0e6) == 50)
      WriteConfig(E_IPCFG, 0x2); // ipclk is 6.25Mhz
   else if ((int)(refclk / 1.0e6) == 100)
      WriteConfig(E_IPCFG, 0x3); // ipclk is 6.25Mhz
   WriteConfig(E_TEMPCFG, 0xd);  // turn on temp sensor
   Delay(10);

   uint32 x = ReadConfig(E_TEMPERATURE);
   data.ip_temp = x;

   if (!(x >> 16)) {
      printf("ERROR: Temp sensor IP shows a fault\n");
      reject = true;
      }
   float t = topologytype().Temp(x);
   printf("Temp IP with default cal is %.1fC\n", t);
   if (!NoTempControl && fabs(t - data.temp) > 5.0) {
      printf("ERROR: Temp sensor is more than 5C from case\n");
      reject = true;
      }
   Pll(0, -1);
   float odv = 0.0;
   int v;
   for (v = 280; v < 350; v += 1) {
      usb.SetVoltage(S_CORE, v);
      Delay(20);
      if ((odv=OnDieVoltage()) >= 0.290) break;
      }
   printf("Setting isl=%dmV for %.1fmV from dvm\n", v, odv * 1000.0);
   const char* chain_name[32] = {"OFF","SVT-2fins","LVT-2fins" ,"ULVT-2fins","SVT-2fins-20nm","LVT-2fins-20nm" ,"ULVT-2fins-20nm",
                                       "SVT-3fins","LVT-3fins" ,"ULVT-3fins","SVT-3fins-20nm","LVT-3fins-20nm" ,"ULVT-3fins-20nm",
                                       "SVT-4fins","LVT-4fins" ,"ULVT-4fins","SVT-4fins-20nm","LVT-4fins-20nm" ,"ULVT-4fins-20nm",
                                       "SVT-3fins-aging","LVT-3fins-aging" ,"ULVT-3fins-aging","LVT-metal","LVT-metal", 
                                       "ELVT-inv","ELVT-nor3","ELVT-nand3","ELVT-csa","ULVT-inv","ULVT-nor3","ULVT-nand3","ULVT-csa"};
   for (int vindex = 0; vindex < 4; vindex++) {
      float v = vindex == 0 ? 260 : vindex == 1 ? 290 : vindex == 2 ? 320 : 350;
      usb.SetVoltage(S_RO, v);
      for (i = 0; i < 32; i++) {
         int window = 2; // 3=511, 2=127, 1=64, 0=31
         WriteConfig(E_DROCFG, 0x2);
         Delay(2);
         WriteConfig(E_DROCFG, 0x3);
         Delay(2);
         WriteConfig(E_DROCFG, 0x1);
         Delay(2);
         WriteConfig(E_DROCFG, 0xf | (i << 12) | (window << 24));// turn on dro sensor
         WriteConfig(E_DROCFG, 0xf | (i << 12) | (window << 24) | (4 << 4));// read status, clearing status
         WriteConfig(E_DROCFG, 0xf | (i << 12) | (window << 24) | (0 << 4));// data
         if (i >= 23) Delay(20);
         Delay(10);
         x = ReadConfig(E_DRO);
         usb.BoardInfo(info, true);
         data.ip_dro[vindex][i] = x & 0xffff;
         data.ip_dro_current[vindex][i] = info.ro_current;
         printf("%.0fmV: DRO[%d],%-16s,%4x(%5d),%.1fuA\n", v,i, chain_name[i], data.ip_dro[vindex][i], data.ip_dro[vindex][i], data.ip_dro_current[vindex][i]*1.0e6);
         //      WriteConfig(E_DROCFG, 0xf | (i << 12) | (3 << 20) | (4 << 4));// data
         //      x = ReadConfig(E_DRO);
         //      printf("DRO[%d],%-16s,%4x %s %s\n", i, chain_name[i], data.ip_dro[i],x&0x100 ? "Under Range Fault":"", x & 0x200 ? "Over Range Fault" : "");
          //     data.ip_dro[i] |= (x & 0x300) ? 0x80000000 : 0; // set bit 31 on overflow
         }
      }
   usb.SetVoltage(S_RO, 0);
   usb.SetVoltage(S_CORE, 290);
   Delay(1000);
   printf("Setting ISL=%dmV for DVM test\n", 290);
   const char* tap_name[18] = { " 0:IR VDD-top of spine"," 1:IR GND-top of spine"," 2:Low Level-top of spine",
                                " 3:IR VDD-Row7 Col5"," 4:IR GND-Row7 Col5"," 5:Low Level-Row7 Col5",
                                " 6:IR VDD-Row13 Col3"," 7:IR GND-Row13 Col3"," 8:Low Level-Row13 Col3",
                                " 9:IR VDD-Row7 Col11","10:IR GND-Row7 Col11","11:Low Level-Row7 Col11",
                                "12:IR VDD-Row11 Col14","13:IR GND-Row11 Col14","14:Low Level-Row11 Col14",
                                "15:SCV Supply","16:SCV in IP","24:Zero Offset" };
   for (int speed = 0; speed < 2; speed++) {
      float f = speed == 0 ? 0 : speed == 1 ? 1000.0 : 1500.0;
      Pll(25, -1);
      Pll(f, -1);
      for (i = 0; i < 18; i++) {
         WriteConfig(E_DVMCFG, 0x2);
         Delay(2);
         WriteConfig(E_DVMCFG, 0x3);
         Delay(2);
         WriteConfig(E_DVMCFG, 0x1);
         Delay(2);
         WriteConfig(E_DVMCFG, 0x9 | ((i == 17 ? 24 : i) << 12));
         Delay(2);
         WriteConfig(E_DVMCFG, 0x5);
         Delay(10);
         x = ReadConfig(E_VOLTAGE);
         if (speed == 0)
            data.ip_voltage_25[i] = x;
         else if (speed == 1) 
            data.ip_voltage_1000[i] = x;
         else if (speed == 2) 
            data.ip_voltage_2000[i] = x;
         else FATAL_ERROR;
         printf("%4.0fMhz Voltage[%2d],%-25s,%4x %6.2fmV\n", f, i, tap_name[i], x, topologytype().Voltage(x)*1000.0);
         }
      printf("Temp IP with default cal is %.1fC\n", topologytype().Temp(ReadConfig(E_TEMPERATURE)));
      }
   Pll(25, -1);
   usb.SetVoltage(S_CORE, 0);
   Delay(100);
   return reject;
   }

bool testtype::PllTest(datatype& data)
   {
   TAtype ta("PllTest");
   bool reject = false;
   boardinfotype info;
   int prediv=4, fb, i;

   usb.OLED_Display("PLL test\n#\n@");
   WriteConfig(E_TESTCLKOUT, 0);  // no testclk
   for (i=0; i<3; i++)
      {
      int scv = i == 0 ? 650 : i == 1 ? 750 : 850;
      usb.SetVoltage(S_SCV, scv);
      data.pll0_min[i] = data.pll1_min[i] = 0;
      for (fb = 100; fb >= 1; fb-=2) {
         WriteConfig(E_PLLFREQ, fb << 20);
         WriteConfig(E_PLLCONFIG, 0x0 | (0 << 10) | (0 << 13)); // disable the pll to reset it, it's probably wedged
         WriteConfig(E_PLLCONFIG, 0x11 | (3 << 10) | (3 << 13) | (prediv << 20));

         WriteConfig(E_PLL2, 0x0); // disable the pll to reset it, it's probably wedged
         WriteConfig(E_PLL2, 0x11 | (3 << 9) | (3 << 6) | (prediv << 12) | (fb << 20));

         Delay(1);
         uint32 x = ReadConfig(E_PLLCONFIG);
         bool nolock = true;
         if ((x >> 16) & 1)
            {
            data.pll0_min[i] = fb * refclk / prediv;
            nolock = false;
            }
         if ((x >> 17) & 1) {
            data.pll1_min[i] = fb * refclk / prediv;
            nolock = false;
            }
         if (nolock) break;
         }
      data.pll0_max[i] = data.pll1_max[i] = 0;
      for (fb = 500; fb < 2500; fb += 16) {
         WriteConfig(E_PLLFREQ, fb << 20);
         WriteConfig(E_PLLCONFIG, 0x0 | (0 << 10) | (0 << 13)); // disable the pll to reset it, it's probably wedged
         WriteConfig(E_PLLCONFIG, 0x11 | (3 << 10) | (3 << 13) | (prediv << 20));

         WriteConfig(E_PLL2, 0x0); // disable the pll to reset it, it's probably wedged
         WriteConfig(E_PLL2, 0x11 | (3 << 9) | (3 << 6) | (prediv << 12) | (fb << 20));

         Delay(1);
         uint32 x = ReadConfig(E_PLLCONFIG);
         bool nolock = true;
         if ((x >> 16) & 1)
            {
            data.pll0_max[i] = fb * refclk / prediv;
            nolock = false;
            }
         if ((x >> 17) & 1) {
            data.pll1_max[i] = fb * refclk / prediv;
            nolock = false;
            }
         if (nolock) break;
         }
      printf("SCV=%dmV %6.1fMhz < PLL0 < %6.1fMhz\n", scv, data.pll0_min[i] * 1.0e-6, data.pll0_max[i] * 1.0e-6);
      printf("SCV=%dmV %6.1fMhz < PLL1 < %6.1fMhz\n", scv, data.pll1_min[i] * 1.0e-6, data.pll1_max[i] * 1.0e-6);
      if (data.pll0_min[i] >= data.pll0_max[i] || data.pll1_min[i] >= data.pll1_max[i]) {
         reject = true;
         printf("Pll failed to lock\n");
         }
      }
   usb.SetVoltage(S_SCV, 750);
   for (i = 0; i < 5; i++) {
      float f = i == 0 ? 0 : i == 1 ? 1.0e9 : i == 2 ? 2.0e9 : i == 3 ? 4.0e9 : 8.0e9;

      WriteConfig(E_PLL2, 0x0); // disable the pll

      WriteConfig(E_PLLFREQ, f / refclk * prediv * (float)(1 << 20));
      WriteConfig(E_PLLCONFIG, 0x0 | (0 << 10) | (0 << 13)); // disable the pll to reset it, it's probably wedged
      if (f != 0)
         WriteConfig(E_PLLCONFIG, 0x11 | (3 << 10) | (3 << 13) | (prediv << 20));
      Delay(20);
      usb.BoardInfo(info, true);
      printf("VCO=%.1fGhz IO_Current=%.3fmA SCV_Current=%.3fmA lock=%d\n", f / 1.0e9, info.io_current * 1000.0, info.scv_current * 1000.0, (ReadConfig(E_PLLCONFIG) >> 16) & 1);
      data.pll_scv_current[i] = info.scv_current;
      }
   WriteConfig(E_PLLCONFIG, 0x0); // disable the pll to reset it, it's probably wedged
   Pll(1000.0, -1);
   Delay(100);
   if (FrequencyEstimate(1.0e9)) {
      printf("ERROR: The measured refclk or hashclk is more than 2% off from expected\n");
      reject = true;
      }
   return reject;
   }


bool testtype::PinTest(datatype& data, gpiotype &gpio)
   {
   TAtype ta("PinTest");
   bool reject = false;
/*   boardinfotype info;
   float v;

   usb.OLED_Display("Pin Test\n\n@");
   usb.BoardInfo(info);

   int zerosetting = 4096 * .106 / info.implied_reference;

   usb.Analog(true, zerosetting);
   gpio.thermal_trip = 1;
   usb.ReadWriteGPIO(gpio);
   for (v = data.tp.io; v >= 0; v -= 0.005)
      {
      usb.Analog(true, zerosetting + 4096 * v / info.implied_reference);
      if ((usb.ReadWriteGPIO(gpio) & 1)) break;
      }
   data.vil = v;
   for (v = 0; v <= data.tp.io; v += 0.005)
      {
      usb.Analog(true, zerosetting + 4096 * v / info.implied_reference);
      if (!(usb.ReadWriteGPIO(gpio) & 1)) break;
      }
   data.vih = v;
   printf("Thermal trip pad has VIH=%.0fmV and VIL=%.0fmV\n",data.vih*1000.0, data.vil * 1000.0);
   if (data.vih < data.vil + 0.05) {
      printf("ERROR: The deadband of the IO is too low\n");
      reject = true;
      }

   usb.Analog(true, zerosetting);
   gpio.thermal_trip = 1;
   usb.ReadWriteGPIO(gpio);
   Delay(100);
   usb.BoardInfo(info, true);
   data.iih = info.thermal_trip_current;

   gpio.thermal_trip = 0;
   usb.ReadWriteGPIO(gpio);
   int i;
   for (v = +50.0e-3, i=0; v >= -400.0e-3; v -= 50.0e-3,i++) {
      usb.Analog(true, zerosetting - 4096 * v / info.implied_reference);
      Delay(100);
      usb.BoardInfo(info,true);

      data.iil[i] = info.thermal_trip_current;
      float r = (data.tp.io - v) / info.thermal_trip_current;
      printf("V=%5.1f mV, I=%4.2f mA, Implied R=%4.2f KOhm\n",v*1000.0, info.thermal_trip_current*1000.0,r/1000.0);
      if (v == 0.0 && r < 2.5e3) {
         printf("ERROR: Pin pullup too strong(<2.5K)\n");
         reject = true;
         }
      if (v == 0.0 && r > 4.0e3) {
         printf("ERROR: Pin pullup too weak(>4K)\n");
         reject = true;
         }
      }
   usb.Analog(false, zerosetting);
*/
   return reject;
   }



// This should test that the baudrate can be changed, do some read/write of config space, measure scv power
bool testtype::ControlTest(datatype& data)
   {
   TAtype ta("ControlTest");
   bool reject = false;
   boardinfotype info;

   usb.OLED_Display("Control Test\n\n@");

   Pll(25,-1);
//   while (true) IsAlive(eval_id);
   if (!IsAlive(eval_id)) {
      printf("ERROR: Aliveness check failed after jumping to %.3fMBaud", expected_baud/1.0e6);
      return true;
      }
   if (ReadWriteStress(eval_id, 10)) {
      printf("ERROR: Read/Write stress failed\n");
      return true;
      }

   usb.BoardInfo(info,true);
   data.scv_current_25Mhz = info.scv_current;
   Pll(1000, -1);
   Delay(100);
   usb.BoardInfo(info, true);
   data.scv_current_1Ghz = info.scv_current;
   Pll(2000, -1);
   Delay(100);
   usb.BoardInfo(info, true);
   data.scv_current_2Ghz = info.scv_current;
   Pll(25, -1);

   printf("Measured scv current at 100Mhz: %.2fmA, %.3fmA/Mhz\n", data.scv_current_100Mhz * 1000.0, (data.scv_current_1Ghz - data.scv_current_25Mhz) * 1000.0 / 975.0);

   return reject;
   }

void testtype::BistSweep(datatype &data)
   {
   usb.OLED_Display("BIST Sweep\n#\n@");
   int v, index;
   boardinfotype info;
   float f = 1000.0;
   WriteConfig(E_HASHCONFIG, (1<<9)|(1<<8));
   for (int i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0);
   Pll(f,-1);
   for (v = 300,index=0; v < 450; v+=1,index++)
      {
      SetVoltage(v*0.001);
      Delay(20);
      WriteConfig(E_BIST, 0);
      usb.BoardInfo(info,true);
      int b;
      while ((b = ReadConfig(E_BIST)) & 1); // poll until bist is complete
      b = 254-((b >> 1) & 0xff)-254/2;
      float vv = OnDieVoltage();
      float eff = vv*info.core_current / (b * 4 * f / 3 * 1.0e-6);
      printf("ISL %d %.1fmV %.1fC %.1fA bist=%d Eff=%.1f\n",v,vv*1000.0,OnDieTemp(),info.core_current,b,eff);
      }
   Pll(25,-1);
   }

void testtype::CalibrateEvalDVM()
   {
   usb.SetVoltage(S_CORE, 0);
   usb.SetVoltage(S_SCV, 800);
   float v[4], avg;
   int i;

   printf("Calibrating DVM\n");
   for (i = 0; i < 4; i++)
      v[i] = OnDieVoltage(-1, S_ZERO);
   avg = (v[0] + v[1] + v[2] + v[3]) / 4;
   printf("Calib: zero setting reading %.2f, adjusting IP offset\n", avg * 1000.0);
   topologytype().ChangeDefaultVoltageParameters(1.0, -avg);
   printf("Calib: zero setting after offset adjustment, %.2fmV\n", OnDieVoltage(-1, S_ZERO) * 1000.0);

   usb.SetVoltage(S_SCV, 800); Delay(100);
   for (i = 0; i < 4; i++)
      v[i] = OnDieVoltage(-1, S_SCV);
   avg = (v[0] + v[1] + v[2] + v[3]) / 4;
   printf("Calib: scv setting to %.0fmV, reading %.1fmV, error=%.2f%%\n", 802.0, avg * 1000.0, (1.0-(avg * 1000.0 / 800.0))*100.0);
   topologytype().ChangeDefaultVoltageParameters(0.800 / avg, 0.0);

   usb.SetVoltage(S_SCV, 800);
   avg = OnDieVoltage(-1, S_SCV);
   printf("Calib: scv setting to %.0fmV, reading %.1fmV, error=%.2f%%\n", 802.0, avg * 1000.0, (1.0 - (avg / 0.8)) * 100.0);

   usb.SetVoltage(S_SCV, 500);
   avg = OnDieVoltage(-1, S_SCV);
   printf("Calib: scv setting to %.0fmV, reading %.1fmV, error=%.2f%%\n", 500.0, avg * 1000.0, (1.0 - (avg / 0.5)) * 100.0);
   usb.SetVoltage(S_SCV, 800);
   Pll(0, -1);
   usb.SetVoltage(S_CORE, 200); Delay(100);
   avg = OnDieVoltage(-1, S_CORE);
   printf("Calib: core setting to %.0fmV, reading %.1fmV, error=%.2f%%\n", 200.0, avg * 1000.0, (1.0 - (avg / 0.2)) * 100.0);
   usb.SetVoltage(S_CORE, 300); Delay(100);
   avg = OnDieVoltage(-1, S_CORE);
   printf("Calib: core setting to %.0fmV, reading %.1fmV, error=%.2f%%\n", 300.0, avg * 1000.0, (1.0 - (avg / 0.3)) * 100.0);
   usb.SetVoltage(S_CORE, 400); Delay(100);
   avg = OnDieVoltage(-1, S_CORE);
   printf("Calib: core setting to %.0fmV, reading %.1fmV, error=%.2f%%\n", 400.0, avg * 1000.0, (1.0 - (avg / 0.4)) * 100.0);
   }


bool testtype::TestPoint(float temp, const char* partname, int station, gpiotype& gpio, bool abbreviated, bool turbo)
{
    bool reject = false;
    boardinfotype info;
    datatype data;

    time(&data.seconds);
    if (strlen(partname) >= sizeof(data.partname)) FATAL_ERROR;
    strcpy(data.partname, partname);
    data.station = station;

    printf("Testpoint for %s: %.0fC\n", data.partname, temp);
    usb.SetVoltage(S_SCV, 750);
    usb.SetVoltage(S_IO, 1500);
    usb.SetVoltage(S_CORE, 290);
    ControlTest(data);

    usb.SetVoltage(S_CORE, 290);
    Pll(0, -1, false);
    Pll(0, -1, true);
    Delay(100);

    usb.BoardInfo(info, true);
    data.temp = temp;
    data.iddq_temp = OnDieTemp();
    float odv = OnDieVoltage();
    data.iddq = info.core_current;
    float iddq_normalized = data.iddq * exp2((85 - data.iddq_temp) / 25.0);
    printf("Voltage = %.1fmV Temp=%.1fC IDDQ=%.2fA Normalized to 85C=%.2fA\n", odv * 1000.0, data.iddq_temp, data.iddq, iddq_normalized);

    SetVoltage(0);
    //reject = ControlTest(data) || reject;
    //reject = PllTest(data) || reject;
    //   reject = PinTest(data, gpio) || reject;
    //reject = IpTest(data) || reject;    // do ip test last so there's more time for thermal head to settle, but before cranking up miners
    //Pll(25, -1);

    if (false) {
        ChipMetric(data.metric[0], 200, 300.0, 4); // phase 4
        ChipMetric(data.metric[1], 200, 600.0, 4); // phase 4
        ChipMetric(data.metric[2], 200, 300.0, 0); // phase 2
        ChipMetric(data.metric[3], 200, 600.0, 0); // phase 2
        ChipMetric(data.metric[4], 200, 300.0, 1); // phase 3
        ChipMetric(data.metric[5], 200, 600.0, 1); // phase 3
        ChipMetric(data.metric[6], 200, 300.0, 5); // phase 6
        ChipMetric(data.metric[7], 200, 600.0, 5); // phase 6
    }
    SetVoltage(200);
    char sfilename[256];
    sprintf(sfilename, "shmoo\\%s_enginemap.csv", partname);
    SetVoltage(0);
    //EngineMap(sfilename, data.engine_map, 0);
    //EngineMap(sfilename, data.engine_map2, 0, -32);
    //EngineMap(sfilename, data.engine_map, 1);
    //EngineMap(sfilename, data.engine_map, 1, -32);
    //EngineMap(sfilename, data.engine_map, 4);

////////////////////////////////////////////
// Leon's experiments
////////////////////////////////////////////



    std::vector<Result> myResult;

    // Tune the following to balance throughput
    // Operating point tuning.
    //////////////////////////////
    float topPercent = 0.25;
    float fdelta = 140;
    const float freq = 1150;

    // TODO (jason)
    // for a fixed base voltage, balance volt difference and freq multiple, so HR is 
    // the same for baseline and optmized runs.
    // Starting point is when baseline HR is at least 97%. Change myVolt*freqMultiple until this is achieved.
    // Changing freqMultiple will also change the throughput.
    // Consider calling RunBist(-1) which returns full chip HR.
    float myVolt = 0.290;
    float freqMultiple = 1.0;
    float baselineVoltOffset = 0.0;
    float optVoltOffset = 0.0;

    // Consider changing PLL1 settings for the fast domain to get more throughput.
    int baselineClkPhase = 0;
    int optClkPhase = 0;
    int baselineClkDC = 0x80a0;
    int fourPhaseClkDC = 0x0;
    int optClkDC = 0x0;
    //////////////////////////////

    float baselineVolt = myVolt + baselineVoltOffset;
    float optVolt = myVolt + optVoltOffset;
    float hiFreq = freq + fdelta;

    // Set test voltage
    SetVoltage(baselineVolt);
    Sleep(1000);
    int twoPhase = 0;
    int fourPhase = 4;


    // Here we are finding the fastest engines. The groups are set at 75%, 25% for a
    // 3:1 ratio. Q: Is this the best ratio?
    PerfExtremes extremes = TestEngineMap(data.engine_map, twoPhase, baselineClkDC, topPercent);
    //int N = Remove(extremes);
    int N = 0;
    PerfExtremes extremes4p8080 = TestEngineMap(data.engine_map, fourPhase, fourPhaseClkDC, topPercent);

    // Enable all engines
    for (int i = 0; i < 8; i++) {
        WriteConfig(E_ENGINEMASK + i, 0);
    }

    // Set baseline clock source parameters
    // Reset settings from previous run
    WriteConfig(E_DUTY_CYCLE, 0x80a0); // 80-20 duty cycle
    // Set base freq
    // Baseline run is at a higher frequency than 1100MHz. Because we have to generate a similar 
    // throughput as the multi-group test

    WriteConfig(E_HASHCONFIG, 1 << 16); // reset
    WriteConfig(E_HASHCONFIG, 0);
    // 2phase freq: 600-1500
    // 4phase freq: 1000-2000
    // chips done: 10, 9, 8, 7, 6, 5, 4, 3, 2, 1
    for (float freq = 1000; freq < 2000; freq += 20) {
        Pll(freq * freqMultiple, -1, false, 5);
        // Sleep(1000);
        Delay(1000);

        // two phase
      //   GetVFromBist(2, freq);

        // four phase
        WriteConfig(E_DUTY_CYCLE, 0x0);
        WriteConfig(E_HASHCONFIG, fourPhase);
        GetVFromBist(4, freq);
    }




    //int maxNum = 5;
    //buckets[0] = { NUM_ENGINES - int(extremes.bottom.size()), freq * freqMultiple, 0.0, 0.0, true};
    //turnOffBadEngines(extremes.bottom, maxNum);
    //jpth2 = GetJthAtV(buckets, extremes, baselineVolt, ignoreMask);

    // 2pll /////////////////////////////
    //  // Start of optimization run
    //  SetVoltage(optVolt);

    //  // Restor lower bucket freq
    //  Pll(freq, -1, false, 5);

    //  // Set High frequency
    //  Pll(hiFreq, -1, true, 5); // config pll2
    //  Sleep(1000);

    //  MoveToPLL2(extremes.top25percent);


    //  // Print efficiency
    //  buckets[0] = { NUM_ENGINES - N - int(extremes.top25percent.size()), freq * float(1.0), 0.0, 0.0, true };
    //  buckets[1] = { int(extremes.top25percent.size()), float(1.0) * hiFreq, 0.0, 0.0, false };
    //  Sleep(1000);
    //  ignoreMask = false;
    //  jpth = GetJthAtV(buckets, extremes, optVolt, ignoreMask);

     //        myResult[i] = { myVolt, fdelta, jpth };
     //        i += 1;
     //    }
     //}
    ////////////////////////////////////////////

     //std::sort(myResult.begin(), myResult.end(), resCompare);

     //for (const auto& r : myResult) {
     //    printf("%f, %f, %f\n", r.voltage, r.freqDiff, r.jth);
     //}

    // const char* filename = "data\\data.bin";
    // FILE* fptr = fopen(filename, "ab");
    // if (fptr == NULL) {
    //     printf("Unable to open %s for append\n", filename);
    //     fptr = fopen(filename, "ab");
    //     if (fptr == NULL) FATAL_ERROR;
    // }
    // fwrite(&data, sizeof(data), 1, fptr);
    // fclose(fptr);
    // fptr = NULL;
    // filename = "data\\data.csv";
    // fptr = fopen(filename, "ab");
    // if (fptr == NULL) {
    //     printf("Unable to open %s for append\n", filename);
    //     fptr = fopen(filename, "ab");
    //     if (fptr == NULL) FATAL_ERROR;
    // }
    // static bool first_time = true;
    // if (first_time)
    //     data.WriteToCSV(fptr, true);
    // first_time = false;
    // data.WriteToCSV(fptr);
    // fclose(fptr);
    // fptr = NULL;

    return reject;
}


void TempHandlerPlot(int head)
   {
   thermalheadtype foo("10.30.0.50",head);
   int count = 0;
   FILE* fptr = fopen(head ? "temphand2.csv": "temphand1.csv", "wt");
   int i;
   float setpoint = 10.0;
   fprintf(fptr, "time,setpoint, actual\n");
   for (int k = 0; k < 7; k++) {
      setpoint = k == 0 ? -20.0 :
         k == 1 ? 40.0 :
         k == 2 ? 60.0 :
         k == 3 ? 80.0 :
         k == 4 ? 100.0 :
         k == 5 ? 120.0 : 25.0;
      foo.SetTemp(setpoint, true);
      int last = time(NULL);
      int three = 0;
      for (i = 0; i < 180; i++)
         {
         float f = foo.ReadTemp();
         fprintf(fptr, "%d,%.1f,%.1f\n", count, setpoint, f);
         printf("%d,%.1f,%.1f\n", count, setpoint, f);
         count++;
         if (fabs(f - setpoint) < 0.71) three++;
         else three = 0;
         if (three >= 4) break;
         while (last == time(NULL));
         last = time(NULL);

         }
      }
   fclose(fptr);
   }


void testtype::PllData()
   {
   int vcosel;
   boardinfotype info;
   float f;
   FILE* fptr = fopen("pll_data.csv", "at");

   fprintf(fptr, "VCO");
   for (f = 500; f < 8000; f += 100)
      fprintf(fptr, ",%.0f", f);
   fprintf(fptr, "\n");

   for (vcosel = 0; vcosel < 4; vcosel++)
      {
      fprintf(fptr, "VCOSEL%d",vcosel);
      for (f = 500; f < 16000; f += 100)
         {
         WriteConfig(E_PLLCONFIG, 0x1c | (vcosel << 8) | (0 << 10) | (0 << 13)); // disable the pll to reset it, it's probably wedged
         WriteConfig(E_PLLCONFIG, 0x1d | (vcosel << 8) | (3 << 10) | (3 << 13));
         WriteConfig(E_PLLFREQ, f / 25 * (float)(1 << 20));
         Delay(100);
         usb.BoardInfo(info, true);
         fprintf(fptr, ",%.2f", info.io_current*1000.0);
         printf("VCOSEL=%d VCO=%5.0fMhz IO_Current=%5.2fmA SCV_Current=%5.2fmA Lock=%d\n", vcosel, f , info.io_current * 1000.0, info.scv_current * 1000.0,(ReadConfig(E_PLLCONFIG)>>16)&1);
         }
      fprintf(fptr, "\n");
      }
   fclose(fptr);
   }

void testtype::IV_Sweep()
   {
   float v;
   boardinfotype info;
   gpiotype gpio;   gpio.clear();

   usb.SetVoltage(S_CORE, 0);
   usb.SetVoltage(S_SCV, 0);
   for (v = 300; v <= 1500; v += 100)
      {
      usb.SetVoltage(S_IO, v);
      Delay(100);
      gpio.id = eval_id = 0xff;
      gpio.enable_25mhz = 1;
      gpio.enable_100mhz = 0;
      gpio.reset = 0;
      gpio.response_i = 0;
      gpio.test_mode = 0;
      gpio.thermal_trip = 1;
      usb.ReadWriteGPIO(gpio);
      gpio.reset = 1;
      Delay(100);
      usb.ReadWriteGPIO(gpio);
      Delay(200);
      usb.SetBaudRate(115200);
      Delay(200);
      usb.BoardInfo(info,true);
      printf("\nTesting IO=%.0fmV, %.2fmA",v,info.io_current*1000.0);
      }
   usb.SetVoltage(S_IO, 0);
   for (v = 200; v <= 900; v += 100)
      {
      usb.SetVoltage(S_SCV, v);
      Delay(100);
      usb.BoardInfo(info, true);
      printf("\nTesting SCV=%.0fmV, %.2fmA", v, info.scv_current * 1000.0 );
      }
   usb.SetVoltage(S_SCV, 0);
   usb.SetVoltage(S_IO, 1200);
   for (v = 100; v <= 500; v += 50)
      {
      usb.SetVoltage(S_CORE, v);
      Delay(100);
      usb.BoardInfo(info, true);
      printf("\nTesting Core=%.0fmV, %.2fmA", v, info.core_current * 1000.0);
      }
   usb.SetVoltage(S_CORE, 0);
   }

void testtype::Characterization(bool abbreviated, bool shmoo, bool turbo)
{
    TAtype ta("Characterization");
    time_t seconds;
    boardinfotype info;
    gpiotype gpio;   gpio.clear();
    char nextname[64] = "example@a0", partname[64] = "", buffer[64];
    bool reject = false;
    float lasttemp = -1.0;
    const char* ip_address = NULL;
    int station = -1;
    vector<float> temps;

    //temps.push_back(35);
    //temps.push_back(60);
    //temps.push_back(85);
 //   temps.push_back(110);
    usb.BoardInfo(info);

    usb.OLED_Display("Remove part from socket");
    usb.SetVoltage(S_CORE, 0);
    usb.SetVoltage(S_SCV, 0);
    usb.SetVoltage(S_IO, 0);
    usb.SetVoltage(S_RO, 0);
    Delay(1000);
    usb.SetCurrentLimit(100);

    printf("\n\nIs the board at a lab station? [1(thermalhead),2(thermalhead),3+ or nothing]: ");
    gets_s(buffer, sizeof(buffer));
    if (atoi(buffer) == 1) {
        ip_address = "10.30.0.50";
        station = 1;
        NoTempControl = false;
    }
    else if (atoi(buffer) == 2) {
        ip_address = "10.30.0.50";
        NoTempControl = false;
        station = 2;
    }
    else {
        NoTempControl = true;
        station = atof(buffer);
    }
    if (station == 1 || station == 2) {
        do {
            if (shmoo) {
                temps.clear();
                for (float temp = 20.0; temp <= 100.0; temp += 10.0) temps.push_back(temp);
                printf("Please enter the starting temp(just enter to continue): ");
                gets_s(buffer, sizeof(buffer));
                if (buffer[0] == 0 || buffer[0] == '\n') break;
                float start = atof(buffer);
                for (int i = 0; i < temps.size(); i++)
                {
                    if (temps[i] < start) { temps.erase(temps.begin() + i); i--; }
                }
                break;
            }
            else {
                printf("Enter any additional temperatures you want to add to the program(just enter to continue): ");
                gets_s(buffer, sizeof(buffer));
                if (buffer[0] == 0 || buffer[0] == '\n') break;
                temps.push_back(atof(buffer));
                printf("Adding %.1fC. ", temps.back());
            }
        } while (true);
        std::sort(temps.begin(), temps.end());
    }

    temps.clear();
    temps.push_back(50.0);
    sprintf(buffer, "log\\station%d_log.txt", station);
    ChangeLogfile(buffer);
    sprintf(buffer, "Station%d", station);
    usb.OLED_Display(buffer);

    // Now do zero of current sense with no parts in the socket
    usb.SetVoltage(S_CORE, 300);
    usb.SetVoltage(S_SCV, 750);
    usb.SetVoltage(S_IO, 1500);
    usb.SetVoltage(S_RO, 250);
    //   printf("Now recording current with no part in socket to zero out board loads\n");
    //   Delay(1000);
    //   usb.ZeroADC();
    //   printf("\nPress enter:");
    //   gets_s(buffer, sizeof(buffer));

    thermalheadtype thermalhead(ip_address, station == 2);
    if (!NoTempControl && false) {
        thermalhead.TurnOnOff(false);
        Delay(1000);
        thermalhead.TurnOnOff(true);
    }

    while (true) {
        usb.SetVoltage(S_CORE, 0);
        usb.SetVoltage(S_SCV, 0);
        usb.SetVoltage(S_IO, 0);
        usb.Analog(false, 0);
        gpio.id = 0;
        gpio.enable_25mhz = 1;
        gpio.enable_100mhz = 0;
        gpio.reset = 0;
        gpio.response_i = 0;
        gpio.test_mode = 0;
        gpio.thermal_trip = 0;
        usb.ReadWriteGPIO(gpio);

        if (!NoTempControl && !shmoo)
        {
            //if (thermalhead.ReadTemp()>=42.0)   // is the head is too hot, cool it down before touching parts
            //   thermalhead.SetTemp(25.0);
            //}
            usb.OLED_Display(reject ? "REJECT\nSafe" : "Safe");
            if (reject) {
                printf("\n\n\n\n***********************************************\n");
                printf("Place the part in the REJECT tray!!!\n");
                printf("***********************************************\n\n\a\a\a");
            }
            reject = false;

            // i want the part # to be xxxxxx.xxxx@A0
            while (true) {
                TAtype ta("Waiting at prompt");
                printf("Place a part in the socket. remember the tray location it came from(%s).\n", partname);
                printf("Please enter the part name(enter=%s): ", nextname);
                gets_s(buffer, sizeof(buffer));
                if (strstr(buffer, "quit") != NULL || strstr(buffer, "QUIT") != NULL) return;
                if (buffer[0] == 0) strcpy(buffer, nextname);
                strcpy(partname, buffer);
                char* chptr = strchr(buffer, '@');
                if (chptr == NULL || chptr[1] < 'a' || chptr[1] > 'z' || chptr[2] < '0' || chptr[2] > '9') {
                    //            printf("I want a part name in the format of 'anything @a0', try again\n");
                    strcpy(nextname, buffer);
                    break;
                }
                else {
                    chptr[2]++;
                    if (chptr[2] > '9') { chptr[2] = '0'; chptr[1]++; }
                    strcpy(nextname, buffer);
                    break;
                }
            }
            time(&seconds);
            const struct tm* localtime_tm = localtime(&seconds);
            printf("$$$$$$ %s", asctime(localtime_tm));  // do this for benefit of log file
            printf("sizeof(data)=%d\n", sizeof(datatype));  // do this for benefit of log file
            printf("Now testing part: %s\n", partname);

            // do a short test
            usb.OLED_Display("Short Test\n\n@");
            // these values need to be relatively close to where I zeroed the adc
            usb.SetVoltage(S_CORE, 300); printf("Setting Core to %dmV\n", 100);
            usb.SetVoltage(S_SCV, 750); printf("Setting SCV to %dmV\n", 200);
            usb.SetVoltage(S_IO, 1500); printf("Setting IO to %dmV\n", 200);
            Delay(100);
            usb.BoardInfo(info);
            printf("IO   current is %.3fmA\n", info.io_current * 1000.0);
            printf("Scv  current is %.3fmA\n", info.scv_current * 1000.0);
            printf("Core current is %.3fA\n", info.core_current);
            if (info.core_current > 20.0 || info.scv_current > 0.020 || info.io_current > 0.020) {
                printf("ERROR: I have detected a short. \nREJECT\n\a\a");
                usb.SetVoltage(S_CORE, 0);
                usb.SetVoltage(S_SCV, 0);
                usb.SetVoltage(S_IO, 0);
            }
            //      IV_Sweep();

            usb.OLED_Display("Aliveness Test\n\n@");
            usb.SetVoltage(S_CORE, 0);
            usb.SetVoltage(S_SCV, 750);
            usb.SetVoltage(S_IO, 1500);
            Delay(400);
            gpio.id = eval_id = 0x3ff;
            gpio.enable_25mhz = 1;
            gpio.enable_100mhz = 0;
            gpio.reset = 0;
            gpio.response_i = 0;
            gpio.test_mode = 0;
            gpio.thermal_trip = 1;
            usb.ReadWriteGPIO(gpio);
            gpio.reset = 1;
            Delay(400);
            usb.ReadWriteGPIO(gpio);
            Delay(200);
            usb.SetBaudRate(115200);
            Delay(200);
            //      eval_id = -1;
            if (!IsAlive(eval_id)) {
                printf("ERROR: Asic did not respond to aliveness test with id=%x.\n", eval_id);
                reject = true;
            }
            gpio.id = eval_id = 0;
            gpio.enable_25mhz = 1;
            gpio.enable_100mhz = 0;
            gpio.reset = 0;
            gpio.response_i = 0;
            gpio.test_mode = 0;
            gpio.thermal_trip = 1;
            usb.ReadWriteGPIO(gpio);
            gpio.reset = 1;
            Delay(200);
            usb.ReadWriteGPIO(gpio);
            Delay(200);
            usb.SetBaudRate(115200);
            Delay(200);
            if (!IsAlive(eval_id)) {
                printf("ERROR: Asic did not respond to aliveness test with id=%x.\n", eval_id);
                reject = true;
            }
            refclk = 25000000.0;
            SetBaud(5000000);
            Delay(500);
            if (!IsAlive(eval_id)) {
                printf("ERROR: Asic did not respond to aliveness test with id=%x.\n", eval_id);
                reject = true;
            }
            FrequencyEstimate();

            //      Shahriar();


            if (false) {
                int i;
                vector<tweaktype> tweaks, tweaks2, tweak3;
                metrictype m;
                char filename[256];
                /*
                      tweak3.push_back(tweaktype());
                      tweak3.back().pll_ratio = 0.25;
                      tweak3.back().pll2_ratio = 0.75;
                      for (i = 0; i < 8; i++)
                         tweak3.back().pll2_map[i] = 0x11111111;
                      ChipMetric3(m, 260, 450, tweak3);*/

                ComputeTweaks(0.95, tweaks);

                tweaks2 = tweaks;
                for (i = 0; i < tweaks2.size(); i++)
                    tweaks2[i].pll2_ratio = tweaks2[i].pll_ratio = 0;

                ChipMetric3(m, 260, 450, tweaks);
                ChipMetric3(m, 260, 450, tweaks2);
                ChipMetric3(m, 260, 450, tweaks);
                //sprintf(filename, "%s_mixed.csv", partname);
                //Ecurve2(filename, -1, tweaks);
          //      Ecurve2("s_mixed_95.csv", -1, tweaks2);
                return;
            }



            /*
                  int i, duty_settings[238], duty_settings2[238];
                  float speed[238];
                  metrictype metric;
                  SetVoltage(0.300);
                  DutyTest(4,duty_settings2,speed,false);
                  memset(duty_settings, 0, sizeof(duty_settings));
            //      ChipMetric(metric, 200, 600, 0, 0, true);
            //      ChipMetric(metric, 200, 600, 0, -16, true);
            //      ChipMetric(metric, 200, 600, 0, -32, true);
            //      ChipMetric(metric, 200, 600, 0, -48, true);

                  ChipMetric(metric, 200, 600, 4, true);
                  ChipMetric2(metric, 600, 4, duty_settings,  speed);
                  ChipMetric2(metric, 600, 4, duty_settings2, speed);
                  ChipMetric(metric, 200, 600, 4, true);
                  exit(0);

            */
            //      DutyTest(0);
            //      DutyTest(1);
            //      DutyTest(4);
            //      DutyTest(5);
            //      exit(0);

                  //      CalibrateEvalDVM();

            if (reject)
                ;
            else if (NoTempControl) {
                reject = TestPoint(-1, partname, station, gpio, abbreviated, turbo) || reject;
                if (shmoo || true) {
                    char filename[128];
                    sprintf(filename, "shmoo\\%s.csv", partname);
                    //            BistShmoo(filename, -1);
                    sprintf(filename, "shmoo\\%s_Eff.csv", partname);
                    Ecurve(filename, -1);
                    vector<tweaktype> tweaks;
                    ComputeTweaks(0.975, tweaks);
                    sprintf(filename, "shmoo\\%s_mixed.csv", partname);
                    Ecurve2(filename, -1, tweaks);
                }
            }
            else {
                for (int k = 0; k < temps.size(); k++) {
                    float temp = temps[k];
                    thermalhead.SetTemp(temp);
                    //            CalibrateEvalDVM();
                    TestPoint(temp, partname, station, gpio, abbreviated, turbo);
                    if (false) {
                        char filename[128];
                        //               sprintf(filename, "shmoo\\%s.csv", partname);
                        //               BistShmoo(filename, temp);
                        //               sprintf(filename, "shmoo\\%s_Eff.csv", partname);
                        //               Ecurve(filename, temp);

                        vector<tweaktype> tweaks;
                        ComputeTweaks(0.975, tweaks);
                        sprintf(filename, "shmoo\\%s_mixed.csv", partname);
                        Ecurve2(filename, temp, tweaks);
                    }
                }
                usb.SetVoltage(S_CORE, 0);
                usb.SetVoltage(S_SCV, 0);
                usb.SetVoltage(S_IO, 0);
                // thermalhead.SetTemp(30.0);
            }
        }
    }
}



