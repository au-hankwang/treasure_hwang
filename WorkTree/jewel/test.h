#ifndef __TEST_H_INCLUDED__
#define __TEST_H_INCLUDED__
#include "pch.h"
#include "helper.h"
#include "miner.h"
#include "dvfs.h"
#include "serial.h"
#include "usb.h"

//#pragma optimize("", off)


struct datatype;
class thermalheadtype;
void PrintMinerMap(short* bist, FILE* fptr);

enum autohiterrortype {
   E_NOTHING, E_TIMEOUT, E_CRC, E_NOUNIQUE
   };

enum arp_resonse_codestype {
   E_ARP_GOOD, E_ARP_TIMEOUT, E_ARP_COLLISION
   }; 

struct testpointtype {
   float temp, io, scv, core;
   };



class testtype : public dvfstype {
public:
   serialtype serial;
   usbtype usb;
   boardmodeltype boardmodel;
   int current_board;
   int expected_baud;
   float refclk;
   float &pll_multiplier;
   int eval_id;
   bool VmonNotConfiged;
   bool zareen;
   const int method; // 0=model, 1=serial, 2=usb
   vector<int> found;
   vector <response_hittype> hitbuffer;
   vector<headertype> block_headers;
public:

   testtype(int _method, const vector<topologytype>& _topology = vector<topologytype>(), const systeminfotype& _systeminfo = systeminfotype()) : method(_method), dvfstype(_topology, _systeminfo),pll_multiplier(dvfstype::pll_multiplier) {
      if (method == 0);
      //      else if (metthod == 1) serial.Init("\\\\.\\COM13");
      else if (method == 1) serial.Init("\\\\.\\COM13");
      else if (method == 2) usb.Init();
      else FATAL_ERROR;
      expected_baud = 115200;
      refclk = 25000000.0;
      pll_multiplier = 0.0;
      eval_id = 0;
      VmonNotConfiged = true;
      current_board = 0;
      zareen = false;
      }
   // these 4 virtual functions are for dvfstype
   void ReadWriteConfig(vector<batchtype>& batch)
      {
      BatchConfig(batch);
      }
   void SetVoltage(float voltage) {
      if (method == 0)
         WriteConfig(255, voltage * 1000.0, -1);   // write to fake address to control power supply in presilicon testing
      else if (method == 2)
         usb.SetVoltage(S_CORE, voltage * 1000.0);
      else FATAL_ERROR;
      }
   void SetBaudRate(int baud) {
      if (method == 1) serial.SetBaudRate(baud);
      else if (method == 2) usb.SetBaudRate(baud);
      }

   void EnablePowerSwitch(int switch_mask) 
      {
      if (method == 2)
         usb.VddEnable(switch_mask);
      else FATAL_ERROR;
      }
   void DisableSystem() {
      if (method == 2)
         usb.VddEnable(0);
      else FATAL_ERROR;
      exit(-1);
      }
   void Foo() {
      CommunicationTest();
      }

   void Delay(int milliSeconds)
      {
      TAtype ta("Delay");
      Sleep(milliSeconds);
      }
   float ReadPower()
      {
      if (method == 0)
         return ReadConfig(255, 0) * 0.001;
      else if (method == 2)
         return usb.ReadPower();
      else FATAL_ERROR;
      return -1.0;
      }
   void FanControl(float percentage)
      {
      int x = 256 * percentage;
      x = MINIMUM(x, 255);
      x = MAXIMUM(x, 0);
      usb.SetFanSpeed(x);
      }

   bool BatchConfig(vector<batchtype>& batch, bool quiet=false);
   void SetBoard(int b)
      {
      current_board = b;
      usb.SetBoard(b);
      }

   uint32 ReadConfig(int addr, int id = -1) {
      vector<batchtype> batch;
      batch.push_back(batchtype().Read(current_board, id, addr));
      BatchConfig(batch);
      return batch[0].data;
      }
   uint32 ReadWriteConfig(int addr, int data, int id = -1) {
      vector<batchtype> batch;
      batch.push_back(batchtype().ReadWrite(current_board, id, addr, data));
      batch[0].action = CMD_READWRITE;
      BatchConfig(batch);
      return batch[0].data;
      }
   void WriteConfig(int addr, int data, int id = -1) {
      vector<batchtype> batch;
      batch.push_back(batchtype().Write(current_board, id, addr, data));
      BatchConfig(batch);
      return;
      }
   bool Load(const headertype& header, response_hittype& rsppacket, int ctx, int b, int id = -1);
   void Load(const headertype& header, int sequence, int difficulty, int ctx);
   arp_resonse_codestype Arp(unsigned __int64 addr, unsigned __int64 mask, unsigned __int64& unique_addr, short id);
   arp_resonse_codestype Arp_Debug1(unsigned __int64& unique_addr, short id);
   bool GetHit(response_hittype& rsppacket, int b, int id = -1);
   bool GetHits(vector<response_hittype>& rsppackets, const vector<int>& ids);
   autohiterrortype GetHitAuto(response_hittype& rsppacket);
   bool IsAlive(int id);
   void SetBaud(int baud);
   void DrainReceiver() { if (method == 1) serial.DrainReceiver(); else if (method == 2) usb.DrainReceiver(); }
   void RegisterDump();
   bool FrequencyEstimate(float expected_hashclk = 0.0);
   bool ReadWriteStress(int id, int iterations = -1); // return true on error
   void BaudSweep(int id);
   void BaudTolerance(int id);
   bool BoardEnumerate();
   void Discovery();
   float RunBist(int id);
   void MineTest();
   void SingleTest(const headertype &h);
   void FakeMineTest();
   void FakeMineTestAuto();
   void MineTest_Auto();
   bool HeaderTest(int context, vector<int>& hits);
   void ReadHeaders(const char* filename);
   void MineTestStatistics(int seconds, vector<int>& hits, vector<int>& truehits);
   void NullHeaderTest(const vector<headertype>& headers, int context);
   void Pll(float f_in_mhz, int = -1);
   float OnDieVoltage(int id=-1, supplytype supply=S_CORE);
   float OnDieTemp(int id=-1);
   float SpeedSearch(float start, float end, int id = -1, float threshold=0);
   void SpeedEnumerate(float start, float end, int id);
   void Paredo(const char* filename);
   void BistParedo();
   bool TestPoint(testpointtype& tp, const char* partname, int station, gpiotype& gpio, bool abbreviated, bool turbo);  // returns true for reject
   bool ControlTest(datatype& data);
   bool PllTest(datatype& data);
   void PllData();
   bool IpTest(datatype& data);
   bool PinTest(datatype& data, gpiotype &gpio);
   bool BistTest(datatype& data, bool quick=false);
   bool Cdyn(datatype& data, bool abbreviated=false, bool turbo=false);
   void BistSweep(datatype& data);
   bool SpeedTest(datatype& data, const char *filename, const char* filename2=NULL);
   void IV_Sweep();
   void Shmoo(const char *filename, float temp);
   void Shmoo_Setting(const char* filename, float temp);
   void Asic3();
   void HashSystem();
   void QuickDVFS(float supply);
   void ReadAllVoltages(const topologytype& t);
   void BistExperiment();
   
   float ReportEfficiency(float supply, bool quiet=false);
   void Characterization(bool abbreviated, bool shmoo, bool turbo);
   void DavidExperiment();
   void ChipMetric(datatype& data, bool abreviated, bool turbo);
   void Simple();
   void SimpleSystem();
   float MinMaxTemp();
   void MinerTweak();
   void MinerTweak2();
   void SystemStatistics(const char* label);
   void CalibrateEvalDVM();
   void IRTest(int b);
   void PllExperiment(int b);
   void PerformanceSweep(int b, const char* label, float experiment);
   void CommunicationTest();
   void CommTest();
   };

#endif //__TEST_H_INCLUDED__
