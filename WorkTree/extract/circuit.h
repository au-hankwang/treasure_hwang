#ifndef __CIRCUIT_H_INCLUDED__
#define __CIRCUIT_H_INCLUDED__

#include "template.h"

#define VSS 0
#define VDD 1
#define VDDO 2
                                                                                 
enum transistortype {D_NMOS=0, D_PMOS=1, D_FUSE=3, D_DCAP=4};
enum vttype {VT_NOMINAL=0, VT_HIGH, VT_LOW, VT_SUPERLOW, VT_NOMINAL_PLA2, VT_HIGH_PLA2, VT_LOW_PLA2, VT_SUPERLOW_PLA2, VT_NOMINAL_PLA4, VT_HIGH_PLA4, VT_LOW_PLA4, VT_SUPERLOW_PLA4, VT_NOMINAL_PLA8, VT_HIGH_PLA8, VT_LOW_PLA8, VT_SUPERLOW_PLA8, VT_SRAM, VT_ALL, VT_MEDIUMTHICKOXIDE, VT_SRAMNFETPD, VT_SRAMPFETPU, VT_SRAMLSRPFETPUB, VT_SRAMNFETWL, VT_SRAMLSRNFETPDB, VT_SRAMLSRNFETWLB, VT_SRAMLSRNFETRBB};
enum valuetype {V_ZERO=0, V_ONE=1, V_X=2, V_Z=3, V_ILLEGAL=4};

extern stringPooltype stringPool;
class wirelisttype;

struct interpolatetype{
public:
   double sample, value;
};


struct nodecoordtype{
   int x, y;
   nodecoordtype()
      {
      x=y=0;
      }
   };


class devicetype{
public:
   const char *name;
   enum transistortype type;
   enum vttype vt;
   bool on, off;			// off is only used for x propogation
   bool m_flipflop_device;
   bool m_access_device;
   int source, gate, drain, bulk;
   float offcount;            // used for leakage power estimation, how many intervals was the device off
   float length, width, r;
   int fingers;
//   float x, y;
   devicetype()
      {
      off = on = false;
      name = NULL;
      length = width = r = 0.0;
      fingers = 0;
      source = gate = drain = bulk = -1;
//	  x = y = 0.0;
      vt = VT_NOMINAL;
      offcount=0.0;
      m_flipflop_device=0;
      m_access_device=0;
      }
   void InvertDevicetype()
      {
      if (type==D_NMOS) type = D_PMOS;
      else if (type==D_PMOS) type = D_NMOS;
      else FATAL_ERROR;
      }
   bool operator<(const devicetype &right) const
      {
      if (source < right.source) return true;
      if (source > right.source) return false;
      if (gate   < right.gate)   return true;
      if (gate   > right.gate)   return false;
      if (drain  < right.drain)  return true;
      if (drain  > right.drain)  return false;
      if (type   < right.type)   return true;
      if (type   > right.type)   return false;
      return width < right.width;
      }
   double Leakage(vttype forceVt = VT_ALL) const; // return nA of leakage assuming Vds=Vdd
   double Leak2HighRDerate() const; // return nA of leakage assuming Vds=Vdd
   double GateCap() const; // returns fF
   double DiffCap() const; // returns fF
   bool StringMatchesAccessDeviceName(){
     if ( strstr(name, "MMPG_L")!=NULL || strstr(name, "MMPG0_L")!=NULL || strstr(name, "MMPG1_L")!=NULL || strstr(name, "MMPG2_L")!=NULL || strstr(name, "MMRPDT")!=NULL ||
	  strstr(name, "MMPG_H")!=NULL || strstr(name, "MMPG0_H")!=NULL || strstr(name, "MMPG1_H")!=NULL || strstr(name, "MMPG2_H")!=NULL )
       return true;
     else
       return false;
   }

   };

class FlipFlop{
  //Basic for now. Has a name, toggle count and last toggle time.
  //Used for publishing flop based power.
 public:
  char m_name[512];
  int m_tc;
  int m_last_ttime;
  float m_sw_cap; //switched cap. this is NOT just the flops output cap, but all the downstream nodes that it is the owner of also included.
  float m_sw_capTare;
  FlipFlop(const char* name){
    strcpy(m_name, name);
    m_sw_cap=m_tc=m_last_ttime=m_sw_capTare=0;
  }
};


struct nodetype
   {
   const char *name;
   int bit;
   int touched;
   int overlay;       // this is used in the resistance extraction for ratioed logic
   int toggleCount;   // used for active power estimation, how many times did this node wiggle
   int toggleTare;
   float capacitance; // fF
   float fixed_cap;   // this is only fixed cap from star file. It's already added into capacitance
   int *outputSigChangedPtr;
   unsigned output   :1;
   unsigned input    :1;
   unsigned state    :1;
   unsigned inflight :1;
   unsigned ratioed  :1;
   unsigned changed  :1;
   unsigned constant :1;
   unsigned drivesInstance :1;
   unsigned bristle  :1;
   unsigned drivesCCR:1;
   unsigned renamedForX:1;
   unsigned forcedBitline:1;
   unsigned replicateFoot:1;
   unsigned xassert_notmodel:1;
   unsigned nofloatcheck:1;
   unsigned gs_delay:1;  // activity will indicate delay in ps if this flag is true
   unsigned preserve_net:1;
   unsigned skipLoad:1;
   unsigned skipDeviceCap:1;
   bool m_is_wl;
   bool m_is_pd;
   float activity;
   float frequency;
   enum valuetype value;
   nodecoordtype gatecoord, sdcoord; 
   arraytype <devicetype *> gates;
   arraytype <devicetype *> sds;
   arraytype <int> rams_wl, rams_bl, rams_on;
   FlipFlop *m_flop_owner;
  
   nodetype(const char *_name="")
      {
      input     = false;
      output    = false;
      value     = V_X;
      state     = false;
      inflight  = false;
      ratioed   = false;
      changed   = false;
      constant  = false;
      drivesInstance = false;
      bristle   = false;
      renamedForX = false;
      forcedBitline = false;
      replicateFoot = false;
      xassert_notmodel = false;
      nofloatcheck = false;
      gs_delay     = false;
      preserve_net = false;
      touched   = -1;
      toggleCount = 0;
      toggleTare = 0;
      capacitance = 0.0;
      activity    = 1.0;
      frequency   = -1.0;
      fixed_cap   = 0.0;
      outputSigChangedPtr = NULL;
      skipLoad = false;
      skipDeviceCap = false;
      m_flop_owner=NULL;
      m_is_wl=false;
      m_is_pd=false;
      NewName(_name);
      }
   void NewName(const char *_name, bool preserveBadNames=false)
      {
      char buffer[512];
      char *chptr;
      
      strcpy(buffer, _name);
      chptr = MAXIMUM(strchr(buffer,'<'), strchr(buffer,'['));
      if (chptr != NULL)
         {
         chptr = MAXIMUM(strchr(chptr+1,'<'), strchr(chptr+1,'['));
         if (chptr!=NULL) // I have a 2 dimensional arraytype. Collapse to 1 dimension
            {
            chptr = MAXIMUM(strchr(buffer,'<'), strchr(buffer,'['));
            *chptr = preserveBadNames ? '[' : '_';
            chptr = MAXIMUM(strchr(buffer,'>'), strchr(buffer,']'));
            if (chptr==NULL) FATAL_ERROR;
            *chptr = preserveBadNames ? ']' : '_';
            }
         }
      while ( (chptr=strchr(buffer,'*')) != NULL)
         *chptr = '_';
      if (buffer[strlen(buffer)-1]=='>' || buffer[strlen(buffer)-1]==']')
         {
         chptr = MAXIMUM(strrchr(buffer,'<'), strrchr(buffer,'['));
         if (chptr==NULL) FATAL_ERROR;
         bit = -1;
         if (chptr[1]>='0' && chptr[1]<='9')
            bit = atoi(chptr+1);
         if (bit <0)
            {  // crazy synopsys design ware has a negative subscripts
            chptr = MAXIMUM(strchr(buffer,'<'), strchr(buffer,'['));
            *chptr = '_';
            chptr = MAXIMUM(strchr(buffer,'>'), strchr(buffer,']'));
            if (chptr==NULL) FATAL_ERROR;
            *chptr = '_';
            while ( (chptr=strchr(buffer,'-')) != NULL)
               *chptr = '$';
            bit = -1;
            }
         else
            *chptr = 0;
         name = stringPool.NewString(buffer);
         }
      else
         {
         bit = -1;
         name = stringPool.NewString(buffer);
         }   
      }
   const char *RName(char *buffer) const
      {
      strcpy(buffer, name);
      if (bit>=0)
         sprintf(buffer+strlen(buffer),"[%d]",bit);
      return buffer;
      }
   const char *VName(char *buffer, bool noPrefix=false, bool preserveBadNames=false) const
      {
      VNameNoBit(buffer, noPrefix, preserveBadNames);
      if (bit>=0)
         sprintf(buffer+strlen(buffer),"[%d]",bit);
      return buffer;
      }
   const char *VNameNoBracket(char *buffer, bool noPrefix=true) const
      {
      VNameNoBit(buffer, noPrefix);
      if (bit>=0)
         sprintf(buffer+strlen(buffer),"_%d_",bit);
      return buffer;
      }
   const char *VNameNoBit(char *buffer, bool noPrefix=false, bool preserveBadNames=false) const
      {
      char *chptr;
      if (stricmp(name,"vdd")==0 || stricmp(name,"gnd")==0 || stricmp(name,"vss")==0 || stricmp(name,"vddo")==0)
         {
         strcpy(buffer, name);
         return buffer;
         }
      if (input && !output)
         strcpy(buffer, "i_");
      else if (!input && output)
         strcpy(buffer, "o_");
      else if (input && output)
         strcpy(buffer, "io_");
      else
         strcpy(buffer, "w_");
      if (noPrefix)
         buffer[0]=0;
      strcat(buffer, name);
      while ((chptr=strchr(buffer,'<'))!=NULL)
         *chptr = '[';
      while ((chptr=strchr(buffer,'>'))!=NULL)
         *chptr = ']';
      if (!preserveBadNames)
         {
         while ((chptr=strchr(buffer,'/'))!=NULL)
            *chptr = '$';
         while ((chptr=strchr(buffer,'\\'))!=NULL)
            *chptr = '$';
         while ((chptr=strchr(buffer,'*'))!=NULL)
            *chptr = '$';
         }
      if (buffer[strlen(buffer)-1]=='>')
         {
         if ((chptr=strrchr(buffer,'<'))!=NULL)
            *chptr = 0;
         else
            FATAL_ERROR;
         }
      if (!preserveBadNames)
         {
         while ((chptr=strchr(buffer,'['))!=NULL)
            *chptr = '_';
         while ((chptr=strchr(buffer,']'))!=NULL)
            *chptr = '_';
 
         if (bit>=0 && !input && !output && !noPrefix)           // make busses be unique for signals
            strcat(buffer,"$");
         }
      return buffer;
      }
     bool StringMatchesWordLine() const{
       int len=strlen(name);
       if (m_is_wl){
	 return true;
       }
       else if ((stricmp(name, "SLC")==0) || (stricmp(name, "SLT")==0) ||
		(stricmp(name, "WL0_H")==0) || (stricmp(name, "WL1_H")==0) || (stricmp(name, "WL2_H")==0) || (stricmp(name, "WL")==0)||
		(strnicmp(name, "WWL", 3)==0)|| (strnicmp(name, "RWL", 3)==0)|| (strnicmp(name, "LWL",3)==0)){
	 return true;
       }else {
	 return false;
       }
     }
     bool StringMatchesPullDown() const{
       int len=strlen(name);
       if (m_is_pd){
	 return true;
       }
       else if ((stricmp(name, "hit_cr_l")==0)||
		(stricmp(name, "ghit_cr_l")==0)){
	 return true;
       }else {
	 return false;
       }
     }
};


struct leakageSignaturetype{
   devicetype pass, n, p;
   leakageSignaturetype(const devicetype &_pass, const devicetype &_n, const devicetype &_p) : pass(_pass), n(_n), p(_p)
      {
      }
   };


struct ramtype
   {
   enum valuetype value;
   int wl;                 // node index which is active high word line
   int bh, bl;             // node index which bitlines
   int leakSigIndex;
   ramtype()
      {
      value = V_ZERO;
      wl = bh = bl = -1;
      }
   ramtype(int _wl, int _bh, int _bl, int _leakSigIndex) : wl(_wl), bh(_bh), bl(_bl), leakSigIndex(_leakSigIndex)
      {
      value = V_ZERO;
      }
   };




// This makes a heap out of many stacks to yield an O(1) heap for most
// frequent keys. I use a real heap for everything else with O(lgn)
const int num_quick_heap_keys=50; // this number is non critical so long as it
                                  // is greater than most paths(on order of
                                  // infinite loop
class nodeheaptype{
   struct datatype 
      {
      int n;
      int key;
      bool operator<(const datatype &right) const
         {
         return key < right.key;
         }
       };
       
   int bottom,size;
   arraytype <int> quickie[num_quick_heap_keys];
   heaptype<datatype> underflowheap;
   heaptype<datatype> overflowheap;
   
public:
   
   nodeheaptype() 
      {bottom=size=0;}
   void clear()
      {
      int i;
      for (i=0; i<num_quick_heap_keys; i++)
         quickie[i].clear();
      underflowheap.clear();
      overflowheap.clear();
      }
   
   void push(int n, int key)
      {
      datatype temp;
      temp.key=key; temp.n=n;
      if (key<0)
         underflowheap.push(temp);
      else if (key<num_quick_heap_keys)
         quickie[key].push(n);
      else
         overflowheap.push(temp);
      bottom=MINIMUM(bottom, key);
      size++;
      }
   int pop(int &n, int &key)
      {
      datatype temp;
      
      if (!size) return(false);
      size--;
      if (bottom<0)
         if (underflowheap.pop(temp))
            {
            n=temp.n; key=temp.key;
            bottom=temp.key;
            return(true);
            }
         else bottom=0;
      for (;bottom<num_quick_heap_keys; bottom++)
         {
         if (quickie[bottom].pop(n))
            {
            key=bottom;
            return(true);
            }
         }
      if (!overflowheap.pop(temp))
	FATAL_ERROR;
      n=temp.n;
      key=bottom=temp.key;
      return(true);
      }
   void down()
      {
      struct datatype temp;
      int delta=bottom,i,n;
      
      for (i=delta; i<num_quick_heap_keys; i++)
         {
         while (quickie[i].pop(n))
            {
            push(n,i-delta);        // reheap these elements at a lower number
            size--;
            }
         }
      while (overflowheap.pop(temp))
         {
         temp.key= MINIMUM(temp.key-delta,num_quick_heap_keys-1);
         push(temp.n,i-delta);        // reheap these elements at a lower number
         size--;
         }      
      }      
   };


class circuittype{
protected:
  double gds_units; /*This is the maximum resolution provided by the technology. Used to round x,y coordinates.*/
public:
   nodeheaptype nodeheap;
   int infinite_loop_threshold;
public:
   texthashtype<int> h;//used as a node hash
   arraytype <devicetype> devices;
   arraytype <nodetype> nodes;
   arraytype <ramtype> rams;
   arraytype <int> inputs;
   arraytype <int> outputs;
   arraytype <int> internals;
   arraytype <leakageSignaturetype> leakSigs;
   arraytype <const char*> bboxes;
   arraytype <FlipFlop*> flops;
   texthashtype<int> flophash; //While computing power, we need to be able to look up the flops quickly.

   int touchcount;
   
   circuittype()
      {
      infinite_loop_threshold = 500;
      }
   circuittype(const circuittype &right)
      {
      operator=(right);
      }
   void Propogate(int time, bool ignoreCrowbars=false, bool ignoreWarnings=false);
   void PropogateX();
   void PropogateActivity(int node_index);
   void Initialize(bool one=false, bool suppress_warnings=false);
   void AdjustDevicesOnThisNode(int n, int time);
   void Eval(int n, bool &drive0, bool &drive1, arraytype <int> &statenodes, arraytype <int> &ramnodes);
   void EvalOff(int n, bool &drive0, bool &drive1, arraytype <int> &statenodes);
   void CrowbarHelpEval(int cn);
   void DebugEval(int cn);
   void SetAsRatioed(int n);
   float QuickStrength(int n, int power);
   void ReSync();
   void ZeroNodes();
   void MergeDevices();
   void Parse();
   bool IsSimpleDriver(int node_index);
   bool IsOnlyInverters(int node_index);
   bool IsOnlyNand(int node_index);
   bool DoesFloatMatter(int node_index);
   void ReadSpiceWirelist(char *filename);
   void ReadCapfile(const char *filename);
   void ReadActivityfile(const char *filename);
   void ReadBboxFile(const char *filename);
   void FlattenWirelist(wirelisttype *wl);
   void MarkAllBboxStructures();
   void ReportAllBboxStructures();
   void SetInfiniteLoopThreshold(int _threshold)
      {
      infinite_loop_threshold = _threshold;
      }
   void UnAllocate()
      {
      devices.UnAllocate();
      nodes.UnAllocate();
      inputs.UnAllocate();
      outputs.UnAllocate();
      internals.UnAllocate();
      }
   circuittype &operator=(const circuittype &right)
      {
      int i, k;
      nodeheap   = right.nodeheap;
      infinite_loop_threshold = right.infinite_loop_threshold;
      devices    = right.devices;
      nodes      = right.nodes;
      inputs     = right.inputs;
      outputs    = right.outputs;
      internals  = right.internals;
      touchcount = right.touchcount;
      for (i=0; i<nodes.size(); i++)
         {
         nodetype &n = nodes[i];
         if (right.devices.size()==0)
            {
            n.sds.clear();
            n.gates.clear();
            }
         for (k=0; k<n.sds.size(); k++)
            {
            int index = n.sds[k]-&right.devices[0];
            n.sds[k] = &devices[index];
            }
         for (k=0; k<n.gates.size(); k++)
            {
            int index = n.gates[k]-&right.devices[0];
            n.gates[k] = &devices[index];
            }
//         nodes[k].sds.clear();
//         nodes[k].gates.clear();
         }
      return *this;
      }
   void CrossReference()
      {
      int k;
      for (k=0; k<nodes.size(); k++)
         {
         nodes[k].sds.clear();
         nodes[k].gates.clear();
         nodes[k].bristle = false;
         nodes[k].drivesInstance = false;
         nodes[k].drivesCCR = false;
         }
      for (k=0; k<devices.size(); k++)
         if (devices[k].width>0.0)
            {
            nodes[devices[k].source].sds.push(&devices[k]);
            nodes[devices[k].drain].sds.push(&devices[k]);
            nodes[devices[k].gate].gates.push(&devices[k]);
            }
      nodes[VSS].sds.clear();
      nodes[VDD].sds.clear();
      }
   virtual void Log(const char *fmt, ...)
      {
      va_list args;
      char buffer[1024];   
      
      va_start(args, fmt);
      int count = vsprintf(buffer, fmt, args);
      va_end(args);
      
      printf(buffer);
      }
   virtual void ErrorLog(const char *fmt, ...)
      {
      va_list args;
      char buffer[1024];   
      
      va_start(args, fmt);
      int count = vsprintf(buffer, fmt, args);
      va_end(args);
      
      printf(buffer);
      }
   virtual void IntermediateNodeDumping(int relativetime, int currentTime)  // useful to dump trace file with gate delay resolution
      {}
   void SetGDSUnits(double units){gds_units=units;}
   double GetGDSUnits(){return gds_units;}
 
   //This function mainly exists because of star RC co-ordinate values. StarRC biases polygons and sometimes writes out values that are
   //slightly smaller than the actual edge values for a given polygon. This function right now simply rounds to the nearest GDS grid location
   int RoundToGDSResolution(double coord_value) {
//     int round_to_grid = (int) round(coord_value/GetGDSUnits());
     int round_to_grid = (int) (coord_value/GetGDSUnits()+0.5);
     // printf("RoundToGDSResolution:  Units: %.6lf In: %.6lf coord_value rounded_value: %d\n", GetGDSUnits(), coord_value, round_to_grid);
     return round_to_grid;

   }
   void PopulateFlopsHash();
   void AssignFlopOwnersForNodes();
   int SearchListForFlopDevice(arraytype<devicetype*>& devlist);
};







#endif
