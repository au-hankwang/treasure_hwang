#include "pch.h"
#include "helper.h"
#include "miner.h"
#include "dvfs.h"
#include "serial.h"
#include "usb.h"
#include "test.h"
#include "characterization.h"
#include "multithread.h"





void testtype::SimpleSystem()
   {
   vector<topologytype> topology_1board{ topologytype(0, 0, 0), topologytype(0, 1, 1), topologytype(0, 2, 2), topologytype(1, 0, 128), topologytype(1, 1, 129), topologytype(1, 2, 130), topologytype(2, 0, 3), topologytype(2, 1, 4), topologytype(2, 2, 5), topologytype(3, 0, 131), topologytype(3, 1, 132), topologytype(3, 2, 133), topologytype(4, 0, 6), topologytype(4, 1, 7), topologytype(4, 2, 8), topologytype(5, 0, 134), topologytype(5, 1, 135), topologytype(5, 2, 136), topologytype(6, 0, 9), topologytype(6, 1, 10), topologytype(6, 2, 11), topologytype(7, 0, 137), topologytype(7, 1, 138), topologytype(7, 2, 139), topologytype(8, 0, 12), topologytype(8, 1, 13), topologytype(8, 2, 14), topologytype(9, 0, 140), topologytype(9, 1, 141), topologytype(9, 2, 142), topologytype(10, 0, 15), topologytype(10, 1, 16), topologytype(10, 2, 17), topologytype(11, 0, 143), topologytype(11, 1, 144), topologytype(11, 2, 145), topologytype(12, 0, 18), topologytype(12, 1, 19), topologytype(12, 2, 20), topologytype(13, 0, 146), topologytype(13, 1, 147), topologytype(13, 2, 148), topologytype(14, 0, 21), topologytype(14, 1, 22), topologytype(14, 2, 23), topologytype(15, 0, 149), topologytype(15, 1, 150), topologytype(15, 2, 151), topologytype(16, 0, 24), topologytype(16, 1, 25), topologytype(16, 2, 26), topologytype(17, 0, 152), topologytype(17, 1, 153), topologytype(17, 2, 154), topologytype(18, 0, 27), topologytype(18, 1, 28), topologytype(18, 2, 29), topologytype(19, 0, 155), topologytype(19, 1, 156), topologytype(19, 2, 157), topologytype(20, 0, 30), topologytype(20, 1, 31), topologytype(20, 2, 32), topologytype(21, 0, 158), topologytype(21, 1, 159), topologytype(21, 2, 160), topologytype(22, 0, 33), topologytype(22, 1, 34), topologytype(22, 2, 35), topologytype(23, 0, 161), topologytype(23, 1, 162), topologytype(23, 2, 163), topologytype(24, 0, 36), topologytype(24, 1, 37), topologytype(24, 2, 38), topologytype(25, 0, 164), topologytype(25, 1, 165), topologytype(25, 2, 166), topologytype(26, 0, 39), topologytype(26, 1, 40), topologytype(26, 2, 41), topologytype(27, 0, 167), topologytype(27, 1, 168), topologytype(27, 2, 169), topologytype(28, 0, 42), topologytype(28, 1, 43), topologytype(28, 2, 44), topologytype(29, 0, 170), topologytype(29, 1, 171), topologytype(29, 2, 172), topologytype(30, 0, 45), topologytype(30, 1, 46), topologytype(30, 2, 47), topologytype(31, 0, 173), topologytype(31, 1, 174), topologytype(31, 2, 175), topologytype(32, 0, 48), topologytype(32, 1, 49), topologytype(32, 2, 50), topologytype(33, 0, 176), topologytype(33, 1, 177), topologytype(33, 2, 178), topologytype(34, 0, 51), topologytype(34, 1, 52), topologytype(34, 2, 53), topologytype(35, 0, 179), topologytype(35, 1, 180), topologytype(35, 2, 181), topologytype(36, 0, 54), topologytype(36, 1, 55), topologytype(36, 2, 56), topologytype(37, 0, 182), topologytype(37, 1, 183), topologytype(37, 2, 184), topologytype(38, 0, 57), topologytype(38, 1, 58), topologytype(38, 2, 59), topologytype(39, 0, 185), topologytype(39, 1, 186), topologytype(39, 2, 187), topologytype(40, 0, 60), topologytype(40, 1, 61), topologytype(40, 2, 62), topologytype(41, 0, 188), topologytype(41, 1, 189), topologytype(41, 2, 190), topologytype(42, 0, 63), topologytype(42, 1, 64), topologytype(42, 2, 65), topologytype(43, 0, 191), topologytype(43, 1, 192), topologytype(43, 2, 193) };
   vector<topologytype> topology_system;
   gpiotype gpio; gpio.clear();
   headertype h;
   boardinfotype info;
   vector<batchtype> batch;
   int i, k;

   usb.SetFanSpeed(100);
   usb.VddEnable(0x0);
   usb.BoardInfo(info, true);
   printf("FW is reporting boards[%c%c%c] present\n", info.boards_present & 1 ? '1' : ' ', info.boards_present & 2 ? '2' : ' ', info.boards_present & 4 ? '3' : ' ');
   gpio.reset = 0;
   usb.ReadWriteGPIO(gpio);
   Delay(100);
   gpio.reset = 1;
   usb.ReadWriteGPIO(gpio);
   Delay(100);
   usb.OLED_Display("System Testing\n#");
   
   for (k = 0; k < 3; k++) {
      SetBoard(k);
      usb.SetBaudRate(115200);
      if (IsAlive(0)) {
         printf("I detected an asic on board %d",k+1);
         if (!((info.boards_present >> k) & 1))
            printf(" but FW doesn't report this board!");
         printf("\n");
         boards.push_back(k);
         }
      else {
         if (((info.boards_present >> k) & 1))
            printf("FW reports board %d but I can't communicate to asic 0!\n",k+1);
         }
      }

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
      SetBaud(1000000);
      BoardEnumerate();
      if (found.size() != 132)
         printf("This board has some missing chips(%d/132)\n",found.size());
      found.clear(); // zero this so we don't get confused with multiple boards
      }
   Load(h, 0, 38, 0);
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      batch.push_back(batchtype().ReadWrite(t.board, t.id, E_VERSION_BOUND, i * 100 + ((i * 100 + 99) << 16)));
      batch.push_back(batchtype().ReadWrite(t.board, t.id, E_HASHCONFIG, (1 << 8) | (1 << 9)));
      }
   ReadWriteConfig(batch);
   batch.clear();
   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");

   char buffer[64];
   printf("What core voltage do you want in V?");
   gets_s(buffer, sizeof(buffer));
   float v = atof(buffer);
   if (v > 15.0) {
      printf("Your voltage of %.2fV is too high for the system. Stay under 15.0\n", v);
      exit(-1);
      }
   printf("Setting supply to %.3fV\n", v);
   SetVoltage(v);
   printf("What pll frequency do you want in Mhz?(>=200):");
   gets_s(buffer, sizeof(buffer));
   float f = MAXIMUM(200.0,atof(buffer));

   printf("\n");
   printf("Press enter to enable power switch");
   gets_s(buffer, sizeof(buffer));

   systeminfo.min_voltage = v;
   systeminfo.min_frequency = f;
   systeminfo.min_fan_percentage = 0.8;
   InitialSetup();
   PrintSystem();

   for (i = 0; i < 30; i++)
      {
      boardinfotype info;
      bool too_hot;
      usb.BoardInfo(info, true);
      float avg_temp, max_temp;
      float hitrate = MeasureHashRate(2000,too_hot, avg_temp, max_temp);
      float eff = info.supply_pin / (hitrate*f * found.size() * 254 * 4 / 3 * 1.0e-6);
      float ir = 0, maxtemp = 0;
      for (k = 0; k < topology.size(); k++)
         {
         topologytype& t = topology[k];
         SetBoard(t.board);
         ir += OnDieVoltage(t.id);
         float odt = OnDieTemp(t.id);
         maxtemp = MAXIMUM(maxtemp, odt);
         }
      ir = v - ir / 3.0;
      eff = info.supply_pin / (hitrate * 254 * 4 / 3.0 * topology.size() * f*1.0e-6);
      printf("IR=%.1fmV, Max Temp=%.1fC Power=%.0fW Current=%.1fA Hit rate=%.1f%% Efficiency=%.1fJ/TH\n", ir * 1000.0, maxtemp, info.supply_pin, info.core_current, hitrate * 100.0, eff);
      }
   PrintSystem();

   usb.VddEnable(0x0);
   usb.OLED_Display("Power Off");
   printf("Powered off\n");
   usb.SetFanSpeed(64);
   }



void testtype::Simple()
   {
   gpiotype gpio; gpio.clear();
   boardinfotype info;
   datatype data;
   int i,k;

   if (usb.version == 2) {
      SimpleSystem();
      return;
      }

   usb.BoardInfo(info);
   usb.OLED_Display("Simple Control\n#\n@");
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
   Pll(25, -1);
   BoardEnumerate();
   CalibrateEvalDVM();

   char buffer[64];
   printf("What core voltage do you want in mV?");
   gets_s(buffer, sizeof(buffer));
   float v = atof(buffer);
   if (usb.version == 1 && v > 500) {
      printf("Your voltage of %.0fmV is too high for the eval board. Stay under 500\n",v);
      exit(-1);
      }
   else if (usb.version == 3 && v > 1.300) {
      printf("Your voltage of %.0fV is too high for the 3asic eval board. Stay under 1300\n",v);
      exit(-1);
      }
   printf("Setting supply to %.3fV\n", v);
   SetVoltage(v*0.001);
   printf("What pll frequency do you want in Mhz?(0 for IDDQ):");
   gets_s(buffer, sizeof(buffer));
   float f = atof(buffer);
   for (k = 0; k < found.size(); k++)
      Pll(f,found[k]);
   printf("\n");
   for (i = 0; i < 60; i++)
      {
      boardinfotype info;
      usb.BoardInfo(info,true);
      for (k=0; k<found.size(); k++)
         printf("asic%d On die temp is %.1fC, On die voltage is %.1fmV, Current is %.1fA\n",k,OnDieTemp(found[k]),OnDieVoltage(found[k])*1000.0,info.core_current);
      Sleep(1000);
      }
   
   usb.SetVoltage(S_IO, 0);
   usb.SetVoltage(S_SCV, 0);
   SetVoltage(0);
   usb.OLED_Display("SAFE");
   }

