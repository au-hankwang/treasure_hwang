#ifndef __DVFS_H_INCLUDED__
#define __DVFS_H_INCLUDED__


struct response_hittype;

enum commandtype {
   CMD_NOTHING = 0,
   CMD_WRITE = 1,
   CMD_READ = 2,	   // read a register
   CMD_READWRITE = 3,	   // read a register and after reading write it
   CMD_LOAD0 = 4,	   // this will load a copy into chunk1 copy0,1,2,3
   CMD_LOAD1 = 5,	   // this will load a copy into chunk1 copy1 only
   CMD_LOAD2 = 6,	   // this will load a copy into chunk1 copy2 only
   CMD_LOAD3 = 7,	   // this will load a copy into chunk1 copy3 only

   CMD_ARP = 12, // inquire the presence of asic based on 64b unique identifier
   CMD_ARP_DEBUG1 = 13, // inquire the presence of asic based on debug1 toggling
   CMD_ARP_DEBUG2 = 14, // inquire the presence of asic based on debug2 toggling

   CMD_RETURNHIT = 0x40,   // If bit 6 is set of command word, miner will respond with most recent hit info. Don't use with CMD_BROADCAST
   CMD_BROADCAST = 0x80,   // If bit 7 is set of command word, id will be ignored and all miners will accept the command
   };

class topologytype {
public:
   short board, row, col, x, y, id; // x,y isn't needed for dvfs, just documentation for phyical location on board
   float tempY, tempK;        // coefficients for the on die temp sensor
   float voltageGain, voltageOffset;
   float bistCoefficient;     // coefficient for hashclk vs bist speed(>1 means faster than bist)

   float temperature, frequency, voltage, bisthitrate, missionhitrate, phase2;        // frequency in Mhz

   float frequency2, bisthitrate2, missionhitrate2;
   unsigned partition[8];  // engine mask for the pll0

   static float defaultVoltageGain, defaultVoltageOffset;

   topologytype() {
      board = row = col = id = -1;
      x = y = 0;
      tempY = 817.01;   // this is default coefficient for the IP. A single point calibration can improve upon this
      tempK = -302.14;
      voltageGain = defaultVoltageGain;
      voltageOffset = defaultVoltageOffset;
      bistCoefficient = 1.0;
      bisthitrate = 0.0;
      bisthitrate2 = 0.0;
      frequency2 = 0.0;
      frequency = 0.0;
      missionhitrate = 0.0;
      missionhitrate2 = 0.0;
      memset(partition, 0, sizeof(partition));
      phase2 = 0.0;
      }
   topologytype(int _row, int _col, int _id) {
      board = 0;
      row = _row;
      col = _col;
      id = _id;
      x = y = 0;
      tempY = 817.01;   // this is default coefficient for the IP. A single point calibration can improve upon this
      tempK = -302.14;
      voltageGain = defaultVoltageGain;
      voltageOffset = defaultVoltageOffset;
      bistCoefficient = 1.0;
      bisthitrate = 0.0;
      bisthitrate2 = 0.0;
      frequency2 = 0.0;
      frequency = 0.0;
      missionhitrate = 0.0;
      missionhitrate2 = 0.0;
      memset(partition, 0, sizeof(partition));
      phase2 = 0.0;
      }
   bool operator<(const topologytype & right) const
      {
      if (board < right.board) return true;
      if (board > right.board) return false;
      if (row < right.row) return true;
      if (row > right.row) return false;
      return col < right.col;
      }
   float Temp(int raw_ip) const {
      raw_ip = raw_ip & 0xffff;
      return (raw_ip -0.5) * tempY * (1.0 / 4096.0) + tempK;
      }
   int InverseTemp(float temp) const { // computes the raw_ip value
      return (int)((temp - tempK) / tempY * 4096) & 0xffff;
      }
   float Voltage(int raw_ip) const {
      raw_ip = raw_ip & 0xffff;
//      return raw_ip * 0.0001011035 - .276029;
      return raw_ip * voltageGain + voltageOffset;
      }
   void ChangeDefaultVoltageParameters(float gain, float offset)
      {
      defaultVoltageGain *= gain;
      defaultVoltageOffset = (defaultVoltageOffset+offset)*gain;
      }
   void ChangeVoltageParameters(float gain, float offset)
      {
      voltageGain *= gain;
      voltageOffset = (voltageOffset + offset) * gain;
      }
   };

class systeminfotype {
public:
   float min_voltage, max_voltage, voltage_step, max_power;      // voltages in Volts, power in W
   float refclk, min_frequency, max_frequency;        // frequency in Mhz
   float max_junction_temp, thermal_trip_temp, optimal_temp;             // temp in Celcius
   float min_fan_percentage;

   systeminfotype() {
      min_voltage = 11.0;
      max_voltage = 14.0;
      voltage_step = 0.005;
//      max_power = 5500.0;
      max_power = 5100.0;
      refclk = 25.0;
      min_frequency = 200.0;
      max_frequency = 2000.0;
      max_junction_temp = 110.0;
      thermal_trip_temp = 120.0;
      optimal_temp = 55.0;
      min_fan_percentage = 0.20;
      }
   };


class batchtype {
public:
   commandtype action;
   short board, id, addr;  // Id=-1 indicates a broadcast command
   uint32 data, extra;
   batchtype() {
      board = 0; addr = 0; data = 0; action = CMD_NOTHING; id = -1; extra = 0;
      }
   batchtype &Write(short _board, short _id, short _addr, uint32 _data) {
      board = _board;
      id = _id;
      addr = _addr;
      data = _data;
      action = CMD_WRITE;
      return *this;
      }
   batchtype& Read(short _board, short _id, short _addr) {
      board = _board;
      id = _id;
      addr = _addr;
      data = 0;
      action = CMD_READ;
      return *this;
      }
   batchtype& ReadWrite(short _board, short _id, short _addr, uint32 _data) {
      board = _board;
      id = _id;
      addr = _addr;
      data = _data;
      action = CMD_READWRITE;
      return *this;
      }
   };

struct speedsorttype {
   int engine;
   float speed;
   speedsorttype(int _engine, float _speed) : engine(_engine), speed(_speed)
      {}
   bool operator<(const speedsorttype& right) const
      {
      return speed < right.speed;
      }
   };


// this guys should derive a specific class to use serial port, usb, or software model for the communication

class dvfstype {
protected:
   vector<topologytype> topology;
   systeminfotype systeminfo;
   vector<int> boards;
   vector<int> problem_asics; // will be index in topology
   int num_rows, num_cols;
   float fan_integral, fan_percentage;
   float voltage, last_optimization_temp;
   float pll_multiplier;
   bool initial;

   void CalibrateDVM();
   void Partition(int experiment);
   void PerEngineTweak2(int experiment);
   void PerEngineTweak(int experiment);
   void Optimize(float average_f, bool& too_hot, int experiment); // this function takes average_f and returns the hitrate(0-1.0), doesn't adjust voltage
   void Optimize2(float average_f, bool& too_hot, int experiment); // this function takes average_f and returns the hitrate(0-1.0), doesn't adjust voltage
   void SettingSweep(float average_f, bool& too_hot, int experiment);
   void ReduceTemp(float avg_temp, bool eco);
public:
   dvfstype(const vector<topologytype>& _topology, const systeminfotype &_systeminfo);
   void DescribeSystem(const vector<topologytype>& _topology, const systeminfotype& _systeminfo);
   void InitialSetup();      // this will setup the PLL's, turn on power switches 
   bool DVFS(const float THS, int experiment, bool verbose=false);     // returns true if performance could not be achieved
   void MinPower();
   void PrintSystem(FILE *fptr=NULL, bool temp_only=false);        // This will printout the temp,voltage,frequency of each asic in the system
   float MeasureHashRate(int period, bool &too_hot, float& avg_temp, float& max_temp, int experiment=0);
   float MeasureHashRate2(int period, bool& too_hot, float& avg_temp, float& max_temp, int experiment = 0);
   float MeasureHashRateHybrid(int period, bool& too_hot, float& avg_temp, float& max_temp, int experiment = 0);
   void FanManage(float avg_temp, float max_temp);
   void PrintSummary(FILE *fptr, const char* label, float& efficiency);
   void Mine();


   virtual bool GetHit(response_hittype& rsppacket, int b, int id = -1)=0;
   virtual void ReadWriteConfig(vector<batchtype>& batch)=0;
   virtual void SetVoltage(float voltage) = 0;
   virtual void EnablePowerSwitch(int board) = 0;
   virtual void DisableSystem() = 0;
   virtual void Delay(int milliSeconds) = 0;
   virtual float ReadPower() = 0;
   virtual void FanControl(float percentage)=0;
   virtual void Foo() = 0;
   };



#endif //__DVFS_H_INCLUDED__


