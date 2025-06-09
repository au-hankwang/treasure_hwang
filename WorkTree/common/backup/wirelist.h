#ifndef __WIRELIST_H_INCLUDED__
#define __WIRELIST_H_INCLUDED__

#include "template.h"
#include "circuit.h"
#include "geometry.h"

class wirelisttype;
class udpCollection;
class udptype;

class instancetype{
public:
   const char *name;
   wirelisttype *wptr;
   translationtype translation;
   array <int> outsideNodes;  // will have same ordering as bristles in wirelisttype   
   const char *VName(char *buffer) const
      {
      char *chptr;
      strcpy(buffer, "dac_");  // this eliminates the possibility of colliding with reserve word
      strcat(buffer, name);
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
   bool output, input, inverter, pad, pureinput; // pure input is if this port connects to gates and is undriven
   bristletype()
      {
      FATAL_ERROR; // needed for sun compiler
      }
   bristletype(const char *_name, int _index)
      {
      char buffer[512];
      char *chptr;
      nodeindex = _index;
      input = output = inverter = pad = pureinput = false;
      
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
   array <int> refs;
   
   buscompresstype()
      {
      FATAL_ERROR;  // needed for sun compiler
      }
   buscompresstype(const bristletype &right, const array <int> &_refs=array<int>())
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
      if (lsb>=0 && !input && !output)           // make busses be unique for signals
         strcat(buffer,"$");
      return buffer;
      }
   };


enum E_CCRtype{CCR_NULL, CCR_BUF, CCR_INV, CCR_NAND, CCR_NOR, CCR_AND, CCR_OR, CCR_TRISTATE, CCR_COMBINATIONAL, CCR_SEQUENTIAL, CCR_SRAM, CCR_SRAM_WRITENABLE};
const int max_input_space = 1 <<17;  // must be less than 2^32/2

class udptype{
public:
   bool sequential;
   int num_inputs, num_states;
   bitfieldtype <max_input_space> table;
   bitfieldtype <max_input_space> xtable;
   array <bitfieldtype <max_input_space> > qt;      // only needed for sequential
   array <bitfieldtype <max_input_space> > qx;      // only needed for sequential
   udptype() : sequential(false)
      {
      FATAL_ERROR;  // needed for sun compiler
      }
   udptype(bool _sequential, int _num_states=0) : sequential(_sequential), num_states(_num_states), qt(_num_states), qx(_num_states)
      {
      }
   void WriteCombinationalPrimitive(filetype &fp, int uu, bool atpg, bool assertionCheck) const;
   void WriteSequentialPrimitive(filetype &fp, int uu, bool atpg, bool assertionCheck) const;
   bool operator==(const udptype &right)
      {
      if (sequential != right.sequential) return false;
      if (num_inputs != right.num_inputs) return false;
      if (table      != right.table)      return false;
      if (xtable     != right.xtable)    return false;
      return true;
      }
   };


class ccrtype : public circuittype{// channel connected region
// these 4 fields are present in the base class
/*
   array <nodetype>   nodes;	// nodes local just to this ccr
   array <devicetype> devices;  // index into devices
   array <int> inputs;
   array <int> outputs;
*/  
   static array <udptype> udps;

public:
   E_CCRtype   type;
   array <int>  external_node_xref;
   array <bool> compliments;  // bitmask to bubble our inputs, index relative to inputs
   bool secondOutputIsComplimentOfFirst;
   
   array <int> arc_prior;
   array <int> arc_next;
   array <int> states;        // internal state nodes for a sequential ccr
   array <udptype> us;        // one udp for each output

   ccrtype()
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
      external_node_xref.clear();
      nodes.clear();
      devices.clear();
      inputs.clear();
      outputs.clear();
      states.clear();
      compliments.clear();
      arc_prior.clear();
      arc_next.clear();
      }
   void WriteSimpleCCR(filetype &fp, int &inum);
   void WriteSram(filetype &fp, int &inum);
   void Setup(const wirelisttype *parent);
   void Merge(const ccrtype &right, bool mergeOutputs);
   void PruneStates();
   void CleanupInputs();
   int ExternalInput(int index) const
      {
      if (index<0 || index>=inputs.size()) FATAL_ERROR;
      return external_node_xref[inputs[index]];
      }
   int ExternalOutput(int index) const
      {
      if (index<0 || index>=outputs.size()) FATAL_ERROR;
      return external_node_xref[outputs[index]];
      }
   void Print(filetype &fp, int index) const;
   void CombinationalTruthTable(filetype &fp, int &inum);
   void SequentialTruthTable(filetype &fp, int &inum);
   void SramWritenableTruthTable(filetype &fp, int &inum);
   void Simplification(wirelisttype &parent, array <ccrtype> &romccrs); // returns true is it modified things
   void SramTransform(wirelisttype &parent, array <ccrtype> &romccrs);  // returns true is it modified things
   static void DumpUdps(filetype &fp, bool atpg, bool assertionCheck);      
   
   
   void Log(const char *fmt, ...)   // this suppresses print statements from circuittype base class
      {
      }
   void ErrorLog(const char *fmt, ...)// this suppresses print statements from circuittype base class
      {
      }
   };




class wirelisttype{
public:
   const char *name;
   array <nodetype> nodes;
   array <bristletype> bristles;
   texthashtype <int> h;
   array <instancetype> instances;
   array <devicetype> devices;
   array <ccrtype> ccrs;

   wirelisttype(array<const char *> &vddNames, array<const char *> &vssNames)
      {
      int i;
      name = NULL;
      nodes.push(nodetype("gnd"));
      nodes.push(nodetype("vdd"));
      nodes.push(nodetype("vddo"));
      h.Add(nodes[0].name, 0);
      h.Add(nodes[1].name, 1);
      h.Add(nodes[2].name, 2);
      for (i=0; i<vssNames.size(); i++)
         h.Add(vssNames[i], 0);
      for (i=0; i<vddNames.size(); i++)
         h.Add(vddNames[i], 1);
      }
   wirelisttype(const wirelisttype *that)
      {
      name = NULL;
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
   bool Flatten();   // this will flatten everything to transistors.
   void FlattenInstance(const instancetype &inst, bool preserveBadNames=false);
   void DetermineBristleDirection(hashtype <const void *, bool> &beenHere, wildcardtype &ignoreStructures);
   void ConvertToCCR(filetype &fp);
   void BuildCCRArcLists();
   void WriteVerilog(wildcardtype &ignoreStructures, wildcardtype &explodeStructures, wildcardtype &explodeUnderStructures, wildcardtype &deleteStructures, const char *&destname, bool atpg, bool assertionCheck, bool debug, filetype &fp, bool makeMap);  // caller most close fp
   void ConsolidateCCR(filetype &fp);
   void AbstractCCR(filetype &fp, int &inum);
   void AbstractDevices(filetype &fp, const char *, int &inum, bool atpg, bool assertionCheck);
   void ExplicitExplodes(wildcardtype &ignoreStructures, wildcardtype &explodeStructures, filetype &fp);
   void ExplicitUnderExplodes(wildcardtype &ignoreStructures, wildcardtype &explodeUnderStructures, filetype &fp);
   void ExplicitDeletes(wildcardtype &ignoreStructures, wildcardtype &deleteStructures, filetype &fp);
   void VddoCheck(hashtype <const void *, bool> &beenHere, filetype &fp);
   void NMerge();
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
   };

void Statistics(const wirelisttype *top, bool ignoreMems=false);
wirelisttype *ReadWirelist(const char *filename, array<const char *> &vddNames, array<const char *> &vssNames);



#endif
