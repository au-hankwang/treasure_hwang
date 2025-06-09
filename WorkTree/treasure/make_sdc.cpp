#include <stdio.h>

// g++ make_sdc.cpp
// ./a.out


void Prolog(FILE* fptr) {
   fprintf(fptr, "#set_units -time ns -resistance kOhm -capacitance pF -voltage V -current mA\n\n");
   fprintf(fptr, "set CLK_NAME clk_hash_i[1]\n");
   fprintf(fptr, "set REF_CLK_NAME clk_ref_i\n\n");
   fprintf(fptr, "set CLK_PERIOD  0.600\n");
   fprintf(fptr, "set CLK_PERIOD1 $CLK_PERIOD\n");
   fprintf(fptr, "set REF_CLK_PERIOD 5.00\n\n");
   fprintf(fptr, "create_clock [get_ports $CLK_NAME] -period ${CLK_PERIOD} -name ${CLK_NAME}\n");
   fprintf(fptr, "create_clock [get_ports $REF_CLK_NAME] -period ${REF_CLK_PERIOD} -name ${REF_CLK_NAME}\n");
   }
void Epilog(FILE* fptr) {
   char buffer[1024];
   FILE* src = fopen("epilog.txt","r");
   if (src == NULL)
      printf("I couldn't open epilog.txt for reading\n");

   while (fread(buffer, 1,1,src) ==1)
      fwrite(buffer, 1, 1, fptr);
   fclose(src);
   }


#ifdef _MSC_VER
int MakeSdc()
#else
int main()
#endif
   {
   const char* hierarchy[67] = {
   "miner_structural/f0[0].startclk",
   "miner_structural/f1[1].pipe/clkgen",
   "miner_structural/f2[2].pipe/clkgen",
   "miner_structural/f3[3].pipe/clkgen",
   "miner_structural/f4[4].pipe/clkgen",
   "miner_structural/f5[5].pipe/clkgen",
   "miner_structural/f6[6].pipe/clkgen",
   "miner_structural/f7[7].pipe/clkgen",
   "miner_structural/f8[8].pipe/clkgen",
   "miner_structural/f9[9].pipe/clkgen",
   "miner_structural/f9[10].pipe/clkgen",
   "miner_structural/f9[11].pipe/clkgen",
   "miner_structural/f9[12].pipe/clkgen",
   "miner_structural/f9[13].pipe/clkgen",
   "miner_structural/f9[14].pipe/clkgen",
   "miner_structural/f9[15].pipe/clkgen",
   "miner_structural/f9[16].pipe/clkgen",
   "miner_structural/f9[17].pipe/clkgen",
   "miner_structural/f9[18].pipe/clkgen",
   "miner_structural/f9[19].pipe/clkgen",
   "miner_structural/f9[20].pipe/clkgen",
   "miner_structural/f9[21].pipe/clkgen",
   "miner_structural/f9[22].pipe/clkgen",
   "miner_structural/f9[23].pipe/clkgen",
   "miner_structural/f24[24].pipe/clkgen",
   "miner_structural/f25[25].pipe/clkgen",
   "miner_structural/f26[26].pipe/clkgen",
   "miner_structural/f27[27].pipe/clkgen",
   "miner_structural/f27[28].pipe/clkgen",
   "miner_structural/f27[29].pipe/clkgen",
   "miner_structural/f27[30].pipe/clkgen",
   "miner_structural/f31[31].pipe/clkgen",
   "miner_structural/s32[32].pipe/clkgen",
   "miner_structural/s33[33].pipe/clkgen",
   "miner_structural/s34[34].pipe/clkgen",
   "miner_structural/s35[35].pipe/clkgen",
   "miner_structural/s36[36].pipe/clkgen",
   "miner_structural/s37[37].pipe/clkgen",
   "miner_structural/s38[38].pipe/clkgen",
   "miner_structural/s39[39].pipe/clkgen",
   "miner_structural/s40[40].pipe/clkgen",
   "miner_structural/s41[41].pipe/clkgen",
   "miner_structural/s42[42].pipe/clkgen",
   "miner_structural/s42[43].pipe/clkgen",
   "miner_structural/s42[44].pipe/clkgen",
   "miner_structural/s42[45].pipe/clkgen",
   "miner_structural/s42[46].pipe/clkgen",
   "miner_structural/s42[47].pipe/clkgen",
   "miner_structural/s42[48].pipe/clkgen",
   "miner_structural/s42[49].pipe/clkgen",
   "miner_structural/s42[50].pipe/clkgen",
   "miner_structural/s42[51].pipe/clkgen",
   "miner_structural/s42[52].pipe/clkgen",
   "miner_structural/s42[53].pipe/clkgen",
   "miner_structural/s42[54].pipe/clkgen",
   "miner_structural/s42[55].pipe/clkgen",
   "miner_structural/s56[56].pipe/clkgen",
   "miner_structural/s57[57].pipe/clkgen",
   "miner_structural/s58[58].pipe/clkgen",
   "miner_structural/s59[59].pipe/clkgen",
   "miner_structural/s59[60].pipe/clkgen",
   "miner_structural/s59[61].pipe/clkgen",
   "miner_structural/s62[62].pipe/clkgen",
   "miner_structural/s63[63].pipe/clkgen",
   "miner_structural/s64[64].pipe/clkgen",
   "miner_structural/s65[65].pipe/clkgen",
   "miner_structural/s66[66].resultclk" };

   int p, i, e0, e1, e2;
   char filename[64];
   for (p = 4; p <8; p++)
      {
      const char* label[] = { "0_2p","1_3p", "2_4p", "3_5p", "4_4p", "5_6p", "6_8p", "7_10p" };
      sprintf(filename, "miner_harness_%s.sdc",label[p]);
      FILE* fptr = fopen(filename, "wt");
      Prolog(fptr);
      int phase = p >= 4 ? (p-4 + 2) * 2 : (p + 2);


      for (i = 1; i <=64; i++)
         {
	 bool skip = i == 33 || i == 64;
	 int offset = i <= 31 ? 4 : i<=33 ? 3 : i <= 63 ? 2 : i <= 64 ? 1 : 0;

	 e0 = (-1 - i * 2 + offset + phase * 256) % (phase);
	 e1 = e0 + 1;
	 e2 = e0 + phase;
	 if (i >= 32) {
	    fprintf(fptr, "\ncreate_generated_clock -name     fast_clk0_%-3d -edges {%2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s0/dont_touch_cell_mux_fast0/X}]", i * 2 + 0, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    fprintf(fptr, "\ncreate_generated_clock -name     fast_clk1_%-3d -edges {%2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s1/dont_touch_cell_mux_fast0/X}]", i * 2 + 0, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    }
	 else if (i > 1)
	    fprintf(fptr, "\ncreate_generated_clock -name     fast_clk_%-3d -edges {%2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_mux_fast0/X}]", i * 2 + 0, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);

	 e0 = ((skip ? -1 : 0) - 2 - i * 2 + offset + phase * 256) % (phase);
         e1 = e0 + 1;
         e2 = e0 + phase;
	 if (i >= 32) {
	    fprintf(fptr, "\ncreate_generated_clock -name     fast_clk0_%-3d -edges {%2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s0/dont_touch_cell_mux_fast1/X}]", i * 2 + 1, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    fprintf(fptr, "\ncreate_generated_clock -name     fast_clk1_%-3d -edges {%2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s1/dont_touch_cell_mux_fast1/X}]", i * 2 + 1, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    }
	 else
	    fprintf(fptr, "\ncreate_generated_clock -name     fast_clk_%-3d -edges {%2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_mux_fast1/X}]", i * 2 + 1, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
         }
      fprintf(fptr, "\n");
      for (i = 0; i <= 66; i++)
	 {
	 int offset = i <= 31 ? 4 : i <= 33 ? 3 : i <= 63 ? 2 : 0;
//         if (phase == 4) offset -= 2;
	 e0 = (0 + phase - i * 2 + offset + phase * 256) % (phase * 2);
         e1 = e0 + 1;
         e2 = e0 + phase * 2;
	 if (i < 32)
	    fprintf(fptr, "\ncreate_generated_clock -name   odd_slow_clk_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_slow1/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	 else if (i >= 34 && i != 64) {
	    fprintf(fptr, "\ncreate_generated_clock -name  odd_slow_clk0_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s0/dont_touch_cell_slow1/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    fprintf(fptr, "\ncreate_generated_clock -name  odd_slow_clk1_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s1/dont_touch_cell_slow1/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    }

	 e0 = (0 - i * 2 + offset + phase * 256) % (phase * 2);
	 e1 = e0 + 1;
	 e2 = e0 + phase * 2;
	 if (i < 32 && i>=1)
	    fprintf(fptr, "\ncreate_generated_clock -name  even_slow_clk_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_slow0/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	 else if (i >= 34 && i!=64 && i != 66) {
	    fprintf(fptr, "\ncreate_generated_clock -name even_slow_clk0_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s0/dont_touch_cell_slow0/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    fprintf(fptr, "\ncreate_generated_clock -name even_slow_clk1_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s1/dont_touch_cell_slow0/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    }
	 }
      fprintf(fptr, "\n\n");
      for (i = 8; i <= 62; i++)
	 {
	 int offset = i <= 31 ? 4 : i <= 33 ? 3 : i <= 63 ? 2 : 0;
//         if (phase == 4) offset += 2;
         e0 = (0 + phase -( i-3) * 2 + offset + phase * 256) % (phase * 2);
	 e1 = e0 + 1;
	 e2 = e0 + phase * 2;
	 if (i < 32)
	    fprintf(fptr, "\ncreate_generated_clock -name   odd_slow_minus3_clk_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_minus1/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	 else if (i >= 41) {
	    fprintf(fptr, "\ncreate_generated_clock -name  odd_slow_minus3_clk0_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s0/dont_touch_cell_minus1/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    fprintf(fptr, "\ncreate_generated_clock -name  odd_slow_minus3_clk1_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s1/dont_touch_cell_minus1/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    }

	 e0 = (0 - (i - 3) * 2 + offset + phase * 256) % (phase * 2);
	 e1 = e0 + 1;
	 e2 = e0 + phase * 2;
	 if (i < 32 && i >= 1)
	    fprintf(fptr, "\ncreate_generated_clock -name  even_slow_minus3_clk_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_minus0/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	 else if (i >= 41) {
	    fprintf(fptr, "\ncreate_generated_clock -name even_slow_minus3_clk0_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s0/dont_touch_cell_minus0/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    fprintf(fptr, "\ncreate_generated_clock -name even_slow_minus3_clk1_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s1/dont_touch_cell_minus0/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    }
	 }
      fprintf(fptr, "\n");
      Epilog(fptr);
      fclose(fptr);
      }
   for (p = 0; p < 4; p++)
      {
      const char* label[] = { "0_2p","1_3p", "2_4p", "3_5p", "4_4p", "5_6p", "6_8p", "7_10p" };
      sprintf(filename, "miner_harness_%s.sdc", label[p]);
      FILE* fptr = fopen(filename, "wt");
      Prolog(fptr);
      int phase = (p + 2);

      for (i = 1; i <= 64; i++)
	 {
	 e0 = (0 - i * 4 + phase * 256) % (phase * 2);
	 e1 = e0 + 1;
	 e2 = e0 + phase*2;
	 if (i >= 32) {
	    fprintf(fptr, "\ncreate_generated_clock -name     fast_clk0_%-3d -edges {%2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s0/dont_touch_cell_mux_fast0/X}]", i * 2 + 0, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    fprintf(fptr, "\ncreate_generated_clock -name     fast_clk1_%-3d -edges {%2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s1/dont_touch_cell_mux_fast0/X}]", i * 2 + 0, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    }
	 else if (i > 1)
	    fprintf(fptr, "\ncreate_generated_clock -name     fast_clk_%-3d -edges {%2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_mux_fast0/X}]", i * 2 + 0, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);

	 e0 = (-2 - i * 4 + phase * 256) % (phase * 2);
	 e1 = e0 + 1;
	 e2 = e0 + phase*2;
	 if (i >= 32) {
	    fprintf(fptr, "\ncreate_generated_clock -name     fast_clk0_%-3d -edges {%2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s0/dont_touch_cell_mux_fast1/X}]", i * 2 + 1, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    fprintf(fptr, "\ncreate_generated_clock -name     fast_clk1_%-3d -edges {%2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s1/dont_touch_cell_mux_fast1/X}]", i * 2 + 1, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    }
	 else
	    fprintf(fptr, "\ncreate_generated_clock -name     fast_clk_%-3d -edges {%2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_mux_fast1/X}]", i * 2 + 1, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	 }
      fprintf(fptr, "\n");
      for (i = 0; i <= 66; i++)
	 {
         e0 = (-4 + phase * 2 - i * 4 + (i == 0 ? -2 : 0) + phase * 256) % (phase * 4);
	 e1 = e0 + 1;
	 e2 = e0 + phase * 4;
	 if (i < 32)
	    fprintf(fptr, "\ncreate_generated_clock -name   odd_slow_clk_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_slow1/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	 else if (i >= 34 && i != 64) {
	    fprintf(fptr, "\ncreate_generated_clock -name  odd_slow_clk0_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s0/dont_touch_cell_slow1/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    fprintf(fptr, "\ncreate_generated_clock -name  odd_slow_clk1_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s1/dont_touch_cell_slow1/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    }

         e0 = (-4 - i * 4 + (i == 0 ? -2 : 0) + phase * 256) % (phase * 4);
	 e1 = e0 + 1;
	 e2 = e0 + phase * 4;
	 if (i < 32 && i >= 1)
	    fprintf(fptr, "\ncreate_generated_clock -name  even_slow_clk_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_slow0/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	 else if (i >= 34 && i != 64 && i != 66) {
	    fprintf(fptr, "\ncreate_generated_clock -name even_slow_clk0_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s0/dont_touch_cell_slow0/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    fprintf(fptr, "\ncreate_generated_clock -name even_slow_clk1_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s1/dont_touch_cell_slow0/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    }
	 }
      fprintf(fptr, "\n");
      for (i = 8; i <= 62; i++)
	 {
         e0 = (-4 + phase * 2 - (i - 3) * 4 + (i == 0 ? -2 : 0) + phase * 256) % (phase * 4);
         e1 = (e0 + 1) % (phase * 4);
         e2 = e0 + phase * 4;
         if (i < 32)
	    fprintf(fptr, "\ncreate_generated_clock -name   odd_slow_minus3_clk_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_minus1/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	 else if (i >= 41) {
	    fprintf(fptr, "\ncreate_generated_clock -name  odd_slow_minus3_clk0_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s0/dont_touch_cell_minus1/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    fprintf(fptr, "\ncreate_generated_clock -name  odd_slow_minus3_clk1_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s1/dont_touch_cell_minus1/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    }

         e0 = (-4 - (i - 3) * 4 + (i == 0 ? -2 : 0) + phase * 256) % (phase * 4);
         e1 = (e0 + 1) % (phase * 4);
         e2 = e0 + phase * 4;
         if (i < 32 && i >= 1)
	    fprintf(fptr, "\ncreate_generated_clock -name  even_slow_minus3_clk_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s/dont_touch_cell_minus0/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	 else if (i >= 41) {
	    fprintf(fptr, "\ncreate_generated_clock -name even_slow_minus3_clk0_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s0/dont_touch_cell_minus0/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    fprintf(fptr, "\ncreate_generated_clock -name even_slow_minus3_clk1_%-3d -edges { %2d  %2d %2d }  -source [get_ports $CLK_NAME ] [get_pins {%s1/dont_touch_cell_minus0/X}]", i, e0 + 1, e1 + 1, e2 + 1, hierarchy[i]);
	    }
	 }
      fprintf(fptr, "\n");
      Epilog(fptr);
      fclose(fptr);
      }
   return 0;
   }


/*
create_generated_clock -name fast_clk_129 -edges { 3  5 10}  -source [get_ports $CLK_NAME ] [get_pins {s64_64].pipe/clkgen/dont_touch_cell_mux_fast1/X}]
create_generated_clock -name fast_clk_128 -edges { 5  7 12}  -source [get_ports $CLK_NAME ] [get_pins {s64_64].pipe/clkgen/dont_touch_cell_mux_fast0/X}]
create_generated_clock -name fast_clk_127 -edges { 7  8 13}  -source [get_ports $CLK_NAME ] [get_pins {s63_63].pipe/clkgen/dont_touch_cell_mux_fast1/X}]


create_generated_clock -name fast_clk_66  -edges { 9 10 15}  -source [get_ports $CLK_NAME ] [get_pins {f33_33].pipe/clkgen/dont_touch_cell_mux_fast0/X}]
create_generated_clock -name fast_clk_65  -edges {10 11 16}  -source [get_ports $CLK_NAME ] [get_pins {f32_32].pipe/clkgen/dont_touch_cell_mux_fast1/X}]
create_generated_clock -name fast_clk_64  -edges {11 13 18}  -source [get_ports $CLK_NAME ] [get_pins {f32_32].pipe/clkgen/dont_touch_cell_mux_fast0/X}]

*/