#ifdef _MSC_VER
  #include "pch.h"
  #include "helper.h"
  #include "miner.h"
#else
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <math.h>
  #include <vector>
  #include <algorithm>
  #include <functional>
#endif
#include "dvfs.h"

const int pll_divider = 6;
const float overclock = 1.0;
float ths_to_tp_conversion = 1000 * 1000 * 3 / (250 * 4);

dvfstype::dvfstype(const vector<topologytype>& _topology, const systeminfotype& _systeminfo) : topology(_topology), systeminfo(_systeminfo)
   {
   DescribeSystem(_topology, _systeminfo);
   }

void dvfstype::DescribeSystem(const vector<topologytype>& _topology, const systeminfotype& _systeminfo)
   {
   int i, k;
   num_boards = 0, num_cols=0, num_rows=0;

   topology = _topology;
   systeminfo = _systeminfo;

   std::sort(topology.begin(), topology.end());
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      if (t.board < 0) FATAL_ERROR;
      num_boards = MAXIMUM(num_boards, t.board + 1);
      num_rows = MAXIMUM(num_rows, t.row + 1);
      num_cols = MAXIMUM(num_cols, t.col+ 1);
      }
   if (num_rows * num_cols * num_boards != topology.size()) FATAL_ERROR;

   // I want to split things into num_col*4 partitions so I can roughly adjust one partition independant of another. A partition should never contain more than 1 row element
   for (i = 0; i < num_cols * 4; i++)
      {
      partitions.push_back(vector<int>());
      for (k = 0; k < topology.size(); k++) {
         topologytype& t = topology[k];
         if ((t.col == i / 4) && (t.row % 4 == i % 4))
            partitions.back().push_back(k);
         }
      }
   int count = 0;
   for (i = 0; i < partitions.size(); i++)
      count += partitions[i].size();
   if (count != topology.size()) FATAL_ERROR;
   }


void dvfstype::InitialSetup()  // this will setup the PLL's 
   {
   int i;
   vector<batchtype> batch;
   float pll_multiplier = pll_divider / systeminfo.refclk * (1 << 20);
   const int vcosel = 2;
   const int div1 = 2, div2 = 1;
   int pll_divider = (div1 + 1) * (div2 + 1);
   pll_multiplier = pll_divider / systeminfo.refclk * (1 << 20);

   initial = true;
  
   for (i = 0; i < num_boards; i++) {
      // broadcast write to everyone
      batch.push_back(batchtype().Write(i, -1, E_PLLOPTION, 0x10000)); // refdiv=1
      batch.push_back(batchtype().Write(i, -1, E_PLLCONFIG, 0x1d + (div2 << 13) + (div1 << 10) + (vcosel << 8))); // vcosel=0, div1=1, div2=2 (nominal vcoclk 1.6-3.2Ghz, hashclk is 800Mhz-1.6Ghz, assume vco can go lower than spec
      batch.push_back(batchtype().Write(i, -1, E_PLLFREQ, systeminfo.min_frequency * pll_multiplier));

      if (systeminfo.refclk == 25.0)
         batch.push_back(batchtype().Write(i, -1, E_IPCFG, 0x1)); // ipclk is 6.25Mhz
      else if (systeminfo.refclk == 40.0)
         batch.push_back(batchtype().Write(i, -1, E_IPCFG, 0x2)); // ipclk is 5.0Mhz
      else if (systeminfo.refclk == 50.0)
         batch.push_back(batchtype().Write(i, -1, E_IPCFG, 0x2)); // ipclk is 6.25Mhz
      else if (systeminfo.refclk == 100.0)
         batch.push_back(batchtype().Write(i, -1, E_IPCFG, 0x3)); // ipclk is 6.25Mhz
      batch.push_back(batchtype().Write(i, -1, E_TEMPCFG, 0xd));  // turn on temp sensor
      batch.push_back(batchtype().Write(i, -1, E_SPEED_DELAY, 512));    // wait 512 cycles after changing pll before starting bist
      batch.push_back(batchtype().Write(i, -1, E_SPEED_INCREMENT, 1.0 * pll_multiplier));    // makde speed search happen in 1Mhz increments
      batch.push_back(batchtype().Write(i, -1, E_BIST_THRESHOLD, systeminfo.allowable_bad_engines));
      }

   for (i = 0; i < num_boards; i++) {
      // i want a little delay between turning on the temp sensor and clearing max_temp
      batch.push_back(batchtype().Write(i, -1, E_MAX_TEMP_SEEN, 0x0));  // clear max temp seen
      }
   for (i = 0; i < topology.size(); i++) {
      const topologytype& t = topology[i];
      batch.push_back(batchtype().Write(t.board, t.id, E_THERMALTRIP, t.InverseTemp(systeminfo.thermal_trip_temp)));  // clear max temp seen
      }
   ReadWriteConfig(batch);
   batch.clear();


   voltage = systeminfo.min_voltage;
   SetVoltage(voltage);
   Delay(500); // let PS settle to new value
   for (i = 0; i < num_boards; i++) {
      EnablePowerSwitch(i);
      Delay(350); // stagger turning on boards to be nice to the PS
      }
   current_THS = systeminfo.min_frequency * topology.size() / ths_to_tp_conversion;
   // we're now up an running at 
   }


void dvfstype::ProbeSubset(const vector<int>& subset, vector<int>& startf, vector<int>& maxf, vector<float>& temp)  // will return actual or theoretical THS
   {
   vector<batchtype> batch;
   int i, increment;

   if (startf.size() != topology.size()) FATAL_ERROR;
   if (maxf.size() != topology.size()) FATAL_ERROR;
   if (temp.size() != topology.size()) FATAL_ERROR;

   for (i = 0; i < subset.size(); i++)
      {
      int index = subset[i];
      const topologytype& t = topology[index];
      batch.push_back(batchtype().Read(t.board, t.id, E_PLLFREQ)); // read out the current frequency
      batch.push_back(batchtype().Read(t.board, t.id, E_TEMPERATURE)); // read out the current temp
      }

   ReadWriteConfig(batch);
   for (i = 0; i < subset.size(); i++) {
      int index = subset[i];
      startf[index] = batch[i * 2 + 0].data;  // note that frequency is in the internal asic format not something easily readable
      temp[index] = topology[index].Temp(batch[i * 2 + 1].data);
      }
   batch.clear();

   increment = 0.390625 * pll_divider / systeminfo.refclk * (1 << 20); // do speed search with .4Mhz step size
   for (i = 0; i < num_boards; i++)
      batch.push_back(batchtype().Write(i, -1, E_SPEED_INCREMENT, increment));
   for (i = 0; i < subset.size(); i++) {
      int index = subset[i];
      const topologytype& t = topology[index];

      batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, startf[index] * 0.98));
      batch.push_back(batchtype().Write(t.board, t.id, E_SPEED_UPPER_BOUND, startf[index] * 1.1));
      batch.push_back(batchtype().Write(t.board, t.id, E_BIST, 2)); // run bist
      }
   int readstart = batch.size();
   for (i = 0; i < subset.size(); i++)
      {
      int index = subset[i];
      const topologytype& t = topology[index];
      batch.push_back(batchtype().Read(t.board, t.id, E_PLLFREQ)); // read the bist fail frequency
      }
   ReadWriteConfig(batch);

   for (i = 0; i < subset.size(); i++)
      {
      int index = subset[i];
      const topologytype& t = topology[index];
      if (batch[readstart + i].id != t.id) FATAL_ERROR;
      maxf[index] = batch[readstart + i].data - increment; // read out the bist fail frequency
      }
   batch.clear();

   // Now I need to restore to the start frequency so I don't influence later measurements
   for (i = 0; i < subset.size(); i++) {
      int index = subset[i];
      const topologytype& t = topology[index];

      batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, startf[index]));
      }
   ReadWriteConfig(batch);
   batch.clear();
   }

float dvfstype::AdjustAll(float max_tp, float &system_maxtemp, float& total_overtemp, int col)
   {
   vector<batchtype> batch;
   vector<int> startf(topology.size(),0), maxf(topology.size(), 0), nextf(topology.size(), 0);
   vector<float> temp(topology.size(), 0.0);
   int i;
   const float pll_multiplier = pll_divider / systeminfo.refclk * (1 << 20);


   for (i = 0; i < partitions.size(); i++)
      {
      const vector<int>& subset = partitions[i];
      ProbeSubset(subset, startf, maxf, temp);
      }

   __int64 pll_sum = 0;
   for (i = 0; i < topology.size(); i++) {
      if (temp[i] + 1 > systeminfo.max_junction_temp)
         pll_sum += startf[i];
      else
         pll_sum += maxf[i];
      }
   float theoretical_tp = (float)pll_sum / pll_multiplier;
   float derate = MINIMUM(0.999, max_tp / theoretical_tp);
   int startrow = -1, minindex=-1, maxindex=-1;
   float mintemp = 0.0, maxtemp = 0.0;
   nextf = startf;
   system_maxtemp = -100.0;
   total_overtemp = 0.0;
   for (i = 0; i < topology.size(); i++) {
      const topologytype& t = topology[i];
      
      nextf[i] = MINIMUM(startf[i] * 1.01, maxf[i] * derate);
      system_maxtemp = MAXIMUM(system_maxtemp, temp[i]);
      total_overtemp += MAXIMUM(0, temp[i] - systeminfo.max_junction_temp);

      if (t.row != startrow)
         {
         startrow = t.row;
         mintemp = temp[i];
         maxtemp = temp[i];
         minindex = i;
         maxindex = i;
         }
      else {
         if (temp[i] <= mintemp) {
            mintemp = temp[i];
            minindex = i;
            }
         if (temp[i] >= maxtemp) {
            maxtemp = temp[i];
            maxindex = i;
            }
         }
      if (i == topology.size() - 1 || topology[i + 1].row != startrow)
         {
         if (false);
         else if (maxtemp - mintemp > 5)
            {
            int transfer = (maxf[minindex] * 0.99 - nextf[minindex])*(maxtemp-mintemp-5)/20.0;
            transfer += MAXIMUM(0, maxtemp - systeminfo.max_junction_temp) * nextf[maxindex] / 50.0;
            if (transfer > 0) {
               nextf[minindex] += transfer;
               nextf[maxindex] -= transfer;
               }
            }
         }
      }

   for (i = 0; i < topology.size(); i++) {
      const topologytype& t = topology[i];

      if (col < 0 || t.col == col)
         batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, MAXIMUM(systeminfo.min_frequency * pll_multiplier, nextf[i])));
      if (false &&t.row == 15) {
         printf("A[%d,%d] f=%.1fMhz, max=%.1fMhz %.1fC \t", t.row, t.col, startf[i] / pll_multiplier, maxf[i] / pll_multiplier, temp[i]);
         if (t.col == 2) printf("\n");
         }
      }
   ReadWriteConfig(batch);
   batch.clear();
   return theoretical_tp;
   }

float dvfstype::Optimize(float max_THS, bool &too_hot) // this function takes maximum TH/s and returns either the actual TH/s or the higher theoretical amount
   {
   vector<batchtype> batch;
   vector<float> frequency, temp;
   int its, itcount=0, same=0;
   float actual_tp, last_tp = 0.0, maxtemp, total_overtemp;
   bool way_off = false;

   float max_tp = max_THS * ths_to_tp_conversion;

   for (its = 0; its < 200; its++, itcount++) {
      actual_tp = AdjustAll(max_tp, maxtemp, total_overtemp);
      way_off =  actual_tp <= last_tp+10 && actual_tp < max_tp * 0.9;   
      if (way_off && its>10) break;  // we're way off so quit early, we need more voltage
      if (fabs(last_tp - actual_tp) < 5) same++; 
      if (same>5) break; // the algorithm has converged so stop
      if (actual_tp > max_tp * 1.02)   // we're already too fast so don't waste a bunch of time getting even faster
         break;
      last_tp = actual_tp;
      Delay(200);
      }
   for (its = 0; its < 10 && !way_off; its++, itcount++) {
      for (int c = 0; c < num_cols; c++) {
         actual_tp = AdjustAll(max_tp, maxtemp, total_overtemp, c);
         Delay(200);
         }
      }
   float actual_THS = actual_tp / ths_to_tp_conversion;
   float ps_power = ReadPower();
   bool too_much_power = ps_power >= systeminfo.max_power;
   too_hot = total_overtemp > 10.0 || too_much_power;
   if (actual_tp >= max_tp)
      printf("VDD:%.3fV Optimization done(%-3d iterations). %.1f TH/s requested and achieved. %.1f TH/s is possible at this operating point. MaxTj=%5.1fC/%.1fC PS=%.2fkW Eff=%.1f J/TH %s\n", voltage, itcount, max_THS, actual_THS,maxtemp,systeminfo.max_junction_temp, ps_power * 0.001, ps_power / actual_THS, too_much_power ? "PS Limit!":too_hot ? "*Too Hot!" : "");
   else
      printf("VDD:%.3fV Optimization done(%-3d iterations). %.1f TH/s requested but only %.1f TH/s was possible at this operating point. MaxTj=%5.1fC/%.1fC PS=%.2fkW Eff=%.1f J/TH %s\n", voltage, itcount, max_THS, actual_THS, maxtemp, systeminfo.max_junction_temp, ps_power*0.001, ps_power/actual_THS, too_much_power ? "PS Limit!" : too_hot?"*Too Hot!":"");

   initial = false;
   return actual_THS;
   }

bool dvfstype::DVFS(float THS)     // returns true if performance could not be achieved or some error happened
   {
   const float high_tolerance = 1.01;
   const float low_tolerance = 0.999;
   float actual_ths;
   bool last_was_too_high = false, too_hot=false, last_too_hot=false;

   printf("Starting DVFS, initial voltage is %.3fV, initial performance is %.1fTH/s, desired performance is %.1fTH/s.\n", voltage, current_THS, THS);
   while (true) {
      actual_ths = Optimize(THS, too_hot);
      if (!too_hot && last_too_hot) return true;
      last_too_hot = too_hot;
      if (!too_hot && actual_ths >= THS * low_tolerance && actual_ths < THS * high_tolerance) return false; // we're done, everything is good
      else if (actual_ths >= THS* low_tolerance || too_hot) {
         if (voltage - systeminfo.voltage_step < systeminfo.min_voltage) {
            printf("Supply minimum has been reached\n");
            return false; // we're as low as the PS can go
            }
         voltage -= systeminfo.voltage_step;
         SetVoltage(voltage);
         Delay(100);
         last_was_too_high = true;
         }
      // we're too low
      else if (last_was_too_high) {
         // this condition is to note that we were too high and now we're too low, so revert to back to the last setting and quit
         // this guarantees the loop will exit if tolerances are too small
         voltage += systeminfo.voltage_step;
         SetVoltage(voltage);
         return false;
         }
      else if (voltage >= systeminfo.max_voltage)
         { // we hit the supply maximum
         printf("Supply maximum has been reached\n");
         return true;
         }
      else if (actual_ths < THS * 0.8) {   // we're way off so make a big jump
         voltage = MINIMUM(voltage + systeminfo.voltage_step * 10, systeminfo.max_voltage);
         SetVoltage(voltage);
         }
      else if (actual_ths < THS * 0.9) {   // we're way off so make a medium jump
         voltage = MINIMUM(voltage + systeminfo.voltage_step * 5, systeminfo.max_voltage);
         SetVoltage(voltage);
         }
      else {
         voltage = MINIMUM(voltage + systeminfo.voltage_step * 1, systeminfo.max_voltage);
         SetVoltage(voltage);
         }
//      PrintSystem();
//      return false;
      }
   return false;
   }


// this will get the max performance while maintaining die temp(but be careful of thermal time constant)
void dvfstype::SimpleOptimize()
   {
   vector<batchtype> batch;
   vector<int> frequency;
   vector<float> temp;
   int its, i;
   uint32 increment = -1;
   float bounds = 1.0;

   for (its = initial ? 0 : 20; its < 50; its++) {
      for (i = 0; i < topology.size(); i++) {
         topologytype& t = topology[i];
         batch.push_back(batchtype().Read(t.board, t.id, E_PLLFREQ)); // read out the current frequency
         batch.push_back(batchtype().Read(t.board, t.id, E_TEMPERATURE)); // read out the current temp
         }
      if (its < 10) {
         increment = 1.0 * pll_divider / systeminfo.refclk * (1 << 20);
         for (i = 0; i < num_boards; i++)
            batch.push_back(batchtype().Write(i, -1, E_SPEED_INCREMENT, increment));
         bounds = 1.1;
         }
      else if (its < 20) {
         increment = 0.2 * pll_divider / systeminfo.refclk * (1 << 20);
         for (i = 0; i < num_boards; i++)
            batch.push_back(batchtype().Write(i, -1, E_SPEED_INCREMENT, increment));
         bounds = 1.02;
         }
      else {
         increment = 0.1 * pll_divider / systeminfo.refclk * (1 << 20);
         for (i = 0; i < num_boards; i++)
            batch.push_back(batchtype().Write(i, -1, E_SPEED_INCREMENT, increment));
         bounds = 1.01;
         }

      ReadWriteConfig(batch);
      frequency.clear();
      temp.clear();
      for (i = 0; i < topology.size(); i++) {
         frequency.push_back(batch[i * 2 + 0].data);  // note that frequency is in the internal asic format not something easily readable
         temp.push_back(topology[i].Temp(batch[i * 2 + 1].data));
         }
      batch.clear();

      for (i = 0; i < topology.size(); i++) {
         topologytype& t = topology[i];
         if (temp[i] > systeminfo.max_junction_temp)
            batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, frequency[i] * 0.97));
         else if (temp[i] + 1.0 > systeminfo.max_junction_temp) {
            batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, frequency[i] * 0.97));
            batch.push_back(batchtype().Write(t.board, t.id, E_SPEED_UPPER_BOUND, frequency[i] * 1.0));
            batch.push_back(batchtype().Write(t.board, t.id, E_BIST, 2)); // run bist
            }
         else {
            batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, frequency[i] * 0.97));
            batch.push_back(batchtype().Write(t.board, t.id, E_SPEED_UPPER_BOUND, frequency[i] * bounds));
            batch.push_back(batchtype().Write(t.board, t.id, E_BIST, 2)); // run bist
            }
         }
      ReadWriteConfig(batch);
      batch.clear();
      }
   // bist overshoot by 1 increment so backup just a bit
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, frequency[i] - 2 * increment));
      }
   ReadWriteConfig(batch);
   batch.clear();
   initial = false;
   }


void dvfstype::PrintSystem(bool temp_only)
   {
//   float pll_multiplier = pll_divider / systeminfo.refclk * (1 << 20);
   vector<batchtype> batch;
   vector<float> frequency, temp, voltage;
   int i;

   for (i = 0; i < topology.size(); i++)
      {
      const topologytype& t = topology[i];
      batch.push_back(batchtype().Read(t.board, t.id, E_PLLFREQ)); // read out the current frequency
      batch.push_back(batchtype().Read(t.board, t.id, E_TEMPERATURE)); // read out the current temp
      batch.push_back(batchtype().Read(t.board, t.id, E_VOLTAGE)); // read out the current temp
      }

   ReadWriteConfig(batch);
   for (i = 0; i < topology.size(); i++) {
      frequency.push_back((float)batch[i * 3 + 0].data / pll_multiplier);  // note that frequency is in the internal asic format not something easily readable
      temp.push_back(topology[i].Temp(batch[i * 3 + 1].data));
      voltage.push_back(topology[i].Voltage(batch[i * 3 + 2].data));
      }
   batch.clear();
   if (temp_only)
      {
      int physical_row;
      for (physical_row = 10; physical_row >= 0; physical_row--)
         {
         for (i = 0; i < topology.size(); i++)
            {
            topologytype& t = topology[i];
            if (t.row == physical_row || t.row == 21 - physical_row || t.row == physical_row + 22 || t.row == 43 - physical_row)
               printf("%3.0f ",temp[i]);
            }
         printf("\n");
         }
      return;
      }

   for (i = 0; i < topology.size(); i++)
      {
      const topologytype& t = topology[i];
      if (t.col == 0)
         printf("\nB:%d R:%2d, ", t.board, t.row);
      else
         printf(" | ");
      printf("%3.0fmV %4.1fC %3.2fGhz", voltage[i]*1000.0, temp[i], frequency[i] * 0.001);
      }
   printf("\n");
   }