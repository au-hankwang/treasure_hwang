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

void testtype::Shmoo(const char* filename, float temp)
   {
   TAtype ta("Shmoo");
   bool hires = false;

   int v_start = 230, v_end = 330, v_incr=10;
   float f_start = 400, f_end = 1600, f_incr = hires ? 10 : 30;
   float v, f, period=hires ? 5000:1500, odt, odv;
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
   for (i = 0; i < 8; i++)
      WriteConfig(E_CLOCK_RETARD + i, 0x0);

   WriteConfig(E_HASHCONFIG, (0 << 8) | (1 << 9));
   for (i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0);
/*   WriteConfig(E_ENGINEMASK + 0, 0xaaaa5555);
   WriteConfig(E_ENGINEMASK + 1, 0xaaaa5555);
   WriteConfig(E_ENGINEMASK + 2, 0xaaaa5555);
   WriteConfig(E_ENGINEMASK + 3, 0xaaaa5555);
   WriteConfig(E_ENGINEMASK + 4, 0xaaaa5555);
   WriteConfig(E_ENGINEMASK + 5, 0xaaaa5555);
   WriteConfig(E_ENGINEMASK + 6, 0xaaaa5555);
   WriteConfig(E_ENGINEMASK + 7, 0x152a5555);*/


   batch_start.push_back(batchtype().Write(0, -1, E_CLKCOUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_HIT_COUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_TRUEHIT_COUNT, 0));
   batch_end.push_back(batchtype().Read(0, -1, E_CLKCOUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_GENERAL_HIT_COUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_GENERAL_TRUEHIT_COUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_BIST));


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
      for (f = f_start, index = hrow.size()-1; f <= f_end; f += f_incr, index--)
         {
         Pll(f, -1);
         for (; trim < 20.0; trim += 0.5)
            {
            SetVoltage((v + trim) * 0.001);
            Delay(100);
            if (OnDieVoltage() * 1000.0 >= v-0.5) break;
            }

         int setting = MINIMUM(64,MAXIMUM(0, 48000/f-32));
//         int setting = f <= 750.0 ? 32 : f <= 1000.0 ? 16 : f <= 1250.0 ? 6 : 0;
         WriteConfig(E_DUTY_CYCLE, setting | (1 << 17) | (0 << 18));
         WriteConfig(E_DUTY_CYCLE, setting | (1 << 17) | (1 << 18));
         Delay(5);
         BatchConfig(batch_start);
         int repeats = 0;
         float raw_percentage = 0.0, percentage = 0.0;
         while (true) {
            repeats++;
            Delay(period * 700 / f * (go_quick?0.25:1.0));
            odv = OnDieVoltage();
            odt = OnDieTemp();
            usb.BoardInfo(info,true);
            BatchConfig(batch_end);
            float seconds = batch_end[0].data / 25.0e6;
            float expected = seconds * f * 1.0e6 * 254 * 4 / 3.0 / ((__int64)1 << 32);
            raw_percentage = batch_end[2].data / expected;
            percentage = (float)batch_end[2].data / MAXIMUM(batch_end[1].data, expected*0.9);
            if (repeats >= 1 && percentage > 0.9 && batch_end[1].data == batch_end[2].data)
               {
               percentage = 1.0;
               break;
               }
            if (repeats>1 && percentage > 0.9 && batch_end[1].data <= batch_end[2].data * 1.015) {
               percentage = 1.0;
               break;
               }
            if (batch_end[2].data == 0) break;
            if (percentage < 0.05 && repeats >= 5) break;
            if (repeats >= 5) break;
            if (info.core_current > 65.0) break;
            }
         lastoneperfect = percentage >= 1.0;
         printf("f=%4.0f repeats=%2d percentage=%5.1f%% true/gen=%5.1f/%4d odv=%.1fmV odt=%.1fC %.1fA\n", f, repeats, raw_percentage * 100.0, percentage*100.0, batch_end[1].data,odv*1000.0,odt,info.core_current);
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
   bool first_time=false;
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
      fprintf(fptr, "\n%.0f_%d_hits",temp,v_start+v_incr*r);
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



void testtype::ChipMetric(datatype& data, bool abreviated, bool turbo)
   {
   TAtype ta("ChipMetric");
   float v, target = data.tp.core, odv, odt;
   boardinfotype info;
   int i;
   headertype h;

   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");
   Load(h, 64, 0, -1);

   usb.OLED_Display("Chip Metric\n#\n@");
   if (sizeof(data) != 6672) FATAL_ERROR;

   VmonNotConfiged = true;
   target = 0.310;
   int period = 5000;

   SetVoltage(target);
   Pll(0, -1);
   WriteConfig(E_VERSION_BOUND, 0xff0000);
   WriteConfig(E_DUTY_CYCLE, 0);
   WriteConfig(E_HASHCONFIG, (1 << 8) | (1 << 9));
   for (i = 0; i < 7; i++)
      WriteConfig(E_ENGINEMASK + i, 0xaaaa5555);
   WriteConfig(E_ENGINEMASK + i, 0x152a5555);
   for (i = 0; i < 8; i++)
      WriteConfig(E_CLOCK_RETARD + i, 0x0);
   Delay(abreviated ? 2000:5000);
/*
   for (v = target; v < .450; v += 0.001) {
      SetVoltage(v);
      Delay(10);
      if ((odv = OnDieVoltage()) >= target) break;
      }*/
   usb.BoardInfo(info, true);
   data.iddq_temp = OnDieTemp();
   odv = OnDieVoltage();
   data.iddq = info.core_current;
   float iddq_normalized = data.iddq * exp2((85 - data.iddq_temp) / 25.0);
   printf("Voltage = %.1fmV Temp=%.1fC IDDQ=%.2fA Normalized to 85C=%.2fA\n", odv * 1000.0, data.iddq_temp, data.iddq, iddq_normalized);

   float f;
   int index;
   Pll(25, -1);
   SetVoltage(0);
   // a perfect chip should return 79 hits/Ghz
   WriteConfig(E_HASHCONFIG, (1<<9));

   vector<batchtype> batch_start, batch_end;
   batch_start.push_back(batchtype().Write(0, -1, E_CLKCOUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_HIT_COUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_TRUEHIT_COUNT, 0));
   batch_start.push_back(batchtype().Write(0, -1, E_HASHCLK_COUNT, 0));
   batch_end.push_back(batchtype().Read(0, -1, E_CLKCOUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_GENERAL_HIT_COUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_GENERAL_TRUEHIT_COUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_HASHCLK_COUNT));
   batch_end.push_back(batchtype().Read(0, -1, E_BIST));

/*
   for (f = 750, v = 0.250, index = 0; f <= (abreviated ? 0 : turbo ? 1500 : 1250); f += 250, index++)
      {
      Pll(f, -1);
      if (true ||!abreviated) {
         printf("   Performance at %4.0fMhz@%3.0fC ", f, data.tp.temp > 0 ? data.tp.temp : OnDieTemp());
         bool quick = true;
         float percentage;
         WriteConfig(E_DUTY_CYCLE, 0);
         WriteConfig(E_HASHCONFIG, (1 << 8) | (1 << 9));
         for (i = 0; i < 8; i++)
            WriteConfig(E_ENGINEMASK + i, 0);

         for (; v < .400; v += 0.002) {
            SetVoltage(v);
            Delay(10);
            BatchConfig(batch_start);
            odv = OnDieVoltage();
            odt = OnDieTemp();
            Delay(period * (quick ? 0.2 : .5) * 1000 / f);
            BatchConfig(batch_end);
            float seconds = batch_end[0].data / 25.0e6;
            float expected = seconds * f * 1.0e6 * 254 * 4 / 3.0 / ((__int64)1 << 32);
            percentage = batch_end[2].data / expected;
            quick = percentage < 0.3;
            if (quick) v += 0.003;
            if (data.hit50[index] == 0.0 && percentage >= 0.50)
               data.hit50[index] = odv;
            if (data.hit95[index] == 0.0 && percentage >= 0.95)
               {
               data.hit95[index] = odv;
               data.hit95isl[index] = v;
               usb.BoardInfo(info, true);
               data.hit95current[index] = info.core_current;
               data.hit95temp[index] = OnDieTemp();
               break;
               }
            }
         float iddq_eff = data.iddq * exp2((data.hit95temp[index] - data.hit95temp[index]) / 22.0) * (data.hit95[index] / 0.31);
//         float efficiency = ((data.hit95current[index] - iddq_eff / 2) * data.hit95[index]) / (f * 1.0e6 * 254 * 4 / 3 / 2 * 1.0e-12) / 0.95;
         float efficiency = data.hit95current[index] * data.hit95[index] / (f * 1.0e6 * 254 * 4 / 3 * 1.0e-12) / 0.95;
         printf("50%% %.1fmV, 95%% %.1fmV(%.0f), Eff=%.1f J/TH\n", data.hit50[index] * 1000.0, data.hit95[index] * 1000.0, data.hit95isl[index] * 1000.0, efficiency);
         }
      }
   */
   SetVoltage(0);
   Delay(100);
   for (f = 750, v = 0.250, index = 0; f <= (abreviated ? 750 : turbo ? 1500 : 1250); f += 250, index++)
      {
      Pll(f, -1);
      printf("CM Performance at %4.0fMhz@%3.0fC", f, data.tp.temp > 0 ? data.tp.temp : OnDieTemp());
      bool quick = true;
      float percentage;
//      int setting = f<=750.0 ? 32 : f <= 1000.0 ? 16 : f <= 1250.0 ? 6 : 0;
      int setting = MINIMUM(96, MAXIMUM(0, 48000 / f - 32));
      WriteConfig(E_DUTY_CYCLE, setting | (1 << 17) | (0 << 18));
      WriteConfig(E_DUTY_CYCLE, setting | (1 << 17) | (1 << 18));
      WriteConfig(E_HASHCONFIG, (1 << 8) | (1 << 9));
      for (i = 0; i < 8; i++)
         WriteConfig(E_ENGINEMASK + i, 0);

      if (index > 0)
         v = data.CMhit95[index - 1]; // backup since we overshot things scanning for bist
      for (; v < .400; v += 0.002) {
         SetVoltage(v);
         Delay(10);
         WriteConfig(E_BIST, 0);
         Delay(1);
         BatchConfig(batch_start);
         odv = OnDieVoltage();
         odt = OnDieTemp();
         if (data.CMhit95[index])
            Delay(5);  // we're just waiting on bist at this point
         else
            Delay(period* (quick ? 0.2 : 0.5)*(abreviated?0.5:1.0) * 1000 / f);
         BatchConfig(batch_end);
         float seconds = batch_end[0].data / 25.0e6;
         float expected = seconds * f * 1.0e6 * 254 * 4 / 3.0 / ((__int64)1 << 32);
         percentage = batch_end[2].data / expected;
         quick = percentage < 0.3;
         if (quick) v += 0.003;
         if (data.CMhit50[index] == 0.0 && percentage >= 0.50)
            data.CMhit50[index] = odv;
         if (((batch_end[4].data >> 1) & 0xff) < 254 / 2) {
            data.CMbist[index] = odv;
            if (data.CMhit95[index] != 0.0)
               break;
            }
         if (data.CMhit95[index] == 0.0 && percentage >= 0.95)
            {
            data.CMhit95[index] = odv;
            data.CMhit95isl[index] = v;
            usb.BoardInfo(info, true);
            data.CMhit95current[index] = info.core_current;
            data.CMhit95temp[index] = OnDieTemp();
            if (data.CMbist[index]!=0.0)
               break;
            }
//         printf("isl=%.0f, v=%.1f hit=%.1f%% quick=%d bist=%d\n",v*1000.0,odv * 1000.0,percentage*100.0,quick, (batch_end[4].data >> 1) & 0xff);
         }
      float iddq_eff = data.iddq * exp2((data.CMhit95temp[index] - data.CMhit95temp[index]) / 22.0) * (data.CMhit95[index]/0.31);
//      float efficiency = ((data.CMhit95current[index]- iddq_eff /2) * data.CMhit95[index]) / (f * 1.0e6 * 254 * 4 / 3 / 2 * 1.0e-12) /0.95;
      float efficiency = data.CMhit95current[index] * data.CMhit95isl[index] / (f * 1.0e6 * 254 * 4 / 3 * 1.0e-12) / 0.95;
      printf("@%3.0fC 50%% %.1fmV, 95%% %.1fmV(%.0f), Eff=%.1f J/TH, BIST %.1fmV\n", data.tp.temp > 0 ? data.tp.temp : OnDieTemp(), data.CMhit50[index] * 1000.0, data.CMhit95[index] * 1000.0, data.CMhit95isl[index] * 1000.0, efficiency, data.CMbist[index] * 1000.0);
      }
   WriteConfig(E_DUTY_CYCLE, 0);
   WriteConfig(E_HASHCONFIG, (1 << 8) | (1 << 9));
   for (i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0);
   Pll(25, -1);
   SetVoltage(0);
   }


bool testtype::BistTest(datatype& data, bool quick)
   {
   TAtype ta("BistTest");
   bool reject = false;
   boardinfotype info;
   vector<batchtype> batch;
   int i, index = 0;
   int f, v;
   float odv;
   const int minf = 500;
   const float voltage = .400;

   printf("Bist test at %.0fmV\n", voltage * 1000.0);
   usb.OLED_Display("BIST (miner test)\n#\n@");
   for (v = 280; v < 550; v += 1) {
      usb.SetVoltage(S_CORE, v);
      Delay(5);
      if ((odv = OnDieVoltage()) >= voltage) break;
      }
   usb.SetVoltage(S_SCV, 900);   // I want some more headroom on other supplies to help with IR and also hashclk paths in control
   usb.SetVoltage(S_IO, 1300);

   f = 0;
   index = 0;
   Pll(f, -1);
   Delay(100);
   usb.BoardInfo(info, true);
   data.core_current[index++] = info.core_current;
   printf("Core current %.1fmV @%.0fMhz is %.2fA\n", odv * 1000.0, (float)f, info.core_current);
   Pll(25, -1);

   for (int partition = 0; partition < 4; partition++) {
      // enable all engines
      WriteConfig(E_HASHCONFIG, (1<<8));   // enable clock disable
      for (i = 0; i < 8; i++)
         batch.push_back(batchtype().Write(0, -1, E_ENGINEMASK + i, ~(0x11111111 << partition)));
      for (i = 0; i < 8; i++)
         WriteConfig(E_CLOCK_RETARD + i, 0x0);
      BatchConfig(batch);
      batch.clear();
      for (i = 0; i < 8; i++)
         batch.push_back(batchtype().Read(0, -1, E_BIST_RESULTS + i));
      v = voltage;
      for (f = minf; f <= 2000; f += 10) {
         Pll(f, -1);
         for (; v < 550; v += 1) {
            usb.SetVoltage(S_CORE, v);
            Delay(5);
            if ((odv = OnDieVoltage()) >= voltage) break;
            }

         WriteConfig(E_BIST, 0);
         while (ReadConfig(E_BIST) & 1);
         BatchConfig(batch);

         int count = 0;
         for (i = 0; i < 254; i++) {
            bool fail = (batch[i / 32].data >> (i & 31)) & 1;
            if (data.bist[i] == 0 && fail) {
               data.bist[i] = f;
               }
            }

         WriteConfig(E_HASHCONFIG, (1 << 9)| (1 << 8));   // enable duty cycle extend
         WriteConfig(E_BIST, 0);
         while (ReadConfig(E_BIST) & 1);
         BatchConfig(batch);

         for (i = 0; i < 254; i++) {
            bool fail = (batch[i / 32].data >> (i & 31)) & 1;
            if (data.bist_extended[i] == 0 && fail) {
               data.bist_extended[i] = f;
               }
            }
         }
      usb.SetVoltage(S_CORE, 0);
      }
   Pll(25.0, -1);
   int defects = 0, never_failed=0;
   float sum = 0, sum2 = 0, min = 1e10, max = 0;
   for (i = 0; i < 254; i++) {
      if (data.bist[i]== minf) defects++;
      else {
         min = MINIMUM(min, data.bist[i]);
         max = MAXIMUM(max, data.bist[i]);
         sum += data.bist[i];
         sum2 += data.bist[i] * (float)data.bist[i];
         }
      if (data.bist[i] == 0)
         never_failed++;
      }
   float stddev = sqrt((sum2 - sum * sum / (254 - defects)) / (253 - defects));
   printf("Normal BIST, %d engines were defective, %d never failed. min=%.0f,avg=%.1f,max=%.0f,std=%.2f\n",defects, never_failed, min,sum/(254-defects),max, stddev);

   defects = 0, never_failed = 0;
   sum = 0, sum2 = 0, min = 1e10, max = 0;
   for (i = 0; i < 254; i++) {
      if (data.bist_extended[i] == minf) defects++;
      else {
         min = MINIMUM(min, data.bist_extended[i]);
         max = MAXIMUM(max, data.bist_extended[i]);
         sum += data.bist_extended[i];
         sum2 += data.bist_extended[i] * (float)data.bist_extended[i];
         }
      if (data.bist_extended[i] == 0)
         never_failed++;
      }
   stddev = sqrt((sum2 - sum * sum / (254 - defects)) / (253 - defects));
   printf("Extend BIST, %d engines were defective, %d never failed. min=%.0f,avg=%.1f,max=%.0f,std=%.2f\n", defects, never_failed, min, sum / (254 - defects), max, stddev);

   usb.SetVoltage(S_SCV, 800);
   usb.SetVoltage(S_IO, 1200);

   return reject || defects>5;
   }


bool testtype::Cdyn(datatype& data, bool abbreviated, bool turbo)
   {
   TAtype ta("Cdyn");
   bool reject = false;
   boardinfotype info;
   vector<batchtype> batch;
   int index = 0, target;
   int f, v;
   float odv;
   const int minf = 100;

   usb.OLED_Display("Cdyn\n#\n@");
   FILE* fptr = fopen("data/cdyn.csv", "at");
   fprintf(fptr, "target,isl,temp");
   for (f = 0; f <= 1500; f += 100) 
      fprintf(fptr, ",%d",f);
   fprintf(fptr, "\n");


   for (target = 270; target <= 390; target += abbreviated?40:20)
      {
      cdyntype* cdyn = target == 270 ? data.cdyn270 : target == 290 ? data.cdyn290 : target == 310 ? data.cdyn310 : target == 330 ? data.cdyn330 : target == 350 ? data.cdyn350 : target == 370 ? data.cdyn370 : data.cdyn390;
      SetVoltage(0);
      Delay(10);
      Pll(25, -1);
      usb.SetVoltage(S_CORE, target);
      Delay(20);
      Pll(0, -1);
      for (v = target; v < 500; v ++) {
         usb.SetVoltage(S_CORE, v);
         Delay(2);
         if ((odv = OnDieVoltage()) >= target*0.001) break;
         }
//      printf("isl=%.1f odv=%.1f\n", v*1000.0, odv * 1000.0);

      if (fptr == NULL) FATAL_ERROR;

      f = 0;
      index = 0;
      Delay(1500);   // some time is needed for chip to cool down from previous loop
      usb.BoardInfo(info,true);
      cdyn[index].mhz = f;
      cdyn[index].voltage = odv;
      cdyn[index].core_current = info.core_current;
      index++;
      printf("IDDQ target=%dmV(isl=%dmV) is %.2fA\n", target, v, info.core_current);
      fprintf(fptr, "%s,%d,%d,%.1f",data.partname,target,v,OnDieTemp());

      for (f = 0; f <= (abbreviated?500:turbo?1500:1000); f += 100) {
         Pll(f, -1);
         for (; v < 500; v += 1) {
            usb.SetVoltage(S_CORE, v);
            Delay(2);
            if ((odv = OnDieVoltage()) >= target * 0.001) break;
            }

         Delay(10);
         usb.BoardInfo(info, true);
         if (index >= sizeof(data.core_current) / 4) FATAL_ERROR;
         data.core_current[index] = info.core_current;
         float odv = OnDieVoltage();
         float cdyn_value = (info.core_current - cdyn[0].core_current) / (f * 0.001) / cdyn[0].voltage;
         cdyn[index].mhz = f;
         cdyn[index].voltage = odv;
         cdyn[index].core_current = info.core_current;
         cdyn[index].cdyn = cdyn_value;
         if (f == 0)
            printf("Core current %.0fmV @%-4dMhz is %.2fA\n", odv * 1000.0, f, info.core_current);
         else if (f%100==0)
            printf("Core current %.0fmV @%-4dMhz is %.2fA, %.2fA/Ghz, %.2fW/Ghz-Volts^2\n", odv * 1000.0, f, info.core_current, (info.core_current - cdyn[0].core_current) / (f * 0.001), cdyn_value);
         fprintf(fptr, ",%.3f", info.core_current);
         if (f%100==0) index++;
         }
      fprintf(fptr, "\n");
      }
   Pll(25.0, -1);
   fclose(fptr);

   return false;
   }


bool testtype::SpeedTest(datatype& data, const char* filename, const char* filename2)
   {
   TAtype ta("SpeedTest");
   bool reject = false;
   int v, i, index = 0;
   float iddq = 0.0;
   const int pll_divider = 6;
   const float pll_multiplier = pll_divider / 25.0 * (1 << 20);
   const float min_frequency = 100.0;
   const int vcosel = 3;
   boardinfotype info;

   usb.OLED_Display("Speed (miner test)\n#\n@");
   // enable all engines
   usb.SetVoltage(S_CORE, 300.0);
   Pll(25, -1);
   Delay(100);
   usb.BoardInfo(info);
   iddq = info.core_current;

   WriteConfig(E_HASHCONFIG, (1<<8));
   for (i = 0; i < 8; i++)
      //   WriteConfig(E_ENGINEMASK + i, (i&1)? 0x40404040:0x01010101);
      WriteConfig(E_ENGINEMASK + i, 0);
   /*   WriteConfig(E_PLLCONFIG, 0x1d + (vcosel << 8) | ((pll_divider - 1) << 13));
   WriteConfig(E_PLLFREQ, min_frequency * pll_multiplier);
   WriteConfig(E_SPEED_DELAY, 512);    // wait 512 cycles after changing pll before starting bist
   WriteConfig(E_SPEED_INCREMENT, 1.0 * pll_multiplier);    // make speed search happen in 1Mhz increments
   WriteConfig(E_BIST_THRESHOLD, 10);
   WriteConfig(E_SPEED_UPPER_BOUND, 2000.0 * pll_multiplier);*/

   for (v = 300,i=0; v <= 450; v+=2,i+=2)
      {
      float f, lastpassingf=0;
      usb.SetVoltage(S_CORE, v);
      Delay(100);
      data.speed[i].mhz = 0;

      for (f = 50.0; f < 2000.0; f += 5.0)
         {
         Pll(f, -1);
         WriteConfig(E_BIST, 0);
         int b;
         while ((b = ReadConfig(E_BIST)) & 1);
         b = b >> 1;
         if (b > 50) break;
         lastpassingf = f;
         }
      usb.BoardInfo(info, true);
//      printf("Voltage=%.3fV\n", OnDieVoltage());
      float odv = OnDieVoltage();
      if (odv < 0.1 || odv>0.5) {
         printf("The on die voltage sense is giving a nonsense value of %.3f\n", odv);
         FATAL_ERROR;
         }
      int index = odv * 1000 - 250;
      if (index >= 0 || index < 150) {
         data.speed[index].voltage = odv;
         data.speed[index].core_current = info.core_current;
         data.speed[index].scv_io_current = info.io_current + info.scv_current;
         data.speed[index].mhz = lastpassingf;
         printf("ISL=%dmV Core=%.1f mV, %.1fA, %.1fW BIST=%.1f Mhz Temp=%.1fC Eff=%.1f\n", v,odv * 1000.0, data.speed[index].core_current, data.speed[index].Power(), data.speed[index].mhz, OnDieTemp(),data.speed[index].Efficiency());
         }
      }
   Pll(25, -1);
   FILE* fptr = fopen(filename, "rt");
   bool firsttime = fptr == NULL;
   if (fptr != NULL) fclose(fptr);
   fptr = fopen(filename, "at");
   if (fptr == NULL) { printf("I couldn't open %s for append\n", filename); FATAL_ERROR; }
   if (firsttime) {
      fprintf(fptr, "partname,temp,iddq(.3V");
      for (v = 250, i = 0; v <= 400; v++, i++)
         fprintf(fptr, ",%.3f", v*0.001);
      fprintf(fptr, "\n");
      }
   fprintf(fptr, "%s,%.1f,%.3f",data.partname,data.tp.temp<-30 ? OnDieTemp():data.tp.temp, iddq);
   for (v = 250, i = 0; v <= 400; v++, i++)
      {
      if (data.speed[i].mhz > 0)
         fprintf(fptr, ",%.2f", data.speed[i].mhz);
      else
         fprintf(fptr, ",");
      }
   fprintf(fptr, "\n");
   fclose(fptr);

   if (filename2 != NULL) {
      fptr = fopen(filename2, "rt");
      firsttime = fptr == NULL;
      if (fptr != NULL) fclose(fptr);
      fptr = fopen(filename2, "at");
      if (fptr == NULL) { printf("I couldn't open %s for append\n", filename2); FATAL_ERROR; }
      if (firsttime) {
         fprintf(fptr, "partname,temp");
         for (v = 200, i = 0; v <= 350; v++, i++)
            fprintf(fptr, ",%.3f", v * 0.001);
         fprintf(fptr, "\n");
         }
      fprintf(fptr, "%s,%.1f", data.partname, data.tp.temp);
      for (v = 200, i = 0; v <= 350; v++, i++)
         fprintf(fptr, ",%.2f", data.speed[i].Efficiency());
      fprintf(fptr, "\n");
      fclose(fptr);
      }
   return false;
   }


bool testtype::IpTest(datatype& data)
   {
   TAtype ta("IPTest");
   bool reject = false;
   int i;

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
   if (!NoTempControl && fabs(t - data.tp.temp) > 5.0) {
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
   for (i = 0; i < 32; i++) {
      int window = 2; // 3=511, 2=127, 1=64, 0=31
      WriteConfig(E_DROCFG, 0x2);
      Delay(2);
      WriteConfig(E_DROCFG, 0x3);
      Delay(2);
      WriteConfig(E_DROCFG, 0x1);
      Delay(2);
      WriteConfig(E_DROCFG, 0xf | (i << 12) | (window << 24));// turn on dro sensor
      WriteConfig(E_DROCFG, 0xf | (i << 12) | (window << 24) | (4<<4));// read status, clearing status
      WriteConfig(E_DROCFG, 0xf | (i << 12) | (window << 24) | (0 << 4));// data
      Delay(10);
      x = ReadConfig(E_DRO);
      data.ip_dro[i] = x&0xffff;
      printf("DRO[%d],%-16s,%4x(%5d)\n", i, chain_name[i], data.ip_dro[i], data.ip_dro[i]);
//      WriteConfig(E_DROCFG, 0xf | (i << 12) | (3 << 20) | (4 << 4));// data
//      x = ReadConfig(E_DRO);
//      printf("DRO[%d],%-16s,%4x %s %s\n", i, chain_name[i], data.ip_dro[i],x&0x100 ? "Under Range Fault":"", x & 0x200 ? "Over Range Fault" : "");
 //     data.ip_dro[i] |= (x & 0x300) ? 0x80000000 : 0; // set bit 31 on overflow
      }

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
      Pll(f, -1, 0);
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
   VmonNotConfiged = true;
   return reject;
   }

bool testtype::PllTest(datatype& data)
   {
   TAtype ta("PllTest");
   bool reject = false;
   boardinfotype info;
   float f;

   usb.OLED_Display("PLL test\n#\n@");
//   WriteConfig(E_TESTCLKOUT, (2<<0) | (0<<2));  // hashclk /4
   WriteConfig(E_TESTCLKOUT, 0);  // no testclk
   for (int vcosel = 0; vcosel < 4; vcosel++) {
      for (f = 1.0e9; f >= 1.0e4; f = f / 1.05) {
         int fb_mul = f / refclk * (float)(1 << 20);
         WriteConfig(E_PLLFREQ, fb_mul);
         WriteConfig(E_PLLCONFIG, 0x1c | (vcosel << 8) | (0 << 10) | (0 << 13)); // disable the pll to reset it, it's probably wedged
         WriteConfig(E_PLLCONFIG, 0x1d | (vcosel << 8) | (3 << 10) | (3 << 13));
         Delay(1);
         if ((ReadConfig(E_PLLCONFIG) >> 16) & 1)
            ;
         else 
            break;
//         if (FrequencyEstimate(f/16)) break;
         data.pll_min[vcosel] = f;
         }
      for (f = 4.0e9; f <= 5.0e10; f = f * 1.05) {
         int fb_mul = f / refclk * (float)(1 << 20);
         WriteConfig(E_PLLFREQ, fb_mul);
         WriteConfig(E_PLLCONFIG, 0x1c | (vcosel << 8) | (0 << 10) | (0 << 13)); // disable the pll to reset it, it's probably wedged
         WriteConfig(E_PLLCONFIG, 0x1d | (vcosel << 8) | (3 << 10) | (3 << 13));
         Delay(1);
         if ((ReadConfig(E_PLLCONFIG) >> 16) & 1)
            ;
         else break;
//         if (FrequencyEstimate(f / 16)) break;
         data.pll_max[vcosel] = f;
         }
      printf("vcosel=%d %.3fMhz - %.2fMhz\n", vcosel, data.pll_min[vcosel] / 1.0e6, data.pll_max[vcosel] / 1.0e6);
      if (data.pll_min[vcosel] >= data.pll_max[vcosel]) {
         reject = true;
         printf("Pll failed to lock\n");
         }
      for (int i = 0; i < 5; i++) {
         f = i == 0 ? 0 : i == 1 ? 1.0e9 : i == 2 ? 2.0e9 : i == 3 ? 4.0e9 : 8.0e9;
         WriteConfig(E_PLLFREQ, f / refclk * (float)(1 << 20));
         WriteConfig(E_PLLCONFIG, 0x1c | (vcosel << 8) | (0 << 10) | (0 << 13)); // disable the pll to reset it, it's probably wedged
         if (f!=0)
            WriteConfig(E_PLLCONFIG, 0x1d | (vcosel << 8) | (3 << 10) | (3 << 13));
         while (true) {
            Delay(20);
            usb.BoardInfo(info, true);
            if (info.io_current < 0.02 || vcosel==0) break;
            printf("VCOSEL=%d VCO=%.1fGhz IO_Current=%.3fmA SCV_Current=%.3fmA lock=%d\n", vcosel, f / 1.0e9, info.io_current * 1000.0, info.scv_current * 1000.0,(ReadConfig(E_PLLCONFIG)>>16)&1);
            FrequencyEstimate();
            }
         data.pll_io_current[i] = info.io_current;
         data.pll_scv_current[i] = info.scv_current;
         if (i >= sizeof(data.pll_io_current) / 4) FATAL_ERROR;
         if (i == 0)
            printf("VCOSEL=%d VCO=%.1fGhz IO_Current=%.3fmA SCV_Current=%.3fmA\n", vcosel, f/1.0e9, info.io_current*1000.0, info.scv_current * 1000.0);
         else 
            printf("VCOSEL=%d VCO=%.1fGhz IO_Current=%.3fmA SCV_Current=%.3fmA\n", vcosel, f / 1.0e9, info.io_current * 1000.0, info.scv_current * 1000.0);
         }
      }
   WriteConfig(E_PLLCONFIG, 0x1c | (3 << 8) | (0 << 10) | (0 << 13)); // disable the pll to reset it, it's probably wedged
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
   boardinfotype info;
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


bool testtype::TestPoint(testpointtype& tp, const char *partname, int station, gpiotype &gpio, bool abbreviated, bool turbo)
   {
   bool reject = false;
   datatype data;

   time(&data.seconds);
   data.tp = tp;
   if (strlen(partname) >= sizeof(data.partname)) FATAL_ERROR;
   strcpy(data.partname, partname);
   data.station = station;

   usb.SetVoltage(S_CORE, tp.core * 1000.0);
   usb.SetVoltage(S_SCV, tp.scv * 1000.0);
   usb.SetVoltage(S_IO, tp.io * 1000.0);
   Delay(100);

   printf("Testpoint for %s: %.0fC, %.0fmV, %.0fmV, %.0fmV\n", data.partname, data.tp.temp, data.tp.io * 1000.0, data.tp.scv * 1000.0, data.tp.core*1000.0);
   
   SetVoltage(0);
//   Cdyn(data);
//   return false;
   reject = ControlTest(data) || reject;

   reject = PllTest(data) || reject;
   reject = PinTest(data, gpio) || reject;
   reject = IpTest(data) || reject;    // do ip test last so there's more time for thermal head to settle, but before cranking up miners
   if (tp.core > 0.0) {
      Pll(25,-1);
      SetVoltage(tp.core);
      ChipMetric(data, abbreviated, turbo);
      Cdyn(data, abbreviated, turbo);
      if (!abbreviated)
         reject = BistTest(data) || reject;
      SetVoltage(0);

/*      reject = HeaderTest(0);
      reject = HeaderTest(1);
      reject = HeaderTest(2);
      reject = HeaderTest(3);*/
      }

   const char* filename = "data\\data.bin";
   FILE* fptr = fopen(filename, "ab");
   if (fptr == NULL) {
      printf("Unable to open %s for append\n",filename);
      fptr = fopen(filename, "ab");
      if (fptr == NULL) FATAL_ERROR;
      }
   fwrite(&data, sizeof(data), 1, fptr);
   fclose(fptr);
   fptr = NULL;
   filename = "data\\data.csv";
   fptr = fopen(filename, "ab");
   if (fptr == NULL) {
      printf("Unable to open %s for append\n", filename);
      fptr = fopen(filename, "ab");
      if (fptr == NULL) FATAL_ERROR;
      }
   static bool first_time = true;
   if (first_time)
      data.WriteToCSV(fptr, true);
   first_time = false;
   data.WriteToCSV(fptr);
   fclose(fptr);
   fptr = NULL;

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
   char nextname[64]="example@a0", partname[64]="", buffer[64];
   bool reject = false;
   float lasttemp = -1.0;
   const char* ip_address = NULL;
   int station=-1;
   vector<float> temps;
   testpointtype testpoints_notemp[] = { 
//         {-100,1.08,.55,0},  {-100,1.08,.65,0},  {-100,1.20,.80,0},  {-100,1.32,.90,0},
//         {-100,1.20,.80,250},{-100,1.20,.80,270},{-100,1.20,.80,290},{-100,1.20,.80,310},{-100,1.20,.80,330},{-100,1.20,.80,350},
         {-100,1.20,.80,0.310},
//         {-100,1.20,.80,250},{-100,1.20,.80,270},{-100,1.20,.80,290},{-100,1.20,.80,310},{-100,1.20,.80,330},{-100,1.20,.80,350},
         {-100,0,0,0} };// semaphore for done


   temps.push_back(35);
   temps.push_back(60);
   temps.push_back(85);
   usb.BoardInfo(info);
   if (info.implied_reference < 3.20 || info.implied_reference>3.35)
      {
      printf("The implied reference voltage of %.3fV is too far out of range. Calibration must be bad\n", info.implied_reference);
      exit(-1);
      }

   usb.Analog(true, 0);
   usb.SetVoltage(S_SCV, 800);
   usb.SetVoltage(S_IO, 1200);
   for (int i = 200; i <= 500; i += 100) {
      printf("Setting core to %d\n", i);
      usb.SetVoltage(S_CORE, i);
      Delay(500);
      usb.BoardInfo(info);
      }
   usb.Analog(false, 0);

   usb.OLED_Display("Remove part from socket");
   usb.SetVoltage(S_CORE, 0);
   usb.SetVoltage(S_SCV, 0);
   usb.SetVoltage(S_IO, 0);
   Delay(1000);
   usb.ZeroADC();
   usb.SetCurrentLimit(70);

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
            for (float temp = 30.0; temp <= 110.0; temp += 10.0) temps.push_back(temp);
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

   sprintf(buffer, "log\\station%d_log.txt", station);
   ChangeLogfile(buffer);
   sprintf(buffer, "Station%d", station);
   usb.OLED_Display(buffer);
   printf("\nPress enter:");
   gets_s(buffer, sizeof(buffer));

   thermalheadtype thermalhead(ip_address, station==2);
   if (!NoTempControl) {
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
         if (thermalhead.ReadTemp()>=42.0)   // is the head is too hot, cool it down before touching parts
            thermalhead.SetTemp(25.0);
         }
      usb.OLED_Display(reject?"REJECT\nSafe":"Safe");
      if (reject) {
         printf("\n\n\n\n***********************************************\n");
         printf("Place the part in the REJECT tray!!!\n");
         printf("***********************************************\n\n\a\a\a");
         }
      reject = false;

      // i want the part # to be xxxxxx.xxxx@A0
      while (true) {
         TAtype ta("Waiting at prompt");
         printf("Place a part in the socket. remember the tray location it came from(%s).\n",partname);
         printf("Please enter the part name(enter=%s): ",nextname);
         gets_s(buffer, sizeof(buffer));
         if (strstr(buffer, "quit") != NULL) return;
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
      usb.SetVoltage(S_CORE, 100); printf("Setting Core to %dmV\n", 100);
      usb.SetVoltage(S_SCV, 200); printf("Setting SCV to %dmV\n", 200);
      usb.SetVoltage(S_IO, 200); printf("Setting IO to %dmV\n", 200);
      Delay(100);
      usb.BoardInfo(info);
      printf("IO   current is %.3fmA\n", info.io_current * 1000.0);
      printf("Scv  current is %.3fmA\n", info.scv_current*1000.0);
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
      usb.SetVoltage(S_SCV, 800);
      usb.SetVoltage(S_IO, 1200);
      Delay(400);
      gpio.id = eval_id = 0xff;
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
         printf("ERROR: Asic did not respond to aliveness test with id=%x.\n",eval_id);
         reject = true;
         }
      refclk = 25000000.0;
      SetBaud(3000000);
      Delay(500);
      if (!IsAlive(eval_id)) {
         printf("ERROR: Asic did not respond to aliveness test with id=%x.\n", eval_id);
         reject = true;
         }
      FrequencyEstimate();
      CalibrateEvalDVM();

      if (reject)
         ;
      else if (NoTempControl) {
         for (int p = 0; testpoints_notemp[p].io != 0.0 && !reject; p++)
            {
            if (!shmoo)
               reject = TestPoint(testpoints_notemp[p], partname, station, gpio, abbreviated, turbo) || reject;
            }
         if (shmoo) {
            char filename[128];
            sprintf(filename, "shmoo\\%s.csv", partname);
//            Shmoo_Setting(filename, 0);
            Shmoo(filename, 0);
            }
         }
      else {
         testpointtype tp;
         for (int k = 0; k < temps.size(); k++) {
            tp.temp = temps[k];
            thermalhead.SetTemp(tp.temp);
            tp.core = .310;
            tp.io = 1.2;
            tp.scv = 0.8;
            CalibrateEvalDVM();
            if (shmoo) {
               char filename[128];
               sprintf(filename, "shmoo\\%s.csv", partname);
//               Shmoo_Setting(filename, tp.temp);
               Shmoo(filename, tp.temp);
               }
            else
               TestPoint(tp, partname, station, gpio, abbreviated, turbo);
            }
         usb.SetVoltage(S_CORE, 0);
         usb.SetVoltage(S_SCV, 0);
         usb.SetVoltage(S_IO, 0);
         thermalhead.SetTemp(30.0);
         }
      }
   }



