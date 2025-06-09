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
  int relativetime=0;            
  int i,k,n,cn;
  enum valuetype value;
  bool drive0, drive1, on, delaythisnode;
  int s_mem[128], d_mem[128], c_mem[128];
  
  /*   static count;
       count++;
       if (count>1952)
       printf("count =%d\n",count);
  */
  
  arraytype <int> statenodes(s_mem, sizeof(s_mem));
  arraytype <int> ramnodes;
  arraytype <int> dummy(d_mem, sizeof(d_mem));
  arraytype <int> crow_bar(c_mem, sizeof(c_mem));
  hashtype <int, bool> delayed_hash;
  int oldtime=0;
  
  crow_bar.clear();
  while (nodeheap.pop(n, relativetime))
    {
      if (relativetime != oldtime){
	IntermediateNodeDumping(relativetime, currentTime);
      }
      oldtime = relativetime;
      if (relativetime >= 500) 
	{       
	  ErrorLog("\nInfinite loop @%dns at node ->%-30s\n", currentTime/1000, nodes[n].name);
	  do{
            nodes[n].value    = V_ILLEGAL;
            nodes[n].inflight = false;
	  }while (nodeheap.pop(n, relativetime));
	  return;
	}
      
      nodes[n].inflight=false;
      cn=n;                                                       
      drive0=drive1=false;
      delaythisnode = false;
      Eval(cn, drive0, drive1, statenodes, ramnodes);

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
		nodeheap.push(n, relativetime+1);  // let's hope that the short goes away so we can prevent doing alot of expensive analysis
	      }
	      delayed_hash.Add(n, true);
            }
	  SetAsRatioed(n);
	}
      else 
	nodes[n].ratioed = false;
      
      if (drive0 && !drive1 && statenodes.size()==2 && ramnodes.size()==0)
	{  // I'm looking for cross-coupled P's where both nodes are pulled low
	  TAtype ta("Metastable check");
	  bool d0_1=false, d1_1=false, d0_2=false, d1_2=false;
	  // This happens in full-swing sense amps with a leaker device
	  Eval(statenodes[0], d0_1, d1_1, dummy, dummy);
	  Eval(statenodes[1], d0_2, d1_2, dummy, dummy);
	  
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
				    ErrorLog("\nERROR: I found a metastable condition @%dns on nodes %s and %s.", currentTime/1000, nodes[statenodes[0]].name,nodes[statenodes[1]].name);
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
	  crow_bar.push(cn);
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
		//if (nodes[cn].state)
		{
		  Eval(cn, drive0, drive1, dummy, dummy);
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
		      crow_bar.push(cn);
		    }
		  else if (drive1)
		    {
		      value = V_ONE;
		      nodes[cn].ratioed = false;
		    }
		  else if (drive0)
		    {
		      value = V_ZERO;
		      nodes[cn].ratioed = false;
		    }
		  else
		    FATAL_ERROR;
		}
	      //if ((nodes[cn].value!=value || nodes[cn].changed) && (nodes[cn].value == value || !nodes[cn].constant))
	      if (nodes[cn].value!=value || nodes[cn].changed || nodes[cn].constant)
		{ // state changed so propogate   
		  if (nodes[cn].outputSigChangedPtr != NULL)
		    *nodes[cn].outputSigChangedPtr = true;
		  
		  if (nodes[cn].value != value)  {
		    nodes[cn].toggleCount++;
		    FlipFlop *owner=nodes[cn].m_flop_owner;
		    if (owner !=NULL){
		      if(owner->m_last_ttime != currentTime){
			owner->m_tc++;
			owner->m_sw_cap+=nodes[cn].capacitance;
			owner->m_last_ttime=currentTime;
		      }
		    }
		  }
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
				  nodeheap.push(d.source, relativetime+1);
				  nodes[d.source].inflight=true;
				}
			    }
			  else if (d.drain>VDDO) 
			    if (!nodes[d.drain].inflight &&     
				(nodes[d.source].value != nodes[d.drain].value ||
				 !nodes[d.source].state || !nodes[d.drain].state))
			      {    
				nodeheap.push(d.drain, relativetime+1);
				nodes[d.drain].inflight=true;
			      }
			}                           
		      else    // if device is off , the s/d could go floating. Push source and drain
			{        
			  if (d.source>VDDO )     
			  //if (d.source>VDDO && nodes[d.source].ratioed)     
			    if (!nodes[d.source].inflight)
			      {
				nodeheap.push(d.source, relativetime+1);
				nodes[d.source].inflight=true;
			      }
			  if (d.drain>VDDO) 
			    //if (d.drain>VDDO && nodes[d.drain].ratioed) 
			    if (!nodes[d.drain].inflight)
                              {
				nodeheap.push(d.drain, relativetime+1);
				nodes[d.drain].inflight=true;
                              }
			}   
		    }
		  for (k=0; k<nodes[cn].rams_wl.size(); k++)
		    {
		      int index = nodes[cn].rams_wl[k];
		      ramtype &r = rams[index];
		      if (r.wl != cn) FATAL_ERROR;
		      if (value==V_ONE)
			{
			  nodes[r.bh].rams_on.push(index);
			  nodes[r.bl].rams_on.push(index);
			}
		      else if (value==V_ZERO)
			{
			  int j;
			  for (j=nodes[r.bh].rams_on.size()-1; j>=0; j--)
			    {
			      if (nodes[r.bh].rams_on[j]==index)
				nodes[r.bh].rams_on.RemoveFast(j);
			    }
			  for (j=nodes[r.bl].rams_on.size()-1; j>=0; j--)
			    {
			      if (nodes[r.bl].rams_on[j]==index)
				nodes[r.bl].rams_on.RemoveFast(j);
			    }
			}
		      if (!nodes[r.bh].inflight)
			{
			  nodeheap.push(r.bh, relativetime+1);
			  nodes[r.bh].inflight=true;
			}
		      if (!nodes[r.bl].inflight)
			{
			  nodeheap.push(r.bl, relativetime+1);
			  nodes[r.bl].inflight=true;
			}
		    }
		}
            }
	  for (i=0; i<ramnodes.size(); i++)
            {
	      ramtype &r = rams[ramnodes[i]];
	      if (nodes[r.wl].value==V_ONE && nodes[r.bh].value==V_ONE && nodes[r.bl].value==V_ZERO)
		r.value = V_ONE;
	      if (nodes[r.wl].value==V_ONE && nodes[r.bh].value==V_ZERO && nodes[r.bl].value==V_ONE)
		r.value = V_ZERO;
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
	  Eval(n, drive0, drive1, statenodes, ramnodes); // check to see if node is even ratioed still
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
		      ErrorLog("\nNode %s @%dns is ratioed, W/L vss->%.2f  W/L vdd->%.2f. I will print all the ON devices:",nodes[n].RName(buf2), currentTime/1000, 1.0/res0,1.0/res1);
		      CrowbarHelpEval(n);
		      ErrorLog("\n");
		    }
		}
	      nodeheap.push(n, relativetime+1); // make sure these errors get reevaluated next time slice
	      nodes[n].value = V_ILLEGAL;
            }
	  else if (!drive0 && !drive1 && !nodes[n].constant)
            {
	      nodes[n].value = V_Z;
	      if (!ignoreCrowbars && !nodes[n].nofloatcheck && DoesFloatMatter(n)){
		char buf2[128];
		ErrorLog("\nNode %s @%dns is floating.",nodes[n].RName(buf2), currentTime/1000);
	      }
            }
	}
    }
  IntermediateNodeDumping(relativetime, currentTime);
  
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

   void circuittype::PropogateX()
      {
      TAtype ta("circuittype::CheckForRelevance()");
      int i, k, cn;
      arraytype <int> s;

      for (i=VDDO+1; i<nodes.size(); i++)
         {
         nodetype &n = nodes[i];

         n.inflight = false;

         if (n.value == V_ILLEGAL || n.value == V_Z)
            {
            s.push(i);
            }
         }
      for (i=0; i<devices.size(); i++)
         {
         devicetype &d = devices[i];
         const nodetype &n = nodes[d.gate];

         d.on  = (d.type==D_NMOS && n.value==V_ONE)  || (d.type==D_PMOS && n.value==V_ZERO) || n.value==V_Z || n.value==V_ILLEGAL;
         d.off = (d.type==D_NMOS && n.value==V_ZERO) || (d.type==D_PMOS && n.value==V_ONE)  || n.value==V_Z || n.value==V_ILLEGAL;
         }

      while (s.pop(cn))
         {
         nodetype &n = nodes[cn];
         arraytype <int> statenodes;
         arraytype <int> ramnodes;
         bool drive0on  = false, drive1on  = false;
         bool drive0off = false, drive1off = false;

         n.inflight = true;
         Eval   (cn, drive0on,  drive1on,  statenodes, ramnodes);
         EvalOff(cn, drive0off, drive1off, statenodes);

         if (drive0on==drive1on || drive0off==drive1off || n.value==V_ILLEGAL || n.value==V_Z)  // I have an X condition
            {
            for (i=0; i<statenodes.size(); i++)
               {
               cn = statenodes[i];
               nodes[cn].value = V_ILLEGAL;

               for (k=0; k<nodes[cn].gates.size(); k++)
                  {
                  devicetype &d = *nodes[cn].gates[k];
                  d.on  = true;		 // make all gates on an X node be on
                  d.off = true;

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

      return;
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


void circuittype::Initialize(bool one, bool suppress_warnings)
      {
      TAtype ta("Initializing nodes");
      int i;
      int nmos=0, pmos=0, fuse=0;
      arraytype <bool> driveninputs(nodes.size(), false);

      ZeroNodes();
      for (i=0; i<inputs.size(); i++)
         {
         driveninputs[inputs[i]] = true;
         nodes[inputs[i]].state = true;
         nodes[inputs[i]].constant = true;  // this will suppress the floating node warning
         }
      for (i=0; i<outputs.size(); i++)
         nodes[outputs[i]].state = true;
      for (i=0; i<rams.size(); i++)
         {
         const ramtype &r = rams[i];
         nodes[r.bh].state = true;
         nodes[r.bl].state = true;
         nodes[r.wl].state = true;
         }

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
         if (!suppress_warnings && nodes[i].sds.size()==0 && nodes[i].gates.size()>0 && !driveninputs[i] && i!=VDD && i!=VSS && strstr(nodes[i].name, "xofiller!FILL")==NULL && strstr(nodes[i].name, "xofiller_FILL")==NULL && 
	     strstr(nodes[i].name, "/uconn")==NULL)
	   {
            int k;
            bool real=false;
            for (k=0; k<nodes[i].gates.size(); k++)
               {
               const devicetype &d = *nodes[i].gates[k];
               if (d.source!=d.drain) real=true;
               }
            if (real)
               ErrorLog("\n>%s is a dangling input.",nodes[i].name);
            }
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
      Log("\n%d NMOS, %d PMOS, %d FUSE, %d 6T Ram",nmos,pmos,fuse,rams.size());
      Log("\nInitializing nodes");
      Log("-> All=0...");   
      Propogate(-1, true);
      for (i=0; i<nodes.size(); i++)
         nodes[i].toggleCount = 0;
      }  


   // I will try to detect leaker devices and not count these as ratioing.
void circuittype::Eval(int cn, bool& drive0, bool& drive1, arraytype <int>& statenodes, arraytype <int>& ramnodes)
   {
   //   TAtype ta("circuittype::Eval()");
   int i;
   int mem[128];
   arraytype <int> s(mem, sizeof(mem));

   statenodes.clear();
   ramnodes.clear();

   if (cn == VSS) { drive0 = true; return; }
   if (cn == VDD) { drive1 = true; return; }

   if (touchcount == 0)
      ReSync();
   touchcount++;

   nodes[cn].touched = touchcount;
   do {
      if (nodes[cn].state)
         statenodes.push(cn);
      for (i = 0; i < nodes[cn].sds.size(); i++)
         {
         const devicetype& d = *nodes[cn].sds[i];
         if (d.on)
            {
            if (d.source == VSS)      drive0 = true;
            else if (d.source == VDD) drive1 = true;
            else if (nodes[d.source].touched != touchcount)
               {
               nodes[d.source].touched = touchcount;
               s.push(d.source);
               }
            if (d.drain == VSS)      drive0 = true;
            else if (d.drain == VDD) drive1 = true;
            else if (nodes[d.drain].touched != touchcount)
               {
               nodes[d.drain].touched = touchcount;
               s.push(d.drain);
               }
            }
         }
      /*
      for (i=0; i<nodes[cn].rams_bl.size(); i++)
      {
      int index = nodes[cn].rams_bl[i];
      const ramtype &r = rams[index];
      if (r.bh == cn && nodes[r.wl].value!=V_ZERO)
      {
      if (r.value==V_ONE) drive1 = true;
      else drive0 = true;
      ramnodes.push(index);
      }
      if (r.bl == cn && nodes[r.wl].value!=V_ZERO)
      {
      if (r.value==V_ONE) drive0 = true;
      else drive1 = true;
      ramnodes.push(index);
      }
      }*/
      for (i = 0; i < nodes[cn].rams_on.size(); i++)
         {
         int index = nodes[cn].rams_on[i];
         const ramtype& r = rams[index];
         if (r.bh == cn && nodes[r.wl].value != V_ZERO)
            {
            if (r.value == V_ONE) drive1 = true;
            else drive0 = true;
            ramnodes.push(index);
            }
         if (r.bl == cn && nodes[r.wl].value != V_ZERO)
            {
            if (r.value == V_ONE) drive0 = true;
            else drive1 = true;
            ramnodes.push(index);
            }
         }
      } while (s.pop(cn));
   }

void circuittype::EvalOff(int cn, bool &drive0, bool &drive1, arraytype <int> &statenodes)
{
  //   TAtype ta("circuittype::Eval()");
  int i;
  int mem[128];
  arraytype <int> s(mem, sizeof(mem));
  
  if (cn==VSS) {drive0=true; return;}
  if (cn==VDD) {drive1=true; return;}
  
  if (touchcount==0)
    ReSync();
  touchcount++;
  
  if (nodes[cn].rams_wl.size()) FATAL_ERROR;  // didn't write support for rams for this routine yet
  if (nodes[cn].rams_bl.size()) FATAL_ERROR;  // didn't write support for rams for this routine yet
  
  nodes[cn].touched=touchcount;   
  do {     
    if (nodes[cn].state)
      statenodes.push(cn);
    for (i=0; i<nodes[cn].sds.size(); i++)
      {
	const devicetype &d = *nodes[cn].sds[i];      
	if (!d.off)
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
  arraytype <int> s(mem, sizeof(mem));
  
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
            char buf1[256], buf2[256], buf3[256];
            ErrorLog("\n\t%s src=%s gate=%s drn=%s",d.name,nodes[d.source].VName(buf1),nodes[d.gate].VName(buf2),nodes[d.drain].VName(buf3));
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
    for (i=0; i<nodes[cn].rams_bl.size(); i++)
      {
	int index = nodes[cn].rams_bl[i];
	ramtype &r = rams[index];
	if (nodes[r.wl].value==V_ONE)
	  ErrorLog("\n\tRam%d=%d bh=%s=%d bl=%s=%d wl=%s=%d", index, rams[index].value, nodes[r.bh].name, nodes[r.bh].value, nodes[r.bl].name, nodes[r.bl].value, nodes[r.wl].name, nodes[r.wl].value);
      }
  }while (s.pop(cn));
}


void circuittype::DebugEval(int cn)
{
  int i;
  arraytype<int> s;
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
  arraytype <int> s(mem, sizeof(mem));
  
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
   arraytype <int> s1(s1_mem, sizeof(s1_mem)), s2(s2_mem, sizeof(s2_mem));
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
      nodes[i].state = false;
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
  arraytype<int> pds, pus, inverters;
  
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


// returns true if this is connected to a gate
bool circuittype::DoesFloatMatter(int node_index)
{
  int i;
  const nodetype &n = nodes[node_index];
  arraytype <int> downstream;
  
  if (n.value!=V_Z)
    FATAL_ERROR;
  
  for (i=0; i<n.gates.size(); i++)
    {
      devicetype &d = *n.gates[i];
      
      d.on = true;  // turn on all devices connected to a floating node
      downstream.push(d.drain);
      downstream.push(d.source);
    }
  downstream.Sort();
  downstream.RemoveDuplicates();
  
  for (i=0; i<downstream.size(); i++)
    if (downstream[i]>VDD)
      {
	arraytype <int> statenodes, ramnodes;
	bool drive0=false, drive1=false;
	Eval(downstream[i], drive0, drive1, statenodes, ramnodes);
	if (drive0==drive1)
	  return true;
      }
  return false;
}

// returns true if this node is driven solely by inverters(can be ganged however)
bool circuittype::IsOnlyInverters(int node_index)
{
  int i;
  arraytype<int> pds, pus, inverters;
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
  arraytype<int> pds, pus, common;
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
            d.r*=1.3;   // pmos is aproximately 1.3 the resistance of NMOS
	  nodes[d.gate].capacitance   += d.GateCap();
	  nodes[d.source].capacitance += d.DiffCap();
	  nodes[d.drain].capacitance  += d.DiffCap();
	  
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


void AvantiFix(char *output, const char *input)
{
  char temp[1024];
  char *chptr;
  bool done;
  
  memset(temp, 0 ,sizeof(temp));
  strcpy(temp, input);
  
  do{
    done = true;
    chptr = strstr(temp, "/X");
    if (chptr!=NULL && chptr[2]!=0)
      {
	chptr[1] = 0;
	strcpy(output, temp);
	strcat(output, chptr+2);
	strcpy(temp, output);
	done = false;
      }
  } while(!done);
  
  strcpy(output,"");
  if (temp[0]=='X')
    strcat(output,temp+1);
  else
    strcat(output,temp);
  // nodes that get split end up looking like foo@1, foo@2, so just add all the fixed cap
  chptr = strchr(output, '@');
  if (chptr!=NULL)
    *chptr = 0;
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
   bool subckt_seen=false;
   bool end_seen=false;
   char filename2[256];
   texthashtype <int> devicename_hash;
   fp.fopen(filename,"rt");
   if (!fp.IsOpen())
      {
      printf("\nI couldn't open %s for reading.\nNo capacitance information will be used",filename);
      FATAL_ERROR;
      }
   printf("\nReading %s for capacitance info.", filename);

   //   FILE *fptr=fopen("devices.txt","wt");
   for (i=0; i<devices.size(); i++)
      {
      const devicetype &d = devices[i];
      devicename_hash.Add(d.name, i);
      //      fprintf(fptr,"%s\n",d.name);
      }
   //   fclose(fptr);

   for (i=0; i<nodes.size(); i++)
      gate += nodes[i].capacitance;

   memset(buffer, 0 , sizeof(buffer));
   while (NULL!=fp.fgets(buffer, sizeof(buffer)-5))
      {
      char *chptr;
      line++;

      end_seen = end_seen || strncmp(buffer, ".ENDS",5)==0;
      if(strncmp(buffer,".SUBCKT",7)==0)
         subckt_seen=subckt_seen|true;
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
      if (strncmp(buffer, "*|I (F",6)==0)
         {
         char *end;
         int fingers=-1;
         char temp[1024];
         strcpy(temp, buffer);

         chptr = temp+6;
         while (*chptr!=' ' && *chptr!=0)
            chptr++;
         chptr++;
         end = chptr+1;
         while (*end!=' ' && *end!='@' && *end!=0)
            end++;
         if (*end=='@')
            fingers = atoi(end+1);
         if (fingers<=0) fingers=1;
         *end=0;
         int dindex=-1;
         char molestedname[200];
         AvantiFix(molestedname, chptr);
         if (devicename_hash.Check(chptr, dindex))
            {
            devices[dindex].fingers = MAXIMUM(devices[dindex].fingers, fingers);
            }
         else if (devicename_hash.Check(molestedname, dindex))
            {
            devices[dindex].fingers = MAXIMUM(devices[dindex].fingers, fingers);
            }
         }
      // look for a line like XXalu2/Xbyp/Xmux[33]/XI22/XI16/MMN0@3 pp_alu2__result_5b[33] Xalu2/Xbyp/Xmux[33]/XI22/zz_result_5b gndl gndl nfet ad=0.0156p as=0.0156p covpccnr=0 l=0.03u lpccnr=3e-08 m=1 nf=1 ngcon=1 p_la=0 pccrit=1 pcpl=0.0000 pcpr=0.0000 pd=0.412u plorient=1 ps=0.412u ptwell=0 rxtuck_d=0 rxtuck_part_d=0 rxtuck_part_s=0 rxtuck_s=0 sa=0.635u sb=0.505u sca=306.873 scb=0.0343562 scc=0.0143754 sd=0 sizedup=0 stress_vt=-0.0033 u0_mult=1.0069 w=0.312u wrxcnr=2.808e-07 x=100.967 y=70.714 x=101.123 y=70.729 angle=0
      if (strchr(buffer,'@')!=NULL && strstr(buffer,"as=")!=NULL)
         {
         char devicename[1024];
         int fingers=-1;
         strcpy(devicename, buffer);
         chptr=devicename;
         while (*chptr!=' ' && *chptr!=0)
            chptr++;
         *chptr=0;
         chptr = strchr(devicename, '@');
         if (chptr!=NULL)
            {
            *chptr=0;
            fingers = atoi(chptr+1);
            }
         if (fingers>=1)
            {
            int dindex=-1;
            char molestedname[1024];
            AvantiFix(molestedname, chptr);
            if (devicename_hash.Check(devicename, dindex))
               {
               devices[dindex].fingers = MAXIMUM(devices[dindex].fingers, fingers);
               }
            else if (devicename_hash.Check(devicename+1, dindex))
               {
               devices[dindex].fingers = MAXIMUM(devices[dindex].fingers, fingers);
               }
            else if (devicename_hash.Check(molestedname, dindex))
               {
               devices[dindex].fingers = MAXIMUM(devices[dindex].fingers, fingers);
               }
            }
         }
      if (strncmp(buffer, "*|I (F",6)==0 && (chptr=strstr(buffer," GATE I"))!=NULL && index>=0)
	{	  // I found a gate coord
	  double x=0.0, y=0.0;
	  chptr+=8;
	  chptr=strchr(chptr,' '); if (chptr==NULL) FATAL_ERROR;
	  chptr++;
	  x = atof(chptr);
	  chptr=strchr(chptr,' '); if (chptr==NULL) FATAL_ERROR;
	  chptr++;
	  y = atof(chptr);
	  nodes[index].gatecoord.x=RoundToGDSResolution(x);
	  nodes[index].gatecoord.y=RoundToGDSResolution(y);
	}
      if (strncmp(buffer, "*|I (F",6)==0 && (chptr=strstr(buffer," SRC B 0 "))!=NULL && index>=0)
	{	  // I found a source coord
	  double x=0.0, y=0.0;
	  chptr+=8;
	  chptr=strchr(chptr,' '); if (chptr==NULL) FATAL_ERROR;
	  chptr++;
	  x = atof(chptr);
	  chptr=strchr(chptr,' '); if (chptr==NULL) FATAL_ERROR;
	  chptr++;
	  y = atof(chptr);
	  if (nodes[index].sdcoord.x==0 && nodes[index].sdcoord.y==0){
	    nodes[index].sdcoord.x=RoundToGDSResolution(x);
	    nodes[index].sdcoord.y=RoundToGDSResolution(y);
	  }
	}
      if (strncmp(buffer, "*|I (F",6)==0 && (chptr=strstr(buffer," DRN B 0 "))!=NULL && index>=0)
	{	  // I found a drain coord
	  double x=0.0, y=0.0;
	  chptr+=8;
	  chptr=strchr(chptr,' '); if (chptr==NULL) FATAL_ERROR;
	  chptr++;
	  x = atof(chptr);
	  chptr=strchr(chptr,' '); if (chptr==NULL) FATAL_ERROR;
	  chptr++;
	  y = atof(chptr);
	  if (nodes[index].sdcoord.x==0 && nodes[index].sdcoord.y==0){
	    nodes[index].sdcoord.x=RoundToGDSResolution(x);
	    nodes[index].sdcoord.y=RoundToGDSResolution(y);
	  }
	}
      /*
      if (strstr(buffer,"angle=0")!=NULL)
      { // I found a transistor definition with coordinates
      char *devicename, *source, *gate, *drain, *bulk, *device;
      char *chptr=buffer;
      float x=0.0, y=0.0;

      devicename = chptr;
      chptr = strchr(chptr, ' ');
      if (chptr==NULL)
      FATAL_ERROR; // I got confused parsing transistor definition
      chptr[0]=0;
      chptr++; source = chptr;
      chptr = strchr(chptr, ' ');
      if (chptr==NULL)
      FATAL_ERROR; // I got confused parsing transistor definition
      chptr[0]=0;
      chptr++; gate = chptr;
      chptr = strchr(chptr, ' ');
      if (chptr==NULL)
      FATAL_ERROR; // I got confused parsing transistor definition
      chptr[0]=0;
      chptr++; drain = chptr;
      chptr = strchr(chptr, ' ');
      if (chptr==NULL)
      FATAL_ERROR; // I got confused parsing transistor definition
      chptr[0]=0;
      chptr++; bulk = chptr;
      chptr = strchr(chptr, ' ');
      if (chptr==NULL)
      FATAL_ERROR; // I got confused parsing transistor definition
      chptr[0]=0;
      chptr++; device = chptr;
      chptr = strchr(chptr, ' ');
      if (chptr==NULL)
      FATAL_ERROR; // I got confused parsing transistor definition
      chptr[0]=0;
      chptr++; 

      chptr=strstr(chptr,"x=");
      if (chptr==NULL)
      FATAL_ERROR;
      x = atof(chptr+2);
      chptr=strstr(chptr,"y=");
      if (chptr==NULL)
      FATAL_ERROR;
      y = atof(chptr+2);


      if (strstr(device,"fet")==NULL)
      FATAL_ERROR;

      // now I need to whack off the X that avanti puts on net names
      char molestedname[200];
      AvantiFix(molestedname, gate);

      if (h.Check(gate, index))
      {
      nodes[index].gatecoord.x = x;
      nodes[index].gatecoord.y = y;
      }
      else if (h.Check(molestedname, index))
      {
      nodes[index].gatecoord.x = x;
      nodes[index].gatecoord.y = y;
      }
      else
      {
      if (mismatches <1000)
      printf("\nI could not match net '%s' in your cap file to a node in the design.",gate);
      index = -1;
      mismatches++;
      }
      }
      */
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

         char *end = netname;
         while (*end!=' ' && *end!='@' && *end!=0)
            end++;
         //         if (*end=='@')
         //            fingers = atoi(end+1);
         *end=0;

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
         char molestedname[200];
         AvantiFix(molestedname, netname);


         if (h.Check(netname, index))
            {
            nodes[index].capacitance += cap;
            nodes[index].fixed_cap   += cap;
            fixed += cap;
            num++;
            }
         else if (h.Check(molestedname, index))
            {
            nodes[index].capacitance += cap;
            nodes[index].fixed_cap   += cap;
            fixed += cap;
            num++;
            }
         else if (strstr(netname, "BADN_X")!=NULL)
            {
            index = -1;
            }
         else 
            {
            //            if (mismatches <1000)
            printf("\nI could not match net '%s' in your cap file to a node in the design.",netname);
            index = -1;
            mismatches++;
            }
         }

      // process a line like:
      //XXIrdp1/XIrd_sa[11]/XIsa/XISamp/MMP2 XIrdp1/XIrd_sa[11]/XIsa/XISamp/zz1 vddl vddl vddl pfet SA=0.373u SB=0.802u SD=0 ad=0.015p as=0.029p l=0.03u m=1 nf=1 ngcon=1 p_la=4e-09 pccrit=1 pcpl=0 pcpr=0 pd=0.422u plorient=1 pre_layout_local=0 ps=0.841u ptwell=0 rxtuck_d=0 rxtuck_part_d=0 rxtuck_part_s=0 rxtuck_s=0 sizedup=0 stress_vt=0 u0_mult=1 w=0.326u x=19.795 y=45.76 angle=0
      if (buffer[0]=='X' && strstr(buffer,"angle=0")!=NULL)
         {
         char devicename[1024], *chptr;
         double x=0.0, y=0.0;
         int fingers=1;
         int dindex=-1;

         while ((chptr=strchr(buffer,'\\'))!=NULL)
            *chptr = '/';

         if (buffer[1]=='X')
            strcpy(devicename, buffer+2);
         else
            strcpy(devicename, buffer+1);
         chptr = devicename;

         while (*chptr != 0 && *chptr != ' ' && *chptr != '\t')
            chptr++;
         *chptr=0;

         char *end = devicename;
         while (*end!=' ' && *end!='@' && *end!=0)
            end++;
         if (*end=='@')
            fingers = atoi(end+1);
         *end=0;

         chptr++;
         chptr = strstr(buffer,"x=");
         if (chptr!=NULL)
            x = atof(chptr+2);
         chptr = strstr(buffer,"y=");
         if (chptr!=NULL)
            y = atof(chptr+2);

         // now I need to whack off the X that avanti puts on net names
         char molestedname[200];
         AvantiFix(molestedname, devicename);
         //printf("Molestedname %s FixedName %s x=%.2f y=%.2f\n",molestedname,devicename,x,y);
         if (devicename_hash.Check(devicename, dindex))
            {
            devices[dindex].fingers = MAXIMUM(devices[dindex].fingers, fingers);
            nodes[devices[dindex].gate].gatecoord.x = x;
            nodes[devices[dindex].gate].gatecoord.y = y;
            }
         else if (devicename_hash.Check(devicename+1, dindex))
            {
            devices[dindex].fingers = MAXIMUM(devices[dindex].fingers, fingers);
            nodes[devices[dindex].gate].gatecoord.x = x;
            nodes[devices[dindex].gate].gatecoord.y = y;
            }
         else if (devicename_hash.Check(molestedname, dindex))
            {
            devices[dindex].fingers = MAXIMUM(devices[dindex].fingers, fingers);
            nodes[devices[dindex].gate].gatecoord.x = x;
            nodes[devices[dindex].gate].gatecoord.y = y;
            }
         }
      }
   fp.fclose();
   int finger_info=0;
   for (i=0; i<devices.size(); i++)
      {
      const devicetype &d = devices[i];
      if (d.fingers>0)
         finger_info++;
      }
   printf("\n%d nodes had capacitance info, %d nodes couldn't be matched\n",num,mismatches);
   printf("%d devices had fingering info, %d devices couldn't be matched\n",finger_info,devices.size()-finger_info);
   printf("%8.3gfF of gate/sd cap and %8.3gfF of fixed cap.\n",gate,fixed);
   if (subckt_seen & !end_seen)
      {
      printf("\nI did not see '.ENDS' in the cap file. Something bad must have happened.\n");
      FATAL_ERROR;
      }
   }

void circuittype::ReadActivityfile(const char *filename)
   {
   TAtype ta("simtype::ReadActivityfile");
   char buffer[2048];
   int line = 0;
   int num=0, mismatches=0;
   int index=-1;
   filetype fp;
   float a;
   float period;
   float capacitance;
   float wire_capacitance;
   float pin_capacitance;
   float external_capacitance;

   fp.fopen(filename,"rt");
   if (!fp.IsOpen())
      {
      printf("\nI couldn't open %s for reading.\n",filename);
      FATAL_ERROR;
      }
   printf("\nReading %s for activity info.\n", filename);

   memset(buffer, 0 , sizeof(buffer));
   while (NULL!=fp.fgets(buffer, sizeof(buffer)-5))
      {
      char *netname, *chptr;
      line++;

      period = -1;
      capacitance = -1;
      wire_capacitance = -1;
      pin_capacitance = -1;
      external_capacitance = -1;

      if (strncmp(buffer,"set_node",8)==0)
         {
         if ((chptr=strstr(buffer, "/activity_factor="))!=NULL)
            {
            chptr += 17;
            a = atof(chptr);
            if (a<=0.0)
               {
               printf("I saw a zero or negative activity factor at line %d of %s.\n",line,filename);
               }
            }
         if ((chptr=strstr(buffer, "/clock_period="))!=NULL)
            {
            chptr += 14;
            period = atof(chptr);
            if (period<=0.0)
               {
               printf("I saw a zero or negative clock period at line %d of %s.\n",line,filename);
               }
            }
         if ((chptr=strstr(buffer, "/capacitance="))!=NULL)
            {
            chptr += 13;
            capacitance = atof(chptr);
            if (capacitance<0.0)
               {
               printf("I saw a negative capacitance at line %d of %s. Ignoring this\n",line,filename);
               capacitance=-1;
               }
            }
         if ((chptr=strstr(buffer, "/wire_capacitance="))!=NULL)
            {
            chptr += 18;
            wire_capacitance = atof(chptr);
            if (wire_capacitance<0.0)
               {
               printf("I saw a negative wire_capacitance at line %d of %s. Ignoring this\n",line,filename);
               wire_capacitance=-1;
               }
            }	  
         if ((chptr=strstr(buffer, "/pin_capacitance="))!=NULL)
            {
            chptr += 17;
            pin_capacitance = atof(chptr);
            if (pin_capacitance<0.0)
               {
               printf("I saw a negative pin_capacitance at line %d of %s. Ignoring this\n",line,filename);
               pin_capacitance=-1;
               }
            }
         if ((chptr=strstr(buffer, "/external_capacitance="))!=NULL)
            {
            chptr += 22;
            external_capacitance = atof(chptr);
            if (external_capacitance<=0.0)
               {
               external_capacitance=-1;
               }
            }

         netname=buffer+8;
         while (*netname==' ' || *netname=='\t')
            netname++;
         chptr=netname;
         while (*chptr!=' ' && *chptr!='\t' && *chptr!=0)
            chptr++;
         *chptr=0;  // zero terminate the node name

         if (h.Check(netname, index))
            {
            nodes[index].activity = a;
            nodes[index].frequency = (period > 0) ? (1.0/period*1000) : -1.0; // Period is in ns and we need frequency in MHz
            if (nodes[index].fixed_cap <= 0.0) 
               {
               // If wire capacitance is available it is the new format
               if (wire_capacitance > 0) 
                  {
                  nodes[index].fixed_cap    = wire_capacitance * 1000; // Capacitance is in pF and we need fF
                  nodes[index].capacitance  = wire_capacitance * 1000; // Capacitance is in pF and we need fF
                  if (pin_capacitance > 0) 
                     {
                     // printf("Net %s is forced to have no device cap to avoid double counting it. pin capacitance = %10.4f\n", netname, pin_capacitance);
                     nodes[index].fixed_cap   += pin_capacitance * 1000; // Capacitance is in pF and we need fF
                     nodes[index].capacitance += pin_capacitance * 1000; // Capacitance is in pF and we need fF
                     nodes[index].skipDeviceCap = true;                  // Don't double count device caps
                     }
                  }
               // If wire capacitance is not available it is the old format
               else if (capacitance > 0)
                  {
                  // printf("Net %s is forced to have no device cap to avoid double counting it.\n", netname, capacitance);
                  nodes[index].fixed_cap    = capacitance * 1000; // Capacitance is in pF and we need fF
                  nodes[index].capacitance  = capacitance * 1000; // Capacitance is in pF and we need fF
                  nodes[index].skipLoad     = true;               // capacitance from EM config includes external load
                  nodes[index].skipDeviceCap = true;              // Don't double count device caps
                  }
               }
            num++;
            }
         else
            {
            if (mismatches <1000)
               printf("\nI could not match net '%s' in your activity file to a node in the design.",netname);
            mismatches++;
            }
         }
      }
   fp.fclose();
   printf("\n%d nodes had activity info, %d nodes couldn't be matched\n",num,mismatches);
   }

void circuittype::FlattenWirelist(wirelisttype *wl)
   {
   TAtype ta("Flattening and translating to sherlock datatype.");
   int i;
   char buffer[512];
   int index;

   if (wl==NULL) FATAL_ERROR;
   wirelisttype *deep;
   
   while ((deep=wl->FindDeepestMinusOne())!=NULL)
      {
	deep->Flatten(leakSigs);
      }
   wl->Flatten(leakSigs);


   nodes   = wl->nodes;

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

   devices = wl->devices;
   wl->devices.UnAllocate();
   rams    = wl->rams;
   wl->rams.UnAllocate();

   for (i=0; i<devices.size(); i++)
      {
      const devicetype &d = devices[i];
      nodes[d.gate].capacitance   += d.GateCap();
      nodes[d.source].capacitance += d.DiffCap();
      nodes[d.drain].capacitance  += d.DiffCap();
      if (d.width <=0.0)
         {
         printf("Device %s has a non-positive device width",d.name);
         FATAL_ERROR;
         }
      }
   for (i=0; i<rams.size(); i++)
      {
      const ramtype &r = rams[i];

      nodes[r.wl].capacitance += leakSigs[r.leakSigIndex].pass.GateCap() * 2;
      nodes[r.bh].capacitance += leakSigs[r.leakSigIndex].pass.DiffCap();
      nodes[r.bl].capacitance += leakSigs[r.leakSigIndex].pass.DiffCap();
      }
   }


void circuittype::Parse()
   {                                                   
   TAtype ta("simtype::Parse()");
   int i,index;   

   printf("\nCross referencing nodes/devices\n");
   for (i=0; i<nodes.size(); i++)
      {
      nodetype &n = nodes[i];
      n.gates.clear();
      n.sds.clear();
      }
   for (i=devices.size()-1; i>=0; i--)
      {
      const devicetype &d = devices[i];
      if (d.drain==d.source)
         {
         nodes[d.gate].capacitance   += d.GateCap();
	     nodes[d.source].capacitance += d.DiffCap();
	     nodes[d.drain].capacitance  += d.DiffCap();
         devices.RemoveFast(i);  // delete devices with source=drain (14nm antenna diodes do this)
         }
      }
 
   for (i=0; i<devices.size(); i++)
      {
      const devicetype &d = devices[i];
      if ((index=d.source)>=2)
         {
         nodes[index].sds.push(&devices[i]);         
         }
      if ((index=d.drain)>=2)
         {
         nodes[index].sds.push(&devices[i]);         
         }
      index=devices[i].gate;
      nodes[index].gates.push(&devices[i]);
      }
 
   for (i=0; i<devices.size(); i++)
      {
      devicetype &d = devices[i];
     
      if (nodes[d.source].sds.size()==1 && nodes[d.source].gates.size()==0)
         d.gate = d.type==D_PMOS ? VDD : VSS;  // tie off the dummy gate to suppress floating checks
      if (nodes[d.drain].sds.size()==1 && nodes[d.drain].gates.size()==0)
         d.gate = d.type==D_PMOS ? VDD : VSS;  // tie off the dummy gate to suppress floating checks
      }
   for (i=0; i<rams.size(); i++)
      {
      ramtype &r = rams[i];

      nodes[r.bh].rams_bl.push(i);
      nodes[r.bl].rams_bl.push(i);
      nodes[r.wl].rams_wl.push(i);
      }
   for (i=0; i<nodes.size(); i++)
      {
      nodetype &n = nodes[i];

      n.sds.RemoveDuplicates();
      n.gates.RemoveDuplicates();
      }
   if (nodes[VSS].sds.size()) FATAL_ERROR;
   if (nodes[VDD].sds.size()) FATAL_ERROR;
   }

void circuittype::ReadBboxFile(const char *filename){
  filetype fp;
  fp.fopen(filename,"rt");
  if (!fp.IsOpen())
    {
      printf("\nI couldn't open %s for reading.\n",filename);
      FATAL_ERROR;
    }
  printf("\nReading blackboxes from %s \n", filename);

  char buffer[2048];
  while (fp.fgets(buffer, sizeof(buffer)-5) != NULL){
    char* chptr=strchr(buffer,'\n');
    if (chptr!=NULL)
      *chptr=0;
    chptr=strchr(buffer,' ');
    if (chptr!=NULL)
      *chptr=0;
    if (buffer!=NULL){
      bboxes.push(stringPool.NewString(buffer));
    }
  }
  fp.fclose();
}

void circuittype::MarkAllBboxStructures(){
  extern arraytype<wirelisttype*> structures;
  for (int s=0; s<structures.size(); s++){
    for (int b=0; b<bboxes.size(); b++){
      if (strcmp(bboxes[b], structures[s]->name)==0){
	structures[s]->is_bbox=true;
      }
    }
  }
}

void circuittype::AssignFlopOwnersForNodes(){
  TAtype ta("circuittype::AssignFlopOwnersForNodes()");
  printf("AssignFlopOwnersForNodes\n");
  int nodeswithowners=0;
  int max_levels=20;
  arraytype<int> nodelist;
  for (int i=VDD+1; i<nodes.size(); i++)
    {
      nodetype &n = nodes[i];
      arraytype<nodetype> nodestoinclude;
      int owner_flop_index=-1;
      nodestoinclude.push(n);
      //At each level do a breadth first search. i.e. at each level construct a list of nodes to search.
      //Then for each node in the list get all SD connected devices into a devices list
      //and search these devices is a flop device. If we hit, done for this node. else, create the node list for the next level.
      int level=0;
      while ((owner_flop_index==-1) && (level<max_levels)){
	arraytype<devicetype*> devlist;
	for (int z=0; z<nodestoinclude.size(); z++){
	  for (int d=0; d<nodestoinclude[z].sds.size(); d++){
	    devlist.push(nodestoinclude[z].sds[d]);
	  }
	}
	if(devlist.size()==0){
	  break;
	}
	owner_flop_index=SearchListForFlopDevice(devlist);
	if (owner_flop_index !=-1){
	  n.m_flop_owner=flops[owner_flop_index];
	  nodeswithowners++;
	  level=0;
	}
	else {
	  nodestoinclude.clear();
	  for (int d=0; d<devlist.size(); d++){
	    nodetype gatenode=nodes[devlist[d]->gate];
	    nodestoinclude.push(gatenode);
	    level++;
	  }
	}
      }//while
    }//nodes[i]
  printf("%d nodes were assigned flop owners\n", nodeswithowners);
  //Assign the remaining as unknown.
  int to_an_unknown_flop_index;
  if (flophash.Check("UnKnown", to_an_unknown_flop_index)){
    for (int i=VDD+1; i<nodes.size(); i++)
      {
	nodetype &n = nodes[i];
	if (n.m_flop_owner==NULL){
	  n.m_flop_owner=flops[to_an_unknown_flop_index];
	  // I could have used flops.back(), but if the unknown gets to be several we need a lookup.
	}
      }
  }
}
void circuittype::PopulateFlopsHash(){
    TAtype ta("circuittype::AssignFlopOwnersForNodes()");
    for (int i=0; i< devices.size(); i++){
      if (devices[i].m_flipflop_device){
	const char* devname=devices[i].name;
	char rootname[512];
	strcpy(rootname, devname);
	//Eg: devname=ap_mem__cim_crb_req_reg/MMPQ_2
	char* chptr=strchr(rootname,'/');
	if(chptr!=NULL)
	  *chptr='\0';
	int flopindex;
	if (!flophash.Check(rootname,flopindex)){
	  FlipFlop *f=new FlipFlop(const_cast<char*>(rootname));
	  flops.push(f);
	  flopindex=flops.size()-1;
	  flophash.Add(rootname, flopindex);
	}
      }
    }
    //Add a last one called Unknown
    FlipFlop *to_an_unknown_flop=new FlipFlop("UnKnown");
    flops.push(to_an_unknown_flop);
    flophash.Add("UnKnown", flops.size()-1);
}
int circuittype::SearchListForFlopDevice(arraytype<devicetype*>& devlist)
{
  for (int i=0; i< devlist.size(); i++){
    if (devlist[i]->m_flipflop_device){
      char rootname[512];
      strcpy(rootname, devlist[i]->name);
      char* chptr=strchr(rootname, '/');
      if (chptr!=NULL){
	*chptr='\0';
	int flopindex;
	if(flophash.Check(rootname, flopindex)){
	  return flopindex;
	}
	else{
	  FATAL_ERROR;
	}
      }
    }
  }
  return -1;
}
