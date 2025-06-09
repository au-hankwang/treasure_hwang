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
struct metrictype {
   float throughput;
   int phase;
   float perf50, perf98, hit50, hit98, peakV, peakI, peakH, peakE, temp;
   float spare;
   };
struct tweaktype {
   float voltage;
   unsigned phase2_map[8];
   unsigned pll2_map[8];
   float pll_ratio;
   float pll2_ratio; // this will be the fast pll
   float b2_freq, b1_freq;
   };


struct datatype {
   char unique[4];   // 'Dac!'
   int sizeof_this_structure;
   time_t seconds;
   char partname[32];
   short station;
   float temp;

   float scv_current_0Mhz, scv_current_25Mhz, scv_current_100Mhz; // hashclk=10Mhz
   float scv_current_1Ghz, scv_current_2Ghz;                      // refclk=25Mhz

   float pll0_min[3], pll0_max[3], pll1_min[3], pll1_max[3];    // min and max lock ranges for scv 0.65, 0.75, 0.85
   float pll_scv_current[5];         // 0,1,2,4,8 Ghz

   unsigned short ip_temp;
   unsigned int ip_dro[4][32];         // bit 31=is error, [260mV, 290mV, 320mV, 350mV]
   float ip_dro_current[4][32];
   unsigned short ip_voltage_25[18], ip_voltage_1000[18], ip_voltage_2000[18];

   metrictype metric[8];

   float iddq, iddq_temp;

   float vih, vil, iih, iil[10];  // iil is +50,0,-50,-100,-150,-200,-250,-300,-350,-400
   float engine_map[238], engine_map2[238];
   float spare2[64];

   datatype() {
      memset(this, 0, sizeof(datatype));
      sizeof_this_structure = sizeof(datatype);
      memcpy(unique, "Dac!", sizeof(unique));
      }
   void WriteToCSV(FILE* fptr, bool label=false) {
      int i,k ;
      if (label) {
         fprintf(fptr, "sizeof,station,time,partname,temp");
         fprintf(fptr, ",iddq,iddq_temp,iddq_normalized");
         for (i = 0; i < 2; i++) {
            fprintf(fptr, ",metric[%.0f/phase%d].Vmin", metric[i].throughput, metric[i].phase);
            fprintf(fptr, ",metric[%.0f/phase%d].Imin", metric[i].throughput, metric[i].phase);
            fprintf(fptr, ",metric[%.0f/phase%d].Hmin", metric[i].throughput, metric[i].phase);
            fprintf(fptr, ",metric[%.0f/phase%d].Emin", metric[i].throughput, metric[i].phase);
            fprintf(fptr, ",metric[%.0f/phase%d].Temp", metric[i].throughput, metric[i].phase);
            }
         for (i = 6; i < 8; i++) {
            fprintf(fptr, ",Missionmetric[%.0f/phase%d].Vmin", metric[i].throughput, metric[i].phase);
            fprintf(fptr, ",Missionmetric[%.0f/phase%d].Imin", metric[i].throughput, metric[i].phase);
            fprintf(fptr, ",Missionmetric[%.0f/phase%d].Hmin", metric[i].throughput, metric[i].phase);
            fprintf(fptr, ",Missionmetric[%.0f/phase%d].Emin", metric[i].throughput, metric[i].phase);
            fprintf(fptr, ",Missionmetric[%.0f/phase%d].Temp", metric[i].throughput, metric[i].phase);
            }

         fprintf(fptr, ",scv_current_25mhz");
         fprintf(fptr, ",scv_current_1ghz,scv_current_2ghz");
         for (i = 0; i < 3; i++) fprintf(fptr, ",pll0_min[%d]", i);
         for (i = 0; i < 3; i++) fprintf(fptr, ",pll0_max[%d]", i);
         for (i = 0; i < 3; i++) fprintf(fptr, ",pll1_min[%d]", i);
         for (i = 0; i < 3; i++) fprintf(fptr, ",pll1_max[%d]", i);
         for (i = 0; i < 5; i++) fprintf(fptr, ",pll_scv_current[%dGhz]", i == 0 ? 0 : i == 1 ? 1 : i == 2 ? 2 : i == 3 ? 4 : 8);
         fprintf(fptr, ",ip_temp");
         for (k = 0; k < 4; k++) {
            for (i = 24; i < 32; i++) {
               const char* label = i == 24 ? "EInv" : i == 25 ? "ENor3" : i == 26 ? "ENand3" : i == 27 ? "ECsa" : i == 28 ? "UInv" : i == 29 ? "UNor3" : i == 30 ? "UNand3" : i == 31 ? "UCsa" : "";
               fprintf(fptr, ",dro[%dmV]%s", k==0?260: k == 1 ? 290 : k == 2 ? 320 : 350, label);
               }
            }
         for (k = 0; k < 4; k++) {
            fprintf(fptr, ",dro_current_reference[%dmV]", k == 0 ? 260 : k == 1 ? 290 : k == 2 ? 320 : 350);
            for (i = 24; i < 32; i++) {
               const char* label = i == 24 ? "EInv" : i == 25 ? "ENor3" : i == 26 ? "ENand3" : i == 27 ? "ECsa" : i == 28 ? "UInv" : i == 29 ? "UNor3" : i == 30 ? "UNand3" : i == 31 ? "UCsa" : "";
               fprintf(fptr, ",dro_current[%dmV]%s", k == 0 ? 260 : k == 1 ? 290 : k == 2 ? 320 : 350, label);
               }
            }

         for (i = 0; i < 18; i++) fprintf(fptr, ",dvm_25[%d]", i);
         for (i = 0; i < 18; i++) fprintf(fptr, ",dvm_1000[%d]", i);

         for (i = 0; i < 238; i++) fprintf(fptr, ",enginemap[%d]", i);

//         fprintf(fptr, ",vih,vil,iih");
//         for (i = 0; i < 10; i++) fprintf(fptr, ",iil[%d]", 50 - i * 50);

         fprintf(fptr, "\n");
         return;
         }
      fprintf(fptr, "%d,%d,%d,%s,%.0f",sizeof_this_structure,station,(int)seconds,partname,temp);
      fprintf(fptr, ",%.2f,%.1f,%.2f",iddq,iddq_temp, iddq * exp2((85 - iddq_temp) / 23.0));
      for (i = 0; i < 2; i++) {
         fprintf(fptr, ",%.3f", metric[i].peakV);
         fprintf(fptr, ",%.2f", metric[i].peakI);
         fprintf(fptr, ",%.2f", metric[i].peakH);
         fprintf(fptr, ",%.2f", metric[i].peakE);
         fprintf(fptr, ",%.2f", metric[i].temp);
         }
      for (i = 6; i < 8; i++) {
         fprintf(fptr, ",%.3f", metric[i].peakV);
         fprintf(fptr, ",%.2f", metric[i].peakI);
         fprintf(fptr, ",%.2f", metric[i].peakH);
         fprintf(fptr, ",%.2f", metric[i].peakE);
         fprintf(fptr, ",%.2f", metric[i].temp);
         }

      fprintf(fptr, ",%0.3e", scv_current_25Mhz);
      fprintf(fptr, ",%0.3e,%0.3e", scv_current_1Ghz, scv_current_2Ghz);
      fprintf(fptr, ",%0.0f,%0.0f,%0.0f", pll0_min[0] * 1.0e-6, pll0_min[1] * 1.0e-6, pll0_min[2] * 1.0e-6);
      fprintf(fptr, ",%0.0f,%0.0f,%0.0f", pll0_max[0] * 1.0e-6, pll0_max[1] * 1.0e-6, pll0_max[2] * 1.0e-6);
      fprintf(fptr, ",%0.0f,%0.0f,%0.0f", pll1_min[0] * 1.0e-6, pll1_min[1] * 1.0e-6, pll1_min[2] * 1.0e-6);
      fprintf(fptr, ",%0.0f,%0.0f,%0.0f", pll1_max[0] * 1.0e-6, pll1_max[1] * 1.0e-6, pll1_max[2] * 1.0e-6);
      for (i = 0; i < 5; i++) fprintf(fptr, ",%0.3e", pll_scv_current[i]);
      fprintf(fptr, ",%d", ip_temp);
      for (k = 0; k < 4; k++) {
         for (i = 24; i < 32; i++) fprintf(fptr, ",%d", ip_dro[k][i]&0xfff);
         }
      for (k = 0; k < 4; k++) {
         fprintf(fptr, ",%.3e", ip_dro_current[k][23]);
         for (i = 24; i < 32; i++)
            fprintf(fptr, ",%.3e", ip_dro_current[k][i]);
         }

      for (i = 0; i < 18; i++) fprintf(fptr, ",%d", ip_voltage_25[i]);
      for (i = 0; i < 18; i++) fprintf(fptr, ",%d", ip_voltage_1000[i]);
      for (i = 0; i < 238; i++) fprintf(fptr, ",%.0f", engine_map[i]);

//      fprintf(fptr, ",%0.3e,%0.3e,%0.3e", vih, vil, iih);
//      for (i = 0; i < 10; i++) fprintf(fptr, ",%0.3e", iil[i]);
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
