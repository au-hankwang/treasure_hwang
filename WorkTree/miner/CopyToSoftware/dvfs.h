#ifndef __DVFS_H_INCLUDED__
#define __DVFS_H_INCLUDED__

#ifndef __HELPER_H_INCLUDED__
using namespace std;
typedef unsigned int   uint32;
typedef unsigned short uint16;
typedef unsigned char  uint8;
#define MINIMUM(a,b) ( ((a)<(b)) ? (a) : (b))
#define MAXIMUM(a,b) ( ((a)>(b)) ? (a) : (b))
#define FATAL_ERROR DebugBreakFunc(__FILE__,__LINE__)
inline void DebugBreakFunc(const char* module, int line) {
   printf("\nFatal error in module %s line %d\n", module, line); 
   exit(-1);   // fixme
   }
#endif

#ifndef __MINER_H_INCLUDED__
enum commandtype {
   CMD_NOTHING = 0,
   CMD_WRITE = 1,
   CMD_READ = 2,	   // read a register
   CMD_READWRITE = 3
   };	   // read a register and after reading write it
enum {
   E_SUMMARY = 18,
   E_BIST_THRESHOLD = 22,
   E_BIST = 23,
   E_PLLCONFIG = 24,
   E_PLLFREQ = 25,
   E_PLLOPTION = 26,
   E_IPCFG = 28,
   E_TEMPCFG = 29,
   E_TEMPERATURE = 30,
   E_DVMCFG = 31,
   E_VOLTAGE = 32,
   E_THERMALTRIP = 35,
   E_MAX_TEMP_SEEN = 36,
   E_SPEED_DELAY = 39,
   E_SPEED_UPPER_BOUND = 40,
   E_SPEED_INCREMENT = 41,
   E_BIST_RESULTS = 64,
   E_DUTY_CYCLE = 104,
   E_CLOCK_RETARD = 106,
   };
#endif


class topologytype {
public:
   short board, row, col, id;
   float tempY, tempK;        // coefficients for the on die temp sensor
   float bistCoefficient;     // coefficient for hashclk vs bist speed(>1 means faster than bist)

   topologytype() {
      board = row = col = id = -1;
      tempY = 662.88;   // this is default coefficient for the IP. A single point calibration can improve upon this
      tempK = -287.48;
      bistCoefficient = 1.0;
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
      return raw_ip * 0.00010467773 - .285892339;
      }
   };

class systeminfotype {
public:
   float min_voltage, max_voltage, voltage_step;      // voltages in Volts
   float refclk, min_frequency, max_frequency;        // frequency in Mhz
   float max_junction_temp, thermal_trip_temp;                           // temp in Celcius
   int allowable_bad_engines;

   systeminfotype() {
      min_voltage = 11.5;
      max_voltage = 15.0;
      voltage_step = 0.025;
      refclk = 25.0;
      min_frequency = 500.0;
      max_frequency = 2500.0;
      max_junction_temp = 100.0;
      thermal_trip_temp = 120.0;
      allowable_bad_engines = 12;
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
   batchtype(short _board, short _id, short _addr) {
      board = _board;
      id = _id;
      addr = _addr;
      action = CMD_READ;
      }
   batchtype(short _board, short _id, short _addr, uint32 _data) {
      board = _board;
      id = _id;
      addr = _addr;
      data = _data;
      action = CMD_WRITE;
      }
   batchtype(int _addr, uint32 _data, commandtype _action, int _id = -1, uint32 _extra = 0) { board = 0; addr = _addr; data = _data; action = _action; extra = _extra; id = _id; }
   };



// this guys should derive a specific class to use serial port, usb, or software model for the communication

class dvfstype {
   vector<topologytype> topology;
   const systeminfotype systeminfo;
   vector<vector<int> > partitions;
   int num_boards, num_rows, num_cols;
   float voltage, current_THS;
   bool initial;

   void ProbeSubset(const vector<int>& subset, vector<int> &startf, vector<int> &maxf, vector<float> &temp);  // will return actual or theoretical THS
   float AdjustAll(float max_throughput, float& system_maxtemp, float& total_overtemp, int col=-1);  // will return actual or theoretical THS
   void SimpleOptimize();   // will return the theoretical THS at this operating point, if less than max_THS this will be the actual achieved
public:
   dvfstype(const vector<topologytype>& _topology, const systeminfotype &_systeminfo);
   void InitialSetup();      // this will setup the PLL's, turn on power switches 
   bool DVFS(float THS);     // returns true if performance could not be achieved
   float Optimize(float max_THS, bool& too_hot); // this function takes maximum TH/s and returns either the actual TH/s or the higher theoretical amount, doesn't adjust voltage
   void PrintSystem();        // This will printout the temp,voltage,frequency of each asic in the system


   virtual void ReadWriteConfig(vector<batchtype>& batch)=0;
   virtual void SetVoltage(float voltage) = 0;
   virtual void EnablePowerSwitch(int board) = 0;
   virtual void Delay(int milliSeconds) = 0;
   };



#endif //__DVFS_H_INCLUDED__


