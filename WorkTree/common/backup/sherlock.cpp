#include "pch.h"
#include "template.h"
#include "multithread.h"
#include "circuit.h"
#include "wirelist.h"

#ifdef _MSC_VER
 #pragma optimize("",off)
#endif





const int MAX_SIGLENGTH        = 2048;  // maximum number of bits a bus can have
const int INFINITE_LOOP        = 75;    // number of gate delay in a time slice before logic is counted as oscillator
      int MAX_ERRORS           = 250;

/* These are .13um numbers      
const float NGATE_CAPACITANCE    = (float)2.34;   // FF / um channel width. Channel length is ignored .
const float NDIFF_CAPACITANCE    = (float)0.75;   // FF / um channel width.
const float PGATE_CAPACITANCE    = (float)2.18;   // FF / um channel width. Channel length is ignored .
const float PDIFF_CAPACITANCE    = (float)0.75;   // FF / um channel width.
const float POWER_VOLTAGE        = (float)1.1;
*/

/* These are 90nm numbers */
double NGATE_CAPACITANCE    = 1.5;   // FF / um channel width. Channel length is ignored .
double NDIFF_CAPACITANCE    = 0.5;   // FF / um channel width.
double PGATE_CAPACITANCE    = 1.5;   // FF / um channel width. Channel length is ignored .
double PDIFF_CAPACITANCE    = 0.5;   // FF / um channel width.
const double POWER_VOLTAGE        = 1.2;
const double MIN_LENGTH = 0.10;
// These values are nA/um @1.2V 100C
const interpolatetype leaktable_highN[] = {{0.2, 105}, {0.4,  90}, {0.6,  90}, {1.0,  92}, {2.0, 120}, {4.0, 135}, {0.0, 0.0}};
const interpolatetype leaktable_highP[] = {{0.2,  35}, {0.4,  42}, {0.6,  54}, {1.0,  67}, {2.0, 115}, {4.0, 142}, {0.0, 0.0}};
const interpolatetype leaktable_nomN[]  = {{0.2, 245}, {0.4, 245}, {0.6, 245}, {1.0, 245}, {2.0, 335}, {4.0, 395}, {0.0, 0.0}};
const interpolatetype leaktable_nomP[]  = {{0.2, 155}, {0.4, 220}, {0.6, 280}, {1.0, 370}, {2.0, 555}, {4.0, 670}, {0.0, 0.0}};
const interpolatetype leaktable_lowN[]  = {{0.2, 420}, {0.4, 410}, {0.6, 450}, {1.0, 440}, {2.0, 655}, {4.0, 810}, {0.0, 0.0}};
const interpolatetype leaktable_lowP[]  = {{0.2, 245}, {0.4, 360}, {0.6, 460}, {1.0, 585}, {2.0, 890}, {4.0, 1060}, {0.0, 0.0}};
const interpolatetype leaktable_sramN[] = {{0.1, 4},   {0.2, 4},   {0.0, 0.0}};
const interpolatetype leaktable_sramP[] = {{0.1, 25},  {0.2, 25},  {0.0, 0.0}};

/**/


double InterpolateLeakage(double width, double length, const interpolatetype *model)
   {
   int i;
   double leak=0.0;
   if (width<=0.0) FATAL_ERROR;
   
   i=0;
   while (model[i].sample < width && model[i].sample>0.0)
      {
      i++;
      }
   if (model[i].sample<=0.0)
      return model[i-1].value*width;   // width is greater than table
   if (i==0)
      return model[0].value*width;     // width is less than table
   
   double slope = (model[i].value - model[i-1].value) / (model[i].sample - model[i-1].sample);
   
   leak = (width-model[i-1].sample)* slope + model[i-1].value;

   if ((int)(length*100) != (int)(MIN_LENGTH*100))
	  {
	  if (model!=leaktable_highN && model!=leaktable_highP)
		 {
//		 printf("You have a non-minimum channel length on a device that isn't high vt.\n");
		 return leak;
		 }
	  // for long channel lengths I'm just doing a simple exponential. Every .01 cuts leakage in half
	  leak = leak  / exp (6.93*(length-MIN_LENGTH)/MIN_LENGTH );
	  }
   return leak;
   };


                        
struct matchtype{
   const char *behname;
   const char *wirename;
   array <int> sigIndex;     // index into input.sig[] list, if size()>1 then ignore lsb,msb verilog exploded a bus
   int lsb, msb;
   array <int> nodelist;  // will go from lsb to msb
   int timing; // I will push this number as the start time for the signal
   unsigned complement :1;
   unsigned input :1;
   unsigned checka :1;
   unsigned checkb :1;
   unsigned inerror :1;
   };


class aliastype{
public:
   const char *name;
   const char *hierarchy;
   int sigIndex;
   int bit;     // this is necessary when aliases change the bit subscript foo[8:4] -> foo[4:0]
   aliastype(const char *_name, const char *_hierarchy, int _index, int _bit) : name(_name), hierarchy(_hierarchy), sigIndex(_index), bit(_bit)
      {}
   aliastype() : name(NULL), hierarchy(NULL)
      {FATAL_ERROR;}  // makes sun compiler happy
   };

class vcdSigtype{
public:
   int msb, lsb;
   char ref[5];
   int id;
   int changed;    // this needs be a variable which atomic writes will happen
   bitfieldtype <MAX_SIGLENGTH> binarystate;
   bitfieldtype <MAX_SIGLENGTH> xstate;   
   array <int> nodes;       // indexes into my node list. Starting lsb,msb (only for outputting vcd's)

   vcdSigtype(int _msb, int _lsb, const char *_ref, int _id) : msb(_msb), lsb(_lsb), id(_id)
      {
      memcpy(ref, _ref, sizeof(ref));
      }
   vcdSigtype()      // this makes sun compiler happy
      {FATAL_ERROR;} 
   };

// this class will read/write vcd files
class vcdtype{
   filetype fp;
   int line;
   char filename[256];
   hashtype <int, int> idxref;
public:
   array <vcdSigtype> sigs;
   array <aliastype> aliases;
   __int64 currentTime, nextTime;
   double multiplier;

   vcdtype()
      {
      line = 0;
      multiplier = 1.0;
      currentTime = -2;
      }
   ~vcdtype()
      {
      if (fp.IsOpen())
         fp.fclose();
      }
   void OpenForWrite(const char *filename, array <nodetype> &nodes);
   void WriteTimeSlice(int currentTime, const array <nodetype> &nodes);
   void OpenForRead(const char *filename);
   bool ReadTimeSlice();    // returns false when file is exhausted   
   bool IsReadyToWrite() const
      {
      return fp.IsOpen();
      }
   };



class simtype : public circuittype{
public:
   int HARD_ERROR, SOFT_ERROR;
   vcdtype inputVcd;
   vcdtype outputVcd;
   array <matchtype> matches;
   bool fineResolution, noDump, noFloatingChecks;
   int compareTime, dumpTime;
   int leakageIntervals;

   simtype()
      {
      HARD_ERROR = 0;
      SOFT_ERROR = 0;
      fineResolution = false;
      noFloatingChecks = false;
      noDump = false;
      compareTime = 20;
	  dumpTime = -1;
      leakageIntervals = 0;
      }
   void Log(const char *fmt, ...)
      {
      va_list args;
      char buffer[1024];   
      
      va_start(args, fmt);
      int count = vsprintf(buffer, fmt, args);
      va_end(args);
      
      printf(buffer);
      }
   void ErrorLog(const char *fmt, ...)
      {
      va_list args;
      char buffer[1024];   
      
      va_start(args, fmt);
      int count = vsprintf(buffer, fmt, args);
      va_end(args);
      
      if (noFloatingChecks && strstr(buffer, " is floating.")!=NULL)
         return;  // suppress floating node check
      HARD_ERROR++;
      printf(buffer);
      }
   int Work(int argc, char *argv[]);
   void Simulate();
   void ReadDescr(char *filename);
   void IntermediateNodeDumping(int timestep)
      {
      if (fineResolution && outputVcd.IsReadyToWrite())
         outputVcd.WriteTimeSlice(inputVcd.currentTime + MAXIMUM(timestep+5,0), nodes);
      }
   double SumPower() // returns total switched cap
      {
      TAtype ta("SumPower()");
      double accum = 0.0;
      for (int i=0; i<nodes.size(); i++)
         if (nodes[i].toggleCount)
            {
            accum += nodes[i].toggleCount * nodes[i].capacitance;
            }
      return accum;
      }
   double SumLeakage(bool force_high=false, bool force_nominal=false)
      {
      TAtype ta("SumLeakage()");
      double accum=0.0;
      
      for (int i=0; i<devices.size(); i++)
         {
         devicetype &d = devices[i];
         if (d.type==D_NMOS)
            {
            if ((d.vt==VT_HIGH || force_high) && !force_nominal)
               accum += InterpolateLeakage(d.width, d.length, leaktable_highN) * d.offcount;
            else if (d.vt==VT_LOW && !force_nominal)
               accum += InterpolateLeakage(d.width, d.length, leaktable_lowN) * d.offcount;
            else if (d.vt==VT_SRAM && !force_nominal)
               accum += InterpolateLeakage(d.width, MIN_LENGTH, leaktable_sramN) * d.offcount;
            else
               accum += InterpolateLeakage(d.width, d.length, leaktable_nomN) * d.offcount;
            }
         else
            {
            if ((d.vt==VT_HIGH || force_high) && !force_nominal)
               accum += InterpolateLeakage(d.width, d.length, leaktable_highP) * d.offcount;
            else if (d.vt==VT_LOW && !force_nominal)
               accum += InterpolateLeakage(d.width, d.length, leaktable_lowP) * d.offcount;
            else if (d.vt==VT_SRAM && !force_nominal)
               accum += InterpolateLeakage(d.width, MIN_LENGTH, leaktable_sramP) * d.offcount;
            else
               accum += InterpolateLeakage(d.width, d.length, leaktable_nomP) * d.offcount;
            }
         if (d.offcount<0) FATAL_ERROR;
         }
      return accum / 1000000.0 ;
      }
   void PowerReport();
   void LeakageReport();
   void LeakageEstimate();
   };


    
int Work(int argc, char *argv[])
   {
   simtype sim;
   
   return sim.Work(argc, argv);
   }


int simtype::Work(int argc, char *argv[])
   {
   TAtype ta("Sherlock.exe main()");
   char filename[200]="";
   char vcdname[200]="";
   char descrname[200]="";
   char outputname[200]="sherlock_out.dmp.gz";
   char capname[200]="";
   int i;
   bool help=false, leakonly=false, found;
   wirelisttype *wl = NULL;

   for (i=1; i<argc; i++)	   
      {
      if (0==stricmp(argv[i],"help") || 0==stricmp(argv[i],"-help"))         
         {
         help = true;
         argc--;
         }
      if (help)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1; i<argc; i++)	   
      {
      if (0==stricmp(argv[i],"leakonly") || 0==stricmp(argv[i],"-leakonly"))         
         {
         leakonly = true;
         argc--;
         }
      if (leakonly)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1; i<argc; i++)	   
      {
      if (0==stricmp(argv[i],"fine") || 0==stricmp(argv[i],"-fine"))         
         {
         fineResolution = true;
         argc--;
         }
      if (fineResolution)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1; i<argc; i++)	   
      {
      if (0==stricmp(argv[i],"nodump") || 0==stricmp(argv[i],"-nodump"))         
         {
         noDump = true;
         argc--;
         }
      if (noDump)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1; i<argc; i++)	   
      {
      if (0==stricmp(argv[i],"nofloat") || 0==stricmp(argv[i],"-nofloat"))         
         {
         noFloatingChecks = true;
         argc--;
         }
      if (noFloatingChecks)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1, found=false; i<argc; i++)	   
      {
      if (0==strnicmp(argv[i],"comparetime=",12) || 0==strnicmp(argv[i],"-comparetime=",13))
         {
         compareTime = atoi(strchr(argv[i],'=')+1);
		 found = true;
         argc--;
         argv[i] = argv[i+1]; // won't exceed array because argc-- above
         }
      if (found)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1, found=false; i<argc; i++)	   
      {
      if (0==strnicmp(argv[i],"dumptime=",8) || 0==strnicmp(argv[i],"-dumptime=",9))
         {
         dumpTime = atoi(strchr(argv[i],'=')+1);
		 found = true;
         argc--;
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
         }
      if (found)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1, found=false; i<argc; i++)	   
      {
      if (0==strnicmp(argv[i],"maxerrors=",10) || 0==strnicmp(argv[i],"-maxerrors=",11))
         {
         MAX_ERRORS = atoi(strchr(argv[i],'=')+1);
		 found = true;
         argc--;
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
         }
      if (found)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1; i<argc; i++)	   
      {
      if (argv[i][0]=='-')
         {
         printf("\nI didn't understand command line parameter '%s'\n",argv[i]);
         exit(-1);
         }
      }

   if (argc==2)
      {
      if (NULL==strchr(argv[1],'.'))
         {
         strcpy(filename,argv[1]);
         strcpy(descrname,argv[1]);
         strcpy(vcdname,argv[1]);
         argc=0;
         printf("$Sherlock %s.hsp %s.descr %s.dmp", filename, descrname, vcdname);
         }
      }
   for (i=1; i<argc; i++)          
      {
      if (argv[i]!=NULL && i==1)
         strcpy(filename,argv[i]);                     
      if (argv[i]!=NULL && i==2)
         strcpy(descrname,argv[i]);                     
      if (argv[i]!=NULL && i==3)
         strcpy(vcdname,argv[i]);                     
      if (argv[i]!=NULL && i==4)
         strcpy(capname,argv[i]);                     
      }

#ifdef _MSC_VER
   strcpy(filename,"z:\\sherlock\\pp_l1i_bnk_debug.nt");
   strcpy(descrname,"z:\\sherlock\\pp_l1i_bnk.descr");
   strcpy(vcdname,"z:\\sherlock\\waves.vcd");
   compareTime = 2000;
//   MAX_ERRORS = 1;
//   fineResolution = true;
//   strcpy(capname,"z:\\v\\pp_issue.star");
   noDump = true;
#endif

   if (argc<=1 || help)
	  {
	  printf("\nSherlock usage:\nSherlock [-leakonly][-fine][-nodump][-nofloat][-comparetime=#][-dumptime=#][-maxerrors=#] foo.nt foo.descr foo.vcd [foo.star]\n");
	  printf("The -leakonly option won't require a descript or vcd file.\n");
	  }
   if (filename[0]==0)
      {
      printf("\nEnter a .hsp wirelist or .nt filename->");
      gets(filename);
      }    
   if (descrname[0]==0 && !leakonly)
      {
      printf("\nEnter a descript filename->");
      gets(descrname);
      }    
   if (vcdname[0]==0 && !leakonly)
      {
      printf("\nEnter a input trace filename->");
      gets(vcdname);
      }    
   
   if (strstr(filename,".hsp")!=NULL)
      ReadSpiceWirelist(filename);
   else
      {
      array <const char *> vssNames, vddNames;
	  
	  vssNames.push("vss");
	  vssNames.push("vssa");
	  vddNames.push("vdda");
	  vddNames.push("vddo");
	  vddNames.push("vddx");      
      wl = ReadWirelist(filename, vddNames, vssNames);
      if (wl==NULL)
         {
         printf("\nI couldn't open %s",filename);
         shutdown(-1);
         }
      FlattenWirelist(wl);
      }
   if (capname[0]!=0)
      ReadCapfile(capname);

   filetype fp;
   fp.fopen("signals.txt.gz", "wt");
   for (i=0; i<nodes.size(); i++)
      {
      char buf2[512], *chptr;
      nodes[i].RName(buf2);
//      strlwr(buf2);
      while ((chptr = strchr(buf2,'$')) !=NULL)
         *chptr = '/';
      fp.fprintf("\n%d %s %.1ffF",i,buf2,nodes[i].capacitance);
      }
   fp.fclose();

   Parse();
   ZeroNodes();

   if (leakonly)
	  {
	  printf("\n");
	  double leak, leakNom, leakHigh;
      
	  noFloatingChecks = true;
	  Initialize();
	  LeakageEstimate();

	  leak     = SumLeakage();
	  leakNom  = SumLeakage(false, true);
	  leakHigh = SumLeakage(true, false);
	  printf("\n Estimated Leakage=%.4gmA(%.4gmA Nominal, %.4gmA High).", leak/leakageIntervals, leakNom/leakageIntervals, leakHigh/leakageIntervals);
	  LeakageReport();
	  return -1;
	  }

   
   if (vcdname[0]==0)
      {
      printf("\nI need a trace file(vcd) to run!");
      shutdown(-1);
      }
   inputVcd.OpenForRead(vcdname);
//   if (fineResolution)
//      inputVcd.multiplier *=1000;
   if (descrname[0]==0)
      {
      printf("\nI need a descript file to run!");
      return -1;
      }
   ReadDescr(descrname);

   Initialize();              // initialize all nodes to legal values. (ie output of inverter is compliment of input)
   if (!noDump)
      {
      outputVcd.OpenForWrite(outputname, nodes);
      outputVcd.WriteTimeSlice(-1, nodes);
      }

   Simulate();
   printf("\n");
   double leak, leakNom, leakHigh;
   leak     = SumLeakage();
   leakNom  = SumLeakage(false, true);
   leakHigh = SumLeakage(true, false);
   printf("\n Total switched capacitance = %.4g fF, Average Leakage=%.4gmA(%.4gmA Nominal, %.4gmA High), %5.0f ns simulated.", SumPower()/1000.0, leak/leakageIntervals, leakNom/leakageIntervals, leakHigh/leakageIntervals, inputVcd.currentTime/1000.0);
   
   PowerReport();
   LeakageReport();

   if (!HARD_ERROR && !SOFT_ERROR)
      {
      printf("\n>>>No errors/warnings found.");
      return 0;
      }
   else if (HARD_ERROR>=MAX_ERRORS)
      printf("\n>>>Simulation ended due to exceeding the max error count(%d)!",MAX_ERRORS);
   else if (HARD_ERROR)
      printf("\n>>>%d errors, %d warnings were found!!!!!!!",HARD_ERROR,SOFT_ERROR);
   else if (SOFT_ERROR)
      {
      printf("\n>>>%d warnings were found.",SOFT_ERROR);
      return 0;
      }
   else
      FATAL_ERROR;  

   return -1;     
   }    
                             

void simtype::Simulate()
   {
   TAtype ta("simtype::Simulate()");
   int i, k, bit, row;
   int oldtime=0;
   double priorPower = 0.0, priorLeakage = 0.0;
   int priorIntervals=0;
   bool compare=false;
   int clknode=-1;
   int randomseed=5;
                                                                                  
   for (i=0; i<matches.size(); i++)
      {
      if (matches[i].input && matches[i].timing<0 && clknode<0)
         clknode = matches[i].nodelist[0];
      if (matches[i].input && matches[i].sigIndex.size()==0)
         for (k=0; k<matches[i].nodelist.size(); k++)
            {
            int index = matches[i].nodelist[k];
            if (stricmp(matches[i].behname, "vss")==0)
               nodes[index].value = V_ZERO;
            else if (stricmp(matches[i].behname, "vdd")==0)
               nodes[index].value = V_ONE;
            else if (stricmp(matches[i].behname, "vddo")==0)
               nodes[index].value = V_ONE;
            else
               FATAL_ERROR;
            nodes[index].constant = true;
            nodes[index].changed  = true;
            nodes[index].inflight = true;
            nodeheap.push(index, 0);
            }
      }   
   
   printf("\nClocking");
   for (row=0; HARD_ERROR<MAX_ERRORS; row++)
      {
      if (!inputVcd.ReadTimeSlice())
         break;
      const int statusWindow = 1000000; // in picoseconds
      if (random(randomseed)<0.01)
         LeakageEstimate();  // Only sample leakage every so often so it doesn't slow everything down
      if (inputVcd.currentTime>=1451000)
		 printf("");
	  if (inputVcd.currentTime/statusWindow != oldtime/statusWindow)
         {
         printf("\nSimulating past time %d ns.", inputVcd.currentTime/1000);
         double deltaPower   = SumPower()   - priorPower;
         double deltaLeakage = SumLeakage() - priorLeakage;
         int   deltaIntervals = leakageIntervals - priorIntervals;
         printf(" P=1/2CV^2, C=%8.3g fF/ps, P(@%.1fV) = %.2fmW, Ioff=%.2fmA",deltaPower*1.0 / statusWindow,POWER_VOLTAGE,0.5*POWER_VOLTAGE*POWER_VOLTAGE*deltaPower/statusWindow, deltaLeakage/deltaIntervals);
         priorPower     += deltaPower;
         priorLeakage   += deltaLeakage;
         priorIntervals += deltaIntervals;
         }
      for (i=0; i<matches.size(); i++)
         {
         if (matches[i].input && matches[i].sigIndex.size())
            {
            for (bit=matches[i].lsb; bit<=matches[i].msb; bit++)
               {
               const vcdSigtype &s = inputVcd.sigs[matches[i].sigIndex[matches[i].sigIndex.size()!=1 ? bit-matches[i].lsb : 0]];
               int index = matches[i].nodelist[bit-matches[i].lsb];
               int offset = bit - s.lsb;
               if (matches[i].sigIndex.size()>1)
                  offset = 0;
               if (s.binarystate.GetBit(bit - s.lsb) ^ matches[i].complement)
                  nodes[index].value = V_ONE;
               else
                  nodes[index].value = V_ZERO;
               nodes[index].constant = true;
               nodes[index].changed  = true;
               nodes[index].inflight = true;
               if (nodes[index].outputSigChangedPtr != NULL)
                  *nodes[index].outputSigChangedPtr = 1;
               nodeheap.push(index, matches[i].timing);
               }
            }
         }                                      
//      Propogate(row < 50);  // ignore floating nodes/ratioed logic the first 2 clock cycles
      bool compareEnable = inputVcd.currentTime/1000 > compareTime;
      if (compare != compareEnable)
         printf("\nEnabling short, floating, metastable node checks and enabling compares at time %dns",inputVcd.currentTime/1000);
      compare = compareEnable;
      Propogate(inputVcd.currentTime/1000, !compareEnable);

      if (!fineResolution && outputVcd.IsReadyToWrite() && inputVcd.currentTime/1000 > dumpTime)
         outputVcd.WriteTimeSlice(inputVcd.currentTime, nodes);

      for (i=0; i<matches.size() && compareEnable; i++)
         {
         unsigned __int64 beh=0, wire=0, wire_l=0;
         matchtype &match = matches[i];
         
         if (!match.input)
            {   
            bitfieldtype <MAX_SIGLENGTH> wirestate=0, behstate=0, mask=0;
            char buffer[2048];
            
            for (k=0; k<match.nodelist.size(); k++)
               {                                  
               int index = match.nodelist[k];
               vcdSigtype &sig = inputVcd.sigs[match.sigIndex[match.sigIndex.size()!=1 ? k:0]];
               int offset = k + match.lsb - sig.lsb;
               if (match.sigIndex.size() >1)
                  offset = 0;

               if ((nodes[index].value == V_ONE) ^ match.complement)
                  wirestate.SetBit(k);
               if (sig.binarystate.GetBit(offset))
                  behstate.SetBit(k);
               if (sig.xstate.GetBit(offset))
                  mask.SetBit(k);
               }

            bool compare=true;
            if (clknode>0 && !match.checka && nodes[clknode].value==V_ONE)
               compare = false;
            if (clknode>0 && !match.checkb && nodes[clknode].value==V_ZERO)
               compare = false;
            if ((wirestate | mask) != (behstate | mask) && compare)
               {
               if (!match.inerror)
                  {
                  const char *chptr = strrchr(match.behname, '.');
                  char buf2[512];
                  if (chptr==NULL)
                     chptr = match.behname;
                  if (match.lsb>=0)
                     sprintf(buf2,"%s[%d:%d]",chptr,match.msb,match.lsb);
                  else
                     strcpy(buf2,chptr);
                  printf("\n\nMismatch at time %6d", inputVcd.currentTime/1000);                  
                  printf("\nBehavioral signal %-40s = %17s", buf2, behstate.Print(buffer)); 
                  printf("\nTransistor signal %-40s =%c%17s", match.wirename, match.complement?'~':' ', wirestate.Print(buffer)); 
                  printf("\nDon't care mask   %-40s   %17s", "", mask.Print(buffer)); 
                  HARD_ERROR++;
                  }
               match.inerror = true;
               }
            else if (match.inerror)
               {
               printf("\n\n**Signals now match at time %6d", inputVcd.currentTime/1000);
               printf("\nTransistor signal %-40s =%c%17s", match.wirename, match.complement?'~':' ', wirestate.Print(buffer)); 
               printf("\nDon't care mask   %-40s   %17s", "", mask.Print(buffer)); 
               match.inerror = false;
               }
            }   
         }
      if (HARD_ERROR > MAX_ERRORS)
         break;
      oldtime = inputVcd.currentTime;
      }                                        
   }                  


void simtype::LeakageEstimate()
   {
   TAtype ta("LeakageEstimate()");
   int i, n;
   array <int> statenodes;

   // this isn't terribly fast but it doesn't get called often either
   leakageIntervals++;
   for (n=VDD+1; n<nodes.size(); n++)
      {
      if (!nodes[n].state)
         nodes[n].value = V_X;
      }
   for (n=VDD+1; n<nodes.size(); n++)
      {
      if (nodes[n].value==V_X)
         {
         bool drive0=false, drive1=false;
         Eval(n,drive0, drive1, statenodes);
         if (drive0 && !drive1)
            nodes[n].value = V_ZERO;
         if (!drive0 && drive1)
            nodes[n].value = V_ONE;
         }
      }
   // now every node is marked as 0, 1, Z. Simply find all OFF devices with a 0/1 on each side
   for (i=0; i<devices.size(); i++)
      {
      devicetype &d=devices[i];

      if (!d.on && nodes[d.source].value != nodes[d.drain].value && nodes[d.source].value!=V_X && nodes[d.drain].value!=V_X)
         d.offcount++;
      }
   }

class devicesorttype{
public:
   const char *name;
   array <int> indices;
   short lsb, msb;
   short len;
   bool deleted;
   double leakage;

   devicesorttype(const char *_name, int _index, double _leakage=0.0) : name(_name), leakage(_leakage)
      {
      const char *chptr = strchr(name, '[');
      lsb = -1;
      msb = -1;
      
      indices.push(_index);

      if (chptr!=NULL)
         {
         len = chptr-name;
         lsb = msb = atoi(chptr+1);
         chptr = strchr(chptr, ':');
         if (chptr!=NULL)
            lsb = atoi(chptr+1);
         }
      else
         len = strlen(name);
      deleted = false;
      }
   devicesorttype() : name(NULL)
      {FATAL_ERROR;}  // makes sun compiler happy
   bool operator<(const devicesorttype &right) const
      {
      const char *a = name, *b = right.name;
      int anglea=0, angleb=0;
      do{
         if (*a == '[' && *b == '[')
            {
            a++, b++;
            while (*a>='0' && *a<='9')
               {
               anglea = anglea*10 + *a-'0';
               a++;
               }
            while (*b>='0' && *b<='9')
               {
               angleb = angleb*10 + *b-'0';
               b++;
               }
            }
         if (*a <'0' || *a >'9' || *b <'0' || *b >'9')
            {
            if (*a < *b) return true;
            if (*a > *b) return false;
            a++;
            b++;
            }
         else
            {
            int numa=0, numb=0;
            
            while (*a>='0' && *a<='9')
               {
               numa = numa*10 + *a-'0';
               a++;
               }
            while (*b>='0' && *b<='9')
               {
               numb = numb*10 + *b-'0';
               b++;
               }
            if (numa < numb) return true;
            if (numa > numb) return false;
            }
         }while (*a != 0 || *b != 0);
      return anglea < angleb;
      }
   };

class leakagePredicatetype{
public:
   bool operator()(const devicesorttype &x, const devicesorttype &y) const
      {
      return x.leakage > y.leakage;
      }
   };
void simtype::LeakageReport()
   {
   TAtype ta("Dumping leakage report");
   array <devicesorttype> devicesorts;
   int i;
   const char *chptr, *lastchptr;
   filetype fp;
   double total=0.0;

   // I now want to compile a list of signals from my wirelist. I'll only get things that touch a gate
   for (i=0; i<devices.size(); i++)
      {
      devicetype &d = devices[i];
      if (d.offcount)
         {
         double leakage;
         if (d.type==D_NMOS)
            {
            if (d.vt==VT_HIGH)
               leakage = InterpolateLeakage(d.width, d.length, leaktable_highN) * d.offcount;
            else if (d.vt==VT_LOW)
               leakage = InterpolateLeakage(d.width, d.length, leaktable_lowN) * d.offcount;
            else if (d.vt==VT_SRAM)
               leakage = InterpolateLeakage(d.width, MIN_LENGTH, leaktable_sramN) * d.offcount;
            else
               leakage = InterpolateLeakage(d.width, d.length, leaktable_nomN) * d.offcount;
            }
         else
            {
            if (d.vt==VT_HIGH)
               leakage = InterpolateLeakage(d.width, d.length, leaktable_highP) * d.offcount;
            else if (d.vt==VT_LOW)
               leakage = InterpolateLeakage(d.width, d.length, leaktable_lowP) * d.offcount;
            else if (d.vt==VT_SRAM)
               leakage = InterpolateLeakage(d.width, MIN_LENGTH, leaktable_sramP) * d.offcount;
            else
               leakage = InterpolateLeakage(d.width, d.length, leaktable_nomP) * d.offcount;
            }
         leakage = leakage / leakageIntervals;
         devicesorts.push(devicesorttype(stringPool.NewString(devices[i].name), i, leakage));
         total += leakage;
         }
      }
   if (devicesorts.size()==0) return;
   devicesorts.Sort();

   // now I want to merge nodes into busses
   int last = 0;
   lastchptr = strchr(devicesorts[0].name, ']');
   for (i=0; i<devicesorts.size(); i++)
      {
      chptr = strchr(devicesorts[i].name, ']');
      if (lastchptr != NULL && chptr != NULL && strcmp(lastchptr, chptr)==0 &&
          strncmp(devicesorts[last].name, devicesorts[i].name, devicesorts[i].len)==0 &&
          devicesorts[last].len == devicesorts[i].len && devicesorts[last].msb == devicesorts[i].lsb-1)
         {
         devicesorts[last].msb = devicesorts[i].msb;
         devicesorts[last].indices += devicesorts[i].indices;
         devicesorts[last].leakage += devicesorts[i].leakage;
         devicesorts[i].deleted = true;
         }
      else
         {
         last = i;
         lastchptr = strchr(devicesorts[i].name, ']');
         }
      }
   std::sort(devicesorts.begin(), devicesorts.end(), leakagePredicatetype());  // now sorted by power
   fp.fopen("leakage.txt.gz","wt");
//   fp.fopen("leakage.txt","wt");
   for (i=0; i<devicesorts.size(); i++)
      {
      if (!devicesorts[i].deleted && devicesorts[i].leakage>0.0)
         {
         if (devicesorts[i].msb > devicesorts[i].lsb)
            {
            char buffer[1024];
            strcpy(buffer, devicesorts[i].name);            
            chptr = strchr(devicesorts[i].name, ']');
            if (chptr==NULL) FATAL_ERROR;
            sprintf(buffer+devicesorts[i].len,"[%d:%d]%s",devicesorts[i].msb,devicesorts[i].lsb,chptr+1);
            fp.fprintf("%8.5f%% %10.2e %s\n",(double)100.0*devicesorts[i].leakage/total,(double)devicesorts[i].leakage,buffer);
            }
         else
            fp.fprintf("%8.5f%% %10.2e %s\n",(double)100.0*devicesorts[i].leakage/total,(double)devicesorts[i].leakage,devicesorts[i].name);
         }
      }   
   fp.fclose();
   }


class nodesorttype{
public:
   const char *name;
   array <int> indices;
   short lsb, msb;
   short len;
   bool deleted;
   double power;

   nodesorttype(const char *_name, int _index, double _cap=0.0, double _toggle=0.0) : name(_name), power(_cap*_toggle)
      {
      const char *chptr = strchr(name, '[');
      lsb = -1;
      msb = -1;
      
      indices.push(_index);

      if (chptr!=NULL)
         {
         len = chptr-name;
         lsb = msb = atoi(chptr+1);
         chptr = strchr(chptr, ':');
         if (chptr!=NULL)
            lsb = atoi(chptr+1);
         }
      else
         len = strlen(name);
      deleted = false;
      }
   nodesorttype() : name(NULL)
      {FATAL_ERROR;}  // makes sun compiler happy
   bool operator<(const nodesorttype &right) const
      {
      const char *a = name, *b = right.name;
      int anglea=0, angleb=0;
      do{
         if (*a == '[' && *b == '[')
            {
            a++, b++;
            while (*a>='0' && *a<='9')
               {
               anglea = anglea*10 + *a-'0';
               a++;
               }
            while (*b>='0' && *b<='9')
               {
               angleb = angleb*10 + *b-'0';
               b++;
               }
            }
         if (*a <'0' || *a >'9' || *b <'0' || *b >'9')
            {
            if (*a < *b) return true;
            if (*a > *b) return false;
            a++;
            b++;
            }
         else
            {
            int numa=0, numb=0;
            
            while (*a>='0' && *a<='9')
               {
               numa = numa*10 + *a-'0';
               a++;
               }
            while (*b>='0' && *b<='9')
               {
               numb = numb*10 + *b-'0';
               b++;
               }
            if (numa < numb) return true;
            if (numa > numb) return false;
            }
         }while (*a != 0 || *b != 0);
      return anglea < angleb;
      }
   };

class powerPredicatetype{
public:
   bool operator()(const nodesorttype &x, const nodesorttype &y) const
      {
      return x.power > y.power;
      }
   };

void simtype::PowerReport()
   {
   TAtype ta("Dumping Power report");
   array <nodesorttype> nodesorts;
   int i;
   const char *chptr, *lastchptr;
   filetype fp;
   double total=0.0;

   // I now want to compile a list of signals from my wirelist. I'll only get things that touch a gate
   for (i=0; i<nodes.size(); i++)
      {
      char buffer[512];
      if (nodes[i].state)
         nodesorts.push(nodesorttype(stringPool.NewString(nodes[i].RName(buffer)), i, nodes[i].capacitance, nodes[i].toggleCount));
      total += nodes[i].capacitance*nodes[i].toggleCount;
      }
   nodesorts.Sort();

   // now I want to merge nodes into busses
   int last = 0;
   lastchptr = strchr(nodesorts[0].name, ']');
   for (i=0; i<nodesorts.size(); i++)
      {
      chptr = strchr(nodesorts[i].name, ']');
      if (lastchptr != NULL && chptr != NULL && strcmp(lastchptr, chptr)==0 &&
          strncmp(nodesorts[last].name, nodesorts[i].name, nodesorts[i].len)==0 &&
          nodesorts[last].len == nodesorts[i].len && nodesorts[last].msb == nodesorts[i].lsb-1)
         {
         nodesorts[last].msb = nodesorts[i].msb;
         nodesorts[last].indices += nodesorts[i].indices;
         nodesorts[last].power += nodesorts[i].power;
         nodesorts[i].deleted = true;
         }
      else
         {
         last = i;
         lastchptr = strchr(nodesorts[i].name, ']');
         }
      }
   std::sort(nodesorts.begin(), nodesorts.end(), powerPredicatetype());  // now sorted by power
   fp.fopen("power.txt.gz","wt");
   for (i=0; i<nodesorts.size(); i++)
      {
      if (!nodesorts[i].deleted && nodesorts[i].power>0.0)
         {
         if (nodesorts[i].msb > nodesorts[i].lsb)
            {
            char buffer[1024];
            strcpy(buffer, nodesorts[i].name);            
            chptr = strchr(nodesorts[i].name, ']');
            if (chptr==NULL) FATAL_ERROR;
            sprintf(buffer+nodesorts[i].len,"[%d:%d]%s",nodesorts[i].msb,nodesorts[i].lsb,chptr+1);
            fp.fprintf("%8.5f%% %10.2e %s\n",(double)100.0*nodesorts[i].power/total,(double)nodesorts[i].power,buffer);
            }
         else
            fp.fprintf("%8.5f%% %10.2e %s\n",(double)100.0*nodesorts[i].power/total,(double)nodesorts[i].power,nodesorts[i].name);
         }
      }   
   fp.fclose();
   }


void vcdtype::OpenForWrite(const char *_filename, array <nodetype> &nodes)
   {
   TAtype ta("Opening output VCD file for writing.");
   array <nodesorttype> nodesorts;
   int i;
   const char *chptr, *lastchptr;
   char ref[5];

   strcpy(filename,_filename);
   fp.fopen(filename, "wt");
   printf("\nOpening VCD file %s",filename);

   fp.fprintf("$date\n    %s\n$end\n\nversion\n    Sherlock.exe\n$end\n\n$timescale\n    1ps\n$end\n\n","today");
   fp.fprintf("$scope module global $end\n");
   
   // I now want to compile a list of signals from my wirelist. I'll only get things that touch a gate
   for (i=0; i<nodes.size(); i++)
      {
      char buffer[512];
      if (nodes[i].state)
         nodesorts.push(nodesorttype(stringPool.NewString(nodes[i].RName(buffer)), i));
      }
   nodesorts.Sort();

   // now I want to merge nodes into busses
   int last = 0;
   lastchptr = strchr(nodesorts[0].name, ']');
   for (i=0; i<nodesorts.size(); i++)
      {
      chptr = strchr(nodesorts[i].name, ']');
      if (lastchptr != NULL && chptr != NULL && strcmp(lastchptr, chptr)==0 &&
          strncmp(nodesorts[last].name, nodesorts[i].name, nodesorts[i].len)==0 &&
          nodesorts[last].len == nodesorts[i].len && nodesorts[last].msb == nodesorts[i].lsb-1)
         {
         nodesorts[last].msb = nodesorts[i].msb;
         nodesorts[last].indices += nodesorts[i].indices;
         nodesorts[i].deleted = true;
         }
      else
         {
         last = i;
         lastchptr = strchr(nodesorts[i].name, ']');
         }
      }
      
   ref[0]=0;
   ref[1]=0;
   ref[2]=0;
   ref[3]=0;
   for (i=0; i<nodesorts.size(); i++)
      {
      if (!nodesorts[i].deleted)
         {
         ref[0]++;
         if (ref[0]==1)
            ref[0]=33;
         if (ref[0]>126)
            {
            ref[0]=33;
            ref[1]++;
            }
         if (ref[1]==1)
            ref[1]=33;
         if (ref[1]>126)
            {
            ref[1]=33;
            ref[2]++;
            }
         if (ref[2]==1)
            ref[2]=33;
         if (ref[2]>126)
            {
            ref[2]=33;
            ref[3]++;
            }
         if (ref[3]==1)
            FATAL_ERROR;
         if (nodesorts[i].msb > nodesorts[i].lsb)
            {
            char buffer[1024];
            strcpy(buffer, nodesorts[i].name);            
            chptr = strchr(nodesorts[i].name, ']');
            if (chptr==NULL) FATAL_ERROR;
            sprintf(buffer+nodesorts[i].len,"[%d:%d]%s",nodesorts[i].msb,nodesorts[i].lsb,chptr+1);
            fp.fprintf("$var wire %d %s %s $end\n",nodesorts[i].msb - nodesorts[i].lsb +1,ref,buffer);
            }
         else
            fp.fprintf("$var wire %d %s %s $end\n",nodesorts[i].msb - nodesorts[i].lsb +1,ref,nodesorts[i].name);

         sigs.push(vcdSigtype(nodesorts[i].msb, nodesorts[i].lsb, ref, -1));
         sigs.back().nodes = nodesorts[i].indices;
         }
      }
   for (i=0; i<sigs.size(); i++)
      {
      for (int k=0; k<sigs[i].nodes.size(); k++)
         {
         nodes[sigs[i].nodes[k]].outputSigChangedPtr = &sigs[i].changed;
         }
      sigs[i].changed = true;  // will force an initial dump out
      }
   fp.fprintf("$upscope $end\n\n");
   fp.fprintf("$enddefinitions $end\n\n");
   fp.fprintf("#0\n");
   fp.fprintf("$dumpvars\n");
   }

void vcdtype::WriteTimeSlice(int newTime, const array <nodetype> &nodes)
   {
   TAtype ta("vcdtype::WriteTimeSlice");
   int i, k;
   char buffer[4096];      
   if (currentTime<0 && newTime >=0)
      fp.fprintf("$end\n");
   if (currentTime != newTime && newTime>=0)
      {
      fp.fprintf("#%d\n",newTime);
      if (newTime < currentTime)
         {
         printf("newTime = %d currentTime=%d",newTime, currentTime);
         FATAL_ERROR;  // this is probably caused by the input trace have a quick transition when doing -fine
         }
      }
   currentTime = newTime;
   for (i=0; i<sigs.size(); i++)
      if (sigs[i].changed)
         {
         bitfieldtype <MAX_SIGLENGTH> currentstate = 0;
         bitfieldtype <MAX_SIGLENGTH> xstate = 0;
         for (k=0; k<sigs[i].nodes.size(); k++)
            {
            if (nodes[sigs[i].nodes[k]].value == V_ONE)
               currentstate.SetBit(k);
            }
         if (sigs[i].binarystate != currentstate || newTime<=0)
            {
            if (sigs[i].nodes.size()==1)
               {
//                  fp.fprintf("%c%s\n",currentstate.GetBit(0) ? '1' : '0',sigs[i].ref);
               buffer[0] = currentstate.GetBit(0) ? '1' : '0';
               strcpy(buffer+1, sigs[i].ref);
               strcat(buffer+1, "\n");
               fp.fputs(buffer);
               }
            else 
               {
               for (k=0; k<sigs[i].nodes.size(); k++)
                  buffer[k] = currentstate.GetBit(sigs[i].nodes.size() - k -1) ? '1' : '0';
               buffer[k]=0;
               fp.fprintf("b%s %s\n",buffer,sigs[i].ref);
               }
            sigs[i].binarystate = currentstate;
            }
         sigs[i].changed = false;
         }

   }


void vcdtype::OpenForRead(const char *_filename)
   {
   TAtype ta("Opening input VCD file for reading.");
   char buffer[4096], *name, *chptr;
   const char *constptr;
   array <const char *> hierarchy;
   const char * fullHierarchyName;
   int len, digits;
   
   strcpy(filename, _filename);
   if (strchr(filename,'.')==NULL)
      strcat(filename,".dmp");
   fp.fopen(filename, "rt");
   printf("\nOpening VCD file %s",filename);

   while (NULL != fp.fgets(buffer, sizeof(buffer)-1))
      {
      line++;
      if (strncmp(buffer,"$timescale",10)==0)
         {
         if (NULL == fp.fgets(buffer, sizeof(buffer)-1)) FATAL_ERROR;
         if (strstr(buffer,"1fs")!=NULL || strstr(buffer,"1 fs")!=NULL)
            multiplier = 1.0 / 1000.0;
         if (strstr(buffer,"10fs")!=NULL || strstr(buffer,"10 fs")!=NULL)
            multiplier = 10.0 / 1000.0;
         if (strstr(buffer,"100fs")!=NULL || strstr(buffer,"100 fs")!=NULL)
            multiplier = 100.0 / 1000.0;
         if (strstr(buffer,"1ps")!=NULL || strstr(buffer,"1 ps")!=NULL)
            multiplier = 1.0;
         if (strstr(buffer,"10ps")!=NULL || strstr(buffer,"10 ps")!=NULL)
            multiplier = 10.0;
         if (strstr(buffer,"100ps")!=NULL || strstr(buffer,"100 ps")!=NULL)
            multiplier = 100.0;
         if (strstr(buffer,"1ns")!=NULL || strstr(buffer,"1 ns")!=NULL)
            multiplier = 1000.0;
         if (strstr(buffer,"10ns")!=NULL || strstr(buffer,"10 ns")!=NULL)
            multiplier = 10000.0;
         if (strstr(buffer,"100ns")!=NULL || strstr(buffer,"100 ns")!=NULL)
            multiplier = 100000.0;
         if (strstr(buffer,"1us")!=NULL || strstr(buffer,"1 us")!=NULL)
            multiplier = 1000000.0;
         }
      if (strncmp(buffer,"$scope ",7)==0)
         {         
         name = strchr(buffer+7, ' ');
         if (name==NULL) FATAL_ERROR;
         while (*name==' ') name++;
         chptr = strchr(name, ' ');
         if (chptr==NULL) FATAL_ERROR;
         *chptr = 0;
         hierarchy.push(stringPool.NewString(name));
         buffer[0]=0;
         for (int i=0; i<hierarchy.size(); i++)
            {
            strcat(buffer,hierarchy[i]);
            strcat(buffer,".");
            }
         fullHierarchyName = stringPool.NewString(buffer);
//         printf("\n%s", fullHierarchyName);
         }
      if (strncmp(buffer,"$upscope ", 9)==0)
         {
         hierarchy.pop(constptr);
         buffer[0]=0;
         for (int i=0; i<hierarchy.size(); i++)
            {
            strcat(buffer,hierarchy[i]);
            strcat(buffer,"/");
            }
         fullHierarchyName = stringPool.NewString(buffer);
         }
      if (strncmp(buffer,"$var ",5)==0)
         {
         chptr = buffer +5;
         if (*chptr == ' ') FATAL_ERROR;
         chptr = strchr(chptr, ' ');
         while (*chptr == ' ')
            chptr++;
         len = atoi(chptr);   // number of bits for this signal
         chptr = strchr(chptr, ' ');
         while (*chptr == ' ')
            chptr++;
         digits = 0;
         
         char ref[5];
         int id=0, msb=-1, lsb=-1;
         while (*chptr >=33 && *chptr <=126)
            {
            ref[digits] = *chptr;
            id = id * 128 + (*chptr);
            digits++;
            chptr++;
            }
         ref[digits]=0;
         if (digits > sizeof(ref)) FATAL_ERROR;
         if (id == 0) FATAL_ERROR;
         if (*chptr != ' ') FATAL_ERROR;
         while (*chptr == ' ')
            chptr++;
         name = chptr;
         chptr = strchr(name, ' ');
         *chptr = 0;
         const char *signame = stringPool.NewString(name);
         chptr++;
         if (*chptr=='[')
            lsb = msb = atoi(chptr+1);
         chptr = strchr(chptr, ':');
         if (chptr != NULL)
            lsb = atoi(chptr+1);

         if (msb - lsb + 1 > MAX_SIGLENGTH)
            {
            printf("\nBehavioral signal %s is too wide. Will be ignored",signame);
            }
         else
            {
            int index;
            if (!idxref.Check(id, index))
               {
               sigs.push(vcdSigtype (msb, lsb, ref, id));
               index = sigs.size()-1;
               idxref.Add(id, index);
               }
//            if (strstr(fullHierarchyName,"dpu_top.emu1.dpu_mul.dpu_mul_rep.")!=NULL)
//               printf("signame");
            aliases.push(aliastype(signame, fullHierarchyName, index, msb==lsb ? msb : -1));
            }
         }
      if (strncmp(buffer,"$dumpvars",9)==0)
         break;
      }
   currentTime=0;
   nextTime = 0;
   }

bool vcdtype::ReadTimeSlice() // return false when no more entries
   {
   TAtype ta("vcdtype::ReadTimeSlice()");
   char buffer[4096];

   while (NULL != fp.fgets(buffer, sizeof(buffer)-1))
      {
      bitfieldtype <MAX_SIGLENGTH> binarystate =0;
      bitfieldtype <MAX_SIGLENGTH> xstate =0;
      char *chptr, *startptr;
      int id=0, index, i;
      line++;

      if (buffer[0]=='x' ||  buffer[0]=='z' || buffer[0]=='0' || buffer[0]=='1')
         {
         chptr = buffer+1;
         while (*chptr >=33 && *chptr <=126)
            {
            id = id * 128 + (*chptr);
            chptr++;
            }
         if (id==0) FATAL_ERROR;
         if (!idxref.Check(id, index))
            FATAL_ERROR;
//         for (index=sigs.size()-1; index>=0; index--)
//            if (sigs[index].id == id)
//               break;
         if (index<0) FATAL_ERROR;   // no match
         if (sigs[index].msb - sigs[index].lsb +1 != 1) FATAL_ERROR;

         switch (buffer[0])
            {
            case '0':
               sigs[index].xstate.ClearBit(0);
               sigs[index].binarystate.ClearBit(0);
               break;
            case '1':
               sigs[index].xstate.ClearBit(0);
               sigs[index].binarystate.SetBit(0);
               break;
            case 'x':
               sigs[index].xstate.SetBit(0);
               sigs[index].binarystate.ClearBit(0);
               break;
            case 'z':
               sigs[index].xstate.SetBit(0);
               sigs[index].binarystate.SetBit(0);
               break;
            default: FATAL_ERROR;
            }
         }
      else if (buffer[0]=='b')
         {         
         startptr = strchr(buffer, ' ');

         if (startptr==NULL) FATAL_ERROR;
         chptr = startptr+1;         
         while (*chptr >=33 && *chptr <=126)
            {
            id = id * 128 + (*chptr);
            chptr++;
            }
         if (id==0) FATAL_ERROR;
         if (!idxref.Check(id, index))
            index = -1;         
         chptr = startptr;
         for (i=0; index>=0 && i<=sigs[index].msb - sigs[index].lsb; i++)
            {
            chptr--;
            if (*chptr == 'b') 
               {
               chptr++;
               if (*chptr=='1') *chptr='0';  // always zero extend
               }
            switch (*chptr)
               {
               case '0':
                  sigs[index].xstate.ClearBit(i);
                  sigs[index].binarystate.ClearBit(i);
                  break;
               case '1':
                  sigs[index].xstate.ClearBit(i);
                  sigs[index].binarystate.SetBit(i);
                  break;
               case 'x':
                  sigs[index].xstate.SetBit(i);
                  sigs[index].binarystate.ClearBit(i);
                  break;
               case 'z':
                  sigs[index].xstate.SetBit(i);
                  sigs[index].binarystate.SetBit(i);
                  break;
               default: FATAL_ERROR;
               }
            }
         }
      else if (buffer[0]=='#')
         {
         currentTime = nextTime;
         nextTime = atoint64(buffer+1) * multiplier;
         return true;
         }
      }
   return false;
   }




void simtype::ReadDescr(char *filename)
   {
   TAtype ta("Reading descript file");
   char buffer[200], *chptr, behname[200], wirename[200], *behstart, *wirestart, *wiretail;
   char rtlPrepend[256]="";
   char wirePrepend[256]="";
   FILE *fptr;              
   bool input, output, complement, smart;
   int line=0, kill=false, i;
   int clock;
   int checka, checkb, totalinputs=0, totaloutputs=0;
   int behmsb, behlsb, wiremsb, wirelsb;   
   
   if (strchr(filename,'.')==NULL)
      strcat(filename,".descr");
   fptr=fopen(filename,"r");
   if (fptr==NULL)
      {
      printf("\nI could not open %s for reading!",filename); 
      shutdown(-1);
      }    
   
   printf("\nReading %s",filename);   
   while (NULL!=fgets(buffer,sizeof(buffer)-1,fptr))
      {
      while (buffer[strlen(buffer)-1]=='\n')
         buffer[strlen(buffer)-1]=0;
      while (buffer[strlen(buffer)-1]=='\r')
         buffer[strlen(buffer)-1]=0;
      line++;       
//      strlwr(buffer);
      
      while ((chptr=strchr(buffer,'\t'))!=NULL) *chptr=' ';
      while ((chptr=strchr(buffer,'<'))!=NULL) *chptr='[';
      while ((chptr=strchr(buffer,'>'))!=NULL) *chptr=']';
      
      input=output=smart=false;
      checka=checkb=false;
      clock=false;
      if (strncmp(buffer, "rtl_prepend",11)==0)
         {const char *chptr=buffer;
         while (*chptr !=' ' && *chptr!='\t' && *chptr!=0) chptr++;
         while (*chptr ==' ' || *chptr=='\t') chptr++;
         strcpy(rtlPrepend, chptr);
         while (rtlPrepend[strlen(rtlPrepend)-1]=='\n' || rtlPrepend[strlen(rtlPrepend)-1]=='\r')
            rtlPrepend[strlen(rtlPrepend)-1] = 0; // squash newline at end
         }
      else if (strncmp(buffer, "wire_prepend",11)==0)
         {const char *chptr=buffer;
         while (*chptr !=' ' && *chptr!='\t' && *chptr!=0) chptr++;
         while (*chptr ==' ' || *chptr=='\t') chptr++;
         strcpy(wirePrepend, chptr);
         while (wirePrepend[strlen(wirePrepend)-1]=='\n' || wirePrepend[strlen(wirePrepend)-1]=='\r')
            wirePrepend[strlen(wirePrepend)-1] = 0; // squash newline at end
         }
      else if (strncmp(buffer,"o ",2)==0)
         {output=true; // found an output
         checka=checkb=true;}
      else if (strncmp(buffer,"a ",2)==0)
         {output=true; // found an input 
         checka=true;}
      else if (strncmp(buffer,"b ",2)==0)
         {output=true; // found an input
         checkb=true;}
      else if (strncmp(buffer,"i ",2)==0)
         input=true; // found an input
      else if (strncmp(buffer,"x ",2)==0)
         smart=true; // either input or output. I'll decide
      else if (strncmp(buffer,"c ",2)==0)
         {input=true; // found an input
         clock=true;}
      else if (strncmp(buffer,";",1)==0 || strncmp(buffer,"//",2)==0)
         ; // comment
      else
         {
         chptr=buffer; 
         while (*chptr!=0)
            {                                       
            if ( (*chptr>='0' && *chptr<='1') ||        
                 (*chptr>='A' && *chptr<='Z') ||
                 (*chptr>='a' && *chptr<='z'))
               {
               printf("\nI didn't understand line %d of file %s.",line,filename);
               FATAL_ERROR;
               }
            chptr++;        
            }
         }
      if (input || output || smart)
         {
         complement=false;
         behmsb  = behlsb  = -1;
         wiremsb = wirelsb = -1;
         matchtype match;
         match.inerror = false;
         
         chptr=buffer+2;
         while ( *chptr==' ' || *chptr==9 || *chptr=='!' || *chptr=='~')
            {
            if (*chptr=='!' || *chptr=='~') complement= !complement;
            chptr++;
            }
         behstart=chptr;
         while ( *chptr!=' ' && *chptr!=9 && *chptr!='\n' && *chptr!=0)
            chptr++;
         wirestart = chptr+1;
         if (*chptr==0)
            wirestart = chptr;
         *chptr=0; // null terminate behname
         chptr = strchr(behstart,'[');
         if (chptr!=NULL)
            {
            *chptr=0;
            chptr++;
            behmsb = behlsb = atoi(chptr);
            chptr = strchr(chptr,':');
            if (chptr!=NULL)
               behlsb = atoi(chptr+1);
            }
         if (stricmp(behstart,"vss")!=0 && stricmp(behstart,"vdd")!=0)
            strcpy(behname, rtlPrepend);
         else
            strcpy(behname,"");
         strcat(behname, behstart);

         while (*wirestart == ' ' || *wirestart==9 || *wirestart=='~' || *wirestart=='!')
            {
            if (*wirestart=='!' || *wirestart=='~') complement= !complement;
            wirestart++;
            }
         chptr=wirestart; // skip to end of wirename
         while (*chptr!=' ' && *chptr!=9 && *chptr!=0 && *chptr!='\n' && *chptr!='\r')
            chptr++;
    
         *chptr=0; // null terminate wirename
         if (wirestart[0]==0)
            {
            wirestart = behstart;
            wirelsb = behlsb;
            wiremsb = behmsb;
            }

         chptr = strchr(wirestart,':');
         wiretail="";
         if (chptr!=NULL)
            {
            while (chptr > wirestart && *chptr!='[')
               chptr--;
            if (*chptr!='[') FATAL_ERROR;
            *chptr=0;
            chptr++;
            wiremsb = wirelsb = atoi(chptr);
            if (strchr(chptr,']')!=NULL)
               wiretail = strchr(chptr,']')+1;
            chptr = strchr(chptr,':');
            if (chptr!=NULL)
               wirelsb = atoi(chptr+1);            
            }

         strcpy(wirename, wirePrepend);
         strcat(wirename, wirestart);
            
         match.behname  = stringPool.NewString(behname);
         if (strcmp(wirename, behname)!=0)
            match.wirename = stringPool.NewString(wirename);
         else
            match.wirename = match.behname;
         match.complement = complement;
         match.checka = checka;
         match.checkb = checkb;
         match.input  = input;

         if (behmsb-behlsb != wiremsb-wirelsb)
            {
            printf("\nYour array subscripts don't match up between wirelist and behavioral\nLine %d of %s.",line,match.behname,filename);
            HARD_ERROR++;
            }         

         i=0;
         if (stricmp(match.behname, "vss")==0)
            ;  // sigIndex.size()==0 as signal that is power node
         else if (stricmp(match.behname, "vdd")==0)
            ;
         else
            {
            for (i=inputVcd.aliases.size()-1; i>=0; i--)
               {
               if (strncmp(inputVcd.aliases[i].hierarchy, match.behname, strlen(inputVcd.aliases[i].hierarchy))==0)
                  {
                  char mergedname[2048];
                  strcpy(mergedname, inputVcd.aliases[i].hierarchy);
                  strcat(mergedname, inputVcd.aliases[i].name);
                  if (strcmp(mergedname, match.behname)==0 && behmsb <= inputVcd.sigs[inputVcd.aliases[i].sigIndex].msb && behlsb >= inputVcd.sigs[inputVcd.aliases[i].sigIndex].lsb)
                     {
                     match.msb = behmsb;
                     match.lsb = behlsb;
                     if (match.sigIndex.size() !=0) FATAL_ERROR;
                     match.sigIndex.push(inputVcd.aliases[i].sigIndex);
                     break;
                     }
/*                  else if (strcmp(mergedname, match.behname)==0 && inputVcd.aliases[i].bit>=0 && inputVcd.aliases[i].bit>=0 && inputVcd.aliases[i].bit>=behlsb && inputVcd.aliases[i].bit<=behmsb)
                     {
                     match.msb = behmsb-behlsb;
                     match.lsb = 0;
                     if (match.sigIndex.size()==0)
                        {
                        for (int k=0; k<behmsb-behlsb+1; k++)
                           match.sigIndex.push(-1);
                        }
                     match.sigIndex[inputVcd.aliases[i].bit - behlsb] = inputVcd.aliases[i].sigIndex;
                     }*/
                  else if (strcmp(mergedname, match.behname)==0 && inputVcd.sigs[inputVcd.aliases[i].sigIndex].msb == inputVcd.sigs[inputVcd.aliases[i].sigIndex].lsb && behmsb >= inputVcd.sigs[inputVcd.aliases[i].sigIndex].msb && behlsb <= inputVcd.sigs[inputVcd.aliases[i].sigIndex].lsb)
                     {
                     match.msb = behmsb;
                     match.lsb = behlsb;
                     if (match.sigIndex.size()==0)
                        {
                        for (int k=0; k<behmsb-behlsb+1; k++)
                           match.sigIndex.push(-1);
                        }
                     match.sigIndex[inputVcd.sigs[inputVcd.aliases[i].sigIndex].lsb - behlsb] = inputVcd.aliases[i].sigIndex;
                     }
                  }
               }
            }
         
         if (i<0 && match.sigIndex.size()==0)
            {
            printf("\nI couldn't match your behavioral signal name(%s) to anything in the vcd dump file.\nLine %d of %s.",match.behname,line,filename);
            HARD_ERROR++;
            kill = true;
            }
         else
            {
            for (i=0; i<match.sigIndex.size(); i++)
               if (match.sigIndex[i] <0) 
                  {
                  printf("\nAt line %d of your descript file you have a mismatch between busses. You're descript file probably has more bits than the VCD.",line);
                  printf("\nThe signal in question is %s",match.behname);
                  int k;
                  printf("\nThe bits in question are: ");
                  for (k=0; k<match.sigIndex.size(); k++)
                     if (match.sigIndex[k] <0)
                        printf("[%d] ",match.lsb+k);
                  FATAL_ERROR;
                  }
            }
            // Now I want to try to match up in wirelist
         for (i=wirelsb; i<=wiremsb; i++)
            {
            int index;
            if (i!=-1)
               sprintf(wirename,"%s[%d]%s",match.wirename,i,wiretail);
            else
               sprintf(wirename,"%s",match.wirename);
            if (!h.Check(wirename, index))
               {
               printf("\nI couldn't match your wirelist signal name(%s) to anything in your spice wirelist.\nLine %d of %s.",wirename,line,filename);
               HARD_ERROR++;
               kill = true;
               break;
               }
            if (smart && !nodes[index].sds.size())
               {
               match.input  = input  = true;
               output = false;
               }
			else if (smart)
			   {
			   match.checka=match.checkb=true;
			   match.input = false;
			   output = true;
			   }
            if (match.input) 
               {
               totalinputs++;
               inputs.push(index);
               }
            else 
               {
               totaloutputs++;
               outputs.push(index);
               }
            match.nodelist.push(index);
            }
         match.timing=0;
         if (clock && !kill)
            {
            match.timing=-5;
            printf("\nThe input signal %s, %s will be retimed for %d gate delays relative to everything else.",match.behname,match.wirename,match.timing);
            }
         if (!kill)
            matches.push(match);
         }
      }
   fclose(fptr);
   if (!kill)
      printf("\n%d signals will be driven from trace file. %d signals will be compared against.",totalinputs,totaloutputs);
   else
      shutdown(-1);   
   }                       



