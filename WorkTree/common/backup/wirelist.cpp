#include "pch.h"
#include "template.h"
#include "multithread.h"
#include "circuit.h"
#include "wirelist.h"


#ifdef _MSC_VER
// #pragma optimize("",off)
#endif

const float NMOS_OVER_PMOS_STRENGTH = 2.3;
static bool DEBUG = false;

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
   void WriteCombinationalPrimitive(filetype &fp, int uu, const char *unique, bool atpg0, bool atpg1, bool assertionCheck) const;
   void WriteSequentialPrimitive(filetype &fp, int uu, const char *unique, bool atpg0, bool atpg1, bool assertionCheck) const;
   bool operator==(const udptype &right)
      {
      if (sequential != right.sequential) return false;
      if (num_inputs != right.num_inputs) return false;
      if (table      != right.table)      return false;
      if (xtable     != right.xtable)    return false;
      return true;
      }
   };

void MinSOP(const bitfieldtype <max_input_space> &input, array <unsigned> &var, const int num_inputs);
void MinSOP(array <unsigned> &var);


class wctype : public circuittype{
private:
public:
   void Log(const char *fmt, ...)
      {
      }
   void ErrorLog(const char *fmt, ...)
      {
      }
   };



class tabletype : public wctype, public MultiThreadtype{
private:
   static array <udptype> udps;
   static int uniqueInstance;
   
   // use for MT
   bool sequential;
   array< array <udptype> > us_threads;


public:
   array <int> states;

   static const char *unique; 
   static bool assertionCheck;
   static filetype *fp;
   
   static void DumpUdps(bool atpg0, bool atpg1);
   tabletype()
      {
      SetInfiniteLoopThreshold(30);  // anything over thirty gate delay will be considered an infinite loop and illegal
      }
   void CombinationalTruthTable()
      {
      sequential = false;
//      if (inputs.size()<8)
         Launch(1);
//      else
//         Launch();
      }
   void SequentialTruthTable()
      {
      sequential = true;
//      if (inputs.size()<7)
         Launch(1);         // the overhead of threading isn't worth it here
//      else
//         Launch();
      }
   void Prolog()
      {
      for (int i=0; i<totalNumberOfThreads; i++)
         if (sequential)
            us_threads.push(array <udptype> (outputs.size(), udptype(true, states.size())));
         else
            us_threads.push(array <udptype> (outputs.size(), false));
      }
   void Func(const int threadnum)
      {
      if (sequential)
         SequentialFunc(threadnum);
      else
         CombinationalFunc(threadnum);
      }
   void SequentialFunc(const int threadnum);
   void CombinationalFunc(const int threadnum);

   bool Simplification(filetype &fp, int &inum, bool atpg);
   bool operator<(const tabletype &right) const
      {
      if (states.size() != right.states.size())
         return states.size() > right.states.size();
      if (inputs.size() != right.inputs.size())
         return inputs.size() > right.inputs.size();
      return internals.size() > right.internals.size();
      }
   };


filetype *tabletype::fp; 
const char *tabletype::unique; 
bool tabletype::assertionCheck;

array <wirelisttype *> structures;
texthashtype <int> hstructs;
array <udptype> tabletype::udps;
int tabletype::uniqueInstance;

// this will compute a minimum sum of products(almost).
// I will deliberately not be minimum to introduce more don't cares into the truth table to help reset
void MinSOP(const bitfieldtype <max_input_space> &input, array <unsigned> &vars, const int num_inputs)
   {
   TAtype ta("MinSOP()");
   int i, j, k;
   bitfieldtype <max_input_space> notdone = input;

   vars.clear();

   for (i=0; i<(1<<num_inputs); i++)
      {
      if (notdone.GetBit(i))
         { // now I want to remove variables and see if still covered
         for (int backwards=0; backwards<=1; backwards++)
            {
            array <int> vacuous;
            int iteration, v;
            for (k=backwards ? 0 : num_inputs-1; backwards ? (k<num_inputs) : (k>=0); k= k + (backwards?1:-1))
               {
               bool zerofound = false;
               vacuous.push(k);
               for (iteration=0; iteration< (1<<vacuous.size()); iteration++)
                  {
                  v = i;
                  for (j=0; j<vacuous.size(); j++)
                     {
                     if ((iteration>>j)&1)
                        v = v ^ (1<<vacuous[j]);
                     }
                  if (!input.GetBit(v))
                     zerofound = true;
                  }
               if (zerofound)
                  vacuous.pop_back();
               }
            v = i;
            for (j=0; j<vacuous.size(); j++)
               {
               v = v | (1<<(vacuous[j]+16));
               v = v | (1<<(vacuous[j]));
               }
            vars.push(v);
            }
         }
      }
   vars.RemoveDuplicates();
   return;
   }

void MinSOP(array <unsigned> &var)
   {
   int i, k;
   bool done;
   
   do{
      done = true;
      for (i=0; i<var.size(); i++)
         {
         unsigned data1 = var[i]&0xFFFF;
         unsigned mask1 = var[i]>>16;
         for (k=i+1; k<var.size(); k++)
            {
            unsigned data2 = var[k]&0xFFFF;
            unsigned mask2 = var[k]>>16;
            int p1, p2;
            
            unsigned diff1 = ((data1 ^ data2) & ~mask2);
            unsigned diff2 = ((data1 ^ data2) & ~mask1);
            
            p1 = PopCount(diff1);
            p2 = PopCount(diff2);
            if (mask1 & (mask1^mask2))
               p1 = 2;
            if (mask2 & (mask1^mask2))
               p2 = 2;
            if (p1==0)
               {
               var[i] = var[k];
               data1   = data2;
               mask1   = mask2;
               var.RemoveFast(k);
               k--;
               done = false;
               }
            else if (p2==0)
               {
               var.RemoveFast(k);
               k--;
               done = false;
               }
            if (p1==1)
               {
               var[i] = var[i] | (diff1<<16);
               mask1  = mask1  | (diff1<<16);
               done = false;
               }
            if (p2==1)
               {
               var[k] = var[k] | (diff2<<16);
               done = false;
               }
            }
         }
      }while (!done);
   }



void WriteBool(filetype &fp, const char *outputNode, int &instance, const bitfieldtype <max_input_space> &input, int num_inputs)
   {
   array <unsigned> var;  // high 16 bits will represent no variable
   array <bool> inv(num_inputs, false);
   int i,k;   
   
   MinSOP(input, var, num_inputs);
   
   // look for xor/_xnor
   bool _xor=false, _xnor=false;
   for (i=0; i< (1<<num_inputs); i++)
      {
      if ((PopCount((unsigned)i)&1) && input.GetBit(i))
         _xor = true;
      if (!(PopCount((unsigned)i)&1) && input.GetBit(i))
         _xnor = true;
      if (!(PopCount((unsigned)i)&1) && !input.GetBit(i))
         _xor = true;
      if ((PopCount((unsigned)i)&1) && !input.GetBit(i))
         _xnor = true;
      }
   if (_xor && !_xnor && num_inputs>1)
      {
      fp.fprintf("\nxor i%-2d(out", instance++);
      for (i=0; i<num_inputs; i++)
         fp.fprintf(",%c",'a'+i);
      fp.fprintf(");");
      return;
      }
   if (!_xor && _xnor && num_inputs>1)
      {
      fp.fprintf("\nxnor i%-2d(out", instance++);
      for (i=0; i<num_inputs; i++)
         fp.fprintf(",%c",'a'+i);
      fp.fprintf(");");
      return;
      }
   if (var.size() == 1)
      { // this could be or/nand
      bool _nor=true, _and=true;
      for (k=0; k<num_inputs; k++)
         {
         if ((var[0]>>(k+16)) &1)
            ;
         else if ((var[0]>>k)&1)
            _nor = false;
         else
            _and = false;
         }
      if (_nor && _and) 
         {
         fp.fprintf("\nbuf i%-2d(out, 1'b1);");
         return;
         }
      if (_nor)
         {
         if (num_inputs==1)
            fp.fprintf("\nnot i%-2d(out", instance++);
         else
            fp.fprintf("\nnor i%-2d(out", instance++);
         for (k=0; k<num_inputs; k++)
            if ((var[0]>>(k+16)) &1)
               ;
            else 
               fp.fprintf(",%c",'a'+k);
         fp.fprintf(");");
         return;
         }
      if (_and)
         {
         if (num_inputs==1)
            fp.fprintf("\nbuf i%-2d(out", instance++);
         else
            fp.fprintf("\nand i%-2d(out", instance++);
         for (k=0; k<num_inputs; k++)
            if ((var[0]>>(k+16)) &1)
               ;
            else 
               fp.fprintf(",%c",'a'+k);
         fp.fprintf(");");
         return;
         }
      }
   if (var.size() ==0)
      {
      fp.fprintf("\nbuf i%-2d(out, 1'b0);");
      return;
      }

   if (var.size() <= num_inputs)
      {
      bool _nand=true, _or=true;

      for (i=0; i<var.size(); i++)
         {
         int count=num_inputs;
         for (k=0; k<num_inputs; k++)
            {
            if ((var[i]>>(k+16)) &1)
               count--;
            else if ((var[i]>>k)&1)
               _nand = false;
            else
               _or = false;
            }
         if (count!=1)
            _or = _nand = false;
         }
      if (_nand && _or) 
         {
         fp.fprintf("\n// ERROR. You have a circuit which is vacuous in all variables.");
         printf("\nYou have a circuit which is vacuous in all variables.");
         }
      if (_or)
         {
         fp.fprintf("\nor i%-2d(out", instance++);
         for (i=0; i<var.size(); i++)
            for (k=0; k<num_inputs; k++)
               if ((var[i]>>(k+16)) &1)
                  ;
               else 
                  fp.fprintf(",%c",'a'+k);
         fp.fprintf(");");
         return;
         }
      if (_nand)
         {
         fp.fprintf("\nnand i%-2d(out", instance++);
         for (i=0; i<var.size(); i++)
            for (k=0; k<num_inputs; k++)
               if ((var[i]>>(k+16)) &1)
                  ;
               else 
                  fp.fprintf(",%c",'a'+k);
         fp.fprintf(");");
         return;
         }
      }

   // first find out if I'll need complements of any of the inputs
   for (i=0; i<var.size(); i++)
      {
      for (k=0; k<num_inputs; k++)
         {
         if ((var[i]>>(k+16)) &1)
            ;
         else if ((var[i]>>k)&1)
            ;
         else
            inv[k] = true;
         }
      }
   for (i=0; i<num_inputs; i++)
      {
      if (inv[i])
         fp.fprintf("\nnot i%-2d(%c_n, %c);", instance++, 'a'+i, 'a'+i);
      }
   if (var.size()==0) FATAL_ERROR;
   // build list of products
   for (i=0; i<var.size(); i++)
      {
      unsigned v = var[i];
      int count=0;
      for (k=0; k<num_inputs; k++)
         if ((v>>(k+16))&1)
            ;
         else
            count++;
      if (count==0) FATAL_ERROR;
      if (count==1)
         fp.fprintf( "\nbuf i%-2d(pla%d",instance++, i);
      else
         fp.fprintf( "\nand i%-2d(pla%d",instance++, i);
      for (k=0; k<num_inputs; k++)
         {
         if ((v>>(k+16))&1)
            ;
         else if ((v>>k) &1)
            fp.fprintf(", %c",'a'+k);
         else
            fp.fprintf(", %c_n",'a'+k);
         }
      fp.fprintf(");");
      }
   // now build final nor gate
   fp.fprintf( "\n%s i%-2d(out",var.size()==1 ? "buf" : "or", instance++);
   for (i=0; i<var.size(); i++)
      fp.fprintf(", pla%d",i);
   fp.fprintf( ");");
   }


void udptype::WriteCombinationalPrimitive(filetype &fp, int uu, const char *unique, bool atpg0, bool atpg1, bool assertionCheck) const
   {
   TAtype ta("udptype::WritePrimitive");
   int i, k;
   int instance=0;
 
   fp.fprintf("\n\nmodule mdp_%03d%s (out", uu, unique);
   for (i=0; i<num_inputs; i++)
      fp.fprintf(",%c",'a'+i);
   fp.fprintf(");\noutput out;");
   if (num_inputs<1)
      {
      fp.fprintf("\nbuf i0(out, ");
      if (xtable.GetBit(0))
         fp.fprintf( "1'bx);");
      else if (table.GetBit(0))
         fp.fprintf( "1'b1);");
      else
         fp.fprintf( "1'b0);");
      fp.fprintf("\nendmodule");
      return;
      }
   fp.fprintf("\ninput a");
   for (i=1; i<num_inputs; i++)
      fp.fprintf(", %c", i+'a');
   fp.fprintf(";");
   if (atpg0)
      WriteBool(fp, "out", instance, (table & ~xtable), num_inputs);
   else if (atpg1)
      WriteBool(fp, "out", instance, (table |  xtable), num_inputs);
   else 
      {
      // do a quick check to see if we have a constant 0/1. This is needed to work around a NC verilog bug
      bool one=true, zero=true;
      for (i= (1<<num_inputs)-1; i>=0; i--)
         {
         if (xtable.GetBit(i))
            one = zero = false;
         if (table.GetBit(i))
            zero = false;
         else
            one = false;
         }
      if (one)
         fp.fprintf("\nbuf i0(out, 1'b1);");
      if (zero)
         fp.fprintf("\nbuf i0(out, 1'b0);");
      if (one || zero)
         {
         fp.fprintf("\nendmodule");
         return;
         }
      for (i= (1<<num_inputs)-1; i>=0; i--)
         if (xtable.GetBit(i))
            break;
      if (i>=0 && assertionCheck)
         { // I'm going to put a wrapper around the udp illegal assertion check
         fp.fprintf("\nreg oneshot;\n\ninitial oneshot <= 1'b0;");
         fp.fprintf("\n\nudp_%03d%s i0 (out", uu, unique);
         for (i=0; i<num_inputs; i++)
            fp.fprintf(",%c",'a'+i);
         fp.fprintf(");");
         fp.fprintf("\n\nalways @(out)\n  if (out===1'bx && ~oneshot) begin\n    #0.250;\n    if (out===1'bx");
         for (i=0; i<num_inputs; i++)
            fp.fprintf(" && %c!==1'bx",'a'+i);
         fp.fprintf(") begin\n      $display(\"**********Illegal circuit condition at %cm!!\");\n      oneshot <= 1'b1;\n      end\n    end",'%');
         }
      else
         {
         fp.fprintf( "\n\nudp_%03d%s i0(out",uu,unique);
         for (i=0; i<num_inputs; i++)
            fp.fprintf(", %c", i+'a');
         fp.fprintf(");");
         }
      }
   fp.fprintf("\nendmodule");

   array <unsigned> var0, var1;  // high 16 bits will represent no variable
   int val;
     
   if (atpg0 || atpg1)
      fp.fprintf("\n\n/*primitive udp_%03d%s (out", uu, unique);
   else
      fp.fprintf("\n\nprimitive udp_%03d%s (out", uu, unique);
   for (i=0; i<num_inputs; i++)
      fp.fprintf(",%c",'a'+i);
   fp.fprintf(");\noutput out;\ninput a");
   for (i=1; i<num_inputs; i++)
      fp.fprintf(", %c",i+'a');
   fp.fprintf(";\ntable");

   MinSOP(~(table | xtable), var0, num_inputs);  // compute minimum sum of products
   MinSOP((table & ~xtable), var1, num_inputs);
   
   bool emptyTable=true;
   for (val=0; val<=1; val++)
      {
      array <unsigned> var = (val==0 ? var0 : var1);
      for (i=0; i<var.size(); i++)
         {
         char ch;
         fp.fprintf("\n\t");
         for (k=0; k<num_inputs; k++)
            {
            if ((var[i]>>(k+16)) &1)
               ch = '?';
            else
               ch = (var[i]>>k)&1 ? '1' : '0';
            fp.fprintf("%c",ch);
            }
         fp.fprintf(" : %d ;",val);
         emptyTable = false;
         }
      }   
   if (emptyTable)
      { // verilog complains if I have an empty table.
      fp.fprintf("\n\t");
      for (k=0; k<num_inputs; k++)
         fp.fprintf("?");
      fp.fprintf(" : x ;");
      }
   fp.fprintf("\nendtable\nendprimitive\n");
   if (atpg0 || atpg1)
      fp.fprintf("\n*/");
   }


  
void udptype::WriteSequentialPrimitive(filetype &fp, int uu, const char *unique, bool atpg0, bool atpg1, bool assertionCheck) const
   {
   TAtype ta("udptype::WritePrimitive");
   int i, iteration;
      
   if (num_inputs + num_states >=32)
      {
      fp.fprintf("\n// ERROR. This udp has too many inputs/states for me to handle.");
      printf("\nI have a S/D region with too many inputs(%d). It will be ignored.",num_inputs);
      return;
      }

   if (DEBUG)
      {
      fp.fprintf("/*\n");
      for (iteration=0; iteration<(1<<(num_inputs+num_states)); iteration++)
         {
         fp.fprintf("\n\t");
         for (i=0; i<num_inputs; i++)
            {
            fp.fprintf("%3d  ",(iteration>>i)&1);
            }
         fp.fprintf(" : ");
         for (i=0; i<num_states; i++)
            fp.fprintf("%c",((iteration>>(num_inputs+i))&1) ? '1' : '0');
         fp.fprintf("  : ");
         for (i=0; i<num_states; i++)
            fp.fprintf("%c",qx[i].GetBit(iteration) ? 'x' : (qt[i].GetBit(iteration) ? '1' : '0'));
         fp.fprintf(" ;");
         }
      fp.fprintf("*/\n");
      }

   array<bool> legit(1<<num_states, false);
   
   for (iteration=0; iteration<(1<<(num_inputs+num_states)); iteration++)
      {
      int nextstate = 0;
      for (i=0; i<num_states; i++)
         {
         if (nextstate>=0 && !qx[i].GetBit(iteration))
            nextstate += qt[i].GetBit(iteration) ? (1<<i) : 0;
         else
            nextstate = -1;
         }

      if (iteration>>num_inputs==nextstate)
         legit[iteration>>num_inputs] = true;
      }


   // I first want to go through and see if this truth table is really combinational
   // basicly if the next state has no dependency on current state then this is really combinational
   bool reallyCombinational = true;
   for (iteration=0; iteration<(1<<num_inputs); iteration++)
      {
      int s=-1;
      bool stable=false, unstable=false;
      
      for (int k=0; k<(1<<num_states); k++)
         {
         int nextstate = 0;
         for (i=0; i<num_states; i++)
            {
            if (nextstate>=0 && !qx[i].GetBit(iteration | (k<<num_inputs)))
               nextstate += qt[i].GetBit(iteration | (k<<num_inputs)) ? (1<<i) : 0;
            else
               nextstate = -1;
            }
         if (s<0)
            s = nextstate;
         else if (s != nextstate && s>=0 && nextstate>=0)
            reallyCombinational = false;

         if (k == nextstate) stable = true;
         if (legit[k] && nextstate<0) unstable = true;
         }
      // I also want to look for a condition where a stable state exists for only 1 input combinations
      if (stable && unstable)
         reallyCombinational = false;
      }
   

   if (reallyCombinational)
      {
      WriteCombinationalPrimitive(fp, uu, unique, atpg0, atpg1, assertionCheck);
      return;
      }

   if (num_inputs+num_states>16)
      {
      fp.fprintf("\n// ERROR. This udp has too many inputs/states for me to handle.");
      printf("\nI have a sequential S/D region with too many inputs(%d). It will be ignored.",num_inputs);
      return;
      }


   // compute signature for this udp
/*   char sigmem[64];
   memset(sigmem, 0, sizeof(sigmem));
   memcpy(sigmem[0], &num_inputs, sizeof(num_inputs));
   memcpy(sigmem[4], &num_states, sizeof(num_states));
   memcpy(sigmem[8], &qt.arr[0], sizeof(num_states));
  
   fp.fprintf("\n\n udp signature -> %X", signature);
*/

   fp.fprintf("\nmodule mdp_%03d%s (out", uu, unique);   
   for (i=0; i<num_inputs; i++)
      fp.fprintf(",%c",'a'+i);
   fp.fprintf(");\noutput out;");
   if (num_inputs<1)
      {
      fp.fprintf("\nbuf i0(out, ");
      if (xtable.GetBit(0))
         fp.fprintf( "1'bx);");
      else if (table.GetBit(0))
         fp.fprintf( "1'b1);");
      else
         fp.fprintf( "1'b0);");
      fp.fprintf("\nendmodule");
      return;
      }
   fp.fprintf("\ninput a");
   for (i=1; i<num_inputs; i++)
      fp.fprintf(", %c", i+'a');
   fp.fprintf(";");

   fp.fprintf( "\n\nudp_%03d%s #(0.03) i0(out",uu,unique);
   for (i=0; i<num_inputs; i++)
      fp.fprintf(", %c", i+'a');
   fp.fprintf(");\nendmodule");


   fp.fprintf("\n\nprimitive udp_%03d%s (out", uu, unique);
   for (i=0; i<num_inputs; i++)
      fp.fprintf(",%c",'a'+i);
   fp.fprintf(");\noutput out; \nreg out;");
   fp.fprintf("\ninput a");
   for (i=1; i<num_inputs; i++)
      fp.fprintf(", %c",i+'a');
   if (assertionCheck)
      fp.fprintf(";\ninitial out = 1'b0");
   fp.fprintf(";\ntable");

// now try to compress the table, and resolve the number of Q states needed
  
   int total_states=0;
   for (iteration=0; iteration<(1<<(num_states)); iteration++)
      {
      if (legit[iteration])
         total_states++;
      }
   int qvar=-1;      
   for (i=0; i<num_states; i++)
      {
      if (qx[i] == xtable && qt[i] == table)
         qvar = i;         
      }
   if (qvar <0)
      {
      fp.fprintf("\n// ERROR. I have a output which isn't a state variable in this sequential circuit.");
      printf("\nI have a output which isn't a state variable in this sequential circuit.");
      return;
//      FATAL_ERROR;   // sequential outputs should always be a state variable.
      }   
   
   
   if (total_states==3)    // I may be able to represent this state diagram as a edge triggered udp, or maybe not
      {// i want to make a trial assumption that a given state is precharge and try to eliminate it with edge triggers
      array <int> lowstate(num_inputs,-1);
      array <int> highstate(num_inputs,-1);
      
      for (iteration=0; iteration<(1<<(num_inputs+num_states)); iteration++)
         {
         int nextstate = 0;
         for (i=0; i<num_states; i++)
            {
            if (nextstate>=0 && !qx[i].GetBit(iteration))
               nextstate += qt[i].GetBit(iteration) ? (1<<i) : 0;
            else
               nextstate = -1;
            }
         for (i=0; i<num_inputs; i++)
            {
            if ((iteration>>i) &1)
               highstate[i] = (highstate[i]==-1 || highstate[i]==nextstate) ? nextstate : -2;
            else
               lowstate[i]  = (lowstate[i]==-1  || lowstate[i]==nextstate)  ? nextstate : -2;
            }
         }
      int clk=-1, precharge;
      bool clkassertion;  // false for negedge, true for posedge
      for (i=0; i<num_inputs; i++)
         {
         if (highstate[i]>=0)
            {
            precharge = highstate[i];
            clk = i;
            clkassertion=false;
            }
         if (lowstate[i]>=0)
            {
            precharge = lowstate[i];
            clk = i;
            clkassertion=true;
            }
         }
      if (clk<0)
         {
//         fp.fprintf("\n// ERROR. I couldn't convert your 3 state udp into an edge triggered udp.");
//         printf("\nI couldn't convert your 3 state udp into an edge triggered udp.");
//         return;
         }
      else{

         // I now know which input will be edge triggered. Now do minimum SOP to remaining variables. Also, compress to 2 states
         array <unsigned> var0, var1;
         for (iteration=0; iteration<(1<<num_inputs); iteration++)
            {
            if (!qx[qvar].GetBit(iteration | (precharge<<num_inputs)) && ((iteration>>clk)&1) == clkassertion)
               {
               if (!qt[qvar].GetBit(iteration | (precharge<<num_inputs)))
                  var0.push(iteration);
               else
                  var1.push(iteration);
               }
            }
         MinSOP(var0);  // compute minimum sum of products
         MinSOP(var1);
         
         char ch;
         int k;
         for (int val=0; val<=1; val++)
            {
            array <unsigned> var = (val==0 ? var0 : var1);
            for (i=0; i<var.size(); i++)
               {
               fp.fprintf("\n\t");
               for (k=0; k<num_inputs; k++)
                  {
                  if ((var[i]>>(k+16)) &1)
                     ch = '?';
                  else
                     ch = (var[i]>>k)&1 ? '1' : '0';
                  if (k==clk && !clkassertion) ch = 'f';
                  if (k==clk && clkassertion)  ch = 'r';
                  fp.fprintf("%c",ch);
                  }
               fp.fprintf(" : ?");
               fp.fprintf(" : %d ;",val);
               }
            }
         // put in precharge clk edge
         fp.fprintf("\n\t");
         for (k=0; k<num_inputs; k++)
            {
            ch = '?';
            if (k==clk && !clkassertion) ch = 'r';
            if (k==clk && clkassertion)  ch = 'f';
            fp.fprintf("%c",ch);
            }
         fp.fprintf(" : ?",ch);
         fp.fprintf(" : %d ;",(precharge>>qvar)&1);
         // put in precharge clk level state(fixes some rare X issues if clock never gets applied)
         fp.fprintf("\n\t");
         for (k=0; k<num_inputs; k++)
            {
            ch = '?';
            if (k==clk && !clkassertion) ch = '1';
            if (k==clk && clkassertion)  ch = '0';
            fp.fprintf("%c",ch);
            }
         fp.fprintf(" : ?",ch);
         fp.fprintf(" : %d ;",(precharge>>qvar)&1);
         // put in other node edges
         for (i=0; i<num_inputs; i++)
            {
            if (i!=clk)
               {
               fp.fprintf("\n\t");
               for (k=0; k<num_inputs; k++)
                  {
                  ch = '?';
                  if (k==i) ch = '*';
                  fp.fprintf("%c",ch);
                  }
               fp.fprintf(" : ?",ch);
               fp.fprintf(" : - ;");
               }
            }
         fp.fprintf("\nendtable\nendprimitive\n");
         return;
         }
      }

   if (total_states <=2)
      {    // I have a purely level sensitive udp
      if (legit[0] || legit[3])
         {
         fp.fprintf("\n// ERROR. sequential 2-state udp has strange states.");
         printf("\nI have a sequential 2-state udp with strange states.");
         return;
         }
      // now make a var0,var1 list which I will compute a minimum sum of products on
      // I'll make qvar be look like another variable in these lists
      array <unsigned> var0, var1;
      bitfieldtype <max_input_space> input0, input1;
      for (i=0; i< 1<<num_states; i++)
         {
         for (iteration=0; iteration<(1<<num_inputs) && legit[i]; iteration++)
            {
            if (!qx[qvar].GetBit(iteration | (i<<num_inputs)))
               {
               unsigned temp = iteration;

               if ((i>>qvar) &1)
                  temp |= 1<<num_inputs;
               if (!qt[qvar].GetBit(iteration | (i<<num_inputs)))
                  input0.SetBit(temp);
               else
                  input1.SetBit(temp);
               }
            }
         }

      MinSOP(input0, var0, num_inputs+1);  // compute minimum sum of products
      MinSOP(input1, var1, num_inputs+1);
      
      for (int val=0; val<=1; val++)
         {
         array <unsigned> var = (val==0 ? var0 : var1);
         for (i=0; i<var.size(); i++)
            {
            char ch;
            int k;
            fp.fprintf("\n\t");
            for (k=0; k<num_inputs; k++)
               {
               if ((var[i]>>(k+16)) &1)
                  ch = '?';
               else
                  ch = (var[i]>>k)&1 ? '1' : '0';
               fp.fprintf("%c",ch);
               }
            if ((var[i]>>(num_inputs+16)) &1)
               ch = '?';
            else
               ch = (var[i]>>num_inputs)&1 ? '1' : '0';
            fp.fprintf(" : %c",ch);
            fp.fprintf(" : %d ;",val);
            }
         }   
      fp.fprintf("\nendtable\nendprimitive\n");
      return;
      }
   else {
      // now make a var0,var1 list which I will compute a minimum sum of products on
      // I'll make qvar be look like another variable in these lists
      // I'll also make the output be conservative with respect to X's
      // one problem I have is that I correct detect the metastable state on cross coupled nands
      // I'm going to ignore this if it requires the change of more than one input simultaneously
      fp.fprintf("\n// INFO. I'm modelling this %d state element as a simple latch. Probably is cross coupled nands",total_states);
      array <unsigned> var0, var1;
      bitfieldtype <max_input_space> input0, input1, inputx;
      for (i=0; i< 1<<num_states; i++)
         {
         for (iteration=0; iteration<(1<<num_inputs) && legit[i]; iteration++)
            {
            if (!qx[qvar].GetBit(iteration | (i<<num_inputs)))
               {
               unsigned temp = iteration;

               if ((i>>qvar) &1)
                  temp |= 1<<num_inputs;
               if (!qt[qvar].GetBit(iteration | (i<<num_inputs)))
                  input0.SetBit(temp);
               else
                  input1.SetBit(temp);
               }
            else {
               // I found an X condition. I'm not going to report this if 
               // I can prove that the input state can't be reached by changing a single input
               int k, m, next=0;
               bool unreachable=true;
               for (k=1; k<(1<<(num_inputs+num_states)); k=k<<1)
                  {
                  for (m=num_states-1; m>=0; m--)
                     {
                     next = next*2 + qt[m].GetBit(iteration ^ k);
                     }
                  if (next == i) unreachable=false;
                  }

               unsigned temp = iteration;

               if ((i>>qvar) &1)
                  temp |= 1<<num_inputs;
               if (!unreachable)
                  inputx.SetBit(temp);
               }
            }
         }
      for (i=0; i<(1<<(num_inputs+1)); i++)
         {
         if ((input0.GetBit(i) && input1.GetBit(i)) || inputx.GetBit(i))
            {
            input0.ClearBit(i);
            input1.ClearBit(i);
            }
         }

      MinSOP(input0, var0, num_inputs+1);  // compute minimum sum of products
      MinSOP(input1, var1, num_inputs+1);
      
      for (int val=0; val<=1; val++)
         {
         array <unsigned> var = (val==0 ? var0 : var1);
         for (i=0; i<var.size(); i++)
            {
            char ch;
            int k;
            fp.fprintf("\n\t");
            for (k=0; k<num_inputs; k++)
               {
               if ((var[i]>>(k+16)) &1)
                  ch = '?';
               else
                  ch = (var[i]>>k)&1 ? '1' : '0';
               fp.fprintf("%c",ch);
               }
            if ((var[i]>>(num_inputs+16)) &1)
               ch = '?';
            else
               ch = (var[i]>>num_inputs)&1 ? '1' : '0';
            fp.fprintf(" : %c",ch);
            fp.fprintf(" : %d ;",val);
            }
         }   
      fp.fprintf("\nendtable\nendprimitive\n");
      return;
      }
   }


void tabletype::SequentialFunc(const int threadnum)
   {
   TAtype ta("tabletype::SequentialFunc()");
   int i, oo, iteration;
   char buffer[512];

/*   if (states.size() >2)
      {
      fp->fprintf("\n// ERROR. This udp has too many states for me to handle.\n// ERROR You will have some signals undriven.");
      printf("\nI have a sequential S/D region with too many states(%d). It will be ignored.",states.size());
      return;
      }*/
   
   if (states.size() <2)
      {
      if (threadnum==0){
         fp->fprintf("\n// ERROR. This udp has only one state.\n// ERROR You will have some signals undriven.");
         printf("\nI have a sequential S/D region with only 1 state. It will be ignored.");
         }
      return;
      }
   if (DEBUG && threadnum==0)
      {
      char buf2[512], buf3[512];
      fp->fprintf("\n");
      for (i=0; i<inputs.size(); i++)
         fp->fprintf("\n//input  %2d -> %s", i, nodes[inputs[i]].VName(buffer));
      for (i=0; i<outputs.size(); i++)
         fp->fprintf("\n//output %2d -> %s", i, nodes[outputs[i]].VName(buffer));
      for (i=0; i<states.size(); i++)
         fp->fprintf("\n//states %2d -> %s", i, nodes[states[i]].VName(buffer));
      for (i=0; i<devices.size(); i++)
         fp->fprintf("\n//%s(%s) %-10s %-10s %-10s", devices[i].type==D_NMOS ? "NMOS" : "PMOS", devices[i].name, nodes[devices[i].source].VName(buffer),nodes[devices[i].gate].VName(buf2),nodes[devices[i].drain].VName(buf3));
      }

   if ((1 << (inputs.size() + states.size()) > max_input_space) || (inputs.size() + states.size())>=32)
      {
      if (threadnum==0){
         char buf2[512], buf3[512];
         fp->fprintf("\n// ERROR. This udp has too many inputs for me to handle.");
         for (i=0; i<inputs.size(); i++)
            fp->fprintf("\n//input  %2d -> %s", i, nodes[inputs[i]].VName(buffer));
         for (i=0; i<outputs.size(); i++)
            fp->fprintf("\n//output %2d -> %s", i, nodes[outputs[i]].VName(buffer));
         for (i=0; i<states.size(); i++)
            fp->fprintf("\n//states %2d -> %s", i, nodes[states[i]].VName(buffer));
         for (i=0; i<devices.size(); i++)
            fp->fprintf("\n//%s(%s) %-10s %-10s %-10s", devices[i].type==D_NMOS ? "NMOS" : "PMOS", devices[i].name, nodes[devices[i].source].VName(buffer),nodes[devices[i].gate].VName(buf2),nodes[devices[i].drain].VName(buf3));
         printf("\nI have a sequential S/D region with too many inputs(%d). It will be ignored.",inputs.size());
         }
      return;
      }
   array <udptype> &us = us_threads[threadnum];
   wctype clone;
   if (threadnum!=0)
      clone = *this;
   wctype &c = (threadnum==0) ? *this : clone;
   
   c.Initialize();
   array <valuetype> outs;
   array <valuetype> qq;
   int k=0;
   for (iteration=threadnum; iteration<(1<<(inputs.size()+states.size())); iteration+=totalNumberOfThreads)
      {
      bool drive0 = false, drive1 = false;
      outs.resize(0);
      qq.resize(states.size(), V_X);      
      
      for (int tries=0; tries<states.size(); tries++)
         {
         for (i=0; i<inputs.size(); i++)
            {
            c.nodes[inputs[i]].value    = ((iteration>>i)&1) ? V_ONE : V_ZERO;
            c.nodes[inputs[i]].changed  = true;
            c.nodes[inputs[i]].constant = true;
            c.nodeheap.push(inputs[i], 0);
            }
         
         for (i=0; i<states.size(); i++)
            {
            c.nodes[states[i]].value    = ((iteration>>(inputs.size()+i))&1) ? V_ONE : V_ZERO;
            c.nodes[states[i]].changed  = true;
            c.nodes[states[i]].constant = tries==i;
            c.nodeheap.push(states[i], tries==i ? 0 : 2);
            }
         
         c.Propogate(false, true);
         for (i=0; i<states.size(); i++)
            {
            c.nodes[states[i]].value    = ((iteration>>(inputs.size()+i))&1) ? V_ONE : V_ZERO;
            c.nodes[states[i]].changed  = true;
            c.nodes[states[i]].constant = false;
            c.nodeheap.push(states[i], tries==i ? 0 : 2);
            c.AdjustDevicesOnThisNode(states[i], tries==i ? 0 : 2);
            }
         c.Propogate(false, true);
         for (oo=0; oo<outputs.size(); oo++)
            {
            if (tries == 0)
               {
               outs.push(c.nodes[outputs[oo]].value);
               for (i=0; i<states.size(); i++)
                  qq[i] = c.nodes[states[i]].value;
               }
            else if (outs[oo] != c.nodes[outputs[oo]].value)
               {
               outs[oo] = V_X;
               for (i=0; i<states.size(); i++)
                  if (c.nodes[states[i]].value != qq[i])
                     qq[i] = V_X;
               }
            }
         }
      bool shortCondition = false;
      for (i=VDD+1; i<c.nodes.size(); i++)  // a short on any internal node will make the output an X
         if (c.nodes[i].value == V_ILLEGAL)
            shortCondition = true;

      for (oo=0; oo<outputs.size(); oo++)
         {
         for (i=0; i<states.size(); i++)
            {
            if (qq[i] == V_ZERO && !shortCondition)
               {
               us[oo].qx[i].ClearBit(iteration);
               us[oo].qt[i].ClearBit(iteration);
               }
            else if (qq[i] == V_ONE && !shortCondition)
               {
               us[oo].qx[i].ClearBit(iteration);
               us[oo].qt[i].SetBit(iteration);
               }
            else
               {
               us[oo].qx[i].SetBit(iteration);               
               }
            }
         
         if (outs[oo] == V_ZERO && !shortCondition)
            {
            us[oo].xtable.ClearBit(iteration);
            us[oo].table.ClearBit(iteration);
            }
         else if (outs[oo] == V_ONE && !shortCondition)
            {
            us[oo].xtable.ClearBit(iteration);
            us[oo].table.SetBit(iteration);
            }
         else  // treat floating nodes, X's, or crow bar cases as X's in truth table
            us[oo].xtable.SetBit(iteration);         
         }
      }
   SynchronizeEveryoneHere();
   if (threadnum!=0) return;
   // Now I'm back to a single thread but I need to merge the multiple threads together
   for (i=1; i<totalNumberOfThreads; i++)
      {
      array <udptype> &them = us_threads[i];
      
      for (oo=0; oo<outputs.size(); oo++)
         {
         int k;
         for (k=0; k<us[oo].qx.size(); k++)
            us[oo].qx[k]     |= them[oo].qx[k];
         for (k=0; k<us[oo].qt.size(); k++)
            us[oo].qt[k]     |= them[oo].qt[k];
         us[oo].table  |= them[oo].table;
         us[oo].xtable |= them[oo].xtable;
         }
      }


   for (oo=0; oo<outputs.size(); oo++)
      {
      us[oo].num_inputs = inputs.size();
      us[oo].num_states = states.size();
/*      bool reallyCombinational = true;
      for (iteration=0; iteration<(1<<inputs.size()); iteration++)
         {
         int s=-1;
         for (int k=0; k<(1<<states.size()); k++)
            {
            int nextstate = 0;
            for (i=0; i<states.size(); i++)
               {
               if (nextstate>=0 && !us[oo].qx[i].GetBit(iteration | (k<<inputs.size())))
                  nextstate += us[oo].qt[i].GetBit(iteration | (k<<inputs.size())) ? (1<<i) : 0;
               else
                  nextstate = -1;
               }
            if (s<0)
               s = nextstate;
            else if (s != nextstate && s>=0 && nextstate>=0)
               reallyCombinational = false;
            }
         }
      if (reallyCombinational && assertionCheck)  // for now keep the delay for duke's dll's
//      if (reallyCombinational)  
         us[oo].sequential = false;
*/      
      for (i=udps.size()-1; i>=0; i--)
         {
         if (udps[i] == us[oo]) 
            break;
         }
      if (i<0)
         {
         udps.push(us[oo]);
         i = udps.size()-1;
         }
      fp->fprintf( "\nmdp_%03d%s \tui_%-4d(%s", i, unique, uniqueInstance++, nodes[outputs[oo]].VName(buffer));
      for (i=0; i<inputs.size(); i++)
         fp->fprintf(", %s",nodes[inputs[i]].VName(buffer));
      fp->fprintf(");");
      }
   return;
   }


void tabletype::CombinationalFunc(const int threadnum)
   {
   TAtype ta("tabletype::CombinationalTruthTable()");
   int i, oo;
   char buffer[512];

   if (DEBUG && threadnum==0)
      {
      char buf2[512], buf3[512];
      fp->fprintf("\n");
      for (i=0; i<inputs.size(); i++)
         fp->fprintf("\n//input  %2d -> %s", i, nodes[inputs[i]].VName(buffer));
      for (i=0; i<outputs.size(); i++)
         fp->fprintf("\n//output %2d -> %s", i, nodes[outputs[i]].VName(buffer));
      for (i=0; i<devices.size(); i++)
         fp->fprintf("\n//%s(%s) %-10s %-10s %-10s", devices[i].type==D_NMOS ? "NMOS" : "PMOS", devices[i].name, nodes[devices[i].source].VName(buffer),nodes[devices[i].gate].VName(buf2),nodes[devices[i].drain].VName(buf3));
      }
   if ((1 << inputs.size() >= max_input_space)  || inputs.size()>16)
      {
      if (threadnum==0){
         char buf2[512], buf3[512];
         printf("\nI have a combinational S/D region with too many inputs(%d). It will be ignored.",inputs.size());
         fp->fprintf("\n// ERROR. This udp has too many inputs for me to handle.");
         for (i=0; i<inputs.size(); i++)
            fp->fprintf("\n//input  %2d -> %s", i, nodes[inputs[i]].VName(buffer));
         for (i=0; i<outputs.size(); i++)
            fp->fprintf("\n//output %2d -> %s", i, nodes[outputs[i]].VName(buffer));
         for (i=0; i<devices.size(); i++)
            fp->fprintf("\n//%s(%s) %-10s %-10s %-10s", devices[i].type==D_NMOS ? "NMOS" : "PMOS", devices[i].name, nodes[devices[i].source].VName(buffer),nodes[devices[i].gate].VName(buf2),nodes[devices[i].drain].VName(buf3));
         }
      return;
      }
/*   if (inputs.size()==0)
      {
      fp->fprintf("\n// INFO. This source/drain region will be deleted since it has no inputs.");
      return;
      }
*/   
   array <udptype> &us = us_threads[threadnum];   
   wctype clone;
   if (threadnum!=0)
      clone = *this;
   wctype &c = (threadnum==0) ? *this : clone;
   
   SynchronizeEveryoneHere();

   c.Initialize();
   for (int iteration=threadnum; iteration<(1<<inputs.size()); iteration+=totalNumberOfThreads)
      {
      bool drive0 = false, drive1 = false;
      
      for (i=0; i<inputs.size(); i++)
         {
         c.nodes[inputs[i]].value    = (iteration>>i)&1 ? V_ONE : V_ZERO;
         c.nodes[inputs[i]].changed  = true;
         c.nodes[inputs[i]].constant = true;
         c.nodeheap.push(inputs[i], 1);
         }
      
      c.Propogate(false, true);
      for (oo=0; oo<outputs.size(); oo++)
         {
         int n = outputs[oo];
         if (c.nodes[n].value == V_ZERO)
            {
            us[oo].xtable.ClearBit(iteration);
            us[oo].table.ClearBit(iteration);
            }
         else if (c.nodes[n].value == V_ONE)
            {
            us[oo].xtable.ClearBit(iteration);
            us[oo].table.SetBit(iteration);
            }
         else  // treat floating nodes, X's, or crow bar cases as X's in truth table
            us[oo].xtable.SetBit(iteration);
         }
      }
   SynchronizeEveryoneHere();
   if (threadnum!=0) return;
   // Now I'm back to a single thread but I need to merge the multiple threads together
   for (i=1; i<totalNumberOfThreads; i++)
      {
      const array <udptype> &them = us_threads[i];
      
      for (oo=0; oo<outputs.size(); oo++)
         {
         us[oo].table  |= them[oo].table;
         us[oo].xtable |= them[oo].xtable;
         }
      }

   for (oo=0; oo<outputs.size(); oo++)
      {
      us[oo].num_inputs = inputs.size();
      us[oo].sequential = false;
      if (inputs.size()==0)  // something has been tied off
         {
         fp->fprintf("\nbuf \tui_%-4d(%s, ",uniqueInstance++, nodes[outputs[oo]].VName(buffer));
         if (us[oo].xtable.GetBit(0))
            fp->fprintf( "1'bx);");
         else if (us[oo].table.GetBit(0))
            fp->fprintf( "1'b1);");
         else
            fp->fprintf( "1'b0);");
         }
      else if (inputs.size()==1 && !us[oo].xtable.GetBit(0) && !us[oo].xtable.GetBit(1) && us[oo].table.GetBit(0)!=us[oo].table.GetBit(1))  // either buf or inverter
         {
         if (!us[oo].table.GetBit(0))
            fp->fprintf("\nbuf \tui_%-4d(%s",uniqueInstance++, nodes[outputs[oo]].VName(buffer));
         else
            fp->fprintf("\nnot \tui_%-4d(%s",uniqueInstance++, nodes[outputs[oo]].VName(buffer));
         fp->fprintf(", %s);",nodes[inputs[0]].VName(buffer));
         }
      else
         {
         for (i=udps.size()-1; i>=0; i--)
            {
            if (udps[i] == us[oo]) 
               break;
            }
         if (i<0)
            {
            udps.push(us[oo]);
            i = udps.size()-1;
            }
         fp->fprintf( "\nmdp_%03d%s \tui_%-4d(%s", i, unique, uniqueInstance++, nodes[outputs[oo]].VName(buffer));
         for (i=0; i<inputs.size(); i++)
            fp->fprintf(", %s",nodes[inputs[i]].VName(buffer));
         fp->fprintf(");");
         }
      }
   }

void tabletype::DumpUdps(bool atpg0, bool atpg1)
   {
   TAtype ta("tabletype::DumpUdps()");
   int uu;

   for (uu=0; uu<udps.size(); uu++)
      {
      const udptype &u = udps[uu];
      
      if (u.sequential)
         u.WriteSequentialPrimitive(*fp, uu, unique, atpg0, atpg1, assertionCheck);
      else
         u.WriteCombinationalPrimitive(*fp, uu, unique, atpg0, atpg1, assertionCheck);
      }
   }

struct romtype{
   int bignode;
   array <devicetype *> pu, pd;
   int muxout;
   devicetype *muxd;
   bool operator<(const romtype &right) const
      {
      if (muxout != right.muxout)
         return muxout < right.muxout;
      return bignode < right.bignode;
      }
   };

// This function will look for nodes with many sd connections and try to simplify. This is useful for rf's, rom, zdets, etc
// if in atpg mode I'll add unnecessary buffers which will increase the number of fault sites
// making atpg do a better jobs checking roms
bool tabletype::Simplification(filetype &fp, int &inum, bool atpg)
   {
   TAtype ta("tabletype::Simplification");
   // i want to search for 1 or more nodes with a lot of source drain connections
   int i, k, pass;
   int bignode = -1;
   array <romtype> roms; 
   static int count=0;
   count++;

   for (bignode = VDD+1; bignode < nodes.size(); bignode++)
      {
      array <devicetype *> pd, pu, junk;
      const nodetype &n = nodes[bignode];
      
      for (i=0; i<n.sds.size(); i++)
         {         
         devicetype *start = n.sds[i], *d;
         int currentnode = bignode;

         d = start;
         while (true){
            for (k=states.size()-1; k>=0;  k--)
               if (d->gate == states[k])
                  break;
            if (k>=0) break;
            if (d->source == VDD || d->drain == VDD)
               {
               if (start->type == d->type)
                  pu.push(start);
               break;
               }
            if (d->source == VSS || d->drain == VSS)
               {
               if (start->type == d->type)
                  pd.push(start); 
               break;
               }

            if (d->source == currentnode)
               currentnode = d->drain;
            else
               currentnode = d->source;
            if (nodes[currentnode].sds.size() != 2) 
               {
               junk.push(start);
               break;
               }
            d = nodes[currentnode].sds[0] == d ? nodes[currentnode].sds[1] :  nodes[currentnode].sds[0];
            if (d==start) break; // nothing was accomplished so quit
            }
         }
      if (pu.size() + pd.size() >=6)
         {  // I found a big node. Convert the pull up/down into OR statements
         roms.push(romtype());
         roms.back().bignode = bignode;
         roms.back().pd      = pd;
         roms.back().pu      = pu;
         roms.back().muxd    = NULL;
         roms.back().muxout  = -1;
         if (n.sds.size() == pu.size()+pd.size() +1 && junk.size()==1)
            {   // this is probably a column mux on bignode
            bool notmux = false;
            for (k=0; k<states.size(); k++)
               if (junk[0]->gate == states[k])
                  notmux = true;
               
            if (!notmux)
               {
               roms.back().muxd = junk[0];
               roms.back().muxout = junk[0]->source == bignode ? junk[0]->drain : junk[0]->source;
               }
            }
         }         
      }
   if (roms.size() ==0) return false;

   char buffer[512];
   int rr;
   roms.Sort();

   fp.fprintf( "\n");

   // build big nodes first
   for (rr=0; rr<roms.size(); rr++)
      {
      romtype &r = roms[rr];
      int pass;
      
      for (pass=0; pass<=1; pass++)
         {         
         array <devicetype *> &pdu = pass ? r.pd : r.pu;

            // now put the logic for >=2 high stacks if any
         for (i=0; i<pdu.size(); i++)
            {
            devicetype *d = pdu[i];         
            
            transistortype type = d->type;
            if (d->source > VDD && d->drain > VDD)
               {
               int currentnode = r.bignode;
               fp.fprintf("\n%s i%d(w_%s$%s%d", type==D_NMOS ? "and " : "nor ", inum++, nodes[r.bignode].VNameNoBracket(buffer), pass ? "low":"high", i);
               while (true)
                  {
                  if (d->gate == VDD)
                     fp.fprintf(", 1'b1");
                  else if (d->gate == VSS)
                     fp.fprintf(", 1'b0");
                  else
                     fp.fprintf(", %s",nodes[d->gate].VName(buffer));
                  currentnode = (d->source == currentnode) ? d->drain : d->source;
                  if (currentnode <= VDD) break;
                  if (nodes[currentnode].sds.size()!=2) FATAL_ERROR;
                  d = (nodes[currentnode].sds[0] == d) ? nodes[currentnode].sds[1] :  nodes[currentnode].sds[0];
                  }
               fp.fprintf(");");
               }
            }
         if (atpg && pdu.size()!=0)
            { // I'll make two passes. First 
            inum++;
            int startinum = inum;
            for (i=0; i<pdu.size(); i++)
               {
               devicetype *d = pdu[i];
               
               if (d->source <= VDD || d->drain <= VDD)
                  fp.fprintf("\n%s  i%d(romfake$%d, %s);", d->type==D_NMOS? "buf":"not", inum, inum, nodes[d->gate].VName(buffer));
               else
                  fp.fprintf("\nbuf  i%d(romfake$%d, w_%s$%s%d);", inum, inum, nodes[r.bignode].VNameNoBracket(buffer), pass ? "low":"high", i);
               inum++;
               }
            fp.fprintf("\nor  i%d (w_%s$%s",inum++, nodes[r.bignode].VNameNoBracket(buffer), pass ? "low":"high");
            
            for (i=0; i<pdu.size(); i++)
               {
               fp.fprintf( ", romfake$%d", startinum++);
               if (i%20==19 && i<pdu.size()-1)
                  fp.fprintf("\n");  // don't let any line get too long
               }
            fp.fprintf(");");
            }
         else if (pdu.size()!=0)
            {
            fp.fprintf("\nor  i%d (w_%s$%s", inum++, nodes[r.bignode].VNameNoBracket(buffer), pass ? "low":"high");
            int priorgate = -1;
            for (i=0; i<pdu.size(); i++)
               {
               devicetype *d = pdu[i];
               
               if (d->source <= VDD || d->drain <= VDD)
                  {
                  if (d->gate == VDD)
                     strcpy(buffer,"1'b1");
                  else if (d->gate == VSS)
                     strcpy(buffer,"1'b0");
                  else
                     nodes[d->gate].VName(buffer);
                  
                  if (priorgate != d->gate || i<=1)
                     fp.fprintf(", %s%s",d->type==D_NMOS? "":"~", buffer);
                  priorgate = d->gate;
                  }
               else
                  fp.fprintf(", w_%s$%s%d",nodes[r.bignode].VNameNoBracket(buffer), pass ? "low":"high", i);
               if (i%20==19 && i<pdu.size()-1)
                  fp.fprintf("\n");  // don't let any line get too long
               }
            fp.fprintf(");");
            }
         if (pdu.size()==0) // no pu/pd so tie off this signal
            fp.fprintf("\nbuf i%d (w_%s$%s, 1'b0);", inum++, nodes[r.bignode].VNameNoBracket(buffer), pass ? "low":"high");            
         }
      }

   array <devicetype> newdevices;
   // now build the col mux structure if present
   for (pass=0; pass<=1; pass++)
      {
      int start=0, end;
      for (end=1; end<=roms.size(); end++)
         {
         if ( end ==roms.size() || roms[start].muxout != roms[end].muxout || roms[start].muxout<0)
            {
            if (end - start >1)
               {  // I have a column mux               
               int muxout = roms[start].muxout;
               for (i=start; i<end; i++)
                  {
                  romtype &r = roms[i];
                  char buf2[512], buf3[512];
                  fp.fprintf("\nand i%d (w_%s$%s%d, %s, w_%s$%s);", inum++, nodes[muxout].VNameNoBracket(buffer), pass ? "low":"high", i, nodes[r.muxd->gate].VName(buf2), nodes[r.bignode].VNameNoBracket(buf3), pass ? "low":"high");
                  }
               fp.fprintf("\nor   i%d (w_%s$%s", inum++, nodes[muxout].VNameNoBracket(buffer), pass ? "low":"high");
               for (i=start; i<end; i++)
                  fp.fprintf( ", w_%s$%s%d", nodes[muxout].VNameNoBracket(buffer), pass ? "low":"high", i);
               fp.fprintf(");");               
               }
            else
               roms[start].muxd = NULL;

            int finalnode = roms[start].muxd != NULL ? roms[start].muxout : roms[start].bignode;
            char buf2[512];
            
            newdevices.push(devicetype());
            newdevices.back().type   = D_NMOS;
            newdevices.back().name   = "MUX Simplification device";
            newdevices.back().source = pass ? VSS : VDD;
            newdevices.back().drain  = finalnode;
            newdevices.back().gate   = nodes.size();  // foo$high
            newdevices.back().r      = (float)0.001;   // I 
            newdevices.back().length = 1.0;
            newdevices.back().width  = 100.0;

            inputs.push(nodes.size());            
            sprintf(buffer,"%s$%s", nodes[finalnode].VNameNoBracket(buf2), pass ? "low":"high");
            nodes.push(nodetype(buffer));

            start = end;
            }
         }
      inputs.RemoveDuplicates();
      }

   // now I want to delete unneeded devices. I mark devices for deletion by using on
   for (i=0; i<devices.size(); i++)
      devices[i].on = false;
   for (rr=0; rr<roms.size(); rr++)
      {
      romtype &r = roms[rr];
      if (r.muxd)
         r.muxd->on = true;
      for (pass=0; pass<=1; pass++)
         {
         array <devicetype *> &pdu = pass ? r.pd : r.pu;
         for (i=0; i<pdu.size(); i++)
            {
            devicetype *d = pdu[i];
            int currentnode = r.bignode;
            
            while(true)
               {
               d->on = true;
               if (d->source <= VDD || d->drain <= VDD) break;
               currentnode = d->source == currentnode ? d->drain : d->source;
               d = nodes[currentnode].sds[0] == d ? nodes[currentnode].sds[1] :  nodes[currentnode].sds[0];
               }
            }
         }
      }
   for (i=0; i<devices.size(); i++)
      {
      const devicetype &d = devices[i];
      if (d.on)
         {
         for (k=inputs.size()-1; k>=0; k--)
            if (d.gate == inputs[k])
               break;
         if (k<0)
            { // I eliminated some logic which is using something other than a primary input
            outputs.push(d.gate);
            }
         }
      }
   outputs.RemoveDuplicates();
   for (i=devices.size()-1; i>=0; i--)
      if (devices[i].on)
         devices.RemoveFast(i);
   devices += newdevices;
  
   // now I need to re crossreference
   CrossReference();
   for (i=inputs.size()-1; i>=0; i--)
      {
      if (nodes[inputs[i]].gates.size()==0)
         inputs.RemoveFast(i);
      }
   for (i=internals.size()-1; i>=0; i--)
      {
      if (nodes[internals[i]].gates.size()==0)
         internals.RemoveFast(i);
      }
   for (i=0; i<states.size(); i++)
      {
      if (nodes[states[i]].gates.size()==0 || nodes[states[i]].sds.size()==0)
         {
         fp.fclose();
         FATAL_ERROR;  // I shouldn't have touched any state nodes
         }
      }
   return true;
   }




void wirelisttype::AbstractDevices(filetype &fp, const char *unique, int &inum, bool atpg0, bool atpg1, bool assertionCheck)
   {
   TAtype ta("wirelisttype::AbstractDevices");
   // I want to find all the nets that make up each source/drain region
   connectiontype connect(nodes.size());
   int i, k;   
   array <tabletype> sds;
   array <int> used(nodes.size(), 0);
   
   TAtype ta1("wirelisttype::AbstractDevices S1");
   tabletype::fp = &fp;
   tabletype::unique = unique;
   tabletype::assertionCheck= assertionCheck;


   for (i=0; i<nodes.size(); i++)
      {
      nodes[i].sds.clear();
      nodes[i].gates.clear();
      nodes[i].touched = 0;
      }
   
   for (i=0; i<devices.size(); i++)
      {
      devicetype &d = devices[i];
      if (d.width > 0.0)
         {         
         if (d.source >= 2 && d.drain >= 2)
            connect.Merge(d.source, d.drain);
         
         if (d.source> VDD)
            nodes[d.source].sds.push(&d);
         if (d.drain> VDD)
            nodes[d.drain].sds.push(&d);
         nodes[d.gate].gates.push(&d);
         d.on = true;  // will be marker that this devices has already been found
         }
      }
   
   array <int> s;
   for (i=2; i<nodes.size(); i++)
      {
      if (connect.Handle(i) == i)
         {
         sds.push(tabletype());
         tabletype &sd = sds.back();
         int n = i;

         nodes[VSS].touched = i;
         nodes[VDD].touched = i;
         s.push(i);
         while (s.pop(n))
            {
            nodes[n].touched = i;
            for (k=0; k<nodes[n].sds.size(); k++)
               {
               devicetype &d = *nodes[n].sds[k];               
               if (d.on)
                  {
                  d.on = false;
                  sd.devices.push(d);
                  }               
               if (nodes[d.drain].touched != i)
                  {
                  nodes[d.drain].touched = i;
                  s.push(d.drain);
                  }
               if (nodes[d.source].touched != i)
                  {
                  nodes[d.source].touched = i;
                  s.push(d.source);
                  }
               }
            }

         if (sd.devices.size()==0)
            sds.pop_back();
         }
      }

   // first try to find any state nodes in the sd. This can happen due to cascode structures
   for (i=0; i<sds.size(); i++)
      {
      tabletype &sd = sds[i];
      array <bool> gates(nodes.size(), false);

      for (k=0; k<sd.devices.size(); k++)
         {
         const devicetype &d = sd.devices[k];
         gates[d.gate] = true;

         if (d.gate >1)
            sd.inputs.push(d.gate);
         if (d.drain>1)
            sd.outputs.push(d.drain);
         if (d.source>1)
            sd.outputs.push(d.source);
         }
      for (k=0; k<sd.devices.size(); k++)
         {
         const devicetype &d = sd.devices[k];
         if (gates[d.drain] && d.drain>1)
            sd.states.push(d.drain);
         if (gates[d.source] && d.source>1)
            sd.states.push(d.source);
         }
      sd.states.RemoveDuplicates();      
      sd.inputs.RemoveDuplicates();      
      sd.outputs.RemoveDuplicates();      
      sd.states.Sort();
      sd.inputs.Sort();
      sd.inputs.RemoveCommonElements(sd.states);
      }
   
   for (i=0; i<bristles.size(); i++)
      {
      const bristletype &brist = bristles[i];

      used[brist.nodeindex] = 1;   // can't optimize away bristles
      }
   for (i=0; i<instances.size(); i++)
      {
      const instancetype &inst = instances[i];
      for (k=0; k<inst.outsideNodes.size(); k++)
         used[inst.outsideNodes[k]] = 1;   // can't optimize away nodes that are inputs to other cells
      }

   for (i=0; i<sds.size(); i++)
      {
      tabletype &sd = sds[i];

      for (k=0; k<sd.inputs.size(); k++)
         used[sd.inputs[k]]++;
      for (k=0; k<sd.outputs.size(); k++)
         used[sd.outputs[k]]++;
      }

   for (i=0; i<sds.size(); i++)
      {
      tabletype &sd = sds[i];

      for (k=sd.outputs.size()-1; k>=0; k--)
         {
         int n = sd.outputs[k];
         if (used[n] <=0) FATAL_ERROR;
         if (used[n] ==1)
            {
            sd.outputs.RemoveFast(k);    // this node is only internal stack node.
            sd.internals.push(n);
            }
         }
      sd.inputs.Sort();
      sd.outputs.Sort();
      }

   // now I want to look for cross coupled sd regions
   for (i=0; i<sds.size(); i++)
      {
      tabletype &sd1 = sds[i];
      array <int> common1, common2, share;

      for (k=i+1; k<sds.size(); k++)
         {
         tabletype &sd2 = sds[k];
         common1.clear();
         common2.clear();
         share.clear();

         sd1.inputs.CommonContents(common1, sd2.outputs);
         sd2.inputs.CommonContents(common2, sd1.outputs);
         
         if (common1.size() && common2.size())
            {  // I found cross coupled region
            int j;

            sd1.inputs.CommonContents(share, sd2.inputs);
            for (j=0; j<share.size(); j++)
               used[share[j]]--;
            share.clear();
            sd1.outputs.CommonContents(share, sd2.outputs);
            for (j=0; j<share.size(); j++)
               used[share[j]]--;
            sd1.inputs   += sd2.inputs;
            sd1.outputs  += sd2.outputs;
            sd1.states   += sd2.states;
            sd1.internals+= sd2.internals;
            sd1.devices  += sd2.devices;

            sd1.states += common1;
            sd1.states += common2;
            sd1.states.RemoveDuplicates();
            sd1.states.Sort();
            sd1.inputs.RemoveDuplicates();
            sd1.inputs.Sort();
            sd1.inputs.RemoveCommonElements(sd1.states);
            sd1.inputs.Sort();

            for (j=common1.size()-1; j>=0; j--)
               {
               used[common1[j]]--;
               if (used[common1[j]] >1)
                  common1.RemoveFast(j);
               }

            for (j=common2.size()-1; j>=0; j--)
               {
               used[common2[j]]--;
               if (used[common2[j]] >1)
                  common2.RemoveFast(j);
               }
            sd1.internals += common1;
            sd1.internals += common2;
            sd1.internals.Sort();            
            sd1.outputs.Sort();
            share.clear();
            sd1.outputs.CommonContents(share, sd2.internals);
            for (j=0; j<share.size(); j++)
               used[share[j]]--;
            sd1.outputs.RemoveCommonElements(sd1.internals);
            sd1.outputs.Sort();
            
            sds.RemoveFast(k);
            k = i;
            }
         }
      }

   for (i=0; i<sds.size(); i++)
      {
      if (sds[i].states.size()==1)
         sds[i].states.clear();
      }
   ta1.Destruct();
   TAtype ta2("wirelisttype::AbstractDevices S2");

   // Now I want to try to merge sd regions where I don't increase the # of inputs or outputs
   sds.Sort();  // order with state -> no state, input side ascending

   // I want to merge logic where I don't increase the number of inputs or outputs
   // I also allow a block to be merged with another block multiple times, this duplication of
   // logic actually reduces the problem size since it typically happens with inverters.
   // Sequential logic is substantially reduced when the clk_n inverter is part of the cell.
   // For master/slave configuration I'll need it to be in both pieces.
   bool done;
   do{
      done = true;
      for (i=0; i<sds.size(); i++)
         {
         tabletype &sd1 = sds[i];
         array <int> common, shared, ioverlap;
         
         for (k=0; k<sds.size(); k++)
            {
            tabletype &sd2 = sds[k];
            common.clear();
            shared.clear();
            
            sd1.inputs.CommonContents(common, sd2.outputs);
            if (sd2.states.size()==0 && common.size() && i!=k)
               { // now figure out if a merge is a net win
               shared = sd1.inputs;
               shared += sd2.inputs;
               shared.RemoveDuplicates();
               int igain = shared.size() - common.size() - sd1.inputs.size();
               
               if (igain <= 0)
                  { // first check that we didn't create any loops
                  sd1.internals.CommonContents(ioverlap, sd2.internals);
                  if (ioverlap.size()==0)// && igain + sd1.inputs.size() >0)
                     { // a merge would be a net win so do it
                     sd1.inputs.RemoveCommonElements(sd2.outputs);
                     sd1.inputs   += sd2.inputs;
                     sd1.inputs.RemoveDuplicates();
                     sd1.inputs.Sort();
                     
                     sd1.internals+= sd2.internals;
                     sd1.internals+= common;
                     sd1.devices  += sd2.devices;               
                     
                     sd1.internals.Sort();
                     sd1.inputs.RemoveCommonElements(sd1.internals);
                     sd1.inputs.Sort();
                     done = false;
                     }
                  else if (ioverlap.size()!=0)
                     {
//                     printf("\nFound a combinational loop in cell %s, %s",name, nodes[ioverlap[0]].name);
                     }
                  }
               }
            }
         }
      }while (!done);
   
   static int oneshot=false;
   if (oneshot && true)
      {
      oneshot = false;
      array <int> printregions;
      fp.fprintf( "\nCell %s",name);
      for (i=0; i<sds.size(); i++)
         printregions.push(i);
      for (i=0; i<printregions.size(); i++)
         {
         tabletype &sd = sds[printregions[i]];
         char buffer[512];
         
         fp.fprintf("\nSD Region %2d, devices= %d",printregions[i],sd.devices.size());
         fp.fprintf("\nInputs    -> ");
         for (k=0; k<sd.inputs.size(); k++)
            fp.fprintf("%s ",nodes[sd.inputs[k]].VName(buffer));
         fp.fprintf("\nOutputs   -> ");
         for (k=0; k<sd.outputs.size(); k++)
            fp.fprintf("%s ",nodes[sd.outputs[k]].VName(buffer));
         fp.fprintf("\nStates   -> ");
         for (k=0; k<sd.states.size(); k++)
            fp.fprintf("%s ",nodes[sd.states[k]].VName(buffer));
         fp.fprintf("\nInternals-> ");
         for (k=0; k<sd.internals.size(); k++)
            fp.fprintf("%s ",nodes[sd.internals[k]].VName(buffer));
         for (k=0; k<sd.devices.size(); k++)
            fp.fprintf("\n %s(%s) %-10s %-10s %-10s", sd.devices[k].type==D_NMOS ? "NMOS" : "PMOS", devices[i].name, nodes[sd.devices[k].source].name,nodes[sd.devices[k].gate].name,nodes[sd.devices[k].drain].name);
         fp.fprintf("\n\n");
         }
      }

   // now I probably have merged away some sd regions so I can delete them if none of their outputs are needed
   for (i=0; i<used.size(); i++)
      used[i] = 0;
   for (i=0; i<bristles.size(); i++)
      {
      const bristletype &brist = bristles[i];
      used[brist.nodeindex] = 2;   // can't optimize away bristles
      }
   for (i=0; i<instances.size(); i++)
      {
      const instancetype &inst = instances[i];
      for (k=0; k<inst.outsideNodes.size(); k++)
         used[inst.outsideNodes[k]] = 2;   // can't optimize away nodes that are inputs to other cells
      }
   
   for (i=0; i<sds.size(); i++)
      {
      tabletype &sd = sds[i];
      
      for (k=0; k<sd.inputs.size(); k++)
         used[sd.inputs[k]]++;
      for (k=0; k<sd.outputs.size(); k++)
         used[sd.outputs[k]]++;
      }

   do{
      done = true;
      for (i=0; i<sds.size(); i++)
         {
         const tabletype &sd = sds[i];
         bool needed = false;
         
         for (k=0; k<sd.outputs.size(); k++)
            if (used[sd.outputs[k]] >1)
               needed = true;
         if (!needed)
            {
            for (k=0; k<sd.inputs.size(); k++)
               used[sd.inputs[k]]--;
            for (k=0; k<sd.outputs.size(); k++)
               used[sd.outputs[k]]--;
            done = false;
            sds.RemoveFast(i);
            }
         }
      }while (!done);


   if (inum <0)
      {
      printf( "\nCell %s",name);
      for (i=0; i<sds.size(); i++)
         {
         tabletype &sd = sds[i];
         char buf1[512], buf2[512], buf3[512];
         
         printf("\nSD Region %2d, devices= %d",i,sd.devices.size());
         printf("\nInputs    -> ");
         for (k=0; k<sd.inputs.size(); k++)
            printf("%s ",nodes[sd.inputs[k]].VName(buf1));
         printf("\nOutputs   -> ");
         for (k=0; k<sd.outputs.size(); k++)
            printf("%s ",nodes[sd.outputs[k]].VName(buf1));
         printf("\nStates   -> ");
         for (k=0; k<sd.states.size(); k++)
            printf("%s ",nodes[sd.states[k]].VName(buf1));
         printf("\nInternals-> ");
         for (k=0; k<sd.internals.size(); k++)
            printf("%s ",nodes[sd.internals[k]].VName(buf1));
         for (k=0; k<sd.devices.size(); k++)
            printf("\n %s(%s) %-10s %-10s %-10s", sd.devices[k].type==D_NMOS ? "NMOS" : "PMOS", sd.devices[k].name, nodes[sd.devices[k].source].VName(buf1),nodes[sd.devices[k].gate].VName(buf2),nodes[sd.devices[k].drain].VName(buf3));
         printf("\n\n");
         }
      }
   ta2.Destruct();
   TAtype ta3("wirelisttype::AbstractDevices S3");
   // I now have everything grouped into sdtype's. These will directly turn into truth tables

   for (i=0; i<sds.size(); i++)
      {
      tabletype &sd = sds[i];

      sd.nodes = nodes;
      sd.CrossReference();
      sd.MergeDevices();
      
//      if (sd.Simplification(fp, inum, atpg0 | atpg1))  
      if ((sd.inputs.size()>=4 || sd.states.size()>0) && sd.Simplification(fp, inum, false))  
         { // i was able to reduce some large fanin pieces. I may need to repartition what is left however.
         wirelisttype wl(this); // I'm going to make a dummy wirelist which I'll call abstractdevices()

/*         char buf1[512], buf2[512], buf3[512];
         printf("\nAfter simplification************************");
         printf("\nSD Region %2d, devices= %d",i,sd.devices.size());
         printf("\nInputs    -> ");
         for (k=0; k<sd.inputs.size(); k++)
            printf("%s ",sd.nodes[sd.inputs[k]].VName(buf1));
         printf("\nOutputs   -> ");
         for (k=0; k<sd.outputs.size(); k++)
            printf("%s ",sd.nodes[sd.outputs[k]].VName(buf1));
         printf("\nStates   -> ");
         for (k=0; k<sd.states.size(); k++)
            printf("%s ",sd.nodes[sd.states[k]].VName(buf1));
         printf("\nInternals-> ");
         for (k=0; k<sd.internals.size(); k++)
            printf("%s ",sd.nodes[sd.internals[k]].VName(buf1));
         for (k=0; k<sd.devices.size(); k++)
            printf("\n %s %-10s %-10s %-10s", sd.devices[k].type==D_NMOS ? "NMOS" : "PMOS", sd.nodes[sd.devices[k].source].VName(buf1), sd.nodes[sd.devices[k].gate].VName(buf2), sd.nodes[sd.devices[k].drain].VName(buf3));
         printf("*******************************\n\n");
*/
         wl = *this;
         wl.nodes   = sd.nodes;
         wl.devices = sd.devices;
         for (k=0; k<sd.outputs.size(); k++)
            {
            wl.bristles.push(bristletype(sd.nodes[sd.outputs[k]].name, sd.outputs[k]));
            }
         for (k=0; k<wl.nodes.size(); k++)
            {
            wl.nodes[k].gates.clear();
            wl.nodes[k].sds.clear();
            }
         wl.AbstractDevices(fp, unique, inum, atpg0, atpg1, assertionCheck);
         }
      else
         {
		 if (sd.inputs.size()>20 || sd.states.size()>20)
			{
			fp.fprintf("\n// ERROR. This udp has too many states/inputs for me to handle.\n// ERROR You will have some signals undriven.");
			printf("\nI have a S/D region with too many states(%d) or inputs(%d). It will be ignored.",sd.states.size(),sd.inputs.size());
            for (k=0; k<sd.states.size(); k++)
               {
               fp.fprintf("\nState%d->%s",k,sd.nodes[sd.states[k]].name);
               }
            for (k=0; k<sd.inputs.size(); k++)
               {
               fp.fprintf("\nInput%d->%s",k,sd.nodes[sd.inputs[k]].name);
               }
			}
		 else if (sd.states.size() ==0)
            sd.CombinationalTruthTable();
         else
            sd.SequentialTruthTable();
         }
      sd.UnAllocate();
      }
   }


// this will attempt to merge fingered devices. This helps for .net files
void wirelisttype::NMerge()
   {
   FATAL_ERROR;
   }



// this function will change the wirelist to untangle source drain regions. Not needed for sherlock
// but is needed for verilog extraction. Basicly, I look for an inverter being fed into a ctran.
// The inverter output is also used to drive a gate as well. I will transform things into 2 inverters
// I will not do this if the inverter is cross coupled to another inverter
void wirelisttype::ArtisanTransform()
   {
   TAtype ta("wirelisttype::ArtisanTransform()");
   int i, k;

   array <nodetype> temp = nodes;
   bool xrefempty = true;
   int unique=0;
   int count=0;

   for (i=0; i<temp.size(); i++)
      {
      int invgate = -1;
      int *ctran_nptr = NULL;
      int *ctran_pptr = NULL;
      int ctran_node=-1;
      devicetype invn, invp;
      invn.type = D_PMOS;
      invp.type = D_NMOS;
      bool otherstuff=false, crosscoupled=false;

      if (xrefempty)
         {
         xrefempty = false;
         temp = nodes;
         for (k=0; k<devices.size(); k++)
            if (devices[k].width >0.0)
               {
               temp[devices[k].source].sds.push(&devices[k]);
               temp[devices[k].drain].sds.push(&devices[k]);
               temp[devices[k].gate].gates.push(&devices[k]);
               }
         i = 0;
         count++;
         }
      const nodetype &n = temp[i];
	  // n will be the node between the inverter and ctran

      for (k=0; k<n.sds.size(); k++)
         {
         devicetype &d = *n.sds[k];            
         
         if (d.type==D_NMOS)
            {
            if (d.drain==VSS || d.source==VSS)
               {
               if (invgate >=0 && invgate != d.gate)
                  ;
               else
                  {
                  invgate = d.gate;
                  invn = d;
                  }
               }
            else
               {
               int &other = (d.drain==i) ? d.source : d.drain;
               if (ctran_node >=0 && ctran_node != other)
                  otherstuff = true;
               else
                  {
                  ctran_nptr = (d.drain==i) ? &d.drain : &d.source;
                  ctran_node = other;
                  }
               }
            }
         if (d.type==D_PMOS)
            {
            if (d.drain==VDD || d.source==VDD)
               {
               if (invgate >=0 && invgate != d.gate)
                  ;
               else
                  {
                  invgate = d.gate;
                  invp = d;
                  }
               }
            else
               {
               int &other = (d.drain==i) ? d.source : d.drain;
               if (ctran_node >=0 && ctran_node != other)
                  otherstuff = true;
               else
                  {
                  ctran_pptr = (d.drain==i) ? &d.drain : &d.source;
                  ctran_node = other;
                  }
               }
            }
         otherstuff = otherstuff || n.gates.size();
		 }
      for (k=0; k<n.gates.size(); k++)
         {
         devicetype &d = *n.gates[k];
		 crosscoupled = crosscoupled || (d.drain == invgate || d.source == invgate);         
		 }
	  if (ctran_nptr!=NULL && ctran_pptr!=NULL && *ctran_nptr==*ctran_pptr && invn.type==D_NMOS && invp.type==D_PMOS && otherstuff && !crosscoupled)
		 {
		 char buffer[512], buf2[512];
		 nodetype newn = n;
		 newn.sds.clear();
		 newn.gates.clear();
		 strcpy(buffer, newn.name);
		 sprintf(buf2,"$ArtisanTransform%d",unique++);
		 strcat(buffer, buf2);
		 newn.name = stringPool.NewString(buffer);
		 nodes.push(newn);
		 *ctran_pptr = nodes.size()-1;
		 *ctran_nptr = nodes.size()-1;  // ctran now connected to new node
		 if (invn.source == VSS)
			invn.drain  = nodes.size()-1;
		 else
			invn.source = nodes.size()-1;
		 if (invp.source == VDD)
			invp.drain  = nodes.size()-1;
		 else
			invp.source = nodes.size()-1;  // make new inverter connected to new node (going to ctran)
		 devices.push(invn);
		 devices.push(invp);
		 //            printf("\nPerforming Artisan transform on node %s in cell %s",n.name, name);
		 xrefempty = true;  // this is needed because device push might have caused a reallocation
//		 break;
		 }
      }
   }



// This looks for two series devices with the same gate and channel length. I will merge them into 1 device
void wirelisttype::MergeStackedTransform(filetype &fp)
   {
   TAtype ta("wirelisttype::MergeStackedTransform()");
   int i;
   
   for (i=0; i<nodes.size(); i++)
      {
      nodes[i].sds.clear();
      nodes[i].gates.clear();
      }
   // remove tied off devices. This will eliminate diodes in input pad receivers.
   for (i=devices.size()-1; i>=0; i--)
      {
      devicetype &d = devices[i];
      if (d.drain>VDD)
         nodes[d.drain].sds.push(&d);
      if (d.source>VDD)
         nodes[d.source].sds.push(&d);
      if (d.gate>VDD)
         nodes[d.gate].gates.push(&d);
      }
   
   for (i=VDD+1; i<nodes.size(); i++)
      {
      nodetype &n = nodes[i];

      if (n.sds.size()==2 && n.gates.size()==0)
         {
         devicetype &d1 = *n.sds[0];
         devicetype &d2 = *n.sds[1];

         int top = d1.drain==i ? d1.source : d1.drain;
         int bot = d2.drain==i ? d2.source : d2.drain;
         int mid = i;
         
         if (d1.type==d2.type && d1.length==d2.length && d1.gate==d2.gate && top!=bot && top!=mid)
            {
            fp.fprintf("\n//INFO: Merging devices %s and %s", d1.name, d2.name);
            d1.width = 1.0/(1.0/d1.width + 1.0/d2.width);
            d1.source = top;
            d1.drain  = bot;
            d2.gate   = VSS;
            d2.source = VSS;
            d2.drain  = VSS;  // this effectively deletes the other device
            }
         }
      }
   for (i=0; i<nodes.size(); i++)
      {
      nodes[i].sds.clear();
      nodes[i].gates.clear();
      }
   }


// this looks for a common device shared between two pulldowns. I untangle these devices
// also look for a pdevice shorting bitlines while other p device pull high   
void wirelisttype::CustomRFHACKTransform()
   {
   TAtype ta("wirelisttype::CustomRFHACKTransform()");
   int i, k;

   array <nodetype> temp = nodes;
   array <devicetype> newdevices;
   bool xrefempty = true;
   int unique=0;
   int count=0;

   temp = nodes;
   for (k=0; k<devices.size(); k++)
      if (devices[k].width >0.0)
         {
         temp[devices[k].source].sds.push(&devices[k]);
         temp[devices[k].drain].sds.push(&devices[k]);
         temp[devices[k].gate].gates.push(&devices[k]);
         }
   for (i=0; i<temp.size(); i++)
      {
      const nodetype &n = temp[i];

      if (n.gates.size()==0 && n.sds.size()==3)
         { // possible candidate
         bool quit=false;
         int n0 = (n.sds[0]->source == i) ? n.sds[0]->drain : n.sds[0]->source;
         int n1 = (n.sds[1]->source == i) ? n.sds[1]->drain : n.sds[1]->source;
         int n2 = (n.sds[2]->source == i) ? n.sds[2]->drain : n.sds[2]->source;
         devicetype *d;
         
         if (n0 == VSS)
            {n0 = n2; d=n.sds[0];}
         else if (n1 == VSS)
            {n1 = n2; d=n.sds[1];}
         else if (n2 == VSS)
            d = n.sds[2];
         else
            quit = true;
         if (!quit && temp[n0].sds.size() >= 16 && temp[n1].sds.size() >= 16)
            {
            nodetype newn = n;
            devicetype newd = *d;
            char buffer[256], buf2[256];
            newn.sds.clear();
            newn.gates.clear();
            strcpy(buffer, newn.name);
            sprintf(buf2,"$RFHACKTransform%d",unique++);
            strcat(buffer, buf2);
            newn.name = stringPool.NewString(buffer);
            nodes.push(newn);

            if (newd.source == VSS)
               newd.drain = nodes.size()-1;
            else if (newd.drain == VSS)
               newd.source = nodes.size()-1;
            else FATAL_ERROR;

            d = n.sds[0];
            if (d->source == i)
               d->source = nodes.size()-1;
            else if (d->drain == i)
               d->drain = nodes.size()-1;
            else
               FATAL_ERROR;

            newdevices.push(newd);  // I can't add to devices because it could cause a reallocation invalidating pointers            
            count++;
            }
         }      
      }

   temp = nodes;
   for (k=0; k<devices.size(); k++)
      if (devices[k].width >0.0)
         {
         temp[devices[k].source].sds.push(&devices[k]);
         temp[devices[k].drain].sds.push(&devices[k]);
         temp[devices[k].gate].gates.push(&devices[k]);
         }

   devices += newdevices;
   for (i=0; i<devices.size(); i++)
      {
      devicetype d = devices[i];
      devicetype *pch;
      bool founds=false, foundd=false;

      if (d.type == D_PMOS && d.source > VDD && d.drain > VDD && temp[d.source].sds.size() >= 16 && temp[d.drain].sds.size() >= 16)
         { // potential equalizing device across 2 bit lines
         for (k=0; k<temp[d.source].sds.size(); k++)
            {
            pch = temp[d.source].sds[k];
            if (pch->gate == d.gate && (pch->source == VDD || pch->drain == VDD))
               founds = true;
            }
         for (k=0; k<temp[d.drain].sds.size(); k++)
            {
            pch = temp[d.drain].sds[k];
            if (pch->gate == d.gate && (pch->source == VDD || pch->drain == VDD))
               foundd = true;
            }
         if (founds && foundd) // both bitlines are precharged to vdd when equalizer is on, so delete equalizer
            {
            d.source = d.drain = d.gate = VSS;
            d.width  = 0.0;
            d.length = 0.0;
            }
         }
      }
   }


// this looks for a 6T ram cell and substitute behavioral code
void wirelisttype::SixTSubstitute(hashtype <const void *, bool> &beenHere)
   {
   TAtype ta("wirelisttype::SixTSubstitute()");
   int i, k;
   for (i=0; i<instances.size(); i++)
      {
      const instancetype &inst = instances[i];
      bool dummy;
      if (!beenHere.Check(inst.wptr, dummy))
         {
         beenHere.Add(inst.wptr, true);
         inst.wptr->SixTSubstitute(beenHere);
         }
      }

   array <nodetype> temp = nodes;
   int unique=0;
   int count=0;
   char buffer[256];

   temp = nodes;
   for (k=0; k<devices.size(); k++)
      if (devices[k].width >0.0)
         {
         temp[devices[k].source].sds.push(&devices[k]);
         temp[devices[k].drain].sds.push(&devices[k]);
         temp[devices[k].gate].gates.push(&devices[k]);
         }
   for (i=0; i<temp.size(); i++)
      {
      const nodetype &n = temp[i];
      int s_h = i, s_l=-1, b_h=-1, b_l=-1, wl=-1;
      bool found=false;

      if (n.sds.size()==3)  // possible cross-coupled node
         {
         int pcount=0, ncount=0;
         for (k=0; k<3; k++)
            {
            if (n.sds[k]->type == D_PMOS && (n.sds[k]->drain==VDD || n.sds[k]->source==VDD))
               {
               pcount++;
               s_l = n.sds[k]->gate;
               }
            }         
         for (k=0; k<3; k++)
            {
            if (n.sds[k]->type == D_NMOS && s_l == n.sds[k]->gate && (n.sds[k]->drain==VSS || n.sds[k]->source==VSS))
               {
               ncount++;
               }
            if (n.sds[k]->type == D_NMOS && s_l != n.sds[k]->gate && n.sds[k]->drain!=VSS && n.sds[k]->source!=VSS)
               {
               wl = n.sds[k]->gate;
               b_h = (n.sds[k]->source != s_h) ? n.sds[k]->source : n.sds[k]->drain;
               }
            }
         
         found = ncount==1 && pcount ==1 && temp[s_l].sds.size()==3;         
         // now verify that I have the other cross coupled inverter
         pcount = 0;
         ncount = 0;
         for (k=0; k<n.gates.size(); k++)
            {
            if (n.gates[k]->type == D_PMOS && (n.gates[k]->source == s_l || n.gates[k]->drain == s_l) && (n.gates[k]->source==VDD || n.gates[k]->drain==VDD))
               pcount++;
            if (n.gates[k]->type == D_NMOS && (n.gates[k]->source == s_l || n.gates[k]->drain == s_l) && (n.gates[k]->source==VSS || n.gates[k]->drain==VSS))
               ncount++;
            }
         found = found && pcount==1 && ncount==1;
         // now verify that the N pass devices' gate is the same wordline         
         if (found)
            {
            for (k=0; k<temp[s_l].sds.size(); k++)
               {
               if (temp[s_l].sds[k]->type == D_NMOS && temp[s_l].sds[k]->gate == wl)
                  b_l = temp[s_l].sds[k]->source!=s_l ? temp[s_l].sds[k]->source : temp[s_l].sds[k]->drain;
               }
            for (k=0; k<bristles.size(); k++)
               { // don't allow cross coupled nodes to be bristles of cell. (I can't guarantee there isn't another sd connection)
               if (bristles[k].nodeindex == s_h) found = false;
               if (bristles[k].nodeindex == s_l) found = false;
               }
            // now I want to see if bit lines is a bristle. If it isn't make sure it has many sd connections otherwise let the default algorithms make a udp here (for clocked cascode)
            ncount=0;
            for (k=0; k<bristles.size(); k++)
               { // don't allow cross coupled nodes to be bristles of cell. (I can't guarantee there isn't another sd connection)
               if (bristles[k].nodeindex == b_h) ncount++;
               if (bristles[k].nodeindex == b_l) ncount++;
               }
            if (b_h<0 || b_l<0)
               found = false;
            if (found && ncount==0 && temp[b_h].sds.size()<4 && temp[b_l].sds.size()<4)
               found = false;
            }
         found = found && s_h>VDD && s_l>VDD && b_l>VDD && b_h>VDD && wl>VDD;
         }
      if (found)
         {
         count++;
         instancetype inst;
         // delete the 6 transistors
         for (k=0; k<temp[s_h].sds.size(); k++)
            {
            temp[s_h].sds[k]->length = 0.0;
            temp[s_h].sds[k]->width = 0.0;
            temp[s_h].sds[k]->source = temp[s_h].sds[k]->drain = temp[s_h].sds[k]->gate = VSS;
            }
         for (k=0; k<temp[s_l].sds.size(); k++)
            {
            temp[s_l].sds[k]->length = 0.0;
            temp[s_l].sds[k]->width = 0.0;
            temp[s_l].sds[k]->source = temp[s_l].sds[k]->drain = temp[s_l].sds[k]->gate = VSS;
            }
         sprintf(buffer,"sixT_%d",unique++);
         inst.name = stringPool.NewString(buffer);
         inst.outsideNodes.push(s_h);
         inst.outsideNodes.push(s_l);
         inst.outsideNodes.push(b_h);
         inst.outsideNodes.push(b_l);
         inst.outsideNodes.push(wl);
         int index = -1;
         if (!hstructs.Check("SixT__Model", index))
            FATAL_ERROR;
         inst.wptr = structures[index];
         instances.push(inst);
         }
      }      
   for (i=devices.size()-1; i>=0; i--)
      {
      if (devices[i].length == 0.0)   // delete devices from list that were marked for deletion
         devices.RemoveFast(i);
      }
   }
   

// this function will look for inputs being received through always on devices with no ON connection to vdd/vss
// this is recursive function going through all levels of hierarchy
void wirelisttype::DolphinTransform(hashtype <const void *, bool> &beenHere, filetype &fp)
   {
   TAtype ta("wirelisttype::DolphinTransform()");
   int i, k;   

   for (i=0; i<instances.size(); i++)
      {
      const instancetype &inst = instances[i];
      bool dummy;
      if (!beenHere.Check(inst.wptr, dummy))
         {
         beenHere.Add(inst.wptr, true);
         inst.wptr->DolphinTransform(beenHere, fp);
         }
      for (int k=0; k<inst.outsideNodes.size(); k++)
         nodes[inst.outsideNodes[k]].state = true;
      }

   for (i=0; i<nodes.size(); i++)
      {
      nodes[i].sds.clear();
      nodes[i].gates.clear();
      }
   // remove tied off devices. This will eliminate diodes in input pad receivers.
   for (i=devices.size()-1; i>=0; i--)
      {
      devicetype &d = devices[i];
      if (d.type==D_NMOS && d.gate==VSS)
         devices.RemoveFast(i);
      else if (d.type==D_PMOS && d.gate==VDD)
         devices.RemoveFast(i);
      }
   for (i=0; i<devices.size(); i++)
      {
      devicetype &d = devices[i];

      if (d.source > VDD)
         nodes[d.source].sds.push(&d);
      if (d.drain > VDD)
         nodes[d.drain].sds.push(&d);
      if (d.gate > VDD)
         nodes[d.gate].gates.push(&d);
      }

   bool notdone;
   do {
      notdone = false;

      for (i=0; i<devices.size(); i++)
         {
         devicetype &d = devices[i];

         if (d.source > VSS || d.drain > VSS)
            {
            if ((d.type == D_NMOS && d.gate == VSS) || (d.type == D_PMOS && d.gate == VDD))  // delete device permanently off
               {
               d.length = d.width = 0.0;  // delete this device
               for (k=nodes[d.source].sds.size()-1; k>=0; k--)
                  {
                  if (nodes[d.source].sds[k] == &d)
                     nodes[d.source].sds.RemoveFast(k);   // delete cross reference to this device
                  }
               d.source = VSS;
               for (k=nodes[d.drain].sds.size()-1; k>=0; k--)
                  {
                  if (nodes[d.drain].sds[k] == &d)
                     nodes[d.drain].sds.RemoveFast(k);   // delete cross reference to this device
                  }
               d.drain = VSS;
               notdone = true;
//               printf("\nDeleting OFF device %s",d.name);
               }

            if ((d.type == D_NMOS && d.gate == VDD && d.source == VSS) || (d.type == D_PMOS && d.gate == VSS && d.source == VDD))  // delete device permanently on
               {               
               // I need to make sure that there aren't any other devices on this node to fight the pullup/dn
               if (nodes[d.drain].sds.size()==1)
                  {
                  for (k=instances.size()-1; k>=0; k--)            // now do the replace on any instances
                     {
                     instancetype &inst = instances[k];
                     int j;
                     for (j=inst.outsideNodes.size()-1; j>=0; j--)
                        {
                        if (inst.outsideNodes[j] == d.drain)
                           break;
                        }
                     if (j>=0) break;
                     }                  
                  if (k<0)  // if this node is passed into another structure don't delete
                     {
                     for (k=0; k<bristles.size(); k++)
                        if (bristles[k].nodeindex == d.drain)
                           break;
                     if (k<0) // if this node is a bristle out of cell don't delete
                        {
                        d.length = d.width = 0.0;  // delete this device                  
                        for (k=0; k<nodes[d.drain].gates.size(); k++)
                           nodes[d.drain].gates[k]->gate = d.source;  // replace this node with vdd/vss
                        nodes[d.drain].sds.clear();
                        nodes[d.drain].gates.clear();
                        notdone = true;
                        d.source = d.drain = d.gate = VSS;
                        }
                     }
                  }
               }
            if ((d.type == D_NMOS && d.gate == VDD && d.drain == VSS) || (d.type == D_PMOS && d.gate == VSS && d.drain == VDD))  // delete device permanently on
               {               
               // I need to make sure that there aren't any other devices on this node to fight the pullup/dn
               if (nodes[d.source].sds.size()==1)
                  {
                  for (k=instances.size()-1; k>=0; k--)            // now do the replace on any instances
                     {
                     instancetype &inst = instances[k];
                     int j;
                     for (j=inst.outsideNodes.size()-1; j>=0; j--)
                        {
                        if (inst.outsideNodes[j] == d.source)
                           break;
                        }
                     if (j>=0) break;
                     }
                  if (k<0)  // if this node is passed into another structure don't delete
                     {
                     for (k=0; k<bristles.size(); k++)
                        if (bristles[k].nodeindex == d.source)
                           break;
                     if (k<0) // if this node is a bristle out of cell don't delete
                        {
                        d.length = d.width = 0.0;  // delete this device                  
                        for (k=0; k<nodes[d.source].gates.size(); k++)
                           {
                           nodes[d.source].gates[k]->gate = d.drain;  // replace this node with vdd/vss
                           }
                        nodes[d.source].sds.clear();
                        nodes[d.source].gates.clear();
                        notdone = true;
                        d.source = d.drain = d.gate = VSS;
                        //                  printf("\nDeleting ON device %s",d.name);
                        }
                     }
                  }
               }
            }
         }
      }while (notdone);
   // now I want to look for ctrans which are always on with no potential for a fight
   for (i=0; i<nodes.size(); i++)
      {
      nodetype &n = nodes[i];

      if (n.sds.size()==2)
         {
         int n1=-1, n2=-1;
         devicetype *d1 = n.sds[0];
         devicetype *d2 = n.sds[1];
         if (n.sds[0]->type == D_PMOS)
            {
            d1 = n.sds[1]; // d1 will be nmos
            d2 = n.sds[0];
            }
         
         n1 = d1->source;
         n2 = d1->drain;
         if (d1->type == D_NMOS && d2->type == D_PMOS && d1->gate == VDD && d2->gate == VSS && n1 > VDD && n2 > VDD && 
             (d2->source == n1 || d2->drain == n1) && (d2->source == n2 || d2->drain == n2) && n1!=n2)
            { // now I just need to delete one of the nodes. Let's make sure the one I delete isn't a bristle
            for (k=0; k<bristles.size(); k++)
               {
               if (bristles[k].nodeindex == n1)
                  {
                  int temp = n1;
                  n1 = n2;
                  n2 = temp;
                  }
               }
            for (k=0; k<bristles.size(); k++)
               {
               if (bristles[k].nodeindex == n1)
                  FATAL_ERROR;   // both nodes must be bristles
               }
            // I want to delete n1 now
            nodes[n2].sds   += nodes[n1].sds;
            nodes[n2].gates += nodes[n1].gates;
            for (k=0; k<nodes[n1].sds.size(); k++)
               {
               devicetype &d = *nodes[n1].sds[k];
               if (d.source == n1)
                  d.source = n2;
               if (d.drain == n1)
                  d.drain = n2;
               }
            for (k=0; k<nodes[n1].gates.size(); k++)
               {
               devicetype &d = *nodes[n1].gates[k];
               d.gate = n2;
               }
            for (k=0;  k<instances.size(); k++)
               {
               instancetype &inst = instances[k];
               int ii;
               
               for (ii=0; ii<inst.outsideNodes.size(); ii++)
                  {
                  if (inst.outsideNodes[ii] == n1)
                     inst.outsideNodes[ii] = n2;
                  }                  
               }
            d1->source = d1->drain = d1->gate = VSS;
            d1->length = d1->width = 0.0;
            d2->source = d2->drain = d2->gate = VSS;
            d2->length = d2->width = 0.0;
            }
         }
      }
   // I want to look for single device always on that can't fight. (resistors will be eliminated this way)
   for (i=0; i<nodes.size(); i++)
      {
      nodetype &n = nodes[i];

      if (n.sds.size()==1)
         {
         int n1=-1, n2=-1;
         devicetype *d1 = n.sds[0];
         
         n1 = d1->source;
         n2 = d1->drain;
         if (((d1->type == D_NMOS && d1->gate == VDD) || (d1->type == D_PMOS && d1->gate == VSS)) && n1 > VDD && n2 > VDD && n1!=n2)
            { // now I just need to delete one of the nodes. Let's make sure the one I delete isn't a bristle
            for (k=0; k<bristles.size(); k++)
               {
               if (bristles[k].nodeindex == n1)
                  {
                  int temp = n1;
                  n1 = n2;
                  n2 = temp;
                  }
               }
            for (k=bristles.size()-1; k>=0; k--)
               {
               if (bristles[k].nodeindex == n1)
                  {
                  fp.fprintf("\n//INFO: I tried to delete device %d (in cell %s) but couldn't because both inputs are bristles.",d1->name,name);
                  break;
                  }
               }
            if (k<0)
               {
               // I want to delete n1 now
               nodes[n2].sds   += nodes[n1].sds;
               nodes[n2].gates += nodes[n1].gates;
               for (k=0; k<nodes[n1].sds.size(); k++)
                  {
                  devicetype &d = *nodes[n1].sds[k];
                  if (d.source == n1)
                     d.source = n2;
                  if (d.drain == n1)
                     d.drain = n2;
                  }
               for (k=0; k<nodes[n1].gates.size(); k++)
                  {
                  devicetype &d = *nodes[n1].gates[k];
                  d.gate = n2;
                  }
               for (k=0;  k<instances.size(); k++)
                  {
                  instancetype &inst = instances[k];
                  int ii;

                  for (ii=0; ii<inst.outsideNodes.size(); ii++)
                     {
                     if (inst.outsideNodes[ii] == n1)
                        inst.outsideNodes[ii] = n2;
                     }                  
                  }
               d1->source = d1->drain = d1->gate = VSS;
               d1->length = d1->width = 0.0;
               }
            }
         }
      }
   for (i=0; i<nodes.size(); i++)
      {
      nodes[i].sds.clear();
      nodes[i].gates.clear();
      }
   for (i=devices.size()-1; i>=0; i--)
      {
      if (devices[i].length == 0.0)   // delete devices from list that were marked for deletion
         devices.RemoveFast(i);
      }
   }


// this looks for a dynamic zdet which is broken in half with a nand gate.
// to save area and power when one side discharges the other side turns off it's keeper as well
// I will put the keepers and dedicated inverters instead of the nand gate
void wirelisttype::NandPchTransform(filetype &fp)
   {
   TAtype ta("wirelisttype::NandPchTransform()");
   int i, k;
   int unique=0;
   int count=0;
   array <nodetype> temp = nodes;
   bool xrefempty = true;
   char buffer[512], buf2[512]; 
   
   for (int nandoutput=0; nandoutput<temp.size(); nandoutput++)
      {
      if (xrefempty)
         {
         xrefempty = false;
         temp = nodes;
         for (k=0; k<devices.size(); k++)
            if (devices[k].width >0.0)
               {
               temp[devices[k].source].sds.push(&devices[k]);
               temp[devices[k].drain].sds.push(&devices[k]);
               temp[devices[k].gate].gates.push(&devices[k]);
               }
         count++;
         }
      // look for 2 input nand gate
      const nodetype &n = temp[nandoutput];
      if (n.sds.size()==3)
         { // potential nand2
         devicetype *dn1=NULL, *dn2=NULL, *dp1=NULL, *dp2=NULL;

         if (n.sds[0]->type == D_NMOS)
            dn1 = n.sds[0];
         if (n.sds[0]->type == D_PMOS)
            {
            dp2 = dp1;
            dp1 = n.sds[0];
            }
         if (n.sds[1]->type == D_NMOS)
            dn1 = n.sds[1];
         if (n.sds[1]->type == D_PMOS)
            {
            dp2 = dp1;
            dp1 = n.sds[1];
            }
         if (n.sds[2]->type == D_NMOS)
            dn1 = n.sds[2];
         if (n.sds[2]->type == D_PMOS)
            {
            dp2 = dp1;
            dp1 = n.sds[2];
            }

         if (dn1)
            {
            int stack = dn1->source!=nandoutput ? dn1->source : dn1->drain;
            if (temp[stack].sds.size()==2)
               {
               dn2 = temp[stack].sds[0];
               if (dn2 == dn1)
                  dn2 = temp[stack].sds[1];
               }
            }

         if (dn1 && dn2 && dp1 && dp2 && (dn1->gate == dp1->gate || dn1->gate == dp2->gate) && (dn2->gate == dp1->gate || dn2->gate == dp2->gate))
            { // I found a nand2
            nodetype &gate1=temp[dp1->gate], &gate2=temp[dp2->gate];
            devicetype *pch1=NULL, *pch2=NULL;

            for (i=0; i<gate1.sds.size(); i++)
               if (gate1.sds[i]->gate == nandoutput && gate1.sds[i]->type == D_PMOS && (gate1.sds[i]->source==VDD || gate1.sds[i]->drain==VDD))
                  pch1 = gate1.sds[i];
            for (i=0; i<gate2.sds.size(); i++)
               if (gate2.sds[i]->gate == nandoutput && gate2.sds[i]->type == D_PMOS && (gate2.sds[i]->source==VDD || gate2.sds[i]->drain==VDD))
                  pch2 = gate2.sds[i];
            if (pch1!=NULL && pch2!=NULL)
               { // I found a precharge on each input of nand2
               // make 2 new nodes
               nodetype newn = gate1;
               newn.sds.clear();
               newn.gates.clear();
               strcpy(buffer, n.name);
               sprintf(buf2,"$PchTransform%d",unique++);
               strcat(buffer, buf2);
               newn.name = stringPool.NewString(buffer);
               nodes.push(newn);
               pch1->gate = nodes.size()-1;

               strcpy(buffer, gate2.name);
               sprintf(buf2,"$PchTransform%d",unique++);
               strcat(buffer, buf2);
               newn.name = stringPool.NewString(buffer);
               nodes.push(newn);
               pch2->gate = nodes.size()-1;

               devicetype invn, invp;
               invn.type = D_NMOS;
               invp.type = D_PMOS;
               invn.source = VSS;
               invp.source = VDD;
               invn.gate  = invp.gate = dp1->gate;
               invn.drain = invp.drain = pch1->gate;
               invp.r     = invn.r = 1;
               invp.length= invn.length = 1;
               invp.width = invn.width = 1;

               devices.push(invn);
               devices.push(invp);

               invn.gate  = invp.gate = dp2->gate;
               invn.drain = invp.drain = pch2->gate;
               devices.push(invn);
               devices.push(invp);

               fp.fprintf("\n//INFO: Transform nand2 keeper into 2 inverters on nodes %s %s",gate1.name,gate2.name);
               xrefempty = true;
               }
            }
         }
      else if (n.sds.size()==4)
         { // potential nand3
         devicetype *dn1=NULL, *dn2=NULL, *dn3=NULL, *dp1=NULL, *dp2=NULL, *dp3=NULL;

         if (n.sds[0]->type == D_NMOS)
            dn1 = n.sds[0];
         if (n.sds[0]->type == D_PMOS)
            {
            dp3 = dp2;
            dp2 = dp1;
            dp1 = n.sds[0];
            }
         if (n.sds[1]->type == D_NMOS)
            dn1 = n.sds[1];
         if (n.sds[1]->type == D_PMOS)
            {
            dp3 = dp2;
            dp2 = dp1;
            dp1 = n.sds[1];
            }
         if (n.sds[2]->type == D_NMOS)
            dn1 = n.sds[2];
         if (n.sds[2]->type == D_PMOS)
            {
            dp3 = dp2;
            dp2 = dp1;
            dp1 = n.sds[2];
            }
         if (n.sds[3]->type == D_NMOS)
            dn1 = n.sds[3];
         if (n.sds[3]->type == D_PMOS)
            {
            dp3 = dp2;
            dp2 = dp1;
            dp1 = n.sds[3];
            }

         if (dn1)
            {
            int stack = dn1->source!=nandoutput ? dn1->source : dn1->drain;
            if (temp[stack].sds.size()==2)
               {
               dn2 = temp[stack].sds[0];
               if (dn2 == dn1)
                  dn2 = temp[stack].sds[1];
               }
            }
         if (dn2)
            {
            int stack = dn2->source!=nandoutput ? dn2->source : dn2->drain;
            if (temp[stack].sds.size()==2)
               {
               dn3 = temp[stack].sds[0];
               if (dn3 == dn2)
                  dn3 = temp[stack].sds[1];
               }
            }

         if (dn1 && dn2 && dn3 && dp1 && dp2 && dp3 && (dn1->gate == dp1->gate || dn1->gate == dp2->gate || dn1->gate == dp3->gate) && (dn2->gate == dp1->gate || dn2->gate == dp2->gate || dn2->gate == dp3->gate) && (dn3->gate == dp1->gate || dn3->gate == dp2->gate || dn3->gate == dp3->gate))
            { // I found a nand3
            nodetype &gate1=temp[dp1->gate], &gate2=temp[dp2->gate], &gate3=temp[dp3->gate];
            devicetype *pch1=NULL, *pch2=NULL;
            int zz1=-1, zz2=-1;

            for (i=0; i<gate1.sds.size(); i++)
               if (gate1.sds[i]->gate == nandoutput && gate1.sds[i]->type == D_PMOS && (gate1.sds[i]->source==VDD || gate1.sds[i]->drain==VDD))
                  {
                  pch2 = pch1;
                  pch1 = gate1.sds[i]; 
                  zz2  = zz1;
                  zz1  = dp1->gate;
                  }
            for (i=0; i<gate2.sds.size(); i++)
               if (gate2.sds[i]->gate == nandoutput && gate2.sds[i]->type == D_PMOS && (gate2.sds[i]->source==VDD || gate2.sds[i]->drain==VDD))
                  {
                  pch2 = pch1;
                  pch1 = gate2.sds[i];
                  zz2  = zz1;
                  zz1  = dp2->gate;
                  }
            for (i=0; i<gate3.sds.size(); i++)
               if (gate3.sds[i]->gate == nandoutput && gate3.sds[i]->type == D_PMOS && (gate3.sds[i]->source==VDD || gate3.sds[i]->drain==VDD))
                  {
                  pch2 = pch1;
                  pch1 = gate3.sds[i];
                  zz2  = zz1;
                  zz1  = dp3->gate;
                  }
            if (pch1!=NULL && pch2!=NULL && zz1>0 && zz2>0)
               { // I found a precharge on each input of nand2
               // make 2 new nodes
               nodetype newn = gate1;
               newn.sds.clear();
               newn.gates.clear();
               strcpy(buffer, n.name);
               sprintf(buf2,"$PchTransform%d",unique++);
               strcat(buffer, buf2);
               newn.name = stringPool.NewString(buffer);
               nodes.push(newn);
               pch1->gate = nodes.size()-1;

               strcpy(buffer, gate2.name);
               sprintf(buf2,"$PchTransform%d",unique++);
               strcat(buffer, buf2);
               newn.name = stringPool.NewString(buffer);
               nodes.push(newn);
               pch2->gate = nodes.size()-1;

               devicetype invn, invp;
               invn.type = D_NMOS;
               invp.type = D_PMOS;
               invn.source = VSS;
               invp.source = VDD;
               invn.gate  = invp.gate = zz1;
               invn.drain = invp.drain = pch1->gate;
               invp.r     = invn.r = 1;
               invp.length= invn.length = 1;
               invp.width = invn.width = 1;

               devices.push(invn);
               devices.push(invp);

               invn.gate  = invp.gate = zz2;
               invn.drain = invp.drain = pch2->gate;
               devices.push(invn);
               devices.push(invp);

               fp.fprintf("\n//INFO: Transform nand2 keeper on nodes %s",nodes[pch1->gate].name);
               xrefempty = true;
               }
            }
         }
      }
   // first I want to downsize keeper devices. (sherlock's strength analyzer isn't good enough for writeability)
   for (i=0; i<temp.size(); i++)
      {
      const nodetype &n = temp[i];
      if (n.gates.size()==2)
         {
         devicetype *invp=NULL, *invn=NULL;

         if (n.gates[0]->type == D_NMOS && (n.gates[0]->source==VSS || n.gates[0]->drain==VSS))
            invn = n.gates[0];
         if (n.gates[1]->type == D_NMOS && (n.gates[1]->source==VSS || n.gates[1]->drain==VSS))
            invn = n.gates[0];
         if (n.gates[0]->type == D_PMOS && (n.gates[0]->source==VDD || n.gates[0]->drain==VDD))
            invp = n.gates[0];
         if (n.gates[1]->type == D_PMOS && (n.gates[1]->source==VDD || n.gates[1]->drain==VDD))
            invp = n.gates[0];

         if (invp && invn)
            {
            int out = invp->drain==VDD ? invp->source : invp->drain;
            if (invn->drain != out && invn->source != out)
               out = -1;
            if (out>VDD)
               {
               devicetype *crossp=NULL, *crossn=NULL;
               for (k=0; k<temp[out].gates.size(); k++)
                  {
                  if (temp[out].gates[k]->type == D_PMOS && (temp[out].gates[k]->source==VDD || temp[out].gates[k]->drain==VDD) && (temp[out].gates[k]->source==invp->gate || temp[out].gates[k]->drain==invp->gate))
                     crossp = temp[out].gates[k];
                  if (temp[out].gates[k]->type == D_NMOS && (temp[out].gates[k]->source==VSS || temp[out].gates[k]->drain==VSS) && (temp[out].gates[k]->source==invp->gate || temp[out].gates[k]->drain==invp->gate))
                     crossp = temp[out].gates[k];
                  }
               if (crossp && crossn==NULL && strstr(temp[invp->gate].name,"zz")!=NULL)
                  {
                  crossp->width *= (float)0.01;
                  crossp->r *= 100;
                  fp.fprintf("\n//INFO: Downsizing keeper device %s on node %s",crossp->name,temp[invp->gate].name);
                  }
               }
            }
         }
      }
   }


// I have a problem with glitch latches when the nand2 is used for the keeper.
// Everything will be fine with the ZZ node but the nand output will be erroneous
// The transform will place a second nand gate dedicated solely for the keeper
void wirelisttype::NandPch2Transform(filetype &fp)
   {
   TAtype ta("wirelisttype::NandPch2Transform()");
   int i, k;
   int unique=0;
   int count=0;
   array <nodetype> temp = nodes;
   connectiontype connections(nodes.size());
   bool xrefempty = true;
   char buffer[512], buf2[512];
   const int original_nodesize = nodes.size();
   
   for (int nandoutput=0; nandoutput<original_nodesize; nandoutput++)
      {
      if (xrefempty)
         {
         xrefempty = false;
         temp = nodes;
         for (k=0; k<temp.size(); k++) {
            if (temp[k].sds.size()) 
               FATAL_ERROR;
            if (temp[k].gates.size()) 
               FATAL_ERROR;
            }
         for (k=0; k<devices.size(); k++)
            if (devices[k].width >0.0)
               {
               temp[devices[k].source].sds.push(&devices[k]);
               temp[devices[k].drain].sds.push(&devices[k]);
               temp[devices[k].gate].gates.push(&devices[k]);
               if (devices[k].source>VDD && devices[k].drain>VDD)
                  connections.Merge(devices[k].source, devices[k].drain);
               }
         count++;
         }
      // look for 2 input nand gate
      const nodetype &n = temp[nandoutput];
      if (n.sds.size()==3)
         { // potential nand2
         devicetype *dn1=NULL, *dn2=NULL, *dp1=NULL, *dp2=NULL;

         if (n.sds[0]->type == D_NMOS)
            dn1 = n.sds[0];
         if (n.sds[0]->type == D_PMOS)
            {
            dp2 = dp1;
            dp1 = n.sds[0];
            }
         if (n.sds[1]->type == D_NMOS)
            dn1 = n.sds[1];
         if (n.sds[1]->type == D_PMOS)
            {
            dp2 = dp1;
            dp1 = n.sds[1];
            }
         if (n.sds[2]->type == D_NMOS)
            dn1 = n.sds[2];
         if (n.sds[2]->type == D_PMOS)
            {
            dp2 = dp1;
            dp1 = n.sds[2];
            }

         if (dn1)
            {
            int stack = dn1->source!=nandoutput ? dn1->source : dn1->drain;
            if (temp[stack].sds.size()==2)
               {
               dn2 = temp[stack].sds[0];
               if (dn2 == dn1)
                  dn2 = temp[stack].sds[1];
               }
            }

         if (dn1 && dn2 && dp1 && dp2 && (dn1->gate == dp1->gate || dn1->gate == dp2->gate) && (dn2->gate == dp1->gate || dn2->gate == dp2->gate))
            { // I found a nand2
            int g1 = dp1->gate, g2=dp2->gate;
            nodetype &gate1=temp[g1], &gate2=temp[dp2->gate];
            devicetype *pch1=NULL, *pch2=NULL;

            for (i=0; i<gate1.sds.size(); i++)
               if (gate1.sds[i]->gate == nandoutput && gate1.sds[i]->type == D_PMOS && (gate1.sds[i]->source==VDD || gate1.sds[i]->drain==VDD))
                  pch1 = gate1.sds[i];
            for (i=0; i<gate2.sds.size(); i++)
               if (gate2.sds[i]->gate == nandoutput && gate2.sds[i]->type == D_PMOS && (gate2.sds[i]->source==VDD || gate2.sds[i]->drain==VDD))
                  pch2 = gate2.sds[i];
            
            if (pch2!=NULL && pch1==NULL)
               {
               pch1 = pch2;
               pch2 = NULL;
               }
                              
            if (pch1!=NULL && pch2==NULL)
               { 
               int zz = pch1->drain==VDD ? pch1->source : pch1->drain;
               // I found a precharge on one of the inputs
               // verify that I only have a precharge and not a cross coupled nand2
               int c=0;
               for (i=0; i<temp[nandoutput].gates.size(); i++)
                  {
                  if (connections.IsSame(temp[nandoutput].gates[i]->source, zz) || connections.IsSame(temp[nandoutput].gates[i]->drain, zz))
                     c++;
                  }
               if (c==1)
                  { // only one connection so must be the precharge

                  if (unique==-1)
                     printf("");

                  // now build nand2
                  // make 2 new nodes
                  nodetype newn = gate1;
                  newn.sds.clear();
                  newn.gates.clear();
                  strcpy(buffer, n.name);
                  sprintf(buf2,"$NandPch2Transform_%d",unique++);
                  strcat(buffer, buf2);
                  newn.name = stringPool.NewString(buffer);
                  nodes.push(newn);
                  pch1->gate = nodes.size()-1;
                  
                  strcpy(buffer, gate2.name);
                  sprintf(buf2,"$NandPch2Transform_%d",unique++);
                  strcat(buffer, buf2);
                  newn.name = stringPool.NewString(buffer);
                  nodes.push(newn);

                  devicetype d;                  
                  sprintf(buffer,"NandPch2Transform_%d",unique++);
                  d.name = stringPool.NewString(buffer);
                  d.type = D_NMOS;
                  d.source = VSS;
                  d.gate   = g1;
                  d.drain  = nodes.size()-1;
                  d.r = d.length = d.width = 1;
                  devices.push(d);
                  d.source = nodes.size()-2;
                  d.gate   = g2;
                  devices.push(d);
                  d.type   = D_PMOS;
                  d.drain  = VDD;
                  devices.push(d);
                  d.gate   = g1;
                  devices.push(d);

                  fp.fprintf("\n//INFO: Transform glat nand2 keeper on node %s",temp[nandoutput].name);
//                  printf("\n//INFO: Transform nand2 keeper into 2 inverters on nodes %s %s",gate1.name,gate2.name);
                  xrefempty = true;
                  }
               }
            }
         }
      }
   }

void wirelisttype::NandPch3Transform(filetype &fp)
   {
   TAtype ta("wirelisttype::NandPch3Transform()");
   int i, k;
   int unique=0;
   int count=0;
   array <nodetype> temp = nodes;
   connectiontype connections(nodes.size());
   bool xrefempty = true;
   char buffer[512], buf2[512];
   const int original_nodesize = nodes.size();
   
   for (int nandoutput=0; nandoutput<original_nodesize; nandoutput++)
      {
      if (xrefempty)
         {
         xrefempty = false;
         temp = nodes;
         for (k=0; k<temp.size(); k++) {
            if (temp[k].sds.size()) 
               FATAL_ERROR;
            if (temp[k].gates.size()) 
               FATAL_ERROR;
            }
         for (k=0; k<devices.size(); k++)
            if (devices[k].width >0.0)
               {
               temp[devices[k].source].sds.push(&devices[k]);
               temp[devices[k].drain].sds.push(&devices[k]);
               temp[devices[k].gate].gates.push(&devices[k]);
               if (devices[k].source>VDD && devices[k].drain>VDD)
                  connections.Merge(devices[k].source, devices[k].drain);
               }
         count++;
         }
      // look for 3 input nand gate
      const nodetype &n = temp[nandoutput];
      if (n.sds.size()==4)
         { // potential nand3
         devicetype *dn1=NULL, *dn2=NULL, *dn3=NULL, *dp1=NULL, *dp2=NULL, *dp3=NULL;

         for (i=0; i<4; i++)
            {
            if (n.sds[i]->type == D_NMOS)
               dn1 = n.sds[i];
            if (n.sds[i]->type == D_PMOS)
               {
               dp3 = dp2;
               dp2 = dp1;
               dp1 = n.sds[i];
               }
            }

         if (dn1)
            {
            int stack = dn1->source!=nandoutput ? dn1->source : dn1->drain;
            if (temp[stack].sds.size()==2)
               {
               dn2 = temp[stack].sds[0];
               if (dn2 == dn1)
                  dn2 = temp[stack].sds[1];
               }
            if (dn2)
               {
               int stack2 = dn2->source!=stack ? dn2->source : dn2->drain;
               if (temp[stack2].sds.size()==2)
                  {
                  dn3 = temp[stack2].sds[0];
                  if (dn3 == dn2)
                     dn3 = temp[stack2].sds[1];
                  }
               }
            }

         if (dn1 && dn2 && dn3 && dp1 && dp2 && dp3)
            {
            devicetype *foo[3]={dp1, dp2, dp3};
            dp1 = NULL;
            dp2 = NULL;            
            dp3 = NULL;
            for (i=0; i<3; i++)
               {
               if (foo[i]->gate==dn1->gate && dp1==NULL)
                  dp1 = foo[i];
               else if (foo[i]->gate==dn2->gate && dp2==NULL)
                  dp2 = foo[i];
               else if (foo[i]->gate==dn3->gate && dp3==NULL)
                  dp3 = foo[i];
               }
            }
         
         if (dn1 && dn2 && dn3 && dp1 && dp2 && dp3 && dn1->gate == dp1->gate && dn2->gate == dp2->gate && dn3->gate == dp3->gate && (dn3->source==VSS || dn3->drain==VSS))
            { // I found a nand2
            int g1 = dp1->gate, g2=dp2->gate, g3=dp3->gate;
            nodetype &gate1=temp[g1], &gate2=temp[g2], &gate3=temp[g3];
            devicetype *pch1=NULL, *pch2=NULL, *pch3=NULL;

            for (i=0; i<gate1.sds.size(); i++)
               if (gate1.sds[i]->gate == nandoutput && gate1.sds[i]->type == D_PMOS && (gate1.sds[i]->source==VDD || gate1.sds[i]->drain==VDD))
                  pch1 = gate1.sds[i];
            for (i=0; i<gate2.sds.size(); i++)
               if (gate2.sds[i]->gate == nandoutput && gate2.sds[i]->type == D_PMOS && (gate2.sds[i]->source==VDD || gate2.sds[i]->drain==VDD))
                  pch2 = gate2.sds[i];
            for (i=0; i<gate3.sds.size(); i++)
               if (gate3.sds[i]->gate == nandoutput && gate3.sds[i]->type == D_PMOS && (gate3.sds[i]->source==VDD || gate3.sds[i]->drain==VDD))
                  pch3 = gate3.sds[i];

            if (pch1==NULL)
               {
               pch1 = pch2;
               pch2 = pch3;
               pch3 = NULL;
               }
            if (pch1==NULL)
               {
               pch1 = pch2;
               pch2 = pch3;
               pch3 = NULL;
               }
                              
            // for now only handle the case with a single precharge
            if (pch1!=NULL && pch2==NULL)
               { 
               int zz = pch1->drain==VDD ? pch1->source : pch1->drain;
               // I found a precharge on one of the inputs
               // verify that I only have a precharge and not a cross coupled nand2
               int c=0;
               for (i=0; i<temp[nandoutput].gates.size(); i++)
                  {
                  if (connections.IsSame(temp[nandoutput].gates[i]->source, zz) || connections.IsSame(temp[nandoutput].gates[i]->drain, zz))
                     c++;
                  }
               if (c==1)
                  { // only one connection so must be the precharge
                  // now build nand3
                  // make 3 new nodes
                  nodetype newn = gate1;
                  newn.sds.clear();
                  newn.gates.clear();
                  strcpy(buffer, n.name);
                  sprintf(buf2,"$NandPch3Transform_%d",unique++);
                  strcat(buffer, buf2);
                  newn.name = stringPool.NewString(buffer);
                  nodes.push(newn);
                  pch1->gate = nodes.size()-1;
                  
                  strcpy(buffer, gate2.name);
                  sprintf(buf2,"$NandPch3Transform_%d",unique++);
                  strcat(buffer, buf2);
                  newn.name = stringPool.NewString(buffer);
                  nodes.push(newn);
                  
                  strcpy(buffer, gate3.name);
                  sprintf(buf2,"$NandPch3Transform_%d",unique++);
                  strcat(buffer, buf2);
                  newn.name = stringPool.NewString(buffer);
                  nodes.push(newn);

                  devicetype d;                  
                  sprintf(buffer,"NandPch3Transform_%d",unique++);
                  d.name = stringPool.NewString(buffer);
                  d.type = D_NMOS;
                  d.source = VSS;
                  d.gate   = g1;
                  d.drain  = nodes.size()-1;
                  d.r = d.length = d.width = 1;
                  devices.push(d);
                  d.source = nodes.size()-2;
                  d.drain  = nodes.size()-1;
                  d.gate   = g2;
                  devices.push(d);
                  d.source = nodes.size()-3;
                  d.drain  = nodes.size()-2;
                  d.gate   = g3;
                  devices.push(d);

                  d.type   = D_PMOS;
                  d.drain  = VDD;
                  d.gate   = g1;
                  devices.push(d);
                  d.gate   = g2;
                  devices.push(d);
                  d.gate   = g3;
                  devices.push(d);

                  fp.fprintf("\n//INFO: Transform glat nand3 keeper on node %s",temp[nandoutput].name);
//                  printf("\n//INFO: Transform nand2 keeper into 2 inverters on nodes %s %s",gate1.name,gate2.name);
                  xrefempty = true;
                  }
               }
            }
         }
      }
   }



void wirelisttype::VddoCheck(hashtype <const void *, bool> &beenHere, filetype &fp)
   {
   TAtype ta("wirelisttype::VddoCheck()");
   int i, k;   

   for (i=0; i<instances.size(); i++)
      {
      const instancetype &inst = instances[i];
      bool dummy;
      if (!beenHere.Check(inst.wptr, dummy))
         {
         beenHere.Add(inst.wptr, true);
         inst.wptr->VddoCheck(beenHere, fp);
         }
      for (int k=0; k<inst.outsideNodes.size(); k++)
         nodes[inst.outsideNodes[k]].state = true;
      }
   // I want to check that if this module uses vddo it appears as a bristle
   // I also will remap to vdd
   bool vddoUsed = false;
   for (i=0; i<devices.size(); i++)
      {
      devicetype &d = devices[i];
      if (d.gate == VDDO)
         {
         vddoUsed = true;
         d.gate   = VDD;
         }
      if (d.source == VDDO)
         {
         vddoUsed = true;
         d.source = VDD;
         }
      if (d.drain == VDDO)
         {
         vddoUsed = true;
         d.drain  = VDD;
         }
      }
   for (i=0; i<instances.size(); i++)
      {
      for (k=0; k<instances[i].outsideNodes.size(); k++)
         {
         if (instances[i].outsideNodes[k] == VDDO)
            {
            vddoUsed = true;
            instances[i].outsideNodes[k]  = VDD;
            }
         }
      }
   if (vddoUsed)
      {
      for (i=0; i<bristles.size(); i++)
         {
         const bristletype &brist = bristles[i];
         if (brist.nodeindex == VDDO)
//         if (stricmp(brist.name, "vddo")==0)
            vddoUsed = false;
         }
      if (vddoUsed)
         printf("WARNING!!!! Cell %s used VDDO and it isn't a bristle.\n",name);
      }
   }



void wirelisttype::WriteVerilog(wildcardtype &ignoreStructures, wildcardtype &explodeStructures, wildcardtype &deleteStructures, const char *&destname, bool atpg0, bool atpg1, bool assertionCheck, bool debug, filetype &fp)
   {
   TAtype ta("wirelisttype::WriteVerilog");   
   char unique[64];
   __uint64 md1, md2;
   int i, ii, k;
   hashtype <const void *, bool> beenHere;
   array <wirelisttype *> notDone;
   wirelisttype *wptr;
   char buffer[512], buffer2[512];
   filetype map;

   DEBUG = debug;

   sha(name, strlen(name), md1, md2);
   sprintf(unique, "_%s", Int64ToString(md1&0xFFFFFFFF,buffer2));
#ifdef _MSC_VER
   sprintf(buffer,"%s%s%s.v",name,assertionCheck?"_assert":"",unique);
#else
//   sprintf(buffer,"%s%s%s.v.gz",name,assertionCheck?"_assert":"",unique);
   sprintf(buffer,"%s%s%s.v",name,assertionCheck?"_assert":"",unique);
#endif
   if (destname==NULL)
      destname = stringPool.NewString(buffer);
   fp.fopen(destname,"wt");
   printf("\nWirelisting to %s\n",destname);
   
   sha(name, strlen(name), md1, md2);
   sprintf(unique, "_%s", Int64ToString(md1&0xFFFFFFFF,buffer2));
#ifdef _MSC_VER
   sprintf(buffer,"%s%s.map",name,unique);
#else
   sprintf(buffer,"%s%s.map.gz",name,unique);
#endif
   map.fopen(buffer,"wt");   
   
   fp.fprintf("\n`timescale 1ns / 1ps\n");
   time_t timer = time(NULL);
   fp.fprintf("\n//INFO This was extracted %s",asctime(localtime(&timer))),

   ExplicitDeletes(ignoreStructures, deleteStructures, fp);
   ExplicitExplodes(ignoreStructures, explodeStructures, fp);
   ExplicitExplodes(ignoreStructures, explodeStructures, fp);
   ExplicitExplodes(ignoreStructures, explodeStructures, fp);
   ExplicitExplodes(ignoreStructures, explodeStructures, fp);

   beenHere.clear();
   VddoCheck(beenHere, fp);
   
   beenHere.clear();
   SixTSubstitute(beenHere);
   beenHere.clear();
   DolphinTransform(beenHere, fp);
   
   // first thing I need to do is find source/drain regions that span hierarchy. I flatten things until this is
   // no longer the case
   
   for (i=0; i<bristles.size(); i++)
      bristles[i].pad = (atpg0 | atpg1);   // I want all top level routes to be input or inout
   beenHere.clear();
   DetermineBristleDirection(beenHere, ignoreStructures);
   beenHere.clear();                       // make two passes to catch all the pad references
   DetermineBristleDirection(beenHere, ignoreStructures);

   beenHere.clear();
   notDone.push(this);

   while (notDone.pop(wptr))
      {
      bool doneAtThisLevel;
      int inum = 0;
      do {
         array <int> driven (wptr->nodes.size(), 0);
         array <bool> inverter (wptr->nodes.size(), false);
         doneAtThisLevel = true;
         // now I want to find nodes driven more than once and explode hierarchy         
         
         for (i=0; i<wptr->devices.size(); i++)
            {
            devicetype &d  = wptr->devices[i];
            if (d.type == D_NMOS && (d.drain==VSS || d.source==VSS))
               {
               driven[d.source] += 1;
               driven[d.drain]  += 1;
               }
            else if (d.type == D_PMOS && (d.drain==VDD || d.source==VDD))
               {
               driven[d.source] += 100;
               driven[d.drain]  += 100;
               }
            else  // definitely not an inverter
               {
               driven[d.source] += 3;
               driven[d.drain]  += 3;
               }
            }
         for (i=0; i<driven.size(); i++)
            {
            inverter[i] = driven[i]==101 || driven[i]==0;
            if (driven[i]>0)
               driven[i]=1;
            }
         for (i=wptr->instances.size()-1; i>=0; i--)
            {
            const instancetype &inst = wptr->instances[i];
            for (k=0; k<inst.outsideNodes.size(); k++)
               {
               if (inst.wptr->bristles[k].output)
                  driven[inst.outsideNodes[k]]++;
               if (inst.wptr->bristles[k].output && !inst.wptr->bristles[k].inverter)
                  inverter[inst.outsideNodes[k]] = false;
               }
            }
         driven[VSS] = 0;  // power nodes don't count
         driven[VDD] = 0;
         for (i=wptr->instances.size()-1; i>=0; i--)
            {
            const instancetype &inst = wptr->instances[i];
            char buffer[256];
            strcpy(buffer, inst.wptr->name);
            strcat(buffer, "_bidir");
            if (ignoreStructures.Check(buffer))
               wptr->instances[i].wptr->name = stringPool.NewString(buffer);
            for (k=0; k<inst.outsideNodes.size(); k++)
               {
               if (inst.wptr->bristles[k].output)
                  {
                  if (driven[inst.outsideNodes[k]]>1 && !inverter[inst.outsideNodes[k]])
                     {
                     if (!ignoreStructures.Check(inst.wptr->name)){
                        const instancetype badinst = inst;
                        const char *foo0 = badinst.name;
                        const char *foo1 = badinst.wptr->name;
                        fp.fprintf("\n//INFO: Flattening instance %s(%s).",badinst.name,badinst.wptr->name);
                        fp.fprintf(" Bristle %s has a S/D connection.",inst.wptr->bristles[k].name);
                        wptr->instances.RemoveFast(i);
                        wptr->FlattenInstance(badinst);
                        doneAtThisLevel = false;
                        break;
                        }
                     else if (strstr(inst.wptr->name,"_BIDIR")==NULL && strstr(inst.wptr->name,"_bidir")==NULL) // don't buffer the output of any cell in substitute file with _BIDIR in name
                        { 
                        // I have a structure that I want to flatten but won't because it's in my substitute list(not _bidir)
                        // First I need to make sure this really is an output. It may be a source/drain connected input
                        // Assume that if vss/vdd is attached to source drain it's really output
                        // This case arises when a flop(which I want to substitute) drives a ctran(mul_cinflopmux3)
                        connectiontype connect(inst.wptr->nodes.size());
                        int l;
                        for (l=0; l<inst.wptr->devices.size(); l++)
                           {
                           const devicetype &d = inst.wptr->devices[l];
                           connect.Merge(d.drain, d.source);
                           }
                        if (connect.IsSame(VSS, inst.wptr->bristles[k].nodeindex) || connect.IsSame(VDD, inst.wptr->bristles[k].nodeindex))
                           {                           
                           // This structure is in my substitute list. I will make dummy inverters for the shorted outputs
                           char buffer[256], buf2[128];
                           devicetype d;
                           
                           fp.fprintf("\n//INFO: Placing ghost inverters on instance %s, ",inst.name,inst.wptr->name);
                           fp.fprintf(" Bristle %s(%s) has a S/D connection.",inst.wptr->bristles[k].name,wptr->nodes[inst.outsideNodes[k]].name);
                           sprintf(buffer,"Ghost$_%s",wptr->nodes[inst.outsideNodes[k]].VNameNoBracket(buf2));
                           wptr->nodes.push(nodetype(buffer));
                           wptr->nodes.back().state=true;
                           d.name = "Ghost inverter";
                           d.type = D_NMOS;
                           d.length = d.width = 1;
                           d.gate = wptr->nodes.size()-1;
                           d.drain  = VDD;
                           d.source = inst.outsideNodes[k];
                           wptr->devices.push(d);
                           d.drain = VSS;
                           d.type  = D_PMOS;
                           wptr->devices.push(d);    // this will be non-inverting buffer
                           // output of buffer is old net. Input is new net connected only to instance
                           wptr->instances[i].outsideNodes[k] = wptr->nodes.size()-1;
                           }
                        }
                     }               
                  }
               }
            }

         }while (!doneAtThisLevel);

      for (i=wptr->instances.size()-1; i>=0; i--)
         {
         const instancetype &inst = wptr->instances[i];
         if (inst.outsideNodes.size()==0)  // no connections, probably dcap so delete
            {
            if (DEBUG)
               fp.fprintf("\n//INFO: Deleting instance %s(%s)", inst.name, inst.wptr->name);
            wptr->instances.RemoveFast(i);
            }
         else if (inst.outsideNodes.size()==1 && inst.wptr->bristles[0].input)  // only 1 input connection, probably dcap so delete
            {
            if (DEBUG)
               fp.fprintf("\n//INFO: Deleting instance %s(%s)", inst.name, inst.wptr->name);
            wptr->instances.RemoveFast(i);
            }
         }

      fp.fprintf( "\nmodule %s%s(", wptr->name, unique);
	  if (stricmp("NAND2X6",wptr->name)==0)
		 printf("");
	  if (stricmp(wptr->name, "xtdivdff")==0)
		 printf("");
      map.fprintf("\nset rtl_scope RTL_PLACEHOLDER.%s",wptr->name);
      map.fprintf("\nset sch_scope SCH_PLACEHOLDER.dac_%s",wptr->name);
      map.fprintf("\nset cmp_clock CLK_PLACEHOLDER\n");
      printf("Processing module %s\n", wptr->name);
      // I need to bus compress the bristles
      array <buscompresstype> bs, ns;
      for (i=0; i<wptr->bristles.size(); i++)
         bs.push(wptr->bristles[i]);
      bs.Sort();
      for (i=1; i<bs.size(); i++)
         {
         if (0==strcmp(bs[i-1].name, bs[i].name) && bs[i-1].input==bs[i].input && bs[i-1].output==bs[i].output)
            {
            bs[i-1].lsb = MINIMUM(bs[i-1].lsb, bs[i].lsb);
            bs[i-1].msb = MAXIMUM(bs[i-1].msb, bs[i].msb);
            bs.Remove(i);
            i--;
            }
         }
      // I want to find undriven inputs(not unconnected inputs which I catch below)
	  array <bool> undriven_inputs(wptr->nodes.size(), false);
	  for (ii=wptr->instances.size()-1; ii>=0; ii--)
		 {
		 const instancetype &inst = wptr->instances[ii];
		 for (k=0; k<inst.outsideNodes.size(); k++)
			if (inst.wptr->bristles[k].pureinput)
			   undriven_inputs[inst.outsideNodes[k]] = true;
		 }
	  for (ii=wptr->instances.size()-1; ii>=0; ii--)
		 {
		 const instancetype &inst = wptr->instances[ii];
		 for (k=0; k<inst.outsideNodes.size(); k++)
			if (inst.wptr->bristles[k].output)
			   undriven_inputs[inst.outsideNodes[k]] = false;
		 }
	  for (k=0; k<wptr->devices.size(); k++)
		 {
		 const devicetype &d=wptr->devices[k];
		 undriven_inputs[d.drain] = false;
		 undriven_inputs[d.source] = false;
		 }
	  for (k=0; k<wptr->bristles.size(); k++)
		 undriven_inputs[wptr->bristles[k].nodeindex] = false;
	  for (k=2; k<undriven_inputs.size(); k++)
		 {
		 if (undriven_inputs[k]){
	        printf("WARNING!!!! Undriven signal %s connects to an input instantiated in cell %s.\n",wptr->nodes[k].VName(buffer), wptr->name);
			fp.fprintf("//WARNING: Undriven signal %s connects to an input instantiated in cell %s.\n",wptr->nodes[k].VName(buffer), wptr->name);
			}
		 }

	  for (i=2; i<wptr->nodes.size(); i++)
         if (wptr->nodes[i].state)   // don't bother with internal diffusion nodes
            ns.push(wptr->nodes[i]);
      ns.Sort();
      for (i=1; i<ns.size(); i++)
         {
         if (0==strcmp(ns[i-1].name, ns[i].name) && ns[i-1].input == ns[i].input && ns[i-1].output == ns[i].output && ns[i-1].lsb>=0 && ns[i].lsb>=0)
            {
            if (ns[i-1].lsb <0 && ns[i].lsb >=0) 
               FATAL_ERROR;
            if (ns[i-1].lsb >=0 && ns[i].lsb <0) 
               FATAL_ERROR;
            ns[i-1].lsb = MINIMUM(ns[i-1].lsb, ns[i].lsb);
            ns[i-1].msb = MAXIMUM(ns[i-1].msb, ns[i].msb);
            ns.Remove(i);
            i--;
            }
         }

      for (i=0; i<bs.size(); i++)
         fp.fprintf( "\n\t\t\t\t%s%s", bs[i].VName(buffer), (i==bs.size()-1)?"":",");
      fp.fprintf(");\n");
      for (k=0; k<bs.size(); k++)
         if (bs[k].lsb>=0)
            {
            char rtl[256];
            bs[k].VName(rtl);
            fp.fprintf( "\n%s\t[%d:%d]\t %s;",bs[k].output ? (bs[k].pad ? "inout" : "output") : "input  ", bs[k].msb, bs[k].lsb, bs[k].VName(buffer));
            map.fprintf("\nw %s[%d:%d] \t%s[%d:%d]",rtl+2, bs[k].msb, bs[k].lsb,bs[k].VName(buffer), bs[k].msb, bs[k].lsb);
            }
         else
            {
            char rtl[256];
            bs[k].VName(rtl);
            fp.fprintf( "\n%s\t\t %s;",bs[k].output ? (bs[k].pad ? "inout" : "output") : "input  ", bs[k].VName(buffer));
            map.fprintf("\nw %s \t%s", rtl+2, bs[k].VName(buffer));
            }
      for (i=0; i<ns.size(); i++)
         {
         for (k=bs.size()-1; k>=0; k--)
            if (bs[k] == ns[i])
               break;
         if (k<0)
            if (ns[i].lsb>=0)
               fp.fprintf( "\nwire\t[%d:%d]\t %s;", ns[i].msb, ns[i].lsb, ns[i].VName(buffer));
            else
               fp.fprintf( "\nwire\t\t %s;", ns[i].VName(buffer));
         }      
      fp.fprintf("\nwire\t\t dummysig;\n");

      if (strcmp(wptr->name,"SixT__Model")==0)
         {
         if (atpg0 || atpg1)
            fp.fputs("\nTLATX1 i0(.D(i_B_H), .G(i_WL), .Q(o_S_H), .QN(o_S_L));");
         else
            {
            fp.fputs(
"\nreg\to_S_H;"\
"\nreg\to_S_L;");
            if (assertionCheck)
               fp.fputs("\ninitial o_S_H = 1'b0;\ninitial o_S_L = 1'b1;");
            fp.fputs(
"\n"\
"\nalways @(i_WL or i_B_H or i_B_L) begin"\
"\n    if (i_WL===1'bx) begin"\
"\n        o_S_H <= 1'bx;"\
"\n        o_S_L <= 1'bx;"\
"\n        end"\
"\n    else if (i_WL===1'b1) begin"\
"\n	       if (i_B_H=== 1'bx || i_B_L=== 1'bx || (i_B_H=== 1'b1 && i_B_L=== 1'b1) || (i_B_H=== 1'b0 && i_B_L=== 1'b0)) begin"\
"\n	           o_S_H <= 1'bx;"\
"\n	           o_S_L <= 1'bx;"\
"\n	           end"\
"\n	       else begin"\
"\n            o_S_H <= i_B_H;"\
"\n            o_S_L <= i_B_L;"\
"\n            end"\
"\n        end"\
"\n    end");
            }
         }
      
      for (ii=wptr->instances.size()-1; ii>=0; ii--)
         {
         const instancetype &inst = wptr->instances[ii];
         bool dummy;
         bool substituteModule=false;
         char appendedname[512];
         strcpy(appendedname, inst.wptr->name);
         strcat(appendedname, unique);
         
//         if (strcmp(inst.wptr->name,"dpr512x2m8")==0) 
//            printf("");
         if (ignoreStructures.Check(inst.wptr->name))
            {
            substituteModule = true;
            fp.fprintf( "\n%s %s(", inst.wptr->name, inst.VName(buffer));
            }
         else if(ignoreStructures.Check(appendedname))
            {
            substituteModule = true;
            fp.fprintf( "\n%s %s(", appendedname, inst.VName(buffer));
            }
         else            
            fp.fprintf( "\n%s%s %s(", inst.wptr->name, unique, inst.VName(buffer));
         
         array <buscompresstype> is;
         for (i=0; i<inst.wptr->bristles.size(); i++)
            if (strcmp(inst.wptr->bristles[i].name,"gnd")!=0 && strcmp(inst.wptr->bristles[i].name,"vdd")!=0)
               {
               is.push(buscompresstype(inst.wptr->bristles[i], array<int>(1,inst.outsideNodes[i])));
               if (substituteModule)
                  {
                  is.back().input = true; 
                  is.back().output = false;  // ignore bristle directions for substitue cells
                  }
               }
         is.Sort();
         for (i=1; i<is.size(); i++)
            {
            if (is[i-1] == is[i])
               {
               // fill in gaps for partially used bus
               for (k=is[i-1].msb+1; k<is[i].lsb; k++)
                  is[i-1].refs.push(-1);               
               is[i-1].msb = is[i].lsb;
               is[i-1].refs += is[i].refs;
               is[i-1].output = is[i-1].output || is[i].output;
               is.Remove(i);
               i--;
               }
            }
         
         for (k=0; k<is.size(); k++)
            {
            bool skipComma = false;
            if (is[k].lsb<0)
               {
               const char *chptr, *bptr = is[k].VName(buffer, substituteModule);
               if (is[k].refs[0] == 0)
                  chptr = "1'b0";
               else if (is[k].refs[0] == 1)
                  chptr = "1'b1";
               else
                  chptr = wptr->nodes[is[k].refs[0]].VName(buffer2);               
               if (strcmp(bptr,"gnd")!=0 && strcmp(bptr,"vdd")!=0)
                  fp.fprintf( "\n\t\t.%-36s (%s)", bptr, chptr);
               else
                  skipComma = true;
               if (!substituteModule && is[k].input && strstr(chptr,"_dac_unconnected")!=NULL){
                  printf("WARNING!!!! Instance %s in cell %s has an unconnected input bristle.\n",inst.VName(buffer), wptr->name);
                  fp.fprintf("//WARNING: Instance %s in cell %s has an unconnected input bristle.\n",inst.VName(buffer), wptr->name);
				  }
               }
            else
               {
               int count=0;
               fp.fprintf( "\n\t\t.%-36s ({", is[k].VName(buffer, substituteModule));
               for (int j=is[k].refs.size()-1; j>=0; j--)
                  {
                  const char *chptr;
                  count++;
                  if (count==33)
                     {
                     count=0;
                     fp.fprintf("\n\t\t\t\t\t\t\t");  // prevent the text line from becoming too long
                     }
                  if (is[k].refs[j] == 0)
                     chptr = "1'b0";
                  else if (is[k].refs[j] == 1)
                     chptr = "1'b1";
                  else if (is[k].refs[j] == -1)
                     chptr = "dummysig";
                  else
                     chptr = wptr->nodes[is[k].refs[j]].VName(buffer2);
                  fp.fprintf("%s%s", chptr, j==0 ? "" : ", ");
                  if (!substituteModule && is[k].input && strstr(chptr,"_dac_unconnected")!=NULL)
                     printf("WARNING!!!! Instance %s in cell %s has an unconnected input bristle.\n",inst.VName(buffer), wptr->name);
                  }
               fp.fprintf("})");
               }
            if (k!=is.size()-1 && !skipComma)
               fp.fprintf(",");
            }
         fp.fprintf(");\n");
         if (!beenHere.Check(inst.wptr, dummy) && !ignoreStructures.Check(inst.wptr->name) && !ignoreStructures.Check(appendedname))
            {
            notDone.push(inst.wptr);
            beenHere.Add(inst.wptr, true);
            }
         }     
      wptr->MergeStackedTransform(fp);
      wptr->CustomRFHACKTransform();
      wptr->ArtisanTransform();
      wptr->NandPchTransform(fp);
      wptr->NandPch2Transform(fp);
      wptr->NandPch3Transform(fp);
      wptr->AbstractDevices(fp, unique, inum, atpg0, atpg1, assertionCheck);
      
      fp.fprintf( "\nendmodule");
      fp.fprintf( "\n");
      map.fprintf("\n\n");
      }
   printf("\nDumping udp's");
//   tabletype::DumpUdps(atpg0, atpg1);
   tabletype::DumpUdps(false, false);
   fp.fprintf("\n\n");
   map.fclose();
   }



void wirelisttype::ExplicitExplodes(wildcardtype &ignoreStructures, wildcardtype &explodeStructures, filetype &fp)
   {
   TAtype ta("wirelisttype::ExplicitExplodes");
   array <wirelisttype *> notDone;
   hashtype <const void *, bool> beenHere;
   wirelisttype *wptr;
   int i;

   beenHere.clear();
   notDone.push(this);

   while (notDone.pop(wptr))
      {
      bool doneAtThisLevel;

      for (i=wptr->instances.size()-1; i>=0; i--)
         {
         const instancetype &inst = wptr->instances[i];

         
         if (strncmp(inst.name,"EXPLODE_",8)==0 || explodeStructures.Check(inst.wptr->name))
            {
            const instancetype badinst = inst;
            fp.fprintf("\n//INFO: Flattening instance %s(%s) due to 'EXPLODE_' in instance name or substitute file.",badinst.name,badinst.wptr->name);
            wptr->instances.RemoveFast(i);
            wptr->FlattenInstance(badinst);
            doneAtThisLevel = false;
            }
         }
      for (int i=wptr->instances.size()-1; i>=0; i--)
         {
         const instancetype &inst = wptr->instances[i];
         bool dummy;
         if (!beenHere.Check(inst.wptr, dummy) && !ignoreStructures.Check(inst.wptr->name))
            {
            notDone.push(inst.wptr);
            beenHere.Add(inst.wptr, true);
            }
         }
      }
   }

void wirelisttype::ExplicitDeletes(wildcardtype &ignoreStructures, wildcardtype &deleteStructures, filetype &fp)
   {
   TAtype ta("wirelisttype::ExplicitDeletes");
   array <wirelisttype *> notDone;
   hashtype <const void *, bool> beenHere;
   wirelisttype *wptr;
   int i;

   beenHere.clear();
   notDone.push(this);

   while (notDone.pop(wptr))
      {
      bool doneAtThisLevel;

      for (i=wptr->instances.size()-1; i>=0; i--)
         {
         const instancetype &inst = wptr->instances[i];
         
         if (strncmp(inst.name,"DELETEME_",9)==0 || deleteStructures.Check(inst.wptr->name))
            {
            const instancetype badinst = inst;
            fp.fprintf("\n//INFO: Deleting instance %s(%s) due to 'DELETEME_' in instance name or substitute file.",badinst.name,badinst.wptr->name);
            wptr->instances.RemoveFast(i);
            doneAtThisLevel = false;
            }
         }
      for (int i=wptr->instances.size()-1; i>=0; i--)
         {
         const instancetype &inst = wptr->instances[i];
         bool dummy;
         if (!beenHere.Check(inst.wptr, dummy) && !ignoreStructures.Check(inst.wptr->name))
            {
            notDone.push(inst.wptr);
            beenHere.Add(inst.wptr, true);
            }
         }
      }
   }


void wirelisttype::DetermineBristleDirection(hashtype <const void *, bool> &beenHere, wildcardtype &ignoreStructures)  // this is a recursive function
   {
   TAtype ta("wirelisttype::DetermineBristleDirection");
   int i;
   
   for (i=0; i<instances.size(); i++)
      {
      const instancetype &inst = instances[i];
      bool dummy;
      if (!beenHere.Check(inst.wptr, dummy))
         {
         beenHere.Add(inst.wptr, true);
         inst.wptr->DetermineBristleDirection(beenHere, ignoreStructures);
         }
      for (int k=0; k<inst.outsideNodes.size(); k++)
         nodes[inst.outsideNodes[k]].state = true;
      }

   for (i=0; i<bristles.size(); i++)
      {
      bristletype &brist = bristles[i];      
      int k, j;
      int gate=-1;

      brist.inverter = true;
      for (k=0; k<devices.size(); k++)
         {
         const devicetype &d = devices[k];
         if (brist.nodeindex == d.source || brist.nodeindex == d.drain)
            {
            brist.output = d.type==D_NMOS || d.type==D_PMOS;
            if ((d.type == D_NMOS && (d.source==VSS || d.drain==VSS)) || (d.type == D_PMOS && (d.source==VDD || d.drain==VDD)))
               {
               if (gate <0 || gate == d.gate) gate = d.gate;
               else brist.inverter=false;
               }
            else brist.inverter=false;
            }
		 if (brist.nodeindex == d.gate)
			brist.pureinput = true;
         }
      if (!brist.output) brist.inverter = false;
      for (k=0; k<instances.size(); k++)
         {
         const instancetype &inst = instances[k];
         for (j=0; j<inst.outsideNodes.size(); j++)
            {
            if (inst.outsideNodes[j] == brist.nodeindex)
               {
               if ((!brist.output || brist.inverter) && inst.wptr->bristles[j].inverter)
                  brist.inverter = true;
               if (inst.wptr->bristles[j].output && !inst.wptr->bristles[j].inverter)
                  brist.inverter = false;
               brist.output    = brist.output || inst.wptr->bristles[j].output;
			   brist.pureinput = brist.pureinput || inst.wptr->bristles[j].pureinput;
               inst.wptr->bristles[j].pad |= brist.pad;
               }
            }
         }
      int net;
//      if (strnicmp(brist.name, "vdda33", 6)==0 || stricmp(brist.name, "vddo")==0 || stricmp(brist.name, "vddx")==0 || stricmp(brist.name, "vdd")==0 || stricmp(brist.name, "gnd")==0)
      if (h.Check(brist.name, net) && net<=1)
         brist.output = false;
      brist.input = !brist.output;
	  brist.pureinput = brist.pureinput && !brist.output;
      nodes[brist.nodeindex].input  = brist.input;
      nodes[brist.nodeindex].output = brist.output;
      nodes[brist.nodeindex].state  = true;
      }
   for (i=0; i<devices.size(); i++)
      {
      const devicetype &d = devices[i];
      nodes[d.gate].state = true;
      }

   
   char appended[256];
   strcpy(appended, name);
   strcat(appended, "_bidir");
   if (ignoreStructures.Check(appended) || (ignoreStructures.Check(name) && strlen(name)>6 && stricmp("_bidir", name+strlen(name)-6)==0))
      {
	  return;
	  }
   
   }


bool wirelisttype::Flatten() // returns false if already flat
   {
   int i;

   if (instances.size()==0) return false;

   array <instancetype> oldInstances = instances;
   instances.clear();

   for (i=0; i<oldInstances.size(); i++)
      FlattenInstance(oldInstances[i], true);

   for (int k=0; k<instances.size(); k++)
      {
      instancetype ni = instances[k];
      for (int j=0; j<ni.outsideNodes.size(); j++)
         if (ni.outsideNodes[j] >= nodes.size())
            FATAL_ERROR;
      }
   return true;
   }


void wirelisttype::FlattenInstance(const instancetype &inst, bool preserveBadNames) 
   {
   int j, k;
   const wirelisttype &child = *inst.wptr;
   char buffer[1024];
   array <int> nodexref(child.nodes.size(), -1);

   // I want to explode this instance
   // I will take and add all of this instance's internal nodes to my node list.

   nodexref[0] = 0;  // map VSS and VDD even if not in port definitions
   nodexref[1] = 1;
   for (k=2; k<child.nodes.size(); k++)
      {
      for (j=child.bristles.size()-1; j>=0; j--)
         if (child.bristles[j].nodeindex == k)
            break;
      if (j>=0) // I have a bristle so use the high level name   
         nodexref[k] = inst.outsideNodes[j];
      else
         {
         char buf2[512];
         nodes.push(child.nodes[k]);
         strcpy(buffer, inst.name);
         strcat(buffer, "/");
         strcat(buffer, nodes.back().VName(buf2, true, preserveBadNames));
         nodes.back().NewName(buffer, preserveBadNames);
         h.Add(buffer, nodes.size()-1);
         nodexref[k] = nodes.size()-1;
         }            
      }
   
   // explode devices
   for (k=0; k<child.devices.size(); k++)
      {
      devicetype d = child.devices[k];
      
      strcpy(buffer, inst.name);
      strcat(buffer, "/");
      strcat(buffer, d.name);
      
      d.name   = stringPool.NewString(buffer);
      d.source = nodexref[d.source];
      d.gate   = nodexref[d.gate];
      d.drain  = nodexref[d.drain];
//	  d.x      = inst.translation.TransformX(child.devices[k].x, child.devices[k].y);
//	  d.y      = inst.translation.TransformY(child.devices[k].x, child.devices[k].y);
      devices.push(d);
      }
   
   // now explode instances
   for (k=0; k<child.instances.size(); k++)
      {
      instancetype ni = child.instances[k];

      strcpy(buffer, inst.name);
      strcat(buffer, "/");
      strcat(buffer, ni.name);
      
      ni.name  = stringPool.NewString(buffer);
      for (j=0; j<ni.outsideNodes.size(); j++)
         ni.outsideNodes[j] = nodexref[ni.outsideNodes[j]];
      ni.translation = inst.translation.Transform(ni.translation);
	  instances.push(ni);
      }
   }



enum keywordstype {KW_NETLIST,KW_VERSION,KW_GLOBAL,KW_CELL,KW_PORT,KW_INST,KW_PROP,KW_PIN,KW_NULLPIN,KW_PARAM,KW_CLOSEBRACE, KW_NULL};
const char *keywords[]={"{netlist ", "{version ", "{net_global ", "{cell ", "{port ", "{inst ", "{prop ", "{pin ", "{pin}", "{param ", "}", NULL};


// return true on error
wirelisttype *ReadWirelist(const char *filename, array<const char *> &vddNames, array<const char *> &vssNames)
   {
   char buffer[2048];
   int line=0;
   enum keywordstype lastkeyword=KW_NULL;
   wirelisttype *current;
   instancetype inst;
   double length=0.0, width=0.0;
   double x=0.0, y=0.0;
   int angle=0, reflection=0;
   bool ignoreInstance;
   texthashtype <bool> ignorestructs;
   filetype fp;
   
   fp.fopen(filename, "rt");
   printf("\nReading %s\n",filename);

//   ignorestructs.Add("C1", true);    // dcap
//   ignorestructs.Add("c1", true);    // dcap
//   ignorestructs.Add("C2", true);    // dcap
//   ignorestructs.Add("c2", true);    // dcap
   ignorestructs.Add("CP", true);    // cap
   ignorestructs.Add("DN", true);    // antenna diodes
   ignorestructs.Add("DW", true);    // well diode?
   ignorestructs.Add("ndio", true);  // antenna diodes
   ignorestructs.Add("NDIO", true);  // antenna diodes
   ignorestructs.Add("NDIO_HVT", true);  // antenna diodes
   ignorestructs.Add("DP", true);    // antenna diodes
   ignorestructs.Add("pdio", true);  // antenna diodes
   ignorestructs.Add("PDIO", true);  // antenna diodes
   ignorestructs.Add("PDIO_HVT", true);  // antenna diodes
   ignorestructs.Add("nch_pcap", true); 
   ignorestructs.Add("pch_pcap", true); 
   ignorestructs.Add("nch_hvt_pcap", true); 
   ignorestructs.Add("pch_hvt_pcap", true); 
   ignorestructs.Add("nch_lvt_pcap", true); 
   ignorestructs.Add("pch_lvt_pcap", true); 
   
   current = new wirelisttype(vddNames, vssNames);
   current->name = "nch";
   current->bristles.push(bristletype("DRN", 0));
   current->bristles.push(bristletype("GATE", 1));
   current->bristles.push(bristletype("SRC", 2));
   current->bristles.push(bristletype("BULK", 3));
   structures.push(current);
   hstructs.Add(current->name, structures.size()-1);
   hstructs.Add("nch_33", structures.size()-1);
   hstructs.Add("nch_25", structures.size()-1);
   hstructs.Add("NCH", structures.size()-1);
   hstructs.Add("N", structures.size()-1);
   hstructs.Add("nd", structures.size()-1);  // thick ox transistors
   hstructs.Add("ND", structures.size()-1);  // thick ox transistors
   current = new wirelisttype(vddNames, vssNames);
   current->name = "nch_hvt";
   current->bristles.push(bristletype("DRN", 0));
   current->bristles.push(bristletype("GATE", 1));
   current->bristles.push(bristletype("SRC", 2));
   current->bristles.push(bristletype("BULK", 3));
   structures.push(current);
   hstructs.Add(current->name, structures.size()-1);
   hstructs.Add("NCH_HVT", structures.size()-1);
   current = new wirelisttype(vddNames, vssNames);
   current->name = "nch_lvt";
   current->bristles.push(bristletype("DRN", 0));
   current->bristles.push(bristletype("GATE", 1));
   current->bristles.push(bristletype("SRC", 2));
   current->bristles.push(bristletype("BULK", 3));
   structures.push(current);
   hstructs.Add(current->name, structures.size()-1);
   hstructs.Add("NCH_LVT", structures.size()-1);
   current = new wirelisttype(vddNames, vssNames);
   current->name = "nchpg_sr";
   current->bristles.push(bristletype("DRN", 0));
   current->bristles.push(bristletype("GATE", 1));
   current->bristles.push(bristletype("SRC", 2));
   current->bristles.push(bristletype("BULK", 3));
   structures.push(current);
   hstructs.Add(current->name, structures.size()-1);
   hstructs.Add("NCHPG_SR", structures.size()-1);
   hstructs.Add("nchpd_sr", structures.size()-1);
   hstructs.Add("NCHPD_SR", structures.size()-1);
   hstructs.Add("NCH_SRAM", structures.size()-1);
   
   current = new wirelisttype(vddNames, vssNames);
   current->name = "pch";
   current->bristles.push(bristletype("DRN", 0));
   current->bristles.push(bristletype("GATE", 1));
   current->bristles.push(bristletype("SRC", 2));
   current->bristles.push(bristletype("BULK", 3));
   structures.push(current);
   hstructs.Add(current->name, structures.size()-1);
   hstructs.Add("pch_33", structures.size()-1);
   hstructs.Add("pch_25", structures.size()-1);
   hstructs.Add("PCH", structures.size()-1);
   hstructs.Add("pch_hvt", structures.size()-1);
   hstructs.Add("pch_lvt", structures.size()-1);
   hstructs.Add("P", structures.size()-1);
   hstructs.Add("pd", structures.size()-1);  // thick ox transistors
   hstructs.Add("PD", structures.size()-1);  // thick ox transistors
   hstructs.Add("PCH_SRAM", structures.size()-1);  // thick ox transistors
   current = new wirelisttype(vddNames, vssNames);
   current->name = "pch_hvt";
   current->bristles.push(bristletype("DRN", 0));
   current->bristles.push(bristletype("GATE", 1));
   current->bristles.push(bristletype("SRC", 2));
   current->bristles.push(bristletype("BULK", 3));
   structures.push(current);
   hstructs.Add(current->name, structures.size()-1);
   hstructs.Add("PCH_HVT", structures.size()-1);
   current = new wirelisttype(vddNames, vssNames);
   current->name = "pch_lvt";
   current->bristles.push(bristletype("DRN", 0));
   current->bristles.push(bristletype("GATE", 1));
   current->bristles.push(bristletype("SRC", 2));
   current->bristles.push(bristletype("BULK", 3));
   structures.push(current);
   hstructs.Add(current->name, structures.size()-1);
   hstructs.Add("PCH_LVT", structures.size()-1);
   current = new wirelisttype(vddNames, vssNames);
   current->name = "pchpu_sr";
   current->bristles.push(bristletype("DRN", 0));
   current->bristles.push(bristletype("GATE", 1));
   current->bristles.push(bristletype("SRC", 2));
   current->bristles.push(bristletype("BULK", 3));
   structures.push(current);
   hstructs.Add(current->name, structures.size()-1);
   hstructs.Add("PCHPU_SR", structures.size()-1);

   current = new wirelisttype(vddNames, vssNames);
   current->name = "c1";
   current->bristles.push(bristletype("DRN", 0));
   current->bristles.push(bristletype("GATE", 1));
   current->bristles.push(bristletype("SRC", 2));
   current->bristles.push(bristletype("BULK", 3));
   structures.push(current);
   hstructs.Add(current->name, structures.size()-1);
   hstructs.Add("c1", structures.size()-1);
   hstructs.Add("c2", structures.size()-1);
   hstructs.Add("C1", structures.size()-1);
   hstructs.Add("C2", structures.size()-1); 



      {
      devicetype d;
      current = new wirelisttype(vddNames, vssNames);
      current->name = "R";                         // will be resistor, I make this be nmos with gate=vdd
            
      current->nodes.push(nodetype("A"));
      current->bristles.push(bristletype("A", current->nodes.size()-1));
      current->h.Add(current->nodes.back().name, current->nodes.size()-1);
      d.source = current->nodes.size()-1;
      current->nodes.push(nodetype("B"));
      current->bristles.push(bristletype("B", current->nodes.size()-1));
      current->h.Add(current->nodes.back().name, current->nodes.size()-1);
      structures.push(current);
      hstructs.Add(current->name, structures.size()-1);
      current->name = "PR";                         // will be resistor, I make this be nmos with gate=vdd
      structures.push(current);
      hstructs.Add(current->name, structures.size()-1);
      hstructs.Add("rppolywo", structures.size()-1);
      hstructs.Add("rppolyl", structures.size()-1);
      }
      {
      devicetype d;
      current = new wirelisttype(vddNames, vssNames);
      current->name = "PV";          // bipolar device which I'll ignore
      structures.push(current);
      hstructs.Add(current->name, structures.size()-1);
      }
   current = new wirelisttype(vddNames, vssNames);
   current->name = "SixT__Model";
   current->nodes.push(nodetype("S_H"));
   current->nodes.push(nodetype("S_L"));
   current->nodes.push(nodetype("B_H"));
   current->nodes.push(nodetype("B_L"));
   current->nodes.push(nodetype("WL"));
   current->bristles.push(bristletype("S_H", current->nodes.size()-5));
   current->bristles.back().output = true;
   current->bristles.push(bristletype("S_L", current->nodes.size()-4));
   current->bristles.back().output = true;
   current->bristles.push(bristletype("B_H", current->nodes.size()-3));
   current->bristles.push(bristletype("B_L", current->nodes.size()-2));
   current->bristles.push(bristletype("WL",  current->nodes.size()-1));
   structures.push(current);
   hstructs.Add(current->name, structures.size()-1);

   while (fp.fgets(buffer, sizeof(buffer)-1) !=NULL)
      {
      int i, index;
      char *chptr, *chptr2;
      const char *name;
      line++;

      while ((chptr=strchr(buffer,'@'))!=NULL)
         *chptr = '_';
      while ((chptr=strchr(buffer,'\\'))!=NULL)
         *chptr = '/';
      chptr = strchr(buffer, '\n');
      if (chptr!=NULL)
         *chptr = 0;
      chptr = buffer;
      while (*chptr==' ' || *chptr=='\t' || *chptr=='\r')
         chptr++;

      for (i=0; keywords[i]!=NULL; i++)
         {
         if (strnicmp(chptr, keywords[i], strlen(keywords[i]))==0)
            {
            lastkeyword = (enum keywordstype)i;
            chptr += strlen(keywords[i]);
			break;
            }
         }
      while (*chptr==' ')
         chptr++;


      switch (lastkeyword)
         {
         case KW_CELL:
            current = new wirelisttype(vddNames, vssNames);
            current->name = stringPool.NewString(chptr);
            structures.push(current);
            hstructs.Add(current->name, structures.size()-1);
            break;

         case KW_PORT:
            while (chptr!=NULL && *chptr !=0)
               {
               int index;
               chptr2 = strchr(chptr, ' ');
               if (chptr2)
                  *chptr2 = 0;
               if (strchr(chptr,'}'))
                  chptr2 = strchr(chptr, '}');
               if (chptr2)
                  *chptr2 = 0;
               
               if (current->h.Check(chptr, index))
                  ;//printf("\nYou are using a reserved word[%s] as a bristle in cell %s.", chptr, current->name);
               else
                  {
                  current->nodes.push(nodetype(chptr));
                  index = current->nodes.size()-1;
                  current->h.Add(chptr, index);
                  }
               current->bristles.push(bristletype(chptr, index));
               if (chptr2)
                  chptr = chptr2+1;
               else
                  chptr = NULL;
               }
            break;

         case KW_INST:
            if (inst.name != NULL) 
               {
               printf("I didn't understand line %d\n",line);
               FATAL_ERROR;
               }
            chptr2 = strchr(chptr, '=');
            if (chptr2==NULL)
               FATAL_ERROR;
            *chptr2 = 0;
            inst.name = stringPool.NewString(chptr);
            chptr = chptr2+1;
            length = width = 0.0;
            ignoreInstance = false;
			chptr2=strchr(chptr,' ');
			if (chptr2!=NULL) 
			   *chptr2=0;
			else chptr2=chptr;
            if (!hstructs.Check(chptr, index)) 
               {
               if (!ignorestructs.Check(chptr))
                  printf("\nI have an instance(%s) of a structure(%s) that isn't defined at line %d.", inst.name, chptr, line);
               inst.name = NULL;
               ignoreInstance = true;  // ignore stuff like capacitors and such
               break;
               }
            inst.wptr = structures[index];
            inst.outsideNodes.clear();
            for (i=0; i<inst.wptr->bristles.size(); i++)
               inst.outsideNodes.push(-1);
			chptr2=strstr(chptr2+1,"{PROP ");
			if (chptr2!=NULL)
			   chptr = chptr2+6;
			else
               break;
//!!!!!!! Watch out. I sometimes skip the break above. This is needed for .net files which have {prop } on the same line as the inst

         case KW_PROP:
            if (ignoreInstance) break;
			chptr2 = strstr(chptr, "x=");
			if (chptr2!=NULL)
			   x = atof(chptr2+2);
			chptr2 = strstr(chptr, "y=");
			if (chptr2!=NULL)
			   y = atof(chptr2+2);
			chptr2 = strstr(chptr, "angle=");
			if (chptr2!=NULL)
			   angle = atoi(chptr2+6);
			chptr2 = strstr(chptr, "reflection=");
			if (chptr2!=NULL)
			   reflection = atoi(chptr2+11);
			chptr2 = strstr(chptr, "top=");
			if (chptr2!=NULL)
			   break;	// ignore mbb info in .net

            chptr2 = strstr(chptr, "W=");
			if (chptr2==NULL)
			   chptr2 = strstr(chptr, "w=");
            if (chptr2==NULL && (stricmp(inst.wptr->name,"R")==0 || stricmp(inst.wptr->name,"PR")==0))
               break;  // ignore the RVAL= property for resistors
            if (chptr2!=NULL)
               width = atof(chptr2+2);
            chptr2 = strstr(chptr, "L=");
			if (chptr2==NULL)
			   chptr2 = strstr(chptr, "l=");
            if (chptr2!=NULL)
               length = atof(chptr2+2);
            break;
         
         case KW_PARAM:
            if (ignoreInstance) break;            
            break;

         case KW_CLOSEBRACE:
            if (ignoreInstance) break;
            if (inst.name==NULL) break;
            for (i=0; i<inst.outsideNodes.size(); i++)
               {
			   if (inst.outsideNodes[i]<0)
                  {
                  char dummyname[1024];        // no connect to a dummy node
                  sprintf(dummyname,"%s_%s_dac_unconnected%d",inst.wptr->bristles[i].name, inst.name, i);
                  current->nodes.push(nodetype(dummyname));
                  inst.outsideNodes[i] = current->nodes.size()-1;
                  }
			   }
			inst.translation.SetToIdentity();
			inst.translation.Translate(x,y);
			if (angle==0) ;
			else if (angle==90)
			   inst.translation.RotateCCW90();
			else if (angle==180)
			   inst.translation.Rotate180();
			else if (angle==270)
			   inst.translation.RotateCW90();
			else FATAL_ERROR;
			if (reflection==0) ;
			else if (reflection==1)
			   inst.translation.NegateY();
            if (inst.name == NULL) FATAL_ERROR;
            if (stricmp(inst.wptr->name, "nch")==0 || stricmp(inst.wptr->name, "pch")==0)
               FATAL_ERROR;
            else if (strncmp(inst.name, "IGNORE_",7)==0)
               printf("\nIgnoring instance %s of cell %s",inst.name, inst.wptr->name);
            else
               {
               current->instances.push(inst);
               for (int j=0; j<inst.outsideNodes.size(); j++)
                  if (inst.outsideNodes[j] >= current->nodes.size())
                     FATAL_ERROR;                        
               }            
            length = width = -1.0;
			x = y =0.0; angle=reflection=0;
            inst.name = NULL;
            lastkeyword = KW_NULL; // this cause us to ignore closing } if end of cell definition
            break;            

         case KW_NULLPIN:
            length = width = -1.0;
            x = y =0.0; angle=reflection=0;
            inst.name = NULL;
            lastkeyword = KW_NULL; // this cause us to ignore closing brace if end of cell definition
            break;

         case KW_PIN:
            if (ignoreInstance) break;
			if (stricmp(inst.wptr->name , "PV")==0)
			   {
			   printf("\nIgnoring bipolar device %s of cell %s",inst.name, inst.wptr->name);
			   length = width = -1.0;
			   inst.name = NULL;
			   lastkeyword = KW_NULL; // this cause us to ignore closing brace if end of cell definition
			   break;
			   }
			if (stricmp(inst.wptr->name , "DB")==0)
			   {
			   printf("\nIgnoring diode %s of cell %s",inst.name, inst.wptr->name);
			   length = width = -1.0;
			   inst.name = NULL;
			   lastkeyword = KW_NULL; // this cause us to ignore closing brace if end of cell definition
			   break;
			   }

            while (chptr!=NULL && *chptr!=0)
               {
               if (*chptr=='}')
                  {
                  for (i=0; i<inst.outsideNodes.size(); i++)
                     if (inst.outsideNodes[i]<0)
                        {
//                        printf("\nBristle %s is not connected for instance %s(%s).",inst.wptr->bristles[i].name, inst.name, inst.wptr->name);
                        char dummyname[1024];        // no connect to a dummy node
                        sprintf(dummyname,"%s_%s_dac_unconnected%d",inst.wptr->bristles[i].name, inst.name, i);
                        current->nodes.push(nodetype(dummyname));
                        inst.outsideNodes[i] = current->nodes.size()-1;
                        }
                  if (inst.name == NULL) FATAL_ERROR;
                  if (strncmp(inst.name, "IGNORE_",7)==0 || strncmp(inst.name, "XIGNORE_",8)==0 || strncmp(inst.name, "RIGNORE_",8)==0 || strncmp(inst.name, "MIGNORE_",8)==0)
                     printf("\nIgnoring instance %s of cell %s",inst.name, inst.wptr->name);
                  else if (stricmp(inst.wptr->name , "nch")==0 || stricmp(inst.wptr->name , "nch_hvt")==0 || stricmp(inst.wptr->name , "nch_lvt")==0 || stricmp(inst.wptr->name , "nchpg_sr")==0 || stricmp(inst.wptr->name , "nchpd_sr")==0)
                     {
                     devicetype d;
                     d.name   = inst.name;
                     d.drain  = inst.outsideNodes[0];
                     d.gate   = inst.outsideNodes[1];
                     d.source = inst.outsideNodes[2];
                     d.length = length;
                     d.width  = width;
                     d.r      = length / width;
                     d.type   = D_NMOS;
                     d.vt     = (stricmp(inst.wptr->name , "nch_hvt")==0) ? VT_HIGH : (stricmp(inst.wptr->name , "nch_lvt")==0) ? VT_LOW : (stricmp(inst.wptr->name , "nchpg_sr")==0) ? VT_SRAM : (stricmp(inst.wptr->name , "nchpd_sr")==0) ? VT_SRAM : VT_NOMINAL;
//					 d.x      = x;
//					 d.y      = y;
                     current->devices.push(d);
                     }
                  else if (stricmp(inst.wptr->name , "pch")==0 || stricmp(inst.wptr->name , "pch_hvt")==0 || stricmp(inst.wptr->name , "pch_lvt")==0 || stricmp(inst.wptr->name , "pchpu_sr")==0)
                     {
                     devicetype d;
                     d.name   = inst.name;
                     d.drain  = inst.outsideNodes[0];
                     d.gate   = inst.outsideNodes[1];
                     d.source = inst.outsideNodes[2];
                     d.length = length;
                     d.width  = width;
                     d.r      = NMOS_OVER_PMOS_STRENGTH * length / width;
                     d.type   = D_PMOS;
                     d.vt     = (stricmp(inst.wptr->name , "pch_hvt")==0) ? VT_HIGH : (stricmp(inst.wptr->name , "pch_lvt")==0) ? VT_LOW : (stricmp(inst.wptr->name , "pchpu_sr")==0) ? VT_SRAM : VT_NOMINAL;
//					 d.x      = x;
//					 d.y      = y;
                     current->devices.push(d);
                     }
                  else if (stricmp(inst.wptr->name, "r")==0 || stricmp(inst.wptr->name, "pr")==0)
                     {
                     devicetype d;
                     d.name   = inst.name;
                     d.drain  = inst.outsideNodes[0];
                     d.gate   = VDD;
                     d.source = inst.outsideNodes[1];
                     d.length = 1.0;
                     d.width  = 1.0;
                     d.r      = 1.0;
                     d.type   = D_NMOS;
//					 d.x      = x;
//					 d.y      = y;
                     current->devices.push(d);
                     }
                  else if (stricmp(inst.wptr->name, "c1")==0 || stricmp(inst.wptr->name, "c2")==0)
                     {
                     devicetype d;
                     d.name   = inst.name;
                     d.drain  = VSS;
                     d.gate   = VDD;
                     d.source = VSS;
                     d.length = length;
                     d.width  = width;
                     d.r      = 1.0;
                     d.type   = D_DCAP;
//					 d.x      = x;
//					 d.y      = y;
                     current->devices.push(d);
                     }
                  else
                     {
					 inst.translation.SetToIdentity();
					 inst.translation.Translate(x,y);
					 if (angle==0) ;
					 else if (angle==90)
						inst.translation.RotateCCW90();
					 else if (angle==180)
						inst.translation.Rotate180();
					 else if (angle==270)
						inst.translation.RotateCW90();
					 else FATAL_ERROR;
					 if (reflection==0) ;
					 else if (reflection==1)
						inst.translation.NegateY();
                     
                     current->instances.push(inst);
                     for (int j=0; j<inst.outsideNodes.size(); j++)
                        if (inst.outsideNodes[j] >= current->nodes.size())
                           FATAL_ERROR;

                     }
                  length = width = -1.0;
			      x = y =0.0; angle=reflection=0;
                  inst.name = NULL;
                  lastkeyword = KW_NULL; // this cause us to ignore closing brace if end of cell definition
                  break;
                  }
               int index;
               chptr2 = strchr(chptr, '=');
               if (chptr2==NULL) FATAL_ERROR;
               *chptr2=0;

               if (strchr(chptr,'/')!=NULL)
                  {  // I want to get rid of X
                  if (chptr[0]=='X')
                     chptr++;
                  }
               
               if (!current->h.Check(chptr, index))
                  {
                  current->nodes.push(nodetype(chptr));
                  index = current->nodes.size()-1;
                  current->h.Add(chptr, index);
                  }
               name = chptr = chptr2+1;
               chptr2 = strchr(chptr, ' ');
               if (chptr2)
                  {
                  *chptr2=0;
                  chptr=chptr2+1;
                  }
               else
                  {
                  chptr2 = strchr(chptr, '}');
                  if (chptr2)
                     {
                     *chptr2 = 0;
                     chptr=chptr2+1;
                     }
                  else chptr=NULL;
                  }

               for (i=inst.wptr->bristles.size()-1; i>=0; i--)
                  if (inst.wptr->bristles[i] == name)
                     break;
               if (i<0) FATAL_ERROR;
               inst.outsideNodes[i] = index;
               }
            break;
         }
      }

   fp.fclose();
   return structures.back();
   }




struct stattype{
   int p, n;
   float pwidth, nwidth;
   int copies;
   bool beenhere;
   const wirelisttype *wptr;
   int depth;
   devicetype dcap[10];

   stattype(const wirelisttype *_wptr=NULL) : wptr(_wptr)
      {
      p      = 0;
      n      = 0;
      pwidth = 0.0;
      nwidth = 0.0;
      copies = 0;
      beenhere = false;
      depth  = 0;
      }
   bool operator<(const stattype &right) const
      {
      if (depth == right.depth)
         return strcmp(wptr->name, right.wptr->name)<0;
      return depth > right.depth;
      }
   };
   
void Statistics(const wirelisttype *top, bool ignoreMems)
   {
   TAtype ta("Wirelist Statistics");
   array <stattype> stats;
   hashtype <const wirelisttype *, int> h;
   array <const wirelisttype *> stack;
   array <const wirelisttype *> stack2;
   const wirelisttype *wptr;
   int index, i, k, dummy;
   int depth=0;

   stack.push(top);
   
   stats.push(stattype(top));
   h.Add(top, 0);

   while (stack.pop(wptr))
      {
      if (!h.Check(wptr, index))
         FATAL_ERROR;
      stats[index].copies++;
      depth = stats[index].depth;
      for (i=0; i<wptr->instances.size(); i++)
         if (!ignoreMems || (strncmp(wptr->instances[i].wptr->name, "spr", 3)!=0 && 
                             strncmp(wptr->instances[i].wptr->name, "dpr", 3)!=0 && 
                             strncmp(wptr->instances[i].wptr->name, "srf", 3)!=0 && 
                             strncmp(wptr->instances[i].wptr->name, "drf", 3)!=0))
            {         
            stack.push(wptr->instances[i].wptr);
            
            if (!h.Check(wptr->instances[i].wptr, dummy))
               {
               stack2.push(wptr->instances[i].wptr);
               stats.push(stattype(wptr->instances[i].wptr));
               
               index = stats.size()-1;
               h.Add(wptr->instances[i].wptr, index);
               for (k=0; k<wptr->instances[i].wptr->devices.size(); k++)
                  {
                  const devicetype &d = wptr->instances[i].wptr->devices[k];
                  if (d.type==D_NMOS)
                     {
                     stats[index].n++;
                     stats[index].nwidth += d.width;
                     }
                  else if (d.type==D_PMOS)
                     {
                     stats[index].p++;
                     stats[index].pwidth += d.width;
                     }
                  else if (d.type==D_DCAP)
                     {
                     for (int m=0; m<10; m++)
                        {
                        if (stats[index].dcap[m].length == 0.0)
                           { 
                           stats[index].dcap[m] = d;
                           break;
                           }
                        else if (stats[index].dcap[m].length == d.length)
                           {
                           stats[index].dcap[m].width += d.width;
                           break;
                           }
                        }
                     }
                  }            
               stats[index].depth = depth + 1;
               }
            else
               stats[dummy].depth = MAXIMUM(stats[dummy].depth, depth +1);
            }
      }

   array <stattype *> sortptrs;
   for (i=0; i<stats.size(); i++)
      sortptrs.push(&stats[i]);
   std::sort(sortptrs.begin(), sortptrs.end(), indirect_lessthan<stattype *>());

   for (i=0; i<sortptrs.size(); i++)
      {
      stattype &stat = *sortptrs[i];
      wptr = stat.wptr;
      if (!h.Check(wptr, index))
         FATAL_ERROR;
      
      if (stat.beenhere)
         FATAL_ERROR;
      stat.beenhere = true;
      for (k=0; k<wptr->instances.size(); k++)
         if (!ignoreMems || (strncmp(wptr->instances[k].wptr->name, "spr", 3)!=0 && 
                             strncmp(wptr->instances[k].wptr->name, "dpr", 3)!=0 && 
                             strncmp(wptr->instances[k].wptr->name, "srf", 3)!=0 && 
                             strncmp(wptr->instances[k].wptr->name, "drf", 3)!=0))
            {
            int child;
            if (!h.Check(wptr->instances[k].wptr, child))
               FATAL_ERROR;
            stat.n += stats[child].n;
            stat.p += stats[child].p;
            stat.nwidth += stats[child].nwidth;
            stat.pwidth += stats[child].pwidth;
            for (int m=0; m<10; m++)
               {
               if (stats[child].dcap[m].length!=0.0)
                  {
                  for (int l=0; l<10; l++)
                     {
                     if (stat.dcap[l].length==0)
                        {
                        stat.dcap[l] = stats[child].dcap[m];
                        break;
                        }
                     else if (stat.dcap[l].length == stats[child].dcap[m].length)
                        {
                        stat.dcap[l].width += stats[child].dcap[m].width;
                        break;
                        }
                     }
                  }
               }
            }
      }

   stack = stack2;
   filetype fp;
   fp.fopen("stat.csv","wt");
   for (i=0; i<sortptrs.size(); i++)
      {
      stattype &stat = *sortptrs[i];
      printf("\n%30s %5d %8d %8d %9.3g %9.3g (%8d %8d %9.3g %9.3g)", stat.wptr->name, stat.copies, stat.n, stat.p, stat.nwidth, stat.pwidth, stat.n*stat.copies, stat.p*stat.copies, stat.nwidth*stat.copies, stat.pwidth*stat.copies);
      fp.fprintf("\n%s,%d,%d,%d,%.3g,%.3g", stat.wptr->name, stat.copies, stat.n, stat.p, stat.nwidth, stat.pwidth, stat.n*stat.copies, stat.p*stat.copies, stat.nwidth*stat.copies, stat.pwidth*stat.copies);
      }
   for (i=0; i<10; i++)
      {
      stattype &stat = *sortptrs.back();
      if (stat.dcap[i].length!=0.0){
         printf("\nDcap Length=%.3f Width=%.3f",stat.dcap[i].length,stat.dcap[i].width);
         fp.fprintf("\nDcap Length=%.3f Width=%.3f",stat.dcap[i].length,stat.dcap[i].width);
         }
      }
   

   fp.fclose();
   }

