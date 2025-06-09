#ifndef __WIRELIST_H_INCLUDED__
#define __WIRELIST_H_INCLUDED__

#include "template.h"
#include "circuit.h"
//#include "globals.h"
#include "geometry.h"

class wirelisttype;
class udptype;
class ccrtype;
class bddtype;

class instancetype{
public:
   const char *name;
   wirelisttype *wptr;
   translationtype translation;
   arraytype <int> outsideNodes;  // will have same ordering as bristles in wirelisttype   
   const char *VName(char *buffer) const
      {
      char *chptr;
//      strcpy(buffer, "dac_");  // this eliminates the possibility of colliding with reserve word
//      strcat(buffer, name);
      strcpy(buffer, name);
      while ((chptr=strchr(buffer,'<'))!=NULL)
         *chptr = '_';
      while ((chptr=strchr(buffer,'>'))!=NULL)
         *chptr = '_';
      while ((chptr=strchr(buffer,'['))!=NULL)
         *chptr = '_';
      while ((chptr=strchr(buffer,']'))!=NULL)
         *chptr = '_';
      while ((chptr=strchr(buffer,'/'))!=NULL)
         *chptr = '_';
      while ((chptr=strchr(buffer,'\\'))!=NULL)
         *chptr = '_';
      while ((chptr=strchr(buffer,'*'))!=NULL)
         *chptr = '_';
      while ((chptr=strchr(buffer,'#'))!=NULL)
         *chptr = '_';
      while ((chptr=strchr(buffer,'!'))!=NULL)
         *chptr = '_';
      return buffer;
      }
   instancetype()
      {
      name = NULL;
      wptr = NULL;
      }
};

class bristletype{
public:
   const char *name;
   int bit;
   int nodeindex;
   bool output, input, inverter, pad, pureinput, substituted, is_bbox_io; // pure input is if this port connects to gates and is undriven
   bristletype()
      {
      FATAL_ERROR; // needed for sun compiler
      }
   bristletype(const char *_name, int _index)
      {
      char buffer[512];
      char *chptr;
      nodeindex = _index;
      input = output = inverter = pad = pureinput = substituted = is_bbox_io = false;
      
      strcpy(buffer, _name);
      while ((chptr=strchr(buffer,'\\'))!=NULL)
         *chptr = '$';
      while ((chptr=strchr(buffer,'/'))!=NULL)
         *chptr = '$';
      chptr=MAXIMUM(strchr(buffer,'<'),strchr(buffer,'['));
      if (chptr==NULL)
         bit = -1;
      else
         {
         bit = atoi(chptr+1);
         *chptr = 0;
         }
      name = stringPool.NewString(buffer);
      }
   const char *VName(char *buffer) const
      {
      VNameNoBit(buffer);
      if (bit>=0)
         sprintf(buffer+strlen(buffer),"[%d]",bit);
      return buffer;
      }
   const char *VNameNoBit(char *buffer) const
      {
      char *chptr;
      if (input && !output)
         strcpy(buffer, "i_");  
      else if (!input && output)
         strcpy(buffer, "o_");  
      else if (input && output)
         strcpy(buffer, "io_");  
      else
         strcpy(buffer, "w_");  
      strcat(buffer, name);
      while ((chptr=strchr(buffer,'<'))!=NULL)
         *chptr = 0;
      while ((chptr=strchr(buffer,'['))!=NULL)
         *chptr = 0;
      while ((chptr=strchr(buffer,'\\'))!=NULL)
         *chptr = '$';
      while ((chptr=strchr(buffer,'/'))!=NULL)
         *chptr = '$';
      if (bit>=0 && !input && !output)           // make busses be unique for signals
         strcat(buffer,"$");
      return buffer;
      }
   bool operator==(const char *rawname) const
      {
      char buffer[512], *chptr;
      int rightbit = -1;
      strcpy(buffer, rawname);
      while ((chptr=strchr(buffer,'\\'))!=NULL)
         *chptr = '$';
      while ((chptr=strchr(buffer,'/'))!=NULL)
         *chptr = '$';
      chptr=MAXIMUM(strchr(buffer,'<'), strchr(buffer,'['));
      if (chptr != NULL)
         {
         rightbit = atoi(chptr+1);
         *chptr = 0;
         }
      if (strcmp(buffer, name)==0 && bit==rightbit)
         return true;
      return false;
      }
   };


class buscompresstype{
public:
   const char *name;
   int lsb, msb;
   bool output, input, pad;
   arraytype <int> refs;
   
   buscompresstype()
      {
      FATAL_ERROR;  // needed for sun compiler
      }
   buscompresstype(const bristletype &right, const arraytype <int> &_refs=arraytype<int>())
      {
      name = right.name;
      lsb = msb = right.bit;
      input  = right.input;
      output = right.output;
      pad    = right.pad;
      refs = _refs;
      }
   buscompresstype(const nodetype &right)
      {
      name = right.name;
      lsb = msb = right.bit;
      input  = right.input;
      output = right.output;
      pad    = false;
      }
   bool operator<(const buscompresstype &right) const
      {
      if (input*1 + output*2 != right.input*1 + right.output*2)
         return input*1 + output*2 < right.input*1 + right.output*2;
      int c = strcmpxx(name, right.name);
      if (c<0) return true;
      if (c>0) return false;

      return lsb < right.lsb;
      }
   bool operator==(const buscompresstype &right) const
      {
      if (input*1 + output*2 != right.input*1 + right.output*2)
         return false;
      return strcmp(name, right.name)==0;
      }
   const char *VName(char *buffer, bool noPrefix=false) const
      {
      char *chptr;

      if (input && !output)
         strcpy(buffer, "i_");  
      else if (!input && output)
         strcpy(buffer, "o_");  
      else if (input && output)
         strcpy(buffer, "io_");  
      else
         strcpy(buffer, "w_");  
      if (noPrefix)
         strcpy(buffer, "");
      strcat(buffer, name);

      while ((chptr=strchr(buffer,'<'))!=NULL)
         *chptr = '[';
      while ((chptr=strchr(buffer,'>'))!=NULL)
         *chptr = ']';
      while ((chptr=strchr(buffer,'/'))!=NULL)
         *chptr = '$';
      while ((chptr=strchr(buffer,'\\'))!=NULL)
         *chptr = '$';
      while ((chptr=strchr(buffer,'*'))!=NULL)
         *chptr = '$';
      if (buffer[strlen(buffer)-1]==']')
         {
         if ((chptr=strrchr(buffer,'['))!=NULL)
            *chptr = 0;
         else
            FATAL_ERROR;
         }
      while ((chptr=strchr(buffer,'['))!=NULL)
         *chptr = '_';
      while ((chptr=strchr(buffer,']'))!=NULL)
         *chptr = '_';
      if (lsb>=0 && !input && !output && !noPrefix)           // make busses be unique for signals
         strcat(buffer,"$");
      return buffer;
      }
   };


enum E_CCRtype{CCR_NULL, CCR_BUF, CCR_INV, CCR_NAND, CCR_NOR, CCR_AND, CCR_OR, CCR_COMBINATIONAL, CCR_SEQUENTIAL, CCR_SRAM, CCR_SRAM_WRITENABLE};
const int max_input_space = 1 <<17;  // must be less than 2^32/2

class udptype{
public:
   bool sequential, addDelay;
   int num_inputs, num_states;
   sha160digesttype digest;
   bitfieldtype <max_input_space> table;
   bitfieldtype <max_input_space> xtable;          // if Xtable=1 then if table=0(node is x), if table=1 then node is Z
   arraytype <bitfieldtype <max_input_space> > qt;      // only needed for sequential
   arraytype <bitfieldtype <max_input_space> > qx;      // only needed for sequential
   udptype() : sequential(false)
      {
      FATAL_ERROR;  // needed for sun compiler
      }
   udptype(bool _sequential, int _num_states=0) : sequential(_sequential), num_states(_num_states), qt(_num_states), qx(_num_states), addDelay(false)
      {
      }
   void WriteCombinationalPrimitive(filetype &fp, int uu, bool atpg, bool assertionCheck, bool _addDelay=false) const;
   void WriteSequentialPrimitive(filetype &fp, int uu, bool atpg, bool assertionCheck) const;
   void WriteMdpWrapper(filetype &fp, int uu, bool atpg, bool assertionCheck, const arraytype<bool> &delays) const;
   bool operator==(const udptype &right)
      {
      if (sequential != right.sequential) return false;
      if (num_inputs != right.num_inputs) return false;
      if (table      != right.table)      return false;
      if (xtable     != right.xtable)     return false;
      if (addDelay	 != right.addDelay)   return false;
      return true;
      }
   };


class ccr_circuittype : public circuittype{
   const wirelisttype &parent;

public:
   static arraytype <udptype> udps;
   arraytype <int> states;        // internal state nodes for a sequential ccr
   arraytype <udptype> us;        // one udp for each output
   bool Zseen;                // note that the underlying circuit can potentially float a node

   ccr_circuittype(const wirelisttype &_parent) : parent(_parent)
      {
      }
   
   void ComputeDigest(const ccrtype &ccr, arraytype <sha160digesttype> &digests);
   bool ConvertToCircuittype(const ccrtype &ccr, filetype &fp, int &inum, arraytype <sha160digesttype> &digests, arraytype <int> &udp_index, bool noPrefix);  // returns true if a this cell has already been simulated   
   void SequentialTruthTable(ccrtype &ccr, filetype &fp, int &inum, arraytype <int> &udp_index, bool noPrefix, bool outputPostfix);
   void CombinationalTruthTable(const ccrtype &ccr, filetype &fp, int &inum, arraytype <int> &udp_index, bool noPrefix);
   void ZTruthTable(ccrtype &ccr, filetype &fp, int &inum, arraytype <int> &udp_index, bool noPrefix, const char *assertNode);
   void SramWritenableTruthTable(const ccrtype &ccr, filetype &fp, int &inum, arraytype <int> &udp_index, bool noPrefix);
   static void DumpUdps(filetype &fp, bool atpg, bool assertionCheck);
   
   // suppress all error messages, we don't care if something is shorted because we'll model it with an X
   virtual void Log(const char *fmt, ...)  
      {
      }
   virtual void ErrorLog(const char *fmt, ...)
      {
      }
   };


struct ccrinputtype{
   int index;
   bool compliment;
   int original_inverted_node;
   ccrinputtype()
      {
      index = -1;
      compliment = false;
      original_inverted_node = -1;
      }
   ccrinputtype(int _index, bool _compliment=false)
      {
      index = _index;
      compliment = _compliment;
      original_inverted_node = -1;
      }
   bool operator<(const ccrinputtype &right) const
      {
      if (index<right.index) return true;
      if (index>right.index) return false;
      if ( compliment && !right.compliment) return true;
      return false;
      }
   bool operator==(const ccrinputtype &right) const
      {
      return index==right.index && compliment==right.compliment;
      }
   };

class ccrtype{// channel connected region

public:
   E_CCRtype   type;
   unsigned secondOutputIsComplimentOfFirst :1;
   unsigned addDelay :1;
   unsigned treatZasOne :1; 				  
   unsigned treatZasZero :1; 				  
   unsigned treatZasLatch :1;
   unsigned sequentialZAssertion :1;
   unsigned crossCoupledNands :1;			  
   unsigned nandKeeper :1;
   unsigned dontMerge :1;
   unsigned lastTwoInputsAreWeakPulldowns :1;  // allow shorts with the last two inputs without causing X's
   unsigned wiredor :1;		  // this denotes a buffer which should get (strong1, weak0) to prevent X's when inverters are shorted together
   float delay;
   int originalNandIndex;	  // will refer to the nand prior to merge if nand keeper
   
   wirelisttype &parent;

   arraytype <devicetype> devices;
   arraytype <ccrinputtype> inputs;
   arraytype <int> nodes_used_byme;  // list of all the nodes used by this ccr
   arraytype <int> outputs;
   arraytype <int> udp_indices;

   arraytype <int> arc_prior;
   arraytype <int> arc_next;
   arraytype <int> states;        // internal state nodes for a sequential ccr

   static arraytype <int> nands, ands, nors, ors;


   ccrtype(wirelisttype &_parent) : parent(_parent)
      {
      Clear();
      }
   ~ccrtype()
      {
      Clear();
      }
   void Clear()
      {
      type = CCR_NULL;
      secondOutputIsComplimentOfFirst = false;
      addDelay = false;
      delay = 0.0;
      treatZasOne = false;
      treatZasZero = false;
      treatZasLatch =  false;
      sequentialZAssertion = false;
      crossCoupledNands = false;
      nandKeeper = false;
      dontMerge = false;
      wiredor = false;
      lastTwoInputsAreWeakPulldowns = false;
      originalNandIndex = -1;
      devices.clear();
      inputs.clear();
      outputs.clear();
      states.clear();
      arc_prior.clear();
      arc_next.clear();
      }
   void CrossReference();	// This will change parent.nodes[]
   void WriteSimpleCCR(filetype &fp, int &inum, bool atpg, bool noPrefix=false);
   void WriteSram(filetype &fp, int &inum);
   void Setup();
   void Merge(const ccrtype &right, bool mergeOutputs);
   void PruneStates();
   void Print(filetype &fp, int index) const;
   void Simplification(filetype &fp, int index, arraytype <ccrtype> &romccrs);
   void SramTransform(arraytype <ccrtype> &romccrs);  
   void SramTransform2(arraytype <ccrtype> &romccrs, const arraytype <int> &bitlines);

   static void DumpPrimitives(filetype &fp);
   };

class substitutetype{
public:
   arraytype <wildcardtype> wcs;
   arraytype <const char *> filenames;
   
   substitutetype(int num) : wcs(num), filenames(num, NULL)
      {

      }
   bool Check(const char *string) const
      {
      int i;
      for (i=0; i<wcs.size(); i++)
         if (wcs[i].Check(string))
            return true;
      return false;
      }
   bool Check(const char *string, const char *&source) const
      {
      int i;
      if (filenames.size() != wcs.size())
         FATAL_ERROR;

      for (i=0; i<wcs.size(); i++)
         if (wcs[i].Check(string))
            {
            source = filenames[i];
            return true;
            }
      source = NULL;
      return false;
      }
   };


class wirelisttype{
public:
   const char *name;
   bool nmosPrimitive, pmosPrimitive;
   arraytype <nodetype> nodes;
   arraytype <bristletype> bristles;
   texthashtype <int> h;
   arraytype <instancetype> instances;
   arraytype <devicetype> devices;
   arraytype <ramtype> rams;
   arraytype <ccrtype> ccrs;
   bool parallelMergeDone;
   bool keepOnlySupplyDevicesDone;
   bool is_bbox;
   wirelisttype(arraytype<const char *> &vddNames, arraytype<const char *> &vssNames)
      {
      int i;
      name = NULL;
      nmosPrimitive = pmosPrimitive = is_bbox = false;
      parallelMergeDone = false;
      keepOnlySupplyDevicesDone = false;
      nodes.push(nodetype("gnd"));
      nodes.push(nodetype("vdd"));
      nodes.push(nodetype("vddo"));
      h.Add(nodes[0].name, 0);
      h.Add(nodes[1].name, 1);
      for (i=0; i<vssNames.size(); i++)
         h.Add(vssNames[i], 0);
      for (i=0; i<vddNames.size(); i++)
         h.Add(vddNames[i], 1);
      }
   wirelisttype(const wirelisttype *that)
      {
      name = NULL;
      nmosPrimitive = pmosPrimitive = false;
      parallelMergeDone = false;
      keepOnlySupplyDevicesDone = false;
      nodes.push(nodetype("gnd"));
      nodes.push(nodetype("vdd"));
      nodes.push(nodetype("vddo"));
      h = that->h;
      }
   int AddNet(const char *name)  // will add a node and return it's index
      {
      nodes.push(nodetype(name));
      return nodes.size()-1;
      }
   bool Flatten(arraytype <leakageSignaturetype> &leakSigs);   // this will flatten everything to transistors.
   wirelisttype *FindDeepestMinusOne();
   bool CheckForSram(int &wl, int &bh, int &bl, devicetype &pass, devicetype &n, devicetype &p); // returns true if this module is a single sram cells, also returns wl,bh,bl port indexes 
   void FlattenInstance(const instancetype &inst, bool preserveBadNames=false);
   void PromoteAnyBboxBristles(instancetype &inst);
   void BlackBoxInstance(instancetype &inst, bool preserveBadNames=false);
   void DetermineBristleDirection(hashtype <const void *, bool> &beenHere, const substitutetype &ignoreStructures);
   void ApplyInverterHint(filetype &fp);
   void ConvertToCCR(filetype &fp);
   void BuildCCRArcLists();
   void MergeParallelDevices();
   void KeepOnlySupplyDevices();
   void WriteVerilog(substitutetype &ignoreStructures, wildcardtype &explodeStructures, wildcardtype &explodeUnderStructures, wildcardtype &deleteStructures, wildcardtype &noXStructures, wildcardtype &xMergeStructures, wildcardtype &prependPortStructures, const char *&destname, bool atpg, bool assertionCheck, bool debug, bool noPrefix, filetype &fp, bool makeMap);  // caller most close fp
   void ConsolidateCCR(filetype &fp, bool aggressivelyMerge, bool atpg);
   void AddExplicitInverters(filetype &fp);
   void AnalyzeNetworksForX(filetype &fp);
   void RecursiveOrder(int nodeIndex, arraytype <int> &order, int touch_semaphore);
   void RecursiveAnalyze(int nodeIndex, arraytype <bddtype *> &bdds, const arraytype <int> &order, arraytype <int> &visited);
   void AbstractCCR(filetype &fp, int &inum, bool atpg, bool noPrefix);
   void ExplicitExplodes(substitutetype &ignoreStructures, wildcardtype &explodeStructures, filetype &fp);
   void ExplicitUnderExplodes(substitutetype &ignoreStructures, wildcardtype &explodeUnderStructures, filetype &fp);
   void ExplicitDeletes(substitutetype &ignoreStructures, wildcardtype &deleteStructures, filetype &fp);
   void VddoCheck(hashtype <const void *, bool> &beenHere, filetype &fp);
   void CtranCamTransform(substitutetype &ignoreStructures, hashtype <const void *, bool> &beenHere, filetype &fp);
   void CtranLatchTransform(substitutetype& ignoreStructures, hashtype <const void*, bool>& beenHere, filetype& fp);
   void BottomStackReplicateTransform(substitutetype &ignoreStructures, hashtype <const void *, bool> &beenHere, filetype &fp);
   void FullKeeperTransform();
   void HintTransform(substitutetype &ignoreStructures, hashtype <const void *, bool> &beenHere, filetype &fp);
   void NMerge();
   void PrintDevices() const;
   wirelisttype(const wirelisttype &right) // copy constructor
      {
      operator=(right);
      }
   void operator=(const wirelisttype &right)
      {
      int i, k;
      
      bristles = right.bristles;
      h        = right.h;
      devices  = right.devices;
      nodes    = right.nodes;
      instances= right.instances;

      for (i=0; i<nodes.size(); i++)
         {
         nodetype &n = nodes[i];
         for (k=0; k<n.gates.size(); k++)
            {
            int index = (n.gates[k] - &right.devices[0]) / sizeof(devicetype);
            if (index <0 || index>= devices.size())
               FATAL_ERROR;
            n.gates[k] = &devices[index];
            }
         for (k=0; k<n.sds.size(); k++)
            {
            int index = (n.sds[k] - &right.devices[0]) / sizeof(devicetype);
            if (index <0 || index>= devices.size())
               FATAL_ERROR;
            n.sds[k] = &devices[index];
            }
         }
      }
   bool IsBitCell() const;
   bool IsPchCell() const;
   bool IsFlipFlop() const;
   bool IsLatch() const;
};

void Statistics(const wirelisttype *top, bool ignoreMems, vttype vt, const bool statMergeParallel, const bool statOnlySupplyDevices, const bool debug);
wirelisttype *FindWirelistStructure(const char *structure);
wirelisttype *ReadWirelist(const char *filename, arraytype<const char *> &vddNames, arraytype<const char *> &vssNames);
wirelisttype* ReadCDLWirelist(const char* filename, arraytype<const char*>& vddNames, arraytype<const char*>& vssNames);
void CDLParseLine(char *buffer, arraytype<const char*>& vddNames, arraytype<const char*>& vssNames);
const char* SquaredName(const char* input);


#endif
