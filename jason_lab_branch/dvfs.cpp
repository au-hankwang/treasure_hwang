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

const int pll_divider = 4;
float ths_to_tp_conversion = 1000 * 1000 * 3 / (250 * 4);

float topologytype::defaultVoltageGain = 0.0001011035;
float topologytype::defaultVoltageOffset = -0.276029;

bool RUNQUICK = false;


dvfstype::dvfstype(const vector<topologytype>& _topology, const systeminfotype& _systeminfo) : topology(_topology), systeminfo(_systeminfo)
   {
   DescribeSystem(_topology, _systeminfo);
   }

void dvfstype::DescribeSystem(const vector<topologytype>& _topology, const systeminfotype& _systeminfo)
   {
   int i, k;
   num_cols=0, num_rows=0;

   topology = _topology;
   systeminfo = _systeminfo;
   boards.clear();

   std::sort(topology.begin(), topology.end());
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      if (t.board < 0) FATAL_ERROR;
      for (k = boards.size() - 1; k >= 0; k--)
         if (boards[k] == t.board) break;
      if (k < 0) boards.push_back(t.board);
      num_rows = MAXIMUM(num_rows, t.row + 1);
      num_cols = MAXIMUM(num_cols, t.col+ 1);
      }
//   if (num_rows * num_cols * boards.size() != topology.size()) FATAL_ERROR;
   }

void dvfstype::PrintSystem(FILE* fptr, bool temp_only)
   {
   vector<batchtype> batch;
   vector<float> frequency, frequency2, temp, voltage;
   int i;

   if (fptr == NULL) fptr = stdout;

   for (i = 0; i < topology.size(); i++)
      {
      const topologytype& t = topology[i];
      batch.push_back(batchtype().Read(t.board, t.id, E_PLLFREQ)); // read out the current frequency
      batch.push_back(batchtype().Read(t.board, t.id, E_TEMPERATURE)); // read out the current temp
      batch.push_back(batchtype().Read(t.board, t.id, E_VOLTAGE)); // read out the current temp
      batch.push_back(batchtype().Read(t.board, t.id, E_PLL2)); // read out the current frequency
      }

   ReadWriteConfig(batch);
   for (i = 0; i < topology.size(); i++) {
      frequency.push_back((float)batch[i * 4 + 0].data / pll_multiplier);  // note that frequency is in the internal asic format not something easily readable
      frequency2.push_back((float)((batch[i * 4 + 3].data>>20)<<20) / pll_multiplier);  // note that frequency is in the internal asic format not something easily readable
      temp.push_back(topology[i].Temp(batch[i * 4 + 1].data));
      voltage.push_back(topology[i].Voltage(batch[i * 4 + 2].data));
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
               if (fptr == NULL) printf("%3.0f ", temp[i]); else fprintf(fptr, "%3.0f ", temp[i]);
            }
         if (fptr == NULL) printf("\n"); else fprintf(fptr, "\n");
         }
      return;
      }

   for (i = 0; i < topology.size(); i++)
      {
      const topologytype& t = topology[i];
      if (i == 0 || t.row != topology[i - 1].row) {
         if (fptr == NULL) printf("\nB:%d R:%2d, ", t.board, t.row); else fprintf(fptr, "\nB:%d R:%2d, ", t.board, t.row);
         }
      else {
         if (fptr == NULL) printf(" | "); else fprintf(fptr, " | ");
         }
      if (fptr == NULL) printf("%5.1fmV %4.1fC %6.1f/%6.1fMhz %5.1f%% %5.1f%%", voltage[i] * 1000.0, temp[i], frequency[i], frequency2[i], t.hitrate * 100.0, t.missionhitrate * 100.0);
      else fprintf(fptr, "%5.1fmV %4.1fC %6.1f/%6.1fMhz %5.1f%% %5.1f%%", voltage[i] * 1000.0, temp[i], frequency[i], frequency2[i], t.hitrate * 100.0, t.missionhitrate * 100.0);
      }
   if (fptr == NULL) printf("\n"); else fprintf(fptr, "\n");
   }

void dvfstype::FanManage(float avg_temp, float max_temp)
   {
   static time_t last_seconds;
   time_t seconds;
   
   time(&seconds);
   if (seconds == last_seconds) return;   // I want this loop to execute 1 a second otherwise integral term will be wrong
   last_seconds = seconds;
   
   float error = (avg_temp - systeminfo.optimal_temp) * 0.5;
   error += MAXIMUM(0, max_temp - systeminfo.optimal_temp-10) * 0.5;

   float proportional = error * 0.05;  // a 20C delta will change the fan 100%
   fan_integral += error * 0.5;
   if (fan_integral > 100.0) fan_integral = 100.0;
   if (fan_integral < -10.0) fan_integral = -10.0;

   fan_percentage = proportional + fan_integral * 0.01;
   fan_percentage = MAXIMUM(systeminfo.min_fan_percentage, fan_percentage);
   fan_percentage = MINIMUM(1.0, fan_percentage);
   if (max_temp >= systeminfo.max_junction_temp) {
      fan_percentage = 1.0;
      fan_integral = 100.0;
      }

//   printf("Fan: avg:%.1fC, max:%.1fC, P=%.2f I=%.2f Fan=%.2f%%\n",avg_temp,max_temp,proportional,fan_integral,fan_percentage*100.0);

   FanControl(fan_percentage);
   }

float dvfstype::MeasureHashRate(int period, bool& too_hot, float& avg_temp, float& max_temp, int experiment) {
   vector<batchtype> batch_start, batch_end, bist_start, bist_end;
   vector<float> seconds(topology.size(), 0.0);
   int i, end_entries = 0;
   const int bist_iterations = 100;

   too_hot = false;

   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      batch_start.push_back(batchtype().Write(t.board, t.id, E_CLKCOUNT, 0));
      batch_start.push_back(batchtype().Write(t.board, t.id, E_HASHCLK_COUNT, 0));
      batch_start.push_back(batchtype().Write(t.board, t.id, E_GENERAL_TRUEHIT_COUNT, 0));
      batch_end.push_back(batchtype().ReadWrite(t.board, t.id, E_CLKCOUNT, 0));  // zero this everytime it's read
      batch_end.push_back(batchtype().ReadWrite(t.board, t.id, E_HASHCLK_COUNT, 0));  // zero this everytime it's read
      batch_end.push_back(batchtype().Read(t.board, t.id, E_GENERAL_TRUEHIT_COUNT));
      batch_end.push_back(batchtype().Read(t.board, t.id, E_PLLFREQ));
      batch_end.push_back(batchtype().Read(t.board, t.id, E_TEMPERATURE));
      batch_end.push_back(batchtype().Read(t.board, t.id, E_VOLTAGE));

      // once I've collected mission mode data, run bist. It will complete long before this long serial sequence is done
      bist_start.push_back(batchtype().Write(t.board, t.id, E_BIST_GOOD_SAMPLES, 0));
      bist_start.push_back(batchtype().Write(t.board, t.id, E_BIST, bist_iterations));

      bist_end.push_back(batchtype().Read(t.board, t.id, E_BIST_GOOD_SAMPLES));
      if (i == 0) end_entries = batch_end.size();
      }
   for (i = 0; i < boards.size(); i++) {
      batch_start.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      bist_start.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      }

   float expected_total, true_total, hit_rate, total_f = 0, bist_total = 0;
   ReadWriteConfig(batch_start);
   Delay(period);
   ReadWriteConfig(batch_end);
   ReadWriteConfig(bist_start);
   ReadWriteConfig(bist_end);
   expected_total = 0.0, true_total = 0.0;
   avg_temp = 0.0, max_temp = 0.0;
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      float f = batch_end[i * end_entries + 3].data / pll_multiplier;
      t.temperature = t.Temp(batch_end[i * end_entries + 4].data);
      t.voltage = t.Voltage(batch_end[i * end_entries + 5].data);
      // for long measurement times, E_CLKCOUNT will overflow. To prevent this zero it each interval and accumulate here
      seconds[i] += batch_end[i * end_entries + 0].data / 25.0e6;
      float hash_frequency = ((float)batch_end[i * end_entries + 1].data / batch_end[i * end_entries + 0].data) * 25.0 * 2.0;

      batch_end[i * end_entries + 0].data = 0;
      batch_end[i * end_entries + 1].data = 0;
      float expected = seconds[i] * f * 238 * (4 / 2.0) * 1.0e6 / ((__int64)1 << 32);
      expected_total += expected;
      true_total += batch_end[i * end_entries + 2].data;
      t.missionhitrate = batch_end[i * end_entries + 2].data / expected;
      t.hitrate = (float)bist_end[i].data / (bist_iterations * 8 * 238);
      total_f += f;
      bist_total += f * t.hitrate;

      if (fabs(f - hash_frequency) > f * 0.10 && period <= 500)
         printf("I see PLL=%.1fMhz expecting %.1fMhz. This is asic %d:%d\n", hash_frequency, f, t.board, t.id); // I'm just looking for gross errors here

      t.frequency = f;
      too_hot = too_hot || t.temperature > systeminfo.max_junction_temp;
      avg_temp += t.temperature;
      max_temp = MAXIMUM(max_temp, t.temperature);
      }
   avg_temp /= topology.size();
   FanManage(avg_temp, max_temp);
   hit_rate = true_total / expected_total;
//   hit_rate = bist_total / total_f;
   return hit_rate;
   }

float dvfstype::MeasureHashRate2(int period, bool& too_hot, float& avg_temp, float& max_temp, int experiment) {
   vector<batchtype> batch_start, batch_start2, batch_end, batch_end2, bist_start, bist_end, bist_end2;
   vector<float> seconds(topology.size(), 0.0);
   vector<float> seconds2(topology.size(), 0.0);
   int i, end_entries = 0;
   const int bist_iterations = 100;

   too_hot = false;

   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      batch_start.push_back(batchtype().Write(t.board, t.id, E_CLKCOUNT, 0));
      batch_start.push_back(batchtype().Write(t.board, t.id, E_HASHCLK_COUNT, 0));
      batch_start.push_back(batchtype().Write(t.board, t.id, E_GENERAL_TRUEHIT_COUNT, 0));
      for (int k=0; k<8; k++)
         batch_start.push_back(batchtype().Write(t.board, t.id, E_ENGINEMASK + k, t.partition[k]));

      batch_start2.push_back(batchtype().Write(t.board, t.id, E_CLKCOUNT, 0));
      batch_start2.push_back(batchtype().Write(t.board, t.id, E_HASHCLK_COUNT, 0));
      batch_start2.push_back(batchtype().Write(t.board, t.id, E_GENERAL_TRUEHIT_COUNT, 0));
      for (int k = 0; k < 8; k++)
         batch_start2.push_back(batchtype().Write(t.board, t.id, E_ENGINEMASK + k, ~t.partition[k]));

      batch_end.push_back(batchtype().ReadWrite(t.board, t.id, E_CLKCOUNT, 0));  // zero this everytime it's read
      batch_end.push_back(batchtype().ReadWrite(t.board, t.id, E_HASHCLK_COUNT, 0));  // zero this everytime it's read
      batch_end.push_back(batchtype().Read(t.board, t.id, E_GENERAL_TRUEHIT_COUNT));
      batch_end.push_back(batchtype().Read(t.board, t.id, E_PLLFREQ));
      batch_end.push_back(batchtype().Read(t.board, t.id, E_TEMPERATURE));

      // once I've collected mission mode data, run bist. It will complete long before this long serial sequence is done
      bist_start.push_back(batchtype().Write(t.board, t.id, E_BIST_GOOD_SAMPLES, 0));
      bist_start.push_back(batchtype().Write(t.board, t.id, E_BIST, bist_iterations));

      bist_end.push_back(batchtype().Read(t.board, t.id, E_BIST_GOOD_SAMPLES));
      if (i == 0) end_entries = batch_end.size();
      }
   batch_end2 = batch_end;
   bist_end2 = bist_end;
   for (i = 0; i < boards.size(); i++) {
      batch_start.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      batch_start2.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      bist_start.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      }

   float expected_total, true_total, hit_rate, total_f = 0, bist_total = 0;
   ReadWriteConfig(batch_start);
   Delay(period);
   ReadWriteConfig(batch_end);
   ReadWriteConfig(bist_start);
   ReadWriteConfig(bist_end);

   ReadWriteConfig(batch_start2);
   Delay(period);
   ReadWriteConfig(batch_end2);
   ReadWriteConfig(bist_start);
   ReadWriteConfig(bist_end2);
   expected_total = 0.0, true_total = 0.0;
   avg_temp = 0.0, max_temp = 0.0;
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      t.temperature = t.Temp(batch_end[i * end_entries + 4].data);
      // for long measurement times, E_CLKCOUNT will overflow. To prevent this zero it each interval and accumulate here
      seconds[i] += batch_end[i * end_entries + 0].data / 25.0e6;
      seconds2[i] += batch_end2[i * end_entries + 0].data / 25.0e6;
      float expected = seconds[i] * t.frequency * 119 * (4 / 2.0) * 1.0e6 / ((__int64)1 << 32);
      float expected2 = seconds2[i] * t.frequency2 * 119 * (4 / 2.0) * 1.0e6 / ((__int64)1 << 32);
      expected_total += expected;
      expected_total += expected2;
      true_total += batch_end[i * end_entries + 2].data;
      true_total += batch_end2[i * end_entries + 2].data;
      t.missionhitrate = batch_end[i * end_entries + 2].data / expected;
      t.missionhitrate2 = batch_end2[i * end_entries + 2].data / expected;
      t.hitrate = (float)bist_end[i].data / (bist_iterations * 8 * 119);
      t.hitrate2 = (float)bist_end2[i].data / (bist_iterations * 8 * 119);
      total_f += t.frequency + t.frequency2;
      bist_total += t.frequency * t.hitrate + t.frequency2*t.hitrate2;

      too_hot = too_hot || t.temperature > systeminfo.max_junction_temp;
      avg_temp += t.temperature;
      max_temp = MAXIMUM(max_temp, t.temperature);
      }
   avg_temp /= topology.size();
   FanManage(avg_temp, max_temp);


   hit_rate = true_total / expected_total;
   hit_rate = bist_total / total_f;
   return hit_rate;
   }

float dvfstype::MeasureHashRateHybrid(int period, bool& too_hot, float& avg_temp, float& max_temp, int experiment) {
   vector<batchtype> batch;
   vector<topologytype> phase2, phase4;
   float hitrate = 0.0;
   int i;

   batch.clear();
   for (i = 0; i < boards.size(); i++) {
      batch.push_back(batchtype().Write(boards[i], -1, E_DUTY_CYCLE, 0x80a0));
      batch.push_back(batchtype().Write(boards[i], -1, E_HASHCONFIG, 1 << 16));
      batch.push_back(batchtype().Write(boards[i], -1, E_HASHCONFIG, 0));
      batch.push_back(batchtype().Write(boards[i], -1, E_HASHCONFIG, 0));
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      }
   if (experiment != 1) {
      ReadWriteConfig(batch);
      hitrate = MeasureHashRate(period / 2, too_hot, avg_temp, max_temp, experiment);
      }
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      t.phase2 = 1.0;
      }
   phase2 = topology;
   if (experiment == 0) return hitrate;

   batch.clear();
   for (i = 0; i < boards.size(); i++) {
      batch.push_back(batchtype().Write(boards[i], -1, E_DUTY_CYCLE, 0));
      batch.push_back(batchtype().Write(boards[i], -1, E_HASHCONFIG, 1 << 16));
      batch.push_back(batchtype().Write(boards[i], -1, E_HASHCONFIG, 4));
      batch.push_back(batchtype().Write(boards[i], -1, E_HASHCONFIG, 4));
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      }
   if (experiment != 0) {
      ReadWriteConfig(batch);
      hitrate = MeasureHashRate(period / 2, too_hot, avg_temp, max_temp, experiment);
      }
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      t.phase2 = 0.0;
      }
   phase4 = topology;
   if (experiment == 1) return hitrate;

   float hit_rate, total_f = 0, bist_total = 0;
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      float f = t.frequency;

      total_f += f;
      t.hitrate = MAXIMUM(phase2[i].hitrate, phase4[i].hitrate);
      t.missionhitrate = MAXIMUM(phase2[i].missionhitrate, phase4[i].missionhitrate);
      bist_total += f * t.hitrate;
      }
   hit_rate = bist_total / total_f;

   batch.clear();
   for (i = 0; i < boards.size(); i++) {
      topologytype& t = topology[i];
      t.phase2 = 0.0;
      if (phase2[i].hitrate > phase4[i].hitrate) {
         batch.push_back(batchtype().Write(t.board, t.id, E_DUTY_CYCLE, 0x80a0));
         batch.push_back(batchtype().Write(t.board, t.id, E_HASHCONFIG, 0));
         t.phase2 = 1.0;
         }
      }
   for (i = 0; i < boards.size(); i++)
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
   ReadWriteConfig(batch);

   return hit_rate;
   }

void dvfstype::PrintSummary(FILE *fptr, const char* label, float &efficiency)
   {
   int i;
   float maxtemp = 0.0, avgtemp=0.0, average_v = 0.0, total_f=0.0, effective_f = 0.0, meffective_f = 0.0;
   float minhit = 100.0, maxhit = 0.0, phase2_total=0.0;

   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      maxtemp = MAXIMUM(maxtemp, t.temperature);
      avgtemp += t.temperature;
      average_v += t.voltage;
      effective_f += t.frequency * t.hitrate;
      meffective_f += t.frequency * t.missionhitrate;
      total_f += t.frequency;
      minhit = MINIMUM(minhit, t.hitrate);
      maxhit = MAXIMUM(maxhit, t.hitrate);
      phase2_total += t.phase2*t.frequency;
      }
   avgtemp /= topology.size();
   float perf = meffective_f * 238 * (4 / 2) * 1.0e-6;   // fixme
   float power = ReadPower();
   efficiency = power / perf;
   float ir = voltage - average_v / (topology.size()/(float)num_rows);
   float hitrate = effective_f / total_f;
   float mhitrate = meffective_f / total_f;
   char buffer[256];
//   sprintf(buffer,"%sVoltage=%.3fV/%.1fmV %.0fW hitrate=%5.1f%%(%5.1f) Perf=%.1fTH/s Eff=%.2fJ/TH AvgTemp=%.1fC MaxTemp=%.1fC IR=%.1fmV Fan=%.0f%%\n", label, voltage, average_v / topology.size() * 1000.0, power, hitrate * 100.0, (maxhit-minhit)*100.0, perf, MINIMUM(99.99,efficiency), avgtemp, maxtemp, ir * 1000.0,fan_percentage*100.0);
   sprintf(buffer, "%sVoltage=%.3fV/%.1fmV %.0fW Bhit=%5.1f%% Mhit=%5.1f%% Perf=%.1fTH/s Eff=%.2fJ/TH AvgTemp=%.1fC MaxTemp=%.1fC Phase2=%5.1f%% Fan=%.0f%%\n", label, voltage, average_v / topology.size() * 1000.0, power, hitrate * 100.0, 100.0*mhitrate, perf, MINIMUM(99.99, efficiency), avgtemp, maxtemp, 100.0*phase2_total/total_f,fan_percentage * 100.0);
   if (fptr != NULL) fputs(buffer, fptr);
   puts(buffer);
   }

void dvfstype::CalibrateDVM()  // this will setup the PLL's 
   {
   int i, k;
   vector<batchtype> batch;
   printf("Calibrating DVM\n");

   for (i = 0; i < boards.size(); i++) {
      int channel = 24; // this will be zero voltage
      int b = boards[i];
      if (systeminfo.refclk == 25.0)
         batch.push_back(batchtype().Write(b, -1, E_IPCFG, 0x1)); // ipclk is 6.25Mhz
      else FATAL_ERROR;
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x2));
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x3));
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x1));
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x9 | (channel << 12)));
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x5));
      batch.push_back(batchtype().Read(b, 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      }
   ReadWriteConfig(batch);
   batch.clear();

   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      batch.push_back(batchtype().Read(t.board, t.id, E_VOLTAGE));
      }
   vector <float> avg(topology.size(), 0.0);
   for (k = 0; k < 4; k++) {
      Delay(50);
      ReadWriteConfig(batch);
      for (i = 0; i < topology.size(); i++) {
         topologytype& t = topology[i];
         avg[i] += t.Voltage(batch[i].data);
         }
      }
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      t.ChangeVoltageParameters(1.0, -avg[i] / 4.0);  // zero the offset
      }
   batch.clear();
   for (i = 0; i < boards.size(); i++) {
      int channel = 15; // this will be scv voltage(0.795)
      int b = boards[i];
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x2));
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x3));
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x1));
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x9 | (channel << 12)));
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x5));
      batch.push_back(batchtype().Read(b, 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      }
   ReadWriteConfig(batch);
   batch.clear();
   for (i = 0; i < topology.size(); i++) {
      avg[i] = 0.0;
      }
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      batch.push_back(batchtype().Read(t.board, t.id, E_VOLTAGE));
      }
   for (k = 0; k < 4; k++) {
      Delay(50);
      ReadWriteConfig(batch);
      for (i = 0; i < topology.size(); i++) {
         topologytype& t = topology[i];
         avg[i] += t.Voltage(batch[i].data);
         }
      }
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
//      printf("%d:%3d SCV=%.1fmV\n", t.board, t.id, avg[i]/4.0 * 1000.0);
      float v = avg[i] / 4.0;
      if (v > 0.830 || v < 0.770)
	 printf("The SCV calibration voltage is too far off. Expecting 795 but got %.0fmV (%d:%d)\n",v*1000.0,t.board,t.id);
      t.ChangeVoltageParameters(0.795/v,0.0);  // adjust the gain
      }
   ReadWriteConfig(batch);
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      float foo= t.Voltage(batch[i].data);
      }
   batch.clear();
   for (i = 0; i < boards.size(); i++) {
      int channel = 5; // return to core voltage channel
      int b = boards[i];
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x2));
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x3));
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x1));
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x9 | (channel << 12)));
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x5));
      batch.push_back(batchtype().Read(b, 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      }
   ReadWriteConfig(batch);
   }


void dvfstype::MinPower()
   {
   int i;
   vector<batchtype> batch;
   // I want to iteratively lower everyone to avoid a discontinuity which can cause a spurious reset
   while (true) {
      bool completed = true;
      for (int stagger = 0; stagger < num_cols; stagger++) {
         for (i = stagger; i < topology.size(); i += num_cols) {
            topologytype& t = topology[i];

            if (t.frequency > systeminfo.min_frequency)
               {
               completed = false;
               t.frequency = MAXIMUM(systeminfo.min_frequency, t.frequency * 0.7);
               }
            batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, t.frequency * pll_multiplier));
            }
         }
      for (i = 0; i < boards.size(); i++)
         batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      ReadWriteConfig(batch);
      batch.clear();
      if (completed) break;
      }
   SetVoltage(systeminfo.min_voltage);
   }

void dvfstype::Mine()
   {
   struct _timeb start, current;
   vector<batchtype> batch, hitbatch;
   bool initial_empty = false;
   int i, hits, difficulty,lastsecond, interval;
   float lastrate = 0.0;

   for (i = 0; i < topology.size(); i++)
      {
      topologytype& t = topology[i];
      batch.push_back(batchtype().Read(t.board, t.id, E_SEQUENCE));
      batch.push_back(batchtype().Write(t.board, t.id, E_HIT0, 0));
      batch.push_back(batchtype().Write(t.board, t.id, E_HIT1, 0));
      batch.push_back(batchtype().Write(t.board, t.id, E_HIT2, 0));
      batch.push_back(batchtype().Write(t.board, t.id, E_HIT0, 0));
      batch.push_back(batchtype().Write(t.board, t.id, E_HIT1, 0));
      batch.push_back(batchtype().Write(t.board, t.id, E_HIT2, 0));
      }
   ReadWriteConfig(batch);
   difficulty = batch[0].data>>8;
   for (i = 0; i < topology.size(); i++)
      {
      topologytype& t = topology[i];
      if ((batch[i * 7 + 0].data>>8) != difficulty) // make sure all chips are doing the same difficulty
         FATAL_ERROR;
      }
   batch.clear();
   _ftime(&start);
   hits = 0;
   interval = 0;
   lastsecond = start.time;

   while (true) {
      for (i = 0; i < topology.size(); i++)
         {
         topologytype& t = topology[i];
         batch.push_back(batchtype().Read(t.board, t.id, E_SUMMARY));
         }
      ReadWriteConfig(batch);
      _ftime(&current);
      int seconds = (current.time - start.time);
      
      if (current.time - lastsecond >= 60)
         {
         lastrate = ((__int64)1 << 45) * interval / (1.0e12 * (current.time - lastsecond));

         lastsecond = current.time;
         interval = 0;
         }
      for (i = 0; i < topology.size(); i++)
         {
         topologytype& t = topology[i];
         
         int hitswaiting = batch[i].data & 0x7;
         if (batch[i].data & 0x7)
            {
            response_hittype rsppacket;
            GetHit(rsppacket, t.board, t.id);

            hits ++;
            interval ++;
            float implied = ((__int64)1 << 45) * hits / (1.0e12 * seconds);
            
            printf("Asic %3d nbits=%d ctx=%d, totalhits=%4d, second=%d, implied=%.2fTH/s, interval=%.2fTH/s difficulty=%d\n",i,rsppacket.nbits,rsppacket.command&3,hits, seconds,implied,lastrate,difficulty);
            }
         }
      float efficiency, avg_temp, max_temp;
      bool too_hot;
      MeasureHashRate(5 * 1000, too_hot, avg_temp, max_temp, 0);
      PrintSummary(NULL, "test", efficiency);
      }
   }


void dvfstype::InitialSetup()  // this will setup the PLL's 
   {
   int i;
   vector<batchtype> batch;
   const int vcosel = 2;
   const int div1 = 1, div2 = 1; // I want the vco reasonably slow to save power but still achieve low min frequency
   const int refdiv = 5;
   int pll_divider = (div1 + 1) * (div2 + 1) * refdiv;
   pll_multiplier = pll_divider / systeminfo.refclk * (1 << 20);

   fan_integral = 50;
   fan_percentage = 0.8;         // start with a decent fan speed in case the system is already hot
   FanControl(fan_percentage);
   initial = true;
   last_optimization_temp = -100.0;

   for (i = 0; i < topology.size(); i++)
      {
      const topologytype& t = topology[i];
      batch.push_back(batchtype().Read(t.board, t.id, E_UNIQUE));
      ReadWriteConfig(batch);
      if (batch[0].data != 0x61727541)
         printf("Board %d Asic %d did not respond\n", t.board, t.id);
      batch.clear();
      }

//   CalibrateDVM();

   for (i = 0; i < boards.size(); i++) {
      // broadcast write to everyone
      int b = boards[i];
      batch.push_back(batchtype().Write(b, -1, E_PLL2, 0));
      batch.push_back(batchtype().Write(b, -1, E_PLLCONFIG, 0x11 + (refdiv << 20) + (div1 << 13) + (div2 << 10)));  // WriteConfig(E_PLLCONFIG, 0x11 + (predivider << 20) + (best2 << 13) + (best1 << 10), id);
      batch.push_back(batchtype().Write(b, -1, E_PLLFREQ, systeminfo.min_frequency * pll_multiplier));

      if (systeminfo.refclk == 25.0)
         batch.push_back(batchtype().Write(b, -1, E_IPCFG, 0x1)); // ipclk is 6.25Mhz
      else if (systeminfo.refclk == 40.0)
         batch.push_back(batchtype().Write(b, -1, E_IPCFG, 0x2)); // ipclk is 5.0Mhz
      else if (systeminfo.refclk == 50.0)
         batch.push_back(batchtype().Write(b, -1, E_IPCFG, 0x2)); // ipclk is 6.25Mhz
      else if (systeminfo.refclk == 100.0)
         batch.push_back(batchtype().Write(b, -1, E_IPCFG, 0x3)); // ipclk is 6.25Mhz
      batch.push_back(batchtype().Write(b, -1, E_TEMPCFG, 0xd));  // turn on temp sensor
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x2));
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x3));
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x1));
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x9 | (5 << 12)));   // select channel 5 for voltage measurements
      batch.push_back(batchtype().Write(b, -1, E_DVMCFG, 0x5));
      batch.push_back(batchtype().Write(b, -1, E_DUTY_CYCLE, 0x80a0));
      batch.push_back(batchtype().Write(b, -1, E_HASHCONFIG, 1<<16));
      batch.push_back(batchtype().Write(b, -1, E_HASHCONFIG, 0));
      }
   for (i = 0; i < boards.size(); i++)
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
   ReadWriteConfig(batch);
   batch.clear();

   Delay(50);
   for (i = 0; i < boards.size(); i++) {
      int b = boards[i];
      // i want a little delay between turning on the temp sensor and clearing max_temp
      batch.push_back(batchtype().Write(b, -1, E_MAX_TEMP_SEEN, 0x0));  // clear max temp seen
      batch.push_back(batchtype().Write(b, -1, E_THERMALTRIP, topologytype().InverseTemp(systeminfo.thermal_trip_temp)));
      }
   for (i = 0; i < boards.size(); i++)
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
   ReadWriteConfig(batch);
   batch.clear();

   voltage = systeminfo.min_voltage;
   SetVoltage(voltage);

   for (int stagger = 0; stagger < num_cols; stagger++) {
      for (i = stagger; i < topology.size(); i += num_cols) {
         topologytype& t = topology[i];
         batch.push_back(batchtype().Write(t.board, t.id, E_PLLCONFIG, 0x12)); // put pll into bypass, the fixes board issue when engaging power switch
         batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, systeminfo.min_frequency * pll_multiplier));
         batch.push_back(batchtype().Write(t.board, t.id, E_PLLCONFIG, 0x11 + (refdiv << 20) + (div1 << 13) + (div2 << 10))); // vcosel=0, div1=1, div2=2 (nominal vcoclk 1.6-3.2Ghz, hashclk is 800Mhz-1.6Ghz, assume vco can go lower than spec
         batch.push_back(batchtype().Read(t.board, t.id, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
         ReadWriteConfig(batch);
         batch.clear();
         }
      }

   for (i = 0; i < boards.size(); i++)
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
   ReadWriteConfig(batch);
   batch.clear();

   Delay(500); // let PS settle to new value
   int enables = 0;
   for (i = 0; i < boards.size(); i++) {
      int b = boards[i];
      enables |= 1 << b;
      EnablePowerSwitch(enables);
      Delay(1000); // stagger turning on boards to be nice to the PS
      }
   
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      batch.push_back(batchtype().Read(t.board, t.id, E_UNIQUE));
      }
   ReadWriteConfig(batch);

   bool error = false;
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      if (batch[i].data != 0x61727541)
	 {
	 printf("Chip %d:%d did survive enabling the power switch\n",t.board,t.id);
	 error = true;
	 }
//      error = error || batch[i].data != 0x61727541;
      }
   if (error&&false) {
      printf("One of more chips didn't survive enabling the power switch.\n Shutting down now\n");
      DisableSystem();
      FanControl(0.4);
      return;
      }
   batch.clear();

   for (i = 0; i < boards.size(); i++) {
      int b = boards[i];
      batch.push_back(batchtype().Write(b, -1, E_PLLFREQ, systeminfo.min_frequency * pll_multiplier));
      batch.push_back(batchtype().Write(b, -1, E_PLLCONFIG, 0x11 + (refdiv << 20) + (div1 << 13) + (div2 << 10))); // vcosel=0, div1=1, div2=2 (nominal vcoclk 1.6-3.2Ghz, hashclk is 800Mhz-1.6Ghz, assume vco can go lower than spec
      }
   for (i = 0; i < boards.size(); i++)
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
   ReadWriteConfig(batch);
   batch.clear();
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      t.frequency = systeminfo.min_frequency;
      }
   }



struct ranktype {
   int index;
   float passing;
   float frequency;
   ranktype(int _index, float _passing, float _frequency) : index(_index), passing(_passing), frequency(_frequency)
      {}
   bool operator< (const ranktype& right) const
      {
      if (passing < right.passing) return true;
      if (passing > right.passing) return false;
      return frequency > right.frequency;
      }
   };

#ifdef foo
void dvfstype::PerEngineTweak2(int experiment)
   {
   vector<batchtype> batch, batch_start, batch_end;
   int i, b, e;
   const int best1 = 1, best2 = 1;
   const int predivider = 5;
   const float pll_multiplier = (best1 + 1) * (best2 + 1) * predivider / systeminfo.refclk * (1 << 20);
   const int bist_iterations = 100;
   const int num_engines = 238;
   const int pll_delay = 2000;
   const int bist_delay_ms = 1000.0 * pll_delay * ((2500 - 100) / 5) / 25.0e6;
   const int max_chips = 500;
   const float bist_threshold = 0.97;
   float phase2_speed[num_engines][max_chips], phase4_speed[num_engines][max_chips];

   if (topology.size() >= max_chips) FATAL_ERROR;
   printf("Running PerEngineTweak\n");
   FanControl(90);

   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];

      if (t.frequency<systeminfo.min_frequency || t.frequency > systeminfo.max_frequency) FATAL_ERROR;

      int fb_divider = t.frequency * (best1 + 1) * (best2 + 1) * predivider / systeminfo.refclk + 0.5;
      unsigned pll2_setting = 0x11 + (predivider << 12) + (best2 << 9) + (best1 << 6) + (fb_divider<<20);
      batch.push_back(batchtype().Write(t.board, t.id, E_PLL2, 0x12));  // reset the pll
      batch.push_back(batchtype().Write(t.board, t.id, E_PLL2, pll2_setting));
      }
   for (i = 0; i < boards.size(); i++)
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      
   ReadWriteConfig(batch);

   batch.clear();
   for (b = 0; b < boards.size(); b++) {
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 0, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 1, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 2, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 3, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 4, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 5, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 6, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 7, 0xffffffff));

      batch.push_back(batchtype().Write(boards[b], -1, E_DUTY_CYCLE, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_SPEED_INCREMENT, pll_multiplier * 5));
      batch.push_back(batchtype().Write(boards[b], -1, E_SPEED_DELAY, pll_delay));
      batch.push_back(batchtype().Write(boards[b], -1, E_SPEED_UPPER_BOUND, pll_multiplier * 2000));
      batch.push_back(batchtype().Write(boards[b], -1, E_BIST_THRESHOLD, (bist_iterations - 1) * 8 * bist_threshold));
      batch.push_back(batchtype().Write(boards[b], -1, E_HASHCONFIG, 0 + (1 << 3)));   // 2 phase with everyone using pll2
      }
   for (i = 0; i < boards.size(); i++) {
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      }
   ReadWriteConfig(batch);
   batch.clear();

   // now everything is pointing to pll2 running at the same frequency as before
   batch_end.clear();
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      batch_end.push_back(batchtype().Read(t.board, t.id, E_PLLFREQ));
      }

   for (e = 0; e < num_engines; e++)
      {
      batch_start.clear();
      for (b = 0; b < boards.size(); b++) {
         batch_start.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + (e / 32), 0xffffffff ^ (1 << (e & 31))));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_DUTY_CYCLE, 0x80a0));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_MINER_CONFIG, (e << 16) | (0 << 3) | 0));  // phase 2 with main pll
         batch_start.push_back(batchtype().Write(boards[b], -1, E_BIST_GOOD_SAMPLES, 0));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_PLLFREQ, 100.0 * pll_multiplier));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_BIST, bist_iterations + (1 << 31)));
         }
      for (i = 0; i < boards.size(); i++) {
         batch_start.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
         }
      ReadWriteConfig(batch_start);
      Delay(bist_delay_ms);
      ReadWriteConfig(batch_end);

      for (i = 0; i < topology.size(); i++) {
         topologytype& t = topology[i];
         phase2_speed[e][i] = batch_end[i].data / pll_multiplier;
         }

      batch_start.clear();
      for (b = 0; b < boards.size(); b++) {
         batch_start.push_back(batchtype().Write(boards[b], -1, E_DUTY_CYCLE, 0));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_MINER_CONFIG, (e << 16) | (0 << 3) | 4));  // phase 2 with main pll
         batch_start.push_back(batchtype().Write(boards[b], -1, E_BIST_GOOD_SAMPLES, 0));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_PLLFREQ, 100.0 * pll_multiplier));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_BIST, bist_iterations + (1 << 31)));
         }
      for (i = 0; i < boards.size(); i++) {
         batch_start.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
         }
      ReadWriteConfig(batch_start);
      Delay(bist_delay_ms);
      ReadWriteConfig(batch_end);

      for (i = 0; i < topology.size(); i++) {
         topologytype& t = topology[i];
         phase4_speed[e][i] = batch_end[i].data / pll_multiplier;
         }

      batch_start.clear();
      for (b = 0; b < boards.size(); b++) {
         batch_start.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + (e / 32), 0xffffffff));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_DUTY_CYCLE, 0x80a0));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_MINER_CONFIG, (e << 16) | (1 << 3) | 0));  // phase 2 with second pll
         }
      for (i = 0; i < boards.size(); i++) {
         batch_start.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
         }
      ReadWriteConfig(batch_start);
      }
   batch.clear();
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];

      batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, t.frequency * pll_multiplier));
      }
   for (i = 0; i < boards.size(); i++)
      batch_start.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
   ReadWriteConfig(batch);

   batch.clear();
   for (b = 0; b < boards.size(); b++) {
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 0, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 1, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 2, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 3, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 4, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 5, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 6, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 7, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_DUTY_CYCLE, 0x80a0));
      batch.push_back(batchtype().Write(boards[b], -1, E_HASHCONFIG, (1<<16) + 0 + (0 << 3)));   // reset the miner just in case the fast clock wedged us
      batch.push_back(batchtype().Write(boards[b], -1, E_HASHCONFIG, 0 + (0 << 3)));   // 2 phase with everyone using main pll
      batch.push_back(batchtype().Write(boards[b], -1, E_HASHCONFIG, 0 + (0 << 3)));   // 2 phase with everyone using main pll
      batch.push_back(batchtype().Write(boards[b], -1, E_DUTY_CYCLE, 0));  // write this once, it will only effect the miner_config writes below
      }
   for (i = 0; i < boards.size(); i++)
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
   ReadWriteConfig(batch);

   batch.clear();
   int phase2_count = 0, phase4_count = 0;
   for (e = 0; e < num_engines; e++)
      {
      for (i = 0; i < topology.size(); i++) {
         topologytype& t = topology[i];
         bool phase2_winner = phase2_speed[e][i] > phase4_speed[e][i];
         if (experiment==2)
            phase2_winner = true;
         else if (experiment == 3)
            phase2_winner = false;
         phase2_count += phase2_winner ? 1 : 0;
         phase4_count += !phase2_winner ? 1 : 0;
         if (!phase2_winner)
            batch.push_back(batchtype().Write(t.board, t.id, E_MINER_CONFIG, (e << 16) | (0 << 3) | 4));  // switch to 4 phase
         }
      }
   for (i = 0; i < boards.size(); i++) {
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      }
   ReadWriteConfig(batch);
   printf("%.1f%% engines are phase2\n", 100.0 * phase2_count / (phase2_count + phase4_count));
   float efficiency = 0.0, avg_temp, max_temp;
   bool too_hot;
   MeasureHashRate(2000, too_hot, avg_temp, max_temp, experiment);
   PrintSummary(NULL, "Post tweak", efficiency);
   }
#endif


void dvfstype::PerEngineTweak(int experiment)
   {
   vector<batchtype> batch, batch_start, batch_end;
   int i, b, e;
   const int best1 = 1, best2 = 1;
   const int predivider = 5;
   const float pll_multiplier = (best1 + 1) * (best2 + 1) * predivider / systeminfo.refclk * (1 << 20);
   const int bist_iterations = 1000;
   const int num_engines = 238;
   const int bist_delay_ms = 10;
   const int max_chips = 500;
   float phase2_hit[num_engines][max_chips], phase4_hit[num_engines][max_chips];

   if (topology.size() >= max_chips) FATAL_ERROR;
   printf("Running PerEngineTweak\n");

   batch.clear();
   for (b = 0; b < boards.size(); b++) {
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 0, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 1, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 2, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 3, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 4, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 5, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 6, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 7, 0xffffffff));
      batch_start.push_back(batchtype().Write(boards[b], -1, E_BIST, 1));
      }
   for (i = 0; i < boards.size(); i++) {
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      }
   ReadWriteConfig(batch);
   batch.clear();
   batch_end.clear();
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      batch_end.push_back(batchtype().Read(t.board, t.id, E_BIST_GOOD_SAMPLES));
      }

   for (e = 0; e < num_engines; e++)
      {
      batch_start.clear();
      for (b = 0; b < boards.size(); b++) {
         batch_start.push_back(batchtype().Write(boards[b], -1, E_DUTY_CYCLE, 0x80a0));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_HASHCONFIG, 0));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + MAXIMUM(e/32-1,0), 0xffffffff));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + (e / 32), 0xffffffff ^ (1 << (e & 31))));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_BIST_GOOD_SAMPLES, 0));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_BIST, bist_iterations));
         }
      for (i = 0; i < boards.size(); i++) {
         batch_start.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
         }
      ReadWriteConfig(batch_start);
      Delay(bist_delay_ms);
      ReadWriteConfig(batch_end);

      for (i = 0; i < topology.size(); i++) {
         topologytype& t = topology[i];
         phase2_hit[e][i] = (float)batch_end[i].data / (bist_iterations*8);
         }

      batch_start.clear();
      for (b = 0; b < boards.size(); b++) {
         batch_start.push_back(batchtype().Write(boards[b], -1, E_DUTY_CYCLE, 0));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_HASHCONFIG, 4));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_BIST_GOOD_SAMPLES, 0));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_BIST, bist_iterations));
         }
      for (i = 0; i < boards.size(); i++) {
         batch_start.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
         }
      ReadWriteConfig(batch_start);
      Delay(bist_delay_ms);
      ReadWriteConfig(batch_end);

      for (i = 0; i < topology.size(); i++) {
         topologytype& t = topology[i];
         phase4_hit[e][i] = (float)batch_end[i].data / (bist_iterations * 8);
         }
      }

   batch.clear();
   for (b = 0; b < boards.size(); b++) {
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 0, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 1, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 2, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 3, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 4, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 5, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 6, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 7, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_DUTY_CYCLE, 0x80a0));
      batch.push_back(batchtype().Write(boards[b], -1, E_HASHCONFIG, 0));   // reset the miner just in case the fast clock wedged us
      batch.push_back(batchtype().Write(boards[b], -1, E_DUTY_CYCLE, 0));  // write this once, it will only effect the miner_config writes below
      }
   for (i = 0; i < boards.size(); i++)
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
   ReadWriteConfig(batch);

   batch.clear();
   int phase2_count = 0, phase4_count = 0;
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      t.phase2 = 0.0;
      for (e = 0; e < num_engines; e++)
         {
         bool phase2_winner = phase2_hit[e][i] > phase4_hit[e][i];
         phase2_count += phase2_winner ? 1 : 0;
         phase4_count += !phase2_winner ? 1 : 0;
         if (!phase2_winner)
            batch.push_back(batchtype().Write(t.board, t.id, E_MINER_CONFIG, (e << 16) | 4));  // switch to 4 phase
         else t.phase2 += 1.0 / 238.0;
         }
      }
   for (i = 0; i < boards.size(); i++) {
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      }
   ReadWriteConfig(batch);
   printf("%.1f%% engines are phase2\n", 100.0 * phase2_count / (phase2_count + phase4_count));
   float efficiency = 0.0, avg_temp, max_temp;
   bool too_hot;
   MeasureHashRate(2000, too_hot, avg_temp, max_temp, experiment);
   PrintSummary(NULL, "Post tweak", efficiency);
   }


void dvfstype::Partition(int experiment)
   {
   vector<batchtype> batch, batch_start, batch_end;
   int i, b, e;
   const int best1 = 1, best2 = 1;
   const int predivider = 5;
   const float pll_multiplier = (best1 + 1) * (best2 + 1) * predivider / systeminfo.refclk * (1 << 20);
   const int bist_iterations = 100;
   const int num_engines = 238;
   const int pll_delay = 2000;
   const int bist_delay_ms = 1000.0 * pll_delay * ((2500 - 100) / 5) / 25.0e6;
   const int max_chips = 500;
   const float bist_threshold = 0.97;
   float phase2_speed[num_engines][max_chips];

   if (topology.size() >= max_chips) FATAL_ERROR;
   printf("Running Partition\n");
   FanControl(90);

   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];

      if (t.frequency<systeminfo.min_frequency || t.frequency > systeminfo.max_frequency) FATAL_ERROR;
      t.frequency2 = t.frequency;

      int fb_divider = t.frequency * (best1 + 1) * (best2 + 1) * predivider / systeminfo.refclk + 0.5;
      unsigned pll2_setting = 0x11 + (predivider << 12) + (best2 << 9) + (best1 << 6) + (fb_divider << 20);
      batch.push_back(batchtype().Write(t.board, t.id, E_PLL2, 0x12));  // reset the pll
      batch.push_back(batchtype().Write(t.board, t.id, E_PLL2, pll2_setting));
      }
   for (i = 0; i < boards.size(); i++)
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels

   ReadWriteConfig(batch);

   batch.clear();
   for (b = 0; b < boards.size(); b++) {
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 0, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 1, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 2, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 3, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 4, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 5, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 6, 0xffffffff));
      batch.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + 7, 0xffffffff));

      batch.push_back(batchtype().Write(boards[b], -1, E_DUTY_CYCLE, 0));
      batch.push_back(batchtype().Write(boards[b], -1, E_SPEED_INCREMENT, pll_multiplier * 5));
      batch.push_back(batchtype().Write(boards[b], -1, E_SPEED_DELAY, pll_delay));
      batch.push_back(batchtype().Write(boards[b], -1, E_SPEED_UPPER_BOUND, pll_multiplier * 2000));
      batch.push_back(batchtype().Write(boards[b], -1, E_BIST_THRESHOLD, (bist_iterations - 1) * 8 * bist_threshold));
      batch.push_back(batchtype().Write(boards[b], -1, E_HASHCONFIG, 0 + (1 << 3)));   // 2 phase with everyone using pll2
      }
   for (i = 0; i < boards.size(); i++) {
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      }
   ReadWriteConfig(batch);
   batch.clear();

   // now everything is pointing to pll2 running at the same frequency as before
   batch_end.clear();
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      batch_end.push_back(batchtype().Read(t.board, t.id, E_PLLFREQ));
      }

   for (e = 0; e < num_engines; e++)
      {
      batch_start.clear();
      for (b = 0; b < boards.size(); b++) {
         batch_start.push_back(batchtype().Write(boards[b], -1, E_ENGINEMASK + (e / 32), 0xffffffff ^ (1 << (e & 31))));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_DUTY_CYCLE, 0x80a0));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_MINER_CONFIG, (e << 16) | (0 << 3) | 0));  // phase 2 with main pll
         batch_start.push_back(batchtype().Write(boards[b], -1, E_BIST_GOOD_SAMPLES, 0));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_PLLFREQ, 100.0 * pll_multiplier));
         batch_start.push_back(batchtype().Write(boards[b], -1, E_BIST, bist_iterations + (1 << 31)));
         }
      for (i = 0; i < boards.size(); i++) {
         batch_start.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
         }
      ReadWriteConfig(batch_start);
      Delay(bist_delay_ms);
      ReadWriteConfig(batch_end);

      for (i = 0; i < topology.size(); i++) {
         topologytype& t = topology[i];
         phase2_speed[e][i] = batch_end[i].data / pll_multiplier;
         }
      }
   batch.clear();
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, t.frequency * pll_multiplier));
      }
   for (i = 0; i < boards.size(); i++) {
      batch.push_back(batchtype().Write(boards[i], -1, E_DUTY_CYCLE, 0x80a0));
      batch.push_back(batchtype().Write(boards[i], -1, E_HASHCONFIG, (1 << 16) + 0 + (0 << 3)));   // reset the miner just in case the fast clock wedged us
      batch.push_back(batchtype().Write(boards[i], -1, E_HASHCONFIG, 0 + (0 << 3)));   // 2 phase with everyone using main pll
      batch.push_back(batchtype().Write(boards[i], -1, E_HASHCONFIG, 0 + (0 << 3)));   // 2 phase with everyone using main pll
      batch_start.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      }
   ReadWriteConfig(batch); // reset and get everyone running phase2

   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      vector<speedsorttype> ss;
      memset(t.partition, 0, sizeof(t.partition));

      for (e = 0; e < num_engines; e++)
         ss.push_back(speedsorttype(e, phase2_speed[e][i]));
      std::sort(ss.begin(), ss.end());
      float low_average = 0.0, high_average = 0.0;
      for (int k = 0; k < num_engines / 2; k++)
         {
         t.partition[ss[k].engine / 32] |= 1 << (ss[k].engine & 31);
         low_average += ss[k].speed;
         batch.push_back(batchtype().Write(t.board, t.id, E_MINER_CONFIG, (ss[k].engine << 16) | (1 << 3)));
         }
      for (int k = num_engines / 2-1; k<num_engines; k++)
         {
         high_average += ss[k].speed;
         }
//      printf("asic %d, low_average=%.0f high_average=%.0f\n", i, low_average / 119, high_average / 119);
      }
   for (i = 0; i < boards.size(); i++)
      batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
   ReadWriteConfig(batch);

   float efficiency = 0.0, avg_temp, max_temp;
   bool too_hot;
   MeasureHashRate2(2000, too_hot, avg_temp, max_temp, experiment);
   PrintSummary(NULL, "Post partition", efficiency);
   }


void dvfstype::Optimize(float average_f, bool &too_hot, int experiment)
   {
   vector<batchtype> batch;
   int its, same=0;
   float hitrate, avg_temp, max_temp;
   int i,stagger;
   int period = 500;
   const int max_its = 25;

   printf("Running optimize()\n");
   for (its = 0; its < max_its; its++)
      {  
      bool done = false;
      vector<ranktype> ranking;
      float total_f = 0.0, phase2=0.0;
      Delay(100);
      if (experiment<3)
         hitrate = MeasureHashRateHybrid(period, too_hot, avg_temp, max_temp, experiment);
      else
         hitrate = MeasureHashRate(period, too_hot, avg_temp, max_temp, experiment);
      FanManage(avg_temp, max_temp);
      if (too_hot) return;

      if (its ==7) period *= 2;   //  slow down to let the thermals settle out
      if (its == 0) {
         // I want to look and see if we have any outlier asics. Everything should be a reasonably constant temp at this point
         ranking.clear();
         for (i = 0; i < topology.size(); i++)
            {
            topologytype& t = topology[i];
            ranking.push_back(ranktype(i, t.temperature, 0.0));
            }
         std::sort(ranking.begin(), ranking.end());
         const int max_problems = 5;
         float worst0 = ranking[ranking.size() - 1].passing;
         float worst5 = ranking[ranking.size() - max_problems - 1].passing;
         for (i = 0; i < max_problems && problem_asics.size()<max_problems; i++)
            {
            int index = ranking[ranking.size() - 1 - i].index;
            topologytype& t = topology[index];

            if (t.temperature > avg_temp + 15.0 && t.temperature > worst5 + 5)  // only delete a chip if it can reduce max temp by 5 degrees
               {
               int k;
               for (k = problem_asics.size() - 1; k >= 0; k--)
                  if (problem_asics[k] == index) break;
//               if (k<0)
//                  problem_asics.push_back(index);  // don't duplicate
               }
            }
         for (i = 0; i < problem_asics.size(); i++)
            {
            topologytype& t = topology[problem_asics[i]];
            for (int k = i + 1; k < problem_asics.size(); k++)
               {
               topologytype& tk = topology[problem_asics[k]];
               if (t.board == tk.board && t.row == tk.row)
                  {
                  problem_asics.clear();  // we have 2 bad guys on the same row so give up on the whole thing
                  break;
                  }
               }
            }
         if (problem_asics.size())
            {
            printf("I found %d HOT asics. I estimate I can reduce max temp by %.1fC", problem_asics.size(), worst0 - worst5);
            for (i = 0; i < problem_asics.size(); i++)
               {
               const topologytype& t = topology[problem_asics[i]];
               printf("(B%dR%dC%d id%d)", t.board, t.row, t.col, t.id);
               }
            printf("\n");
            }
         float slosh = 0.0;
         for (i = 0; i < problem_asics.size(); i++)
            {
            topologytype& t = topology[problem_asics[i]];
            if (t.frequency > average_f * 0.5) {
               slosh += t.frequency - average_f * 0.5;
               t.frequency = average_f * 0.5;
               }
            }
         // now spread the frequency I stole across all chips
         for (i = 0; i < topology.size(); i++)
            {
            topologytype& t = topology[i];
            t.frequency += slosh / topology.size();
            }
         }

      ranking.clear();
      for (i = 0; i < topology.size(); i++)
         {
         topologytype& t = topology[i];
         int k;
         for (k = problem_asics.size() - 1; k >= 0; k--)
            if (problem_asics[k] == i) break;
         if (k < 0)  // exclude any problem asics from frequency swapping
            ranking.push_back(ranktype(i, t.hitrate, t.frequency));
         phase2 += t.phase2;
         total_f += t.frequency;
         }
      std::sort(ranking.begin(), ranking.end());

      for (i = 0; i < topology.size() / 4; i++)
         {
         topologytype& slow = topology[ranking[i].index];
         topologytype& fast = topology[ranking[ranking.size() - i - 1].index];
         if (i==0)
            printf("Iteration %2d/%d: total=%5.1f%% fast=%5.1f%% slow=%5.1f%% phase2=%5.1f%% AvgTemp=%5.1fC MaxTemp=%5.1fC\n", its, max_its,hitrate*100.0,fast.hitrate*100.0, slow.hitrate * 100.0, 100.0*phase2/total_f,avg_temp, max_temp);

         if (slow.frequency >= average_f * 0.4 && fast.temperature < avg_temp+15.0)
            {
            float step = fast.hitrate - slow.hitrate > 0.4 ? 0.05 : 
                         fast.hitrate - slow.hitrate > 0.2 ? 0.03 : 
                         fast.hitrate - slow.hitrate > 0.1 ? 0.02 : 0.01;
            float f_incr = average_f * step / (1+its/5);
            f_incr = (int)((f_incr + 4.5) / 5.0) * 5.0;

            if (fast.frequency + f_incr < systeminfo.max_frequency && slow.frequency - f_incr > systeminfo.min_frequency) {
               fast.frequency += f_incr;
               slow.frequency -= f_incr;
               }
//            printf("%2d/%2d %6.1f -> %6.1f %.1f%%", fast.row, fast.col, fast.frequency - f_incr, fast.frequency,fast.hitrate*100.0);
//            printf("  %2d/%2d %6.1f -> %6.1f %.1f%%\n", slow.row, slow.col, slow.frequency + f_incr, slow.frequency,slow.hitrate * 100.0);
            }
         }

      for (stagger = 0; stagger < num_cols; stagger++) {
         for (i = stagger; i < topology.size(); i += num_cols) {
            topologytype& t = topology[i];
            batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, t.frequency * pll_multiplier));
            }
         }
      for (i = 0; i < boards.size(); i++)
         batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels

      ReadWriteConfig(batch);
      batch.clear();
      last_optimization_temp = avg_temp;
      if (done) break;
      }
   }

void dvfstype::Optimize2(float average_f, bool& too_hot, int experiment)
   {
   vector<batchtype> batch;
   int its, same = 0;
   float hitrate, avg_temp, max_temp;
   int i, stagger;
   int period = 500;
   const int max_its = 25;

   printf("Running optimize2()\n");
   for (its = 0; its < max_its; its++)
      {
      bool done = false;
      vector<ranktype> ranking;
      float total_f = 0.0, phase2 = 0.0;
      Delay(100);
      if (experiment < 3)
         hitrate = MeasureHashRateHybrid(period, too_hot, avg_temp, max_temp, experiment);
      else
         hitrate = MeasureHashRate2(period, too_hot, avg_temp, max_temp, experiment);
      FanManage(avg_temp, max_temp);
      if (too_hot) return;

      if (its == 7) period *= 2;   //  slow down to let the thermals settle out
      if (its == 0) {
         // I want to look and see if we have any outlier asics. Everything should be a reasonably constant temp at this point
         ranking.clear();
         for (i = 0; i < topology.size(); i++)
            {
            topologytype& t = topology[i];
            ranking.push_back(ranktype(i, t.temperature, 0.0));
            }
         std::sort(ranking.begin(), ranking.end());
         const int max_problems = 5;
         float worst0 = ranking[ranking.size() - 1].passing;
         float worst5 = ranking[ranking.size() - max_problems - 1].passing;
         for (i = 0; i < max_problems && problem_asics.size() < max_problems; i++)
            {
            int index = ranking[ranking.size() - 1 - i].index;
            topologytype& t = topology[index];

            if (t.temperature > avg_temp + 15.0 && t.temperature > worst5 + 5)  // only delete a chip if it can reduce max temp by 5 degrees
               {
               int k;
               for (k = problem_asics.size() - 1; k >= 0; k--)
                  if (problem_asics[k] == index) break;
               }
            }
         for (i = 0; i < problem_asics.size(); i++)
            {
            topologytype& t = topology[problem_asics[i]];
            for (int k = i + 1; k < problem_asics.size(); k++)
               {
               topologytype& tk = topology[problem_asics[k]];
               if (t.board == tk.board && t.row == tk.row)
                  {
                  problem_asics.clear();  // we have 2 bad guys on the same row so give up on the whole thing
                  break;
                  }
               }
            }
         if (problem_asics.size())
            {
            printf("I found %d HOT asics. I estimate I can reduce max temp by %.1fC", problem_asics.size(), worst0 - worst5);
            for (i = 0; i < problem_asics.size(); i++)
               {
               const topologytype& t = topology[problem_asics[i]];
               printf("(B%dR%dC%d id%d)", t.board, t.row, t.col, t.id);
               }
            printf("\n");
            }
         float slosh = 0.0;
         for (i = 0; i < problem_asics.size(); i++)
            {
            topologytype& t = topology[problem_asics[i]];
            if (t.frequency > average_f * 0.5) {
               slosh += t.frequency - average_f * 0.5;
               t.frequency = average_f * 0.5;
               }
            }
         // now spread the frequency I stole across all chips
         for (i = 0; i < topology.size(); i++)
            {
            topologytype& t = topology[i];
            t.frequency += slosh / topology.size();
            }
         }

      ranking.clear();
      for (i = 0; i < topology.size(); i++)
         {
         topologytype& t = topology[i];
         int k;
         for (k = problem_asics.size() - 1; k >= 0; k--)
            if (problem_asics[k] == i) break;
         if (k < 0)  // exclude any problem asics from frequency swapping
            ranking.push_back(ranktype(i, t.hitrate, t.frequency));
         if (k < 0)  // exclude any problem asics from frequency swapping
            ranking.push_back(ranktype(i+topology.size(), t.hitrate2, t.frequency2));
         phase2 += t.phase2;
         total_f += t.frequency;
         }
      std::sort(ranking.begin(), ranking.end());

      for (i = 0; i < ranking.size() / 4; i++)
         {
         int slow_index = ranking[i].index;
         int fast_index = ranking[ranking.size() - i - 1].index;
         topologytype& slow = topology[slow_index % topology.size()];
         topologytype& fast = topology[fast_index % topology.size()];
         float slow_f = slow_index >= topology.size() ? slow.frequency2 : slow.frequency;
         float fast_f = fast_index >= topology.size() ? fast.frequency2 : fast.frequency;
         if (i == 0)
            printf("Iteration %2d/%d: total=%5.1f%% fast=%5.1f%% slow=%5.1f%% phase2=%5.1f%% AvgTemp=%5.1fC MaxTemp=%5.1fC\n", its, max_its, hitrate * 100.0, fast.hitrate * 100.0, slow.hitrate * 100.0, 100.0 * phase2 / total_f, avg_temp, max_temp);
         
         if (slow_f >= average_f * 0.4 && fast.temperature < avg_temp + 15.0)
            {
            float step = fast.hitrate - slow.hitrate > 0.4 ? 0.05 :
               fast.hitrate - slow.hitrate > 0.2 ? 0.03 :
               fast.hitrate - slow.hitrate > 0.1 ? 0.02 : 0.01;
            float f_incr = average_f * step / (1 + its / 5);
            f_incr = (int)((f_incr + 4.5) / 5.0) * 5.0;  // round up to nearest 5Mhz

            if (fast_f + f_incr < systeminfo.max_frequency && slow_f - f_incr > systeminfo.min_frequency) {
               if (fast_index >= topology.size())
                  fast.frequency2 += f_incr;
               else
                  fast.frequency += f_incr;
               if (slow_index >= topology.size())
                  slow.frequency2 -= f_incr;
               else
                  slow.frequency -= f_incr;
               }
            }
         }

      for (stagger = 0; stagger < num_cols; stagger++) {
         for (i = stagger; i < topology.size(); i += num_cols) {
            topologytype& t = topology[i];
            const int best1 = 1, best2 = 1;
            const int predivider = 5;
            int fb_divider = t.frequency2 * (best1 + 1) * (best2 + 1) * predivider / systeminfo.refclk + 0.5;
            unsigned pll2_setting = 0x11 + (predivider << 12) + (best2 << 9) + (best1 << 6) + (fb_divider << 20);

            batch.push_back(batchtype().Write(t.board, t.id, E_PLL2, pll2_setting));
            batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, t.frequency* pll_multiplier));
            }
         }
      for (i = 0; i < boards.size(); i++)
         batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels

      ReadWriteConfig(batch);
      batch.clear();
      last_optimization_temp = avg_temp;
      if (done) break;
      }
   }



bool dvfstype::DVFS(const float THS, int experiment, bool verbose)     // returns true if performance could not be achieved or some error happened
   {
   const float low_rate = 0.95;
   const float high_rate = 0.97;
   const float start_rate = low_rate - 0.03;
   const float optimize_trigger_rate = 0.6;
   float average_f, current_f, hitrate = 0.0;
   float best_efficiency = 1.0e9, best_voltage = voltage, initial_voltage = voltage;
   bool too_hot = false, last_too_hot = false, not_optimized_yet, upvoltage = false;
   int i, period, too_low_count = 0;
   float currentTHS = THS;
   float avg_temp, max_temp;
   bool eco = true;

   vector<batchtype> batch;

   period = 1500;
   MeasureHashRate(1, too_hot, avg_temp, max_temp, experiment);   // just get the avg_temp
   average_f = THS * 1.0e6 / (238.0 * 4 / 2) / topology.size() / low_rate;
   average_f = MAXIMUM(average_f, systeminfo.min_frequency);
   average_f = MINIMUM(average_f, systeminfo.max_frequency);
   average_f = (int)((average_f + 2.5)/5.0)*5.0; // round to nearest 5Mhz
   current_f = 0.0;
   //   period = 1000 * (1000.0 / average_f);
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      if (t.frequency < systeminfo.min_frequency*0.5) FATAL_ERROR;
      if (t.frequency > systeminfo.max_frequency) FATAL_ERROR;
      current_f += t.frequency;
      }
   current_f /= topology.size();
   not_optimized_yet = fabs(average_f - current_f) > 10.0;  // if we're already at the target frequency, I can assume optimization has already happened
   not_optimized_yet = not_optimized_yet || fabs(avg_temp - last_optimization_temp) > 5.0;
   not_optimized_yet = true; // erase me, this is here for eco vs non-eco comparison

   if (not_optimized_yet) {
      // I might be returning after performance got lowered due to temp/power. Don't bump back to the old performance without getting voltage down to a safe level
      voltage = MAXIMUM(systeminfo.min_voltage, voltage - systeminfo.voltage_step * 200);
      voltage = systeminfo.min_voltage;
      SetVoltage(voltage);

      // I want to iteratively raise everyone up/down to avoid a discontinuity which can cause a spurious reset
      while (true) {
         bool completed = true;
         for (int stagger = 0; stagger < num_cols; stagger++) {
            for (i = stagger; i < topology.size(); i += num_cols) {
               topologytype& t = topology[i];
               int k;
               float target = average_f;

               for (k = problem_asics.size() - 1; k >= 0; k--)
                  if (problem_asics[k] == i)
                     target = average_f * 0.5;

               if (t.frequency < target)
                  {
                  completed = false;
                  t.frequency = MINIMUM(target, t.frequency * 1.4);
                  }
               if (t.frequency > target)
                  {
                  completed = false;
                  t.frequency = MAXIMUM(target, t.frequency * 0.7);
                  }
               batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, t.frequency * pll_multiplier));
               t.frequency2 = t.frequency;
               }
            }
         for (i = 0; i < boards.size(); i++)
            batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
         ReadWriteConfig(batch);
         batch.clear();
         if (completed) break;
         }
      }
   if (!not_optimized_yet)
      {  // we're returning so lower the voltage slightly and we'll walk back up to find the best point
      voltage -= systeminfo.voltage_step * 20;
      SetVoltage(voltage);
      }
   for (i = 0; i < topology.size(); i++) {
      topologytype& t = topology[i];
      batch.push_back(batchtype().Write(t.board, t.id, E_ENGINEMASK + 0, 0));
      batch.push_back(batchtype().Write(t.board, t.id, E_ENGINEMASK + 1, 0));
      batch.push_back(batchtype().Write(t.board, t.id, E_ENGINEMASK + 2, 0));
      batch.push_back(batchtype().Write(t.board, t.id, E_ENGINEMASK + 3, 0));
      batch.push_back(batchtype().Write(t.board, t.id, E_ENGINEMASK + 4, 0));
      batch.push_back(batchtype().Write(t.board, t.id, E_ENGINEMASK + 5, 0));
      batch.push_back(batchtype().Write(t.board, t.id, E_ENGINEMASK + 6, 0));
      batch.push_back(batchtype().Write(t.board, t.id, E_ENGINEMASK + 7, 0));
      batch.push_back(batchtype().Write(t.board, t.id, E_DUTY_CYCLE, 0x80a0));
      batch.push_back(batchtype().Write(t.board, t.id, E_HASHCONFIG, 1 << 16));
      batch.push_back(batchtype().Write(t.board, t.id, E_HASHCONFIG, 0));
      batch.push_back(batchtype().Write(t.board, t.id, E_HASHCONFIG, 0));
      for (int k = 0; k < boards.size(); k++)
         batch.push_back(batchtype().Read(boards[k], 0, E_UNIQUE)); // this ensures we wait for the tranmission to complete before switching channels
      ReadWriteConfig(batch);
      batch.clear();
      }

   printf("Starting DVFS initial voltage is %.3fV, average frequency is %.1fMhz, desired performance is %.1fTH/s.\n", initial_voltage, average_f, THS);
   while (true) {

      if (not_optimized_yet) {
         for (int b = 0; b < boards.size(); b++) {
            batch.push_back(batchtype().Write(boards[b], -1, E_DUTY_CYCLE, 0x80a0));
            batch.push_back(batchtype().Write(boards[b], -1, E_HASHCONFIG, 1 << 16));
            batch.push_back(batchtype().Write(boards[b], -1, E_HASHCONFIG, 0));
            batch.push_back(batchtype().Write(boards[b], -1, E_HASHCONFIG, 0));

            batch.push_back(batchtype().Write(boards[b], -1, E_BIST, 1));
            batch.push_back(batchtype().Write(boards[b], -1, E_CLKCOUNT, 0)); // add some delay so bist finishes and a nonce exhaust completes
            batch.push_back(batchtype().Read(boards[b], 0, E_UNIQUE)); // add some delay so bist finishes and a nonce exhaust completes
            }
         ReadWriteConfig(batch);
         batch.clear();
         }


      bool almost_there = hitrate >= start_rate;
      if (experiment==4)
         hitrate = MeasureHashRate2(period * (almost_there ? 1.0 : 1.0), too_hot, avg_temp, max_temp, experiment);
      else if (not_optimized_yet)
         hitrate = MeasureHashRateHybrid(period * (almost_there ? 1.0 : 1.0), too_hot, avg_temp, max_temp, experiment);
      else
         hitrate = MeasureHashRate(period * (almost_there ? 1.0 : 1.0), too_hot, avg_temp, max_temp, experiment);

      float true_efficiency;
      float p = ReadPower();
      float efficiency = voltage * voltage / hitrate;   // the AA supply gives unreliable power so use V^2 as a good proxy for power
      if (p >= systeminfo.max_power) too_hot = true;

      if (verbose)
         PrintSummary(NULL, "", true_efficiency);
//      efficiency = true_efficiency;

      if (efficiency < best_efficiency) {
         best_efficiency = efficiency;
         best_voltage = voltage;
         too_low_count = 0;      // reset the too_low_count because we just improved efficiency
         }
      last_too_hot = too_hot;

      //      printf("hitrate = %.3f too_low=%d upvoltage=%d best_eff=%.2f eff=%.2f\n",hitrate,too_low_count,upvoltage,best_efficiency, efficiency);

      if (too_hot || voltage >= systeminfo.max_voltage)
         {  // I want to quickly get things under control, so lower the voltage and then reduce performance
         float scaledown = 0.90;
         currentTHS *= scaledown;
         printf("We hit an overtemp/overpower/max V condition. Reducing performance to %.1fTH/s\n", currentTHS);
         if (true) {// fixme
            voltage = best_voltage;
            SetVoltage(voltage);
            break; // we're done everything is good
            }
         voltage = MAXIMUM(systeminfo.min_voltage, voltage - systeminfo.voltage_step * 100);
         SetVoltage(voltage);

         // reduce the performance 5%(scale each chip proportionally)
         for (int stagger = 0; stagger < num_cols; stagger++) {
            for (i = stagger; i < topology.size(); i += num_cols) {
               topologytype& t = topology[i];
               t.frequency = MAXIMUM(systeminfo.min_frequency, t.frequency * scaledown);
               batch.push_back(batchtype().Write(t.board, t.id, E_PLLFREQ, t.frequency * pll_multiplier));
               }
            }
         for (i = 0; i < boards.size(); i++)
            batch.push_back(batchtype().Read(boards[i], 0, E_UNIQUE)); // this ensures we wait for the transmission to complete before switching channels
         ReadWriteConfig(batch);
         batch.clear();
         not_optimized_yet = true; // this will force a reoptimization. Might be helpful if a single chip is too hot
         upvoltage = false;
         Delay(5000);   // give a little time before looking to let an overtemp condition be better
         }
      else if (not_optimized_yet && (hitrate >= optimize_trigger_rate && upvoltage)) {
         if (experiment == 4) {
            Partition(experiment);
            Optimize2(average_f, too_hot, experiment);
            }
         else if (experiment >= 3) {
            PerEngineTweak(experiment);
            Optimize(average_f, too_hot, experiment);
            PerEngineTweak(experiment);
            }
         else
            Optimize(average_f, too_hot, experiment);
         not_optimized_yet = false;
         upvoltage = false;
         }
      else if (hitrate >= low_rate && too_low_count >= 5) { // I want to see 5 target met periods in a row so we don't quit early due to bad luck
         voltage = best_voltage;
         SetVoltage(voltage);
         break; // we're done everything is good
         }
      // if we're returning and the performance is above the trigger, lower the supply until we get under the trigger
      else if ((hitrate >= high_rate && !upvoltage) || too_hot || (not_optimized_yet && hitrate >= optimize_trigger_rate)) {
         if (voltage - systeminfo.voltage_step < systeminfo.min_voltage) {
            if (verbose)
               printf("Supply minimum has been reached\n");
            if (not_optimized_yet) {
               if (experiment == 4) {
                  Partition(experiment);
                  Optimize2(average_f, too_hot, experiment);
                  }
               else if (experiment >= 3) {
                  PerEngineTweak(experiment);
                  Optimize(average_f, too_hot, experiment);
                  PerEngineTweak(experiment);
                  }
               else
                  Optimize(average_f, too_hot, experiment);
               not_optimized_yet = false;
               }
            else
               return false; // we're as low as the PS can go
            }
         voltage -= systeminfo.voltage_step * 10;
         SetVoltage(voltage);
         Delay(100);
         too_low_count = 0;
         upvoltage = false;
         }
      else if (hitrate >= low_rate) {
         too_low_count++;
         voltage += systeminfo.voltage_step*3;  // fixme
         SetVoltage(voltage);
         upvoltage = true;
         }
      else if (hitrate < 0.3) {   // we're way off so make a big jump
         voltage = MINIMUM(voltage + systeminfo.voltage_step * 16, systeminfo.max_voltage);
         SetVoltage(voltage);
         upvoltage = true;
         }
      else if (hitrate < 0.70) {   // we're way off so make a medium jump
         voltage = MINIMUM(voltage + systeminfo.voltage_step * 4, systeminfo.max_voltage);
         SetVoltage(voltage);
         too_low_count = 0;
         upvoltage = true;
         }
      else if (hitrate < start_rate) {   // we're a little off so make a small jump
         voltage = MINIMUM(voltage + systeminfo.voltage_step * 5, systeminfo.max_voltage);
         SetVoltage(voltage);
         too_low_count = 0;
         upvoltage = true;
         }
      else {
         voltage = MINIMUM(voltage + systeminfo.voltage_step * 2, systeminfo.max_voltage);
         SetVoltage(voltage);
         upvoltage = true;
         }
      if (hitrate > start_rate && experiment >= 5)
         PerEngineTweak(experiment);
      }
      if (experiment == 4)
         hitrate = MeasureHashRate2(period * 5, too_hot, avg_temp, max_temp, experiment);
      else
         hitrate = MeasureHashRate(period * 5, too_hot, avg_temp, max_temp, experiment);
   float true_efficiency;
   PrintSummary(NULL, "Final", true_efficiency);
   return false;
   }



