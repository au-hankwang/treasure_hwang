#ifndef __ELECTRICAL_H_INCLUDED__
#define __ELECTRICAL_H_INCLUDED__

void System_DVFS_Sim();
void Electrical_Sim();


const float num_boards = 1;
const float fan_power = 200.0;   // real number is 205 but that's with ps inefficency
const float ps_efficiency = 0.965;
const float ir_resistance = 0.00025;  // Ohm/chip
const float air_conduction = .017;  // C/W
//const float hashmul = 202 * 4 / 1.0 / 1.0e6;
const float hashmul = 238 * 4 / 3.0 / 1.0e6;
const float ref_frequency = 1800;	   // Mhz
const float ref_efficiency = 14.6;     // J/TH
const float ref_voltage = .260;	       // Volts
const float ref_temperature = 60;	   // Celcius
const float ref_leakage_percentage = .20;	       // 0-1.0
const float ref_current = hashmul * ref_efficiency * ref_frequency / ref_voltage;
const float ref_leakage = ref_current * ref_leakage_percentage;
const float ref_dynamic = ref_current * (1-ref_leakage_percentage);
const float leakage_half = 23.0;	   // Celcius
const float ref_vt = .18;	           // Volts
const float vt_temp = -0.00045;         // V/C
const float perf_half_up = 300;	       // Celcius
const float perf_half_down = 200;	   // Celcius
//const float perf_half_up = 100;	       // Celcius
//const float perf_half_down = 95;	   // Celcius
//const float hashmul = 253 * 4 / 3.0 / 1.0e6;
const float min_frequency = 400;
const float max_junction = 110;


class curvepointtype {
public:
   float frequency, hit, current, odv;
   curvepointtype() {
      frequency = hit = current = odv = 0.0;
      }
   };

class curvetype {
public:
   float temp, voltage;
   float mean, stddev;  // Mhz
   float cdyn, iddq;    // A/Mhz, A
   vector<curvepointtype> points;
   curvetype()
      {
      temp = voltage = mean = stddev = 0.0;
      }
   float HitExtrapolate(float f, float _mean, float _stddev) const {
      double x = (_mean - f) / _stddev;
      // this should be the cumulative normal distribution function. I googled and got this answer, no idea what erfc actually does
      double cndf = std::erfc(-x / std::sqrt(2)) / 2;
      return cndf;
      }
   float CurrentExtrapolate(float f, float _cdyn, float _iddq) const {
      double x = f * _cdyn * voltage * 0.001 + _iddq;
      return x;
      }

   float Current(int f) const
      {
      int i;
      for (i = 0; i < points.size() - 1; i++)
         {
         if (points[i + 1].frequency <= f && points[i].frequency >= f)
            return (f - points[i + 1].frequency) * (points[i].current - points[i + 1].current) / (points[i].frequency - points[i + 1].frequency) + points[i + 1].current;
         }
      if (f > points[0].frequency)
         return points[0].current * (f / points[0].frequency);
      if (f < points.back().frequency)
         return points.back().current * (f / points.back().frequency);
      FATAL_ERROR;
      return 0.0;
      }
   float HitError() const
      {
      int i;
      float e = 0.0;
      bool allzeroes = true;
      for (i = 0; i < points.size(); i++) {
         allzeroes = allzeroes && points[i].hit < 0.4;
//         if (points[i].hit >= 0.1 && points[i].hit <= 0.98)
         e += fabs(points[i].hit - HitExtrapolate(points[i].frequency, mean, stddev));
         }
      if (allzeroes) e = mean;
      return e;
      }
   float CurrentError() const
      {
      int i;
      float e = 0.0;
      for (i = 0; i < points.size(); i++) {
         if (points[i].hit>0.0 ||points[i].frequency<=600)
            e += fabs(points[i].current - CurrentExtrapolate(points[i].frequency, cdyn, iddq));
         }
      return e;
      }
   void Fit();
   };

class chiptype {
public:
   char label[256];
   vector<curvetype> curves;
   bool fake_curve;

public:
   chiptype(const char* filename)
      {
      strcpy(label, filename);
      ReadCsv(filename);
      fake_curve = false;
      }
   chiptype()
      {
      fake_curve = true;
      }
   void Performance(float& pbest, float& p100, float& p140, float& p180);
   float Minimum(float f, float cost, float& v, float& temp);  // will return minimum efficiency
   float MinimumForceT(float f, float cost, float& v, float force_t);  // will return minimum efficiency
   void FakeInterpolate(float& hit, float& current, float v, float t, float f) const
      {
      float stddev = 150;
      float scale = (ref_voltage - ref_vt)* (ref_voltage - ref_vt) / ref_voltage;
      float new_vt = ref_vt + vt_temp * (t - ref_temperature);
      float timing = ref_frequency * (v - new_vt) * (v - new_vt) / v / scale;
      float mean = timing + stddev*1.5;
      float dynamic = ref_dynamic * v * v / (ref_voltage * ref_voltage) * f / ref_frequency;
      float leakage = ref_leakage * v / ref_voltage * pow(2, (t-ref_temperature)/leakage_half);
      current = dynamic + leakage;
      hit = curvetype().HitExtrapolate(f, mean, stddev);
      }

   void Interpolate(float &hit, float &current, float v, float t, float f) const
      {
      TAtype ta("chiptype::HitInterpolate()");
      int i, left = -1, right = -1;
      if (fake_curve)
         return FakeInterpolate(hit, current, v, t, f);

      v *= 1000.0; // curves are stored in mV

      // left will be the curve with less voltage and less temp than actual
      
      t = MAXIMUM(t, curves[0].temp);
      for (i = 0; i < curves.size() - 1; i++)
         {
         if (curves[i].voltage <= v && curves[i].temp <= t)
            left = i;
         if (curves[i].voltage >= v && curves[i].temp >= t && right<0)
            right = i;
         }
      if (left < 0) {
         for (i = 0; i < curves.size() - 1; i++)
            {
            if (curves[i].voltage <= v && left<0)
               left = i;
            }
         }
      if (left == 0 && right == 0) left = -1;
      if (left < 0 && right >= 0) { // the requested point is off the curve so interpolate based on 
         hit = 0.0;
         current = curves[right].Current(f);
         current = current * v / curves[right].voltage;
//         printf("0) V=%.0f current=%.0f\n",v,current);
         return;
         }
      if (left >=0 && right < 0) { // the requested point is off the curve so interpolate based on 
         hit = 1.0;
         current = curves[left].Current(f);
         current = current * v / curves[left].voltage;
//         printf("1) V=%.0f current=%.0f\n", v, current);
         return;
         }
      if (left < 0 || right < 0) FATAL_ERROR;
      if (curves[left].temp != curves[left + 1].temp) FATAL_ERROR;
      if (curves[left].voltage >= curves[left + 1].voltage) FATAL_ERROR;
      if (curves[right-1].temp != curves[right].temp) FATAL_ERROR;
      if (curves[right-1].voltage >= curves[right].voltage) FATAL_ERROR;

      float lmean = (v == curves[left].voltage) ? curves[left].mean : (v - curves[left].voltage) * (curves[left + 1].mean - curves[left].mean) / (curves[left + 1].voltage - curves[left].voltage) + curves[left].mean;
      float lstddev = (v == curves[left].voltage) ? curves[left].stddev : (v - curves[left].voltage) * (curves[left + 1].stddev - curves[left].stddev) / (curves[left + 1].voltage - curves[left].voltage) + curves[left].stddev;
      float rmean = (v == curves[right - 1].voltage) ? curves[right - 1].mean : (v - curves[right - 1].voltage) * (curves[right].mean - curves[right - 1].mean) / (curves[right].voltage - curves[right - 1].voltage) + curves[right - 1].mean;
      float rstddev = (v == curves[right - 1].voltage) ? curves[right - 1].stddev : (v - curves[right - 1].voltage) * (curves[right].stddev - curves[right - 1].stddev) / (curves[right].voltage - curves[right - 1].voltage) + curves[right - 1].stddev;
      float lc = (v == curves[left].voltage) ? curves[left].Current(f) : (v - curves[left].voltage) * (curves[left + 1].Current(f) - curves[left].Current(f)) / (curves[left + 1].voltage - curves[left].voltage) + curves[left].Current(f);
      float rc = (v == curves[right-1].voltage) ? curves[right-1].Current(f) : (v - curves[right - 1].voltage) * (curves[right].Current(f) - curves[right - 1].Current(f)) / (curves[right].voltage - curves[right - 1].voltage) + curves[right - 1].Current(f);

      float mean = (t == curves[left].temp || curves[left].temp== curves[right].temp) ? lmean : (t - curves[left].temp) * (rmean - lmean) / (curves[right].temp - curves[left].temp) + lmean;
      float stddev = (t == curves[left].temp || curves[left].temp == curves[right].temp) ? lstddev : (t - curves[left].temp) * (rstddev - lstddev) / (curves[right].temp - curves[left].temp) + lstddev;
      current = (t == curves[left].temp || curves[left].temp == curves[right].temp) ? lc : (t - curves[left].temp) * (rc - lc) / (curves[right].temp - curves[left].temp) + lc;

      // use a fixed stdev of 90Mhz. It's conservative and gets rid of some noise from the stdev estimation
      hit = curvetype().HitExtrapolate(f, mean, 90.0);
//      hit = curvetype().HitExtrapolate(f, mean, stddev);
//      printf("2) V=%.0f current=%.1f\n", v, current);
      if (current > 0.0);
      else FATAL_ERROR;
      }
   void ReadCsv(const char* filename);
   };




class electricaltype {
public:
   float vt, leakage;
   float frequency, voltage, case_temp, junction_temp, theta_j, hitrate;

   chiptype chip;

   // these are for the dvfs model to respond to the controller   
   float pll_multiplier, pll_increment, pll_max;
   bool bist_fail;
   int random;

   electricaltype(const chiptype &_chip) : chip(_chip) {
      vt = ref_vt;
      leakage = ref_leakage;
      frequency = min_frequency;
      case_temp = 85;
      junction_temp = 60;
      theta_j = 3;
      }
   float PerfFactor(float v, float vt) const {
      return (v - vt) * (v - vt) / v;
      }
   float MaxFrequency(float v, float t) const {
      if (voltage <= vt) return 50;
      float templimited = ref_frequency * PerfFactor(voltage, vt + vt_temp * (125 - ref_temperature)) / PerfFactor(ref_voltage, ref_vt);
      float maxf        = ref_frequency * PerfFactor(voltage, vt + vt_temp * (t   - ref_temperature)) / PerfFactor(ref_voltage, ref_vt);
         
      if (t >= max_junction) templimited = 25.0;
      if (v <= vt) return 25.0;
      return MINIMUM(maxf, templimited);
      }
   float MaxFrequency() const {
      return MaxFrequency(voltage, junction_temp);
      }
/*   float Conductance() const { // return conductance in Mhos
      float ref_power = ref_efficiency * hashmul * ref_frequency;
      float power_minus_leakage = ref_power - ref_leakage * ref_voltage;
      float leakconductance = (leakage / ref_voltage) * exp2((junction_temp - ref_temperature) / leakage_half);
//      float leakconductance = (leakage / ref_voltage);
      float dynamicconductance = (frequency / ref_frequency) * (power_minus_leakage / ref_voltage / ref_voltage);
      return leakconductance + dynamicconductance;
      }*/
   float Conductance()
      {
      float current;
      chip.Interpolate(hitrate, current, voltage, junction_temp, frequency);
      return current / voltage;
      }
   float Power() { // return conductance in W
      float current = voltage * Conductance();
      return voltage * current + current*current*ir_resistance;
      }
   bool operator<(const electricaltype& right) const
      {
      return random < right.random;
      }
   };

enum methodtype {
   CONSTANT_FREQUENCY,
   CONSTANT_CONDUCTANCE,
   BEST_PERFORMANCE,
   OTHER,
   };

class scenariotype {
public:
   vector<electricaltype> asics;
   systeminfotype systeminfo;
   int xref[256];
   bool runaway;

public:
   int rows, cols, boards;
   float supply, current, reference_clock, ambient;
   vector<float> outlet_temps;

   scenariotype(const systeminfotype& sinfo, int _boards, int _rows, int _cols, const vector<chiptype> &chips, int arrangement=0);
   void Reset();
   void Stats(float& power, float& hashrate, float &hitrate, float &max_temp, float &avg_temp, float& outlet_temp, float &ir, float &efficiency, bool quiet=false);
   void Optimize(const float THS, float ambient, bool uniform=false, bool quiet=false);
   float Dvfs_Optimize(const float average_f, bool quiet);
   bool Solve();
   void Print(const char* label = "", bool summary = false);
   void PrintToCSV(const char* filename = "");
//   void Program(const vector<batchtype>& batch);
//   void Program(const command_cfgtype &cmdpacket); 
//   void Respond(vector<batchtype>& batch);
//   bool Respond(const command_cfgtype& cmdpacket, uint32 &data);  // returns true if no response should be sent
   };



#endif //__ELECTRICAL_H_INCLUDED__
