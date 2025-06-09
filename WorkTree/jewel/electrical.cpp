#include "pch.h"
#include "sha256.h"
#include "helper.h"
//#include "miner.h"
#include "dvfs.h"
#include "electrical.h"

int seed = 0;

void chiptype::ReadCsv(const char* filename)
   {
   TAtype ta("chiptype::ReadCsv()");
   char raw_filename[256];

   strcpy(raw_filename, "shmoo/");
   strcat(raw_filename, filename);
   FILE* fptr = fopen(raw_filename,"rt");
   char* chptr, buffer[4096];
   curvetype curve;

   printf("Reading %s\n", raw_filename);
   if (fptr == NULL) {
      printf("I couldn't open %s for reading\n", raw_filename);
      FATAL_ERROR;
      }
   strcpy(label, filename);
   while ((chptr = strchr(label, '\\')) != NULL)
      {
      strcpy(label, chptr+1);
      };
   while ((chptr = strchr(label, '/')) != NULL)
      {
      strcpy(label, chptr+1);
      };
   chptr = strchr(label, '.');
   if (chptr != NULL) *chptr = 0;  // delete the .csv stuff

   while (NULL!=fgets(buffer, sizeof(buffer), fptr)) {
      if (strncmp(buffer, "Temp_ISL,",9) == 0) {
         chptr = buffer;
         curve.points.clear();
         while (NULL != (chptr = strchr(chptr, ',')))
            {
            chptr++;
            float f = atof(chptr);
            if (f <= 0.0) FATAL_ERROR;
            if (curve.points.size() > 0 && curve.points.back().frequency <= f) FATAL_ERROR;
            curve.points.push_back(curvepointtype());
            curve.points.back().frequency = f;
            }
         }
      if (strstr(buffer, "_hits,") !=NULL) {
         sscanf(buffer, "%f_%f", &curve.temp, &curve.voltage);
         chptr = buffer;
         int index = 0;
         while (NULL != (chptr = strchr(chptr, ',')))
            {
            chptr++;
            float x = atof(chptr);
            curve.points[index].hit = x;
            curve.points[index].current = -1;
            curve.points[index].odv = -1;
            index++;
            }
         curves.push_back(curve);
         }
      if (strstr(buffer, "_current,") != NULL) {
         float temp, voltage;
         int index;
         sscanf(buffer, "%f_%f", &temp, &voltage);
         chptr = buffer;
         for (index = curves.size() - 1; index >= 0; index--)
            if (curves[index].temp == temp && curves[index].voltage == voltage) break;
         if (index < 0) FATAL_ERROR;
         curvetype& c = curves[index];
         index = 0;
         while (NULL != (chptr = strchr(chptr, ',')))
            {
            chptr++;
            float x = atof(chptr);
            c.points[index].current = x;
            index++;
            }
         }
      }

   int i, k;
   for (k =curves.size()-1; k>=0; k--)
      {
      // this makes the current not decreasing at high frequency to improve numerical stabilty of the solve routine
      // the optimum point will never be around here, so this won't effect the eventual accuracy
      curves[k].Fit();
      for (i = curves[k].points.size() - 2; i >= 0; i--)
         {
         curves[k].points[i].current = MAXIMUM(curves[k].points[i].current, curves[k].points[i + 1].current);
         }
      // Next I need to improve the linearity of conductance vs voltage to help numerical stability
      if (k > 0 && curves[k].temp == curves[k - 1].temp)
         {
         for (i = curves[k].points.size()-1; i >= 0; i--)
            {
            curves[k - 1].points[i].current = MAXIMUM(curves[k - 1].points[i].current, curves[k].points[i].current*0.9);
            }
         }
      }
   }

float chiptype::Minimum(float f, float cost, float& v, float& t)  // will return minimum efficiency
   {
   float e, newe, hit, current;
   int count = 0;

   // starting point
   v = 0.260;
   t = 60.0;

   while (true) {
      bool done = true;
      Interpolate(hit, current, v, t, f); e = hit / (current * current * ir_resistance + v * current + cost);

      Interpolate(hit, current, v, t + 0.1, f); newe = hit / (current * current * ir_resistance + v * current + cost);
      if (newe > e && t < 110.0) {
         t += 0.1; done = false;
         e = newe;
         }
      Interpolate(hit, current, v, t - 0.1, f); newe = hit / (current * current * ir_resistance + v * current + cost);
      if (newe > e && t > 30.1) {
         t -= 0.1; done = false;
         e = newe;
         }
      Interpolate(hit, current, v + 0.0001, t, f); newe = hit / (current * current * ir_resistance + v * current + cost);
      if (newe > e) {
         v += 0.0001; done = false;
         e = newe;
         }
      Interpolate(hit, current, v - 0.0001, t, f); newe = hit / (current * current * ir_resistance + v * current + cost);
      if (newe > e) {
         v -= 0.0001; done = false;
         e = newe;
         }
      count++;
      if (done || count>1000) break;
      }
   Interpolate(hit, current, v, t, f); newe = hit / (current * current * ir_resistance + v * current + cost);
   return 3151.3 / e / f;
   }

float chiptype::MinimumForceT(float f, float cost, float& v, float force_t)  // will return minimum efficiency
   {
   float e, newe, hit, current;
   int count = 0;

   // starting point
   v = 0.3;

   while (true) {
      bool done = true;
      Interpolate(hit, current, v, force_t, f); e = hit / (current * current * ir_resistance + v * current + cost);

      Interpolate(hit, current, v + 0.0001, force_t, f); newe = hit / (current * current * ir_resistance + v * current + cost);
      if (newe > e) {
         v += 0.0001; done = false;
         e = newe;
         }
      Interpolate(hit, current, v - 0.0001, force_t, f); newe = hit / (current * current * ir_resistance + v * current + cost);
      if (newe > e) {
         v -= 0.0001; done = false;
         e = newe;
         }
      count++;
      if (done || count > 1000) break;
      }
   Interpolate(hit, current, v, force_t, f); newe = hit / (current * current * ir_resistance + v * current + cost);
   return 2952.756 / e / f;
   }

// classic system is 134.112 TH/Ghz
void chiptype::Performance(float &pbest, float &p100, float& p140, float& p180)
   {
   float f, e, v, t;
   float min_e = 1e9, min_f;

   e = Minimum(1600, fan_power / 396, v, t);


   for (f = 400; f <= 2500; f += 25) {
      e = Minimum(f, fan_power / 396.0, v, t);
      if (e < min_e) {
         min_e = e;
         min_f = f;
         }
      }
   Minimum(min_f, fan_power / 396, v, t);
   pbest = min_e;
   printf("Best efficiency %4.1fJ/TH %3.0fTH %.0fC %.1fmV\n", min_e, min_f * .125664 * 0.98, t, v * 1000.0);
   p100 = Minimum(1000, fan_power / 396, v, t);
   printf("Best efficiency %4.1fJ/TH %3.0fTH %.0fC %.1fmV\n", p100, 1000 * .125664 * 0.98, t, v * 1000.0);
   p140 = Minimum(1500, fan_power / 396, v, t);
   printf("Best efficiency %4.1fJ/TH %3.0fTH %.0fC %.1fmV\n", p140, 1500 * .125664 * 0.98, t, v * 1000.0);
   p180 = Minimum(2000, fan_power / 396, v, t);
   printf("Best efficiency %4.1fJ/TH %3.0fTH %.0fC %.1fmV\n", p180, 2000 * .125664 * 0.98, t, v * 1000.0);
   }


void curvetype::Fit()
   {
   TAtype ta("chiptype::Fit()");
   float best = -1, best_error = 1.0e20;

   if (temp==90 && voltage==290)
      printf(""); // erase me

   stddev = 50.0;
   for (mean = 0; mean <= 1600; mean += 5)
      {
      float e = HitError();
      if (e <= best_error) {
         best_error = e;
         best = mean;
         }
//      printf("k1=%.0f e=%.3f\n", mean, e);
      }
   mean = best;
   best_error = 1.0e20;
   for (stddev = 1.0; stddev <= 2000.0; stddev *= 1.001)
      {
      float e = HitError();
      if (e < best_error) {
         best_error = e;
         best = stddev;
         }
      }
   stddev = best;
   best_error = 1.0e20;
   for (mean = 0; mean <= 1600; mean += 1)
      {
      float e = HitError();
      if (e <= best_error) {
         best_error = e;
         best = mean;
         }
      }
   mean = best;
   if (mean < 0.0 || stddev < 1.0) FATAL_ERROR;

   int i;
   for (i = 0; i < points.size() - 6 && points[i].hit == 0.0; i++)
      ;
   cdyn = (points[i].current - points.back().current) / (points[i].frequency - points.back().frequency)/(voltage*0.001);
   
   iddq = 0.0;
   bool done;
   while (true)
      {
      done = true;
      best_error = CurrentError();
      iddq += 0.010; if (CurrentError() < best_error) done = false; else iddq -= 0.01;
      if (done) break;
      }

//   printf("%.3fV %3.0fC mean=%6.1f stddev=%5.1f cdyn=%.1fmA/Mhz/V iddq=%.2fA\n",voltage,temp, mean,stddev,cdyn*1000.0,iddq);
   }


scenariotype::scenariotype(const systeminfotype& sinfo, int _boards, int _rows, int _cols, const vector<chiptype>& chips, int arrangement) : systeminfo(sinfo), boards(_boards), rows(_rows), cols(_cols) {
   int i, b, r, c;
   int seed = 0;
   
   reference_clock = 0.0;
   
   for (i = 0; i < rows * cols * boards; i++) {
      if (arrangement == 1)      // arrange by columns
         asics.push_back(electricaltype(chips[i % MINIMUM(chips.size(), cols)]));
      else if (arrangement == 2) // arrange by rows(striped)
         asics.push_back(electricaltype(chips[(i / cols) % chips.size()]));
      else if (arrangement == 3) // arrange by boards
         asics.push_back(electricaltype(chips[(i / (cols * rows)) % chips.size()]));
      else if (arrangement == 4) // random arrangement
         asics.push_back(electricaltype(chips[i % chips.size()]));
      else if (arrangement == 5) // arrange by rows( not striped)
         asics.push_back(electricaltype(chips[(i / (cols * (rows / chips.size()))) % chips.size()]));
      else if (arrangement == 6) // arrange by rows( not striped) opposite orientation with respect to airflow
         asics.push_back(electricaltype(chips[((chips.size()-i-1) / (cols * (rows / chips.size()))) % chips.size()]));
      }
   if (arrangement == 4)
      {
      for (i = 0; i < asics.size(); i++)
         {
         electricaltype& a = asics[i];
         a.random = random(seed)*1000;
         }
      std::sort(asics.begin(), asics.end());
      }
   for (b = 0; b < boards; b++) {
      outlet_temps.push_back(ambient);
      for (r = 0; r < rows; r++) {
         for (c = 0; c < cols; c++) {
            electricaltype& a = asics[b * rows * cols + r * cols + c];
            float process = gasdev(seed) * 1.0;
            //            if (variation) printf("process=%.2f\n",process);
            a.vt = process * 0.020 + ref_vt;  // 20mV stddev
            a.leakage = ref_leakage * exp2(-process * 0.4);
            a.voltage = 0.3; // provide a reasonable starting point for the solver
            if (true) {
               a.theta_j = 3.5;
               }
            else if (rows >= 40 && true) {
               if (r < 16)
                  a.theta_j = 3.7;
               else if (r < 28)
                  a.theta_j = 3.2;
               else if (r < 41)
                  a.theta_j = 2.7;
               else
                  a.theta_j = 2.7;
               }
            else {
               if (r < rows * .4)
                  a.theta_j = 3.0;
               else if (r < rows * .7)
                  a.theta_j = 2.5;
               else
                  a.theta_j = 2.0;
               }
            }
         }
      }
   }

void scenariotype::Reset()
   {
   int r, c;
   for (r = 0; r < rows; r++) {
      for (c = 0; c < cols; c++) {
         electricaltype& a = asics[r * cols + c];

         a.frequency = 200;
         a.case_temp = ambient;
         a.junction_temp = a.case_temp;
         }
      }
   }


void scenariotype::Print(const char* label, bool summary) {
   int r, c, b;

   for (b = 0; b < boards; b++)
      {
      for (r = 0; r < rows; r++)
         {
         int index = b * rows * cols + r * cols + 0;
         electricaltype& a = asics[index];
         printf("%d_%2d_%3d: %5.1fmV %4.0fMhz(%5.1f%%)%5.1fC", b,r,index,a.voltage*1000.0,a.frequency,a.hitrate*100.0,a.junction_temp);
         for (c = 1; c < cols; c++) {
            electricaltype& a1 = asics[b * rows * cols + r * cols + c];
            printf("   %4.0fMhz(%5.1f%%)%5.1fC", a1.frequency, a.hitrate * 100.0,a.junction_temp);
            }
         printf("\n");
         }
      }
   }

bool scenariotype::Solve() { // return true if thermal runaway
   int b, r, c, its;
   float board_current;
   
   runaway = false;
   current = 0.0;
   asics[51].Conductance();
   for (b = 0; b < boards; b++) {
      for (its = 0; its < 1000; its++) {
         float resistance = 0.0;
         bool done = true;

         // find the resistance of board using the current voltages
         for (r = 0; r < rows; r++) {
            float cond = 0.0;
            for (c = 0; c < cols; c++) {
               electricaltype& a = asics[b * rows * cols + r * cols + c];
               if (a.junction_temp >= 110.0) 
                  { runaway = true; return true; }
               float m = a.Conductance();
               cond += 1.0 / (1.0 / m + ir_resistance);
               }
            resistance += 1.0 / cond;
            }
         board_current = supply / resistance;
         // I now know resistance for the board so I can calculate the supply current
         outlet_temps[b] = board_current * supply * air_conduction + ambient;
         for (r = 0; r < rows; r++) {
            for (c = 0; c < cols; c++) {
               electricaltype& a = asics[b * rows * cols + r * cols + c];
               a.case_temp = (outlet_temps[b] - ambient) * ((r / 4)+1) / MAXIMUM(1, (rows / 4)) + ambient;
               }
            }

         for (r = 0; r < rows; r++) {
            float cond = 0.0;
            for (c = 0; c < cols; c++) {
               electricaltype& a = asics[b * rows * cols + r * cols + c];
               cond += a.Conductance();
               }
            float v = board_current / cond;
            if (v < 0) FATAL_ERROR;
            for (c = 0; c < cols; c++) {
               electricaltype& a = asics[b * rows * cols + r * cols + c];
               a.voltage = (a.voltage + v) / 2;  // average the voltage between this iteration and last to improve convergeance
               float newtemp = (a.junction_temp + a.case_temp + a.Power() * a.theta_j) / 2;
               if (newtemp < 40.0) newtemp = 40.0;
               done = done && fabs(a.voltage - v) < 0.0001 && fabs(newtemp - a.junction_temp) < 0.1;

               if (fabs(a.junction_temp - newtemp) < 1.0)
                  a.junction_temp = newtemp;
               else if (a.junction_temp > newtemp + 5) a.junction_temp -= 2;
               else if (a.junction_temp > newtemp) a.junction_temp -= 1;
               else if (a.junction_temp < newtemp - 5) a.junction_temp += 2;
               else if (a.junction_temp < newtemp) a.junction_temp += 1;

               if (a.junction_temp < 0 || a.junction_temp>=110)
                  {
                  Print("State before fatal_error.");
                  runaway = true;
                  return true;
                  }
               }
            }
         if (done) break;
         if (false) {
            float cond = 0.0;
            b = 0, r = 8, c = 0;
            electricaltype& a = asics[b * rows * cols + r * cols + c];
            printf("ITS=%d Row%d F=%.0f V=%.1f T=%.1fC I=%.1fA 1/R=%.2f\n", its, r, a.frequency, a.voltage * 1000.0, a.junction_temp, a.Conductance() * a.voltage, a.Conductance());
            }
         }
      current += board_current;
      }
   return false;
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


float scenariotype::Dvfs_Optimize(const float average_f, bool quiet)
   {
   int its, i;
   float hitrate = 0.0;

   for (its = 0; ; its++)
      {
      bool done = false;
      vector<ranktype> ranking;
      float avg_temp = 0.0, max_temp = -1000.0;
      double total_f = 0.0, total_hit = 0.0;

      if (Solve()) return -1.0;
      float power = current * supply;
      ranking.clear();
      for (i = 0; i < asics.size(); i++)
         {
         electricaltype& a = asics[i];
         ranking.push_back(ranktype(i, a.hitrate, a.frequency));
         max_temp = MAXIMUM(max_temp, a.junction_temp);
         avg_temp += a.junction_temp / asics.size();
         total_f += a.frequency;
         total_hit += a.hitrate * a.frequency;
         }
      hitrate = total_hit / total_f;
      if (its >= 40) break;

      std::sort(ranking.begin(), ranking.end());
      for (i = 0; i < asics.size() / 4; i++)
         {
         electricaltype& slow = asics[ranking[i].index];
         electricaltype& fast = asics[ranking[ranking.size() - i - 1].index];
         if (i == 0 && !quiet)
            printf("Iteration %d/40: total=%.1f%% fast=%.1f%% slow=%.1f%% AvgTemp=%.1fC MaxTemp=%.1fC\n", its, hitrate * 100.0, fast.hitrate * 100.0, slow.hitrate * 100.0, avg_temp, max_temp);

         if (slow.frequency >= average_f * 0.4)
            {
            float step = fast.hitrate - slow.hitrate > 0.4 ? 0.05 :
               fast.hitrate - slow.hitrate > 0.2 ? 0.03 :
               fast.hitrate - slow.hitrate > 0.1 ? 0.02 : 0.01;
            float f_incr = average_f * step / (1 + its / 5) *0.5;
            if (fast.frequency + f_incr < systeminfo.max_frequency && slow.frequency - f_incr > systeminfo.min_frequency) {
               fast.frequency += f_incr;
               slow.frequency -= f_incr;
               }
            }
         }
      }
   return hitrate;
   }

void scenariotype::Stats(float& power, float& hashrate, float& hitrate, float& max_temp, float& avg_temp, float& outlet_temp, float& ir, float& efficiency, bool quiet)
   {
   int i;
   double total_f = 0.0, total_hit = 0.0, average_v = 0.0;
   avg_temp = 0.0, max_temp = -1000.0;

   for (i = 0; i < asics.size(); i++)
      {
      electricaltype& a = asics[i];
      total_f += a.frequency;
      total_hit += a.hitrate * a.frequency;
      max_temp = MAXIMUM(max_temp, a.junction_temp);
      avg_temp += a.junction_temp / asics.size();
      average_v += a.voltage / asics.size();
      }
   outlet_temp = 0.0;
   for (i = 0; i < boards; i++)
      outlet_temp += outlet_temps[i] / boards;
   hitrate = total_hit / total_f;
   power = (current * supply + fan_power*asics.size()/396) / ps_efficiency;
   hashrate = total_f * hitrate * 254 * 4 / 3 * 1.0e-6;
   efficiency = power / hashrate;
   ir = supply - average_v * rows;
   if (runaway) {
      hashrate = efficiency = 0.0;
      }
   if (!quiet)
      printf("Voltage=%.3fV/%.1fmV %4.0fW hitrate=%5.1f%% Perf=%4.1fTH/s Eff=%.2fJ/TH AvgTemp=%.1fC MaxTemp=%.1fC IR=%.1fmV\n", supply, supply / rows * 1000.0, power, hitrate * 100.0, hashrate, efficiency, avg_temp, max_temp, ir * 1000.0);
   }

void scenariotype::Optimize(const float THS, float _ambient, bool uniform, bool quiet) {
   const float low_rate = 0.97;
   const float high_rate = low_rate + 0.02;
   const float start_rate = low_rate - 0.02;
   const float optimize_trigger_rate = 0.75;
   float average_f, hitrate = 0.0;
   float best_efficiency = 1.0e9, best_voltage = -1;
   bool not_optimized_yet=true, quit=false;
   int i;
   
   ambient = _ambient;
   average_f = THS * 1.0e6 / (254.0 * 4 / 3) / asics.size() / ((low_rate+high_rate)/2.0);
   average_f = MAXIMUM(average_f, systeminfo.min_frequency);
   average_f = MINIMUM(average_f, systeminfo.max_frequency);
   supply = systeminfo.min_voltage;

   for (i = 0; i < asics.size(); i++)
      {
      electricaltype& a = asics[i];
      a.frequency = average_f;
      a.voltage = supply / rows; // provide a reasonable starting point for the solver
      a.junction_temp = MAXIMUM(40.0,ambient);
      }
   if (!quiet)
      printf("Starting DVFS, average frequency is %.1fMhz, desired performance is %.1fTH/s.\n", average_f, THS);
   while (true) {
      if (Solve())
         {
         printf("Runaway occurred\n");
         Print("");
         Solve();
         return;
         }
      float power, hashrate, hitrate, max_temp, avg_temp, outlet_temp, ir, efficiency;
      Stats(power, hashrate, hitrate, max_temp, avg_temp, outlet_temp, ir, efficiency, quiet);

      if (quit)
         {
//         if (!quiet)
            printf("Ending DVFS, final voltage %.3fV/%.1fmV %.0fW, hit=%.1f%%, Efficiency %.1fTH/s AvgTemp=%.1fC MaxTemp=%.1fC OutletTemp=%.1fC IR=%.1fmV\n", supply, (supply-ir)/rows*1000.0,power,hitrate * 100.0, efficiency,avg_temp,max_temp,outlet_temp,ir*1000.0);
         break;
         }
      else if (efficiency < best_efficiency) {
         best_efficiency = efficiency;
         best_voltage = supply;
         }
      if (not_optimized_yet && (hitrate >= optimize_trigger_rate) && !uniform) {
         hitrate = Dvfs_Optimize(average_f, quiet);
         not_optimized_yet = false;
         if (hitrate >= start_rate) supply -= 0.25; //backup some on the voltage
         }
      else if (supply >= systeminfo.max_voltage || hitrate > high_rate) {
         supply = best_voltage;
         quit = true;
         }
      else if (hitrate < 0.3) supply += 0.050;
      else if (hitrate < 0.8) supply += 0.010;
      else supply += 0.005;
      }
   }


/*
void scenariotype::Program(const vector<batchtype>& batch) {
   int i;

   for (i = 0; i < batch.size(); i++) {
      const batchtype& b = batch[i];
      if (b.board != 0) FATAL_ERROR;
      if (b.id >= 256) FATAL_ERROR;

      if (b.action == CMD_READWRITE || b.action >= CMD_LOAD0) FATAL_ERROR;
      for (int index = b.id < 0 ? 0 : xref[b.id]; index <= (b.id < 0 ? asics.size() - 1 : xref[b.id]) && b.action == CMD_WRITE; index++)
         {
         command_cfgtype cmdpacket;
         
         cmdpacket.command = b.action | (b.id < 0 ? CMD_BROADCAST : 0);
         cmdpacket.address = b.addr;
         cmdpacket.id = b.id;
         cmdpacket.data = b.data;
         if (b.action==CMD_WRITE)
            Program(cmdpacket);
         }
      }
   }

void scenariotype::Program(const command_cfgtype &cmdpacket) {

   bool broadcast = cmdpacket.command & CMD_BROADCAST;
   if ((cmdpacket.command & 0xf) == CMD_READWRITE) FATAL_ERROR;
   if ((cmdpacket.command & 0xf) != CMD_WRITE) return;

   for (int index = broadcast ? 0 : xref[cmdpacket.id]; index <= (broadcast ? asics.size() - 1 : xref[cmdpacket.id]); index++)
      {
      electricaltype& a = asics[index];
      switch (cmdpacket.address) {
         case 255:   // fake register to communicate supply voltage for presilicon testing
            supply = cmdpacket.data * 0.001;
            break;
         case E_PLLCONFIG:
            a.pll_multiplier = reference_clock / (((cmdpacket.data >> 10) & 7) + 1) / (((cmdpacket.data >> 13) & 7) + 1);
            break;
         case E_PLLFREQ:
            a.frequency = a.pll_multiplier * (float)cmdpacket.data / (1 << 20);
            Solve();
            break;
         case E_SPEED_DELAY: break;
         case E_SPEED_INCREMENT: a.pll_increment = a.pll_multiplier * cmdpacket.data / (float)(1 << 20); break;
         case E_SPEED_UPPER_BOUND: a.pll_max = a.pll_multiplier * cmdpacket.data / (float)(1 << 20); break;
         case E_BIST_THRESHOLD: break;
         case E_IPCFG: break;
         case E_TEMPCFG: break;
         case E_THERMALTRIP: break;
         case E_MAX_TEMP_SEEN: break;
         case E_BIST:
            if (cmdpacket.data == 0 || cmdpacket.data == 2)
               a.bist_fail = false;
            if (cmdpacket.data == 0 || cmdpacket.data == 1) {
               a.bist_fail = a.bist_fail || a.MaxFrequency() < a.frequency;
               }
            if (cmdpacket.data == 2) {
               while (a.frequency < a.pll_max) {
                  a.bist_fail = a.bist_fail || a.MaxFrequency() < a.frequency;
                  if (a.bist_fail) break;
                  a.frequency += a.pll_increment;
                  Solve();
                  }
               }
            break;
         default: FATAL_ERROR;
         }
      }
   }


void scenariotype::Respond(vector<batchtype>& batch) {
   int i;

   for (i = 0; i < batch.size(); i++) {
      batchtype& b = batch[i];
      command_cfgtype cmdpacket;
      if (b.board != 0) FATAL_ERROR;
      if (b.action == CMD_READWRITE || b.action >= CMD_LOAD0) FATAL_ERROR;

      cmdpacket.command = b.action | (b.id < 0 ? CMD_BROADCAST : 0);
      cmdpacket.address = b.addr;
      cmdpacket.id = b.id;
      cmdpacket.data = b.data;
      if (b.action == CMD_READ && xref[b.id]>=0)
         Respond(cmdpacket, b.data);
      }
   }

bool scenariotype::Respond(const command_cfgtype& cmdpacket, uint32& data) {
   if (cmdpacket.command != CMD_READ) FATAL_ERROR;
   if (cmdpacket.id < 0 || cmdpacket.id >= 256) FATAL_ERROR;
   if (xref[cmdpacket.id] < 0) return true;
   electricaltype& a = asics[xref[cmdpacket.id]];
   switch (cmdpacket.address) {
      case E_UNIQUE:
         data = 0x61727541; break;
      case E_BUILD_DATE:
         data = 0x10232022; break;
      case E_PLLFREQ:
         data = a.frequency / a.pll_multiplier * (1 << 20); break;
      case E_MAX_TEMP_SEEN:
         data = 0.0; break;
      case E_SUMMARY:
         data = ((int)((a.junction_temp - (-287.48)) / 662.88 * 4096) << 16) + ((int)(a.frequency / a.pll_multiplier * 4) << 6) + 0xf; break;
      case E_TEMPERATURE:
         data = (a.junction_temp - (-287.48)) / 662.88 * 4096; break;
      case E_VOLTAGE:
         data = (a.voltage + .28589) * 9553.13; break;
      case 255: 
      {
      float p = 0.0;
      Solve();
      for (int i = 0; i < asics.size(); i++)
         p += asics[i].Power();
      p = p;
      p += fan_power;
      p = p / ps_efficiency;
      data = p * 1000.0;
      }break;
      default: FATAL_ERROR;
      }
   return false;
   }
*/

/*
class isotype : public MultiThreadtype {
   scenariotype& s;
   const char* filename;
   double ambient_low, ambient_high, ambient_step;
   double supply_low, supply_high, supply_step;
   double perf_low, perf_high;
   FILE* fptr;
   vector<vector<float> > table_hashrate;
   vector<vector<float> > table_power;
   vector<vector<float> > table_minj;
   vector<vector<float> > table_maxj;
   vector<vector<float> > table_outlet;
public:
   isotype(scenariotype& _s, const char* _filename) : s(_s), filename(_filename)
      {
      ambient_low = 20.0;
      ambient_high = 55.0;
      ambient_step = 5.0;
      supply_low = 11.5;
      supply_high = 15.0;
      supply_step = 0.01;
      perf_low = 50;
      perf_high = 200;
      fptr = NULL;
      }
   void Prolog() {
      for (float ambient = ambient_low; ambient <= ambient_high; ambient += ambient_step) {
         table_hashrate.push_back(vector<float>());
         table_power.push_back(vector<float>());
         table_minj.push_back(vector<float>());
         table_maxj.push_back(vector<float>());
         table_outlet.push_back(vector<float>());
         for (double supply = supply_low; supply <= supply_high; supply += supply_step) {
            int index = 0.5 + (supply - supply_low) / supply_step;
            if (index != table_hashrate.back().size()) FATAL_ERROR;
            table_hashrate.back().push_back(-1.0);
            table_power.back().push_back(-1.0);
            table_minj.back().push_back(-1.0);
            table_maxj.back().push_back(-1.0);
            table_outlet.back().push_back(-1.0);
            }
         }
      fptr = fopen(filename, "wt"); if (fptr == NULL) FATAL_ERROR;
      fprintf(fptr, "supply,ambient,C,TH/s,W,J/TH\n");
      }

   void Epilog() {
      const float perf_incr = num_boards>1 ? 10.0 : 5.0;
      double perf, ambient;
      int outer_index;
      fprintf(fptr, "\n\n%.0f miners %.2fGhz %.1fJ/TH @%.0fmV", hashmul / (4 / 3.0 / 1.0e6), ref_frequency * 0.001, ref_efficiency, ref_voltage * 1000.0);
      for (int plot = 0; plot < 5; plot++) {
         if (plot==0)
            fprintf(fptr, "\nEfficiency\n");
         else if (plot==1)
            fprintf(fptr, "\nAverage Voltage\n");
         else if (plot==2)
            fprintf(fptr, "\nSystem Power\n");
         else if (plot == 3)
            fprintf(fptr, "\nWorst Junction\n");
         else if (plot == 4)
            fprintf(fptr, "\nOutlet Air Temp\n");
         for (perf = perf_low; perf <= perf_high; perf += perf_incr)
            fprintf(fptr, ",%.0f", perf);
         for (ambient = ambient_low, outer_index = 0; ambient <= ambient_high; ambient += ambient_step, outer_index++) {
            fprintf(fptr, "\n%.0f", ambient);
            for (perf = perf_low; perf <= perf_high; perf += perf_incr) {
               float eff = 1000, supply = 0.0, tj=0.0, outlet=0.0;
               int i;
               for (i = 0; i < table_hashrate[outer_index].size(); i++) {
                  float perf_actual = table_hashrate[outer_index][i];
                  float error = fabs(perf - perf_actual);
                  if (error < perf_incr / 3.0) {
                     if (eff > table_power[outer_index][i] / perf_actual) {
                        eff = table_power[outer_index][i] / perf_actual;
                        supply = i * supply_step + supply_low;
                        tj = table_maxj[outer_index][i];
                        outlet = table_outlet[outer_index][i];
                        }
                     }
                  if (table_hashrate[outer_index][i] > perf) {
                     float z = ((table_power[outer_index][i] - fan_power) * (perf / perf_actual) + fan_power) / perf;
                     if (eff > z) {
                        eff = z;
                        supply = i * supply_step + supply_low;
                        tj = table_maxj[outer_index][i];
                        outlet = table_outlet[outer_index][i];
                        }
                     }
                  }
               if (eff > 50.0)
                  fprintf(fptr, ",");
               else if (plot==0)
                  fprintf(fptr, ",%.1f", eff);
               else if (plot == 1)
                  fprintf(fptr, ",%.0f", supply * 1000.0 / s.rows);
               else if (plot == 2)
                  fprintf(fptr, ",%.0f", perf * eff);
               else if (plot == 3)
                  fprintf(fptr, ",%.0f", tj);
               else
                  fprintf(fptr, ",%.0f", outlet);
               }
            }
         fprintf(fptr, "\n");
         }
      fclose(fptr); fptr = NULL;
      }
   void Func(const int threadnum) {
      double ambient, supply;
      int outer_index;
      scenariotype mys(s);
      for (ambient = ambient_low, outer_index = 0; ambient <= ambient_high; ambient += ambient_step, outer_index++) {
         for (supply = supply_low+ supply_step * threadnum; supply <= supply_high; supply += supply_step * totalNumberOfThreads) {
            int index = 0.5 + (supply - supply_low) / supply_step;
            float hashrate, possible, minj, maxj, deltav, power, outlet;
//            mys.Optimize(BEST_PERFORMANCE, supply, ambient);
            mys.Stats(hashrate, possible, minj, maxj, deltav, power, outlet);
            hashrate *= num_boards;
            power = (power * num_boards) / ps_efficiency + fan_power * num_boards;
            table_hashrate[outer_index][index] = hashrate;
            table_power[outer_index][index] = power;
            table_minj[outer_index][index] = minj;
            table_maxj[outer_index][index] = maxj;
            table_outlet[outer_index][index] = outlet;

            EnterCriticalSection(&CS_general);
//            printf("solving for %.2fV %.0fC\t\t", supply, ambient);
//            printf("%.2f,%.0f,%.1f,%.2f,%.2f,%.2f\n", supply, ambient, maxj, hashrate, power, power / hashrate);
            fprintf(fptr, "%.3f,%.0f,%.1f,%.2f,%.2f,%.2f\n", supply, ambient, maxj, hashrate, power, power / hashrate);
            if (hashrate < 0.1) {
//               mys.Print("foo");
//               exit(-1);
               }
            LeaveCriticalSection(&CS_general);
            }
         }
      }
   };
*/




class sweeptype : public MultiThreadtype {
   const scenariotype& s;
   vector< scenariotype> working_s;
   const char* filename, *label;
   float min_temp, max_temp, temp_incr;
   float min_hash, max_hash, hash_incr;
   plottype plot;
public:
   vector<float> p100, p140, p180;

   sweeptype(scenariotype& _s, const char *_label, const char* _filename) : s(_s), label(_label), filename(_filename)
      {
      min_temp = 25.0;
      max_temp = 40.0;
      temp_incr = 15;
      min_hash = 50;
      max_hash = 350;
      hash_incr = 10;
      }
   void Prolog() {
      int i;
      for (i = 0; i < totalNumberOfThreads; i++)
         working_s.push_back(s);
      for (float h = min_hash; h <= max_hash; h += hash_incr)
         {
         plot.AddData(label, h);
         }
      for (float t = min_temp; t <= max_temp; t += temp_incr)
         {
         p100.push_back(0.0);
         p140.push_back(0.0);
         p180.push_back(0.0);
         }

      }
   void Func(const int threadnum) {
      int work = 0;

      for (int uniform=0; uniform<=0; uniform++)
      for (float t = min_temp+ threadnum*temp_incr; t <= max_temp; t += temp_incr*totalNumberOfThreads) {
         char buffer[64];
         sprintf(buffer, "%s@%.0f", label, t);
         for (float h = min_hash; h <= max_hash; h += hash_incr)
            {
            working_s[threadnum].Optimize(h, t, !!uniform, true);
            float power, hashrate, hitrate, max_temp, avg_temp, outlet_temp, ir, efficiency;
            EnterCriticalSection(&CS_general);
            printf("target%.0f:", h);
            working_s[threadnum].Stats(power, hashrate, hitrate, max_temp, avg_temp, outlet_temp, ir, efficiency);
            if (efficiency < 1.0) efficiency = 1000.0;
            plot.AddData(buffer, efficiency);
            int tempindex = (t - min_temp) / temp_incr;
            if (h == 100) p100[tempindex] = efficiency;
            if (h == 140) p140[tempindex] = efficiency;
            if (h == 180) p180[tempindex] = efficiency;
            LeaveCriticalSection(&CS_general);
            }
         }
      }
   void Epilog() {
      plot.AppendToFile(filename);
      }
   };


 
void Electrical_Sim2()
   {
   vector<chiptype> chips, chips2;
   systeminfotype sinfo;
   sinfo.min_frequency = min_frequency;   // I need to keep min frequency to something I characterized
   sinfo.min_voltage = 11;

   chips.push_back(chiptype());

   float pbest, p100, p140, p180;
   chips[0].Performance(pbest, p100, p140, p180);

   scenariotype s1(sinfo, 3, 46, 3, chips, 1);
   sweeptype sweep1(s1, "treasure", "treasure.csv");
   sweep1.Launch(12);
   exit(0);


   }


void Electrical_Sim() {
   seed = 0;
//   scenariotype s1(44, 3, -1, 0.0, true);
/*   const char* models[] = {
      "bin1_tt_a.csv","bin1_tt_a1.csv","bin1_tt_b.csv","bin1_tt_hb1_1.csv","bin1_tt_hb1_2.csv","bin1_tt_hb1_3.csv","bin1_tt_hb1_4.csv","bin1_tt_hb1_5.csv",
      "bin1_ff_a.csv","bin1_ff_2.csv","ff_a.csv","bin1_ff_tr2_e1_21.6j_9.7l.csv","bin1_ff_tr2d5_21.7j_15.2l.csv","bin1_ff_tr3_b8_21.2j_9.7l.csv","bin1_ff_tr3_q8_25j_22.4l.csv","bin1_ff_tr3_r6_23j_16.csv",
      "bin2_tt_1.csv","bin2_tt_2.csv","bin2_tt_hb2_1.csv","bin2_tt_hb2_2.csv","bin2_tt_hb2_3.csv","bin2_tt_hb2_3_hires.csv",
      "bin3_tt_1.csv","bin3_tt_2.csv","ecopart_hires.csv","ecoff.csv","ecoff_2.csv","ecoff_3.csv",NULL};*/
   const char* models[] = {
//      "bin1_tt_a.csv","bin1_tt_a1.csv","bin1_tt_b.csv","bin1_tt_hb1_1.csv","bin1_tt_hb1_2.csv","bin1_tt_hb1_3.csv","bin1_tt_hb1_4.csv","bin1_tt_hb1_5.csv",
//      "bin2_tt_1.csv","bin2_tt_2.csv","bin2_tt_hb2_1.csv","bin2_tt_hb2_2.csv","bin2_tt_hb2_3.csv","bin2_tt_hb2_3_hires.csv",
//      "bin3_tt_1.csv","bin3_tt_2.csv","ecopart_hires.csv",
      "eco_022.csv","eco_bin2_11.csv","eco_bin2_22.csv","Tune_023_A0.csv","Tune_024_A0.csv","Tune_025_A0.csv",
      "ald32_b2.csv","ald29_bin3.csv","ald38_bin3.csv","ald32_bin3.csv","ald39_bin3.csv","ald29_bin1.csv","ald32-bin1.csv","ald38-b2.csv","ald29-b2.csv",
      NULL
      };




   vector<chiptype> chips, chips2;
   systeminfotype sinfo;
   sinfo.min_frequency = min_frequency;   // I need to keep min frequency to something I characterized
   sinfo.min_voltage = 11;   
   const char* system_models[] = { "bin1_tt_b.csv","bin1_ff_a.csv","bin1_old.csv",NULL,"tt_bin1_a.csv", NULL };

   for (int m=0; models[m]!=NULL; m++)
      {
      vector<chiptype> chips;
      systeminfotype sinfo;
      sinfo.min_frequency = min_frequency;   // I need to keep min frequency to something I characterized
      sinfo.min_voltage = 11;
      bool gagan = false;

      chips.push_back(chiptype(models[m]));
      if (gagan)
         {
         float v, f;
         for (v = .250; v <= .350; v += 0.005)
            {
            float hit, current;
            for (f = 1600; f >= 200; f -= 10) {
               chips.back().Interpolate(hit, current, v, 60.0, f);
               if (hit > 0.95)
                  {
                  float e = (v * current) / (f * 0.95 * 254 * 4 / 3 * 1.0e-6);
                  printf("%s V=%.0fmV F=%.0fMhz H=%0.1fGH/s P=%.1fW E=%.1fJ/TH\n", chips.back().label, v * 1000.0, f = f, f * 0.95 * 254 * 4 / 3 * 0.001, v * current, e);
                  break;
                  }
               }
            }
         }

      scenariotype s1(sinfo, 3, 44, 3, chips, 1);
      sweeptype sweep1(s1, models[m], "tuned.csv");
      sweep1.Launch(12);
      }
   exit(0);

   

   chips.push_back(chiptype("ecopart_hires.csv"));
   chips2.push_back(chiptype("ecoff.csv"));
/*
   scenariotype s1(sinfo, 3, 44, 3, chips, 1);
   sweeptype sweep1(s1, "tt", "foo.csv");
   sweep1.Launch(12);

   scenariotype s2(sinfo, 3, 44, 3, chips2, 1);
   sweeptype sweep2(s2, "ff", "foo.csv");
   sweep2.Launch(12);
*/
   chips.push_back(chips2[0]);
/*
   scenariotype s3(sinfo, 3, 44, 3, chips, 4);
   sweeptype sweep3(s3, "random", "foo.csv");
   sweep3.Launch(12);
*/
   scenariotype s4(sinfo, 3, 44, 3, chips, 5);
   sweeptype sweep4(s4, "arrangement5", "foo.csv");
   sweep4.Launch(12);
   
   scenariotype s5(sinfo, 3, 44, 3, chips, 6);
   sweeptype sweep5(s5, "arrangement6", "foo.csv");
   sweep5.Launch(12);


/*
   chiptype c("bin1_ff_2.csv");
   chips.push_back(c);
   scenariotype s1(sinfo, 3, 44, 3, chips, 1);
   s1.Optimize(180,40,false);
   exit(0);
*/
/*
   FILE* fptr = fopen("Baredie.csv", "wt");
   fprintf(fptr, "temp");
   for (float t = 30.0; t < 110.0; t += 5.0)
      fprintf(fptr, ",%.1f", t);
   
   for (int m = 0; models[m] != NULL; m++)
      {
      const char* chptr = models[m];
      
      if (strstr(chptr, "bin1_tt_hb1") != NULL || true)
         {
         chiptype c(chptr);
         chips.push_back(c);
         }
      float v;
      for (int f = 1250; f <= 1250; f += 250) {
         fprintf(fptr, "\n%s_%d", chips.back().label, f);
         for (float t = 30.0; t <= 110.0; t += 10.0)
            fprintf(fptr, ",%.2f", chips.back().MinimumForceT(f, 0.0, v, t));
         }
      }
   fclose(fptr);
   exit(0);
   */

   /*
   chips.push_back(chiptype("bin3_tt_1.csv"));
   scenariotype s1(sinfo, 3, 44, 3, chips, 1);
   scenariotype s2(sinfo, 3, 44, 3, chips, 2);
   scenariotype s3(sinfo, 3, 44, 3, chips, 3);
   scenariotype s4(sinfo, 3, 44, 3, chips, 4);

   s1.Optimize(160, 25, true, true);
   s2.Optimize(160, 25, true, true);
   s3.Optimize(160, 25, true, true);
   s4.Optimize(160, 25, true, true);


   sweeptype sweep(s1, "worst_tt", "worst_tt.csv");
   sweep.Launch(12);

//   scenariotype s2(sinfo, 3, 44, 3, chips, 2);
//   scenariotype s3(sinfo, 3, 44, 3, chips, 3);
//   scenariotype s4(sinfo, 3, 44, 3, chips, 4);
   s1.Optimize(60, 25, true, true);
   s1.Optimize(160, 36, false, true);
   s1.Optimize(100, 25, true, true);
   s1.Optimize(100, 25, false, true);
   //   s2.Optimize(160, 32, true, true);
//   s3.Optimize(160, 32, true, true);
//   s4.Optimize(160, 32, true, true);
   s1.Print();

   exit(0);

   */
#ifdef FOO   
   
   FILE* fptr = fopen("models.csv", "at");
   if (fptr == NULL) FATAL_ERROR;
//   FILE* foo = fopen("foo.csv", "at"); if (foo == NULL) FATAL_ERROR;
   

   fprintf(fptr,"Name,iddq,Cdyn,Fmean,Fstdev,Perf@best,Perf@100,Perf@140,Perf@180,System100@25,System140@25,System180@25,System100@40,System140@40,System180@40\n");
   for (int m = 0; models[m] != NULL; m++)
      {
      float pbest, p100, p140, p180, cdyn, iddq, fmean, fstdev;
      int i;
      chiptype c(models[m]);
      chips.push_back(c);
      chips.back().Performance(pbest, p100, p140, p180);

      for (i = c.curves.size()-1; i>=0; i--)
         {
         if (c.curves[i].temp == 60.0 && c.curves[i].voltage==270)
            {
            cdyn = c.curves[i].cdyn;
            iddq = c.curves[i].iddq;
            fmean = c.curves[i].mean;
            fstdev = c.curves[i].stddev;
            break;
            }
//         fprintf(foo, "%s,%.0f,%.0f,%.0f,%.0f\n",models[m],c.curves[i].voltage, c.curves[i].temp, c.curves[i].mean, c.curves[i].stddev);
         }
      if (i < 0) 
         FATAL_ERROR;
      
      vector<chiptype> single;
      single.push_back(c);
      
      scenariotype s(sinfo, 3, 44, 3, single);
      sweeptype sweep(s, single.back().label, "sweep.csv");
      sweep.Launch(4);

      fprintf(fptr,"%s,%.1f,%.1f,%.0f,%.0f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n",models[m],iddq,cdyn*1000.0,fmean,fstdev,pbest,p100,p140,p180,sweep.p100[0], sweep.p140[0], sweep.p180[0], sweep.p100[1], sweep.p140[1], sweep.p180[1]);
      }
   fclose(fptr);
#endif
   exit(0);
   }
/*
class systemsimtype : public dvfstype {
public:
   int commands;
   int delays;
   scenariotype scenario;
   systemsimtype(const vector<topologytype>& _topology, const systeminfotype& _systeminfo, float variation, float ambient) : dvfstype(_topology, _systeminfo), scenario(systeminfotype(), 44, 3, chiptype("fs_smt.csv")) {
      commands = 0;
      delays = 0;
      InitialSetup();
//      DVFS(50);
      return;
      }
   void ReadWriteConfig(vector<batchtype>& batch) {
      scenario.Program(batch);
      scenario.Solve();
      scenario.Respond(batch);
      commands += batch.size();
      }
   float Power() {
      float power, hashrate, hitrate, max_temp, avg_temp, outlet_temp, ir, efficiency;
      scenario.Stats(power, hashrate, hitrate, max_temp, avg_temp, outlet_temp, ir, efficiency, true);
      return power;
      }
   void SetVoltage(float voltage) {
      scenario.supply = voltage;
      }
   void EnablePowerSwitch(int board){}
   void DisableSystem() {}
   void Delay(int milliSeconds) { delays += milliSeconds; }
   float ReadPower()
      {
      return Power();
      }
   void FanControl(float percentage) {}
   void Foo() {}
   };
*/


