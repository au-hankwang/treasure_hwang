#include "pch.h"
#include "helper.h"
#include "miner.h"
#include "dvfs.h"
#include "serial.h"
#include "usb.h"
#include "test.h"
#include "characterization.h"
#include "multithread.h"


void PrintMinerMap(short* bist, FILE* fptr);


void DataProcessing()
   {
   const char *filename = "data/data.bin";
   FILE* fptr = fopen(filename, "rb");
   if (fptr == NULL) {
      printf("Couldn't open %s for reading\n", filename);
      FATAL_ERROR;
      }
   datatype data;
   vector<datatype> datas;
   while (1 == fread(&data, sizeof(data), 1, fptr))
      {
      if (memcmp(data.unique, "Dac!",sizeof(data.unique)) !=0) FATAL_ERROR;
      if (data.sizeof_this_structure!=sizeof(data)) FATAL_ERROR;
      datas.push_back(data);
      }
   printf("I found %d data records\n", datas.size());
   fclose(fptr);
   filename = "data/data_regenerated.csv";
   fptr = fopen(filename, "wt");
   if (fptr == NULL) {
      printf("Couldn't open %s for writing\n", filename);
      FATAL_ERROR;
      }
   datas[0].WriteToCSV(fptr,true);
   for (int i = 0; i < datas.size(); i++)
      {
      datas[i].WriteToCSV(fptr);
      }
   fclose(fptr);
   vector<float> bist_average(254,0.0);
   short bist_short[254];
   int count = 0, i, k;
   for (i = 0; i < datas.size(); i++)
      {
      if (datas[i].bist_total[0]) {
         for (k = 0; k < 254; k++)
            bist_average[k] += datas[i].bist_extended[k];
         count++;
         }
      }
   for (i = 0; i < 254; i++)
      bist_short[i] = bist_average[i] / count;
   fptr = fopen("data/bistaveragemap.csv", "wt");
   PrintMinerMap(bist_short, fptr);
   fclose(fptr);
   
   fptr = fopen("data/david.csv", "wt");
   datas[0].WriteToCSV(fptr, true);
   for (i = 0; i < datas.size(); i++)
      {
      if (datas[i].CMhit95[0] && datas[i].tp.temp==60.0 && datas[i].station==2) {
         datas[i].WriteToCSV(fptr);
         }
      }

   fclose(fptr);
   }

void PrintMinerMap(short* bist, FILE* fptr)
   {
   int r, c, index;
   for (r = 0; r < 15; r++)
      {
      for (c = 0; c < 16; c++)
         {
         if (c % 16 < 8)
            index = r * 16 + (15 - (c % 16));
         else
            index = r * 16 + (c % 16) - 8;
         fprintf(fptr, ",%d", bist[index]);
         if (c == 7)
            fprintf(fptr, ",");
         }
      fprintf(fptr, "\n");
      }
   for (c = 0; c < 7; c++)
      {
      index = 253 - c;
      fprintf(fptr, ",%d", bist[index]);
      }
   fprintf(fptr, ",,,");
   for (c = 0; c < 7; c++)
      {
      index = 240 + c;
      fprintf(fptr, ",%d", bist[index]);
      }
   fprintf(fptr, "\n");
   }
