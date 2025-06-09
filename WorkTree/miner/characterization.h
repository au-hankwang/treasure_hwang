#ifndef __CHARACTERIZATION_H_INCLUDED__
#define __CHARACTERIZATION_H_INCLUDED__
#include "pch.h"
#include "helper.h"
#include "miner.h"
#include "dvfs.h"
#include "serial.h"
#include "usb.h"

//#pragma optimize("", off)

struct speedpointtype {
   float voltage, mhz, core_current, scv_io_current;
   float Power() {
      return voltage * core_current + scv_io_current * 6.0;
      }
   float Efficiency() {
      return Power() / (mhz * 250 * 4 / 3.0 * 1.0e-6);
      }
   };
struct cdyntype {
   float voltage, mhz, core_current, cdyn;
   };

struct datatype {
   char unique[4];   // 'Dac!'
   int sizeof_this_structure;
   time_t seconds;
   char partname[32];
   short station;
   testpointtype tp;

   float vih, vil, iih, iil[10];  // iil is +50,0,-50,-100,-150,-200,-250,-300,-350,-400
   float scv_current_0Mhz, scv_current_25Mhz, scv_current_100Mhz; // hashclk=10Mhz
   float scv_current_1Ghz, scv_current_2Ghz;                      // refclk=25Mhz

   float pll_min[4], pll_max[4];    // min and max lock ranges for vcosel={0,1,2,3}
   float pll_io_current[5];         // 0,1,2,4,8 Ghz

   unsigned short ip_temp;
   unsigned int ip_dro[32];         // bit[31]=is error
   unsigned short ip_voltage_25[18], ip_voltage_1000[18], ip_voltage_2000[18];

   float core_current[33]; // current in 100Mhz increments starting at 0
   short bist[254], bist_extended[254];   // frequency in Mhz
   speedpointtype speed[151], spare[39];             // voltage from 200mV to 350mV
   cdyntype cdyn270[16], cdyn290[16], cdyn310[16], cdyn330[16], cdyn350[16], cdyn370[16], cdyn390[16];

   float pll_scv_current[5];         // 0,1,2,4,8 Ghz
   float iddq, iddq_temp;
   short bist_total[4], bistextended_total[4];  // bist results for 750,1000,1250,1500 at this testpoint(odv compensated)
   float hit50[4], hit95[4], hit95current[4];          // voltage for 50% and 95% hit rate for 750,1000,1250,1500 Mhz
   float CMhit50[4], CMhit95[4], CMhit95current[4], CMbist[4];          // voltage for 50% and 95% hit rate for 750,1000,1250,1500 Mhz
   float hit95temp[4], CMhit95temp[4];
   float hit95isl[4], CMhit95isl[4];
   float spare2[64-11-12-16-8-8];

   datatype() {
      memset(this, 0, sizeof(datatype));
      sizeof_this_structure = sizeof(datatype);
      memcpy(unique, "Dac!", sizeof(unique));
      }
   void WriteToCSV(FILE* fptr, bool label=false) {
      int i,k;
      if (label) {
         fprintf(fptr, "sizeofstructure,station,time,partname,temp,io voltage,scv voltage,core voltage");
         fprintf(fptr, ",iddq,iddq_temp,iddq_normalized");
//         fprintf(fptr, ",bist[800],bist[1000],bist[1200],bist[1400]");
//         fprintf(fptr, ",bist_extend[750],bist_extend[1000],bist_extend[1250],bist_extend[1500]");
//         fprintf(fptr, ",hit50[750],hit50[1000],hit50[1250],hit50[1500]");
//         fprintf(fptr, ",hit95[750],hit95[1000],hit95[1250],hit95[1500]");
//         fprintf(fptr, ",hit95isl[750],hit95isl[1000],hit95isl[1250],hit95isl[1500]");
//         fprintf(fptr, ",hit95current[750],hit95current[1000],hit95current[1250],hit95current[1500]");
//         fprintf(fptr, ",hit95Efficiency[750],hit95Efficiency[1000],hit95Efficiency[1250],hit95Efficiency[1500]");
//         fprintf(fptr, ",hit95Temp[750],hit95Temp[1000],hit95Temp[1250],hit95Temp[1500]");

//         fprintf(fptr, ",CMhit50[750],CMhit50[1000],CMhit50[1250],CMhit50[1500]");
         fprintf(fptr, ",CMhit95[750],CMhit95[1000],CMhit95[1250],CMhit95[1500]");
         fprintf(fptr, ",CMhit95isl[750],CMhit95isl[1000],CMhit95isl[1250],CMhit95isl[1500]");
         fprintf(fptr, ",CMhit95current[750],hit95current[1000],hit95current[1250],hit95current[1500]");
         fprintf(fptr, ",CMhit95Efficiency[750],hit95Efficiency[1000],hit95Efficiency[1250],hit95Efficiency[1500]");
         fprintf(fptr, ",CMbist[750],CMbist[1000],CMbist[1250],CMbist[1500]");
         fprintf(fptr, ",CMhit95Temp[750],CMhit95Temp[1000],CMhit95Temp[1250],CMhit95Temp[1500]");

         fprintf(fptr, ",vih,vil,iih");
         for (i = 0; i < 10; i++) fprintf(fptr, ",iil[%d]", 50-i*50);
         fprintf(fptr, ",scv_current_25mhz");
         fprintf(fptr, ",scv_current_1ghz,scv_current_2ghz");
         for (i = 0; i < 4; i++) fprintf(fptr, ",pll_min[%d]", i);
         for (i = 0; i < 4; i++) fprintf(fptr, ",pll_max[%d]", i);
         for (i = 0; i < 5; i++) fprintf(fptr, ",pll_current[%dGhz]", i == 0 ? 0 : i == 1 ? 1 : i == 2 ? 2 : i == 3 ? 4 : 8);
         for (i = 0; i < 5; i++) fprintf(fptr, ",pll_scv_current[%dGhz]", i == 0 ? 0 : i == 1 ? 1 : i == 2 ? 2 : i == 3 ? 4 : 8);
         fprintf(fptr, ",ip_temp");
         for (i = 0; i < 32; i++) {
            const char* label = i == 24 ? "EInv" : i == 25 ? "ENor3" : i == 26 ? "ENand3" : i == 27 ? "ECsa" : i == 28 ? "UInv" : i == 29 ? "UNor3" : i == 30 ? "UNand3" : i == 31 ? "UCsa" : "";
            fprintf(fptr, ",dro[%d]%s", i, label);
            }
         for (i = 0; i < 18; i++) fprintf(fptr, ",dvm_25[%d]", i);
         for (i = 0; i < 18; i++) fprintf(fptr, ",dvm_1000[%d]", i);
//         for (i = 0; i < 18; i++) fprintf(fptr, ",dvm_1500[%d]", i);
         for (i = 0; i <= 20; i++) fprintf(fptr, ",core_current[%d]", i*100);
         for (i = 1; i < 6; i++) {
            fprintf(fptr, ",cdyn270[%dMhz].voltage", i*100);
            fprintf(fptr, ",cdyn270[%dMhz].mhz", i * 100);
            fprintf(fptr, ",cdyn270[%dMhz].core_current", i * 100);
            fprintf(fptr, ",cdyn270[%dMhz].cdyn", i * 100);
            }
         /*for (i = 0; i < 6; i++) {
            fprintf(fptr, ",cdyn290[%dMhz].voltage", i * 100);
            fprintf(fptr, ",cdyn290[%dMhz].mhz", i * 100);
            fprintf(fptr, ",cdyn290[%dMhz].core_current", i * 100);
            fprintf(fptr, ",cdyn290[%dMhz].cdyn", i * 100);
            }*/
         for (i = 1; i < 6; i++) {
            fprintf(fptr, ",cdyn310[%dMhz].voltage", i * 100);
            fprintf(fptr, ",cdyn310[%dMhz].mhz", i * 100);
            fprintf(fptr, ",cdyn310[%dMhz].core_current", i * 100);
            fprintf(fptr, ",cdyn310[%dMhz].cdyn", i * 100);
            }
         /*for (i = 0; i < 6; i++) {
            fprintf(fptr, ",cdyn330[%dMhz].voltage", i * 100);
            fprintf(fptr, ",cdyn330[%dMhz].mhz", i * 100);
            fprintf(fptr, ",cdyn330[%dMhz].core_current", i * 100);
            fprintf(fptr, ",cdyn330[%dMhz].cdyn", i * 100);
            }*/
         for (i = 1; i < 6; i++) {
            fprintf(fptr, ",cdyn350[%dMhz].voltage", i * 100);
            fprintf(fptr, ",cdyn350[%dMhz].mhz", i * 100);
            fprintf(fptr, ",cdyn350[%dMhz].core_current", i * 100);
            fprintf(fptr, ",cdyn350[%dMhz].cdyn", i * 100);
            }


/*         for (i = 0; i < 150; i++) {
            fprintf(fptr, ",speed[%dMhz].voltage", 250 + i);
            fprintf(fptr, ",speed[%dMhz].mhz", 250 + i);
            fprintf(fptr, ",speed[%dMhz].core_current", 250 + i);
            fprintf(fptr, ",speed[%dMhz].efficiency", 250 + i);
            }*/
//         for (i = 0; i < 254; i++) fprintf(fptr, ",bist[%d]", i);
//         for (i = 0; i < 254; i++) fprintf(fptr, ",bist_extended[%d]", i);
         fprintf(fptr, "\n");
         return;
         }
      fprintf(fptr, "%d,%d,%d,%s,%0.1f,%.3f,%.3f,%.3f",sizeof_this_structure,station,(int)seconds,partname,tp.temp,tp.io,tp.scv,tp.core);
      fprintf(fptr, ",%.2f,%.1f,%.2f",iddq,iddq_temp, iddq * exp2((85 - iddq_temp) / 23.0));
//      fprintf(fptr, ",%d,%d,%d,%d",bist_total[0], bist_total[1], bist_total[2], bist_total[3]);
//      fprintf(fptr, ",%d,%d,%d,%d", bistextended_total[0], bistextended_total[1], bistextended_total[2], bistextended_total[3]);
//      fprintf(fptr, ",%.4f,%.4f,%.4f,%.4f",hit50[0], hit50[1], hit50[2], hit50[3]);
//      fprintf(fptr, ",%.4f,%.4f,%.4f,%.4f", hit95[0], hit95[1], hit95[2], hit95[3]);
//      fprintf(fptr, ",%.4f,%.4f,%.4f,%.4f", hit95isl[0], hit95isl[1], hit95isl[2], hit95isl[3]);
//      fprintf(fptr, ",%.2f,%.2f,%.2f,%.2f", hit95current[0], hit95current[1], hit95current[2], hit95current[3]);
//      for (k = 0; k < 4; k++) {
//         float f = k == 0 ? 750 : k == 1 ? 1000 : k == 2 ? 1250 : 1500;
//         float iddq_eff = iddq * exp2((hit95temp[k] - hit95temp[k]) / 23.0) * (hit95[k] / 0.31);
//         float efficiency = ((hit95current[k] - iddq_eff / 2) * hit95[k]) / (f * 1.0e6 * 254 * 4 / 3 / 2 * 1.0e-12) / 0.95;
//         fprintf(fptr, ",%.1f", efficiency);
//         }

//      fprintf(fptr, ",%.1f,%.1f,%.1f,%.1f", (hit95current[0] * hit95[0]) / (750 * 1.0e6 * 254 * 4 / 3 * 1.0e-12), (hit95current[1] * hit95[1]) / (1000 * 1.0e6 * 254 * 4 / 3 * 1.0e-12), (hit95current[2] * hit95[2]) / (1250 * 1.0e6 * 254 * 4 / 3 * 1.0e-12), (hit95current[3] * hit95[3]) / (1500 * 1.0e6 * 254 * 4 / 3 * 1.0e-12));
      fprintf(fptr, ",%.1f,%.1f,%.1f,%.1f", hit95temp[0], hit95temp[1], hit95temp[2], hit95temp[3]);

//      fprintf(fptr, ",%.4f,%.4f,%.4f,%.4f", CMhit50[0], CMhit50[1], CMhit50[2], CMhit50[3]);
      fprintf(fptr, ",%.4f,%.4f,%.4f,%.4f", CMhit95[0], CMhit95[1], CMhit95[2], CMhit95[3]);
      fprintf(fptr, ",%.4f,%.4f,%.4f,%.4f", CMhit95isl[0], CMhit95isl[1], CMhit95isl[2], CMhit95isl[3]);
      fprintf(fptr, ",%.2f,%.2f,%.2f,%.2f", CMhit95current[0], CMhit95current[1], CMhit95current[2], CMhit95current[3]);
      for (k = 0; k < 4; k++) {
         float f = k == 0 ? 750 : k == 1 ? 1000 : k == 2 ? 1250 : 1500;
         float iddq_eff = iddq * exp2((CMhit95temp[k] - CMhit95temp[k]) / 23.0) * (CMhit95[k] / 0.31);
//         float efficiency = ((CMhit95current[k] - iddq_eff / 2) * CMhit95[k]) / (f * 1.0e6 * 254 * 4 / 3 / 2 * 1.0e-12) / 0.95;
         float efficiency = (CMhit95current[k] * CMhit95isl[k]) / (f * 1.0e6 * 254 * 4 / 3 * 1.0e-12) / 0.95;
         fprintf(fptr, ",%.1f", efficiency);
         }
      fprintf(fptr, ",%.4f,%.4f,%.4f,%.4f", CMbist[0], CMbist[1], CMbist[2], CMbist[3]);
      fprintf(fptr, ",%.1f,%.1f,%.1f,%.1f", CMhit95temp[0], CMhit95temp[1], CMhit95temp[2], CMhit95temp[3]);

      fprintf(fptr, ",%0.3e,%0.3e,%0.3e", vih,vil,iih);
      for (i=0; i<10; i++) fprintf(fptr, ",%0.3e", iil[i]);
      fprintf(fptr, ",%0.3e", scv_current_25Mhz);
      fprintf(fptr, ",%0.3e,%0.3e", scv_current_1Ghz, scv_current_2Ghz);
      for (i = 0; i < 4; i++) fprintf(fptr, ",%0.3f", pll_min[i]);
      for (i = 0; i < 4; i++) fprintf(fptr, ",%0.3f", pll_max[i]);
      for (i = 0; i < 5; i++) fprintf(fptr, ",%0.3e", pll_io_current[i]);
      for (i = 0; i < 5; i++) fprintf(fptr, ",%0.3e", pll_scv_current[i]);
      fprintf(fptr, ",%d", ip_temp);
      for (i = 0; i < 32; i++) fprintf(fptr, ",%d", ip_dro[i]);
      for (i = 0; i < 18; i++) fprintf(fptr, ",%d", ip_voltage_25[i]);
      for (i = 0; i < 18; i++) fprintf(fptr, ",%d", ip_voltage_1000[i]);
//      for (i = 0; i < 18; i++) fprintf(fptr, ",%d", ip_voltage_2000[i]);
      for (i = 0; i <= 20; i++) fprintf(fptr, ",%.2f", core_current[i]);
      for (i = 1; i < 6; i++) {
         fprintf(fptr, ",%.4f",cdyn270[i].voltage);
         fprintf(fptr, ",%.0f", cdyn270[i].mhz);
         fprintf(fptr, ",%.2f", cdyn270[i].core_current);
         fprintf(fptr, ",%.2f", cdyn270[i].cdyn);
         }
      for (i = 1; i < 6; i++) {
         fprintf(fptr, ",%.4f", cdyn310[i].voltage);
         fprintf(fptr, ",%.0f", cdyn310[i].mhz);
         fprintf(fptr, ",%.2f", cdyn310[i].core_current);
         fprintf(fptr, ",%.2f", cdyn310[i].cdyn);
         }
      for (i = 1; i < 6; i++) {
         fprintf(fptr, ",%.4f", cdyn350[i].voltage);
         fprintf(fptr, ",%.0f", cdyn350[i].mhz);
         fprintf(fptr, ",%.2f", cdyn350[i].core_current);
         fprintf(fptr, ",%.2f", cdyn350[i].cdyn);
         }

//      for (i = 0; i < 254; i++) fprintf(fptr, ",%d", bist[i]);
//      for (i = 0; i < 254; i++) fprintf(fptr, ",%d", bist_extended[i]);
      fprintf(fptr, "\n");
      }
   };




class thermalheadtype {
   char ip_address[64];
   const int secondhead;
   SOCKET thermal_head_socket;
   float tolerance;

public:
   thermalheadtype(const char* _ip_address, int _secondhead) : tolerance(1.5), secondhead(_secondhead)
      {
      if (_ip_address == NULL)   // semaphore for no thermal head
         {
         ip_address[0] = 0;
         thermal_head_socket = INVALID_SOCKET;
         return;
         }
      strcpy(ip_address, _ip_address);
      Connect();
      }
   void Connect() {
      WSADATA wsaData;
      WSAStartup(MAKEWORD(2, 2), &wsaData);
      thermal_head_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      if (thermal_head_socket == INVALID_SOCKET) {
         int error = WSAGetLastError();
         FATAL_ERROR;
         }
      sockaddr_in clientService;
      clientService.sin_family = AF_INET;
      clientService.sin_addr.s_addr = inet_addr(ip_address);
      clientService.sin_port = htons(secondhead ? 5001 : 5000);
      if (0 != connect(thermal_head_socket, (SOCKADDR*)&clientService, sizeof(clientService)))
         {
         clientService.sin_port = htons(5002);
         if (0 != connect(thermal_head_socket, (SOCKADDR*)&clientService, sizeof(clientService))) {
            printf("I could not connect to the thermal head at address %s\n", ip_address);
            FATAL_ERROR;
            }
         }
      }
   void TurnOnOff(bool on)
      {
      TAtype ta("SetTemp");
      if (thermal_head_socket == INVALID_SOCKET) FATAL_ERROR;

      char command[64], response[64];
      printf("Turning %s the thermal head.\n",on?"on":"off");
      sprintf(command, secondhead ? "MB0033,%d":"MB0020,%d", on?0:1);
      int len = strlen(command);
      if (len != send(thermal_head_socket, command, len, 0)) {
         FATAL_ERROR; // couldn't send to thermal head for some reason
         }
      Sleep(100);
      len = recv(thermal_head_socket, response, sizeof(response), 0);
      if (len <= 0)
         FATAL_ERROR; // thermal head didn't respond
      response[len] = 0;
      if (strcmp(response, "OK") != 0) {
         printf("Thermal head responded to a MB0020/MB0033 command with %s\n", response);
         FATAL_ERROR;
         }
      }
   float ReadTemp()
      {
      TAtype ta("ReadTemp");
      if (thermal_head_socket == INVALID_SOCKET) FATAL_ERROR;

      char command[64], response[64];
      sprintf(command, secondhead ?"MI0023?":"MI0006?");
      int len = strlen(command);
      if (len != send(thermal_head_socket, command, len, 0)) {
         FATAL_ERROR; // couldn't send to thermal head for some reason
         }
      Sleep(100);
      len = recv(thermal_head_socket, response, sizeof(response), 0);
      if (len <= 0)
         FATAL_ERROR; // thermal head didn't respond
      response[len] = 0;
      if (!secondhead && strncmp(response, "MI6,", 4) != 0) {
         printf("Thermal head responded to a MI0006 command with %s\n", response);
         FATAL_ERROR;
         }
      if (secondhead && strncmp(response, "MI23,", 5) != 0) {
         printf("Thermal head responded to a MI0023 command with %s\n", response);
         FATAL_ERROR;
         }
      float temp = atof(response + (secondhead?5:4)) / 10.0;
      return temp;
      }
   void SetTemp(float temp, bool dontwait=false)
      {
      TAtype ta("SetTemp");
      int tries;
      if (thermal_head_socket == INVALID_SOCKET) FATAL_ERROR;

      char command[64], response[64];
      printf("Setting thermal head to %.1fC\n",temp);
      sprintf(command, secondhead ?"MI0701,%04d":"MI0699,%04d", (int)(temp * 10.0));
      for (tries = 10; tries >= 0;  tries--) {
         int len = strlen(command);
         if (len == send(thermal_head_socket, command, len, 0)) {
            Sleep(200);
            len = recv(thermal_head_socket, response, sizeof(response), 0);
            //         if (len <= 0)FATAL_ERROR; // thermal head didn't respond
            if (len > 0) {
               response[len] = 0;
               break;
               }
            }
         Sleep(2000);
         if (tries == 1) {
            printf("The thermal head isn't communicating. Resetting the socket!!!!!!\n");
            Connect();
            }
         }
      if (tries < 0) {
         printf("Thermal head isn't responding. Maybe try rebooting it\n");
         FATAL_ERROR;
         }
      if (strcmp(response, "OK") != 0) {
         printf("Thermal head responded to a MI0699/MI0701 command with %s\n", response);
         FATAL_ERROR;
         }
      if (dontwait) return;
      int count = 0;
      int four_in_a_row=0;
      while (true) {
         float f = ReadTemp();
         if (fabs(f - temp) <= tolerance)
            four_in_a_row++;
         else
            four_in_a_row = 0;
         if (four_in_a_row > 4) break;
         if (count >= 180) {
            printf("Still waiting for thermal head to acheive setpoint. There may be a problem.\n");
            if (count == 180) {
               TurnOnOff(false);
               Sleep(2000);
               TurnOnOff(true);
               }
            }
         printf("\ractual=%.1fC, setpoint=%.1fC elapsed time %dS       ", f, temp, count);
         count++;
         Sleep(900);
         }
      printf("\n");
      }
   };



#endif //__CHARACTERIZATION_H_INCLUDED__
