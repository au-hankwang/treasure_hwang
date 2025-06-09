#include "pch.h"
#include "template.h"
#include "multithread.h"
#include "circuit.h"
#include "wirelist.h"


#ifdef _MSC_VER
 #pragma optimize("",off)
#endif

const float NMOS_OVER_PMOS_STRENGTH = 2.3;
static bool DEBUG = false;


void MinSOP(const bitfieldtype <max_input_space> &input, array <unsigned> &var, const int num_inputs);
void MinSOP(array <unsigned> &var);


array <wirelisttype *> structures;
texthashtype <int> hstructs;
array <udptype> ccr_circuittype::udps;
char unique_string[64];

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


void udptype::WriteCombinationalPrimitive(filetype &fp, int uu, bool atpg, bool assertionCheck, bool _addDelay) const
   {
   TAtype ta("udptype::WritePrimitive");
   int i, k;
   int instance=0;
 
   fp.fprintf("\n\nmodule mdp_%03d%s (out", uu, unique_string);
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
   if (atpg)
      WriteBool(fp, "out", instance, (table & ~xtable), num_inputs);
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
         fp.fprintf("\n\nudp_%03d%s %si0 (out", uu, unique_string, (addDelay||_addDelay) ? "#(0.01) ":"");
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
         fp.fprintf( "\n\nudp_%03d%s %si0(out",uu,unique_string, (addDelay||_addDelay) ? "#(0.01) ":"");
         for (i=0; i<num_inputs; i++)
            fp.fprintf(", %c", i+'a');
         fp.fprintf(");");
         }
      }
   fp.fprintf("\nendmodule");

   array <unsigned> var0, var1;  // high 16 bits will represent no variable
   int val;
     
   if (atpg)
      fp.fprintf("\n\n/*primitive udp_%03d%s (out", uu, unique_string);
   else
      fp.fprintf("\n\nprimitive udp_%03d%s (out", uu, unique_string);
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
   if (atpg)
      fp.fprintf("\n*/");
   }

void udptype::WriteMdpWrapper(filetype &fp, int uu, bool atpg, bool assertionCheck, const array<bool> &delays) const
   {
   int i;

   fp.fprintf("\nmodule mdp_%03d%s (out", uu, unique_string);   
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
   fp.fprintf(";\n");

   for (i=0; i<num_inputs; i++)
	  {
	  if (delays[i])
		 fp.fprintf("\nbuf #(0.01) id%d(delay_%c, %c);",i,i+'a',i+'a');
	  }

   fp.fprintf( "\nudp_%03d%s #(0.01) iudp(out", uu, unique_string);
   for (i=0; i<num_inputs; i++)
      fp.fprintf(", %s%c", delays[i]?"delay_":"", i+'a');
   fp.fprintf(");\nendmodule");

   fp.fprintf("\n\nprimitive udp_%03d%s (out", uu, unique_string);
   for (i=0; i<num_inputs; i++)
      fp.fprintf(",%c",'a'+i);
   fp.fprintf(");\noutput out; \nreg out;");
   fp.fprintf("\ninput a");
   for (i=1; i<num_inputs; i++)
      fp.fprintf(", %c",i+'a');
   if (assertionCheck)
      fp.fprintf(";\ninitial out = 1'b0");
   fp.fprintf(";\ntable");
   }
   
void udptype::WriteSequentialPrimitive(filetype &fp, int uu, bool atpg, bool assertionCheck) const
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
      fp.fprintf("\n/*mdp_%03d", uu);  
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
         fp.fprintf(" : %c",xtable.GetBit(iteration) ? 'x' : (table.GetBit(iteration) ? '1' : '0'));
         fp.fprintf(" (%x);",iteration);
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
   // basicly if the next state has no dependency on current state
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
      WriteCombinationalPrimitive(fp, uu, atpg, assertionCheck, true);
      return;
      }

   if (num_inputs+num_states>16)
      {
      fp.fprintf("\n// ERROR. This udp has too many inputs/states for me to handle.");
      printf("\nI have a sequential S/D region with too many inputs(%d). It will be ignored.",num_inputs);
      return;
      }

   if (num_inputs<1)
	  {
      array <bool> delays(num_inputs,false);
	  WriteMdpWrapper(fp, uu, atpg, assertionCheck, delays);
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

// now try to compress the table, and resolve the number of Q states needed
  
   int total_states=0;
   for (iteration=0; iteration<(1<<(num_states)); iteration++)
      {
      if (legit[iteration])
         total_states++;
      }
   int qvar=-1;
   bool outputIsInverted = false;
   for (i=0; i<num_states; i++)
      {
      if (qx[i] == xtable && qt[i] == table)
         qvar = i;
      }
   if (qvar <0)
      {
	  bitfieldtype <max_input_space> nottable;
	  int iteration;
	  
	  for (iteration=0; iteration<(1<<(num_inputs+num_states)); iteration++)
		 if (!table.GetBit(iteration) && !xtable.GetBit(iteration))
			nottable.SetBit(iteration); 
		 for (i=0; i<num_states; i++)
			{
			if (qx[i] == xtable && qt[i] == nottable)
			   {
			   qvar = i;
			   outputIsInverted = true;
			   }
			}
	  }
   if (qvar <0)
      {
      fp.fprintf("\n// ERROR. I have a output which isn't a state variable in this sequential circuit.");
      printf("\nI have a output which isn't a state variable in this sequential circuit.");
      return;
      }   
   

   if (total_states==4)
      {                    
	  // I may be able to represent this state diagram as a edge triggered udp, or maybe not
	  // This is most likely a flop(either sa-casc or master-slave)
	  
	  // first identify the four states. 
	  // I know the active states because they're nextstate is either self and precharge and q is same for both
	  int prechargelow=-1, prechargehigh=-1, activelow=-1, activehigh=-1;
	  int clk=-1;
	  bool clkassertion=false;  // false is negedge, true is posedge
	  for (iteration=0; iteration<(1<<(num_inputs+num_states)); iteration++)
		 {
		 int nextstate = 0;
		 int currentstate=iteration>>num_inputs;		 
		 for (i=0; i<num_states; i++)
			{
			if (nextstate>=0 && !qx[i].GetBit(iteration))
			   nextstate += qt[i].GetBit(iteration) ? (1<<i) : 0;
			else
			   nextstate = -1;
			}
		 if (legit[currentstate])
			{
			bool q     = (currentstate>>qvar)&1;
			bool nextq = (nextstate>>qvar)&1;

			if (q!=nextq && q) 
			   {
			   prechargehigh = currentstate;
			   activelow     = nextstate;
			   }
			if (q!=nextq && !q)
			   {
			   prechargelow  = currentstate;
			   activehigh     = nextstate;
			   }
			}
		 }
//	  if (prechargelow<0 || activelow<0 || prechargehigh<0 || activehigh<0)
//         FATAL_ERROR;
	  if (prechargelow>=0 && activelow>=0 && prechargehigh>=0 && activehigh>=0)
		 {
		 // now validate the earlier guesses were correct(ie this really is a flop)
		 // also determine what signal is clock and what the assertion level is
		 unsigned oneinmask = 0, zeroinmask = 0;
		 for (iteration=0; iteration<(1<<(num_inputs+num_states)); iteration++)
			{
			int nextstate = 0;
			int currentstate=iteration>>num_inputs;		 
			for (i=0; i<num_states; i++)
			   {
			   if (nextstate>=0 && !qx[i].GetBit(iteration))
				  nextstate += qt[i].GetBit(iteration) ? (1<<i) : 0;
			   else
				  nextstate = -1;
			   }
			if (legit[currentstate])
			   {
			   bool q     = (currentstate>>qvar)&1;
			   bool nextq = (nextstate>>qvar)&1;
			   
			   if (q!=nextq &&  q && prechargehigh!=currentstate)
				  FATAL_ERROR;
			   if (q!=nextq && !q && prechargelow!=currentstate)
				  FATAL_ERROR;
			   if (currentstate==activehigh && (q!=nextq || !q))
				  FATAL_ERROR;
			   if (currentstate==activelow && (q!=nextq || q))
				  FATAL_ERROR;
			   
			   for (i=0; i<num_inputs; i++)
				  {
				  if ((iteration>>i)&1)
					 {	 // i is input bit and is 1
					 if (currentstate==activehigh && nextstate==prechargehigh)
						oneinmask |= 1<<i;
					 if (currentstate==activelow && nextstate==prechargelow)
						oneinmask |= 1<<i;
					 }
				  else
					 {	 // i is input bit and is 0
					 if (currentstate==activehigh && nextstate==prechargehigh)
						zeroinmask |= 1<<i;
					 if (currentstate==activelow && nextstate==prechargelow)
						zeroinmask |= 1<<i;
					 }
				  }
			   }
			}
		 // I should have exactly 1 bit left unset. This will correspond to the clk
		 // input. It also tells me the assertion level of clock
		 for (i=0; i<num_inputs; i++)
			{
			if (((zeroinmask>>i)&1)==0)
			   {
			   if (clk>=0) FATAL_ERROR;
			   clk = i;
			   clkassertion = false;
			   }
			if (((oneinmask>>i)&1)==0)
			   {
			   if (clk>=0) FATAL_ERROR;
			   clk = i;
			   clkassertion = true;
			   }
			}
		 if (clk<0) FATAL_ERROR;
		 
		 // I now know which input will be edge triggered. Now do minimum SOP to remaining variables.
		 // I'll also compare that the q result doesn't matter which precharge state you're coming from
		 array <unsigned> var0_low, var1_low, var0_high, var1_high;
		 for (iteration=0; iteration<(1<<num_inputs); iteration++)
			{
			if (!qx[qvar].GetBit(iteration | (prechargelow<<num_inputs)) && ((iteration>>clk)&1) == clkassertion)
			   {
			   if (!qt[qvar].GetBit(iteration | (prechargelow<<num_inputs)))
				  var0_low.push(iteration);
			   else
				  var1_low.push(iteration);
			   }
			if (!qx[qvar].GetBit(iteration | (prechargehigh<<num_inputs)) && ((iteration>>clk)&1) == clkassertion)
			   {
			   if (!qt[qvar].GetBit(iteration | (prechargehigh<<num_inputs)))
				  var0_high.push(iteration);
			   else
				  var1_high.push(iteration);
			   }
			}
		 if (var0_low.size() != var0_high.size() || var1_low.size() != var1_high.size())
			FATAL_ERROR;
		 MinSOP(var0_low);  // compute minimum sum of products
		 MinSOP(var1_low);
		 
		 array <bool> delays(num_inputs,false);
		 WriteMdpWrapper(fp, uu, atpg, assertionCheck, delays);
		 
		 char ch;
		 int k;
		 for (int val=0; val<=1; val++)
			{
			array <unsigned> var = (val==0 ? var0_low : var1_low);
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
			   fp.fprintf(" : %d ;",outputIsInverted ? !val : val);
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
		 fp.fprintf(" : ? : - ;",ch);
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
   
   if (total_states==3)    // I may be able to represent this state diagram as a edge triggered udp, or maybe not
      {                    // This is most likely a sense amp, i want to make a trial assumption that a given state is precharge and try to eliminate it with edge triggers
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
         
	     array <bool> delays(num_inputs,false);
		 WriteMdpWrapper(fp, uu, atpg, assertionCheck, delays);

         char ch;
         int k, val;
         for (val=0; val<=1; val++)
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
               fp.fprintf(" : %d ;",outputIsInverted ? !val : val);
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
		 val = (precharge>>qvar)&1;
         fp.fprintf(" : %d ;",outputIsInverted ? !val : val);
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
		 val = (precharge>>qvar)&1;
         fp.fprintf(" : %d ;",outputIsInverted ? !val : val);
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
      int legitcount=0;
      for (i=0; i<legit.size(); i++)
         legitcount += legit[i] ? 1:0;

      if (legitcount!=2)
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
      
	  array <bool> delays(num_inputs,false);
	  WriteMdpWrapper(fp, uu, atpg, assertionCheck, delays);
	  
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
               ch = ((var[i]>>num_inputs)&1) ? (outputIsInverted ? '0':'1') : (outputIsInverted ? '1':'0');
            fp.fprintf(" : %c",ch);
            fp.fprintf(" : %d ;",outputIsInverted ? !val : val);
            }
         }   
      fp.fprintf("\nendtable\nendprimitive\n");
      return;
      }
   else {
      // now make a var0,var1 list which I will compute a minimum sum of products on
      // I'll make qvar be look like another variable in these lists
      // I'll also make the output be conservative with respect to X's
      // one problem I have is that is to correctly detect the metastable state on cross coupled nands
      // I'm going to ignore this if it requires the change of more than one input simultaneously

      fp.fprintf("// INFO. I'm modelling this %d state element as a simple latch. Probably is cross coupled nands\n",total_states);
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
					 if (qx[m].GetBit(iteration^k))
						next = -1;
                     }
                  if (next == i) unreachable=false;
                  }
			   unreachable = false;  // I don't think I trust this so this effectively comments it out

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
	  // I also have the problem where I can get a race. This happens often with glat embedded with keeper
	  // if the clock transition before the precharge the zz node will float causing an X
	  // the fix is to delay the clock relative to the pulldowns/pullups
	  array <bool> early(num_inputs, false);
	  array <bool> late(num_inputs, false);

      for (i=0; i< (1<<num_states) && num_inputs; i++)
         {
         for (iteration=0; iteration<(1<<num_inputs) && legit[i]; iteration++)
            {
			int k, switch1, switch2, state1, finalstate1, state2, finalstate2, nextstate;
			bool x1, x2, finalx1, finalx2;
			
			nextstate = 0;
			for (k=0; k<num_states; k++)
			   {
			   if (qx[k].GetBit((iteration)| (i<<num_inputs)))
				  nextstate = -1;
			   else if (nextstate>=0)
				  nextstate |= qt[k].GetBit((iteration) | (i<<num_inputs)) <<k;
			   }

			// only consider starting states that are stable(state==nextstate)
			for (switch1=0; switch1<num_inputs && i==nextstate; switch1++)
			   {
			   for (switch2=0; switch2<num_inputs; switch2++)
				  {
				  bool always1=true, always0=true, alwaysx=true;
				  for (k=0; k<(1<<num_states); k++)
					 {
					 if (legit[k] && xtable.GetBit((iteration^(1<<switch1)^(1<<switch2))| (k<<num_inputs)))
						always0 = always1 = false;
					 else if (legit[k] && table.GetBit((iteration^(1<<switch1)^(1<<switch2))| (k<<num_inputs)))
						alwaysx = always0 = false;
					 else if (legit[k] && !table.GetBit((iteration^(1<<switch1)^(1<<switch2))| (k<<num_inputs)))
						alwaysx = always1 = false;
					 }
				  // if the final output is the same regardless of the start state then skip all the analysis below
				  if (alwaysx || (always0 && !always1) || (!always0 && always1) || switch1==switch2)
					 break;
				  state1 = 0;
				  for (k=0; k<num_states; k++)
					 {
					 if (qx[k].GetBit((iteration^(1<<switch1))| (i<<num_inputs)))
						state1 = -1;
					 else if (state1>=0)
						state1 |= qt[k].GetBit((iteration^(1<<switch1)) | (i<<num_inputs)) <<k;
					 }
				  x1 = xtable.GetBit((iteration^(1<<switch1)) | (i<<num_inputs));
				  // state1 is the state after the first switch, x1 is the output after the first switch
				  finalstate1 = 0;
				  for (k=0; k<num_states && state1>=0; k++)
					 {
					 if (qx[k].GetBit((iteration^(1<<switch1)^(1<<switch2)) | (state1<<num_inputs)))
						finalstate1 = -1;
					 else if (state1>=0 && finalstate1>=0)
						finalstate1 |= qt[k].GetBit((iteration^(1<<switch1)^(1<<switch2)) | (state1<<num_inputs)) <<k;
					 else
						finalstate1 = -1;
					 }
				  if (state1<0) finalstate1 = -1;
				  finalx1 = state1<0 ? true : xtable.GetBit((iteration^(1<<switch1)^(1<<switch2)) | (state1<<num_inputs));
				  // now switch the other direction			   
				  state2 = 0;
				  for (k=0; k<num_states; k++)
					 {
					 if (qx[k].GetBit((iteration^(1<<switch2)) | (i<<num_inputs)))
						state2 = -1;
					 else if (state1>=0)
						state2 |= qt[k].GetBit((iteration^(1<<switch2)) | (i<<num_inputs)) <<k;
					 }
				  x2 = xtable.GetBit((iteration^(1<<switch2)) | (i<<num_inputs));
				  // state1 is the state after the first switch, x1 is the output after the first switch
				  finalstate2 = 0;
				  for (k=0; k<num_states && state2>=0; k++)
					 {
					 if (qx[k].GetBit((iteration^(1<<switch1)^(1<<switch2)) | (state2<<num_inputs)))
						finalstate2 = -1;
					 else if (state2>=0 && finalstate2>=0)
						finalstate2 |= qt[k].GetBit((iteration^(1<<switch1)^(1<<switch2)) | (state2<<num_inputs)) <<k;
					 else
						finalstate2 = -1;
					 }
				  if (state2<0) finalstate2 = -1;
				  finalx2 = state2<0 ? true : xtable.GetBit((iteration^(1<<switch1)^(1<<switch2)) | (state2<<num_inputs));
				  
				  if (finalstate1>=0 && finalstate2<0)
					 {
					 early[switch1]=true;
					 late[switch2] = true;
					 }
				  if (finalstate1<0 && finalstate2>=0)
					 {
					 early[switch2]=true;
					 late[switch1] = true;
					 }
				  }
			   }
			}
		 }
	  array <bool> delays(num_inputs,false);
	  int delay_count=0;
	  for (i=0; i<num_inputs; i++)
		 {
		 delays[i] = late[i];
		 delay_count += late[i] ? 1:0;
		 }
	  if (delay_count==num_inputs)
		 delays.resize(num_inputs, false);

      MinSOP(input0, var0, num_inputs+1);  // compute minimum sum of products
      MinSOP(input1, var1, num_inputs+1);

	  WriteMdpWrapper(fp, uu, atpg, assertionCheck, delays);
      
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
            fp.fprintf(" : %d ;",outputIsInverted ? !val : val);
            }
         }   
      fp.fprintf("\nendtable\nendprimitive\n");
      return;
      }
   }


// to speed up the circuit analysis I want to cross reference the nodes so that only
// active nodes for this circuit appear and not the entire module
void ccr_circuittype::ConvertToCircuittype(const ccrtype &ccr)
   {
   TAtype ta("ccr_circuittype::ConvertToCircuittype()");
   int i;

   array <int> xref(parent.nodes.size(), -1);
   array <bool> inuse(parent.nodes.size(), false);

   devices = ccr.devices;

   inuse[VSS] = true;
   inuse[VDD] = true;
   inuse[VDDO] = true;
   
   for (i=0; i<devices.size(); i++)
	  {
	  const devicetype &d = devices[i];
	  
	  inuse[d.source] = true;
	  inuse[d.gate] = true;
	  inuse[d.drain] = true;
	  }
   if (ccr.type==CCR_SRAM_WRITENABLE)
	  inuse[ccr.outputs[0]] = true;
   
   for (i=0; i<inuse.size(); i++)
	  {
	  if (inuse[i])
		 {
		 nodes.push(nodetype(parent.nodes[i]));
		 xref[i] = nodes.size()-1;
		 }
	  }
   for (i=0; i<devices.size(); i++)
	  {
	  devicetype &d = devices[i];
	  
	  d.source = xref[d.source];
	  d.gate   = xref[d.gate];
	  d.drain  = xref[d.drain];
	  }
   inputs  = ccr.inputs;
   outputs = ccr.outputs;
   states  = ccr.states;

   for (i=0; i<inputs.size(); i++)
	  {
	  if (xref[inputs[i]]<0) FATAL_ERROR;
	  inputs[i] = xref[inputs[i]];
	  }

   for (i=0; i<outputs.size(); i++)
	  {
	  if (xref[outputs[i]]<0) FATAL_ERROR;
	  outputs[i] = xref[outputs[i]];
	  }
   
   for (i=states.size()-1; i>=0; i--)
	  {
	  if (states[i]<=VDDO || xref[states[i]]<0) 
//		 FATAL_ERROR;
		 states.RemoveFast(i);	   // this can happen sometimes in a dummy cell
	  else
	     states[i] = xref[states[i]];
	  }
   }


void ccr_circuittype::SequentialTruthTable(const ccrtype &ccr, filetype &fp, int &inum)
   {
   TAtype ta("ccr_circuittype::SequentialTruthTable()");
   int i, oo, iteration;
   char buffer[512];   

   ConvertToCircuittype(ccr);

/*   if (states.size() >2)
      {
      fp.fprintf("// ERROR. This udp has too many states for me to handle.\n// ERROR You will have some signals undriven.\n");
      printf("I have a sequential S/D region with too many states(%d). It will be ignored.\n",states.size());
      return;
      }*/
   
/*
   if (states.size() <2)
      {
      fp.fprintf("// ERROR. This udp has only one state.\n// ERROR You will have some signals undriven.\n");
      printf("I have a sequential S/D region with only 1 state. It will be ignored.\n");
      return;
      }*/
   CrossReference();
   if (DEBUG)
      {
      char buf2[512], buf3[512];
      fp.fprintf("\n");
      for (i=0; i<inputs.size(); i++)
         fp.fprintf("//input  %2d -> %s\n", i, nodes[inputs[i]].VName(buffer));
      for (i=0; i<outputs.size(); i++)
         fp.fprintf("//output %2d -> %s\n", i, nodes[outputs[i]].VName(buffer));
      for (i=0; i<states.size(); i++)
         fp.fprintf("//states %2d -> %s\n", i, nodes[states[i]].VName(buffer));
      for (i=0; i<devices.size(); i++)
         fp.fprintf("//%s(%s) %-10s %-10s %-10s %.1f/%.1f\n", devices[i].type==D_NMOS ? "NMOS" : "PMOS", devices[i].name, nodes[devices[i].source].VName(buffer),nodes[devices[i].gate].VName(buf2),nodes[devices[i].drain].VName(buf3),devices[i].width,devices[i].length);
      }

   if ((1 << (inputs.size() + states.size()) > max_input_space) || (inputs.size() + states.size())>=32)
      {
      char buf2[512], buf3[512];
      fp.fprintf("// ERROR. This udp has too many inputs for me to handle.\n");
      for (i=0; i<inputs.size(); i++)
         fp.fprintf("//input  %2d -> %s\n", i, nodes[inputs[i]].VName(buffer));
      for (i=0; i<outputs.size(); i++)
         fp.fprintf("//output %2d -> %s\n", i, nodes[outputs[i]].VName(buffer));
      for (i=0; i<states.size(); i++)
         fp.fprintf("//states %2d -> %s\n", i, nodes[states[i]].VName(buffer));
      for (i=0; i<devices.size(); i++)
         fp.fprintf("//%s(%s) %-10s %-10s %-10s\n", devices[i].type==D_NMOS ? "NMOS" : "PMOS", devices[i].name, nodes[devices[i].source].VName(buffer),nodes[devices[i].gate].VName(buf2),nodes[devices[i].drain].VName(buf3));
      printf("I have a sequential S/D region with too many inputs(%d). It will be ignored.\n",inputs.size());
      return;
      }
   if (udps.size()>=6)
	  printf("");
   
   us.clear();
   us.resize(outputs.size(), udptype(true, states.size()));  // this is a fair bit of memory so don't allocate till needed
   array <valuetype> outs;
   array <valuetype> qq;
   int k=0;
   for (iteration=0; iteration<(1<<(inputs.size()+states.size())); iteration++)
      {
	  const int iterationWithZeroLastTwoInputs = iteration & ~(1<<(inputs.size()-1)) & ~(1<<(inputs.size()-2));
      bool drive0 = false, drive1 = false;
      outs.resize(0);
      qq.resize(states.size(), V_X);
	  Initialize();
      
      for (int tries=0; tries<states.size(); tries++)
         {
         for (i=0; i<inputs.size(); i++)
            {
            nodes[inputs[i]].value    = ((iteration>>i)&1) ? V_ONE : V_ZERO;
            nodes[inputs[i]].changed  = true;
            nodes[inputs[i]].constant = true;
            nodeheap.push(inputs[i], 0);
            }
         
         for (i=0; i<states.size(); i++)
            {
            nodes[states[i]].value    = ((iteration>>(inputs.size()+i))&1) ? V_ONE : V_ZERO;
            nodes[states[i]].changed  = true;
            nodes[states[i]].constant = tries==i;
            nodeheap.push(states[i], tries==i ? 0 : 2);
            }
         
         Propogate(0, false, true);
         for (i=0; i<states.size(); i++)
            {
            nodes[states[i]].value    = ((iteration>>(inputs.size()+i))&1) ? V_ONE : V_ZERO;
            nodes[states[i]].changed  = true;
            nodes[states[i]].constant = false;
            nodeheap.push(states[i], tries==i ? 0 : 2);
            AdjustDevicesOnThisNode(states[i], tries==i ? 0 : 2);
            }
         Propogate(0, false, true);
         for (oo=0; oo<outputs.size(); oo++)
            {
            if (tries == 0)
               {
               outs.push(nodes[outputs[oo]].value);
               for (i=0; i<states.size(); i++)
                  qq[i] = nodes[states[i]].value;
               }
            else if (outs[oo] != nodes[outputs[oo]].value)
               {
               outs[oo] = V_X;
               for (i=0; i<states.size(); i++)
                  if (nodes[states[i]].value != qq[i])
                     qq[i] = V_X;
               }			
            }
         }
      // There are some cases where I merged enough stuff together such that an X(short/open) in one 
	  // part of the circuit can't propogate. I want to notice these cases and not treat the output as X	  
	  PropogateX();

      for (oo=0; oo<outputs.size(); oo++)
         {
         for (i=0; i<states.size(); i++)
            {
			if (nodes[states[i]].value==V_ILLEGAL)
			   qq[i] = V_X;
            if (qq[i] == V_ZERO)
               {
               us[oo].qx[i].ClearBit(iteration);
               us[oo].qt[i].ClearBit(iteration);
               }
            else if (qq[i] == V_ONE)
               {
               us[oo].qx[i].ClearBit(iteration);
               us[oo].qt[i].SetBit(iteration);
               }
			else if (ccr.lastTwoInputsAreWeakPulldowns && iteration!=iterationWithZeroLastTwoInputs)
			   { // copy from a prior iteration which had the inputs as zero
			   us[oo].qx[i].MakeBit(iteration, us[oo].qx[i].GetBit(iterationWithZeroLastTwoInputs));
			   us[oo].qt[i].MakeBit(iteration, us[oo].qt[i].GetBit(iterationWithZeroLastTwoInputs));
			   }
            else
               {
               us[oo].qx[i].SetBit(iteration);
               }
            }
         if (nodes[outputs[oo]].value==V_Z || nodes[outputs[oo]].value==V_ILLEGAL)
			outs[oo] = V_X;


         if (outs[oo] == V_ZERO)
            {
            us[oo].xtable.ClearBit(iteration);
            us[oo].table.ClearBit(iteration);
            }
         else if (outs[oo] == V_ONE)
            {
            us[oo].xtable.ClearBit(iteration);
            us[oo].table.SetBit(iteration);
            }
		 else if (ccr.lastTwoInputsAreWeakPulldowns && iteration!=iterationWithZeroLastTwoInputs)
			{
			us[oo].xtable.MakeBit(iteration, us[oo].xtable.GetBit(iterationWithZeroLastTwoInputs));
			us[oo].table.MakeBit(iteration,  us[oo].table.GetBit(iterationWithZeroLastTwoInputs));
			}
         else  // treat floating nodes, X's, or crow bar cases as X's in truth table
            us[oo].xtable.SetBit(iteration);
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
      fp.fprintf( "mdp_%03d%s \tui_%-4d(%s", i, unique_string, inum++, nodes[outputs[oo]].VName(buffer));
      for (i=0; i<inputs.size(); i++)
		 {
		 if (inputs[i]==VSS)
			fp.fprintf(", %s", ccr.compliments[i] ? "1'b1":"1'b0");
		 else if (inputs[i]==VDD || inputs[i]==VDDO)
			fp.fprintf(", %s", ccr.compliments[i] ? "1'b0":"1'b1");
		 else
            fp.fprintf(", %s%s", ccr.compliments[i] ? "~":"", nodes[inputs[i]].VName(buffer));
		 }
      fp.fprintf(");\n");
      }

   return;
   }

void ccr_circuittype::CombinationalTruthTable(const ccrtype &ccr, filetype &fp, int &inum)
   {
   TAtype ta("ccrtype::CombinationalTruthTable()");
   int i, oo;
   char buffer[512];
   
   ConvertToCircuittype(ccr);

   CrossReference();
   if (DEBUG)
      {
      char buf2[512], buf3[512];
      fp.fprintf("\n");
      for (i=0; i<inputs.size(); i++)
         fp.fprintf("//input  %2d -> %s\n", i, nodes[inputs[i]].VName(buffer));
      for (i=0; i<outputs.size(); i++)
         fp.fprintf("//output %2d -> %s\n", i, nodes[outputs[i]].VName(buffer));
      for (i=0; i<devices.size(); i++)
         fp.fprintf("//%s(%s) %-10s %-10s %-10s\n", devices[i].type==D_NMOS ? "NMOS" : "PMOS", devices[i].name, nodes[devices[i].source].VName(buffer),nodes[devices[i].gate].VName(buf2),nodes[devices[i].drain].VName(buf3));
      }
   if ((1 << inputs.size() >= max_input_space)  || inputs.size()>16)
      {
      char buf2[512], buf3[512];
      printf("I have a combinational S/D region with too many inputs(%d). It will be ignored.\n",inputs.size());
      fp.fprintf("// ERROR. This udp has too many inputs for me to handle.\n");
      for (i=0; i<inputs.size(); i++)
         fp.fprintf("//input  %2d -> %s\n", i, nodes[inputs[i]].VName(buffer));
      for (i=0; i<outputs.size(); i++)
         fp.fprintf("//output %2d -> %s\n", i, nodes[outputs[i]].VName(buffer));
      for (i=0; i<devices.size(); i++)
         fp.fprintf("//%s(%s) %-10s %-10s %-10s\n", devices[i].type==D_NMOS ? "NMOS" : "PMOS", devices[i].name, nodes[devices[i].source].VName(buffer),nodes[devices[i].gate].VName(buf2),nodes[devices[i].drain].VName(buf3));
      return;
      }
/*   if (inputs.size()==0)
      {
      fp.fprintf("\n// INFO. This source/drain region will be deleted since it has no inputs.");
      return;
      }
*/   

   us.clear();
   us.resize(outputs.size(), udptype(false, 0));  // this is a fair bit of memory so don't allocate till needed
   Initialize();
   for (int iteration=0; iteration<(1<<inputs.size()); iteration++)
      {
      bool drive0 = false, drive1 = false;
      
      for (i=0; i<inputs.size(); i++)
         {
         nodes[inputs[i]].value    = (iteration>>i)&1 ? V_ONE : V_ZERO;
         nodes[inputs[i]].changed  = true;
         nodes[inputs[i]].constant = true;
         nodeheap.push(inputs[i], 1);
         }
      for (oo=0; oo<outputs.size(); oo++)
         {
		 nodeheap.push(outputs[oo], 2);
		 }
      
      Propogate(0, false, true);
      for (oo=0; oo<outputs.size(); oo++)
         {
         int n = outputs[oo];
         if (nodes[n].value == V_ZERO)
            {
            us[oo].xtable.ClearBit(iteration);
            us[oo].table.ClearBit(iteration);
            }
         else if (nodes[n].value == V_ONE || (nodes[n].value==V_Z && ccr.treatZasOne))
            {
            us[oo].xtable.ClearBit(iteration);
            us[oo].table.SetBit(iteration);
            }
         else  // treat floating nodes, X's, or crow bar cases as X's in truth table
            us[oo].xtable.SetBit(iteration);
         }
      }

   for (oo=0; oo<outputs.size(); oo++)
      {
      us[oo].num_inputs = inputs.size();
      us[oo].sequential = false;
	  us[oo].addDelay   = ccr.addDelay;
      if (inputs.size()==0)  // something has been tied off
         {
         fp.fprintf("buf \tui_%-4d(%s, ",inum++, nodes[outputs[oo]].VName(buffer));
         if (us[oo].xtable.GetBit(0))
            fp.fprintf( "1'bx);\n");
         else if (us[oo].table.GetBit(0))
            fp.fprintf( "1'b1);\n");
         else
            fp.fprintf( "1'b0);\n");
         }
      else if (inputs.size()==1 && !us[oo].xtable.GetBit(0) && !us[oo].xtable.GetBit(1) && us[oo].table.GetBit(0)!=us[oo].table.GetBit(1))  // either buf or inverter
         {
         if ((!us[oo].table.GetBit(0) && !ccr.compliments[0]) || (us[oo].table.GetBit(0) && ccr.compliments[0]))
            fp.fprintf("buf \tui_%-4d(%s",inum++, nodes[outputs[oo]].VName(buffer));
         else
            fp.fprintf("not \tui_%-4d(%s",inum++, nodes[outputs[oo]].VName(buffer));
         fp.fprintf(", %s);\n",nodes[inputs[0]].VName(buffer));
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
         fp.fprintf("mdp_%03d%s \tui_%-4d(%s", i, unique_string, inum++, nodes[outputs[oo]].VName(buffer));
         for (i=0; i<inputs.size(); i++)
			if (inputs[i]==VSS)
			   fp.fprintf(", %s", ccr.compliments[i] ? "1'b1":"1'b0");
			else if (inputs[i]==VDD || inputs[i]==VDDO)
			   fp.fprintf(", %s", ccr.compliments[i] ? "1'b0":"1'b1");
			else
			   fp.fprintf(", %s%s", ccr.compliments[i] ? "~":"", nodes[inputs[i]].VName(buffer));
         fp.fprintf(");\n");
         }
      }
   }


void ccr_circuittype::SramWritenableTruthTable(const ccrtype &ccr, filetype &fp, int &inum)
   {
   TAtype ta("ccrtype::SramWritenableTruthTable()");
   int i, oo;
   char buffer[512];

   ConvertToCircuittype(ccr);
   if (states.size()==0)
	  {
	  fp.fprintf("//INFO This ccr has no states and will be discarded(probably a dummy cell)\n");
	  return;
	  }

   CrossReference();
   if (DEBUG)
      {
      char buf2[512], buf3[512];
      fp.fprintf("\n");
      for (i=0; i<inputs.size(); i++)
         fp.fprintf("//input  %2d -> %s\n", i, nodes[inputs[i]].VName(buffer));
      for (i=0; i<outputs.size(); i++)
         fp.fprintf("//output %2d -> %s\n", i, nodes[outputs[i]].VName(buffer));
      for (i=0; i<devices.size(); i++)
         fp.fprintf("//%s(%s) %-10s %-10s %-10s\n", devices[i].type==D_NMOS ? "NMOS" : "PMOS", devices[i].name, nodes[devices[i].source].VName(buffer),nodes[devices[i].gate].VName(buf2),nodes[devices[i].drain].VName(buf3));
      }
   if ((1 << inputs.size() >= max_input_space)  || inputs.size()>16)
      {
      char buf2[512], buf3[512];
      printf("I have a combinational S/D region with too many inputs(%d). It will be ignored.\n",inputs.size());
      fp.fprintf("// ERROR. This udp has too many inputs for me to handle.\n");
      for (i=0; i<inputs.size(); i++)
         fp.fprintf("//input  %2d -> %s\n", i, nodes[inputs[i]].VName(buffer));
      for (i=0; i<outputs.size(); i++)
         fp.fprintf("//output %2d -> %s\n", i, nodes[outputs[i]].VName(buffer));
      for (i=0; i<devices.size(); i++)
         fp.fprintf("//%s(%s) %-10s %-10s %-10s\n", devices[i].type==D_NMOS ? "NMOS" : "PMOS", devices[i].name, nodes[devices[i].source].VName(buffer),nodes[devices[i].gate].VName(buf2),nodes[devices[i].drain].VName(buf3));
      return;
      }
/*   if (inputs.size()==0)
      {
      fp.fprintf("\n// INFO. This source/drain region will be deleted since it has no inputs.");
      return;
      }
*/   

   if (states.size()!=2)   // states are the 2 bitline nets I'll monitor to know if we're reading or writing
	  FATAL_ERROR;
   if (outputs.size()!=1)
	  FATAL_ERROR;

   us.clear();
   us.resize(outputs.size(), udptype(false, 0));  // this is a fair bit of memory so don't allocate till needed
   Initialize();
   for (int iteration=0; iteration<(1<<inputs.size()); iteration++)
      {
      bool drive0 = false, drive1 = false;
      
      for (i=0; i<inputs.size(); i++)
         {
         nodes[inputs[i]].value    = (iteration>>i)&1 ? V_ONE : V_ZERO;
         nodes[inputs[i]].changed  = true;
         nodes[inputs[i]].constant = true;
         nodeheap.push(inputs[i], 1);
         }
      for (i=0; i<states.size(); i++)
         {
         nodeheap.push(states[i], 2); // this helps me determine Z
		 nodes[states[i]].state = true;
         }
      
      Propogate(0, false, true);
	  valuetype v1 = nodes[states[0]].value, v2 = nodes[states[1]].value;
	  
	  us[0].xtable.ClearBit(iteration);
	  if ((v1==V_ONE || v1==V_Z) && (v2==V_ONE || v2==V_Z))
		 us[0].table.ClearBit(iteration);
	  else 
		 us[0].table.SetBit(iteration);
      }

   for (oo=0; oo<outputs.size(); oo++)
      {
      us[oo].num_inputs = inputs.size();
      us[oo].sequential = false;
      if (inputs.size()==0)  // something has been tied off
         {
         fp.fprintf("buf \tui_%-4d(%s, ",inum++, nodes[outputs[oo]].VName(buffer));
         if (us[oo].xtable.GetBit(0))
            fp.fprintf( "1'bx);\n");
         else if (us[oo].table.GetBit(0))
            fp.fprintf( "1'b1);\n");
         else
            fp.fprintf( "1'b0);\n");
         }
      else if (inputs.size()==1 && !us[oo].xtable.GetBit(0) && !us[oo].xtable.GetBit(1) && us[oo].table.GetBit(0)!=us[oo].table.GetBit(1))  // either buf or inverter
         {
         if (!us[oo].table.GetBit(0))
            fp.fprintf("buf \tui_%-4d(%s",inum++, nodes[outputs[oo]].VName(buffer));
         else
            fp.fprintf("not \tui_%-4d(%s",inum++, nodes[outputs[oo]].VName(buffer));
         fp.fprintf(", %s);\n",nodes[inputs[0]].VName(buffer));
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
         fp.fprintf("mdp_%03d%s \tui_%-4d(%s", i, unique_string, inum++, nodes[outputs[oo]].VName(buffer));
         for (i=0; i<inputs.size(); i++)
			if (inputs[i]==VSS)
			   fp.fprintf(", %s", ccr.compliments[i] ? "1'b1":"1'b0");
			else if (inputs[i]==VDD || inputs[i]==VDDO)
			   fp.fprintf(", %s", ccr.compliments[i] ? "1'b0":"1'b1");
			else
			   fp.fprintf(", %s%s", ccr.compliments[i] ? "~":"", nodes[inputs[i]].VName(buffer));
         fp.fprintf(");\n");
         }
      }
   }


void ccr_circuittype::DumpUdps(filetype &fp, bool atpg, bool assertionCheck)
   {
   TAtype ta("ccr_circuittype::DumpUdps()");
   int uu;

   for (uu=0; uu<udps.size(); uu++)
      {
      const udptype &u = udps[uu];
      
      if (u.sequential)
         u.WriteSequentialPrimitive(fp, uu, atpg, assertionCheck);
      else
         u.WriteCombinationalPrimitive(fp, uu, atpg, assertionCheck);
      }
   fp.fprintf("\n");
   fp.fprintf("primitive sram_udp(q, wl, we, bl_h, bl_l, clear);\n");
   fp.fprintf("output q;\n");
   fp.fprintf("reg    q;\n");
   fp.fprintf("input  wl, we, bl_h, bl_l, clear;\n");
   fp.fprintf("table\n");
   fp.fprintf("\t0???? : 0 : 0 ;\n");
   fp.fprintf("\t0???0 : 1 : 1 ;\n");
   fp.fprintf("\t0???1 : ? : 0 ;\n");
   fp.fprintf("\t?0??? : 0 : 0 ;\n");
   fp.fprintf("\t?0??0 : 1 : 1 ;\n");
   fp.fprintf("\t?0??1 : ? : 0 ;\n");
   fp.fprintf("\t1101? : ? : 0 ;\n");
   fp.fprintf("\t11100 : ? : 1 ;\n");
   fp.fprintf("\t11101 : ? : x ;\n");
   fp.fprintf("\t1111? : ? : x ;\n");
   fp.fprintf("\t1100? : ? : x ;\n");
   fp.fprintf("endtable\n");
   fp.fprintf("endprimitive\n");
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

void ccrtype::CrossReference()
   {
   TAtype ta("ccrtype::CrossReference()");
   int i;

   array <nodetype> &nodes = parent.nodes;

   for (i=0; i<devices.size(); i++)
	  {
	  devicetype &d = devices[i];
	  nodes[d.drain].sds.clear();
	  nodes[d.source].sds.clear();
	  nodes[d.gate].gates.clear();
	  }
   for (i=0; i<states.size(); i++)
	  {
	  nodetype &n = nodes[states[i]];
	  n.sds.clear();
	  n.gates.clear();
	  }
   for (i=0; i<inputs.size(); i++)
	  {
	  nodetype &n = nodes[inputs[i]];
	  n.sds.clear();
	  n.gates.clear();
	  }
   for (i=0; i<outputs.size(); i++)
	  {
	  nodetype &n = nodes[outputs[i]];
	  n.sds.clear();
	  n.gates.clear();
	  }
   
   for (i=0; i<devices.size(); i++)
	  {
	  devicetype &d = devices[i];
	  if (d.source>VDDO)
		 nodes[d.source].sds.push(&d);
	  if (d.drain>VDDO)
		 nodes[d.drain].sds.push(&d);
  	  nodes[d.gate].gates.push(&d);
	  }
   }



// This function will look for nodes with many sd connections and try to simplify. This is useful for rf's, rom, zdets, etc
void ccrtype::Simplification(array <ccrtype> &romccrs)
   {
   TAtype ta("ccrtype::Simplification");
   // i want to search for 1 or more nodes with a lot of source drain connections
   int i, k, pass;
   int bignode = -1;
   array <romtype> roms;
   array <nodetype> &nodes = parent.nodes;	 // reference saves me a little typing

   if (romccrs.size()!=0) FATAL_ERROR;

   for (i = VDDO+1; i<nodes.size(); i++)
	  {
	  nodetype &n = nodes[i];
	  n.sds.clear();
	  n.gates.clear();
	  }
   CrossReference();

   for (bignode = VDDO+1; bignode < nodes.size(); bignode++)
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
            for (k=inputs.size()-1; k>=0; k--)
               if (d->gate == inputs[k])
                  break;
			if (k<0)
			   break;	 // gate wasn't an input so probably a internal keeper node we shouldn't touch
            if (d->source == VDD)
               {
               if (start->type == d->type)
                  pu.push(start);
               break;
               }
            if (d->source == VSS)
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
            for (k=inputs.size()-1; k>=0; k--)
               if (junk[0]->gate == inputs[k])
                  break;
			if (k<0)
			   notmux = true;	 // gate wasn't an input so probably a internal keeper node we shouldn't touch
               
            if (!notmux)
               {
               roms.back().muxd = junk[0];
               roms.back().muxout = junk[0]->source == bignode ? junk[0]->drain : junk[0]->source;
               }
            }
         }         
      }
   if (roms.size() ==0) return;

   texthashtype <int> hash;
   char buffer[512], buf2[512];
   const char *name;
   int rr, index;
   roms.Sort();

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
            if (d->source > VDDO && d->drain > VDDO)
               {
			   romccrs.push(ccrtype(parent));
			   ccrtype &ccr = romccrs.back();
			   ccr.type = type==D_NMOS ? CCR_AND : CCR_NOR;
			   name = stringPool.sprintf("%s$%s%d", nodes[r.bignode].VNameNoBracket(buffer), pass ? "low":"high", i);
			   nodes.push(nodetype(name));  // add this node to the parent
			   ccr.outputs.push(nodes.size()-1);
			   hash.Add(name, nodes.size()-1);

               int currentnode = r.bignode;
//               fp.fprintf("%s i%d(w_%s$%s%d", type==D_NMOS ? "and " : "nor ", inum++, nodes[r.bignode].VNameNoBracket(buffer), pass ? "low":"high", i);
               while (true)
                  {
                  ccr.inputs.push(d->gate);
				  ccr.compliments.push(false);
                  currentnode = (d->source == currentnode) ? d->drain : d->source;
                  if (currentnode <= VDD) break;
                  if (nodes[currentnode].sds.size()!=2) FATAL_ERROR;
                  d = (nodes[currentnode].sds[0] == d) ? nodes[currentnode].sds[1] :  nodes[currentnode].sds[0];
                  }
               }
            }
         if (pdu.size()!=0)
            {
			romccrs.push(ccrtype(parent));
			ccrtype &ccr = romccrs.back();
			ccr.type = CCR_OR;
			name = stringPool.sprintf("%s$%s", nodes[r.bignode].VNameNoBracket(buffer), pass ? "low":"high");
			nodes.push(nodetype(name));
			ccr.outputs.push(nodes.size()-1);
			hash.Add(name, nodes.size()-1);

//            fp.fprintf("or  i%d (w_%s$%s", inum++, nodes[r.bignode].VNameNoBracket(buffer), pass ? "low":"high");
            int priorgate = -1;
            for (i=0; i<pdu.size(); i++)
               {
               devicetype *d = pdu[i];
               
               if (d->source <= VDDO)
                  {
				  ccr.inputs.push(d->gate);
				  ccr.compliments.push(d->type!=D_NMOS);
                  priorgate = d->gate;
                  }
               else
                  {
				  sprintf(buf2,"%s$%s%d",nodes[r.bignode].VNameNoBracket(buffer), pass ? "low":"high",i);
				  int index;
				  if (!hash.Check(buf2, index))
					 FATAL_ERROR;
				  ccr.inputs.push(index);
				  ccr.compliments.push(false);
				  }
               }
            }
         if (pdu.size()==0) // no pu/pd so tie off this signal
            {
			romccrs.push(ccrtype(parent));
			ccrtype &ccr = romccrs.back();
			ccr.type = CCR_BUF;
			name = stringPool.sprintf("%s$%s", nodes[r.bignode].VNameNoBracket(buffer), pass ? "low":"high");
			nodes.push(nodetype(name));  // add this node to the parent
			ccr.outputs.push(nodes.size()-1);
			hash.Add(name, nodes.size()-1);
			}
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
				  int index;

				  romccrs.push(ccrtype(parent));
				  ccrtype &ccr = romccrs.back();
				  ccr.type = CCR_AND;
				  name = stringPool.sprintf("%s$%s%d", nodes[muxout].VNameNoBracket(buffer), pass ? "low":"high", i);
				  nodes.push(nodetype(name));  // add this node to the parent
				  ccr.outputs.push(nodes.size()-1);
			      hash.Add(name, nodes.size()-1);
				  ccr.inputs.push(r.muxd->gate);
				  ccr.compliments.push(false);
				  sprintf(buf2,"%s$%s", nodes[r.bignode].VNameNoBracket(buffer), pass ? "low":"high");
				  if (!hash.Check(buf2, index)) FATAL_ERROR;
				  ccr.inputs.push(index);
				  ccr.compliments.push(false);
                  }
			   romccrs.push(ccrtype(parent));
			   ccrtype &ccr = romccrs.back();
			   ccr.type = CCR_OR;
			   name = stringPool.sprintf("%s$%s", nodes[muxout].VNameNoBracket(buffer), pass ? "low":"high");
               nodes.push(nodetype(name));  // add this node to the parent
			   ccr.outputs.push(nodes.size()-1);
			   hash.Add(name, nodes.size()-1);

               for (i=start; i<end; i++)
                  {
				  int index;
				  sprintf(buf2,"%s$%s%d", nodes[muxout].VNameNoBracket(buffer), pass ? "low":"high", i);
				  if (!hash.Check(buf2, index)) FATAL_ERROR;
				  ccr.inputs.push(index);
				  ccr.compliments.push(false);
				  }
               }
            else
               roms[start].muxd = NULL;

            int finalnode = roms[start].muxd != NULL ? roms[start].muxout : roms[start].bignode;
            
            newdevices.push(devicetype());
            newdevices.back().type   = D_NMOS;
            newdevices.back().name   = "MUX Simplification device";
            newdevices.back().source = pass ? VSS : VDD;
            newdevices.back().drain  = finalnode;
            newdevices.back().r      = (float)0.001;  
            newdevices.back().length = 1.0;
            newdevices.back().width  = 100.0;

            sprintf(buf2,"%s$%s", nodes[finalnode].VNameNoBracket(buffer), pass ? "low":"high");
            if (!hash.Check(buf2, index)) FATAL_ERROR;
            newdevices.back().gate   = index;  // foo$high
            inputs.push(index);            
			compliments.push(false);

            start = end;
            }
         }
      }

   // I've probably now created 2 or more ccr's which have all the pulldown logic
   // What's left in this ccr is just the keeper, inverter, glat, etc.

   // I want to propogate any compliments from the original ccr to the pulldown ccrs
   for (i=0; i<romccrs.size(); i++)
	  {
	  ccrtype &ccr = romccrs[i];

	  for (k=0; k<ccr.inputs.size(); k++)
		 {
		 int j;
		 for (j=0; j<inputs.size(); j++)
			{
			if (ccr.inputs[k] == inputs[j] && compliments[j])
			   ccr.compliments[k] = !ccr.compliments[k];
			}
		 }
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
               if (d->source <= VDD) break;
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
         {
		 inputs.RemoveFast(i);
		 compliments.RemoveFast(i);
		 }
      }
   for (i=0; i<states.size(); i++)
      {
      if (nodes[states[i]].gates.size()==0 || nodes[states[i]].sds.size()==0)
         {
         FATAL_ERROR;  // I shouldn't have touched any state nodes
         }
      }
   return;
   }

struct ramgrouptype{
   bool valid;
   int output;
   array <int> bitlines;
   array <devicetype *> devices;
   };


// first I want to identify all the 6t cells and then see how many bitlines(colmux) I have
void ccrtype::SramTransform(array <ccrtype> &romccrs)
   {
   TAtype ta("ccrtype::SramTransform");
   int q, i, k;
   array <int> bitlines;
   array <nodetype> &nodes = parent.nodes;	 // reference saves me a little typing
   if (romccrs.size()!=0) FATAL_ERROR;

   for (i=0; i<inputs.size(); i++)
	  if (compliments[i]) 
		 FATAL_ERROR;
   for (i=0; i<nodes.size(); i++)
	  {
	  nodetype &n = nodes[i];
	  n.sds.clear();
	  n.gates.clear();
	  n.inflight = false;
	  }
   CrossReference();
   for (i=0; i<devices.size(); i++)
	  {
	  devicetype &d = devices[i];
	  d.on = false;   // I'll use on to mark this device for deletion
	  }

   for (q = VDDO+1; q < nodes.size(); q++)
      {
	  const nodetype &node_q = nodes[q];
      int qn = -1;
	  bool found = true, invn=false, invqn=false, invqp=false;
	  const devicetype *passq=NULL, *passqn=NULL, *set=NULL, *clear=NULL;

	  for (i=0; i<node_q.sds.size(); i++)
		 {
		 const devicetype &d = *node_q.sds[i];

		 if (d.type==D_PMOS && !d.on)
			{
			if (d.source!=VDD || qn>=0)
			   found = false;
			else
			   qn = d.gate;
			}
		 }
	  if (qn<0) found = false;
	  const nodetype &node_qn = nodes[!found ? 0:qn];

	  for (i=0; found && i<node_q.sds.size(); i++)
		 {
		 const devicetype &d = *node_q.sds[i];

		 if (d.type==D_NMOS && d.source==VSS && d.gate==qn)
			invn = true;
		 else if (d.type==D_NMOS && d.source==VSS)
			{
			if (clear!=NULL) found = false;
			clear = &d;
			}
		 else if (d.type==D_NMOS)
			{
			if (passq!=NULL) found = false;
			passq = &d;
			}
		 }
	  for (i=0; found && i<node_qn.sds.size(); i++)
		 {
		 const devicetype &d = *node_qn.sds[i];

		 if (d.type==D_NMOS && d.source==VSS && d.gate==q)
			invqn = true;
		 else if (d.type==D_NMOS && d.source==VDD)
			{
			if (set!=NULL) found = false;
			set = &d;
			}
		 else if (d.type==D_NMOS)
			{
			if (passqn!=NULL) found = false;
			passqn = &d;
			}
		 if (d.type==D_PMOS)
			{
			if (d.source==VDD && d.gate==q && !invqp)
			   invqp = true;
			else
			   found = false;
			}
		 }
	  found = found && invn && invqn && invqp && passq!=NULL && passqn!=NULL;
	  found = found && passq->gate==passqn->gate;
	  if (set!=NULL && clear==NULL) found = false;
	  if (set==NULL && clear!=NULL) found = false;
	  if (set!=NULL && set->gate!=clear->gate) found = false;

	  if (found)
		 { // I've found a 6T device
		 int bl_h =  passq->source == q  ?  passq->drain :  passq->source;
		 int bl_l = passqn->source == qn ? passqn->drain : passqn->source;
		 if (bl_h<=VDDO || bl_l<=VDDO)
			return;  // probably a casc head don't transform into sram
		 romccrs.push(ccrtype(parent));
		 ccrtype &ccr = romccrs.back();

		 for (k=bitlines.size()-1; k>=0; k--)
			if (bl_h==bitlines[k] || bl_l==bitlines[k])
			   break;
		 if (k<0){
		    bitlines.push(bl_h);
		    bitlines.push(bl_l);
			}		 
		 ccr.type = CCR_SRAM;
		 ccr.secondOutputIsComplimentOfFirst = true;
		 ccr.inputs.push(passq->gate);
		 ccr.inputs.push(-1);			// write enable, will be filled in later
		 ccr.inputs.push(bl_h);
		 ccr.inputs.push(bl_l);
		 if (clear!=NULL)
			ccr.inputs.push(clear->gate);
		 else
			ccr.inputs.push(VSS);
		 ccr.outputs.push(q);
		 ccr.outputs.push(qn);
		 ccr.compliments.resize(5, false);

		 for (i=0; i<node_q.sds.size(); i++)
			{
			devicetype &d = *node_q.sds[i];
			d.on = true;   // mark these devices for deletion
			}
		 for (i=0; i<node_qn.sds.size(); i++)
			{
			devicetype &d = *node_qn.sds[i];
			d.on = true;   // mark these devices for deletion
			}
		 }
      }
   if (romccrs.size()==0) return;
   
   for (i=devices.size()-1; i>=0; i--)
      {
	  if (devices[i].on)
         devices.RemoveFast(i);
	  }
  

   if (bitlines.size()==2)
	  {
	  SramTransform2(romccrs, bitlines);
	  return;
	  }
   // now solve the column mux case
   if (bitlines.size()%2) FATAL_ERROR;  // an odd number of bitlines is unexpected
   array <ccrtype> accumulatedccrs;
   int b;
   ramgrouptype group0, group1;
   group0.valid = group1.valid = false;

   // now I need to re crossreference
   CrossReference();

   // first identify the colmux devices if any
   for (i=0; i<nodes.size(); i++)
	  {
	  nodetype &n = nodes[i];
	  ramgrouptype g;
	  unsigned count=0;
	  int j;

	  n.touched = -1;
	  g.valid = true;
	  g.output = i;
	  for (k=0; k<n.sds.size(); k++)
		 {
		 devicetype &d = *n.sds[k];
		 for (j=0; j<bitlines.size(); j++)
			{
			if ((bitlines[j]==d.source || bitlines[j]==d.drain) && d.type==D_PMOS)
			   {
			   count |= 1<<j;
			   g.bitlines.push(bitlines[j]);
			   g.devices.push(&d);
			   }
			}
		 }
	  if (n.sds.size()==bitlines.size()/2 && PopCount(count)==bitlines.size()/2 && g.bitlines.size()==bitlines.size()/2)
		 {
		 if (!group0.valid)
			group0 = g;
		 else if (!group1.valid)
			group1 = g;
		 else
			FATAL_ERROR;
		 }
	  }
   if (!group0.valid || !group1.valid)
	  FATAL_ERROR;
   
   for (b=0; b<bitlines.size(); b+=2)
	  {
	  array <ccrtype> sramccrs;
	  array <int> thisbitlines;
	  ccrtype base = *this;
	  int cn;

	  for (i=0; i<devices.size(); i++)
		 {
		 devicetype &d = devices[i];
		 d.on = false;
		 }
	  for (i=0; i<group0.devices.size(); i++)
		 {
		 devicetype &d = *group0.devices[i];
		 d.on = true;
		 }
	  for (i=0; i<group1.devices.size(); i++)
		 {
		 devicetype &d = *group1.devices[i];
		 d.on = true;
		 }

	  thisbitlines.push(bitlines[b+0]);
	  thisbitlines.push(bitlines[b+1]);

	  for (i=0; i<romccrs.size(); i++)
		 {
		 ccrtype &ccr = romccrs[i];

		 if (ccr.inputs[2]==bitlines[b+0] || ccr.inputs[2]==bitlines[b+1])
			sramccrs.push(ccr);
		 }
	  base.devices.clear();
	  // now i want to flood fill through source/drain regions catching devices for these bitlines
	  array <int> stack;
	  stack = thisbitlines;
	  while (stack.pop(cn))
		 {
		 nodetype &n = nodes[cn];
		 n.touched = b;
		 for (i=0; i<n.gates.size(); i++)
			{
			devicetype &d = *n.gates[i];
			if (!d.on)
			   base.devices.push(d);
			nodes[d.gate].touched = b;
			}
		 for (i=0; i<n.sds.size(); i++)
			{
			devicetype &d = *n.sds[i];
			if (!d.on)
			   {
			   nodes[d.gate].touched = b;
			   base.devices.push(d);
			   d.on = true;
			   if (d.source>VDDO && nodes[d.source].touched!=b)
				  stack.push(d.source);
			   if (d.drain>VDDO && nodes[d.drain].touched!=b)
				  stack.push(d.drain);
			   }
			}
		 }
	  for (i=base.inputs.size()-1; i>=0; i--)
		 {
		 const nodetype &n = nodes[base.inputs[i]];
		 if (n.touched !=b)
			{
			base.inputs.RemoveFast(i);
			base.compliments.RemoveFast(i);
			}
		 }
	  for (i=base.outputs.size()-1; i>=0; i--)
		 {
		 const nodetype &n = nodes[base.outputs[i]];
		 if (n.touched !=b)
			{
			base.outputs.RemoveFast(i);
			}
		 }
	  for (i=base.states.size()-1; i>=0; i--)
		 {
		 const nodetype &n = nodes[base.states[i]];
		 if (n.touched !=b)
			{
			base.states.RemoveFast(i);
			}
		 }
	  base.SramTransform2(sramccrs, thisbitlines);
	  accumulatedccrs += sramccrs;
	  accumulatedccrs.push(base);
	  }
   devices.clear();
   romccrs.clear();
   romccrs += accumulatedccrs;

   // now I need to build the column mux
   // Note that this isn't conservative. I'm not modelling the case where 0,2-N colselects
   // assert simulataneously
   type = CCR_NULL;

   romccrs.push(ccrtype(parent));
   ccrtype &or0 = romccrs.back();
   for (i=0; i<group0.bitlines.size(); i++)
	  {
	  char buffer[256];
      romccrs.push(ccrtype(parent));
	  ccrtype &ccr = romccrs.back();

	  ccr.type = CCR_AND;
	  ccr.inputs.push(group0.bitlines[i]);
	  ccr.compliments.push(false);
	  ccr.inputs.push(group0.devices[i]->gate);
	  ccr.compliments.push(true);	// invert because colmux is pmos
	  const char *mux0_name = stringPool.sprintf("%s$mux%d",nodes[group0.bitlines[i]].VNameNoBracket(buffer), i);
	  const int mux0_net = parent.AddNet(mux0_name);
	  ccr.outputs.push(mux0_net);

	  or0.inputs.push(mux0_net);
	  or0.compliments.push(false);
	  }
   or0.outputs.push(group0.output);
   or0.type = CCR_OR;

   romccrs.push(ccrtype(parent));
   ccrtype &or1 = romccrs.back();
   for (i=0; i<group1.bitlines.size(); i++)
	  {
	  char buffer[256];
      romccrs.push(ccrtype(parent));
	  ccrtype &ccr = romccrs.back();

	  ccr.type = CCR_AND;
	  ccr.inputs.push(group1.bitlines[i]);
	  ccr.compliments.push(false);
	  ccr.inputs.push(group1.devices[i]->gate);
	  ccr.compliments.push(true);
	  const char *mux1_name = stringPool.sprintf("%s$mux%d",nodes[group1.bitlines[i]].VNameNoBracket(buffer), i);
	  const int mux1_net = parent.AddNet(mux1_name);
	  ccr.outputs.push(mux1_net);

	  or1.inputs.push(mux1_net);
	  or1.compliments.push(false);
	  }
   or1.outputs.push(group1.output);
   or1.type = CCR_OR;
   
   return;
   }

void ccrtype::SramTransform2(array <ccrtype> &romccrs, const array <int> &bitlines)
   {
   TAtype ta("ccrtype::SramTransform2");
   int i;
   char buffer[512];
   array <nodetype> &nodes = parent.nodes;	 // reference saves me a little typing

   if (romccrs.size()==0) return;
   if (bitlines.size()!=2)  
	  FATAL_ERROR;

   // Add the write enable net
   const char *wename = stringPool.sprintf("%s$fakewriteenable",nodes[bitlines[0]].VNameNoBracket(buffer));
   const int wenet = parent.AddNet(wename);
   const char *wbl0name = stringPool.sprintf("%s$fakewrite_bl",nodes[bitlines[0]].VNameNoBracket(buffer));
   const char *wbl1name = stringPool.sprintf("%s$fakewrite_bl",nodes[bitlines[1]].VNameNoBracket(buffer));
   const int wbl0net = parent.AddNet(wbl0name);
   const int wbl1net = parent.AddNet(wbl1name);

   for (i=0; i<romccrs.size(); i++)
	  {
	  ccrtype &ccr = romccrs[i];
	  if (ccr.type == CCR_SRAM)
		 {
		 ccr.inputs[1] = wenet;  // backfill the write enable net
		 // backfill the bitlines to be the write bitlines
		 if (ccr.inputs[2]==bitlines[0])
			ccr.inputs[2] = wbl0net;
		 else if (ccr.inputs[2]==bitlines[1])
			ccr.inputs[2] = wbl1net;
		 else FATAL_ERROR;
		 if (ccr.inputs[3]==bitlines[0])
			ccr.inputs[3] = wbl0net;
		 else if (ccr.inputs[3]==bitlines[1])
			ccr.inputs[3] = wbl1net;
		 else FATAL_ERROR;
		 }
	  }
  
   // now I need to re crossreference
   CrossReference();

   for (i=inputs.size()-1; i>=0; i--)
	  {
	  const nodetype &n = nodes[inputs[i]];
	  if (n.sds.size()!=0) FATAL_ERROR;
	  if (n.gates.size()==0)
		 {
		 inputs.RemoveFast(i);
		 compliments.RemoveFast(i);
		 }
	  }
   for (i=states.size()-1; i>=0; i--)
	  {
	  const nodetype &n = nodes[states[i]];
	  if (n.gates.size()==0)
		 {
		 states.Remove(i);
		 }
	  }
   for (i=outputs.size()-1; i>=0; i--)
	  {
	  const nodetype &n = nodes[outputs[i]];
	  if (n.sds.size()==0)
		 {
		 outputs.RemoveFast(i);
		 }
	  }


   // push ccr which will be the write enable  
   bool found0=false, found1=false;
	  {
	  romccrs.push(*this);	 // inherit the base ccr devices and inputs
      ccrtype &nccr = romccrs.back();

	  nccr.type = CCR_SRAM_WRITENABLE;	  
	  nccr.states.clear();
	  nccr.states = bitlines;
	  nccr.outputs.clear();
	  nccr.outputs.push(wenet);

	  for (i=0; i<devices.size(); i++)
		 {
		 devicetype &d = devices[i];
		 if (d.source==bitlines[0]) found0=true;
		 if (d.drain==bitlines[0])  found0=true;
		 if (d.source==bitlines[1]) found1=true;
		 if (d.drain==bitlines[1])  found1=true;
		 }
	  if (!found0 && !found1)  // no drivers attached to sram, probably dummy cells
		 romccrs.pop();
	  }
   // push ccr which will be the write bitline driver
   if (found0 || found1)  // no drivers attached to sram, probably dummy cells
	  {
	  romccrs.push(*this);
      ccrtype &nccr = romccrs.back();

	  nccr.type = CCR_COMBINATIONAL;
	  nccr.addDelay = true;  // this prevents a race against write enable
	  nccr.states.clear();
	  nccr.outputs.clear();
	  nccr.outputs.push(wbl0net);
	  nccr.outputs.push(wbl1net);
	  nccr.secondOutputIsComplimentOfFirst = false;
	  nccr.treatZasOne = true;

	  for (i=0; i<nccr.devices.size(); i++)
		 {
		 devicetype &d = nccr.devices[i];
		 if (d.source == bitlines[0])
			d.source = wbl0net;
		 if (d.drain == bitlines[0])
			d.drain = wbl0net;
		 if (d.gate == bitlines[0])
			d.gate = wbl0net;
		 if (d.source == bitlines[1])
			d.source = wbl1net;
		 if (d.drain == bitlines[1])
			d.drain = wbl1net;
		 if (d.gate == bitlines[1])
			d.gate = wbl1net;
		 }
	  }
// now I will build the read bitline logic separate similarly to rf style
   const char *bitline0_name = stringPool.sprintf("%s$low",nodes[bitlines[0]].VNameNoBracket(buffer));
   const char *bitline1_name = stringPool.sprintf("%s$low",nodes[bitlines[1]].VNameNoBracket(buffer));
   const int bitline0_net = parent.AddNet(bitline0_name);
   const int bitline1_net = parent.AddNet(bitline1_name);
   ccrtype or0(parent), or1(parent);
   or0.type = CCR_OR;
   or0.outputs.push(bitline0_net);
   or1.type = CCR_OR;
   or1.outputs.push(bitline1_net);

   array <ccrtype> pulldowns;
   for (i=romccrs.size()-1; i>=0; i--)	  
	  {
	  ccrtype &ccr = romccrs[i];
	  if (ccr.type == CCR_SRAM)
		 {							  
		 const char *bl_h_name = stringPool.sprintf("%s$low%d",nodes[bitlines[0]].VNameNoBracket(buffer),i);
		 const char *bl_l_name = stringPool.sprintf("%s$low%d",nodes[bitlines[1]].VNameNoBracket(buffer),i);
		 const int bl_h_net = parent.AddNet(bl_h_name);
		 const int bl_l_net = parent.AddNet(bl_l_name);

		 ccrtype nccr(parent);
		 nccr.type = CCR_AND;
		 nccr.inputs.push(ccr.outputs[0]);	// q
		 nccr.inputs.push(ccr.inputs[0]);	// wl
		 nccr.outputs.push(bl_h_net);
		 nccr.compliments.resize(2, false);
		 nccr.compliments[0] = true;
		 pulldowns.push(nccr);

		 nccr.inputs.clear(); nccr.outputs.clear();
		 nccr.inputs.push(ccr.outputs[0]);	// q
		 nccr.compliments[0] = false;
		 nccr.inputs.push(ccr.inputs[0]);	// wl
		 nccr.outputs.push(bl_l_net);
		 pulldowns.push(nccr);

		 if (ccr.inputs[2]==wbl0net && ccr.inputs[3]==wbl1net)
			{
		    or0.inputs.push(bl_h_net);
		    or1.inputs.push(bl_l_net);
			}
		 else if (ccr.inputs[2]==wbl1net && ccr.inputs[3]==wbl0net)
			{
		    or1.inputs.push(bl_h_net);
		    or0.inputs.push(bl_l_net);
			}
		 else 
			FATAL_ERROR;
		 }
	  }
   romccrs+=pulldowns;
   pulldowns.clear();
   or0.compliments.resize(or0.inputs.size(), false);
   or1.compliments.resize(or1.inputs.size(), false);
   romccrs.push(or0);
   romccrs.push(or1);
   devicetype d;

   d.name   = stringPool.NewString("bitline 0 pulldown");
   d.source = VSS;
   d.drain  = bitlines[0];
   d.gate   = bitline0_net;
   d.width  = 100;
   d.length = 1;
   d.r      = d.length / d.width;
   d.type   = D_NMOS;
   devices.push(d);

   d.drain  = bitlines[1];
   d.gate   = bitline1_net;
   devices.push(d);
   inputs.push(bitline0_net);
   compliments.push(false);
   inputs.push(bitline1_net);
   compliments.push(false);
   addDelay = true;    // put some delay on the precharges as they are likely to race
   lastTwoInputsAreWeakPulldowns = true; // allow the write logic to win any fights with the rams

   return;
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



// this converts from Raw devices to channel connected regions
void wirelisttype::ConvertToCCR(filetype &fp)
   {
   TAtype ta("wirelisttype::ConvertToCCR");   
   connectiontype connect(devices.size());
   array <int> xref(devices.size(), -1);
   int top, ccindex;
   int i, k;

   if (ccrs.size()) FATAL_ERROR;

   for (k=0; k<nodes.size(); k++)
	  {
	  nodes[k].sds.clear();
	  nodes[k].gates.clear();
	  nodes[k].bristle = false;
	  nodes[k].drivesInstance = false;
	  nodes[k].drivesCCR = false;
	  }
   for (k=0; k<devices.size(); k++)
	  {
	  devicetype &d = devices[k];
	  if (d.gate == VSS && d.type == D_NMOS)	// eliminate tied off devices(this helps isolate dummy mem rows)
		 {
		 d.width = 0.0;
		 d.source = d.gate = d.drain = VSS;
		 }
	  if (d.gate == VDD && d.type == D_PMOS)
		 {
		 d.width = 0.0;
		 d.source = d.gate = d.drain = VSS;
		 }
	  if (d.width>0.0)
		 {
		 if (d.source > VDDO)
		    nodes[devices[k].source].sds.push(&d);
		 if (d.drain > VDDO)
		    nodes[devices[k].drain].sds.push(&d);
		 if (d.gate > VDDO)
		    nodes[d.gate].gates.push(&d);
		 }
	  }
   nodes[VSS].sds.clear();
   nodes[VDD].sds.clear();
   for (i=0; i<instances.size(); i++)
	  {
	  const instancetype &inst = instances[i];
	  for (k=0; k<inst.outsideNodes.size(); k++)
		 {
		 nodes[inst.outsideNodes[k]].drivesInstance = true;
		 }
	  }
   for (i=0; i<bristles.size(); i++)
	  {
	  const bristletype &brist = bristles[i];
	  nodes[brist.nodeindex].bristle = true;
	  }
   for (k=0; k<nodes.size(); k++)
	  {
	  nodetype &n = nodes[k];

	  // look for dangling devices(this happens after the vss tie off optimization)
	  if (n.sds.size()==1 && n.gates.size()==0 && !n.bristle && !n.drivesInstance)
		 {
		 devicetype &d = *n.sds[0];
		 d.source = d.drain = d.gate = VSS;
		 d.width = 0.0;
		 }
	  }

   for (i=VDDO+1; i<nodes.size(); i++)
      {
	  const nodetype &n = nodes[i];
      
	  for (k=1; k<n.sds.size(); k++)
		 {
		 int index1 = n.sds[k-1] - &devices[0];
		 int index2 = n.sds[k]   - &devices[0];
		 if (index1 <0 || index2<0) FATAL_ERROR;
		 if (n.sds[k-1] != &devices[index1]) FATAL_ERROR;
		 if (n.sds[k]   != &devices[index2]) FATAL_ERROR;
		 connect.Merge(index1, index2);
		 }
      }
   // I now know which devices are connected together

   for (i=0, top=0; i<devices.size(); i++)
      {
      int h = connect.Handle(i);
      if (h==i)
         {
         xref[h] = top;
         top++;
         }
      }
   for (i=0; i<top; i++)
      ccrs.push(ccrtype(*this));
   for (i=0; i<devices.size(); i++)
      {
      int h = connect.Handle(i);
      if (xref[h]<0) FATAL_ERROR;
      ccrtype &ccr=ccrs[xref[h]];
	  const devicetype &d = devices[i];

      ccr.devices.push(d);
      }

   for (ccindex=0; ccindex<ccrs.size(); ccindex++)
      {
      ccrtype &ccr=ccrs[ccindex];
	  
	  ccr.Setup();
      }

   if (DEBUG){
	  fp.fprintf("//Initial ccr creation.\n");
	  for (i=0; i<ccrs.size(); i++)
		 {
		 ccrtype &ccr = ccrs[i];
         ccr.Print(fp, i);
		 }
	  }
   }

// sort devices by gate node

struct deviceSorttype { // function object (predicate for STL)
   const int personality;
   deviceSorttype (int _personality) : personality(_personality)
      {}
   bool operator()(const devicetype &x, const devicetype &y) const
      {
      // put 0 length devices at the end of the list
      if (x.length<=0.0 && y.length>0.0)
         return false;
      if (y.length<=0.0 && x.length>0.0)
         return true;
      
      // put nmos first then pmos
      if (x.type < y.type)
         return true;
      else if (x.type > y.type)
         return false;
      switch (personality)
         {
         default:
         case 0:
            return x.gate < y.gate;
         }      
      }
   };



void ccrtype::Setup()
   {
   TAtype ta("ccrtype::Setup");   
   int i, k;
   array <nodetype> &nodes = parent.nodes;   // save me some typing
   
   type = CCR_COMBINATIONAL;

// add input/output to lists
   for (i=0; i<devices.size(); i++)
      {
      devicetype &d=devices[i];

      // make source<=drain
      if (d.source > d.drain)
         {
         int temp = d.source;
         d.source = d.drain;
         d.drain  = temp;
         }

      if (true)
         {
         for (k=inputs.size()-1; k>=0; k--)
            {
            if (inputs[k]==d.gate)
               break;
            }
         if (k<0)
            inputs.push(d.gate);
         }
      
      if (d.source>VDDO && (nodes[d.source].gates.size()!=0 || nodes[d.source].drivesInstance || nodes[d.source].bristle))
         {
         for (k=outputs.size()-1; k>=0; k--)
            {
            if (outputs[k]==d.source)
               break;
            }
         if (k<0)
            outputs.push(d.source);
         }
      
      if (d.drain>VDDO && (nodes[d.drain].gates.size()!=0 || nodes[d.drain].drivesInstance || nodes[d.drain].bristle))
         {
         for (k=outputs.size()-1; k>=0; k--)
            {
            if (outputs[k]==d.drain)
               break;
            }
         if (k<0)
            outputs.push(d.drain);
         }
      }

   if (outputs.size()==0)
	  {
	  Clear(); // this ccr doesn't drive anything so effectively remove it
	  return;
	  }

   inputs.Sort();
   outputs.Sort();
   inputs.RemoveCommonElements(outputs);
   inputs.Sort();
   compliments.clear();
   compliments.resize(inputs.size(), false);
   std::sort(devices.begin(), devices.end(), deviceSorttype(0)); // sort by gate
// now merge parallel devices
   CrossReference();

// first I want to look for and delete balancing devices.
// the pattern is 3 pmos with the same gate, with 2 of them tied to vdd
// the case that causes this is a precharge race. It's ok for the node being
// written low to glitch to X when the precharge happens. The otherside should
// remain at a constant 1 regardless(so I need to delete the X path through the balancer)
   for (i=0; i<devices.size(); i++)
      {
      devicetype &d = devices[i];

	  if (d.type==D_PMOS && d.source>VDDO && d.drain>VDDO)
		 {
		 nodetype &n = nodes[d.gate];
		 bool foundsource=false, founddrain=false;
		 for (k=0; k<n.gates.size(); k++)
			{
			const devicetype &dp = *n.gates[k];

			if (dp.type==D_PMOS && dp.source==VDD && dp.drain == d.source)
			   foundsource = true;
			if (dp.type==D_PMOS && dp.source==VDD && dp.drain == d.drain)
			   founddrain = true;
			}
		 if (foundsource && founddrain)
			{
			d.width = 0.0;	// delete this device
			d.source = d.drain = VSS;
			}
		 }
	  }


   for (i=0; i<devices.size(); i++)
      {
      devicetype &d1 = devices[i];
      for (k=i+1; k<devices.size() && devices[k].gate==d1.gate; k++)
         {
         devicetype &d2 = devices[k];

         if (d1.source == d2.source && d1.drain == d2.drain)
            {
            d1.width += d2.width;
            d2.width  = 0;    // will be indicator to delete this device later
            d2.length = 0;
			d2.source = d2.drain = d2.gate = VSS;
            }
		 // this will merge series devices like we use in our crippled inverters
		 if (d1.drain == d2.source && d1.drain >VDDO)
			{
			if (nodes[d1.drain].sds.size()==2 && nodes[d1.drain].gates.size()==0 && d1.type==d2.type)
			   {
			   d1.drain = d2.drain;
			   if (d1.drain < d1.source)
				  SWAP(d1.drain, d1.source);				  
			   d1.width = 1.0/(1.0/d1.width + 1.0/d2.width);
			   d2.width = d2.length = 0;
			   d2.source = d2.drain = d2.gate = VSS;
			   }
			}
		 if (d1.drain == d2.drain && d1.drain >VDDO)
			{
			if (nodes[d1.drain].sds.size()==2 && nodes[d1.drain].gates.size()==0 && d1.type==d2.type)
			   {
			   d1.drain = d2.source;
			   if (d1.drain < d1.source)
				  SWAP(d1.drain, d1.source);
			   d1.width = 1.0/(1.0/d1.width + 1.0/d2.width);
			   d2.width = d2.length = 0;
			   d2.source = d2.drain = d2.gate = VSS;
			   }
			}
		 if (d1.source == d2.source && d1.source >VDDO)
			{
			if (nodes[d1.source].sds.size()==2 && nodes[d1.source].gates.size()==0 && d1.type==d2.type)
			   {
			   d1.source = d2.drain;
			   if (d1.drain < d1.source)
				  SWAP(d1.drain, d1.source);
			   d1.width = 1.0/(1.0/d1.width + 1.0/d2.width);
			   d2.width = d2.length = 0;
			   d2.source = d2.drain = d2.gate = VSS;
			   }
			}
		 if (d1.source == d2.drain && d1.source >VDDO)
			{
			if (nodes[d1.source].sds.size()==2 && nodes[d1.source].gates.size()==0 && d1.type==d2.type)
			   {
			   d1.source = d2.source;
			   if (d1.drain < d1.source)
				  SWAP(d1.drain, d1.source);
			   d1.width = 1.0/(1.0/d1.width + 1.0/d2.width);
			   d2.width = d2.length = 0;
			   d2.source = d2.drain = d2.gate = VSS;
			   }
			}
         }
      }
   std::sort(devices.begin(), devices.end(), deviceSorttype(0)); // sort by gate
   for (i=devices.size()-1; i>=0; i--)
      {
      if (devices[i].length==0.0)
         devices.pop();   // remove deleted devices
      }
   
   CrossReference();

// now look to see if this is a inverter
   if (devices.size()<2) 
	  {			  // this can happen because of a dummy cell sometimes
	  return;
//	  FATAL_ERROR;
	  }
   if (outputs.size()==1 && inputs.size()<=5)
	  {
	  int out = outputs[0];
	  if (devices.size()==2)
		 {
		 const devicetype &dn = devices[0];
		 const devicetype &dp = devices[1];
		 if (dn.type==D_NMOS && dp.type==D_PMOS)
			{
			if (dn.source == VSS && dn.drain==outputs[0] && dp.source==VDD && dp.drain==outputs[0] && dn.gate==inputs[0] && dp.gate==inputs[0]) 
			   {
			   type = CCR_INV;
			   if (inputs.size()!=1) 
				  FATAL_ERROR;
			   return;
			   }
			}
		 }

	  // look for nand gates. This should work even if multiple nands are ganged together
	  bool found=true;
	  for (i=0; i<inputs.size(); i++)
		 {
		 const nodetype &n = nodes[inputs[i]];
		 bool vacuous=true;
		 for (k=0; k<n.gates.size(); k++)
			{
			const devicetype &d = *n.gates[k];
			if (d.type==D_PMOS && (d.source!=VDD || d.drain!=out))
			   found = false;
			vacuous = vacuous && d.type!=D_PMOS;
			}
		 found = found && !vacuous;
		 }
	  for (i=0; i<nodes[out].sds.size() && found; i++)
		 {
	     int last = out;
		 do{
			const devicetype &d = *nodes[last].sds[i];
			if (d.type==D_NMOS)
			   {
			   if (d.drain!=last && d.source!=last)
				  found = false;
			   last = d.drain==last ? d.source : d.drain;
			   if (last==VDD || last==VDDO) found = false;
			   if (last==VSS) break;
			   if (nodes[last].sds.size()!=2) found = false;
			   }
			else if (last!=out)
			   found = false;
			}while (last<=VDDO && found);
		 }
	  if (found)
		 {
		 type = CCR_NAND;
		 return;
		 }

	  // look for nor gates. This should work even if multiple nands are ganged together
	  found=true;
	  for (i=0; i<inputs.size(); i++)
		 {
		 const nodetype &n = nodes[inputs[i]];
		 bool vacuous=true;
		 for (k=0; k<n.gates.size(); k++)
			{
			const devicetype &d = *n.gates[k];
			if (d.type==D_NMOS && (d.source!=VSS || d.drain!=out))
			   found = false;
			vacuous = vacuous && d.type!=D_NMOS;
			}
		 found = found && !vacuous;
		 }
	  for (i=0; i<nodes[out].sds.size() && found; i++)
		 {
	     int last = out;
		 do{
			const devicetype &d = *nodes[last].sds[i];
			if (d.type==D_PMOS)
			   {
			   if (d.drain!=last && d.source!=last)
				  found = false;
			   last = d.drain==last ? d.source : d.drain;
			   if (last==VSS) found = false;
			   if (last==VDD || last==VDDO) break;
			   if (nodes[last].sds.size()!=2) found = false;
			   }
			else if (last!=out)
			   found = false;
			}while (last<=VDDO && found);
		 }
	  if (found)
		 {
		 type = CCR_NOR;
		 return;
		 }
	  return;
	  }

   CrossReference();

   if (outputs.size()==2)
      {
// now see if the two outputs are complementary, probably cascode structure
      const nodetype &n = nodes[outputs[1]];
      bool nfound=false, pfound=false;
      for (i=0; i<n.sds.size(); i++)
         {
         const devicetype &d = *n.sds[i];
         if (d.type==D_NMOS && d.source==VSS && d.gate==outputs[0])
            nfound = true;
         if (d.type==D_PMOS && d.source==VDD && d.gate==outputs[0])
            pfound = true;
         }
      if (nfound && pfound)
         {
		 secondOutputIsComplimentOfFirst = true;
         type = CCR_SEQUENTIAL;
         states = outputs;
		 return;
		 }
      }


   // now see if what ever this is has cross coupled gates
   for (i=0; i<devices.size(); i++)
	  {
	  const devicetype &d = devices[i];
	  const nodetype &n = nodes[d.source];

	  if (nodes[d.source].sds.size()!=0 && nodes[d.source].gates.size()!=0)
		 {
		 type = CCR_SEQUENTIAL;  // assume anything driving itself is sequential
		 states = outputs;
		 return;
		 }
	  if (nodes[d.drain].sds.size()!=0 && nodes[d.drain].gates.size()!=0)
		 {
		 type = CCR_SEQUENTIAL;  // assume anything driving itself is sequential
		 states = outputs;
		 return;
		 }
	  if (nodes[d.gate].sds.size()!=0 && nodes[d.gate].gates.size()!=0)
		 {
		 type = CCR_SEQUENTIAL;  // assume anything driving itself is sequential
		 states = outputs;
		 return;
		 }
	  }
   type = CCR_COMBINATIONAL;  
   }



// merge the two ccr's together, unifying inputs/outputs/nodes
void ccrtype::Merge(const ccrtype &right, bool mergeOutputs)
   {
   TAtype ta("ccrtype::Merge");
   int i, k;

   for (i=0; i<right.devices.size(); i++)
      {
      devicetype d = right.devices[i];
      
      devices.push(d);
      }
   inputs += right.inputs;
   compliments += right.compliments;
   outputs += right.outputs;
   for (i=0; i<inputs.size(); i++)
      {
      if (compliments[i])
         {
         compliments[i] = false;
         // now bubble all the devices
         for (k=0; k<devices.size(); k++)
            {
            devicetype &d = devices[k];
            if (d.gate == inputs[i])
               d.type = (d.type==D_NMOS) ? D_PMOS : D_NMOS;
            if (d.source == inputs[i] || d.drain == inputs[i])
               FATAL_ERROR;
            }
         }
      }
   outputs.RemoveDuplicates();
   outputs.Sort();
   inputs.RemoveDuplicates();
   inputs.Sort();
   // now remove any inputs which are now outputs
   array <int> common;
   inputs.CommonContents(common, outputs);
   for (i=0; i<common.size(); i++)
      {
      for (k=inputs.size()-1; k>=0; k--)
         {
         if (inputs[k]==common[i])
            break;
         }
      if (k<0) FATAL_ERROR;
      inputs.RemoveFast(k);
      if (!mergeOutputs){
         for (k=outputs.size()-1; k>=0; k--)
            {
            if (outputs[k]==common[i])
               break;
            }
         if (k<0) FATAL_ERROR;
         outputs.RemoveFast(k);
         }
      }
   if (!mergeOutputs)
	  {
	  states += right.states;
	  }
   else
	  {
	  states = outputs;
	  }
   if (common.size()){
      inputs.Sort();
      outputs.Sort();
      }
   
   CrossReference();
   
   if (mergeOutputs && outputs.size()==2)
      {
// now see if the two outputs are complementary
      const nodetype &n = parent.nodes[outputs[1]];
      bool nfound=false, pfound=false;
      for (k=0; k<n.sds.size(); k++)
         {
         const devicetype &d = *n.sds[k];
         if (d.type==D_NMOS && d.source==VSS && d.gate==outputs[0])
            nfound = true;
         if (d.type==D_PMOS && d.source==VDD && d.gate==outputs[0])
            pfound = true;
         }
      if (nfound && pfound)
         secondOutputIsComplimentOfFirst = true;
      }
   }

void ccrtype::PruneStates()
   {
   int i, k, j;
   const array <nodetype> &nodes = parent.nodes;

   CrossReference();

   for (i=states.size()-1; i>=0; i--)
      {
// now see if any state nodes are the output of an inverter driven by another state
// this will really speed up things like carry chains(cutting the states in half)
      const nodetype &n = nodes[states[i]];
      for (k=states.size()-1; k>=0; k--)
         {
         bool nfound=false, pfound=false;
         for (j=0; j<n.sds.size(); j++)
            {
            const devicetype &d = *n.sds[j];
            if (d.type==D_NMOS && d.source==VSS && d.gate==states[k])
               nfound = true;
            if (d.type==D_PMOS && d.source==VDD && d.gate==states[k])
               pfound = true;
            }
//		 for (j=0; j<outputs.size(); j++)
//			if (outputs[j]==states[i])
//			   nfound = pfound = false;	 // don't remove states if it's an output
         if (nfound && pfound && i!=k) {
            states.RemoveFast(i);
            break;
            }
         }
      }
   }

void ccrtype::CleanupInputs()
   {
   int i;
   
   for (i=inputs.size()-1; i>=0; i--)
	  {
	  if (inputs[i]<=VDDO && (type==CCR_COMBINATIONAL || type==CCR_SEQUENTIAL))
		 {
		 inputs.RemoveFast(i);  // remove vss/vdd connections since the connections exists in the devicelist
		 compliments.RemoveFast(i);
		 }
	  }
   }
	  


void ccrtype::Print(filetype &fp, int index) const
   {
   const char *chptr=NULL, *tag=NULL;
   char buf[256];
   int k;
   const array <nodetype> &nodes = parent.nodes;
   
   switch (type)
      {
      case CCR_NULL:			 return;
      case CCR_BUF:				 tag="BUF";  break;
      case CCR_INV:				 tag="INV";  break;
      case CCR_NAND:			 tag="NAND"; break;
      case CCR_AND:				 tag="AND";  break;
      case CCR_NOR:				 tag="NOR";  break;
      case CCR_OR:				 tag="OR";   break;
      case CCR_COMBINATIONAL:	 tag="COMB"; break;
      case CCR_SEQUENTIAL:		 tag="SEQ";  break;
      case CCR_SRAM:			 tag="SRAM"; break;
      case CCR_SRAM_WRITENABLE:	 tag="SRAM_WRITEENABLE";  break;
      default:					 tag="???";  break;
      }
   
   fp.fprintf("// CCR %d %s, OUT:",index,tag);
   for (k=0; k<outputs.size(); k++)
      {
      fp.fprintf(" %s(%d)", nodes[outputs[k]].VName(buf),outputs[k]);
      }
   fp.fprintf("   IN:");
   for (k=0; k<inputs.size(); k++)
      {
      fp.fprintf(" %s%s(%d)", compliments[k] ? "~" : "", nodes[inputs[k]].VName(buf),inputs[k]);
      }
   fp.fprintf("   STATE:");
   for (k=0; k<states.size(); k++)
      {
      fp.fprintf(" %s(%d)", nodes[states[k]].VName(buf),states[k]);
      }
   fp.fprintf(" [%d T]",devices.size());
   if (secondOutputIsComplimentOfFirst)
	  fp.fprintf("  OutputsAreComplimentary");
   if (crossCoupledNands)
	  fp.fprintf("  CrossCoupledNands");
   if (nandKeeper)
	  fp.fprintf("  NandKeeper (original nand %d)",originalNandIndex);
   if (lastTwoInputsAreWeakPulldowns)
	  fp.fprintf("  LastTwoInputsAreWeakPulldowns");   

   fp.fprintf("\n");

   for (k=0; k<devices.size(); k++)
	  {
	  const devicetype &d = devices[k];

	  if (d.gate<0 || d.gate>=nodes.size()) FATAL_ERROR;
	  if (d.drain<0 || d.drain>=nodes.size()) FATAL_ERROR;
	  if (d.source<0 || d.source>=nodes.size()) FATAL_ERROR;
	  }
   }

void ccrtype::WriteSimpleCCR(filetype &fp, int &inum)
   {
   const char *chptr=NULL, *tag=NULL;
   char buf[256];
   int i, flips;
   const array <nodetype> &nodes = parent.nodes;
   
   for (i=0, flips=0; i<inputs.size(); i++)
	  {
	  flips += compliments[i] ? 1:0;
	  }
   if (flips*2 > inputs.size())
	  {	// most of the inputs have been bubbled so demorgan the gate
	  switch (type)
		 {
		 case CCR_BUF:  type = CCR_INV;  break;
		 case CCR_INV:  type = CCR_BUF;  break;
		 case CCR_NAND: type = CCR_OR;   break;
		 case CCR_AND:  type = CCR_NOR;  break;
		 case CCR_NOR:  type = CCR_AND;  break;
		 case CCR_OR:   type = CCR_NAND; break;
		 default: FATAL_ERROR;
		 }
	  for (i=0; i<inputs.size(); i++)
		 compliments[i] = !compliments[i];
	  }
   if (inputs.size()==1 && inputs[0]<=VDDO && type==CCR_INV)
	  {
	  type = CCR_BUF;
	  compliments[0] = !compliments[0];
	  }
   
   switch (type)
      {
      case CCR_BUF:  chptr="buf";  break;
      case CCR_INV:  chptr="not";  break;
      case CCR_NAND: chptr="nand"; break;
      case CCR_AND:  chptr="and"; break;
      case CCR_NOR:  chptr="nor";  break;
      case CCR_OR:   chptr="or";  break;
      default:       chptr=NULL;   break;
      }
   
   if (chptr!=NULL)
      {
	  if (outputs.size()!=1) FATAL_ERROR;
      fp.fprintf("%-4s i%d(%s", chptr, inum++, nodes[outputs[0]].VName(buf));
      for (i=0; i<inputs.size(); i++)
         {
		 if (inputs[i]==VSS)
			fp.fprintf(", %s",compliments[i] ? "1'b1":"1'b0");
		 else if (inputs[i]==VDD || inputs[i]==VDDO)
			fp.fprintf(", %s",compliments[i] ? "1'b0":"1'b1");
		 else
            fp.fprintf(", %s%s",compliments[i] ? "~" : "", nodes[inputs[i]].VName(buf));
         }
      fp.fprintf(");\n");
	  }
   }

void ccrtype::WriteSram(filetype &fp, int &inum)
   {
   char buf[256];
   int i;
   const array <nodetype> &nodes = parent.nodes;
   
   if (outputs.size()!=1) FATAL_ERROR;
   
   fp.fprintf("sram_udp #(0.01) i%d(%s", inum++, nodes[outputs[0]].VName(buf));
   for (i=0; i<inputs.size(); i++)
	  {
	  if (inputs[i]==VSS)
		 fp.fprintf(", %s",compliments[i] ? "1'b1":"1'b0");
	  else if (inputs[i]==VDD || inputs[i]==VDDO)
		 fp.fprintf(", %s",compliments[i] ? "1'b0":"1'b1");
	  else
		 fp.fprintf(", %s%s",compliments[i] ? "~" : "", nodes[inputs[i]].VName(buf));
	  }
   fp.fprintf(");\n");
   }


void wirelisttype::BuildCCRArcLists()
   {
   TAtype ta("wirelisttype::BuildCCRArcLists");   
   array <array <int> > inputs(nodes.size());
   array <array <int> > outputs(nodes.size());
   int i, k;

   for (i=0; i<ccrs.size(); i++)
      {
      ccrtype &ccr = ccrs[i];

      for (k=0; k<ccr.inputs.size(); k++)
         {
         inputs[ccr.inputs[k]].push(i);
         }
      for (k=0; k<ccr.outputs.size(); k++)
         {
         outputs[ccr.outputs[k]].push(i);
         }
	  ccr.arc_next.clear();
	  ccr.arc_prior.clear();
      }
   for (i=0; i<nodes.size(); i++)
      {
      inputs[i].RemoveDuplicates();
      outputs[i].RemoveDuplicates();

      for (k=0; k<inputs[i].size(); k++)
         {
         int ccrindex = inputs[i][k];
         ccrtype &ccr = ccrs[ccrindex];

         ccr.arc_prior += outputs[i];
         }
      for (k=0; k<outputs[i].size(); k++)
         {
         int ccrindex = outputs[i][k];
         ccrtype &ccr = ccrs[ccrindex];

         ccr.arc_next += inputs[i];
         }
      }
   for (i=0; i<ccrs.size(); i++)
      {
      ccrtype &ccr = ccrs[i];

      ccr.arc_next.Sort();
      ccr.arc_prior.Sort();
	  for (k=0; k<ccr.arc_next.size(); k++)
		 if (ccr.arc_next[k]==i) FATAL_ERROR; // devices shorted to themselves shouldn't happen
	  for (k=0; k<ccr.arc_prior.size(); k++)
		 if (ccr.arc_prior[k]==i) FATAL_ERROR;
      }
   }

void wirelisttype::ConsolidateCCR(filetype &fp)
   {
   TAtype ta("wirelisttype::ConsolidateCCR");   
   int i, k;

// first find cross coupled nodes   
   BuildCCRArcLists();
   for (i=0; i<ccrs.size(); i++)
      {
      ccrtype &ccr = ccrs[i];
      array <int> common;

      ccr.arc_prior.CommonContents(common, ccr.arc_next);

      for (k=0; k<common.size(); k++)
         {
         ccrtype &other = ccrs[common[k]];
         if (common[k]==i) FATAL_ERROR;

		 if (ccr.type==CCR_NAND && other.type==CCR_NAND)
			{
			ccr.crossCoupledNands = true;	// hint for later operations
			ccr.Merge(other, true);
			ccr.type = CCR_SEQUENTIAL;
			ccr.arc_prior.clear();		// these are now stale
			ccr.arc_next.clear();		// these are now stale
			other.Clear();
			}
		 else if ((ccr.type==CCR_COMBINATIONAL || ccr.type==CCR_SEQUENTIAL) && other.type==CCR_NAND)
			{ // now see if the other device is just a keeper(single pmos, source=vdd)
			bool found=true;
			int j;
			int zznode = -1;
			for (j=0; j<ccr.devices.size(); j++)
			   {
			   const devicetype &d = ccr.devices[j];
			   int m;
			   if (d.gate == other.outputs[0] && (d.source!=VDD || d.type!=D_PMOS))
				  found = false;
			   if (d.gate == other.outputs[0])
				  {				  
				  for (m=other.inputs.size()-1; m>=0; m--)
					 if (other.inputs[m]==d.drain)
						{
						zznode = d.drain;
						break;
						}
				  if (m<0) found = false;
				  }
			   }
			if (other.inputs.size()==3)
			   {
			   // nand3 keepers cause too much trouble because I can't represent multibit state in udp's
			   // to eliminate this problem I'm replacing nand keepers with simple keepers
			   // this does carry some risk!
			   ccrtype inv(*this);
			   devicetype d;
			   inv.type = CCR_INV;
			   inv.inputs.push(zznode);
			   inv.compliments.push(false);
			   inv.outputs.push(other.outputs[0]);
			   d.type = D_NMOS;
			   d.source = VSS;
			   d.gate = zznode;
			   d.drain = other.outputs[0];
			   d.length = d.width = 1.0;
			   d.name = "Inv replacement for nand keeper";
			   inv.devices.push(d);
			   d.type = D_PMOS;
			   d.source = VDD;
			   inv.devices.push(d);
			   ccr.Merge(inv, true);
			   ccr.type = CCR_SEQUENTIAL;
			   ccr.arc_prior.clear();		// these are now stale
			   ccr.arc_next.clear();		// these are now stale
			   }
			else
			   {
			   ccr.Merge(other, true);
			   ccr.nandKeeper = true;
			   ccr.originalNandIndex = common[k];
			   ccr.type = CCR_SEQUENTIAL;
			   ccr.arc_prior.clear();		// these are now stale
			   ccr.arc_next.clear();		// these are now stale
			   other.type = CCR_NULL;
			   other.arc_next.clear();
			   other.arc_prior.clear();
			   }
			}
		 else if (ccr.type==CCR_NAND)
			; // I want nands to be other, if things are cross coupled it will come around
		 else
			{
			ccr.Merge(other, true);
			ccr.type = CCR_SEQUENTIAL;
			ccr.arc_prior.clear();		// these are now stale
			ccr.arc_next.clear();		// these are now stale
			other.Clear();
			}
         }
      }
   BuildCCRArcLists();
   for (i=0; i<ccrs.size(); i++)
      {
      ccrtype &ccr = ccrs[i];
      array <int> common;

      ccr.arc_prior.CommonContents(common, ccr.arc_next);

      for (k=0; k<common.size(); k++)
         {
         ccrtype &other = ccrs[common[k]];
         if (common[k]==i) FATAL_ERROR;

//		 printf("Merging %d %d\n",i, common[k]);

		 ccr.Merge(other, true);
         ccr.type = CCR_SEQUENTIAL;
		 ccr.crossCoupledNands = ccr.nandKeeper = false;
         other.Clear();
		 BuildCCRArcLists();
         }
      }


   if (DEBUG){
	  fp.fprintf("//Completed looking for cross coupled.\n");
	  for (i=0; i<ccrs.size(); i++)
		 {
		 ccrtype &ccr = ccrs[i];
         ccr.Print(fp, i);
		 }
	  }


// now find any place where by sucking in another ccr things can be simplified
   BuildCCRArcLists();
   for (i=0; i<ccrs.size(); i++)
      {
      ccrtype &ccr = ccrs[i];
      for (k=0; k<ccr.arc_prior.size() && !ccr.dontMerge; k++)
         {
		 ccrtype &prior = ccrs[ccr.arc_prior[k]];
         if ((ccr.type==CCR_SEQUENTIAL || ccr.type==CCR_COMBINATIONAL) && (prior.type==CCR_SEQUENTIAL || prior.type==CCR_COMBINATIONAL) && !prior.dontMerge)
            {
            array <int> common, outs, sts;
            int j;

            for (j=0; j<ccr.inputs.size(); j++)
               common.push(ccr.inputs[j]);
            for (j=0; j<prior.inputs.size(); j++)
               common.push(prior.inputs[j]);
            for (j=0; j<prior.outputs.size(); j++)
               outs.push(prior.outputs[j]);
            common.RemoveDuplicates();
            common.Sort();
			outs.Sort();
            common.RemoveCommonElements(outs);
			sts+=ccr.states;
			sts+=prior.states;
			sts.RemoveDuplicates();
            if ((common.size() <= MAXIMUM(ccr.inputs.size() , prior.inputs.size()) && sts.size()<=4 && ccr.type==CCR_SEQUENTIAL && prior.type==CCR_SEQUENTIAL) ||
			    (common.size() < MAXIMUM(ccr.inputs.size() , prior.inputs.size()) && sts.size()<=4))
               {
			   if (ccr.crossCoupledNands && prior.nandKeeper)
				  {
				  ccrtype &oldnand = ccrs[prior.originalNandIndex];
				  ccr.Merge(oldnand, false);
				  ccr.dontMerge = true;
				  for (j=prior.outputs.size()-1; j>=0; j--)
					 {
					 if (prior.outputs[j]==oldnand.outputs[0])
						{
						prior.outputs.RemoveFast(j);
						break;
						}
					 }
				  if (j<0) FATAL_ERROR;
				  }
			   else
				  {
                  ccr.Merge(prior, false);
				  }
			   ccr.crossCoupledNands = ccr.nandKeeper = false;
			   ccr.arc_prior.clear();
			   ccr.arc_next.clear();
               k=-1;  // the arc list may have been reordered so start searching again
               }
            }
         }
      }
   BuildCCRArcLists();
   for (i=0; i<ccrs.size(); i++)
      {
      ccrtype &ccr = ccrs[i];
      for (k=0; k<ccr.arc_prior.size() && !ccr.dontMerge; k++)
         {
		 ccrtype &prior = ccrs[ccr.arc_prior[k]];
         if ((ccr.type==CCR_SEQUENTIAL || ccr.type==CCR_COMBINATIONAL) && (prior.type==CCR_SEQUENTIAL || prior.type==CCR_COMBINATIONAL || prior.type==CCR_NAND) && !prior.dontMerge)
            {
            array <int> common, outs, sts;
            int j;

            for (j=0; j<ccr.inputs.size(); j++)
               common.push(ccr.inputs[j]);
            for (j=0; j<prior.inputs.size(); j++)
               common.push(prior.inputs[j]);
            for (j=0; j<prior.outputs.size(); j++)
               outs.push(prior.outputs[j]);
            common.RemoveDuplicates();
            common.Sort();
			outs.Sort();
            common.RemoveCommonElements(outs);
			sts+=ccr.states;
			sts+=prior.states;
			sts.RemoveDuplicates();
            if ((common.size() <= MAXIMUM(ccr.inputs.size() , prior.inputs.size()) && sts.size()<=4 && ccr.type==CCR_SEQUENTIAL && prior.type==CCR_SEQUENTIAL) ||
			    (common.size() < MAXIMUM(ccr.inputs.size() , prior.inputs.size()) && sts.size()<=4) ||
				(common.size() == ccr.inputs.size() && ccr.crossCoupledNands && prior.type==CCR_NAND))
               {
			   if (ccr.crossCoupledNands && prior.nandKeeper)
				  {
				  ccrtype &oldnand = ccrs[prior.originalNandIndex];
				  ccr.Merge(oldnand, false);
				  ccr.dontMerge = true;
				  for (j=prior.outputs.size()-1; j>=0; j--)
					 {
					 if (prior.outputs[j]==oldnand.outputs[0])
						{
						prior.outputs.RemoveFast(j);
						break;
						}
					 }
				  if (j<0) FATAL_ERROR;
				  }
			   else
				  {
                  ccr.Merge(prior, false);
				  }
			   ccr.crossCoupledNands = ccr.nandKeeper = false;
			   BuildCCRArcLists();
               k=-1;  // the arc list may have been reordered so start searching again
               }
            }
         }
      }
// I want to look specificly for a seq ccr with 2 outputs going through inverters to a 2 input seq ccr
// This will allow me to merge our common sense amp flops reducing the udp's from 3 to 1
   for (i=0; i<ccrs.size(); i++)
      {
      ccrtype &ccr = ccrs[i];
	  if (ccr.type==CCR_SEQUENTIAL && ccr.inputs.size()==2 && ccr.arc_prior.size()==2)
		 {
		 ccrtype &inv1 = ccrs[ccr.arc_prior[0]];
		 ccrtype &inv2 = ccrs[ccr.arc_prior[1]];
		 
		 if (inv1.type==CCR_INV && inv2.type==CCR_INV && inv1.arc_prior.size()==1 && inv2.arc_prior.size()==1 && inv1.arc_prior[0]==inv2.arc_prior[0])
			{
			ccrtype &prior = ccrs[inv1.arc_prior[0]];
			
			if (prior.type==CCR_SEQUENTIAL && prior.outputs.size()==2 && !prior.dontMerge && !ccr.dontMerge)
			   {
			   ccr.Merge(inv1, false);
			   ccr.Merge(inv2, false);
			   ccr.Merge(prior, false);
			   ccr.crossCoupledNands = ccr.nandKeeper = false;
               BuildCCRArcLists();
			   }
			
			}
         }
      }

   if (DEBUG){
	  fp.fprintf("//Completed merge to reduce inputs.\n");
	  for (i=0; i<ccrs.size(); i++)
		 {
		 ccrtype &ccr = ccrs[i];
         ccr.Print(fp, i);
		 }
	  }
   // go through and look for 6t sram's. I will remodel them to separate read from write
   array <ccrtype> newentries;   

   for (i=0; i<ccrs.size(); i++)
      {
      ccrtype &ccr = ccrs[i];

	  if (ccr.type==CCR_SEQUENTIAL)
		 {
		 ccr.SramTransform(newentries);
		 if (newentries.size()!=0)
			{
			ccrs += newentries;
			newentries.clear();
			}
		 }
	  }

   if (DEBUG){
	  fp.fprintf("//Completed sram transform.\n");
	  for (i=0; i<ccrs.size(); i++)
		 {
		 ccrtype &ccr = ccrs[i];
         ccr.Print(fp, i);
		 }
	  }


// Go through and eliminate ccr's outputs are compliments
   bool invalidateNodes=false;
   for (i=ccrs.size()-1; i>=0; i--)
      {
      ccrtype &ccr = ccrs[i];
      if (ccr.outputs.size()==2 && ccr.secondOutputIsComplimentOfFirst)
         {
         const nodetype *n0 = &nodes[ccr.outputs[0]];
         const nodetype *n1 = &nodes[ccr.outputs[1]];
         
         if ((n1->drivesInstance || n1->bristle) && ccr.type!=CCR_SRAM)  // swap outputs so I delete the less needed one
            {
            int temp = ccr.outputs[0];
            ccr.outputs[0] = ccr.outputs[1];
            ccr.outputs[1] = temp;
            n0 = &nodes[ccr.outputs[0]];
            n1 = &nodes[ccr.outputs[1]];
            }
         int out0 = ccr.outputs[0];
         int out1 = ccr.outputs[1];
         
         ccr.outputs.pop();
		 // now add an inverter, this inverter has a high likelihood of being deleted later
		 ccrs.push(ccrtype(*this));
		 ccrtype &nccr  = ccrs.back();
		 nccr.type = CCR_INV;
		 nccr.inputs.push(out0);
		 nccr.compliments.push(false);
		 nccr.outputs.push(out1);
         }
      }
   if (DEBUG){
	  fp.fprintf("//Completed 2 outputs are compliments reduction.\n");
	  for (i=0; i<ccrs.size(); i++)
		 {
		 ccrtype &ccr = ccrs[i];
         ccr.Print(fp, i);
		 }
	  }

// now find any ccr's which have unneeded outputs
   bool done;
   do{
	  done = true;
	  for (i=0; i<nodes.size(); i++)
		 {
		 nodetype &n = nodes[i];
		 n.drivesCCR = false;
		 }
	  for (i=0; i<ccrs.size(); i++)
		 {
		 ccrtype &ccr = ccrs[i];
		 
		 for (k=0; k<ccr.inputs.size() && ccr.type!=CCR_NULL; k++)
			{
			nodetype &n = nodes[ccr.inputs[k]];
			n.drivesCCR = true;
			}
		 }
	  for (i=0; i<ccrs.size(); i++)
		 {
		 ccrtype &ccr = ccrs[i];
		 bool deleted=false;
		 
		 for (k=ccr.outputs.size()-1; k>=0; k--)
			{
			const nodetype &n = nodes[ccr.outputs[k]];
			
			if (!n.drivesCCR && !n.drivesInstance && !n.bristle)
			   {
			   ccr.outputs.RemoveFast(k);
			   deleted = true;
			   }
			}
		 if (deleted)
			ccr.outputs.Sort();
		 if (ccr.outputs.size()==0 && ccr.type!=CCR_NULL)
			{
			ccr.type = CCR_NULL;
			done = false;
			}
		 }
	  }while (!done);

   if (DEBUG){
	  fp.fprintf("//Completed first unneeded ccr pruning.\n");
	  for (i=0; i<ccrs.size(); i++)
		 {
		 ccrtype &ccr = ccrs[i];
         ccr.Print(fp, i);
		 }
	  }

   BuildCCRArcLists();

// now look for an ccr's with too many inputs and try to rom/rf simplify them
   for (i=ccrs.size()-1; i>=0; i--)
      {
      ccrtype &ccr = ccrs[i];

	  if ((ccr.type==CCR_COMBINATIONAL || ccr.type==CCR_SEQUENTIAL) && ccr.inputs.size()+ccr.states.size()>9)
		 {
		 ccr.Simplification(newentries);
		 
		 ccrs += newentries;
		 newentries.clear();
		 }	 
	  }

   if (DEBUG){
	  fp.fprintf("//Completed rom/rf simplification.\n");
	  for (i=0; i<ccrs.size(); i++)
		 {
		 ccrtype &ccr = ccrs[i];
         ccr.Print(fp, i);
		 }
	  }

// get rid of compliments for blocks that I simulate
   for (i=0; i<ccrs.size(); i++)
	  {
	  ccrtype &ccr = ccrs[i];

	  if (ccr.type==CCR_COMBINATIONAL || ccr.type==CCR_SEQUENTIAL ||ccr.type==CCR_SRAM_WRITENABLE)
		 {
		 for (k=0; k<ccr.inputs.size(); k++)
			{
			int in = ccr.inputs[k];
			int j;
			if (ccr.compliments[k])
			   {
			   ccr.compliments[k] = false;
			   for (j=0; j<devices.size(); j++)
				  {
				  devicetype &d = ccr.devices[j];

				  if (d.source==in || d.drain==in) FATAL_ERROR;
				  if (d.gate==in)
					 {
					 d.InvertDevicetype();
					 }
				  }
			   }
			}
		 }
	  }


// now go through and trim out inverters/buffers
   do{
      done = true;

	  BuildCCRArcLists();

      for (i=0; i<ccrs.size(); i++)
         {
         ccrtype &ccr = ccrs[i];
         for (k=0; k<ccr.arc_prior.size(); k++)
            {
			ccrtype &prior = ccrs[ccr.arc_prior[k]];

			if (prior.type==CCR_INV || prior.type==CCR_BUF)
			   {
			   if (prior.inputs.size()!=1) FATAL_ERROR;
			   if (prior.outputs.size()!=1) FATAL_ERROR;
			   int out = prior.outputs[0];
			   int j;
			   
			   for (j=ccr.inputs.size()-1; j>=0; j--)
				  {
				  if (ccr.inputs[j]==prior.inputs[0] && prior.type==CCR_INV && !ccr.compliments[j])
					 break;
				  if (ccr.inputs[j]==prior.inputs[0] && prior.type==CCR_BUF && ccr.compliments[j])
					 break;
				  }
			   
			   if (j>=0 && (ccr.type==CCR_COMBINATIONAL || ccr.type==CCR_SEQUENTIAL || ccr.type==CCR_SRAM_WRITENABLE))
				  {	// change the device polarity to model inversion
				    // because I'm in this code block I know that the inverter input is already an input to this ccr
				  for (j=0; j<ccr.devices.size(); j++)
					 {
					 devicetype &d = ccr.devices[j];
					 
					 if (d.source==out || d.drain==out) FATAL_ERROR;
					 if (d.gate==out)
						{
						d.InvertDevicetype();
						d.gate = prior.inputs[0];
						}
					 }
				  for (j=ccr.inputs.size()-1; j>=0; j--)
					 {
					 if (ccr.inputs[j]==out)
						{
						ccr.inputs.RemoveFast(j);
						ccr.compliments.RemoveFast(j);
						done = false;
						}
					 }
				  
				  }
			   else if (j<0 || ccr.type==CCR_AND || ccr.type==CCR_OR || ccr.type==CCR_NAND || ccr.type==CCR_NOR)
				  {
				  bool bubbleDevices=false;
				  for (j=ccr.inputs.size()-1; j>=0; j--)
					 {
					 if (ccr.inputs[j]==out)
						{
						if (prior.type==CCR_INV)
						   ccr.compliments[j] = !ccr.compliments[j];
						if (prior.compliments[0])
						   ccr.compliments[j] = !ccr.compliments[j];
						ccr.inputs[j] = prior.inputs[0];
						done = false;
						break;
						}
					 }
				  for (j=0; j<ccr.devices.size(); j++)
					 {
					 devicetype &d = ccr.devices[j];
					 
					 if (d.source==out || d.drain==out) FATAL_ERROR;
					 if (d.gate==out)
						{
						d.gate = prior.inputs[0];
						}
					 }
				  }
			   // I can now have a case where an output of this ccr now drives itself
			   // This happens with artisan pnr gates(cmpr42)
			   for (j=ccr.inputs.size()-1; j>=0; j--)
				  {
				  int m, n;
				  for (m=0; m<ccr.outputs.size(); m++)
					 {
					 if (ccr.inputs[j]==ccr.outputs[m] && !ccr.compliments[j])
						{
						ccr.inputs.RemoveFast(j);
						ccr.compliments.RemoveFast(j);
						}
					 else if (ccr.inputs[j]==ccr.outputs[m] && ccr.compliments[j])
						{
						// Now I need to bubble the devices
						for (n=0; n<ccr.devices.size(); n++)
						   {
						   devicetype &d = ccr.devices[n];
						   if (d.gate==ccr.inputs[j])
							  d.InvertDevicetype();
						   }
						ccr.inputs.RemoveFast(j);
						ccr.compliments.RemoveFast(j);
						}
					 }

				  }
			   }
            }
         if (ccr.type==CCR_INV && ccr.inputs.size()==1 && ccr.compliments[0])
            {
            ccr.type = CCR_BUF;
            ccr.compliments[0] = false;
            }
         }
      }while (!done);


// now find any ccr's which have unneeded outputs
   do{
	  done = true;
	  for (i=0; i<nodes.size(); i++)
		 {
		 nodetype &n = nodes[i];
		 n.drivesCCR = false;
		 }
	  for (i=0; i<ccrs.size(); i++)
		 {
		 ccrtype &ccr = ccrs[i];
		 
		 for (k=0; k<ccr.inputs.size() && ccr.type!=CCR_NULL; k++)
			{
			nodetype &n = nodes[ccr.inputs[k]];
			n.drivesCCR = true;
			}
		 // this will get rid of duplicated inputs
		 for (k=ccr.inputs.size()-1; k>=0 && ccr.type!=CCR_SRAM; k--)
			{
			int j;
			for (j=k-1; j>=0; j--)
			   {
			   if (ccr.inputs[k]==ccr.inputs[j] && ccr.compliments[k]==ccr.compliments[j])
				  {
				  ccr.inputs.RemoveFast(k);
				  ccr.compliments.RemoveFast(k);
				  break;
				  }
			   }
			}
		 }
	  for (i=0; i<ccrs.size(); i++)
		 {
		 ccrtype &ccr = ccrs[i];
		 bool deleted=false;
		 
		 for (k=ccr.outputs.size()-1; k>=0; k--)
			{
			const nodetype &n = nodes[ccr.outputs[k]];
			
			if (!n.drivesCCR && !n.drivesInstance && !n.bristle)
			   {
			   ccr.outputs.RemoveFast(k);
			   deleted = true;
			   }
			}
		 if (deleted)
			ccr.outputs.Sort();
		 if (ccr.outputs.size()==0 && ccr.type!=CCR_NULL)
			{
			ccr.type = CCR_NULL;
			done = false;
			}
		 }
	  }while (!done);

   if (DEBUG){
	  fp.fprintf("//Completed inverter/buffer deletion.\n");
	  for (i=0; i<ccrs.size(); i++)
		 {
		 ccrtype &ccr = ccrs[i];
         ccr.Print(fp, i);
		 }
	  }


   BuildCCRArcLists();

// now whack unused ccrs   
   for (i=0; i<ccrs.size(); i++)
      {
      ccrtype &ccr = ccrs[i];

      bool needed=false;
      for (k=0; k<ccr.outputs.size(); k++)
         {
         const nodetype &n = nodes[ccr.outputs[k]];
         if (ccr.arc_next.size()!=0 || n.bristle || n.drivesInstance)
            needed=true;
         }
      if (!needed)
         {
         ccr.Clear();
         }
      ccr.PruneStates();
      }

   if (DEBUG){
	  fp.fprintf("//Completed whacking ccr's with unused outputs.\n");
	  for (i=0; i<ccrs.size(); i++)
		 {
		 ccrtype &ccr = ccrs[i];
         ccr.Print(fp, i);
		 }
	  }
   

// now whack unused ccrs   
   for (i=0; i<ccrs.size(); i++)
      {
      ccrtype &ccr = ccrs[i];

      bool needed=false;
      for (k=0; k<ccr.outputs.size(); k++)
         {
         const nodetype &n = nodes[ccr.outputs[k]];
         if (ccr.arc_next.size()!=0 || n.bristle || n.drivesInstance)
            needed=true;
         }
      if (!needed)
         {
         ccr.Clear();
         }
      ccr.PruneStates();
      }

// now cleanup the inputs for comb/seq ccr's
   for (i=0; i<ccrs.size(); i++)
      {
      ccrtype &ccr = ccrs[i];

	  if (ccr.type==CCR_COMBINATIONAL || ccr.type==CCR_SEQUENTIAL)
	     ccr.CleanupInputs();
	  }
   if (DEBUG){
	  fp.fprintf("//Completed pruning inputs.\n");
	  for (i=0; i<ccrs.size(); i++)
		 {
		 ccrtype &ccr = ccrs[i];
         ccr.Print(fp, i);
		 }
	  }
   // now remove unneeded wires. This gets rid of some clutter from signalscan mainly
   for (i=0; i<nodes.size(); i++)
      {
      nodetype &n = nodes[i];
      n.drivesCCR = false;
      }
   for (i=0; i<ccrs.size(); i++)
	  {
	  ccrtype &ccr = ccrs[i];
	  for (k=0; k<ccr.inputs.size(); k++)
		 {
		 nodes[ccr.inputs[k]].drivesCCR = true;
		 }
	  }
   for (i=0; i<nodes.size(); i++)
      {
      nodetype &n = nodes[i];
      
	  if (n.bristle || n.drivesInstance || n.drivesCCR)
		 ;
	  else
		 n.state = false;
      }
   }

void wirelisttype::AbstractCCR(filetype &fp, int &inum)
   {
   TAtype ta("wirelisttype::AbstractCCR");
   int i;

   for (i=0; i<ccrs.size(); i++)
	  {
	  ccrtype &ccr = ccrs[i];

      if (DEBUG)
         ccr.Print(fp, i);      
      
      switch (ccr.type)
         {
         case CCR_COMBINATIONAL:
			{
			ccr_circuittype circuit(*this);			
            circuit.CombinationalTruthTable(ccr, fp, inum); 
			break;
			}
         case CCR_SEQUENTIAL:
			{
			ccr_circuittype circuit(*this);			
            circuit.SequentialTruthTable(ccr, fp, inum);
            break;
			}
         case CCR_BUF:
         case CCR_INV:
         case CCR_NAND:
         case CCR_NOR:
         case CCR_AND:
         case CCR_OR:
            ccr.WriteSimpleCCR(fp, inum); break;
		 case CCR_SRAM:
			ccr.WriteSram(fp, inum); break;
         case CCR_SRAM_WRITENABLE:
			{
			ccr_circuittype circuit(*this);			
			circuit.SramWritenableTruthTable(ccr, fp, inum); 
			break;
			}
		 case CCR_NULL: 
            break;
         default:
            FATAL_ERROR; break;
         }
      }
   }





void wirelisttype::WriteVerilog(wildcardtype &ignoreStructures, wildcardtype &explodeStructures, wildcardtype &explodeUnderStructures, wildcardtype &deleteStructures, const char *&destname, bool atpg, bool assertionCheck, bool debug, filetype &fp, bool makeMap)
   {
   TAtype ta("wirelisttype::WriteVerilog");   
   __uint64 md1, md2;
   int i, ii, k;
   hashtype <const void *, bool> beenHere;
   array <wirelisttype *> notDone;
   wirelisttype *wptr;
   char buffer[512], buffer2[512];
   filetype map;

   DEBUG = debug;

   sha(name, strlen(name), md1, md2);
   sprintf(unique_string, "_%s", Int64ToString(md1&0xFFFFFFFF,buffer2));
#ifdef _MSC_VER
   sprintf(buffer,"%s%s%s.v",name,assertionCheck?"_assert":"", unique_string);
#else
   sprintf(buffer,"%s%s%s.v",name,assertionCheck?"_assert":"", unique_string);
#endif
   if (destname==NULL)
      destname = stringPool.NewString(buffer);
   fp.fopen(destname,"wt");
   printf("\nWirelisting to %s\n",destname);
   
   sha(name, strlen(name), md1, md2);
   sprintf(unique_string, "_%s", Int64ToString(md1&0xFFFFFFFF, buffer2));
#ifdef _MSC_VER
   sprintf(buffer,"%s%s.map",name, unique_string);
#else
   sprintf(buffer,"%s%s.map.gz",name, unique_string);
#endif
   if (makeMap)
	  map.fopen(buffer,"wt");   
   
   fp.fprintf("`timescale 1ns / 1ps\n\n");
   time_t timer = time(NULL);
   fp.fprintf("//INFO This was extracted %s\n",asctime(localtime(&timer))),

   ExplicitDeletes(ignoreStructures, deleteStructures, fp);
   ExplicitUnderExplodes(ignoreStructures, explodeUnderStructures, fp);
   ExplicitExplodes(ignoreStructures, explodeStructures, fp);
   ExplicitExplodes(ignoreStructures, explodeStructures, fp);
   ExplicitExplodes(ignoreStructures, explodeStructures, fp);
   ExplicitExplodes(ignoreStructures, explodeStructures, fp);

   beenHere.clear();
   VddoCheck(beenHere, fp);
   beenHere.clear();
   CtranCamTransform(beenHere, fp);
   
   // first thing I need to do is find source/drain regions that span hierarchy. I flatten things until this is
   // no longer the case
   
   for (i=0; i<bristles.size(); i++)
      bristles[i].pad = atpg;   // I want all top level routes to be input or inout
   beenHere.clear();
   DetermineBristleDirection(beenHere, ignoreStructures);
   beenHere.clear();                       // make two passes to catch all the pad references
   DetermineBristleDirection(beenHere, ignoreStructures);

   beenHere.clear();
   notDone.push(this);
   int inum = 0;

   while (notDone.pop(wptr))
      {
      bool doneAtThisLevel;
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
                  if (inst.outsideNodes[k]<=VDDO || (driven[inst.outsideNodes[k]]>1 && !inverter[inst.outsideNodes[k]]))
                     {
                     if (!ignoreStructures.Check(inst.wptr->name)){
                        const instancetype badinst = inst;
                        const char *foo0 = badinst.name;
                        const char *foo1 = badinst.wptr->name;
						if (DEBUG){
                           fp.fprintf("//INFO: Flattening instance %s(%s).",badinst.name,badinst.wptr->name);
                           fp.fprintf(" Bristle %s has a S/D connection.\n",inst.wptr->bristles[k].name);
						   }
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
                           
                           fp.fprintf("//INFO: Placing ghost inverters on instance %s, ",inst.name,inst.wptr->name);
                           fp.fprintf(" Bristle %s(%s) has a S/D connection.\n",inst.wptr->bristles[k].name,wptr->nodes[inst.outsideNodes[k]].name);
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
               fp.fprintf("//INFO: Deleting instance %s(%s)\n", inst.name, inst.wptr->name);
            wptr->instances.RemoveFast(i);
            }
         else if (inst.outsideNodes.size()==1 && inst.wptr->bristles[0].input)  // only 1 input connection, probably dcap so delete
            {
            if (DEBUG)
               fp.fprintf("//INFO: Deleting instance %s(%s)\n", inst.name, inst.wptr->name);
            wptr->instances.RemoveFast(i);
            }
         else if (inst.wptr->devices.size()==0 && inst.wptr->instances.size()==0)  // empty cell(perhaps empty rom cell)
            {
            if (DEBUG)
               fp.fprintf("//INFO: Deleting instance %s(%s)\n", inst.name, inst.wptr->name);
            wptr->instances.RemoveFast(i);
            }
         }

      printf("Processing module %s\n", wptr->name);
	  wptr->ConvertToCCR(fp);
	  wptr->BuildCCRArcLists();
      wptr->ConsolidateCCR(fp);
      fp.fprintf( "module %s%s(\n", wptr->name, unique_string);
      if (makeMap){
		 map.fprintf("\nset rtl_scope RTL_PLACEHOLDER.%s",wptr->name);
         map.fprintf("\nset sch_scope SCH_PLACEHOLDER.dac_%s",wptr->name);
         map.fprintf("\nset cmp_clock CLK_PLACEHOLDER\n");
		 }
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
         fp.fprintf( "\t\t\t\t%s%s\n", bs[i].VName(buffer), (i==bs.size()-1)?"":",");
      fp.fprintf(");\n");
      for (k=0; k<bs.size(); k++)
         if (bs[k].lsb>=0)
            {
            char rtl[256];
            bs[k].VName(rtl);
            fp.fprintf( "%s\t[%d:%d]\t %s;\n",bs[k].output ? (bs[k].pad ? "inout" : "output") : "input  ", bs[k].msb, bs[k].lsb, bs[k].VName(buffer));
            if (makeMap) map.fprintf("\nw %s[%d:%d] \t%s[%d:%d]",rtl+2, bs[k].msb, bs[k].lsb,bs[k].VName(buffer), bs[k].msb, bs[k].lsb);
            }
         else
            {
            char rtl[256];
            bs[k].VName(rtl);
            fp.fprintf( "%s\t\t %s;\n",bs[k].output ? (bs[k].pad ? "inout" : "output") : "input  ", bs[k].VName(buffer));
            if (makeMap) map.fprintf("\nw %s \t%s", rtl+2, bs[k].VName(buffer));
            }
      for (i=0; i<ns.size(); i++)
         {
         for (k=bs.size()-1; k>=0; k--)
            if (bs[k] == ns[i])
               break;
         if (k<0)
            if (ns[i].lsb>=0)
               fp.fprintf( "wire\t[%d:%d]\t %s;\n", ns[i].msb, ns[i].lsb, ns[i].VName(buffer));
            else
               fp.fprintf( "wire\t\t %s;\n", ns[i].VName(buffer));
         }      
      fp.fprintf("wire\t\t dummysig;\n");
      
      for (ii=wptr->instances.size()-1; ii>=0; ii--)
         {
         const instancetype &inst = wptr->instances[ii];
         bool dummy;
         bool substituteModule=false;
         char appendedname[512];
         strcpy(appendedname, inst.wptr->name);
         strcat(appendedname, unique_string);
         
//         if (strcmp(inst.wptr->name,"dpr512x2m8")==0) 
//            printf("");
         if (ignoreStructures.Check(inst.wptr->name))
            {
            substituteModule = true;
            fp.fprintf("\n%s %s(\n", inst.wptr->name, inst.VName(buffer));
            }
         else if(ignoreStructures.Check(appendedname))
            {
            substituteModule = true;
            fp.fprintf("%s %s(\n", appendedname, inst.VName(buffer));
            }
         else            
            fp.fprintf("%s%s %s(\n", inst.wptr->name, unique_string, inst.VName(buffer));
         
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
                  fp.fprintf( "\t\t.%-36s (%s)", bptr, chptr);
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
               fp.fprintf(",\n");
            }
         fp.fprintf(");\n");
         if (!beenHere.Check(inst.wptr, dummy) && !ignoreStructures.Check(inst.wptr->name) && !ignoreStructures.Check(appendedname))
            {
            notDone.push(inst.wptr);
            beenHere.Add(inst.wptr, true);
            }
         }
      wptr->AbstractCCR(fp, inum);
  
      fp.fprintf( "endmodule\n\n");
      if (makeMap) map.fprintf("\n\n");
      }
   printf("Dumping udp's\n");
   ccr_circuittype::DumpUdps(fp, atpg, false);

   fp.fprintf("\n\n");
   if (makeMap) map.fclose();
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
			if (DEBUG)
               fp.fprintf("//INFO: Flattening instance %s(%s) due to 'EXPLODE_' in instance name or substitute file.\n",badinst.name,badinst.wptr->name);
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

void wirelisttype::ExplicitUnderExplodes(wildcardtype &ignoreStructures, wildcardtype &explodeUnderStructures, filetype &fp)
   {
   TAtype ta("wirelisttype::ExplicitUnderExplodes");
   array <wirelisttype *> notDone;
   hashtype <const void *, bool> beenHere;
   wirelisttype *wptr;
   int i;

   beenHere.clear();
   notDone.push(this);

   while (notDone.pop(wptr))
      {
      if (explodeUnderStructures.Check(wptr->name))
		 {
		 bool doneAtThisLevel = false;

		 while (!doneAtThisLevel)
			{
			doneAtThisLevel = true;
			for (i=wptr->instances.size()-1; i>=0; i--)
			   {
			   const instancetype &inst = wptr->instances[i];
			   const instancetype badinst = inst;
			   fp.fprintf("//INFO: Flattening instance %s(%s) due to 'EXPLODEUNDER_' in parent's instance name or substitute file.\n",badinst.name,badinst.wptr->name);
			   wptr->instances.RemoveFast(i);
			   wptr->FlattenInstance(badinst);
			   doneAtThisLevel = false;
			   }
			}
		 }
	  else{
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
            fp.fprintf("//INFO: Deleting instance %s(%s) due to 'DELETEME_' in instance name or substitute file.\n",badinst.name,badinst.wptr->name);
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

// This function looks for a 2:1 ctran mux with the inverter in the cell.
// I'll transform it such that it's no longer a ccr
// this is recursive function going through all levels of hierarchy
void wirelisttype::CtranCamTransform(hashtype <const void *, bool> &beenHere, filetype &fp)
   {
   TAtype ta("wirelisttype::CtranCamTransform()");
   int i, k;   

   for (i=0; i<instances.size(); i++)
      {
      const instancetype &inst = instances[i];
      bool dummy;
      if (!beenHere.Check(inst.wptr, dummy))
         {
         beenHere.Add(inst.wptr, true);
         inst.wptr->CtranCamTransform(beenHere, fp);
         }
      }
   for (i=0; i<nodes.size(); i++)
      {
      nodes[i].sds.clear();
      nodes[i].gates.clear();
      }
   for (i=0; i<devices.size(); i++)
	  {
	  devicetype &d = devices[i];

	  if (d.source > d.drain)
		 SWAP(d.source, d.drain);

	  nodes[d.source].sds.push(&d);
	  nodes[d.drain].sds.push(&d);
	  nodes[d.gate].gates.push(&d);
	  }


   for (i=VDDO+1; i<nodes.size(); i++)
	  {
	  nodetype &n = nodes[i];

	  if (n.sds.size()==4) // this is a potential ctran output node
		 {
		 int left=-1, right=-1;
		 devicetype *n1=NULL, *n2=NULL, *p1=NULL, *p2=NULL;
		 bool found=true;

		 for (k=0; k<4; k++)
			{
		    if (n.sds[k]->type==D_NMOS && n1==NULL)
			   n1 = n.sds[k];
			else if (n.sds[k]->type==D_NMOS)
			   n2 = n.sds[k];
		    if (n.sds[k]->type==D_PMOS && p1==NULL)
			   p1 = n.sds[k];
			else if (n.sds[k]->type==D_PMOS)
			   p2 = n.sds[k];
			}
		 found = found && n1!=NULL && n2!=NULL && p1!=NULL && p2!=NULL;
		 if (found && n1->gate==p1->gate)
			SWAP(p1, p2); 
		 found = found && p1->gate==n2->gate && p2->gate==n1->gate;
		 found = found && n1->source==p1->source && n1->drain==p1->drain;  // this counts on source/drain being sorted
		 found = found && n2->source==p2->source && n2->drain==p2->drain;  // this counts on source/drain being sorted

		 if (found) // now just verify that n1->gate,p1->gate are connected by inverter
			{
			bool nfound1=false, pfound1=false;
			bool nfound2=false, pfound2=false;
			const nodetype &gn1 = nodes[n1->gate];
			const nodetype &gn2 = nodes[n2->gate];
			for (k=0; k<gn1.gates.size(); k++)
			   {
			   devicetype &d = *gn1.gates[k];
			   if (d.source==VSS && d.type==D_NMOS && d.drain==n2->gate)
				  nfound1 = true;
			   if (d.source==VDD && d.type==D_PMOS && d.drain==n2->gate)
				  pfound1 = true;
			   }
			for (k=0; k<gn2.gates.size(); k++)
			   {
			   devicetype &d = *gn2.gates[k];
			   if (d.source==VSS && d.type==D_NMOS && d.drain==n1->gate)
				  nfound2 = true;
			   if (d.source==VDD && d.type==D_PMOS && d.drain==n1->gate)
				  pfound2 = true;
			   }
			if ((nfound1 && pfound1) || (nfound2 && pfound2))
			   ;
			else
			   found = false;
			}
		 if (found)
			{ // I want to add 4 devices to isolate the ctran, this will untangle things
			devicetype d;
			const char *name1, *name2;
			char buffer[256];
			int net1 = n1->source==i ? n1->drain : n1->source;
			int net2 = n2->source==i ? n2->drain : n2->source;

			name1 = stringPool.sprintf("%s$%s$CtranTransform",nodes[net1].VNameNoBracket(buffer),n1->name);
			nodes.push(nodetype(name1));
			name2 = stringPool.sprintf("%s$%s$CtranTransform",nodes[net2].VNameNoBracket(buffer),n2->name);
			nodes.push(nodetype(name2));

			if (n1->source==i)
			   n1->drain = nodes.size()-2;
			else
			   n1->source = nodes.size()-2;
			if (p1->source==i)
			   p1->drain = nodes.size()-2;
			else
			   p1->source = nodes.size()-2;
			if (n2->source==i)
			   n2->drain = nodes.size()-1;
			else
			   n2->source = nodes.size()-1;
			if (p2->source==i)
			   p2->drain = nodes.size()-1;
			else
			   p2->source = nodes.size()-1;
			
			d.name = "CtransIsolationdevice";
			d.length = d.width = d.r = 1.0; 
			d.type = D_NMOS;
			d.source = VDD;
			d.gate   = net1;
			d.drain  = nodes.size()-2;
			devices.push(d);
			d.type = D_PMOS;
			d.source = VSS;
			d.gate   = net1;
			d.drain  = nodes.size()-2;
			devices.push(d);

			d.type = D_NMOS;
			d.source = VDD;
			d.gate   = net2;
			d.drain  = nodes.size()-1;
			devices.push(d);
			d.type = D_PMOS;
			d.source = VSS;
			d.gate   = net2;
			d.drain  = nodes.size()-1;
			devices.push(d);
			}
		 }
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
                  printf("I have an instance(%s) of a structure(%s) that isn't defined at line %d.\n", inst.name, chptr, line);
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
               printf("Ignoring instance %s of cell %s\n",inst.name, inst.wptr->name);
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
			   printf("Ignoring bipolar device %s of cell %s\n",inst.name, inst.wptr->name);
			   length = width = -1.0;
			   inst.name = NULL;
			   lastkeyword = KW_NULL; // this cause us to ignore closing brace if end of cell definition
			   break;
			   }
			if (stricmp(inst.wptr->name , "DB")==0)
			   {
			   printf("Ignoring diode %s of cell %s\n",inst.name, inst.wptr->name);
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
                     printf("Ignoring instance %s of cell %s\n",inst.name, inst.wptr->name);
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






