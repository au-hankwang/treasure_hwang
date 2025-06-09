#include "pch.h"
#include "helper.h"
#include "miner.h"
#include "dvfs.h"
#include "serial.h"
#include "usb.h"
#include "test.h"
#include "multithread.h"

bool testtype::BatchConfig(vector<batchtype>& batch, bool quiet) {
   int i;
   command_cfgtype cmdpacket;
   response_cfgtype rsppacket;
   vector<char> transmit0, transmit1, transmit2;
   int reads0 = 0, reads1 = 0, reads2 = 0;
   for (i = 0; i < batch.size(); i++) {
      const batchtype& b = batch[i];
      vector<char>& transmit = b.board <= 0 ? transmit0 : b.board == 1 ? transmit1 : transmit2;
      int &reads= b.board <= 0 ? reads0 : b.board == 1 ? reads1 : reads2;
      cmdpacket.command = b.action | (b.id < 0 ? CMD_BROADCAST : CMD_NOTHING);
      cmdpacket.id = b.id;
      cmdpacket.address = b.addr;
      cmdpacket.data = b.data;
      cmdpacket.GenerateCrc();
      cmdpacket.SerializeOut(transmit);
      for (int k = 0; k < 4; k++)
         transmit.push_back(0); // this padding of 1 byte ensures reads don't overrun, necessary with collision avoidance
      if (b.action == CMD_READ || b.action == CMD_READWRITE) {
         reads++;
         }
      }
   vector<char> receive0(sizeof(rsppacket) * reads0, 0);
   vector<char> receive1(sizeof(rsppacket) * reads1, 0);
   vector<char> receive2(sizeof(rsppacket) * reads2, 0);
   if (method == 0) {
      receive0.clear();
      boardmodel.ProcessSerial(transmit0, receive0);
      if (receive0.size() != sizeof(rsppacket) * reads0) FATAL_ERROR;	   // receive data is unexpected
      }
   else if (method == 1) {
      serial.SendData(&transmit0[0], transmit0.size());
      if (reads0 > 0)
         serial.RcvData(&receive0[0], sizeof(rsppacket) * reads0);
      }
   else if (method == 2) {
      if (transmit0.size()) usb.SendData(0, &transmit0[0], transmit0.size());
      if (reads0 > 0) usb.RcvData(0, &receive0[0], sizeof(rsppacket) * reads0, quiet);

      if (transmit1.size()) usb.SendData(1, &transmit1[0], transmit1.size());
      if (reads1 > 0) usb.RcvData(1, &receive1[0], sizeof(rsppacket) * reads1, quiet);

      if (transmit2.size()) usb.SendData(2, &transmit2[0], transmit2.size());
      if (reads2 > 0) usb.RcvData(2, &receive2[0], sizeof(rsppacket) * reads2, quiet);
      }
   else FATAL_ERROR;
   reads0 = reads1 = reads2 = 0;
   for (i = 0; i < batch.size(); i++) {
      batchtype& b = batch[i];
      vector<char> &receive = b.board <= 0 ? receive0 : b.board == 1 ? receive1 : receive2;
      int& reads = b.board <= 0 ? reads0 : b.board == 1 ? reads1 : reads2;
      if (b.action == CMD_READ || b.action == CMD_READWRITE) {
         if ((reads + 1) * sizeof(rsppacket) > receive.size()) return true;
         memcpy(&rsppacket, &receive[reads * sizeof(rsppacket)], sizeof(rsppacket));
         if (!rsppacket.IsCrcCorrect()) {
            if (!quiet) printf("Crc error on batch reads\n");
            return true;
            }
         if (rsppacket.address != b.addr) {
            printf("Address mismatch batch reads\n"); return true;
            }
         b.data = rsppacket.data;
         b.id = rsppacket.id;
         reads++;
         }
      }
   return false;
   }

bool testtype::Load(const headertype& header, response_hittype& rsppacket, int ctx, int b, int id) {
   vector<char> transmit;
   command_loadtype load;

   load.command = (ctx == 0 ? CMD_LOAD0 : ctx == 1 ? CMD_LOAD1 : ctx == 2 ? CMD_LOAD2 : CMD_LOAD3) | CMD_RETURNHIT | (id < 0 ? CMD_BROADCAST : CMD_NOTHING);
   load.id = id;
   load.sequence = 0;
   load.header = header;
   load.GenerateCrc();
   load.SerializeOut(transmit);

   if (method == 0) {
      vector<char> receive;
      boardmodel.ProcessSerial(transmit, receive);
      if (receive.size() != sizeof(rsppacket)) FATAL_ERROR;	   // receive data is unexpected
      memcpy(&rsppacket, &receive[0], sizeof(rsppacket));
      }
   else if (method == 1) {
      serial.SendData(&transmit[0], transmit.size());
      if (sizeof(rsppacket) != serial.RcvData(&rsppacket, sizeof(rsppacket)))
         return false;
      }
   else if (method == 2) {
      if (b < 0 || boards.size() == 0) {
         usb.SendData(b, &transmit[0], transmit.size());
         if (sizeof(rsppacket) != serial.RcvData(&rsppacket, sizeof(rsppacket)))
            return false;
         }
      else FATAL_ERROR;
      }
   else FATAL_ERROR;
   if (rsppacket.hit_unique != HIT_UNIQUE)
      return false;
   if (!rsppacket.IsCrcCorrect())
      printf("CRC error on serial response packet doing load\n");
   return true;
   }
void testtype::Load(const headertype& header, int sequence, int difficulty, int ctx) {
   vector<char> transmit;
   command_loadtype load;

   load.command = (ctx == 0 ? CMD_LOAD0 : ctx == 1 ? CMD_LOAD1 : ctx == 2 ? CMD_LOAD2 : CMD_LOAD3) | CMD_BROADCAST;
   load.id = -1;
   load.sequence = sequence;
   load.header = header;
   load.nbits = difficulty;
   load.GenerateCrc();
   load.SerializeOut(transmit);

   if (method == 0) {
      vector<char> receive;
      boardmodel.ProcessSerial(transmit, receive);
      if (receive.size() != 0) FATAL_ERROR;	   // receive data is unexpected
      }
   else if (method == 1)
      serial.SendData(&transmit[0], transmit.size());
   else if (method == 2) {
      if (boards.size() == 0) {
         usb.SendData(-1, &transmit[0], transmit.size());
         }
      else {
         for (int k = 0; k < boards.size(); k++) {
            usb.SendData(boards[k], &transmit[0], transmit.size());
            }
         }
      }
   else FATAL_ERROR;
   }
bool testtype::GetHit(response_hittype& rsppacket, int b, int id) {
   vector<char> transmit;
   command_cfgtype cmdpacket;

   cmdpacket.command = CMD_WRITE | CMD_RETURNHIT | (id < 0 ? CMD_BROADCAST : CMD_NOTHING);
   cmdpacket.address = E_UNIQUE;
   cmdpacket.data = 0;
   cmdpacket.id = id;
   cmdpacket.GenerateCrc();
   cmdpacket.SerializeOut(transmit);

   if (method == 0) {
      vector<char> receive;
      boardmodel.ProcessSerial(transmit, receive);
      if (receive.size() != sizeof(rsppacket)) FATAL_ERROR;	   // receive data is unexpected
      memcpy(&rsppacket, &receive[0], sizeof(rsppacket));
      }
   else if (method == 1) {
      serial.SendData(&transmit[0], transmit.size());
      if (sizeof(rsppacket) != serial.RcvData(&rsppacket, sizeof(rsppacket)))
         return false;
      }
   else if (method == 2) {
      usb.SendData(b, &transmit[0], transmit.size());
      usb.RcvData(b,&rsppacket, sizeof(rsppacket));
      }
   else FATAL_ERROR;
   if (rsppacket.hit_unique != HIT_UNIQUE)
      return false;
   if (!rsppacket.IsCrcCorrect()) {
      printf("CRC error on serial response packet getting a hit\n");
      return false;
      }
   return true;
   }
bool testtype::GetHits(vector<response_hittype>& rsppackets, const vector<int>& ids) {
   vector<char> transmit;
   command_cfgtype cmdpacket;
   int i, k;

   if (rsppackets.size() != ids.size()) FATAL_ERROR;
   for (k = 0; k < ids.size(); k++) {
      cmdpacket.command = CMD_WRITE | CMD_RETURNHIT;
      cmdpacket.address = E_UNIQUE;
      cmdpacket.data = 0;
      cmdpacket.id = ids[k];
      cmdpacket.GenerateCrc();
      cmdpacket.SerializeOut(transmit);
      for (i = 0; i < 96 - 16 + 1; i++)
         transmit.push_back(0);
      }

   if (method == 0) {
      vector<char> receive;
      boardmodel.ProcessSerial(transmit, receive);
      if (receive.size() != rsppackets.size() * sizeof(response_hittype)) FATAL_ERROR;	   // receive data is unexpected
      for (k = 0; k < rsppackets.size(); k++)
         memcpy(&rsppackets[k], &receive[0], sizeof(rsppackets[k]));
      }
   else if (method == 1) {
      serial.SendData(&transmit[0], transmit.size());
      if (rsppackets.size() * sizeof(response_hittype) != serial.RcvData(&rsppackets[0], rsppackets.size() * sizeof(response_hittype)))
         return false;
      }
   else if (method == 2) {
      usb.SendData(-1, &transmit[0], transmit.size());
      if (rsppackets.size() * sizeof(response_hittype) != serial.RcvData(&rsppackets[0], rsppackets.size() * sizeof(response_hittype)))
         return false;
      }
   for (k = 0; k < rsppackets.size(); k++) {
      if (rsppackets[k].hit_unique != HIT_UNIQUE)
         return false;
      if (!rsppackets[k].IsCrcCorrect()) {
         printf("CRC error on serial response packet getting a hit\n");
         return false;
         }
      }
   return true;
   }
autohiterrortype testtype::GetHitAuto(response_hittype& rsppacket) {
   if (hitbuffer.size()) {
      rsppacket = hitbuffer.back();
      hitbuffer.pop_back();
      }
   else {
      while (true) {
         if (4 != serial.RcvData(&rsppacket, 4, true))
            return E_TIMEOUT;
         if (rsppacket.hit_unique == HIT_UNIQUE) {
            if (sizeof(rsppacket) - 4 != serial.RcvData((char*)&rsppacket + 4, sizeof(rsppacket) - 4, true))
               return E_NOUNIQUE;
            break;
            }
         else return E_NOUNIQUE;
         }
      }
   if (!rsppacket.IsCrcCorrect()) {
      return E_CRC;
      }
   return E_NOTHING;
   }

bool testtype::IsAlive(int id) {
   char zeroes[92];
   int i;

   DrainReceiver();
   for (i = 0; i < sizeof(zeroes); i++) zeroes[i] = 0;
   if (method == 1) serial.SendData(zeroes, sizeof(zeroes));	 // clear out any bad state that might be left around
   else if (method == 2) usb.SendData(current_board, zeroes, sizeof(zeroes));	 // clear out any bad state that might be left around
   else FATAL_ERROR;
   vector<batchtype> batch;
   batch.push_back(batchtype().Read(current_board, id, E_UNIQUE));
   DrainReceiver();
   BatchConfig(batch, true);
   return batch[0].data == 0x61727541;
   }

void testtype::Pll(float f_in_mhz, int id, int vcosel) {
   TAtype ta("Pll()");
   static bool reset_needed = true;
   float vco_center = 0;
   if (vcosel < 0) {
      if (f_in_mhz < 500) vcosel = 0;
      else vcosel = 2;
      }
   vcosel = 2;
   if (vcosel == 0) vco_center = 2400;
   else if (vcosel == 1) vco_center = 4200;
   else if (vcosel == 2) vco_center = 6000;
   else if (vcosel == 3) vco_center = 9000;
   else FATAL_ERROR;
   int best1 = -1, best2 = -1;
   float bestvalue = 1.0e10;
   for (int div1 = 0; div1 < 8; div1++) {
      for (int div2 = 0; div2 < 8; div2++)
         {
         float vco = f_in_mhz * (div1 + 1) * (div2 + 1);
         if (fabs(vco - vco_center) < bestvalue && div1>=div2) {
            bestvalue = fabs(vco - vco_center);
            best1 = div1;
            best2 = div2;
            }
         }
      }
   float vco = (best1 + 1) * (best2 + 1) * f_in_mhz;
   float fb_divider = f_in_mhz / 25.0 * (best1 + 1) * (best2 + 1) * (1 << 20);
   pll_multiplier = (best1 + 1) * (best2 + 1) / 25.0 * (1 << 20);
   WriteConfig(E_PLLFREQ, (int)fb_divider, id);
   if (reset_needed) {
      WriteConfig(E_PLLCONFIG, 0x12, id);   // bypass pll
      Delay(10);
      reset_needed = false;
      }
   if (f_in_mhz == 0) {
      WriteConfig(E_PLLCONFIG, 0, id);   // disable pll with no bypass
      reset_needed = true;
      }
   else if (f_in_mhz == 25){
      WriteConfig(E_PLLCONFIG, 0x12, id);   // bypass pll
      reset_needed = false;
      }
   else
      WriteConfig(E_PLLCONFIG, 0x1d + (best2 << 13) + (best1 << 10) + (vcosel<<8), id); // vcosel=0, div1=1, div2=2 (nominal vcoclk 1.6-3.2Ghz, hashclk is 800Mhz-1.6Ghz, assume vco can go lower than spec
   //   printf("Setting pll(%.1f) with div1=%d, div2=%d, fb_divider=%.3f, vco=%.2f, lock=%d\n", f_in_mhz, best1, best2, (float)fb_divider / (1 << 20), vco, (data >> 16) & 1);
   }

float testtype::SpeedSearch(float start, float end, int id, int threshold) {
   const bool fpga = true;
   float vco_upper = 12000.0;
   int i;
   unsigned data;

   Pll(start, id);
   for (int i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0, id);
   Pll(start, id);
   WriteConfig(E_BIST_THRESHOLD, threshold, id);
   WriteConfig(E_SPEED_DELAY, 512, id);
   WriteConfig(E_SPEED_UPPER_BOUND, end*pll_multiplier, id);
   WriteConfig(E_SPEED_INCREMENT, 1.0 * pll_multiplier, id);
   WriteConfig(E_BIST, 2, id);
   while (true) {
      data = ReadConfig(E_BIST, id);
      if (!(data & 1)) break;
      data = ReadConfig(E_PLLFREQ, id);
      }
   data = ReadConfig(E_PLLFREQ, id);
   float hashclk = data / pll_multiplier;
   data = ReadConfig(E_BIST_RESULTS + 0, id);
      
   for (i = 0; i < 32; i++)
      if ((data >> i) & 1)
         printf("Asic %d Miner %d failed bist at %.2fMhz\n", id, i, hashclk);
      
   printf("\n");
   return hashclk;
   }

void testtype::SpeedEnumerate(float start, float end, int id) {
   const bool fpga = true;
   float vco_upper = 12000.0;
   int i;
   unsigned data;
   unsigned engine_mask[8] = { 0xffffffff,0xffffffff,0xffffffff,0xffffffff,0xffffffff,0xffffffff,0xffffffff,0xffffffff };

   if (fpga) vco_upper = 1500.0;
   int bestdiv1 = -1, bestdiv2 = -1, bestdiff = -1, div1, div2;
   for (div1 = 1; div1 < 8; div1++)
      for (div2 = 0; div2 < 8; div2++) {
         float vco = end * (div1 + 1) * (div2 + 1);
         if (fabs(vco - vco_upper) < bestdiff || bestdiff < 0) {
            bestdiv1 = div1, bestdiv2 = div2, bestdiff = fabs(vco - vco_upper);
            }
         }
   float vco = start * (bestdiv1 + 1) * (bestdiv2 + 1);
   int lower_divider = (float)(1 << 20) * vco / 25.0;
   int increment = 1 << 13;
   vco = end * (bestdiv1 + 1) * (bestdiv2 + 1);
   int upper_divider = (float)(1 << 20) * vco / 25.0;


   for (int i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, engine_mask[i], id);
   Pll(start, id);
   WriteConfig(E_BIST_THRESHOLD, 0, id);
   WriteConfig(E_SPEED_DELAY, 512, id);
   WriteConfig(E_SPEED_UPPER_BOUND, end * pll_multiplier, id);
   WriteConfig(E_SPEED_INCREMENT, increment * pll_multiplier, id);
   /*   while (true) {
         unsigned data = ReadConfig(E_SUMMARY, 0);
         printf("\nBist running=%d, hashclk=%.0f    ", (data >> 4) & 1, ((data >> 8) & 0xff) * 25.0);
         }*/
   while (engine_mask[0] != 0xffffffff) {
      for (int i = 0; i < 8; i++)
         WriteConfig(E_ENGINEMASK + i, engine_mask[i], id);
      WriteConfig(E_BIST, 2, id);
      while (true) {
         data = ReadConfig(E_BIST, id);
         if (!(data & 1)) break;
         data = ReadConfig(E_PLLFREQ, id);
         printf("\rSpeed search running=%d, vco=%.3f hashclk=%.0f    ", 1, data * 25.0 / (1 << 20), data * 25.0 / (1 << 20) / (bestdiv1 + 1) / (bestdiv2 + 1));
         }
      data = ReadConfig(E_PLLFREQ, id);
      float hashclk = data * 25.0 / (1 << 20) / (bestdiv1 + 1) / (bestdiv2 + 1);
      data = ReadConfig(E_BIST_RESULTS + 0, id);
      engine_mask[0] |= data;

      for (i = 0; i < 32; i++)
         if ((data >> i) & 1)
            printf("Asic %d Miner %d failed bist at %.2fMhz\n", id, i, hashclk);
      }
   for (int i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, engine_mask[i], id);

   printf("\n");
   }


int testtype::RunBist(int id) {
   uint32 bist_reg;
   int loops;

//   for (i = 0; i < 8; i++)
//      WriteConfig(E_ENGINEMASK + i, 0, id);

   WriteConfig(E_BIST, 0, id); // run bist, clearing failures
   for (loops = 0; loops < 100; loops++) {
      bist_reg = ReadConfig(E_BIST, id);
      if (!(bist_reg & 1)) break;
      }
   bist_reg = (bist_reg >> 1) & 0x1ff;
   printf("Asic%d bist took %d iterations to complete and there are %d failures\n", id, loops, bist_reg);
//   for (i = 0; i < 7; i++)
//      printf("bist results[%d]=%X\n", i, ReadConfig(E_BIST_RESULTS + i, id));
   return bist_reg;
   }

bool testtype::BoardEnumerate() {
   vector<batchtype> batch;
   char zeroes[92];
   int i;

   DrainReceiver();
   found.clear();
   for (i = 0; i < sizeof(zeroes); i++) zeroes[i] = 0;
   
   if (method == 1)
      serial.SendData(zeroes, sizeof(zeroes));	 // clear out any bad state that might be left around
   else if (method == 2)
      usb.SendData(-1, zeroes, sizeof(zeroes));	 // clear out any bad state that might be left around
   for (i = 0; i < 256; i++)
      batch.push_back(batchtype().Read(current_board, i, E_UNIQUE));
   if (!BatchConfig(batch,true)) FATAL_ERROR;  // this must timeout unless there are exactly 256 asic's

   for (i = 0; i < batch.size(); i++) {
      if (batch[i].data == 0x61727541) {
         found.push_back(batch[i].id);
//         printf("Found asic %d, build %x\n", batch[i].id,ReadConfig(E_BUILD_DATE, batch[i].id));
         }
      }
   if (found.size() == 0) {
      printf("Could not locate any asic's\n");
      return true;
      }
   printf("Found %d asic's, build date is %x\n", found.size(), ReadConfig(E_BUILD_DATE, found[0]));
   return false;
   }

void testtype::SetBaud(int baud) {
   int div = refclk * 64 / baud;
   WriteConfig(E_BAUD, div | (div << 16));
   printf("Setting baud to %d, div=%x\n", baud, div);
   Delay(200); // need to wait for the command to leave before changing the uart
   if (method==1)
      serial.SetBaudRate(baud);
   else if (method==2)
      usb.SetBaudRate(baud);
   expected_baud = baud;
   }

bool testtype::ReadWriteStress(int id, int iterations) {
   TAtype ta("ReadWriteStress()");
   vector<batchtype> batch;
   vector<int> regs;
   vector<uint32> expected;
   int i, tries = 0;
   int seed = 0;
   uint32 data;

   regs.push_back(E_ENGINEMASK + 0);
   regs.push_back(E_ENGINEMASK + 1);
   regs.push_back(E_ENGINEMASK + 2);
   regs.push_back(E_ENGINEMASK + 3);
   regs.push_back(E_ENGINEMASK + 4);
   regs.push_back(E_ENGINEMASK + 5);

   expected.resize(regs.size(), 0);
   for (i = 0; i < regs.size(); i++) {
      expected[i] = 0;
      batch.push_back(batchtype().Write(0, id, regs[i], 0));
      }
   if (BatchConfig(batch)) return true;
   for (tries = 0; tries < iterations || iterations < 0; tries++) {
      batch.clear();

      for (i = 0; i < 512; i++) {
         int index = (int)(random(seed) * 6542317) % regs.size();
         data = (__int64)(random(seed) * 1.0e10);
         commandtype action = random(seed) < 0.5 ? CMD_READWRITE : random(seed) < 0.5 ? CMD_WRITE : CMD_READ;
         //		 commandtype action = random(seed) < 0.5 ? CMD_WRITE : CMD_READ;

         if (action == CMD_READ)
            {
            batch.push_back(batchtype().Read(current_board, id, regs[index]));
            batch.back().extra = expected[index];
            }
         else if (action == CMD_READWRITE)
            {
            batch.push_back(batchtype().ReadWrite(current_board, id, regs[index], data));
            batch.back().extra = expected[index];
            }
         else if (action == CMD_WRITE)
            {
            batch.push_back(batchtype().Write(current_board, id, regs[index], data));
            batch.back().extra = expected[index];
            }
         else FATAL_ERROR;
         if (action == CMD_WRITE || action == CMD_READWRITE)
            expected[index] = data;
         }
      WriteConfig(E_COMERROR, 0, id);
      WriteConfig(E_RSPERROR, 0, id);
      if (BatchConfig(batch)) return true;
      if ((data = ReadConfig(E_COMERROR, id)) != 0) {
         printf("Got a comerror, %x\n", data);
         return true;
         }
      if (0 != ((data = ReadConfig(E_RSPERROR, id))&0xffff)) {
         printf("collisions = %d, overruns=%d\n", (data>>16) & 0xffff, data&0xffff);
         return true;
         }
      for (i = 0; i < batch.size(); i++) {
         const batchtype& b = batch[i];
         if (b.action == CMD_READ || b.action == CMD_READWRITE) {
            if (b.data != b.extra) {
               printf("Try %d, Batch %d Mismatch of %s, data=%x, expected=%x\n", tries, i, configtype(0).Label(b.addr), b.data, b.extra);
               return true;
               }
            }
         }
      if ((tries + 1) % 10 == 0) printf("Completed %d register stress loops\n", tries + 1);
      }
   return false;
   }

bool testtype::FrequencyEstimate(float expected_hashclk) {  // return true on error
   TAtype ta("FrequencyEstimate()");
   vector<batchtype> clear, read1, read2;
   int milliseconds = 500;    // wait 1 second, 1 seconds means the hashclk needs to be less than 4Ghz
   struct _timeb start, end;
   float measured_refclk, hashclk;
   int i;
   bool error = false;

   if (found.size() == 0) found.push_back(-1);
   for (i = 0; i < found.size(); i++) {
      clear.push_back(batchtype().Write(current_board, found[i], E_HASHCLK_COUNT, 0));  // zero registers
      read1.push_back(batchtype().Read(current_board, found[i], E_HASHCLK_COUNT));
      read1.push_back(batchtype().Read(current_board, found[i], E_CLKCOUNT));
      read1.push_back(batchtype().Read(current_board, found[i], E_CLKCOUNT_64B));
      }
   read2 = read1;
   BatchConfig(clear);
   _ftime(&start);
   BatchConfig(read1);
   Sleep(milliseconds);
   _ftime(&end);
   BatchConfig(read2);
   int elapsed = end.time * 1000 + end.millitm - start.time * 1000 - start.millitm;
   for (i = 0; i < found.size(); i++) {
      __int64 first_ref  = ((__int64)read1[i * 3 + 2].data << 32) + read1[i * 3 + 1].data;
      __int64 second_ref = ((__int64)read2[i * 3 + 2].data << 32) + read2[i * 3 + 1].data;
      uint32 first_hash  = read1[i * 3 + 0].data;
      uint32 second_hash = read2[i * 3 + 0].data;

      measured_refclk = (second_ref - first_ref) / (elapsed * 0.001);
      hashclk = (second_hash - first_hash) / (elapsed * 0.001);
      if (measured_refclk <= 0.0) measured_refclk = 1.0;
      float snaprefclk = measured_refclk;
      if (fabs(25.0e6 - snaprefclk) < 1.0e6) snaprefclk = 25.0e6;
      if (fabs(50.0e6 - snaprefclk) < 1.0e6) snaprefclk = 50.0e6;
      if (fabs(100.0e6 - snaprefclk) < 1.0e6) snaprefclk = 100.0e6;
      int time = second_ref / snaprefclk;
      int days = time / (3600 * 24);
      int hours = time - days * (3600 * 24);
      int minutes = hours;
      hours = hours / 3600;
      minutes = minutes - hours * 3600;
      int seconds = minutes;
      minutes = minutes / 60;
      seconds = seconds - minutes * 60;

      error = error || fabs(measured_refclk - refclk) > refclk*0.02;
      if (expected_hashclk==0.0)
         printf("asic%-3d refclk is %.1fMhz, hashclk is %.1fMhz, uptime is %dD:%dH:%dM:%dS\n", read1[i*3+0].id, measured_refclk * 1e-6, hashclk * 1e-6, days, hours, minutes, seconds);
      else
         error = error || fabs(hashclk - expected_hashclk) > expected_hashclk * 0.02;
      }
   return error;
   }

void testtype::RegisterDump() {
   int i, k;

   for (k = 0; k < found.size(); k++) {
      vector<batchtype> batch;
      for (i = 0; i < 256; i++)
         batch.push_back(batchtype().Read(current_board, found[k], i));
      BatchConfig(batch);
      for (i = 0; i < 256; i++) {
         printf("Asic%-3d Csr[%3d], %20s = %8x\n", found[k], batch[i].addr, configtype(0).Label(batch[i].addr), batch[i].data);
         }
      }
   }

float testtype::OnDieVoltage(int id, supplytype supply)
   {
   float v;
   for (int tries = 0; tries < 5; tries++) {
      if (VmonNotConfiged || supply!=S_CORE) {
         VmonNotConfiged = supply != S_CORE;
         if (supply == S_IO) FATAL_ERROR;
         int channel = supply == S_CORE ? 5 : supply == S_SCV ? 15 : 24;
         WriteConfig(E_IPCFG, 0x1, -1); // ipclk is 6.25Mhz
         WriteConfig(E_TEMPCFG, 0xd, -1);  // turn on temp sensor
         Delay(10);
         WriteConfig(E_DVMCFG, 0x2, -1);
         Delay(10);
         WriteConfig(E_DVMCFG, 0x3, -1);
         Delay(10);
         WriteConfig(E_DVMCFG, 0x1, -1);
         Delay(10);
         WriteConfig(E_DVMCFG, 0x9 | (channel << 12), -1);   // channel 5
         Delay(10);
         WriteConfig(E_DVMCFG, 0x5, -1);
         Delay(250);
         }
      int x = ReadConfig(E_VOLTAGE, id);
      v = topologytype().Voltage(x);
      if (v > -0.1 && v < 1.0) return v;
      VmonNotConfiged = supply != S_CORE;
      printf("Voltage is crazy. Retrying...");
      DrainReceiver();
      }
   printf("On die voltage monitor is giving a crazy value of %.2fV\n", v);
   FATAL_ERROR;
   return 0.0;
   }

float testtype::OnDieTemp(int id)
   {
   float t;
   for (int tries = 0; tries < 5; tries++) {
      if (VmonNotConfiged) {
         VmonNotConfiged = false;
         WriteConfig(E_IPCFG, 0x1, -1); // ipclk is 6.25Mhz
         WriteConfig(E_TEMPCFG, 0xd, -1);  // turn on temp sensor
         Delay(10);
         WriteConfig(E_DVMCFG, 0x2, -1);
         Delay(10);
         WriteConfig(E_DVMCFG, 0x3, -1);
         Delay(10);
         WriteConfig(E_DVMCFG, 0x1, -1);
         Delay(10);
         WriteConfig(E_DVMCFG, 0x9 | (5 << 12), -1);   // channel 5
         Delay(10);
         WriteConfig(E_DVMCFG, 0x5, -1);
         Delay(250);
         }
      int x = ReadConfig(E_TEMPERATURE, id);
      t = topologytype().Temp(x);
      if (t > -50.0 && t < 150.0) return t;
      VmonNotConfiged = true;
      printf("Temperature is crazy. Retrying...");
      DrainReceiver();
      }
   FATAL_ERROR;
   return 0.0;
   }

void testtype::BaudSweep(int id) {
   int setting;
   for (setting = 320; setting >= 1; setting--) {
      int baud = refclk * 64 / setting;
      WriteConfig(E_BAUD, setting | (setting << 16), id);
      printf("Setting baud to %d, div=%x\n", baud, setting);
      Sleep(50); // need to wait for the command to leave before changing the uart
      serial.SetBaudRate(baud);
      expected_baud = baud;
      if (!IsAlive(id)) {
         printf("Aliveness check failed, baud setting=%d\n", setting);
         return;
         SetBaud(115200);
         if (!IsAlive(id)) return;
         //		 return;
         }
      else
         ReadWriteStress(id, 1);
      }
   }

void testtype::BaudTolerance(int id) {
   printf("\n");
   float offset = 0.0;
   for (offset = 0.0; offset < 10.0; offset += 0.05) {
      int baud = expected_baud * (1.0 + offset / 100.0);
      serial.SetBaudRate(baud);
      printf("\rSetting baudrate to %d. %.1f%%", baud, offset);
      if (ReadWriteStress(id, 1)) break;
      }
   printf("\n");
   for (offset = 0.0; offset < 10.0; offset += 0.05) {
      int baud = expected_baud * (1.0 - offset / 100.0);
      serial.SetBaudRate(baud);
      printf("\rSetting baudrate to %d. %.1f%%", baud, -offset);
      if (ReadWriteStress(id, 1)) break;
      }
   }

void testtype::BistParedo() {
   float min = 200.0, max = 900.0, incr = 5.0;
   int period = 60;
   int index = 0, iterations = 0;

   vector<vector<int> > hits, truehits, retard_hits, retard_truehits;
   FILE* fptr = fopen("bist.csv", "at");

   fprintf(fptr, "\nSample");
   for (index = 0; index * incr + min <= max; index++)
      {
      float speed = index * incr + min;
      hits.push_back(vector<int>(found.size(), 0));
      retard_hits.push_back(vector<int>(found.size(), 0));
      truehits.push_back(vector<int>(found.size(), 0));
      retard_truehits.push_back(vector<int>(found.size(), 0));
      fprintf(fptr, ",%.0f", speed);
      }
   for (int a = 0; a < found.size() * 2; a++) {
      fprintf(fptr, "\nBIST_asic%s%d_ref%.0f", a >= found.size() ? "-retard" : "", a % 2, refclk/1.0e6);
      for (index = 0; index * incr + min <= max; index++)
         {
         float speed = index * incr + min;
         Pll(speed, -1);
         if (a < found.size())
            WriteConfig(E_CLOCK_RETARD, 0x000);
         else
            WriteConfig(E_CLOCK_RETARD, 0x101);
         Sleep(100);  // make sure pll has settled
         fprintf(fptr, ",%d", RunBist(found[a % found.size()]));
         }
      }
   fclose(fptr);
   Pll(200, -1);
   }

void testtype::Paredo(const char *filename) {
   float min = 200.0, max = 1600.0, incr = 50.0;
   int period = 10;
   int index = 0, iterations = 0;

   if (found.size() == 0) found.push_back(eval_id);

   vector<vector<int> > hits, truehits, retard_hits, retard_truehits, bist,retard_bist;
   vector<float> power;
   FILE* fptr = fopen(filename, "at");
   if (fptr == NULL) { printf("couldn't open %s", filename); FATAL_ERROR; }

   fprintf(fptr, "Sample");
   for (index = 0; index * incr + min <= max; index++)
      {
      float speed = index * incr + min;
      hits.push_back(vector<int>(found.size(), 0));
      retard_hits.push_back(vector<int>(found.size(), 0));
      truehits.push_back(vector<int>(found.size(), 0));
      retard_truehits.push_back(vector<int>(found.size(), 0));
      bist.push_back(vector<int>(found.size(), 0));
      retard_bist.push_back(vector<int>(found.size(), 0));
      fprintf(fptr, ",%.0f", speed);
      }
   for (index = 0; index * incr + min <= max; index++)
      {
      float speed = index * incr + min;
      Pll(speed, -1);
      Delay(100);
      WriteConfig(E_HASHCONFIG, (0 << 9));
      WriteConfig(E_UNIQUE, 0);
      WriteConfig(E_BIST, 0);
      for (int i = 0; i < found.size(); i++)
         {
         int b;
         while ((b = ReadConfig(E_BIST, found[i])) & 1);
         bist[index][i] = 254 - ((b >> 1) & 0xff);
         }
      MineTestStatistics(period, hits[index], truehits[index]);
      WriteConfig(E_HASHCONFIG, (1 << 9));
      WriteConfig(E_UNIQUE, 0);
      WriteConfig(E_BIST, 0);
      float totalv = 0.0;
      for (int i = 0; i < found.size(); i++)
         {
         int b;
         while ((b = ReadConfig(E_BIST, found[i])) & 1);
         retard_bist[index][i] = 254-((b >> 1) & 0xff);
         totalv += OnDieVoltage(found[i]);
         }
      MineTestStatistics(period, retard_hits[index], retard_truehits[index]);
      boardinfotype info;
      usb.BoardInfo(info,true);
      power.push_back(info.core_current * totalv);
      }
   for (int a = 0; a < found.size() * 6; a++) {
      if (a % 6 == 0)
         fprintf(fptr, "\nBist%d", found[a / 6]);
      else if (a % 6 == 1)
         fprintf(fptr, "\nBist_Duty%d", found[a / 6]);
      else if (a % 6 == 2)
         fprintf(fptr, "\nTrue%d", found[a / 6]);
      else if (a % 6 == 3)
         fprintf(fptr, "\nAll%d", found[a / 6]);
      else if (a % 6 == 4)
         fprintf(fptr, "\nTrue_Duty%d", found[a / 6]);
      else if (a % 6 == 5)
         fprintf(fptr, "\nAll_Duty%d", found[a / 6]);
      else FATAL_ERROR;
      vector<vector<int> >& data = (a % 6 == 0) ? bist : (a % 6 == 1) ? retard_bist : (a % 6 == 2) ? truehits : (a % 6 == 3) ? hits : (a % 6 == 4) ? retard_truehits : retard_hits;
      for (index = 0; index * incr + min <= max; index++)
         {
         float speed = index * incr + min;
         if (a%6<2)
            fprintf(fptr, ",%.3f", data[index][a/6]/254.0);
         else
            fprintf(fptr, ",%.3f", (float)data[index][a / 6] / period /speed / (254*4/3.0*1.0e6/((__int64)1<<32)));
         }
      }
   fprintf(fptr,"\n");
   for (int a = 0; a < 6; a++) {
      if (a % 6 == 0)
         fprintf(fptr, "\nBist");
      else if (a % 6 == 1)
         fprintf(fptr, "\nBist_Duty");
      else if (a % 6 == 2)
         fprintf(fptr, "\nTrue");
      else if (a % 6 == 3)
         fprintf(fptr, "\nAll");
      else if (a % 6 == 4)
         fprintf(fptr, "\nTrue_Duty");
      else if (a % 6 == 5)
         fprintf(fptr, "\nAll_Duty");
      else FATAL_ERROR;
      vector<vector<int> >& data = (a % 6 == 0) ? bist : (a % 6 == 1) ? retard_bist : (a % 6 == 2) ? truehits : (a % 6 == 3) ? hits : (a % 6 == 4) ? retard_truehits : retard_hits;
      for (index = 0; index * incr + min <= max; index++)
         {
         float speed = index * incr + min;
         float total = 0.0;
         for (int i = 0; i < found.size(); i++)
            total += data[index][i];
         if (a % 6 < 2)
            fprintf(fptr, ",%.3f", total / 254.0);
         else
            fprintf(fptr, ",%.3f", total / period / speed / (254 * 4 / 3.0 * 1.0e6 / ((__int64)1 << 32)));
         }
      }
   fprintf(fptr, "\nPower");
   for (index = 0; index * incr + min <= max; index++)
      {
      fprintf(fptr, ",%.2f", power[index]);
      }
   fprintf(fptr, "\nEfficiency,=B$27/(B21*B$1*0.000338667)\n");
   fclose(fptr);
   iterations++;
   }

void testtype::MineTestStatistics(int seconds, vector<int> &hits, vector<int> &truehits) {
   int i, k;
   int seed = seconds;// time(NULL);
   command_loadtype load;
   response_hittype rsppacket;
   headertype headers[256];
   int iterations = 0;
   int total_hits = 0;
   vector<batchtype> batch;
   vector<int> statistics(found.size(), 0);
   vector<__int64> repeats;
   unsigned last_load=-1;
   const int version_space_per_asic = 256;

   WriteConfig(E_VERSION_SHIFT, 0, -1);
   for (i = 0; i < found.size(); i++) {
      WriteConfig(E_VERSION_BOUND, i * version_space_per_asic | ((i * version_space_per_asic + version_space_per_asic - 1) << 16), found[i]);
      batch.push_back(batchtype().Read(current_board, found[i], E_HIT0));
      }
   for (i = 0; i < 256; i++) {
      randomcpy(seed, &headers[i], sizeof(headertype));
      headers[i].x.version = 0;
      }
   Load(headers[37], 0, 64, 0);
   WriteConfig(E_HIT2, 0);  // clear out any old hits so the checkers below don't get confused
   WriteConfig(E_HIT2, 0);
   WriteConfig(E_HIT1, 0);
   WriteConfig(E_HIT1, 0);
   WriteConfig(E_HIT0, 0);
   WriteConfig(E_HIT0, 0);
   WriteConfig(E_GENERAL_HIT_COUNT, 0);
   WriteConfig(E_GENERAL_TRUEHIT_COUNT, 0);
   WriteConfig(E_DROPPED_HIT_COUNT, 0);
   struct _timeb start, end;
   _ftime(&start);

   while (true) {
      _ftime(&end);
      int elapsed = end.time * 1000 + end.millitm - start.time * 1000 - start.millitm;
      if (elapsed >= seconds*1000) break;
      if (last_load/30000!=elapsed/30000) {  // load new job every 30 seconds
         Load(headers[iterations % 256], iterations % 256, 32, 0);
         iterations++;
         last_load = elapsed;
         }
      bool hit = false;

      for (k = 0; k < 10; k++) {
         if (BatchConfig(batch)) FATAL_ERROR;
         for (i = 0; i < batch.size(); i++)
            if ((batch[i].data >> 12) & 1) {
               memset(&rsppacket, 0, sizeof(rsppacket));
               if (!GetHit(rsppacket, batch[i].id)) FATAL_ERROR;
               hit = true;
               break;
               }
         if (hit) break;
         }
      if (!hit) Sleep(200);

      if (hit)
         {
         for (i = 256 - 1; i >= 0; i--)
            if (memcmp(headers[i].x.merkle, rsppacket.header.x.merkle, sizeof(rsppacket.header.x.merkle)) == 0)
               break;
         int zeroes = rsppacket.header.ReturnZeroes();   // this checks the result for correctness
         if (zeroes < 32 || zeroes != rsppacket.nbits) {
            printf("Hit returned had %d zeroes\n", zeroes);
            FATAL_ERROR;
            }
         if (i < 0) {
            printf("I had a hit that didn't merkle match any known headers!\n");
//            FATAL_ERROR;
            }
         else if (i != rsppacket.sequence) {
            printf("I had a hit with the wrong sequence #!\n");
//            FATAL_ERROR;
            }
         total_hits++;
         int timedifference = rsppacket.header.x.time - headers[i].x.time;
         int general_hits = 0;
         int true_hits = 0, dropped = 0;
         for (k = 0; k < batch.size(); k++) {
            hits[k]     += ReadWriteConfig(E_GENERAL_HIT_COUNT, 0, batch[k].id);
            truehits[k] += ReadWriteConfig(E_GENERAL_TRUEHIT_COUNT, 0, batch[k].id);
            general_hits += hits[k];
            true_hits += truehits[k];
            }
         printf("It%d: I saw a hit on context %3d, id %d, nonce %8x, header %3d, time=%x, nbits=%d (%d,%d,%d,%d,%.4f/sec)\n", iterations, rsppacket.header.x.version, rsppacket.id, rsppacket.header.x.nonce, i, timedifference, rsppacket.nbits, total_hits, general_hits, true_hits, dropped, (float)total_hits*1000.0 / elapsed);
         __int64 key = ((__int64)rsppacket.header.x.nonce << 32) | rsppacket.header.x.merkle[0];
         if (std::find(repeats.begin(), repeats.end(), key) != repeats.end())
            {
            printf("A hit matched in the repeat list\n");
//            FATAL_ERROR;
            }
         if (timedifference > 25) {
            printf("I saw a timedifference>25 which shouldn't happen with smallnonce=0\n");
            FATAL_ERROR;
            }
         repeats.push_back(key);
         int index;
         for (index = found.size() - 1; index >= 0; index--) if (found[index] == rsppacket.id) break;
         if (index < 0) FATAL_ERROR;
         statistics[index]++;
         if (iterations % 64 == 0)
            printf("It%d: asic0=%d asic1=%d %.4f/sec,%.4f/sec)\n", iterations, statistics[0], statistics[1], (float)statistics[0] *1000.0/ elapsed, (float)statistics[1] / elapsed);
         }
      }
   }

/*
void testtype::FakeMineTest() {
   int i, k;
   int seed = time(NULL);
   command_loadtype load;
   vector<batchtype> batch, stats;
   int iterations = 0;
   int total_hits = 0;
   vector<int> statistics(found.size(), 0);
   unsigned starttime = time(NULL);
   unsigned last_load = -1;
   const int version_space_per_asic = 256;

   headertype h;
   h.x.nonce = 0x3f93ada7;// 0xa7ad933f;
   h.words[0] = 10;
   h.words[2] = 0x14f32d97;
   printf("Header=%d\n", h.ReturnZeroes());
   //   h.AsciiIn("0000002045569767d88d3962a3fbb9a0776f1f97c636f327d8ec0300000000000000000026ed8403a595b6e5fd172050db42eabfe76700aedd17e6b2ce416b92a1c4b7660553aa5d5ca31517a7ad933f");

   //   WriteConfig(E_HASHCONFIG, 0x10000, -1);    // turn off small nonce
   //   WriteConfig(E_HASHCONFIG, 0x00000, -1);    // turn off small nonce
   for (int i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0, -1);

   WriteConfig(E_VERSION_SHIFT, 30, -1);
   WriteConfig(E_VERSION_BOUND, 0 | (0xffff << 16), -1);
   for (i = 0; i < found.size(); i++) {
      batch.push_back(batchtype(E_SUMMARY, 0, CMD_READ, found[i]));
      stats.push_back(batchtype(E_GENERAL_HIT_COUNT, 0, CMD_READ, found[i]));
      stats.push_back(batchtype(E_GENERAL_TRUEHIT_COUNT, 0, CMD_READ, found[i]));
      stats.push_back(batchtype(E_DROPPED_HIT_COUNT, 0, CMD_READ, found[i]));
      stats.push_back(batchtype(E_DROPPED_DIFFICULT_COUNT, 0, CMD_READ, found[i]));
      Pll(160.0*(0.95 + 0.10*i/(found.size()-1)), found[i]);
      }
   FrequencyEstimate();
   Load(h, 32, 32, 0);
   WriteConfig(E_HIT2, 0, -1);  // clear out any old hits so the checkers below don't get confused
   WriteConfig(E_HIT2, 0, -1);
   WriteConfig(E_HIT1, 0, -1);
   WriteConfig(E_HIT1, 0, -1);
   WriteConfig(E_HIT0, 0, -1);
   WriteConfig(E_HIT0, 0, -1);
   WriteConfig(E_GENERAL_HIT_COUNT, 0, -1);
   WriteConfig(E_GENERAL_TRUEHIT_COUNT, 0, -1);
   WriteConfig(E_DROPPED_HIT_COUNT, 0, -1);
   WriteConfig(E_DROPPED_DIFFICULT_COUNT, 0, -1);
   time_t start = time(NULL);

   while (true) {
      unsigned elapsed = time(NULL) - start;
      if (last_load / 10 != elapsed / 10) {  // load new job every 10 seconds
         Load(h, 32, 32, 0);
         last_load = elapsed;
         }
      iterations++;

      if (BatchConfig(batch)) FATAL_ERROR;
      bool nothing = true;
      vector<int> ids;
      vector<response_hittype> rsppackets;
      for (int h = 3; h >= 1; h--) {
         for (k = 0; k < batch.size(); k++)
            {
            if (false && ((batch[k].data >> 12) & 1)) {
               response_hittype rsppacket;
               memset(&rsppacket, 0, sizeof(rsppacket));
               if (!GetHit(rsppacket, batch[k].id)) FATAL_ERROR;
               total_hits++;
               statistics[rsppacket.id]++;
               nothing = false;
               }
            if ((batch[k].data & 0xf) >= h) {
               ids.push_back(batch[k].id);
               rsppackets.push_back(response_hittype());
               total_hits++;
               nothing = false;
               }
            }
         }
      if (!nothing) {
         GetHits(rsppackets, ids);
         for (k = 0; k < rsppackets.size(); k++)
            statistics[rsppackets[k].id]++;
         }
      if (iterations % 500 == 0) {
         int general_hits = 0, true_hits = 0, dropped = 0, dropdifficult=0;
         unsigned elapsed = time(NULL) - starttime;
         if (BatchConfig(stats)) FATAL_ERROR;

         for (k = 0; k < batch.size(); k++) {
            general_hits += stats[k * 4 + 0].data;
            true_hits += stats[k * 4 + 1].data;
            dropped += stats[k * 4 + 2].data;
            dropdifficult += stats[k * 4 + 3].data;
            printf("Asic%d total=%d(%.2f%%), general=%d, true=%d, dropped=%d, dropdiff=%d, %.4f/sec)\n", k, statistics[k], 100.0 * statistics[k] / stats[k * 4 + 1].data, stats[k * 4 + 0].data, stats[k * 4 + 1].data, stats[k * 4 + 2].data, stats[k * 4 + 3].data, (float)statistics[k] / elapsed);
            }
         printf("It%d: total=%d(%.2f%%), general=%d, true=%d, dropped=%d, dropdiff=%d, %.4f/sec)\n", iterations, total_hits, 100.0 * total_hits / true_hits, general_hits, true_hits, dropped, dropdifficult, (float)total_hits / elapsed);
         }
      }
   }

void testtype::FakeMineTestAuto() {
   int i, k;
   int seed = time(NULL);
   command_loadtype load;
   vector<batchtype> batch, stats;
   int iterations = 0;
   int total_hits = 0;
   vector<int> statistics(found.size(), 0);
   int crcs = 0, nounique = 0;
   unsigned starttime = time(NULL);
   unsigned last_load = -1;
   const int version_space_per_asic = 256;
   unsigned hitconfig = 0x18;
//   unsigned hitconfig = 0x80000006;

   headertype h;
   h.x.nonce = 0x3f93ada7;// 0xa7ad933f;
   h.words[0] = 10;
   h.words[2] = 0x14f32d97;
   for (int i = 0; i < 8; i++)
      WriteConfig(E_ENGINEMASK + i, 0, -1);

   WriteConfig(E_HITCONFIG, hitconfig | 0, -1);   // turn off autoreporting
   WriteConfig(E_VERSION_SHIFT, 30, -1);
   WriteConfig(E_VERSION_BOUND, 0 | (0xffff << 16), -1);
   for (i = 0; i < found.size(); i++) {
      batch.push_back(batchtype(E_SUMMARY, 0, CMD_READ, found[i]));
      stats.push_back(batchtype(E_GENERAL_HIT_COUNT, 0, CMD_READ, found[i]));
      stats.push_back(batchtype(E_GENERAL_TRUEHIT_COUNT, 0, CMD_READ, found[i]));
      stats.push_back(batchtype(E_DROPPED_HIT_COUNT, 0, CMD_READ, found[i]));
      stats.push_back(batchtype(E_DROPPED_DIFFICULT_COUNT, 0, CMD_READ, found[i]));
      Pll(100.0 * (0.95 + 0.10 * i / (found.size() - 1)), found[i]);
      }
   FrequencyEstimate();
   Load(h, 32, 32, 0);
   WriteConfig(E_HIT2, 0, -1);  // clear out any old hits so the checkers below don't get confused
   WriteConfig(E_HIT2, 0, -1);
   WriteConfig(E_HIT1, 0, -1);
   WriteConfig(E_HIT1, 0, -1);
   WriteConfig(E_HIT0, 0, -1);
   WriteConfig(E_HIT0, 0, -1);
   WriteConfig(E_GENERAL_HIT_COUNT, 0, -1);
   WriteConfig(E_GENERAL_TRUEHIT_COUNT, 0, -1);
   WriteConfig(E_DROPPED_HIT_COUNT, 0, -1);
   WriteConfig(E_DROPPED_DIFFICULT_COUNT, 0, -1);
//   WriteConfig(E_HITCONFIG, hitconfig|1, -1);   // turn on autoreporting
   time_t start = time(NULL);

   while (true) {
      unsigned elapsed = time(NULL) - start;
      if (last_load / 10 != elapsed / 10) {  // load new job every 10 seconds
         Load(h, 32, 32, 0);
         last_load = elapsed;
         }
      iterations++;
      if (iterations%100==0)
         WriteConfig(E_HITCONFIG, hitconfig | 0, -1);   // turn off autoreporting

      autohiterrortype lasterror = E_NOTHING;
      int loop = 0;
      while(loop<100) {
         response_hittype rsppacket;
         autohiterrortype error = GetHitAuto(rsppacket);
         if (error == E_CRC) {
            crcs++;
            printf("Crc error found, it=%d\n", iterations);
            lasterror = error;
            }
         else if (error == E_NOUNIQUE && lasterror!=E_NOUNIQUE)
            {
            nounique++;
            printf("No unique field found, it=%d\n",iterations);
            lasterror = error;
            if (rsppacket.hit_unique == HIT_UNIQUE) {
               total_hits++;
               statistics[rsppacket.id]++;
               lasterror = error;
               }
            }
         else if (error == E_NOTHING) {
            total_hits++;
            statistics[rsppacket.id]++;
            lasterror = error;
            }
         else if (error == E_TIMEOUT) {
            lasterror = error;
            break;
            }
         loop++;
         }

      if (iterations % 100 == 0) {
         int general_hits = 0, true_hits = 0, dropped = 0, dropdifficult = 0;
         unsigned elapsed = time(NULL) - starttime;

         serial.DrainReceiver();
         if (BatchConfig(stats)) FATAL_ERROR;

         for (k = 0; k < batch.size(); k++) {
            general_hits += stats[k * 4 + 0].data;
            true_hits += stats[k * 4 + 1].data;
            dropped += stats[k * 4 + 2].data;
            dropdifficult += stats[k * 4 + 3].data;
            printf("Asic%d total=%d(%.2f%%), general=%d, true=%d, dropped=%d, dropdiff=%d, %.4f/sec)\n", k, statistics[k],100.0* statistics[k]/ stats[k * 4 + 1].data, stats[k * 4 + 0].data, stats[k * 4 + 1].data, stats[k * 4 + 2].data, stats[k * 4 + 3].data, (float)statistics[k] / elapsed);
            }
         printf("It%d: total=%d(%.2f%%), general=%d, true=%d, dropped=%d, dropdiff=%d, crc=%d(%.2f%%), %.4f/sec)\n", iterations, total_hits, 100.0*total_hits/true_hits,general_hits, true_hits, dropped, dropdifficult, crcs+nounique, 100.0 * (crcs + nounique) / true_hits, (float)total_hits / elapsed);
         }
      WriteConfig(E_HITCONFIG, hitconfig | 1, -1);   // turn on autoreporting
//      printf("It%d: total=%d, crc=%d(%.2f%%), %.4f/sec)\n", iterations, total_hits, crcs + nounique, 100.0*(crcs + nounique)/total_hits,(float)total_hits / elapsed);
      }
   }
*/

void testtype::MineTest() {
   int i,k;
   int seed = time(NULL);
   command_loadtype load;
   response_hittype rsppacket;
   headertype headers[256];
   int iterations = 0;
   int total_hits = 0;
   vector<batchtype> batch;
   vector<int> statistics(found.size(), 0);
   vector<__int64> repeats;
   unsigned starttime = time(NULL);
   unsigned last_load = -1;
   const int version_space_per_asic = 256;

   WriteConfig(E_VERSION_SHIFT, 0, -1);
   WriteConfig(E_HITCONFIG, 0x18, -1);    // turn off autoreporting
   WriteConfig(E_HASHCONFIG, 0x0, -1);    // turn off small nonce
   for (i = 0; i < found.size(); i++) {
      WriteConfig(E_VERSION_BOUND, i * version_space_per_asic | ((i * version_space_per_asic + version_space_per_asic-1) << 16), found[i]);
      batch.push_back(batchtype().Read(current_board, found[i],E_HIT0));
      }
   for (i = 0; i < 256; i++) {
      randomcpy(seed, &headers[i], sizeof(headertype));
      headers[i].x.version = 0;
      }
   Load(headers[37], 64, 0, -1);
   WriteConfig(E_HIT2, 0);  // clear out any old hits so the checkers below don't get confused
   WriteConfig(E_HIT2, 0);
   WriteConfig(E_HIT1, 0);
   WriteConfig(E_HIT1, 0);
   WriteConfig(E_HIT0, 0);
   WriteConfig(E_HIT0, 0);
/*   for (i = 0; i < batch.size(); i++) {
      GetHit(rsppacket, batch[i].id);  // there should be 3 old hits, pop them so the checker below won't be confused
      GetHit(rsppacket, batch[i].id);
      GetHit(rsppacket, batch[i].id);
      }*/
   WriteConfig(E_GENERAL_HIT_COUNT, 0);
   WriteConfig(E_GENERAL_TRUEHIT_COUNT, 0);
   WriteConfig(E_DROPPED_HIT_COUNT, 0);
   time_t start = time(NULL);

   while (true) {
      unsigned elapsed = time(NULL) - start;
      if (last_load / 10 != elapsed / 10) {  // load new job every 10 seconds
         randomcpy(seed, &headers[iterations % 256], sizeof(headertype));
         headers[iterations % 256].x.version = 0;
         Load(headers[iterations % 256], iterations % 256, 32, 0);
         iterations++;
         last_load = elapsed;
         }
      bool hit = false;
      
      for (k = 0; k < 10; k++) {
         if (BatchConfig(batch)) FATAL_ERROR;
         for (i = 0; i < batch.size(); i++)
            if ((batch[i].data>>12)&1) {
               memset(&rsppacket, 0, sizeof(rsppacket));
               if (!GetHit(rsppacket, batch[i].id)) FATAL_ERROR;
               hit = true;
               break;
               }
         if (hit) break;
         }
      if (!hit) Sleep(1000);
      /*
      int bist_reg = ReadConfig(E_BIST, batch[0].id);
      if (bist_reg & 1) FATAL_ERROR;  // bist should have completed
      bist_reg = (bist_reg >> 1) & 0x1ff;
      if (bist_reg != 200) FATAL_ERROR;
      WriteConfig(E_BIST, 1, batch[0].id); // run bist
      */

      if (hit)
         {
         for (i = 256 - 1; i >= 0; i--)
            if (memcmp(headers[i].x.merkle, rsppacket.header.x.merkle, sizeof(rsppacket.header.x.merkle)) == 0)
               break;
         int zeroes = rsppacket.header.ReturnZeroes();   // this checks the result for correctness
         if (zeroes < 32 || zeroes != rsppacket.nbits) {
            printf("Hit returned had %d zeroes\n", zeroes);
            FATAL_ERROR;
            }
         if (i < 0) {
            printf("I had a hit that didn't merkle match any known headers!\n");
            FATAL_ERROR;
            }
         else if (i != rsppacket.sequence) {
            printf("I had a hit with the wrong sequence #!\n");
            FATAL_ERROR;
            }
         total_hits++;
         int timedifference = rsppacket.header.x.time - headers[i].x.time;
         int general_hits = 0;
         int true_hits = 0, dropped=0;
         unsigned elapsed = time(NULL) - starttime;
         for (k = 0; k < batch.size(); k++) {
            general_hits += ReadConfig(E_GENERAL_HIT_COUNT, batch[k].id);
            true_hits += ReadConfig(E_GENERAL_TRUEHIT_COUNT, batch[k].id);
            dropped += ReadConfig(E_DROPPED_HIT_COUNT, batch[k].id);
            }
         printf("It%d: I saw a hit on context %3d, id %d, nonce %8x, header %3d, time=%x, nbits=%d (%d,%d,%d,%d,%.4f/sec)\n", iterations, rsppacket.header.x.version, rsppacket.id, rsppacket.header.x.nonce, i, timedifference, rsppacket.nbits, total_hits, general_hits, true_hits, dropped, (float)total_hits / elapsed);
         __int64 key = ((__int64)rsppacket.header.x.nonce << 32) | rsppacket.header.x.merkle[0];
         if (std::find(repeats.begin(), repeats.end(), key) != repeats.end())
            {
            printf("A hit matched in the repeat list\n");
            FATAL_ERROR;
            }
         if (timedifference > 5) {
            printf("I saw a timedifference>2 which should happen with smallnonce=0\n");
            FATAL_ERROR;
            }
         repeats.push_back(key);

         statistics[rsppacket.id]++;
         if (iterations%64==0)
            printf("It%d: asic0=%d asic1=%d %.5f/sec,%.5f/sec)\n",iterations,statistics[0], statistics[1], (float)statistics[0] /elapsed, (float)statistics[1] / elapsed);
         }
      }
   }

void testtype::MineTest_Auto() {
   int i;
   int seed = time(NULL);
   command_loadtype load;
   response_hittype rsppacket;
   headertype headers[256];
   int iterations = 0;
   int total_hits = 0;
   vector<batchtype> batch;
   vector<int> statistics(found.size(), 0);
   vector<__int64> repeats;
   unsigned starttime = time(NULL);
   unsigned last_load = -1;
   const int version_space_per_asic = 256;

   WriteConfig(E_VERSION_SHIFT, 0, -1);
   WriteConfig(E_HASHCONFIG, 0x0000, -1);    // turn off small nonce
   WriteConfig(E_HITCONFIG, 0x80000007, -1);   // disable collision, turn on autoreporting
   for (i = 0; i < found.size(); i++) {
      WriteConfig(E_VERSION_BOUND, i * version_space_per_asic | ((i * version_space_per_asic + version_space_per_asic - 1) << 16), found[i]);
      batch.push_back(batchtype().Read(current_board, found[i],E_HIT0));
      }
   for (i = 0; i < 256; i++) {
      randomcpy(seed, &headers[i], sizeof(headertype));
      headers[i].x.version = 0;
      }
   Load(headers[37], 64, 0, -1);
   WriteConfig(E_HIT2, 0);  // clear out any old hits so the checkers below don't get confused
   WriteConfig(E_HIT2, 0);
   WriteConfig(E_HIT1, 0);
   WriteConfig(E_HIT1, 0);
   WriteConfig(E_HIT0, 0);
   WriteConfig(E_HIT0, 0);
   WriteConfig(E_GENERAL_HIT_COUNT, 0);
   WriteConfig(E_GENERAL_TRUEHIT_COUNT, 0);
   WriteConfig(E_DROPPED_HIT_COUNT, 0);
   time_t start = time(NULL);

   while (true) {
      unsigned elapsed = time(NULL) - start;
      if (last_load / 10 != elapsed / 10) {  // load new job every 10 seconds
         Load(headers[iterations % 256], iterations % 256, 32, 0);
         iterations++;
         last_load = elapsed;
         }

      bool hit = GetHitAuto(rsppacket)==E_NOTHING;
      if (!hit) {
//         int bist_reg = ReadConfig(E_BIST, batch[0].id);
//         if (bist_reg & 1) FATAL_ERROR;  // bist should have completed
//         bist_reg = (bist_reg >> 1) & 0x1ff;
//         if (bist_reg != 200) FATAL_ERROR;
         WriteConfig(E_BIST, 1, batch[0].id); // run bist
         Sleep(2000);
         }

      if (hit)
         {
         for (i = 256 - 1; i >= 0; i--)
            if (memcmp(headers[i].x.merkle, rsppacket.header.x.merkle, sizeof(rsppacket.header.x.merkle)) == 0)
               break;
         int zeroes = rsppacket.header.ReturnZeroes();   // this checks the result for correctness
         if (zeroes < 32 || zeroes != rsppacket.nbits) {
            printf("Hit returned had %d zeroes\n", zeroes);
            FATAL_ERROR;
            }
         if (i < 0) {
            printf("I had a hit that didn't merkle match any known headers!\n");
            FATAL_ERROR;
            }
         else if (i != rsppacket.sequence) {
            printf("I had a hit with the wrong sequence #!\n");
            FATAL_ERROR;
            }
         total_hits++;
         int timedifference = rsppacket.header.x.time - headers[i].x.time;
         int general_hits = 0;
         int true_hits = 0, dropped = 0;
         unsigned elapsed = time(NULL) - starttime;
/*         for (k = 0; k < batch.size(); k++) {
            general_hits += ReadConfig(E_GENERAL_HIT_COUNT, batch[k].id);
            true_hits += ReadConfig(E_GENERAL_TRUEHIT_COUNT, batch[k].id);
            dropped += ReadConfig(E_DROPPED_HIT_COUNT, batch[k].id);
            }*/
         printf("It%d: I saw a hit on context %3d, id %d, nonce %8x, header %3d, time=%x, nbits=%d (%d,%d,%d,%d,%.4f/sec)\n", iterations, rsppacket.header.x.version% version_space_per_asic, rsppacket.id, rsppacket.header.x.nonce, i, timedifference, rsppacket.nbits, total_hits, general_hits, true_hits, dropped, (float)total_hits / elapsed);
         __int64 key = ((__int64)rsppacket.header.x.nonce << 32) | rsppacket.header.x.merkle[0];
         if (std::find(repeats.begin(), repeats.end(), key) != repeats.end())
            {
            printf("A hit matched in the repeat list\n");
            FATAL_ERROR;
            }
         if (timedifference > 5) {
            printf("I saw a timedifference>0 which should happen with smallnonce=0\n");
            FATAL_ERROR;
            }
         repeats.push_back(key);

         statistics[rsppacket.id]++;
         if (iterations % 64 == 0)
            printf("It%d: asic0=%d asic1=%d %.4f/sec,%.4f/sec)\n", iterations, statistics[0], statistics[1], (float)statistics[0] / elapsed, (float)statistics[1] / elapsed);
         }
      }
   }


bool testtype::HeaderTest(int context, vector<int> &hits) {
   int i, k;
   response_hittype rsppacket;
   vector<batchtype> batch;

   if (found.size() == 0) found.push_back(eval_id);

   hits.clear();
   for (i = 0; i < block_headers.size(); i++) hits.push_back(0);
   WriteConfig(E_VERSION_SHIFT, 0, -1);
   WriteConfig(E_VERSION_BOUND, 0x0, -1);   // turn off asicboost
   WriteConfig(E_HITCONFIG, 0x20, -1);    // turn off autoreporting
   WriteConfig(E_HASHCONFIG, 0x0000, -1);    // turn off small nonce

   for (i = 0; i < found.size(); i++)
      {
      batch.push_back(batchtype().Read(1, found[i], E_SUMMARY));
      }


   printf("Testing block headers for context %d, ",context);
   int errors = 0;
   for (i = 0; i < block_headers.size(); i++) {
      const headertype& h = block_headers[i];
      int difficulty = h.ReturnZeroes();
      if (difficulty < 32)
         FATAL_ERROR;
      if (context != 0) {
         headertype h0 = h;
         headertype h1 = h;
         h0.x.version = 0;
         h0.x.prevblock[0] = 0;
         h0.x.prevblock[1] = 0;
         h0.x.prevblock[2] = 0;
         h0.x.prevblock[3] = 0;
         h0.x.prevblock[4] = 0;
         h0.x.prevblock[5] = 0;
         h0.x.prevblock[6] = 0;
         h0.x.prevblock[7] = 0;
         h0.x.merkle[0] = 0;
         h0.x.merkle[1] = 0;
         h0.x.merkle[2] = 0;
         h0.x.merkle[3] = 0;
         h0.x.merkle[4] = 0;
         h0.x.merkle[5] = 0;
         h0.x.merkle[6] = 0;
         h1.x.time = 0;
         h1.x.merkle[7] = 0;
         h1.x.difficulty = 0;
         Load(h0, i, difficulty, 0);
         Load(h1, i, difficulty, context);
         }
      else
         Load(h, i, difficulty, 0);
//      if (i % 100 == 0) printf("Testing %d, difficulty=%d, context=%d\n", i, difficulty,context);
      Delay(50);
      int tries;
      for (tries = 10; tries >= 0 && hits[i]<found.size(); tries--) {
         Delay(5);
         ReadWriteConfig(batch);
         for (k = 0; k < found.size(); k++) {
            if (batch[k].data & 0xf) {
               if (!GetHit(rsppacket, found[k])) {
                  printf("hit error\n");
//                  FATAL_ERROR;
                  }
               //               printf("It%d: I saw a hit on context %3d, version %X, id %d, nonce %8x, nbits=%d\n", i, rsppacket.command&0x3, rsppacket.header.x.version, rsppacket.id, rsppacket.header.x.nonce, rsppacket.nbits);
               if (rsppacket.header == h) {
                  if (false && context == 0 && rsppacket.command != (CMD_RETURNHIT | CMD_LOAD3)) {    // any context could happen for ctx0
                     printf("Incorrect context # returned for this packet!\n");
                     FATAL_ERROR;
                     }
                  if (context == 1 && rsppacket.command != (CMD_RETURNHIT | CMD_LOAD1)) {
                     printf("Incorrect context # returned for this packet!\n");
                     FATAL_ERROR;
                     }
                  if (context == 2 && rsppacket.command != (CMD_RETURNHIT | CMD_LOAD2)) {
                     printf("Incorrect context # returned for this packet!\n");
                     FATAL_ERROR;
                     }
                  if (context == 3 && rsppacket.command != (CMD_RETURNHIT | CMD_LOAD3)) {
                     printf("Incorrect context # returned for this packet!\n");
                     FATAL_ERROR;
                     }
                  hits[i]++;
                  }
               }
            }
         }
      if (tries < 0) {
         printf("header %d only produced %d hits\n", i, hits[i]);
         errors++;
         }
      }
   return false;
   }

void testtype::NullHeaderTest(const vector<headertype>& headers, int context) {
   int i, k;
   unsigned data;
   response_hittype rsppacket;

   WriteConfig(E_VERSION_SHIFT, 0, -1);
   WriteConfig(E_VERSION_BOUND, 0, -1);   // turn off asicboost
   WriteConfig(E_HITCONFIG, 0x20, -1);    // turn off autoreporting
   WriteConfig(E_HASHCONFIG, 0x0000, -1);    // turn off small nonce

   int errors = 0;
   for (i = 0; i < headers.size(); i++) {
      const headertype& h = headers[i];
      int difficulty = h.ReturnZeroes();
      if (difficulty < 32)
         FATAL_ERROR;
      Load(h, i, difficulty+1, 0);
      if (i % 100 == 0) printf("Testing %d, difficulty=%d\n", i, difficulty);
      Sleep(350);
      int tries, hits = 0;
      for (tries = 100; tries >= 0 && hits < found.size(); tries--) {
         Sleep(5);
         for (k = 0; k < found.size(); k++) {
            while (true) {
               data = ReadConfig(E_SUMMARY, found[k]);
               if ((data & 0xf) <= 0) break;
               if (!GetHit(rsppacket, found[k])) FATAL_ERROR;
               if (rsppacket.header == h) {
                  printf("A hit was found when the difficulty bits weren't met\n");
                  FATAL_ERROR;
                  }
               }
            }
         }
      }
   }


void testtype::ReadHeaders(const char* filename)
   {
   TAtype ta("ReadHaeaders()");
   filetype f;
   
   f.fopen(filename, "rt");
   char buffer[1024];
   int line = 0;

   printf("Reading blockchain headers % s\n", filename);
   if (!f.IsOpen()) {
      printf("Couldn't open %s for reading\n", filename);
      FATAL_ERROR;
      }
   while (NULL != f.fgets(buffer, sizeof(buffer) - 1)) {
      headertype h;
      line++;
      h.AsciiIn(buffer);
      if (h.ReturnZeroes()>=32)
         block_headers.push_back(h);
      }
   f.fclose();
   printf("Found %d headers\n", block_headers.size());
   }

void Test() {

   vector<topologytype> topology_1board{topologytype(0,0,0),topologytype(0,1,1),topologytype(0,2,2),topologytype(1,0,128),topologytype(1,1,129),topologytype(1,2,130),topologytype(2,0,3),topologytype(2,1,4),topologytype(2,2,5),topologytype(3,0,131),topologytype(3,1,132),topologytype(3,2,133),topologytype(4,0,6),topologytype(4,1,7),topologytype(4,2,8),topologytype(5,0,134),topologytype(5,1,135),topologytype(5,2,136),topologytype(6,0,9),topologytype(6,1,10),topologytype(6,2,11),topologytype(7,0,137),topologytype(7,1,138),topologytype(7,2,139),topologytype(8,0,12),topologytype(8,1,13),topologytype(8,2,14),topologytype(9,0,140),topologytype(9,1,141),topologytype(9,2,142),topologytype(10,0,15),topologytype(10,1,16),topologytype(10,2,17),topologytype(11,0,18),topologytype(11,1,19),topologytype(11,2,20),topologytype(12,0,143),topologytype(12,1,144),topologytype(12,2,145),topologytype(13,0,21),topologytype(13,1,22),topologytype(13,2,23),topologytype(14,0,146),topologytype(14,1,147),topologytype(14,2,148),topologytype(15,0,24),topologytype(15,1,25),topologytype(15,2,26),topologytype(16,0,149),topologytype(16,1,150),topologytype(16,2,151),topologytype(17,0,27),topologytype(17,1,28),topologytype(17,2,29),topologytype(18,0,152),topologytype(18,1,153),topologytype(18,2,154),topologytype(19,0,30),topologytype(19,1,31),topologytype(19,2,32),topologytype(20,0,155),topologytype(20,1,156),topologytype(20,2,157),topologytype(21,0,33),topologytype(21,1,34),topologytype(21,2,35),topologytype(22,0,36),topologytype(22,1,37),topologytype(22,2,38),topologytype(23,0,158),topologytype(23,1,159),topologytype(23,2,160),topologytype(24,0,39),topologytype(24,1,40),topologytype(24,2,41),topologytype(25,0,161),topologytype(25,1,162),topologytype(25,2,163),topologytype(26,0,42),topologytype(26,1,43),topologytype(26,2,44),topologytype(27,0,164),topologytype(27,1,165),topologytype(27,2,166),topologytype(28,0,45),topologytype(28,1,46),topologytype(28,2,47),topologytype(29,0,167),topologytype(29,1,168),topologytype(29,2,169),topologytype(30,0,48),topologytype(30,1,49),topologytype(30,2,50),topologytype(31,0,170),topologytype(31,1,171),topologytype(31,2,172),topologytype(32,0,51),topologytype(32,1,52),topologytype(32,2,53),topologytype(33,0,54),topologytype(33,1,55),topologytype(33,2,56),topologytype(34,0,173),topologytype(34,1,174),topologytype(34,2,175),topologytype(35,0,57),topologytype(35,1,58),topologytype(35,2,59),topologytype(36,0,176),topologytype(36,1,177),topologytype(36,2,178),topologytype(37,0,60),topologytype(37,1,61),topologytype(37,2,62),topologytype(38,0,179),topologytype(38,1,180),topologytype(38,2,181),topologytype(39,0,63),topologytype(39,1,64),topologytype(39,2,65),topologytype(40,0,182),topologytype(40,1,183),topologytype(40,2,184),topologytype(41,0,66),topologytype(41,1,67),topologytype(41,2,68),topologytype(42,0,185),topologytype(42,1,186),topologytype(42,2,187),topologytype(43,0,69),topologytype(43,1,70),topologytype(43,2,71)};


   testtype t(2, topology_1board);
   return;
   t.InitialSetup();
   t.PrintSystem();
//   t.DVFS(50);
   t.PrintSystem();
   while (true) Sleep(100);
   return;

   uint32 data = 0;
   vector<batchtype> batch;
   int i;



   /*
   for (i = 0; i < 3; i++) {
      t.serial.SetBaudRate(115200);
      Sleep(200);
      if (t.IsAlive(0)) {
         printf("refclk is 25Mhz\n");
         t.refclk = 25e6;
         }
      else {
         t.serial.SetBaudRate(170500);
         Sleep(200);
         if (t.IsAlive(0)) {
            printf("refclk is 37Mhz\n");
            t.refclk = 37e6;
            }
         else
            {
            t.serial.SetBaudRate(262650);
            Sleep(200);
            if (t.IsAlive(0)) {
               printf("refclk is 57Mhz\n");
               t.refclk = 57e6;
               }
            else FATAL_ERROR;
            }
         }
      if (t.BoardEnumerate()) 
         return;
      t.BistParedo();
      t.WriteConfig(E_HITCONFIG, 0x80000006, -1);
      t.WriteConfig(E_HITCONFIG, 0xc0000006, -1); // toggle refclk
      Sleep(1000);
      }
   return;
*/

   t.SetBaud(5000000);

   t.WriteConfig(E_HITCONFIG, 0x0, -1);
   t.WriteConfig(E_HITCONFIG, 0x80000006, -1);   // disable collision
   if (t.BoardEnumerate()) return;

//   t.SpeedSearch(100, 500, 0);
//   t.SpeedSearch(100, 500, 1);
//   t.BistParedo();
   t.Pll(350, -1);
   t.RunBist(0);
   t.RunBist(1);
   t.MineTest();
   /*
   t.WriteConfig(E_CLOCK_RETARD, 0x000, -1);
   t.RunBist(0);
   t.RunBist(1);
   t.MineTest();
   return;*/
   t.ReadHeaders("data\\block_headers.txt.gz");
   vector<headertype> pruned;
   for (i = 0; i < t.block_headers.size(); i++) {
      const headertype& h = t.block_headers[i];
      if ((h.x.nonce & 0xff) != 0)
         pruned.push_back(h);
      else if ((h.x.nonce & 0xff) == 8)
         pruned.push_back(h);
      if ((h.x.nonce & 0xff) <=31)
         pruned.push_back(h);
      }
   t.block_headers = pruned;
   printf("Found %d headers that are testable\n", pruned.size());
/*   t.HeaderTest(0);
   t.HeaderTest(1);
   t.HeaderTest(2);
   t.HeaderTest(3);*/
   t.MineTest();
   return;


   t.FrequencyEstimate();
   for (i = 0; i < t.found.size(); i++) {
      t.ReadWriteStress(t.found[i], 10);
      t.RunBist(t.found[i]);
      }

   return;
   for (i = 100; i < 950; i += 10) {
      t.Pll(i, 0);
      t.Pll(i, 1);
      //      t.FrequencyEstimate(1, refclk, hashclk);
      if (202 == t.RunBist(0)) break;
      if (202 == t.RunBist(1)) break;
      }
   return;

   printf("Running bist");
   for (i = 0; i < t.found.size(); i++)
      t.RunBist(t.found[i]);
   t.MineTest();
   //   for (i = 0; i < t.found.size(); i++)
   //	  t.ReadWriteStress(t.found[i], 1);
   //   t.BaudSweep(t.found[0]);
   return;
   }



