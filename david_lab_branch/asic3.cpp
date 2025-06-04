#include "pch.h"
#include "helper.h"
#include "miner.h"
#include "dvfs.h"
#include "serial.h"
#include "usb.h"
#include "test.h"
#include "characterization.h"
#include "multithread.h"



float testtype::ReportEfficiency(float supply, bool quiet)
   {
   float ir = supply;
   int totalpassing = 0, i;
   float passing_f = 0.0;
   for (i = 0; i < found.size(); i++)
      {
      WriteConfig(E_BIST, 0, found[i]);
      int x = ReadConfig(E_PLLFREQ,found[i]);
      float f = x / pll_multiplier;
      int timeout = 100, b;
      while (((b = ReadConfig(E_BIST, found[i])) & 1) && timeout)
         timeout--;
      if (timeout <= 0) printf("bist timed out, check the core supply!\n");
      b = (b >> 1) & 0xff;
      b = 254 - b;
      passing_f += b * f;
      float odv = OnDieVoltage(found[i]);
      float odt = OnDieTemp(found[i]);
      ir -= odv;
      if (!quiet) printf("V=%5.1fmV T=%5.1fC BIST=%3d,  ", odv * 1000.0, odt, b);
      }
   boardinfotype info;
   usb.BoardInfo(info, true);
   float eff = (supply - ir) * info.core_current / (passing_f * 4 / 3.0) * 1.0e6;
   if (!quiet) printf("IR=%.1fmV %4.1fA Eff=%.1fJ/TH\n", ir * 1000.0, info.core_current, eff);
   return eff;
   }


void testtype::Asic3()
   {
   boardinfotype info;
   gpiotype gpio;   gpio.clear();

   usb.BoardInfo(info);
   gpio.reset = 0;
   gpio.enable_25mhz = 1;
   usb.ReadWriteGPIO(gpio);
   Delay(100);
   gpio.reset = 1;
   usb.ReadWriteGPIO(gpio);
   usb.OLED_Display("Asic3 Testing\n#\n@");
   usb.SetVoltage(S_IO, 1200);
   usb.SetVoltage(S_SCV, 800);
   usb.SetBaudRate(115200);
   Pll(25, -1);
   if (!IsAlive(0)) {
      printf("ERROR: Asic did not respond to aliveness test with id=%x.\n", 0);
      }
   SetBaud(3000000);
   if (!IsAlive(0)) {
      printf("ERROR: Asic did not respond to aliveness test with id=%x.\n", 0);
      }
   BoardEnumerate();
   for (int i = 0; i < found.size(); i++)
      {
      printf("Found asic with id %d\n", found[i]);
      }
   SetVoltage(1.0);
   usb.BoardInfo(info);

   int i, b;
   float v, f;
   batchtype batch;
//   WriteConfig(E_HASHCONFIG, (1 << 9));
   Pll(25, -1);
   Pll(500, -1);
   FrequencyEstimate();
   for (i = 0; i < found.size(); i++)
      {
      float odv = OnDieVoltage(found[i]);
      float odt = OnDieTemp(found[i]);
      printf("V=%5.1fmV T=%5.1fC BIST=%3d,  ", odv * 1000.0, odt);
      }

   SetVoltage(1.0);
   Pll(500, -1);
   ReportEfficiency(1.0);
   Paredo("data/paredo_10v.csv");
   SetVoltage(0.90);
   Paredo("data/paredo_09v.csv");
   SetVoltage(1.00);
   Paredo("data/paredo_11v.csv");
   Pll(25, -1);
   exit(-1);





   for (v = 0.800; v <= 1.100; v += .050)
      {
      SetVoltage(v);
      Delay(200);
      float besteff = 10000;
      for (f = 50; f < 1500; f += 25)
         {
         Pll(f, -1); Delay(10);
         printf("Supply %4.2fV %4.0fMhz:  ", v,f);
         bool allfail = true;
         float ir = v;
         int totalpassing=0;
         for (i = 0; i < found.size(); i++)
            {
            WriteConfig(E_BIST, 0, found[i]);
            int timeout = 100;
            while (((b = ReadConfig(E_BIST, found[i])) & 1) && timeout)
               timeout--;
            if (timeout <= 0) printf("bist timed out, check the core supply!\n");
            b = (b >> 1)&0xff;
            b = 254 - b;
            allfail = allfail && (b == 0);
            totalpassing += b;
            float odv = OnDieVoltage(found[i]);
            ir -= odv;
            printf("V=%5.1fmV T=%5.1fC BIST=%3d,  ", odv*1000.0, OnDieTemp(found[i]), b);
            }
         usb.BoardInfo(info, true);
         float eff = (v - ir) * info.core_current / (f * totalpassing * 4 / 3.0) * 1.0e6;
         printf("IR=%.1fmV %4.1fA Eff=%.1fJ/TH\n",ir*1000.0,info.core_current,eff);
         if (eff < besteff) besteff = eff;
         if (allfail) break;
         }
      printf("Best efficiency = %.1fJ/TH\n", besteff);
      }
   Pll(25, -1);

//   QuickDVFS(1.0);
   Pll(25, -1);
   }






void testtype::DavidExperiment()
   {
   gpiotype gpio; gpio.clear();
   boardinfotype info;
   datatype data;

   printf("Starting cdyn testing\n");

//   thermalheadtype temphead("10.30.0.50", 0);
   
   usb.OLED_Display("David Testing\n#\n@");
   usb.SetVoltage(S_IO, 0);
   usb.SetVoltage(S_SCV, 0);
   usb.SetVoltage(S_CORE, 0);
   Delay(10);
   usb.ZeroADC();
   usb.BoardInfo(info);
   usb.SetVoltage(S_IO, 1200);
   usb.SetVoltage(S_SCV, 800);
   gpio.reset = 1;
   usb.ReadWriteGPIO(gpio);
   Delay(200);
   gpio.reset = 0;
   gpio.enable_25mhz = 1;
   usb.ReadWriteGPIO(gpio);
   Delay(200);
   gpio.reset = 1;
   usb.ReadWriteGPIO(gpio);
   usb.ReadWriteGPIO(gpio);
   Delay(200);
   usb.SetBaudRate(115200);
   Delay(200);
   if (!IsAlive(0)) {
      printf("ERROR: Asic did not respond to aliveness test with id=%x.\n", 0);
      exit(-1);
      }
   SetBaud(3000000);
   Delay(300);
   if (!IsAlive(0)) {
      printf("ERROR: Asic did not respond to aliveness test with id=%x.\n", 0);
      exit(-1);
      }
   Pll(25, -1);
   BoardEnumerate();
   usb.SetVoltage(S_CORE, 300);
   usb.SetVoltage(S_SCV, 800);
   int i;


   Cdyn(data,false);
   ChipMetric(data, false, false); data.CMhit95[0] = data.CMhit95[1] = data.CMhit95[2] = data.CMhit95[3] = 0;
//   ChipMetric(data, false); data.CMhit95[0] = data.CMhit95[1] = data.CMhit95[2] = data.CMhit95[3] = 0;
//   ChipMetric(data, true);
   return;

   float f, voltage = .300, t;
   vector<batchtype> batch_start, batch_end;
   const int period=10000;
   const float f_start = 100, f_end = 1900, f_incr = 50;

   WriteConfig(E_HASHCONFIG, (1 << 8) | (1 << 9));
/*
   for (i = 0; i < 7; i++)
      WriteConfig(E_ENGINEMASK + i, 0xaaaa5555);
   WriteConfig(E_ENGINEMASK + i, 0x152a5555);*/
   WriteConfig(E_DUTY_CYCLE, 0);

   for (t = -40.0; t <= 100.0; t += 10)
      {
//      temphead.SetTemp(t);

      for (voltage = .260+(t<=60?.02:.0); voltage <= 0.33 + (t <= 60 ? .02 : .0); voltage += 0.010)
         {
         SetVoltage(voltage);

         int scenarios = 2;
         vector<float> zero;
         vector<vector<float> > plots;
         vector<float> power;
         vector<bool> goquick(scenarios, false);
         int index, b, p;

         for (f = f_start; f <= f_end; f += f_incr)
            zero.push_back(0.0);
         for (i = 0; i < scenarios * 3; i++)
            plots.push_back(zero);

         WriteConfig(E_VERSION_BOUND, 0xff0000);
         batch_start.push_back(batchtype().Write(0, -1, E_CLKCOUNT, 0));
         batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_HIT_COUNT, 0));
         batch_start.push_back(batchtype().Write(0, -1, E_GENERAL_TRUEHIT_COUNT, 0));
         batch_end.push_back(batchtype().Read(0, -1, E_CLKCOUNT));
         batch_end.push_back(batchtype().Read(0, -1, E_GENERAL_HIT_COUNT));
         batch_end.push_back(batchtype().Read(0, -1, E_GENERAL_TRUEHIT_COUNT));
         for (f = f_start, index = 0; f <= f_end; f += f_incr, index++)
            {
            Pll(f, -1);
            for (p = 0; p < scenarios; p++)
               {
               float scaling = 1.0;
               if (p == 0) {
                  WriteConfig(E_DUTY_CYCLE, 0);
                  WriteConfig(E_HASHCONFIG, (1 << 8) | (1 << 9));
                  for (i = 0; i < 8; i++)
                     WriteConfig(E_ENGINEMASK + i, 0);
                  for (i = 0; i < 8; i++)
                     WriteConfig(E_CLOCK_RETARD + i, 0x0);
                  }
               else if (p == 1) {
                  WriteConfig(E_DUTY_CYCLE, 8 | (1 << 17) | (0 << 18));
                  WriteConfig(E_DUTY_CYCLE, 8 | (1 << 17) | (1 << 18));
                  WriteConfig(E_HASHCONFIG, (1 << 8) | (1 << 9));
                  for (i = 0; i < 7; i++)
                     WriteConfig(E_ENGINEMASK + i, 0xaaaa5555);
                  WriteConfig(E_ENGINEMASK + i, 0x152a5555);
                  scaling = 2.0;
                  }
               else if (p == 2) {
                  WriteConfig(E_DUTY_CYCLE, 0);
                  WriteConfig(E_HASHCONFIG, (1 << 8) | (0 << 9));
                  for (i = 0; i < 8; i++)
                     WriteConfig(E_ENGINEMASK + i, 0);
                  for (i = 0; i < 8; i++)
                     WriteConfig(E_CLOCK_RETARD + i, 0xffffffff);
                  }
               else if (p == 3) {
                  WriteConfig(E_DUTY_CYCLE, 0);
                  WriteConfig(E_HASHCONFIG, (1 << 8) | (0 << 9));
                  for (i = 0; i < 8; i++)
                     WriteConfig(E_ENGINEMASK + i, 0);
                  for (i = 0; i < 8; i++)
                     WriteConfig(E_CLOCK_RETARD + i, 0x0);
                  }
               else FATAL_ERROR;

               WriteConfig(E_BIST, 0);
               while ((b = ReadConfig(E_BIST)) & 1);
               if (p != 1)
                  b = 254 - ((b >> 1) & 0xff);
               else
                  b = 128 - ((b >> 1) & 0xff);
               plots[p][index] = (b / 254.0)*scaling;

               BatchConfig(batch_start);
               Delay(period * sqrt(1000 / f) * (goquick[p] ? 0.05 : 1.0));
               BatchConfig(batch_end);
               float seconds = batch_end[0].data * 40.0e-9;
               float expected = seconds * f * 254 * 4 / 3.0 * 1.0e6 / ((__int64)1 << 32);
               plots[p + scenarios * 1][index] = (batch_end[2].data / expected)* scaling;
               plots[p + scenarios * 2][index] = (batch_end[1].data / expected)* scaling;

               if (p == 0) {
                  usb.BoardInfo(info, true);
                  float odv = OnDieVoltage();
                  float odt = OnDieTemp();
                  power.push_back(info.core_current * odv);
                  printf("Testing %.0fMhz, odv=%.0fmV ODT=%.1fC Power=%.1fW, hits=%d, goquick=%d\n", f, odv,odt, info.core_current * odv, batch_end[2].data,goquick[p]?1:0);
                  goquick[p] = batch_end[2].data == 0;
                  }
               }
            }
         FILE* fptr = fopen("data/paredo.csv", "at");
         float odt = OnDieTemp();
         if (fptr == NULL) FATAL_ERROR;
         for (f = f_start, index = 0; f <= f_end; f += f_incr, index++)
            {
            fprintf(fptr, ",%.0f", f);
            }
         fprintf(fptr, "\n");
         for (p = 0; p < scenarios * 2; p++)
            {
            int pmod = p % scenarios;
            if (p < scenarios)
               fprintf(fptr, "Bist%s_%.0f_%.0fC", pmod == 0 ? "0" : pmod == 1 ? "8/127" : pmod == 2 ? "retard" : "noduty", voltage * 1000, t);
            else if (p < scenarios * 2)
               fprintf(fptr, "True%s_%.0f_%.0fC", pmod == 0 ? "0" : pmod == 1 ? "8/127" : pmod == 2 ? "retard" : "noduty", voltage * 1000, t);
            else
               fprintf(fptr, "All%s_%.0f_%.0fC", pmod == 0 ? "0" : pmod == 1 ? "8/127" : pmod == 2 ? "retard" : "noduty", voltage * 1000, t);

            for (f = f_start, index = 0; f <= f_end; f += f_incr, index++)
               {
               fprintf(fptr, ",%.3f", plots[p][index]);
               }
            fprintf(fptr, "\n");
            }
         fprintf(fptr, "Power_%.0f_%.0fC", voltage * 1000, t);
         for (f = f_start, index = 0; f <= f_end; f += f_incr, index++)
            {
            fprintf(fptr, ",%.3f", power[index]);
            }
         fprintf(fptr, "\n");
         fprintf(fptr, "Efficiency_%.0f_%.0fC", voltage * 1000, t);
         for (f = f_start, index = 0; f <= f_end; f += f_incr, index++)
            {
            fprintf(fptr, ",%.2f", power[index] / (f * plots[scenarios][index] * 0.000338667));
            }
         fprintf(fptr, "\n");
         fclose(fptr);
         }
      }
   
   usb.SetVoltage(S_IO, 0);
   usb.SetVoltage(S_SCV, 0);
   SetVoltage(0);
   usb.OLED_Display("SAFE");
   }

