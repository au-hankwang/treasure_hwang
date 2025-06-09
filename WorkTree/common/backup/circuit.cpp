#include "pch.h"
#include "template.h"
#include "circuit.h"
#include "wirelist.h"

#ifdef _MSC_VER
//  #pragma optimize("",off)
#endif

void circuittype::Propogate(int currentTime, bool ignoreCrowbars, bool ignoreWarnings)
   {
   TAtype ta("circuittype::Propogate()");
   int time;            
   int i,k,n,cn;
   enum valuetype value;
   bool drive0, drive1, on, delaythisnode;
   int s_mem[128], d_mem[128], c_mem[128];

/*   static count;
   count++;
   if (count>1952)
      printf("count =%d\n",count);
*/

   array <int> statenodes(s_mem, sizeof(s_mem));
   array <int> dummy(d_mem, sizeof(d_mem));
   array <int> crow_bar(c_mem, sizeof(c_mem));
   hashtype <int, bool> delayed_hash;
   int oldtime=0;
   
   crow_bar.clear();
   while (nodeheap.pop(n, time))
      {
//      if (n==3647 || n==10444 || n==1060 || n==210646)
//         printf("");
//      if (count>1952)
//         printf("count =%d n=%d time=%d\n",count,n,time);
      if (time != oldtime)
         IntermediateNodeDumping(time);
      oldtime = time;
      if (time >= 500) 
         {       
         ErrorLog("\nInfinite loop @%dns at node ->%-30s\n", currentTime, nodes[n].name);
         do{
            nodes[n].value    = V_ILLEGAL;
            nodes[n].inflight = false;
            }while (nodeheap.pop(n, time));
         return;
         }                                                                                  

      
      nodes[n].inflight=false;
      cn=n;                                                       
      drive0=drive1=false;
      delaythisnode = false;

      Eval(cn, drive0, drive1, statenodes);
      // I now positively know the state of this node. I also know the state of every node
      // touching this one through an ON device. Unless the node is
      // ratioed or metastable!
      if (nodes[n].constant)
         {
         if (nodes[n].value == V_ONE) 
            {drive1=true; drive0=false;}
         else if (nodes[n].value == V_ZERO) 
            {drive0=true; drive1=false;}
         else FATAL_ERROR;
         }
      
      if (drive0 && drive1)
         {
		 bool foo;
		 if (!delayed_hash.Check(n, foo))
			{
            delaythisnode = !nodes[n].ratioed;
            if (delaythisnode){
               nodeheap.push(n, time+2);  // let's hope that the short goes away so we can prevent doing alot of expensive analysis
			   }
			delayed_hash.Add(n, true);
			}
         SetAsRatioed(n);
         }
      else 
         nodes[n].ratioed = false;
      
      if (drive0 && !drive1 && statenodes.size()==2)
         {  // I'm looking for cross-coupled P's where both nodes are pulled low
         TAtype ta("Metastable check");
         bool d0_1=false, d1_1=false, d0_2=false, d1_2=false;
         // This happens in full-swing sense amps with a leaker device
         Eval(statenodes[0], d0_1, d1_1, dummy);
         Eval(statenodes[1], d0_2, d1_2, dummy);

         if (d0_1 && d0_2 && !d1_1 && !d1_2)
            {
            cn=statenodes[1];
            for (k=0; k<nodes[cn].gates.size(); k++)
               {
               devicetype &d = *nodes[cn].gates[k];
               if (d.type == D_PMOS)
                  if ((d.source==statenodes[0] && d.drain  ==VDD) ||
                     (d.drain ==statenodes[0] && d.source ==VDD))
                     {
                     cn=statenodes[0];
                     for (k=0; k<nodes[cn].gates.size(); k++)
                        {
                        devicetype &d2 = *nodes[cn].gates[k];
                        if (d2.type==D_PMOS)
                           if ((d2.source==statenodes[1] && d2.drain ==VDD) ||
                              (d2.drain ==statenodes[1] && d2.source==VDD))
                              {// I found a metastable condition. 
                              float res0,res1;
                              
                              res0=QuickStrength(statenodes[0], VSS);
                              res1=QuickStrength(statenodes[1], VSS);
                              if (res0==res1 && !ignoreCrowbars)
                                 ErrorLog("\nI found a metastable condition @%dns on nodes %s and %s.", currentTime, nodes[statenodes[0]].name,nodes[statenodes[1]].name);
                              if (res0<res1)
                                 statenodes.pop_back(); // don't discharge the weakly driven node
                              else
                                 {
                                 statenodes[0] = statenodes[1];
                                 statenodes.pop_back();
                                 }   
                              break;
                              }
                        }     
                     break;  
                     }
               }
            }
         }
      
      if (!drive0 && !drive1)
         {
         crow_bar += statenodes;
         }
      
      if ((drive0 || drive1) && !delaythisnode)
         {  
         value = nodes[n].value;
         if (drive0) value = V_ZERO;
         if (drive1) value = V_ONE;
         for (i=0; i<statenodes.size(); i++)
            {
            cn=statenodes[i];
            if (nodes[cn].ratioed && nodes[cn].state)
               {
               Eval(cn, drive0, drive1, statenodes);
//               if (count>1952 && cn==712)
//                  CrowbarHelpEval(cn);
                  
               if (drive0 && drive1)
                  {
                  float res0,res1;
                  
                  res0=QuickStrength(cn, VSS);
                  res1=QuickStrength(cn, VDD); 
                  
                  if (res0<res1)
                     value = V_ZERO;
                  else
                     value = V_ONE;
//                  if (res0/res1<2.95 && res1/res0<2.95)
                     crow_bar.push(cn);
                  }
               else if (drive1)
                  value = V_ONE;
               else if (drive0)
                  value = V_ZERO;
               else
                  FATAL_ERROR;
               }
            if ((nodes[cn].value!=value || nodes[cn].changed) && (nodes[cn].value == value || !nodes[cn].constant))
               { // state changed so propogate   
               if (nodes[cn].outputSigChangedPtr != NULL)
                  *nodes[cn].outputSigChangedPtr = true;
               
               if (nodes[cn].value != value)  
                  nodes[cn].toggleCount++;
               nodes[cn].value=value;
               nodes[cn].changed=false;
               for (k=0; k<nodes[cn].gates.size(); k++)
                  {
                  devicetype &d = *nodes[cn].gates[k];
                  on = (d.type==D_PMOS && value==V_ZERO) || (d.type==D_NMOS && value==V_ONE);
                  d.on = on;
                  if (on)  // if device is on then only push source or drain not both
                     {     // VSS-VDD are permanently inflight so they never get pushed
                     if (d.source>VDDO)
                        {
                        if (!nodes[d.source].inflight && 
                           (nodes[d.source].value != nodes[d.drain].value ||
                           !nodes[d.source].state || !nodes[d.drain].state))
                           {
                           nodeheap.push(d.source, time+1);
                           nodes[d.source].inflight=true;
                           }
                        }
                     else if (d.drain>VDDO) 
                        if (!nodes[d.drain].inflight &&     
                           (nodes[d.source].value != nodes[d.drain].value ||
                           !nodes[d.source].state || !nodes[d.drain].state))
                           {    
                           nodeheap.push(d.drain, time+1);
                           nodes[d.drain].inflight=true;
                           }
                     }                           
                  else    // if device is off then push source and drain
                     {        
                     if (d.source>VDDO && nodes[d.source].ratioed)     
                        if (!nodes[d.source].inflight)
                           {
                           nodeheap.push(d.source, time+1);
                           nodes[d.source].inflight=true;
                           }
                     if (d.drain>VDDO && nodes[d.drain].ratioed) 
                        if (!nodes[d.drain].inflight)
                           {
                           nodeheap.push(d.drain, time+1);
                           nodes[d.drain].inflight=true;
                           }
                     }   
                  }
               }
            }   
         }         
      }                  

   if (crow_bar.size())
      {
      TAtype ta("crow bar checks");
      crow_bar.RemoveDuplicates();
      while (crow_bar.pop(n))
         {
         bool drive0=false, drive1=false;
         Eval(n, drive0, drive1, statenodes); // check to see if node is even ratioed still
         if (drive0 && drive1)
            {
            if (!ignoreCrowbars)
               {
               if (!ignoreWarnings)
                  {
                  float res0,res1;
                  
                  res0 = QuickStrength(n, VSS);
                  res1 = QuickStrength(n, VDD);
/*                  if (res0/res1 <2.95 && res1/res0 <2.95)
                     {               
                     ErrorLog("\nThe ratioed logic at node -> %s didn't resolve solidly.",nodes[n].name);
                     Log("\nThe effective W/L drive to 0 is -> %.1f",1.0/res0);
                     Log("\nThe effective W/L drive to 1 is -> %.1f",1.0/res1);
                     Log("\nThis doesn't meet the design guide requirements of a Beta 1:2");
                     nodes[n].value = V_ILLEGAL;
                     }*/

                  char buf2[128];
                  ErrorLog("\nNode %s is ratioed, W/L vss->%.2f  W/L vdd->%.2f. I will print all the ON devices:",nodes[n].RName(buf2),1.0/res0,1.0/res1);
                  CrowbarHelpEval(n);
                  }
               }
            if (n==3647 || n==10444 || n==1060 || n==210646)
               printf("");
//            else
            nodeheap.push(n, 0); // make sure these errors get reevaluated next time slice
            nodes[n].value = V_ILLEGAL;
            }
         else if (!drive0 && !drive1 && !nodes[n].constant)
            {
            if (!ignoreCrowbars){
               char buf2[128];
               ErrorLog("\nNode %s @%dns is floating.",nodes[n].RName(buf2), currentTime);
               }
//            else
//               nodeheap.push(n, 0); // make sure these errors get reevaluated next time slice
            nodes[n].value = V_Z;
            }
         }
      }
   IntermediateNodeDumping(time);

   if (false)   // only needed for debugging   
      {
      TAtype ta("Checking nodes for inflightness");
      for (i=0; i<nodes.size(); i++)                            
         if (nodes[i].inflight)
            {
            ErrorLog("\n%s was still in flight after propogating!",nodes[i].name);
            nodes[i].inflight=false;
            }
      }
   }

bool circuittype::CheckForRelevance(const array <int> &checknodes)
   {
   TAtype ta("circuittype::CheckForRelevance()");
   int i, k, cn;
   array <int> s;
   array <valuetype> restorevalues(nodes.size());
   bool XonOutput=false;


   for (i=VDDO+1; i<nodes.size(); i++)
	  {
	  nodetype &n = nodes[i];

	  n.inflight = false;
	  restorevalues[i] = n.value;
	  }
   
   s += checknodes;

   while (s.pop(cn))
	  {
	  nodetype &n = nodes[cn];
	  array <int> statenodes;
	  bool drive0 = false, drive1 = false;

	  n.inflight = true;
	  Eval(cn, drive0, drive1, statenodes);

	  if (drive0==drive1)  // I have an X condition
		 {
		 for (i=0; i<statenodes.size(); i++)
			{
			cn = statenodes[i];
			if (nodes[cn].output) XonOutput = true;

			for (k=0; k<nodes[cn].gates.size(); k++)
			   {
			   devicetype &d = *nodes[cn].gates[k];
			   d.on = true;		 // make all gates on an X node be on

			   if (d.source>VDDO)
				  {
				  if (!nodes[d.source].inflight && 
					 (!nodes[d.source].state || !nodes[d.drain].state))
					 {
					 s.push(d.source);
					 nodes[d.source].inflight=true;
					 }
				  }
			   else if (d.drain>VDDO) 
				  if (!nodes[d.drain].inflight &&     
					 (!nodes[d.source].state || !nodes[d.drain].state))
					 {    
					 s.push(d.drain);
					 nodes[d.drain].inflight=true;
					 }			   
			   }
			}		 
		 }
	  }

   // now restore everything to the way we found it
   for (i=VDDO+1; i<nodes.size(); i++)
	  {
	  nodetype &n = nodes[i];

	  n.inflight = false;
	  n.value = restorevalues[i];

	  for (k=0; k<n.gates.size(); k++)
		 {
		 devicetype &d = *n.gates[k];

		 d.on = (d.type == D_NMOS && n.value==V_ONE) || (d.type == D_PMOS && n.value==V_ZERO);
		 }
	  }

   return XonOutput;
   }


void circuittype::AdjustDevicesOnThisNode(int n, int time)
   {
   int k;
   valuetype value = nodes[n].value;
   for (k=0; k<nodes[n].gates.size(); k++)
      {
      devicetype &d = *nodes[n].gates[k];
      bool on = (d.type==D_PMOS && value==V_ZERO) || (d.type==D_NMOS && value==V_ONE);
      d.on = on;
      if (on)  // if device is on then only push source or drain not both
         {     // VSS-VDD are permanently inflight so they never get pushed
         if (d.source>=2)     
            {
            if (!nodes[d.source].inflight && 
               (nodes[d.source].value != nodes[d.drain].value ||
               !nodes[d.source].state || !nodes[d.drain].state))
               {
               nodeheap.push(d.source, time+1);
               nodes[d.source].inflight=true;
               }
            }
         else if (d.drain>=2) 
            if (!nodes[d.drain].inflight &&     
               (nodes[d.source].value != nodes[d.drain].value ||
               !nodes[d.source].state || !nodes[d.drain].state))
               {    
               nodeheap.push(d.drain, time+1);
               nodes[d.drain].inflight=true;
               }
         }                           
      else    // if device is off then push source and drain
         {        
         if (d.source>=2 && nodes[d.source].ratioed)     
            if (!nodes[d.source].inflight)
               {
               nodeheap.push(d.source, time+1);
               nodes[d.source].inflight=true;
               }
            if (d.drain>=2 && nodes[d.drain].ratioed) 
               if (!nodes[d.drain].inflight)
                  {
                  nodeheap.push(d.drain, time+1);
                  nodes[d.drain].inflight=true;
                  }
         }   
      }
   }


void circuittype::Initialize(bool one)
   {
   TAtype ta("Initializing nodes");
   int i;
   int nmos=0, pmos=0, fuse=0;
   array <bool> driveninputs(nodes.size(), false);

   ZeroNodes();
   for (i=0; i<inputs.size(); i++)
      {
      driveninputs[inputs[i]] = true;
      nodes[inputs[i]].state = true;
      nodes[inputs[i]].constant = true;  // this will suppress the floating node warning
      }
   for (i=0; i<outputs.size(); i++)
      nodes[outputs[i]].state = true;
   
   for (i=VDD+1; i<nodes.size(); i++)
      {
      nodes[i].state = nodes[i].state || (nodes[i].gates.size() != 0);
      nodes[i].value = one ? V_ONE : V_ZERO;
      if (strlen(nodes[i].name)>3 && stricmp(nodes[i].name+strlen(nodes[i].name)-2, "_l")==0)
         nodes[i].value = one ? V_ZERO : V_ONE;

      if (nodes[i].state)
         {
         nodes[i].inflight = true;
         nodeheap.push(i, 0);
         }
      if (nodes[i].sds.size()==0 && nodes[i].gates.size()>0 && !driveninputs[i] && i!=VDD && i!=VSS)
         ErrorLog("\n>%s is a dangling input.",nodes[i].name);         
      }
   
   nodes[VSS].value = V_ZERO;
   nodes[VDD].value = V_ONE;
   
   for (i=0; i<devices.size(); i++)
      {
      if (devices[i].type==D_FUSE)
         {
         devices[i].on = true;
         fuse++;
         }      
      if (devices[i].type==D_NMOS)
         {
         devices[i].on = nodes[devices[i].gate].value == V_ONE;
         nmos++;             
         }
      if (devices[i].type==D_PMOS)
         {
         devices[i].on = nodes[devices[i].gate].value == V_ZERO;
         pmos++;
         }
      }   
   
   nodeheap.push(VSS, 0);
   nodeheap.push(VDD, 0);
   Log("\n%d nodes were found.",nodes.size());
   Log("\n%d NMOS, %d PMOS, %d FUSE",nmos,pmos,fuse); 
   Log("\nInitializing nodes");
   Log("-> All=0...");   
   Propogate(-1, true);
   for (i=0; i<nodes.size(); i++)
      nodes[i].toggleCount = 0;
   }  


// I will try to detect leaker devices and not count these as ratioing.
void circuittype::Eval(int cn, bool &drive0, bool &drive1, array <int> &statenodes)
   {
//   TAtype ta("circuittype::Eval()");
   int i;
   int mem[128];
   array <int> s(mem, sizeof(mem));
     
   statenodes.clear();
   
   if (cn==VSS) {drive0=true; return;}
   if (cn==VDD) {drive1=true; return;}
   
   if (touchcount==0)
      ReSync();
   touchcount++;        

   nodes[cn].touched=touchcount;   
   do {     
      if (nodes[cn].state)
         statenodes.push(cn);
      for (i=0; i<nodes[cn].sds.size(); i++)
         {
         const devicetype &d = *nodes[cn].sds[i];      
         if (d.on)
            {
            if (d.source==VSS)      drive0=true;
            else if (d.source==VDD) drive1=true;
            else if (nodes[d.source].touched != touchcount)
               {
               nodes[d.source].touched=touchcount;
               s.push(d.source);
               }
            if (d.drain==VSS)      drive0=true;
            else if (d.drain==VDD) drive1=true;
            else if (nodes[d.drain].touched != touchcount)
               {
               nodes[d.drain].touched=touchcount;
               s.push(d.drain);
               }
            }
         }
      }while (s.pop(cn));
   }



void circuittype::CrowbarHelpEval(int cn)
   {
   TAtype ta("circuittype::CrowbarHelpEval()");
   int i;
   int mem[128];
   array <int> s(mem, sizeof(mem));
     
   if (cn==VSS) return;
   if (cn==VDD) return;
   
   if (touchcount==0)
      ReSync();
   touchcount++;        

   nodes[cn].touched=touchcount;   
   do {     
      for (i=0; i<nodes[cn].sds.size(); i++)
         {
         const devicetype &d = *nodes[cn].sds[i];      
         if (d.on)
            {
            ErrorLog("\n\t%s",d.name);
            if (d.source==VSS)      ;
            else if (d.source==VDD) ;
            else if (nodes[d.source].touched != touchcount)
               {
               nodes[d.source].touched=touchcount;
               s.push(d.source);
               }
            if (d.drain==VSS)      ;
            else if (d.drain==VDD) ;
            else if (nodes[d.drain].touched != touchcount)
               {
               nodes[d.drain].touched=touchcount;
               s.push(d.drain);
               }
            }
         }
      }while (s.pop(cn));
   }


void circuittype::DebugEval(int cn)
   {
   int i;
   array<int> s;
   const devicetype *d;
   listtype<const devicetype *> dev_list;
      
   printf("\n");
   if (touchcount==0)
      ReSync();
   touchcount++;        
            
   nodes[cn].touched=touchcount;   
   do {     
      for (i=0; i<nodes[cn].sds.size(); i++)
         {
         d = nodes[cn].sds[i];      
         if (d->on)
            {
            dev_list.push(d);
            if (d->source==VSS)      ;
            else if (d->source==VDD) ;
            else if (d->source!=cn && nodes[d->source].touched != touchcount)
               {
               nodes[d->source].touched=touchcount;
               s.push(d->source);
               }
            if (d->drain==VSS)       ;
            else if (d->drain==VDD)  ;
            else if (d->drain!=cn && nodes[d->drain].touched != touchcount)
               {
               nodes[d->drain].touched=touchcount;
               s.push(d->drain);
               }
            }
         }
      }while (s.pop(cn));
   dev_list.sort();
   const devicetype *last = NULL;
   while (dev_list.pop(d))
      {
      if (last != d)  // this removes duplicates
         printf(" %s", d->name);
      last = d;
      }   
   }



void circuittype::SetAsRatioed(int cn)
   {
//   TAtype ta("circuittype::SetAsRatioed()");
   int i;
   int mem[128];
   array <int> s(mem, sizeof(mem));
      
   if (cn==VSS) return;
   if (cn==VDD) return;
   
   if (touchcount==0)
      ReSync();
   touchcount++;        
   nodes[cn].touched=touchcount;   
   do {     
      nodes[cn].ratioed=true;
      for (i=0; i<nodes[cn].sds.size(); i++)
         {
         const devicetype &d = *nodes[cn].sds[i];
         if (d.on)
            {
            if (d.source==VSS || d.source==VDD)      
               ;
            else if (d.source!=cn && nodes[d.source].touched != touchcount)
               {
               nodes[d.source].touched=touchcount;
               s.push(d.source);
               }
            if (d.drain==VSS || d.drain==VDD)
               ;
            else if (d.drain!=cn && nodes[d.drain].touched != touchcount)
               {
               nodes[d.drain].touched=touchcount;
               s.push(d.drain);
               }
            }
         }
      }while (s.pop(cn));
   }

struct connectionheaptype{
   int n;
   int key;
   bool operator<(const connectionheaptype &right) const
      {
      return key < right.key;
      }
   };

float circuittype::QuickStrength(int n, int power)
   {
//   TAtype ta("circuittype::QuickStrength");

   const int msize=512;
   int s1_mem[256], s2_mem[256];   // these will eliminate so malloc() calls speeding things up especially of MP
   connectionheaptype h_mem[256];

// it's much more efficient to allocate stuff on the stack if you know ahead of time how large it can be   
   array <int> s1(s1_mem, sizeof(s1_mem)), s2(s2_mem, sizeof(s2_mem));
   heaptype<connectionheaptype> h(h_mem, sizeof(h_mem));
   connectionheaptype con;
   float rmatrix[msize][msize];
   
   int i,k,numc;
   const float _INFINITY = 0.0;
   
   if (n==VSS || n==VDD) FATAL_ERROR;   
   if (touchcount==0)
      ReSync();
   touchcount++;
         
   nodes[n].touched=touchcount;
   s1.push(n);
   s2.push(n);
        
   // 0=power, 1=node under test
   numc=1;
   while (s1.pop(n))
      {
      int numr=0;
      nodes[n].overlay=numc;
      for (i=0; i<nodes[n].sds.size(); i++)
         {
         const devicetype &d = *nodes[n].sds[i];
         if (d.on)
            {
            if (d.source==power || d.drain==power) numr++;
            else if (d.source!=VSS && d.source!=VDD && d.drain!=VSS  && d.drain!=VDD) 
               {
               if (nodes[d.source].touched!=touchcount)
                  {
                  nodes[d.source].touched=touchcount;
                  s2.push(d.source); // I'll just push things for pass two
                  s1.push(d.source);
                  }
               if (d.source != n)
                  numr++;
               if (nodes[d.drain].touched != touchcount)
                  {
                  nodes[d.drain].touched=touchcount;
                  s2.push(d.drain); // I'll just push things for pass two
                  s1.push(d.drain);
                  }
               if (d.drain != n)
                  numr++;
               if (d.source!=n && d.drain!=n) FATAL_ERROR;
               }
            }
         }
      if (nodes[n].overlay<=0) FATAL_ERROR;
      if (numr>1 || numc==1)  // must have more than one connection or be the main node under test(test point is explicit connection)
         numc++;
      else nodes[n].overlay=0;      
      }

   if (numc<2) FATAL_ERROR;
   if (numc>=msize)
      {
      printf("\nYou have too many nodes for my ratio solver to handle.");
      FATAL_ERROR;
      }

   h.clear();
   for (i=0; i<numc; i++)
      for (k=0; k<numc; k++)
         rmatrix[i][k]=_INFINITY; // resistor matrix
  

   while (s2.pop(n))
      {
      int me = nodes[n].overlay;
      if (me >= numc || me<0) FATAL_ERROR;

// me can be zero if this node has only one connection. It got pruned above and the flag was .overlay=0
      if (me != 0)
         {
         for (i=0; i<nodes[n].sds.size(); i++)
            {
            devicetype &d = *nodes[n].sds[i];
            if (d.on)
               {
               if (d.source==power || d.drain==power)
                  {
                  if (rmatrix[me][0]==_INFINITY)
                     rmatrix[0][me]=rmatrix[me][0] = d.r;
                  else                             // resistors are in parallel so merge              
                     rmatrix[0][me]=rmatrix[me][0] = 1.0/(1.0 / rmatrix[me][0]+1.0 / d.r);
                  }
               else if (d.source!=VSS && d.source!=VDD && d.source!=n) 
                  { 
                  int you = nodes[d.source].overlay;
                  
                  // you can be zero if this node has only one connection. It got pruned above and the flag was .overlay=0
                  if (you!=0)
                     {
                     if (you==me) FATAL_ERROR;
                     if (rmatrix[me][you]==_INFINITY)
                        rmatrix[you][me]=rmatrix[me][you] = d.r;
                     else                          // resistors are in parallel so merge              
                        rmatrix[you][me]=rmatrix[me][you] = 1.0/(1.0/rmatrix[me][you]+1.0 / d.r);             
                     }
                  }
               }
            }
         }
      }
//  I now have a matrix showing resistances from everything to everything. I
//  now need to reduce this to 2 nodes and 1 resistor

   for (i=2; i<numc; i++)
      {
      int count;

      for (k=0,count=0; k<numc; k++)
         if (rmatrix[i][k]!=_INFINITY) 
            count++;

      con.key=count;
      con.n=i;
      h.push(con);
      }

#ifdef DEBUG
   for (i=0; i<numc; i++)
      {
      for (k=0; k<numc; k++)
         if (rmatrix[i][k] != rmatrix[k][i]) FATAL_ERROR;
      if (rmatrix[i][i]!=_INFINITY) FATAL_ERROR;
      }
#endif
 
   // get nodes with the smallest number of connections first
   // and systematicly delete nodes until only 0,1 left
   // things will work whether you start with nodes with the
   // smallest or largest connections. Starting with small
   // connections is both faster and much more accurate.
   // In many case exact. (delta-wye configuaration won't be)
   while (h.pop(con))
      {
      float newr;
      n=con.n;
      for (i=0; i<numc-1; i++)
         if (rmatrix[n][i]!=_INFINITY)
            {
            for (k=i+1; k<numc; k++)
               if (rmatrix[n][k]!=_INFINITY)
                  {
                  newr = rmatrix[n][i] + rmatrix[n][k];

                  if (rmatrix[i][k] == _INFINITY)
                     rmatrix[k][i]=rmatrix[i][k] = newr;
                  else
                     rmatrix[k][i]=rmatrix[i][k] = 1.0/(1.0/rmatrix[i][k]+1.0/newr);
                  rmatrix[n][k] = rmatrix[k][n] = _INFINITY; // zero out this node it's empty
                  }         
            rmatrix[n][i] = rmatrix[i][n] = _INFINITY; // zero out this node it's empty
            }      
      }

#ifdef DEBUG
// debugging purposes only
   for (i=0; i<numc; i++)
      for (k=0; k<numc; k++)
         if (i>2 || k>2)
            if (rmatrix[i][k] != _INFINITY) FATAL_ERROR;
#endif

   if (rmatrix[0][1] != rmatrix[1][0]) FATAL_ERROR;
   if (rmatrix[0][1]==_INFINITY)
      return((float)1.0e0);   // return a huge resistance
   return(rmatrix[0][1]);   
   }                                                                          

void circuittype::ZeroNodes()
   {
   TAtype ta("circuittype::ZeroNodes()");
   int i;
   for (i=0; i<nodes.size(); i++)
      {
      nodes[i].value   = V_ZERO;
      nodes[i].changed =false;
      nodes[i].inflight=false;
      nodes[i].ratioed =false;
      nodes[i].constant=false;
      nodes[i].state   =false;
      nodes[i].touched =0;
      }
   touchcount = 1;
   }


void circuittype::ReSync()
   {
   TAtype ta("circuittype::ReSync()");
   int i;
   for (i=0; i<nodes.size(); i++)
      nodes[i].touched = 0;
   touchcount = 1;
   }


// I want to merge findered devices into one big device
void circuittype::MergeDevices()
   {
   TAtype ta("circuittype::MergeDevices()");
   int i, nn;   
   
   // for now just do the simple cases of multiple devices with the same src/gate/drain connections
   for (nn=0; nn<nodes.size(); nn++)
      {
      nodetype &n = nodes[nn];

      for (i=0; i<n.gates.size(); i++)
         {
         devicetype *d1 = n.gates[i];
         int k;
         for (k=n.gates.size()-1; k>i; k--)
            {
            devicetype *d2 = n.gates[k];
            if (d1->source == d2->source && d1->drain == d2->drain && d1->length == d2->length)
               {
               d1->width += d2->width;
               d2->width  = 0.0;  // effectively delete device
               d2->length = 0.0;
               }               
            }
         }
      }
   // I leave the device in the list but I'll delete it from my cross reference
   for (nn=0; nn<nodes.size(); nn++)
      {
      nodetype &n = nodes[nn];

      for (i=n.gates.size()-1; i>=0; i--)
         {
         if (n.gates[i]->width == 0.0)
            n.gates.RemoveFast(i);
         }
      for (i=n.sds.size()-1; i>=0; i--)
         {
         if (n.sds[i]->width == 0.0)
            n.sds.RemoveFast(i);
         }
      }
   }


// this will propogate the activity field downstream following inverters or nand2 gates.
// It won't go through other gate types
// this function is recursive
void circuittype::PropogateActivity(int node_index)
   {
   nodetype &n = nodes[node_index];
   float currentActivity = n.activity;
   int i;
   array<int> pds, pus, inverters;

   for (i=0; i<n.gates.size(); i++)
	  {
	  const devicetype &d = *n.gates[i];

	  if (d.type==D_NMOS && d.drain==VSS)
		 pds.push(d.source);
	  if (d.type==D_NMOS && d.source==VSS)
		 pds.push(d.drain);
	  if (d.type==D_PMOS && d.drain==VDD)
		 pus.push(d.source);
	  if (d.type==D_PMOS && d.source==VDD)
		 pus.push(d.drain);
	  }
   pds.RemoveDuplicates();
   pus.RemoveDuplicates();
   pds.CommonContents(inverters, pus);
   for (i=0; i<inverters.size(); i++)
	  {
	  if (nodes[inverters[i]].activity!=currentActivity)
		 {
		 nodes[inverters[i]].activity = currentActivity;
		 PropogateActivity(inverters[i]);
		 }
	  }
   pus.RemoveCommonElements(inverters);
   // pus now contains a list of output nodes that aren't being driven by inverts so could be driven by nands
   for (i=0; i<pus.size(); i++)
	  {
	  nodetype &n2 = nodes[pus[i]];
	  int p0_gate=-1, p1_gate=-1, n0_gate=-1, n1_gate=-1, n0=-1;
	  if (n2.sds.size()==3 && n2.activity!=currentActivity)
		 {
		 devicetype *d = n2.sds[0];
		 if (d->type==D_PMOS && (d->source==VDD || d->drain==VDD))
			p0_gate = d->gate;
		 if (d->type==D_NMOS && (d->source==pus[i] || d->drain==pus[i]))
			n0_gate = d->gate;
		 if (d->type==D_NMOS && d->source==pus[i])
			n0 = d->drain;
		 if (d->type==D_NMOS && d->drain==pus[i])
			n0 = d->source;

		 d = n2.sds[1];
		 if (d->type==D_PMOS && (d->source==VDD || d->drain==VDD)) {
			if (p0_gate<0)
			   p0_gate = d->gate;
			else									  
			   p1_gate = d->gate;
			}
		 if (d->type==D_NMOS && (d->source==pus[i] || d->drain==pus[i]))
			n0_gate = d->gate;
		 if (d->type==D_NMOS && d->source==pus[i])
			n0 = d->drain;
		 if (d->type==D_NMOS && d->drain==pus[i])
			n0 = d->source;
		 
		 d = n2.sds[2];
		 if (d->type==D_PMOS && (d->source==VDD || d->drain==VDD)) {
			if (p0_gate<0)
			   p0_gate = d->gate;
			else
			   p1_gate = d->gate;
			}
		 if (d->type==D_NMOS && (d->source==pus[i] || d->drain==pus[i]))
			n0_gate = d->gate;
		 if (d->type==D_NMOS && d->source==pus[i])
			n0 = d->drain;
		 if (d->type==D_NMOS && d->drain==pus[i])
			n0 = d->source;

		 if (p0_gate>0 && p1_gate>0 && (p0_gate==n0_gate || p1_gate==n0_gate) && n0>VDD && nodes[n0].sds.size()==2)
			{
			d = nodes[n0].sds[0];
			if (d->type==D_NMOS && (d->source==VSS || d->drain==VSS) && (d->gate==p0_gate || d->gate==p1_gate))
			   {
			   nodes[pus[i]].activity = currentActivity;
			   PropogateActivity(pus[i]);
			   }
			d = nodes[n0].sds[1];
			if (d->type==D_NMOS && (d->source==VSS || d->drain==VSS) && (d->gate==p0_gate || d->gate==p1_gate))
			   {
			   nodes[pus[i]].activity = currentActivity;
			   PropogateActivity(pus[i]);
			   }
			}
		 }
	  }
   }


// returns true if this node is driven solely by inverters(can be ganged however)
bool circuittype::IsOnlyInverters(int node_index)
   {
   int i;
   array<int> pds, pus, inverters;
   const nodetype &n = nodes[node_index];

   for (i=0; i<n.sds.size(); i++)
	  {
	  const devicetype &d = *n.sds[i];

	  if (d.type==D_NMOS && (d.drain==VSS || d.source==VSS)) 
		 pds.push(d.gate);
	  else if (d.type==D_PMOS && (d.drain==VDD || d.source==VDD))
		 pus.push(d.gate);
	  else return false;
	  }
   pds.RemoveDuplicates();
   pus.RemoveDuplicates();
   pds.Sort();
   pus.Sort();
   if (pds.size() != pus.size()) return false;
   for (i=0; i<pds.size(); i++)
	  if (pds[i]!=pus[i])
		 return false;
   return true;
   }

// this isn't bullet proof. 
bool circuittype::IsOnlyNand(int node_index)
   {
   int i;
   array<int> pds, pus, common;
   const nodetype &n = nodes[node_index];

   for (i=0; i<n.sds.size(); i++)
	  {
	  const devicetype &d = *n.sds[i];

	  if (d.type==D_NMOS && (d.drain!=VSS && d.source!=VSS)) 
		 pds.push(d.gate);
	  else if (d.type==D_PMOS && (d.drain==VDD || d.source==VDD))
		 pus.push(d.gate);
	  else return false;
	  }
   pds.RemoveDuplicates();
   pus.RemoveDuplicates();
   pds.Sort();
   pus.Sort();

   pds.CommonContents(common, pus);
   if (common.size() != pds.size())
	  return false;

   if (pds.size()*2 != pus.size() && pds.size()*3 != pus.size()) return false;
   for (i=0; i<pds.size(); i++)
	  if (pds[i]!=pus[i])
		 return false;
   return true;
   }

bool circuittype::IsSimpleDriver(int node_index)
   {
   if (IsOnlyInverters(node_index) || IsOnlyNand(node_index))
	  return true;

   return false;
   }


// this reads an hspice wirelist         
void circuittype::ReadSpiceWirelist(char *filename)
   {
   TAtype ta("simtype::ReadSpiceWirelist()");
   FILE *fptr;
   char buffer[1024], *name, *chptr;
   int line=0, index;    
   int fuse;
   nodetype node;
   devicetype d;
   
   memset(&node, 0, sizeof(node));
   node.bit = -1;
   if (nodes.size()==0)
      {
      node.name = "vss";
      nodes.push(node);
      h.Add(nodes.back().name, nodes.size()-1);
      h.Add("gnd", nodes.size()-1);
      node.name = "vdd";
      nodes.push(node);
      h.Add(nodes.back().name, nodes.size()-1);
      }
             
   if (strchr(filename,'.')==NULL)
      strcat(filename,".hsp");
   fptr=fopen(filename,"r");
   if (fptr==NULL)
      {printf("\nI could not open %s for reading!",filename); exit(-1);} 
   printf("\nReading %s",filename);   
   while (NULL!=fgets(buffer,sizeof(buffer)-1,fptr))
      {   
      line++;
      
      if (buffer[0]=='M')
         {
         while ((chptr=strchr(buffer,'<'))!=NULL)
            *chptr = '[';
         while ((chptr=strchr(buffer,'>'))!=NULL)
            *chptr = ']';
         chptr=strchr(buffer,' ');
         *chptr=0;
         d.name = NULL;
         
         name=chptr+1;
         chptr=strchr(name,' ');   // source name
         *chptr=0;
         if (!h.Check(name, index))
            {
            index = nodes.size();
            node.name = stringPool.NewString(name);
            nodes.push(node);
            h.Add(node.name, index);
            }
         d.source = index;         
            
         name=chptr+1;
         chptr=strchr(name,' '); // gate name
         *chptr=0;
         if (!h.Check(name, index))
            {
            index = nodes.size();
            node.name = stringPool.NewString(name);
            nodes.push(node);
            h.Add(node.name, index);
            }
         d.gate = index;

         name=chptr+1;            
         chptr=strchr(name,' '); // drain name
         *chptr=0;
         
         if (!h.Check(name, index))
            {
            index = nodes.size();
            node.name = stringPool.NewString(name);
            nodes.push(node);
            h.Add(node.name, index);
            }
         d.drain = index;

         name=chptr+1;            
         chptr=strchr(name,' '); // bulk name
         *chptr=0;
         name=chptr+1;            
         chptr=strchr(name,' '); // device type
         *chptr=0;

         fuse=false;
         if (strcmp(name,"nch")==0)
            d.type=D_NMOS; 
         else if (strcmp(name,"pch")==0)    
            d.type=D_PMOS; 
         else if (strcmp(name,"FUS")==0)     // fuse
            fuse=true; 
         else
            {printf("I don't understand the type of device %s at line %d.",d.name,line); FATAL_ERROR;}

         name=chptr+1;            
         chptr=strchr(name,' '); // channel length
         *chptr=0;
         if (strncmp(name,"l=", 2)!=0) FATAL_ERROR;
         d.r=atof(name+2);
         name=chptr+1;            
         chptr=strchr(name,' '); // channel width 
         if (strncmp(name, "w=", 2)!=0) FATAL_ERROR;
         if (atof(name+2)<=0.0)
            {
            printf("\nI see a negative/zero channel width at line %d of %s.",line,filename);
            FATAL_ERROR;
            }
         else
            d.r= d.r/atof(name+2);
         if (d.r==0.0)
            d.r = (float)1e-15;
         if (d.type==D_PMOS)
            {
            d.r*=2.0;   // pmos is aproximately twice the resistance of NMOS
            nodes[d.gate].capacitance   += d.width * PGATE_CAPACITANCE;
            nodes[d.source].capacitance += d.width * PDIFF_CAPACITANCE;
            nodes[d.drain].capacitance  += d.width * PDIFF_CAPACITANCE;
            }
         else
            {
            nodes[d.gate].capacitance   += d.width * NGATE_CAPACITANCE;
            nodes[d.source].capacitance += d.width * NDIFF_CAPACITANCE;
            nodes[d.drain].capacitance  += d.width * NDIFF_CAPACITANCE;
            }
         
         if (fuse) 
            {
            d.r/=100.0;  // Fuse dimensions are in TiN which is better conductor than channel!
            d.type=D_FUSE;
            d.gate=VSS;
            printf("\nFuse %s is permanently on.",d.name);
            }
         devices.push(d);
         }                        
      if (line%20480==0) printf(".");    
      }                           
   fclose(fptr);  
   printf(" %6d lines read!",line);
   }
                        

void circuittype::ReadCapfile(const char *filename)
   {
   TAtype ta("simtype::ReadCapfile");
   char buffer[2048];
   int line = 0;
   int i, num=0, mismatches=0;
   double fixed = 0.0, gate = 0.0;
   int index=-1;
   filetype fp;
   bool end_seen=false;
   char filename2[256];

   fp.fopen(filename,"rt");
   if (!fp.IsOpen())
      {
      printf("\nI couldn't open %s for reading.\nNo capacitance information will be used",filename);
      FATAL_ERROR;
      }
   printf("\nReading %s for capacitance info.", filename);

   for (i=0; i<nodes.size(); i++)
      gate += nodes[i].capacitance;

   while (NULL!=fp.fgets(buffer, sizeof(buffer)-1))
      {
	  char *chptr;
      line++;
      
      end_seen = end_seen || strncmp(buffer, ".ENDS",5)==0;
      
      if (strncmp(buffer,".include",8)==0)
         {
         strcpy(filename2,filename);
         chptr = strrchr(filename2,'/');
         if (chptr==NULL)
            chptr = filename2;
         else
            chptr++;
         strcpy(chptr, buffer+9);
         while (*chptr!=' ' && *chptr!='\t' && *chptr!='\n' && *chptr!=0)
            chptr++;
         *chptr=0;
         filename = filename2;
         line=0;
         fp.fclose();
         fp.fopen(filename,"rt");
         if (!fp.IsOpen())
            {
            printf("\nI couldn't open %s for reading.\nNo capacitance information will be used",filename);
            FATAL_ERROR;
            }
         printf("\nReading %s for capacitance info.", filename);
         }      
      if (strncmp(buffer, "*|I (F",6)==0 && (chptr=strstr(buffer," GATE I"))!=NULL && index>=0)
		 {	  // I found a gate coord
		 float x=0.0, y=0.0;
		 chptr+=8;
		 chptr=strchr(chptr,' '); if (chptr==NULL) FATAL_ERROR;
		 chptr++;
		 x = atof(chptr);
		 chptr=strchr(chptr,' '); if (chptr==NULL) FATAL_ERROR;
		 chptr++;
		 y = atof(chptr);
		 nodes[index].gatecoord.x = x;
		 nodes[index].gatecoord.y = y;
		 }
      if (strncmp(buffer, "*|I (F",6)==0 && (chptr=strstr(buffer," SRC B 0 "))!=NULL && index>=0)
		 {	  // I found a gate coord
		 float x=0.0, y=0.0;
		 chptr+=8;
		 chptr=strchr(chptr,' '); if (chptr==NULL) FATAL_ERROR;
		 chptr++;
		 x = atof(chptr);
		 chptr=strchr(chptr,' '); if (chptr==NULL) FATAL_ERROR;
		 chptr++;
		 y = atof(chptr);
		 nodes[index].sdcoord.x = x;
		 nodes[index].sdcoord.y = y;
		 }
      if (strncmp(buffer, "*|I (F",6)==0 && (chptr=strstr(buffer," DRN B 0 "))!=NULL && index>=0)
		 {	  // I found a gate coord
		 float x=0.0, y=0.0;
		 chptr+=8;
		 chptr=strchr(chptr,' '); if (chptr==NULL) FATAL_ERROR;
		 chptr++;
		 x = atof(chptr);
		 chptr=strchr(chptr,' '); if (chptr==NULL) FATAL_ERROR;
		 chptr++;
		 y = atof(chptr);
		 nodes[index].sdcoord.x = x;
		 nodes[index].sdcoord.y = y;
		 }
	  if (strncmp(buffer, "*|NET ", 6)==0)
         {
         char netname[1024], *chptr;
         double cap=0;

         while ((chptr=strchr(buffer,'\\'))!=NULL)
            *chptr = '/';

         strcpy(netname, buffer+6);
         chptr = netname;

         while (*chptr != 0 && *chptr != ' ' && *chptr != '\t')
            chptr++;
         *chptr=0;

         chptr++;
         if (strncmp(chptr, "0PF", 3)==0)
            cap = 0.0;
         else {
            cap = atof(chptr);
            if (cap == 0.0)
               {
               printf("\nAt line %d of file %s, I couldn't understand this capacitance syntax.",line,filename);
               }
            }
         cap = cap * 1000.0;  // change from pF to fF 

         // now I need to whack off the X that avanti puts on net names
         bool done;
         char temp[200];
         do{
            done = true;
            chptr = strstr(netname, "/X");
            if (chptr!=NULL && chptr[2]!=0)
               {
               chptr[1] = 0;
               strcpy(temp, netname);
               strcat(temp, chptr+2);
               strcpy(netname, temp);
               done = false;
               }
            } while(!done);
//         strcpy(temp,"dpu_mul_dp/");
         strcpy(temp,"");
         if (netname[0]=='X')
            strcat(temp,netname+1);
         else
            strcat(temp,netname);         

//         strlwr(temp);
         if (h.Check(temp, index))
            {
            nodes[index].capacitance += cap;
			nodes[index].fixed_cap = cap;
            fixed += cap;
            num++;
            }
         else
            {
            if (mismatches <1000)
               printf("\nI could not match net '%s' in your cap file to a node in the design.",temp);
			index = -1;
            mismatches++;
            }
         }
      }
   fp.fclose();
   printf("\n%d nodes had capacitance info, %d nodes couldn't be matched",num,mismatches);
   printf("\n%8.3gfF of gate/sd cap and %8.3gfF of fixed cap.",gate,fixed);
   if (!end_seen)
      {
      printf("\nI did not see '.ENDS' in the cap file. Something bad must have happened.\n");
      FATAL_ERROR;
      }
   }

 
void circuittype::FlattenWirelist(wirelisttype *wl)
   {
   TAtype ta("Flattening and translating to sherlock datatype.");
   int i;
   char buffer[512];
   int index;
   
   if (wl==NULL) FATAL_ERROR;
   while (wl->Flatten())
      ;
   
   nodes   = wl->nodes;
   devices = wl->devices;

   for (i=0; i<nodes.size(); i++)
      {
      char *chptr;
      wl->nodes[i].RName(buffer);
//      strlwr(buffer);
      while ((chptr = strchr(buffer,'$')) !=NULL)
         *chptr = '/';

      if (h.Check(buffer, index) && index>1) 
         {
         printf("\nI have a nodename %s used twice\n",buffer);
         FATAL_ERROR;  // node name is used twice
         }
      else
         h.Add(buffer, i);
      }

   wl->nodes.UnAllocate();
   wl->devices.UnAllocate();

   for (i=0; i<devices.size(); i++)
      {
      const devicetype &d = devices[i];

      if (d.type==D_PMOS)
         {
         nodes[d.gate].capacitance   += d.width * PGATE_CAPACITANCE;
         nodes[d.source].capacitance += d.width * PDIFF_CAPACITANCE;
         nodes[d.drain].capacitance  += d.width * PDIFF_CAPACITANCE;
         }
      else
         {
         nodes[d.gate].capacitance   += d.width * NGATE_CAPACITANCE;
         nodes[d.source].capacitance += d.width * NDIFF_CAPACITANCE;
         nodes[d.drain].capacitance  += d.width * NDIFF_CAPACITANCE;
         }
      if (d.width <=0.0)
         {
         printf("Device %s has a non-positive device width",d.name);
         FATAL_ERROR;
         }
      }
   }


void circuittype::Parse()
   {                                                   
   TAtype ta("simtype::Parse()");
   int i,index;   

   printf("\nCross referencing nodes/devices");
   for (i=0; i<devices.size(); i++)
      {
      if ((index=devices[i].source)>=2)
         {
         nodes[index].sds.push(&devices[i]);         
         nodes[index].sds.RemoveDuplicates();
         }
      if ((index=devices[i].drain)>=2)
         {
         nodes[index].sds.push(&devices[i]);         
         nodes[index].sds.RemoveDuplicates();
         }
      index=devices[i].gate;
      nodes[index].gates.push(&devices[i]);
      nodes[index].gates.RemoveDuplicates();
      }
   if (nodes[VSS].sds.size()) FATAL_ERROR;
   if (nodes[VDD].sds.size()) FATAL_ERROR;
   }
