#include "pch.h"
#include "sha256.h"
#include "helper.h"
#include "miner.h"
#include "dvfs.h"
#include "electrical.h"




void Eval_Simulator_Reference() {
   const char* models[] = {
            "bin1_tt_a.csv","bin1_tt_a1.csv","bin1_tt_b.csv","bin1_tt_hb1_1.csv","bin1_tt_hb1_2.csv","bin1_tt_hb1_3.csv","bin1_tt_hb1_4.csv","bin1_tt_hb1_5.csv",
            "bin1_ff_a.csv","bin1_ff_2.csv","ff_a.csv","bin1_ff_tr2_e1_21.6j_9.7l.csv","bin1_ff_tr2d5_21.7j_15.2l.csv","bin1_ff_tr3_b8_21.2j_9.7l.csv","bin1_ff_tr3_q8_25j_22.4l.csv","bin1_ff_tr3_r6_23j_16.csv",
            "bin2_tt_1.csv","bin2_tt_2.csv","bin2_tt_hb2_1.csv","bin2_tt_hb2_2.csv","bin2_tt_hb2_3.csv","bin2_tt_hb2_3_hires.csv",
            "bin3_tt_1.csv","bin3_tt_2.csv","ecopart_hires.csv","eco_8.csv","eco_complex.csv","ecoff.csv","ecoff_2.csv","ecoff_3.csv",NULL };
   vector<chiptype> chips;
   systeminfotype sinfo;
   sinfo.min_frequency = min_frequency;   // I need to keep min frequency to something I characterized
   sinfo.min_voltage = 11;
   int i;


   for (int m = 0; models[m] != NULL; m++)
      {
      const char* chptr = models[m];

      if (strstr(chptr, "bin1_tt") != NULL)
         {
         chiptype c(chptr);
         chips.push_back(c);
         }
      }
   
   scenariotype s(sinfo, 3, 44, 3, chips, 4);   // random arrangement of all chip types

   s.ambient = 30;
   s.supply = 12.5;
   for (i = 0; i < s.asics.size(); i++)
      {
      electricaltype& a = s.asics[i];
      a.frequency = 900.0; // set every chip to 800Mhz
      }
   s.Solve();  
   s.Print();
   float power, hashrate, hitrate, max_temp, avg_temp, outlet_temp, ir, efficiency;
   s.Stats(power, hashrate, hitrate, max_temp, avg_temp, outlet_temp, ir, efficiency);
   printf("Efficiency is %.1f J/TH for %.1fTH\n",efficiency,hashrate);
   
   // Run dvfs for 140TH, 25C ambient
   s.Optimize(140, 25);
   s.Print();
   s.Stats(power, hashrate, hitrate, max_temp, avg_temp, outlet_temp, ir, efficiency);
   printf("Efficiency is %.1f J/TH for %.1fTH\n", efficiency, hashrate);

   exit(0);
   }

