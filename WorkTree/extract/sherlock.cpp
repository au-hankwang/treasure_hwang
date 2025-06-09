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
const float POWER_VOLTAGE      = 0.90;
extern bool finfet;



                        
struct matchtype{
   const char *behname;      // can also be scan_in
   const char *wirename;     // can also be scan_out
   array <int> sigIndex;     // index into input.sig[] list, if size()>1 then ignore lsb,msb verilog exploded a bus
   int lsb, msb;
   array <int> nodelist;  // will go from lsb to msb
   int timing; // I will push this number as the start time for the signal
  unsigned complement :1;
  unsigned input :1;
  unsigned checka :1;
  unsigned checkb :1;
  unsigned changes_in_a :1;
  unsigned changes_in_b :1;
  unsigned inerror :1;
  unsigned scan :1;
  unsigned scanenable :1;
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
  __int64 currentTime, nextTime, lastTime;
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
   void OpenForWrite(const char *filename, array <nodetype> &nodes, char* powerreportmode);
   void WriteTimeSlice(int currentTime, const array <nodetype> &nodes);
   void WritePowerSlice(int newTime, double dynamic, double dynamic_integral, double smooth);
   void OpenForRead(const char *filename);
   bool ReadTimeSlice();    // returns false when file is exhausted   
   bool IsReadyToWrite() const
      {
      return fp.IsOpen();
      }
  void DumpAllSignals();
  int GetLastToggleTime();
};


struct chaintype{
  matchtype match;
  bool complement;
  int in, out;
  int length;
  array <valuetype> fifo;
};


class simtype : public circuittype{
public:
  int HARD_ERROR, SOFT_ERROR;
  vcdtype inputVcd;
  vcdtype outputVcd;
  array <matchtype> matches;
  bool fineResolution, noDump, noFloatingChecks;
  int compareTimeStart, dumpTime, reportWindow; //report delta power every reportWindow cycles.
  int powerstart, powerend; //Report power between these times
  char powerreportmode[12]; //summary or detail. summary=report net/flop power for all cycles. Detail=multipe net/flop power reports, every reportWindow cycles. 
  int leakageIntervals;
  float peakPower;
  int peakPowerTimeStart, peakPowerTimeEnd;
  
   simtype()
      {
      HARD_ERROR = 0;
      SOFT_ERROR = 0;
      fineResolution = false;
      noFloatingChecks = false;
      noDump = false;
      compareTimeStart = 20;
      dumpTime = -1;
      leakageIntervals = 0;
      reportWindow = 1000; // report power etc every 1000 cycles
      peakPower=0;
      peakPowerTimeStart=0;
      peakPowerTimeEnd=0;
      powerstart=20;
      strcpy(powerreportmode,"summary");
      powerend=MAX_INT;
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
  void ScanTest();
  void InjectRandomnValuesinChainsAndOptionallyCheckOutput(array<chaintype>& chains, int& time, char phase='X', bool check_output=true);
  void PulseScanEnableForCapture(array<int>& scanenables, int& time);
  void ResetChainsToVX(array<chaintype>& chains);
  void InjectRandomnValuesToInputs(char phase='X');
  void ScanPulseClock(int &time, bool ignoreWarnings=false, char phase='X');//phase={A,B,X=both}
  void ReadDescr(char *filename, bool scanonly);
  void IntermediateNodeDumping(int relativetime, int currentTime)
  {
    bool vcd_is_ready_to_write=outputVcd.IsReadyToWrite();
    if (fineResolution && vcd_is_ready_to_write){
      outputVcd.WriteTimeSlice(currentTime + MAXIMUM(relativetime+32,0), nodes);
    }
  }
   void ClearPower()
      {
      TAtype ta("ClearPower()");
      for (int i=0; i<nodes.size(); i++)
         nodes[i].toggleCount = 0;
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
   double SumLeakage(vttype forceVt = VT_ALL)
      {
      TAtype ta("SumLeakage()");
      double accum=0.0;
      int i;
      
      for (i=0; i<devices.size(); i++)
         {
         devicetype &d = devices[i];
         accum += d.Leakage(forceVt) * d.offcount;
         if (d.offcount<0.0) FATAL_ERROR;
         }
      for (i=0; i<rams.size(); i++)
         {
         ramtype &r = rams[i];
         int k;

         for (k=0; k<3; k++)
            {
            devicetype &d = k==0 ? leakSigs[r.leakSigIndex].n :
                            k==1 ? leakSigs[r.leakSigIndex].p :
                                   leakSigs[r.leakSigIndex].pass;
            accum += d.Leakage(forceVt);
            }
         }
      return accum / 1000000.0 ;
      }
   void PowerReport(int cycle=-1);
  void FlopPowerReport(int cycle=-1);
  void LeakageReport();
   void LeakageEstimate();
  void PublishStats(wirelisttype* wl);
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
   char bboxfilename[200]="";
   char outputname[200]="sherlock_out.dmp.gz";
   char capname[200]="";
   int i;
   bool help=false, leakonly=false, scan=false, stat=false, found;
   wirelisttype *wl = NULL;

   int processed_args[24]={}; //upto 24 arguments right now.
   for (i=1; i<argc; i++)	   
     {
       if (0==stricmp(argv[i],"help") || 0==stricmp(argv[i],"-help"))         
         {
	   help = true;
	   processed_args[i]=1;
         }
     }
   for (i=1; i<argc; i++)	   
     {
       if (0==stricmp(argv[i],"leakonly") || 0==stricmp(argv[i],"-leakonly"))         
         {
	   leakonly = true;
	   processed_args[i]=1;
         }
     }
   for (i=1; i<argc; i++)	   
      {
      if (0==stricmp(argv[i],"fine") || 0==stricmp(argv[i],"-fine"))         
         {
	   fineResolution = true;
	   processed_args[i]=1;
         }
      }
   for (i=1; i<argc; i++)	   
     {
       if (0==stricmp(argv[i],"nodump") || 0==stricmp(argv[i],"-nodump"))         
         {
	   noDump = true;
	   processed_args[i]=1;
         }
     }
   for (i=1; i<argc; i++)	   
     {
       if (0==strncmp(argv[i],"powerreportmode=", 16) || 0==strncmp(argv[i],"-powerreportmode=", 17))         
	 {
	   char* prmode=strchr(argv[i],'=')+1;
	   if ((strcmp(prmode, "summary")!=0) && (strcmp(prmode, "detail")!=0))
	     printf("\n'%s should be summary or detail '\n",argv[i]);
	   strcpy(powerreportmode,prmode);
	   processed_args[i]=1;
	 }
       
     }
   for (i=1; i<argc; i++)	   
     {
       if (0==strncmp(argv[i],"powerstart=", 11) || 0==strncmp(argv[i],"-powerstart=", 12))         
	 {
	   powerstart=atoi(strchr(argv[i],'=')+1);
	   processed_args[i]=1;
	 }
     }
   for (i=1; i<argc; i++)	   
      {
	if (0==strncmp(argv[i],"powerend=", 9) || 0==strncmp(argv[i],"-powerend=", 10))         
	  {
	    powerend=atoi(strchr(argv[i],'=')+1);
	    processed_args[i]=1;
	  }
      }

   for (i=1; i<argc; i++)	   
      {
      if (0==stricmp(argv[i],"nofloat") || 0==stricmp(argv[i],"-nofloat"))         
	{
	  noFloatingChecks = true;
	  processed_args[i]=1;
	}
      }
   for (i=1; i<argc; i++)	   
     {
       if (0==stricmp(argv[i],"scan") || 0==stricmp(argv[i],"-scan"))         
	 {
	   scan = true;
	   processed_args[i]=1;
	 }
     }
   for (i=1; i<argc; i++)	   
     {
       if (0==strnicmp(argv[i],"-bbox_file",10) || 0==strnicmp(argv[i],"-bbox_file=",11))         
	 {
	   strcpy(bboxfilename,strchr(argv[i],'=')+1);
	   processed_args[i]=1;
	 }
     }
   for (i=1; i<argc; i++)	   
     {
       if (0==strnicmp(argv[i],"comparetime=",12) || 0==strnicmp(argv[i],"-comparetime=",13))
         {
	   compareTimeStart = atoi(strchr(argv[i],'=')+1);
	   processed_args[i]=1;
         }
     }
   for (i=1; i<argc; i++)	   
     {
      if (0==strnicmp(argv[i],"dumptime=",9) || 0==strnicmp(argv[i],"-dumptime=",10))
         {
	   dumpTime = atoi(strchr(argv[i],'=')+1);
	   processed_args[i]=1;
         }
      }
   for (i=1; i<argc; i++)	   
      {
      if (0==strnicmp(argv[i],"maxerrors=",10) || 0==strnicmp(argv[i],"-maxerrors=",11))
	{
	   MAX_ERRORS = atoi(strchr(argv[i],'=')+1);
	   processed_args[i]=1;
         }
      }
   for (i=1; i<argc; i++)	   
     {
       if (0==strnicmp(argv[i],"reportwindow=",13) || 0==strnicmp(argv[i],"-reportwindow=",14))
	 {
	   reportWindow = atoi(strchr(argv[i],'=')+1);
	   processed_args[i]=1;
	 }
     }
   for (i=1; i<argc; i++)	   
     {
       if (0==strnicmp(argv[i],"stat",4) || 0==strnicmp(argv[i],"-stat",5))
	 {
	   stat = true;
	   processed_args[i]=1;
	 }
     }
   
   for (i=1; i<argc; i++)	   
     {
       if ((processed_args[i]==0) && (argv[i][0]=='-'))
	 {
	   printf("\nI didn't understand command line parameter '%s'\n",argv[i]);
	   exit(-1);
         }
     }

/*   if (argc==2)
      {
      if (NULL==strchr(argv[1],'.'))
         {
         strcpy(filename,argv[1]);
         strcpy(descrname,argv[1]);
         strcpy(vcdname,argv[1]);
         argc=0;
         printf("$Sherlock %s.hsp %s.descr %s.dmp", filename, descrname, vcdname);
         }
      }*/
   int remainderindex=1;
   for (i=1; i<argc; i++)          
     {
	if (processed_args[i]==0){
	  if (argv[i]!=NULL && remainderindex==1)
	    strcpy(filename,argv[i]);                     
	  if (argv[i]!=NULL && remainderindex==2)
	    strcpy(descrname,argv[i]);                     
	  if (argv[i]!=NULL && remainderindex==3)
	    strcpy(vcdname,argv[i]);                     
	  if (argv[i]!=NULL && remainderindex==4)
	    strcpy(capname,argv[i]);                     
	  remainderindex++;
	}
      }
#ifdef _MSC_VER
//   strcpy(filename,"z:\\junk\\sram_p080_membnk8kb.nt");
//   strcpy(descrname,"z:\\junk\\sram_p080_membnk8kb.descr");
   strcpy(filename,"z:\\junk\\fpu_14nm.nt");
   strcpy(descrname,"z:\\junk\\fpu.descr");
   strcpy(vcdname,"z:\\junk\\fadd.vcd.bz2");
//   strcpy(capname,"z:\\junk\\ap_mem_wbf.star.bz2");
//   strcpy(filename,"z:\\vlsi\\pp_exe_dp.nt.gz");
//   strcpy(descrname,"z:\\vlsi\\pp_exe_dp.descr");
//   strcpy(filename,"z:\\vlsi\\pp.nt");
//   strcpy(descrname,"z:\\vlsi\\pp.descr");
//   strcpy(vcdname,"z:\\junk\\tc14.vcd.bz2");
//   leakonly = true;
//   compareTimeStart = 2000;
   MAX_ERRORS = 50;
//   fineResolution = true;
//   strcpy(capname,"z:\\vlsi\\pp_mul_dp.star.gz");
//   scan   = true;
   noDump = true;
//     fineResolution = true;

#endif

   if (argc<=1 || help)
      {
      printf("\nSherlock usage:\nSherlock [-scan][-leakonly][-fine][-nodump][-nofloat][-stat][-reportwindow=#][-comparetime=#][-dumptime=#][-maxerrors=#][-powerreportmode=summary|detail][-powerstart=#][-powerend=#] foo.nt foo.descr foo.vcd [foo.star]\n");
      printf("The -leakonly option won't require a descript or vcd file.\n");
      printf("The -scan option won't require a vcd file.\n");
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
   if (vcdname[0]==0 && !leakonly && !scan)
      {
      printf("\nEnter a input trace filename->");
      gets(vcdname);
      }    
   if (bboxfilename[0]!=0)
     ReadBboxFile(bboxfilename);

   if (strstr(filename,".hsp")!=NULL)
      ReadSpiceWirelist(filename);
   else
      {
      array <const char *> vssNames, vddNames;
      vssNames.push("gnd");
      vssNames.push("gndl");
      vssNames.push("vss");
      vssNames.push("vssa");

      vddNames.push("vddl");
      vddNames.push("vdda");
      vddNames.push("vddo");
      vddNames.push("vddx");      
      vddNames.push("vdd");

      wl = ReadWirelist(filename, vddNames, vssNames);
      if (stat)
	PublishStats(wl);

      if (wl==NULL)
         {
         printf("\nI couldn't open %s",filename);
         shutdown(-1);
         }
      MarkAllBboxStructures();
      FlattenWirelist(wl);
      }
   if (capname[0]!=0)
      ReadCapfile(capname);

   filetype fp;
#ifdef _MSC_VER
   fp.fopen("signals.txt", "wt");
#else
   fp.fopen("signals.txt.gz", "wt");
#endif
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
   //Now we need to assign owners to flops. Also create a flopshash for fast lookup if needed.
   PopulateFlopsHash();
   AssignFlopOwnersForNodes();

   ZeroNodes();

   if (leakonly)
      {
      printf("\n");
      double leak, leakNom, leakHigh, leakHighPla34, leakNomPla4, leakNomPla8;
      
      noFloatingChecks = true;
      Initialize(false, true);
      LeakageEstimate();

      if (finfet)
         {
	   double leak, leakNom, leakLow, leakLow2, leakSuper, leakSuper2;
         leak     = SumLeakage();
         leakSuper = SumLeakage(VT_SUPERLOW);
         leakSuper2 = SumLeakage(VT_SUPERLOW_PLA2);
         leakLow = SumLeakage(VT_LOW);
	 leakLow2=SumLeakage(VT_LOW_PLA2);
         leakNom  = SumLeakage(VT_NOMINAL);
         printf("\n Estimated Leakage=%.4gmA Whatif(%.4gmA svt_0, %.4gmA svt_2, %.4gmA lvt_0, %.4gmA lvt_2, %.4gmA rvt_0).", leak/leakageIntervals, leakSuper/leakageIntervals, leakSuper2/leakageIntervals, leakLow/leakageIntervals, leakLow2/leakageIntervals, leakNom/leakageIntervals);
         LeakageReport();
         }
      else{
         double leak, leakNom, leakHigh, leakHighPla34, leakNomPla4, leakNomPla8;
         leak     = SumLeakage();
         leakHigh = SumLeakage(VT_HIGH);
         leakHighPla34 = SumLeakage(VT_HIGH_PLA4);
         leakNom  = SumLeakage(VT_NOMINAL);
         leakNomPla4  = SumLeakage(VT_NOMINAL_PLA4);
         leakNomPla8  = SumLeakage(VT_NOMINAL_PLA8);
         printf("\n Estimated Leakage=%.4gmA Whatif(%.4gmA rvt30, %.4gmA rvt34, %.4gmA rvt38, %.4gmA hvt30, %.4gmA hvt34).", leak/leakageIntervals, leakNom/leakageIntervals, leakNomPla4/leakageIntervals, leakNomPla8/leakageIntervals, leakHigh/leakageIntervals, leakHighPla34/leakageIntervals);
         LeakageReport();
         }
      return -1;
      }

   
   if (!scan)
      {
      if (vcdname[0]==0)
         {
         printf("\nI need a trace file(vcd) to run!");
         shutdown(-1);
         }
      inputVcd.OpenForRead(vcdname);
      //inputVcd.DumpAllSignals();
      inputVcd.GetLastToggleTime();
      }
   if (fineResolution)
      inputVcd.multiplier *=10;
   if (descrname[0]==0)
      {
      printf("\nI need a descript file to run!");
      return -1;
      }
   ReadDescr(descrname, scan);
   Initialize();              // initialize all nodes to legal values. (ie output of inverter is compliment of input)
   if (!noDump || (strcmp(powerreportmode,"detail")==0))
     {  //The vcd can be used to dump waves or we can use it to dump the power graph. Dumping the power graph only makes sense in the detail mode.
       outputVcd.OpenForWrite(outputname, nodes, powerreportmode);
     }
   if (strcmp(powerreportmode,"detail")!=0)
     outputVcd.WriteTimeSlice(-1, nodes);

   if (scan)
      ScanTest();
   else
      Simulate();
   printf("\n");
   if (finfet)
     {
       double leak, leakNom, leakLow, leakSuper, leakSuper2;
       leak     = SumLeakage();
       leakSuper = SumLeakage(VT_SUPERLOW);
       leakSuper2 = SumLeakage(VT_SUPERLOW_PLA2);
       leakLow = SumLeakage(VT_LOW);
       leakNom  = SumLeakage(VT_NOMINAL);
       if (!scan){
	 __int64 reportWindowPicoSeconds=(__int64) (reportWindow*1000);
	 printf("\n Total switched capacitance = %.4g fF, Average Leakage=%.4gmA(%.4gmA sct_0, %.4gmA svt_2, %.4gmA lvt_0, %.4gmA rvt_0), %5.0f ns simulated.", SumPower(), leak/leakageIntervals, leakSuper/leakageIntervals, leakSuper2/leakageIntervals, leakLow/leakageIntervals, leakNom/leakageIntervals, inputVcd.currentTime/1000.0);
	 printf("\n PeakPower=%.4gmW PeakPowerStart=%dns PeakPowerEnd=%dns\n", 0.5*POWER_VOLTAGE*POWER_VOLTAGE*peakPower/reportWindowPicoSeconds, peakPowerTimeStart/1000, peakPowerTimeEnd/1000);
       }
     }
   else{
     double leak, leakNom, leakHigh, leakNomPla4, leakNomPla8;
     leak     = SumLeakage();
     leakHigh = SumLeakage(VT_HIGH);
     leakNom  = SumLeakage(VT_NOMINAL);
     leakNomPla4  = SumLeakage(VT_NOMINAL_PLA4);
     leakNomPla8  = SumLeakage(VT_NOMINAL_PLA8);
     if (!scan){
       __int64 reportWindowPicoSeconds=(__int64) (reportWindow*1000);
       printf("\n Total switched capacitance = %.4g fF, Average Leakage=%.4gmA(%.4gmA Rvt30, %.4gmA Rvt34, %.4gmA Rvt38, %.4gmA Hvt30), %5.0f ns simulated.", SumPower(), leak/leakageIntervals, leakNom/leakageIntervals, leakNomPla4/leakageIntervals, leakNomPla8/leakageIntervals, leakHigh/leakageIntervals, inputVcd.currentTime/1000.0);
       printf("\n PeakPower=%.4gmW PeakPowerStart=%dns PeakPowerEnd=%dns\n", 0.5*POWER_VOLTAGE*POWER_VOLTAGE*peakPower/reportWindowPicoSeconds, peakPowerTimeStart/1000, peakPowerTimeEnd/1000);
     }
   }
   if (strcmp(powerreportmode, "summary")==0){
     //No cycle is provided, so this will be the average power report.
     PowerReport();
     FlopPowerReport();
   }
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

const int MAX_SCAN_LENGTH = 10000;
void simtype::ScanPulseClock(int &time, bool ignoreWarnings, char phase)
{
  int i;
  if( (phase=='A') || (phase=='X')){
    for (i=0; i<matches.size(); i++)
      {
	int index = matches[i].nodelist[0];
	if (matches[i].timing<0)
	  {
	    if (index<=VDDO) FATAL_ERROR;
	    nodes[index].value = V_ONE;
	    nodes[index].constant = true;
	    nodes[index].changed  = true;
	    nodes[index].inflight = true;
	    nodeheap.push(index, -32);
	  }
      }
    Propogate(time, ignoreWarnings);
    time+=500;
    if (!fineResolution && outputVcd.IsReadyToWrite()){
      outputVcd.WriteTimeSlice(time, nodes);
    }
  }
  if ((phase=='B')|| (phase=='X')){
    for (i=0; i<matches.size(); i++)
      {
	int index = matches[i].nodelist[0];
	if (matches[i].timing<0)
	  {
	    if (index<=VDDO) FATAL_ERROR;
	    nodes[index].value = V_ZERO;
	    nodes[index].constant = true;
	    nodes[index].changed  = true;
	    nodes[index].inflight = true;
	    nodeheap.push(index, -32);
	  }
      }
    Propogate(time, ignoreWarnings);
    time+=500;
    if (!fineResolution && outputVcd.IsReadyToWrite()){
      outputVcd.WriteTimeSlice(time, nodes);
    }
  }
}




void simtype::ScanTest()
{
  TAtype ta("simtype::ScanTest()");
  int i, k, row;
  int time=0;

  array <chaintype> chains;
  array<int> scanenables;
  int longestchain=-1;
  
  for (i=matches.size()-1; i>=0; i--)
    {
      if (matches[i].scan)
	{
	  chaintype c;
	  if (matches[i].nodelist.size()<=1)
            FATAL_ERROR;
	  else if (matches[i].nodelist[0]<=VDD)
            {
	      c.match = matches[i];         
	      c.in  = matches[i].nodelist[0];
	      c.out = -1;
	      c.length = -1;
            }
	  else if (matches[i].nodelist.size()==2)
            {
	      c.match = matches[i];         
	      c.in  = matches[i].nodelist[0];
	      c.out = matches[i].nodelist[1];
	      c.length = -1;
            }
	  else
            FATAL_ERROR;
	  chains.push(c);
	}
      else if (matches[i].scanenable)
	{
	  if (scanenables.size()>0)
            {
	      printf("I see more than one scan enable. This is unexpected but allowed.\n");
            }
	  if (matches[i].nodelist.size()>1 && matches[i].nodelist[0] == matches[i].nodelist[1])
            matches[i].nodelist.Remove(1);
	  if (matches[i].nodelist.size()!=1) 
            FATAL_ERROR;
	  scanenables.push(matches[i].nodelist[0]);
	  inputs.push(matches[i].nodelist[0]);
	}
    }
  if (scanenables.size()==0)
    {
      printf("\nYou need to provide a scan enable signal in the descript file. Use 'e'.\n");
      FATAL_ERROR;
    }
  for (i=matches.size()-1; i>=0; i--)
    {
      if (!matches[i].scan)
	{
	  for (k=0; k<matches[i].nodelist.size(); k++)
            {
	      if (scanenables.IsPresent(matches[i].nodelist[k]))
		{
		  matches.RemoveFast(i);// remove any i,x matches that collide with a scan enable(include the 'e se_in')
		  break;
		}
            }
	}
    }
  int cindex;
  for (cindex=0; cindex<chains.size(); cindex++)
    {
      chaintype &c = chains[cindex];
      for (i=matches.size()-1; i>=0; i--)
	{
	  for (k=0; k<matches[i].nodelist.size(); k++)
            {
	      if (matches[i].nodelist[k]==c.in && matches[i].input)
		{
		  matches.RemoveFast(i);
		  break;   // remove any i,x matches that collide with a chain input
		}
            }
	}
    }
  // now put back any chains that are vdd/vss
  for (cindex=chains.size()-1; cindex>=0; cindex--)
    {
      chaintype &c = chains[cindex];
      if (c.in<=VDD)
	{
	  c.match.input = true;
	  matches.push(c.match);
	  chains.RemoveFast(cindex);
	}
    }
  
  for (i=0; i<matches.size(); i++)
    {
      if (matches[i].input && matches[i].sigIndex.size()==0 && !matches[i].scanenable)
	{
	  for (k=0; k<matches[i].nodelist.size(); k++)
            {
	      int index = matches[i].nodelist[k];
	      if (index>VDDO) 
		{
		  if (stricmp(matches[i].behname, "vss")==0)
		    nodes[index].value = V_ZERO;
		  else if (stricmp(matches[i].behname, "vdd")==0)
		    nodes[index].value = V_ONE;
		  else if (stricmp(matches[i].behname, "vddo")==0)
		    nodes[index].value = V_ONE;
		  nodes[index].constant = true;
		  nodes[index].changed  = true;
		  nodes[index].inflight = true;
		  if (index>VDD)
		    nodeheap.push(index, 0);
		}
            }
	}
    }
  for (i=0; i<scanenables.size(); i++)
    {
      int se = scanenables[i];
      nodes[se].value = V_ONE;
      nodes[se].constant = true;
      nodes[se].changed  = true;
      nodes[se].inflight = true;
      nodeheap.push(se, 0);
    }
  for (i=0; i<chains.size(); i++)
    {
      int in = chains[i].in;
      if (in<=VDDO) FATAL_ERROR;
      nodes[in].constant = true;
      nodes[in].changed  = true;
      nodes[in].inflight = true;
      nodes[in].value = chains[i].match.complement ? V_ONE : V_ZERO;
      nodeheap.push(in, 0);
    }
  // I need to do this to prevent crowbar conditions where people mirror scan chains to prevent contention, there will still be contention at startup which I want to ignore
  for (i=0; i<512; i++)
    ScanPulseClock(time, true);   // put 100 clocks into the circuit before enabling warnings
  
  if (HARD_ERROR>=MAX_ERRORS)
    {
      printf("\n>>>Simulation ended due to exceeding the max error count(%d)!",MAX_ERRORS);
      return;
    }
  
  printf("\nClearing chains with 0\n");
  for (row=0; HARD_ERROR<MAX_ERRORS && row<MAX_SCAN_LENGTH; row++)
    {
      for (i=0; i<chains.size(); i++)
	{
	  int in = chains[i].in;
	  nodes[in].constant = true;
	  nodes[in].changed  = true;
	  nodes[in].inflight = true;
	  nodes[in].value = chains[i].match.complement ? V_ONE : V_ZERO;
	  nodeheap.push(in, 0);
	}
      ScanPulseClock(time);
    }                                        
  
  printf("\nMeasuring chain length\n");
  char phases[2]={'A', 'B'};
  for (row=0; HARD_ERROR<MAX_ERRORS && row<MAX_SCAN_LENGTH; row++)
    {
      for (int p=0; p<2; p++){
	char phase=phases[p];
	for (i=0; i<chains.size(); i++)
	  {
	    chaintype &c = chains[i];
	    int in  = c.in;
	    int out = c.out;
	    
	    if (in<=VDDO) FATAL_ERROR;
	    
	    bool this_node_can_change_in_this_phase=true;
	    if ((phase=='X') || (phase=='A')&&c.match.changes_in_a || (phase=='B')&&c.match.changes_in_b){
	      ;
	    }
	    else{
	      this_node_can_change_in_this_phase=false;
	    }
	    if(this_node_can_change_in_this_phase){
	      nodes[in].constant = true;
	      nodes[in].changed  = true;
	      nodes[in].inflight = true;
	      nodes[in].value = ((row==0) != c.match.complement) ? V_ONE : V_ZERO;
	      nodeheap.push(in, 0);
	      char indexstr[8]="";
	      if (nodes[in].bit>=0)
		sprintf(indexstr,"[%d]", nodes[in].bit);
	      
	      if (nodes[out].value==V_ONE && c.length>=0)
		{
		  printf("Chain %s%s is busted(I expected a zero at time %d)\n",nodes[in].name,indexstr,time/1000);
		  HARD_ERROR++;
		}
	      else if (nodes[out].value==V_ONE)
		{
		  printf("Chain %s%s is %d in length.\n",nodes[in].name,indexstr,row);
		  c.length = row;
		  c.fifo.resize(c.length, V_ZERO);
		  longestchain = MAXIMUM(longestchain, c.length);
		}
	    }
	  }
	ScanPulseClock(time, true, phase);
      }
    }                                        
  for (i=0; i<chains.size(); i++)
    {
      chaintype &c = chains[i];
      if (c.length<0)
	{
	  char indexstr[8]="";
	  if (nodes[c.in].bit>=0)
	    sprintf(indexstr,"[%d]", nodes[c.in].bit);
	  printf("Chain %s%s is busted, it appears to be stuck at zero(or greater than %d cells)\n",nodes[c.in].name,indexstr,MAX_SCAN_LENGTH);
	  HARD_ERROR++;
	}
    }
  if (HARD_ERROR>=MAX_ERRORS)
    {
      printf("\n>>>Simulation ended due to exceeding the max error count(%d)!",MAX_ERRORS);
      return;
    }
  printf("\nTesting with random scan data\n");
  //This has two parts. In the first part we inject some random values to regular inputs and scan inputs, pulse the clock and check the o/p that comes out.
  //The chains are initialsed to V_X to begin with, so, as we pulse the clock, we get V_X out the other end for a while and then a real value, which better match what we
  //had injected into the fifo. Once in a while this process is interrupted by a scan capture, which is also randomn and lasts for randomn number of cycles as well.
  //This part tests the continuity of the scan chain. The second part is to mostly verify that there are no contentions if the data were allowed to change in either phase.
  //This was introduced (circa: jan 2015) after we had a scan chain based escape in Thunder memory array scan. In this part, the we inject data in A phase and pulse clock in A phase,
  //and repeat the same for B phase. In the case of the memory scan, this will flag a contention, because the data should only be allowed to change in one phase and not the other.
  //Since we allow the data to change in either phase, we expect to see a contention.

  printf("\nTesting chain continuity ...\n");
  ResetChainsToVX(chains);  
  for (row=0; HARD_ERROR<MAX_ERRORS && row<longestchain*200; row++)
    {
      char phases[2]={'A','B'};
      for (int p=0; p<2; p++){
	char phase=phases[p];
	InjectRandomnValuesToInputs(phase);
	InjectRandomnValuesinChainsAndOptionallyCheckOutput(chains, time, phase, true);
	double randomval=rand()%RAND_MAX; randomval/=RAND_MAX; 
	if (randomval<(0.25/longestchain)){
	  printf("\nPulsing scanenable at time %d ns(%d random cycles).", time/1000, row);
	  PulseScanEnableForCapture(scanenables, time);
	  ResetChainsToVX(chains);
	}
	ScanPulseClock(time, false, phase);
      }
      if (row%1000==0)
	printf("\nSimulating past time %d ns(%d random cycles).", time/1000, row);
    }// end part1
  printf("\nTesting for contention in either phase ...\n");
  for (row=0; HARD_ERROR<MAX_ERRORS && row<longestchain; row++)
    {
      InjectRandomnValuesToInputs('A');
      InjectRandomnValuesinChainsAndOptionallyCheckOutput(chains, time, 'A', false);
      ScanPulseClock(time, false, 'A');
      
      InjectRandomnValuesToInputs('B');
      InjectRandomnValuesinChainsAndOptionallyCheckOutput(chains, time, 'B', false);
      ScanPulseClock(time, false, 'B');
      if (row%1000==0)
	printf("\nSimulating past time %d ns(%d random cycles).", time/1000, row);
    }// end part2
  
}


void simtype::Simulate()
   {
   TAtype ta("simtype::Simulate()");
   int i, k, bit, row;
   __int64 oldtime=0;
   double priorPower = 0.0, priorLeakage = 0.0;
   int priorIntervals=0;
   bool compare=false;
   int clknode=-1;


   float fraction_untested=1000.0*compareTimeStart/inputVcd.lastTime;//compareTime is specified by the user in ns, hence the factor of 1000
   if (fraction_untested > 0.5)
     ErrorLog("\n>Your comparetime may be too high.(compare starts at %d out of %d total cycles). This could be a user error.\n",compareTimeStart, inputVcd.lastTime/1000);

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
            else if (stricmp(matches[i].behname, "vdd")==0 || matches[i].scanenable)
               nodes[index].value = V_ONE;
            else if (stricmp(matches[i].behname, "vddo")==0)
               nodes[index].value = V_ONE;
            else
               FATAL_ERROR;
            nodes[index].constant = true;
            nodes[index].changed  = true;
            nodes[index].inflight = true;
            nodeheap.push(index, matches[i].timing);
            }
      }   
   
     printf("\nClocking");

   for (row=0; HARD_ERROR<MAX_ERRORS; row++)
      {
      if (!inputVcd.ReadTimeSlice())
         break;
      double randomval=rand()%RAND_MAX; randomval/=RAND_MAX;
      if (randomval<0.01)
	LeakageEstimate();  // Only sample leakage every so often so it doesn't slow everything down
      //For convininece sake, we allow the user to specify the window in ns. (Eg. report every 1000 ns) but the unit of time inside
      //this code is normalised to ps. Hence the factor of 1000 below.
      __int64 reportWindowPicoSeconds=(__int64) (reportWindow*1000);
      if (inputVcd.currentTime/reportWindowPicoSeconds != oldtime/reportWindowPicoSeconds)
	{
	  printf("\nSimulating past time %d ns.", inputVcd.currentTime/1000);
	  double deltaPower   = SumPower()   - priorPower;
	  double deltaLeakage = SumLeakage() - priorLeakage;
	  int   deltaIntervals = leakageIntervals - priorIntervals;
	 if (deltaPower > peakPower){
	   peakPower=deltaPower;
	   peakPowerTimeStart=inputVcd.currentTime-reportWindowPicoSeconds;
	   peakPowerTimeEnd=inputVcd.currentTime;
	 }
         if (deltaIntervals==0)
            {
            LeakageEstimate();
            deltaIntervals = leakageIntervals - priorIntervals;
            }
	   printf(" P=1/2CV^2, C=%8.3g fF/ps, P(@%.1fV) = %6.5fmW, Ioff=%.2fmA",deltaPower*1.0 /reportWindowPicoSeconds,POWER_VOLTAGE,0.5*POWER_VOLTAGE*POWER_VOLTAGE*deltaPower/reportWindowPicoSeconds, deltaLeakage/deltaIntervals);
	   priorPower += deltaPower;
	   if ((strcmp(powerreportmode,"detail")==0)&& (powerstart<=inputVcd.currentTime/1000) && (powerend>=inputVcd.currentTime/1000)) {
	     PowerReport(inputVcd.currentTime/1000);
	     FlopPowerReport(inputVcd.currentTime/1000);
	   }
	   priorLeakage   += deltaLeakage;
	   priorIntervals += deltaIntervals;
	   oldtime = inputVcd.currentTime;
	}
/*
      if (inputVcd.currentTime>=199068000)
         {
         printf("\ntime = %d %d\n",inputVcd.currentTime/1000,inputVcd.currentTime%1000);
         printf("%s = %d\n",nodes[1033].name,nodes[1033].value);
         printf("%s = %d\n",nodes[1043].name,nodes[1043].value);
         printf("%s = %d\n",nodes[1049].name,nodes[1049].value);
         printf("%s = %d\n",nodes[1048].name,nodes[1048].value);
         printf("%s = %d\n",nodes[124].name,nodes[124].value);
         }
      if (inputVcd.currentTime>=199076000)
         printf("");// erase me
*/
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
               if (offset<0) offset=0;
               enum valuetype value;                             
               if (s.binarystate.GetBit(offset) ^ matches[i].complement)
                  value = V_ONE;
               else
                  value = V_ZERO;
               if (nodes[index].value != value)  {
		 nodes[index].toggleCount++;
		 FlipFlop *owner=nodes[index].m_flop_owner;
		 if (owner !=NULL){
		   if(owner->m_last_ttime != inputVcd.currentTime){
		     owner->m_tc++;
		     owner->m_sw_cap+=nodes[index].capacitance;
		     owner->m_last_ttime=inputVcd.currentTime;
		   }
		 }
	       }
               nodes[index].value = value;

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
      bool compareEnable = (inputVcd.currentTime/1000 > compareTimeStart);
      if (compare != compareEnable)
         {
         printf("\nEnabling short, floating, metastable node checks, clearing power info, and enabling compares at time %dns",inputVcd.currentTime/1000);
         ClearPower();
         }
      compare = compareEnable;
      Propogate(inputVcd.currentTime, !compareEnable);
      
      if (strcmp(powerreportmode,"detail")==0)
         {
         static bool flipflop;
         static double priorPower[10];
         static double priorTime[10];
         flipflop = !flipflop; // dump every other timeslice(once per cycle) to be faster
         if (flipflop){
            double power   = SumPower();
            double deltaPower   = MAXIMUM(0.0, power   - priorPower[0]);
            double deltaTime    = inputVcd.currentTime - priorTime[0];
            double longdeltaPower   = MAXIMUM(0.0, power   - priorPower[9]);
            double longdeltaTime    = inputVcd.currentTime - priorTime[9];
            //         double deltaLeakage = SumLeakage() - priorLeakage;  // too slow
//            outputVcd.WritePowerSlice(inputVcd.currentTime, 0.5*POWER_VOLTAGE*deltaPower/deltaIntervals, 0.5*POWER_VOLTAGE*power, deltaLeakage/deltaIntervals);
            outputVcd.WritePowerSlice(inputVcd.currentTime, 0.5*POWER_VOLTAGE*deltaPower/deltaTime, 0.5*POWER_VOLTAGE*power, 0.5*POWER_VOLTAGE*longdeltaPower/longdeltaTime);
            memmove(priorPower+1, priorPower+0, sizeof(priorPower[0])*9);
            memmove(priorTime+1,  priorTime+0,  sizeof(priorTime[0])*9);
            priorPower[0]   = power;
            priorTime[0]    = inputVcd.currentTime;
            }
         }
      else {
	if (!fineResolution && outputVcd.IsReadyToWrite() && inputVcd.currentTime/1000 > dumpTime){
	  outputVcd.WriteTimeSlice(inputVcd.currentTime, nodes);
	}
      }
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

            bool compare_in_this_phase=true;
            if (clknode>0 && !match.checka && nodes[clknode].value==V_ONE)
               compare_in_this_phase = false;
            if (clknode>0 && !match.checkb && nodes[clknode].value==V_ZERO)
               compare_in_this_phase = false;
            if ((wirestate | mask) != (behstate | mask) && compare_in_this_phase)
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
      }                                        
   }                


void simtype::LeakageEstimate()
   {
   TAtype ta("LeakageEstimate()");
   int i, n;
   array <int> statenodes;
   array <int> ramnodes;

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
         Eval(n,drive0, drive1, statenodes, ramnodes);
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
      if (strstr(d.name, "spare_eclk_svn89577_0_0_3")!=NULL)
         printf(""); // erase me

      if (!d.on && nodes[d.source].value != nodes[d.drain].value && nodes[d.source].value!=V_X && nodes[d.drain].value!=V_X)
         {
         // now I want to check both source and drain and see if they are strongly driven or floating
         bool source=d.source<=VDDO, drain=d.drain<=VDDO;
         int k;
         for (k=0; k<nodes[d.source].sds.size() && !source; k++)
            {
            const devicetype &d2 = *nodes[d.source].sds[k];
            if (d2.on)
               source = true;
            }
         for (k=0; k<nodes[d.drain].sds.size() && !drain; k++)
            {
	      const devicetype &d2 = *nodes[d.drain].sds[k];
            if (d2.on)
               drain = true;
            }

         d.offcount += (source && drain) ? 1.0 : d.Leak2HighRDerate();
         }
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
class leakreporttype{
public:
   transistortype type;
   vttype vt;
   double width, current;
   leakreporttype(const transistortype _type, const vttype _vt)
      {
      type = _type;
      vt = _vt;
      width = 0.0;
      current = 0.0;
      }
   bool operator==(const leakreporttype &right) const
      {
      return type==right.type && vt==right.vt;
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
   array <leakreporttype> rep_list;

   // I now want to compile a list of signals from my wirelist. I'll only get things that touch a gate
   for (i=0; i<devices.size(); i++)
      {
      devicetype &d = devices[i];
      if (d.offcount>=0.0)
         {
         double leakage = d.Leakage() * d.offcount;
         leakage = leakage / leakageIntervals;
         devicesorts.push(devicesorttype(stringPool.NewString(devices[i].name), i, leakage));
         total += leakage;

         int index = rep_list.LinearSearch(leakreporttype(d.type, d.vt));
         if (index<0)
            {
            index = rep_list.size();
            rep_list.push(leakreporttype(d.type, d.vt));
            }
         rep_list[index].width += d.width;
         rep_list[index].current += leakage;
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

#ifdef DUKE
   // now cross reference stuff from iddq files from Duke
      {
      FILE *fptr=fopen("z:\\vlsi\\iddq_sim_pp_mrc.pa0","rt");
      if (fptr==NULL) FATAL_ERROR;
      char buffer[1024];
      array <const char *> spice;
      array <float> current;
      char *chptr, *chptr2;
      texthashtype<int> h;
      spice.push("zero entry");
      while (NULL!=fgets(buffer, sizeof(buffer), fptr))
         {
         chptr = buffer+0;
         int index=-1;
         while (*chptr==' ' || *chptr=='\t')
            chptr++;
         index = atoi(chptr);
         if (index<=0) break;
         while (*chptr>='0' && *chptr<='9')
            chptr++;
         chptr+=2;
         chptr2 = strchr(chptr,'.');
         if (chptr2==NULL)
            FATAL_ERROR;
         *chptr2=0;
         if (spice.size()!=index)
            FATAL_ERROR;
         spice.push(stringPool.NewString(chptr));
         }
      for (i=0; i<spice.size(); i++)
         {
         h.Add(spice[i], i);
         current.push(0.0);
         }
      fclose(fptr);
      fptr=fopen("z:\\vlsi\\currents.txt","rb");
      if (fptr==NULL) FATAL_ERROR;
      int index=-1, len;
      array <char> txt;
      do {
         len=fread(buffer, 1, sizeof(buffer), fptr);
         for (i=0; i<len; i++)
            txt.push(buffer[i]);
         }while (len==sizeof(buffer));
      txt.push(0);
      fclose(fptr);

      chptr=&txt[0];
      strlwr(chptr);
      while (true)
         {
         char *chptr2 = strstr(chptr, ":drain");
         if (chptr2==NULL)
            break;
         chptr = chptr2;
         chptr2--;
         while (*chptr2>='0' && *chptr2<='9')
            chptr2--;
         index = atoi(chptr2+1);
         if (index<=0)
            FATAL_ERROR;
         while (*chptr<'0' || *chptr>'9')
            chptr++;

         float f=0.0;
         f = atof(chptr);
         if (strchr(chptr,'n'))
            f = f * 1.0e-9;
         else if (strchr(chptr,'p'))
            f = f * 1.0e-12;
         else if (strchr(chptr,'u'))
            f = f * 1.0e-6;
         else if (strchr(chptr,'m'))
            f = f * 1.0e-3;
         else if (strchr(chptr,'f'))
            f = f * 1.0e-12;
         if (index>=current.size()) FATAL_ERROR;
         current[index] = f;
         index = -1;
         }


#ifdef _MSC_VER
      fp.fopen("leakage.txt","wt");
#else
      fp.fopen("leakage.txt.gz","wt");
#endif
      for (i=0; i<devicesorts.size(); i++)
         {
         if (!devicesorts[i].deleted && devicesorts[i].leakage>0.0)
            {
            if (devicesorts[i].msb > devicesorts[i].lsb)
               {
               char buffer[1024];
               strcpy(buffer, devicesorts[i].name);            
               chptr = strchr((char*)devicesorts[i].name, ']');
               if (chptr==NULL) FATAL_ERROR;
               sprintf(buffer+devicesorts[i].len,"[%d:%d]%s",devicesorts[i].msb,devicesorts[i].lsb,chptr+1);
               fp.fprintf("%8.5f%%, %10.2e, %s",(double)100.0*devicesorts[i].leakage/total,(double)devicesorts[i].leakage,buffer);
               }
            else
               fp.fprintf("%8.5f%%, %10.2e, %s",(double)100.0*devicesorts[i].leakage/total,(double)devicesorts[i].leakage,devicesorts[i].name);
            strcpy(buffer, devicesorts[i].name);
            strlwr(buffer);
            if (h.Check(buffer, index))
               {
               fp.fprintf(",%.3e",current[index]);
               }

            fp.fprintf("\n");
            }
         }   
      fp.fclose();
      return;
      }
#endif
#ifdef _MSC_VER
   fp.fopen("leakage.txt","wt");
#else
   fp.fopen("leakage.txt.gz","wt");
#endif
   for (i=0; i<rep_list.size(); i++)
      {
	if(finfet){
	   fp.fprintf("%s %5s %.0ffins %10.2emA\n",rep_list[i].type==D_NMOS ? "NMOS" : rep_list[i].type==D_PMOS ? "PMOS" : "Unknown", 
		      rep_list[i].vt==VT_NOMINAL ? "Rvt14" : rep_list[i].vt==VT_NOMINAL_PLA2 ? "Rvt16" : 
		      rep_list[i].vt==VT_LOW ? "Lvt14" : rep_list[i].vt==VT_LOW_PLA2 ? "Lvt16" :  
		      rep_list[i].vt==VT_SUPERLOW ? "SLvt14" : rep_list[i].vt==VT_SUPERLOW_PLA2 ? "SLvt16" :  
		      rep_list[i].vt==VT_SRAM    ? "Sram" : "Unknown", 
		      rep_list[i].width, rep_list[i].current*0.000001); 
	}
	else {
	  fp.fprintf("%s %5s %.0fum %10.2emA\n",rep_list[i].type==D_NMOS ? "NMOS" : rep_list[i].type==D_PMOS ? "PMOS" : "Unknown", 
		     rep_list[i].vt==VT_NOMINAL ? "Rvt30" : rep_list[i].vt==VT_NOMINAL_PLA4 ? "Rvt34" : rep_list[i].vt==VT_NOMINAL_PLA8 ? "Rvt38" : 
		     rep_list[i].vt==VT_HIGH    ? "Hvt30" : rep_list[i].vt==VT_HIGH_PLA4    ? "Hvt34" : rep_list[i].vt==VT_HIGH_PLA8    ? "Hvt38" : 
		     rep_list[i].vt==VT_LOW     ? "Lvt30" : rep_list[i].vt==VT_LOW_PLA4     ? "Lvt34" : rep_list[i].vt==VT_LOW_PLA8     ? "Lvt38" : 
		     rep_list[i].vt==VT_SRAM    ? "Sram" : "Unknown", 
		     rep_list[i].width, rep_list[i].current*0.000001); 
	}
      }
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
            fp.fprintf("%8.5f%%, %10.2e, %s\n",(double)100.0*devicesorts[i].leakage/total,(double)devicesorts[i].leakage,buffer);
            }
         else
            fp.fprintf("%8.5f%%, %10.2e, %s\n",(double)100.0*devicesorts[i].leakage/total,(double)devicesorts[i].leakage,devicesorts[i].name);
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
   double power, toggle;

   nodesorttype(const char *_name, int _index, double _cap=0.0, double _toggle=0.0) : name(_name), power(_cap*_toggle), toggle(_toggle)
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
void simtype::PowerReport(int cycle)
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
      int toggle = nodes[i].toggleCount - nodes[i].toggleTare;
      nodes[i].toggleTare = nodes[i].toggleCount;
      if (nodes[i].state)
         nodesorts.push(nodesorttype(stringPool.NewString(nodes[i].RName(buffer)), i, nodes[i].capacitance, toggle));
      total += nodes[i].capacitance*toggle;
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
         nodesorts[last].toggle += nodesorts[i].toggle;
         nodesorts[i].deleted = true;
         }
      else
         {
         last = i;
         lastchptr = strchr(nodesorts[i].name, ']');
         }
      }
   std::sort(nodesorts.begin(), nodesorts.end(), powerPredicatetype());  // now sorted by power
   char filename[100];
   if (cycle>0)
      sprintf(filename,"power_%d.txt.gz",cycle);
   else
      strcpy(filename,"power.txt.gz");
   fp.fopen(filename,"wt");
   fp.fprintf(" power%%    power  toggles/bit bus\n");
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
            fp.fprintf("%8.5f%% %10.2e %8.0f %s\n",(double)100.0*nodesorts[i].power/total, (double)nodesorts[i].power, (double)nodesorts[i].toggle/(nodesorts[i].msb-nodesorts[i].lsb+1), buffer);
            }
         else
            fp.fprintf("%8.5f%% %10.2e %8.0f %s\n",(double)100.0*nodesorts[i].power/total, (double)nodesorts[i].power, (double)nodesorts[i].toggle, nodesorts[i].name);
         }
      }   
   fp.fclose();
   }

void simtype::FlopPowerReport(int cycle){
  char filename[100];
  if (cycle>0)
    sprintf(filename,"floppower_%d.txt.gz",cycle);
  else
    strcpy(filename,"floppower.txt.gz");
  filetype fp;
  fp.fopen(filename,"wt");
  fp.fprintf("%-100s %-20s\n", "FlopName", "Power @0.9V(mW)");
  float total_flop_power=0.0;
  for(int i=0; i<flops.size(); i++){
    float sw_cap=flops[i]->m_sw_cap-flops[i]->m_sw_capTare;
    flops[i]->m_sw_capTare=flops[i]->m_sw_cap;
    float flop_power_in_mW=1e-3*0.5*POWER_VOLTAGE*POWER_VOLTAGE*sw_cap/reportWindow; //reportWindow in nS; so this number is 1e-3=mW.
    fp.fprintf("%-100s %.6f\n", flops[i]->m_name, flop_power_in_mW);
    total_flop_power+=flop_power_in_mW;
  }
  fp.fprintf("Total Flop Power=%.6fmW\n", total_flop_power);
  fp.fclose();
}
void vcdtype::OpenForWrite(const char *_filename, array <nodetype> &nodes, char* powerreportmode)
   {
   TAtype ta("Opening output VCD file for writing.");
   array <nodesorttype> nodesorts;
   int i;
   const char *chptr, *lastchptr;
   char ref[6];

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
   ref[4]=0;
   for (i=0; i<nodesorts.size() && strcmp(powerreportmode,"detail")!=0; i++)
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
            ref[3]=33;
         if (ref[3]>126)
            {
            ref[3]=33;
            ref[4]++;
            }
         if (ref[4]==1)
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
   for (i=0; i<sigs.size() && strcmp(powerreportmode,"detail")!=0; i++)
      {
      for (int k=0; k<sigs[i].nodes.size(); k++)
         {
         nodes[sigs[i].nodes[k]].outputSigChangedPtr = &sigs[i].changed;
         }
      sigs[i].changed = true;  // will force an initial dump out
      }
   if (strcmp(powerreportmode,"detail")==0)
     {
       fp.fprintf("$var real 64 %c Dynamic_Current_Smooth $end\n",33+0);
       fp.fprintf("$var real 64 %c Dynamic_Current $end\n",33+1);
       fp.fprintf("$var real 64 %c Dynamic_Integral $end\n",33+2);
     }
   fp.fprintf("$upscope $end\n\n");
   fp.fprintf("$enddefinitions $end\n\n");

   fp.fprintf("$dumpvars\n\n");
   //fp.fprintf("#0\n");
   }

void vcdtype::WritePowerSlice(int newTime, double dynamic, double dynamic_integral, double smooth)
   {
   TAtype ta("vcdtype::WritePowerSlice");
   
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
   if (dynamic<0)
      FATAL_ERROR;
   fp.fprintf("r%.10g %c\n",smooth, 33+0);
   fp.fprintf("r%.10g %c\n",dynamic, 33+1);
   fp.fprintf("r%.10g %c\n",dynamic_integral, 33+2);
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
            strcat(buffer,".");
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
            printf("\nBehavioral signal '%s' is too wide, it will be ignored. The limit is %d bits, ",signame,MAX_SIGLENGTH);
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
   char buffer[65*1024];

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

         if (startptr==NULL) {
	   FATAL_ERROR;
	 }
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
         nextTime = (__int64)((double)atoint64(buffer+1) * multiplier);
         return true;
         }
      }
   return false;
   }




void simtype::ReadDescr(char *filename, bool scanonly)
{
  TAtype ta("Reading descript file");
  char buffer[200], *chptr, behname[200], wirename[200], *behstart, *wirestart, *wiretail, *phasestart;
  char rtlPrepend[256]="";
  char wirePrepend[256]="";
  FILE *fptr;              
  bool input, output, complement, smart, scan, scanenable, nofloat;
  int line=0, kill=false, i;
  int clock;
  int checka, checkb, changes_in_a, changes_in_b, totalinputs=0, totaloutputs=0;
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
      changes_in_a=changes_in_b=true; //Used only by scan 
      clock=false;
      scan=false;
      scanenable = false;
      nofloat = false;
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
      else if (strncmp(buffer,"s ",2)==0)
	{
	  scan=scanonly; // found an scan chain
	}
      else if (strncmp(buffer,"e ",2)==0)
	{
	  scanenable=scanonly; // found an scan chain
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
      else if (strncmp(buffer,"f ",2)==0)
	nofloat=true; // disable floating node checks on this one bit/bus
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
      
      if (input || output || smart || scan || scanenable || nofloat)
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
	  *chptr=0; // null terminate behstart
	  
	  chptr=wirestart;
	  while ( *chptr==' ' || *chptr==9 || *chptr=='!' || *chptr=='~')
	    {
	      if (*chptr=='!' || *chptr=='~') complement= !complement;
	      chptr++;
	    }
	  wirestart = chptr;
	  //Get to the end of wirestart
	  while ( *chptr!=' ' && *chptr!=9 && *chptr!='\n' && *chptr!=0)
            chptr++;
	  phasestart=chptr+1;
	  if (*chptr==0)
	    phasestart=chptr;
	  *chptr=0; //null terminate wirestart
	  
	  chptr=phasestart;
	  while ( *chptr==' ' || *chptr==9 || *chptr=='!' || *chptr=='~')
	    {
	      chptr++;
	    }
	  phasestart=chptr;
	  
	  //behstart could be like a/b/c[9:0] or just a/b/c. No vectoring in the middle.
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
            strcpy(behname, (scan||nofloat||scanenable) ? wirePrepend : rtlPrepend);
	  else
            strcpy(behname,"");
	  strcat(behname, behstart);
	  
	  wiretail="";
	  if (wirestart[0]==0)
            {
	      wirestart = behstart;
	      wirelsb = behlsb;
	      wiremsb = behmsb;
            }
	  
	  //wirestart could be like a/b/c[9:0] or a/b[9:0]/c or a/b/c
	  chptr = strchr(wirestart,'[');
	  if (chptr!=NULL)
            {
	      *chptr=0;
	      chptr++;
	      char* start_of_numerical=chptr;
	      wiremsb = wirelsb = atoi(chptr);
	      chptr=strchr(chptr,':');
	      if (chptr!=NULL)
		wirelsb=atoi(chptr+1);
	      if (strchr(start_of_numerical,']')!=NULL)
		wiretail = strchr(start_of_numerical,']')+1;
	      else
		FATAL_ERROR;
            }
	  strcpy(wirename, wirePrepend);
	  strcat(wirename, wirestart);
	  
	  chptr=phasestart;
	  while (*chptr!=' ' && *chptr!=9 && *chptr!=0 && *chptr!='\n' && *chptr!='\r')
            chptr++;
	  
	  *chptr=0; //Null terminate phase term.
	  if(*phasestart==0){;}
	  else if (*(phasestart)=='a')
	    changes_in_b=false;
	  else if (*(phasestart)=='b')
	    changes_in_a=false;
	  else if (*(phasestart)=='x')
	    changes_in_a=changes_in_b=true;
	  else{
	    printf("\nYour syntax for the scan input is not correct. The phase of the signal has to be either a, b or x. You provided %s\n", phasestart);
	    FATAL_ERROR;
	  }
	  match.behname  = stringPool.NewString(behname);
	  if (strcmp(wirename, behname)!=0)
            match.wirename = stringPool.NewString(wirename);
	  else
            match.wirename = match.behname;
	  match.complement = complement;
	  match.checka = checka;
	  match.checkb = checkb;
	  match.changes_in_a=changes_in_a;
	  match.changes_in_b=changes_in_b;
	  match.input  = input;
	  match.scan   = scan;
	  match.scanenable = scanenable;
	  
	  if (stricmp(match.behname, "vss")==0 || stricmp(match.behname, "vdd")==0)
            ;
	  else if (behmsb-behlsb != wiremsb-wirelsb)
            {
	      printf("\nYour array subscripts don't match up between wirelist(%s) and behavioral(%s)\nLine %d of %s.",match.wirename,match.behname,line,filename);
	      HARD_ERROR++;
            }         
	  
	  i=0;
	  if (scan || scanenable)
            {// behavioral signal will really be schematic signal, scan in also lets only allow single indexing, if at all. like si[0]
	      int index;
	      
	      if (behlsb<0)
		sprintf(behname,"%s",match.behname);
	      else{
		if (behlsb != behmsb){
		  printf("Scan commands cannot contain multiple bits in a single command. Line %d of %s", line, filename);
		  FATAL_ERROR;
		}
		sprintf(behname,"%s[%d]",match.behname,behlsb);
	      }
	      if (!h.Check(behname, index))
		{
		  printf("\nI couldn't match your wirelist signal name(%s) to anything in your spice wirelist.\nLine %d of %s.",behname,line,filename);
		  HARD_ERROR++;
		  kill = true;
		}
	      else if (nodes[index].sds.size())
		{
		  printf("\nYour scan in signal is going to a source drain.\nLine %d of %s.",behname,line,filename);
		  HARD_ERROR++;
		  kill = true;
		}
	      else {
		if (index<=VDDO)
                  index = -1;
		else{
                  totalinputs++;
                  inputs.push(index);
		}
		match.nodelist.push(index);
	      }
            }
	  else if (stricmp(match.behname, "vss")==0 || stricmp(match.behname, "gnd")==0)
            ;  // sigIndex.size()==0 as signal that is power node
	  else if (stricmp(match.behname, "vdd")==0)
            ;
	  else if (!scanonly)
            {
	      for (i=inputVcd.aliases.size()-1; i>=0; i--)
		{
		  if (strncmp(inputVcd.aliases[i].hierarchy, match.behname, strlen(inputVcd.aliases[i].hierarchy))==0)
		    {
		      char mergedname[2048];
		      strcpy(mergedname, inputVcd.aliases[i].hierarchy);
		      strcat(mergedname, inputVcd.aliases[i].name);
		      if (strcmp(mergedname, match.behname)==0 && ((behmsb <= inputVcd.sigs[inputVcd.aliases[i].sigIndex].msb && behlsb >= inputVcd.sigs[inputVcd.aliases[i].sigIndex].lsb) ||
								   (behmsb<0 && inputVcd.aliases[i].bit<0)))
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
	  else if (nofloat)
            {
	      kill = true;
            }
	  
	  if (!scanonly && i<0 && match.sigIndex.size()==0)
            {
	      printf("\nI couldn't match your behavioral signal name(%s) to anything in the vcd dump file.\nLine %d of %s.",match.behname,line,filename);
	      HARD_ERROR++;
	      kill = true;
            }
	  else if (!scanonly)
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
	      if (nofloat)
		{
		  nodes[index].nofloatcheck = true;
		}
	      else if (smart && !nodes[index].sds.size())
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
	      if (match.input || match.scanenable || (match.scan && match.nodelist[0]<=VDD))
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
	      match.timing=-32;
	      printf("\nThe input signal %s, %s will be retimed for %d gate delays relative to everything else.",match.behname,match.wirename,match.timing);
            }
	  if (!kill)
            matches.push(match);
	}
    }
  fclose(fptr);
  if (!kill)
    {
      if (!scanonly)
	{
	  bool clockfound = false;
	  for (i=0; i<matches.size(); i++)
            {
	      if (matches[i].timing<0)
		clockfound = true;
            }
	  if (!clockfound)
            printf("\nWARNING: I didn't see any clocks defined in your descript file.\n");
	  printf("\n%d signals will be driven from trace file. %d signals will be compared against.",totalinputs,totaloutputs);
	}
    }
  else
    shutdown(-1);   
}                       

void simtype::ResetChainsToVX(array<chaintype>& chains){
  int i=0;
  for (i=0; i<chains.size(); i++)
    {
      chaintype &c = chains[i];
      for (int k=0; k<c.fifo.size(); k++)
	c.fifo[k]=V_X; // doing a capture will make the chain unpredictable
    }
}
void simtype::InjectRandomnValuesToInputs(char phase){
  int i=0; double randomval=rand()%RAND_MAX; randomval/=RAND_MAX;
  for (i=0; i<matches.size(); i++)
    {
      if (matches[i].input)
	{
	  for (int k=0; k<matches[i].nodelist.size(); k++)
	    {
	      int index = matches[i].nodelist[k];
	      bool this_node_can_change_in_this_phase=true;
	      if ((phase=='X') || (phase=='A')&&matches[i].changes_in_a || (phase=='B')&&matches[i].changes_in_b){
		;
	      }
	      else{
		this_node_can_change_in_this_phase=false;
	      }
	      if (this_node_can_change_in_this_phase){
		if (index>VDDO){
		  bool val;
		  if (stricmp(matches[i].behname, "vss")==0)
		    val = false;
		  else if (stricmp(matches[i].behname, "vdd")==0)
		    val = true;
		  else if (stricmp(matches[i].behname, "vddo")==0)
		    val = true;
		  else
		    val = randomval<0.5;
		  nodes[index].constant = true;
		  nodes[index].changed  = true;
		  nodes[index].inflight = true;
		  nodes[index].value = val ? V_ONE : V_ZERO;
		  nodeheap.push(index, 0);
		}
	      }
	    }
	}
    }
}
void simtype::PulseScanEnableForCapture(array<int>& scanenables, int& time){
  int i=0; double randomval=rand()%RAND_MAX; randomval/=RAND_MAX;

  for (i=0; i<scanenables.size(); i++)
    {
      int se = scanenables[i];
      
      nodes[se].constant = true;
      nodes[se].changed  = true;
      nodes[se].inflight = true;
      nodes[se].value = V_ZERO;
    }
  do{
    ScanPulseClock(time);
    randomval=rand()%RAND_MAX; randomval/=RAND_MAX;
  }while (randomval<0.5);
  for (i=0; i<scanenables.size(); i++)
    {
      int se = scanenables[i];
      nodes[se].constant = true;
      nodes[se].changed  = true;
      nodes[se].inflight = true;
      nodes[se].value = V_ONE;
    }
}
void simtype::InjectRandomnValuesinChainsAndOptionallyCheckOutput(array<chaintype>& chains, int& time, char phase, bool check_output){
  int i=0; 
  for (i=0; i<chains.size(); i++)
    {
      chaintype &c = chains[i];
      int in  = c.in;
      bool this_node_can_change_in_this_phase=true;
      if ((phase=='X') || (phase=='A')&&c.match.changes_in_a || (phase=='B')&&c.match.changes_in_b){
	;
      }
      else{
	this_node_can_change_in_this_phase=false;
      }
      if (this_node_can_change_in_this_phase){
	int out = c.out;
	double randomval=rand()%RAND_MAX; randomval/=RAND_MAX;
	bool input = randomval<0.5;
	nodes[in].constant = true;
	nodes[in].changed  = true;
	nodes[in].inflight = true;
	
	nodes[in].value = (input != c.match.complement) ? V_ONE : V_ZERO;
	nodeheap.push(in, 0);
	if(check_output){
	  c.fifo.push(input ? V_ONE:V_ZERO);
	  valuetype output = c.fifo[0];
	  c.fifo.Remove(0);
	  if (nodes[out].value != output && output!=V_X)
	    {
	      char indexstr[8]="";
	      if (nodes[in].bit>=0)
		sprintf(indexstr,"[%d]", nodes[in].bit);
	      printf("Chain %s%s is busted(I expected a %d at time %d)\n",nodes[in].name,indexstr,output,time/1000);
	      HARD_ERROR++;
	    }
	}
      }
    }
}
int vcdtype::GetLastToggleTime(){
  TAtype ta("vcdtype::GetLastToggleTime()");
  char buffer[65*1024];
  filetype vcd;

  vcd.fopen(filename, "rt");
  while (NULL != vcd.fgets(buffer, sizeof(buffer)-1)){
    if(buffer[0]=='#'){
      lastTime = (__int64)((double)atoint64(buffer+1) * multiplier);      
    }
  }
  vcd.fclose();
  return lastTime;
}

void vcdtype::DumpAllSignals(){
  filetype vcdsigs;
  vcdsigs.fopen("vcd_signals.txt.gz", "wt");

  for (int i=0; i< aliases.size(); i++){
    aliastype al=aliases[i];
    int index;
    vcdsigs.fprintf("%s %s[%d:%d]\n", al.hierarchy, al.name, sigs[al.sigIndex].msb, sigs[al.sigIndex].lsb);
  }
  vcdsigs.fclose();
}

void simtype::PublishStats(wirelisttype* wl){
  bool statMergeParallel=false, statOnlySupplyDevices=false, debug=false;
  Statistics(wl, false, VT_NOMINAL,	statMergeParallel, statOnlySupplyDevices, debug);
  Statistics(wl, false, VT_LOW,	statMergeParallel, statOnlySupplyDevices, debug);
  Statistics(wl, false, VT_LOW_PLA2,	statMergeParallel, statOnlySupplyDevices, debug);
  Statistics(wl, false, VT_SUPERLOW,	statMergeParallel, statOnlySupplyDevices, debug);
  Statistics(wl, false, VT_SUPERLOW_PLA2,	statMergeParallel, statOnlySupplyDevices, debug);
}
