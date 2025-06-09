#include "pch.h"
#include "template.h"
#include "multithread.h"
#include "circuit.h"      
#include "wirelist.h"

void ReadSubstitute(const char *filename, wildcardtype &ignoreStructures, wildcardtype &explodeStructures, wildcardtype &explodeUnderStructures, wildcardtype &deleteStructures, wildcardtype &noXStructures, wildcardtype &xMerge, wildcardtype &prependPortStructures, stringPooltype &stringPool, arraytype<const char *> &vddNames, arraytype<const char *> &vssNames);
void AppendFiles(filetype &fp, const char *subname);


   
int Work(int argc, char *argv[])
   {
   TAtype ta("Extract.exe main()");
   char filename[200]="";
   arraytype <const char *> subnames;
   const char *destname=NULL;
   int i;    
   wirelisttype *wl = NULL;
   wildcardtype explodeStructures;
   wildcardtype explodeUnderStructures;
   wildcardtype deleteStructures;
   wildcardtype noXStructures;
   wildcardtype xMergeStructures;
   wildcardtype prependPortStructures;
   arraytype<const char *> vddNames, vssNames;
   stringPooltype stringPool;
   const char *structure=NULL;
   bool atpg = false, assertionCheck = false, stat=false, statMergeParallel=false, statOnlySupplyDevices=false, statNoMem=false, debug=false, makeMap=false, noX=false, noPrefix=false;

   for (i=1; i<argc; i++)	   
      {
      if (NULL!=strstr(argv[i],"-atpg"))
         {
         atpg = true;
         argc--;
         }
      if (atpg)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1; i<argc; i++)	   
      {
      if (NULL!=strstr(argv[i],"-debug") || NULL!=strstr(argv[i],"-DEBUG"))
         {
         debug = true;
         argc--;
         }
      if (debug)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1; i<argc; i++)	   
      {
      if (NULL!=strstr(argv[i],"-assertion") || NULL!=strstr(argv[i],"-ASSERTION"))
         {
         assertionCheck = true;
         argc--;
         }
      if (assertionCheck)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1; i<argc; i++)	   
      {
      if (NULL!=strstr(argv[i],"-stat_merge_parallel") || NULL!=strstr(argv[i],"-statistics_merge_parallel"))
         {
         statMergeParallel = true;
         argc--;
         }
      if (statMergeParallel)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1; i<argc; i++)	   
      {
      if (NULL!=strstr(argv[i],"-stat_only_supply_devices") || NULL!=strstr(argv[i],"-statistics_only_supply_devices"))
         {
         statOnlySupplyDevices = true;
         argc--;
         }
      if (statOnlySupplyDevices)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1; i<argc; i++)	   
      {
      if (NULL!=strstr(argv[i],"-stat") || NULL!=strstr(argv[i],"-statistics"))
         {
         stat = true;
         argc--;
         }
      if (stat)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1; i<argc; i++)	   
      {
      if (NULL!=strstr(argv[i],"-noPrefix") || NULL!=strstr(argv[i],"-noprefix"))
         {
         noPrefix = true;
         argc--;
         }
      if (noPrefix)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1; i<argc; i++)	   
      {
      if (NULL!=strstr(argv[i],"-logicstat"))
         {
         statNoMem = true;
         argc--;
         }
      if (statNoMem)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1; i<argc; i++)	   
      {
      if (NULL!=strstr(argv[i],"-map"))
         {
         makeMap = true;
         argc--;
         }
      if (makeMap)
         argv[i] = argv[i+1]; // won't exceed array because argc-- above        
      }
   for (i=1; i<argc; i++)	   
      {
      if (NULL!=strstr(argv[i],"-structure=") || NULL!=strstr(argv[i],"-STRUCTURE="))
         {
         structure = argv[i]+11;
         argc--;
         }
      if (structure!=NULL)
         argv[i] = argv[i+1];
      }
   
   for (i=1; i<argc; i++)	   
      {
      if (argv[i][0]=='-')
         {
         printf("\nI didn't understand command line parameter '%s'\n",argv[i]);
         exit(-1);
         }
      }

   for (i=1; i<argc; i++)          
      {
      if (argv[i]!=NULL && i==1)
         strcpy(filename, argv[i]);
      if (argv[i]!=NULL && strstr(argv[i],".v")!=NULL)
         subnames.push(stringPool.NewString(argv[i]));
      }
#ifdef _MSC_VER
   debug = true;
   strcpy(filename,"z:\\david\\vlsi\\SF4V54ULT_LVLSAH2L_H2V0_4.cdl");
//   strcpy(filename, "z:\\david\\vlsi\\SF4V54ULT_LVLSAL2H_H2V0_4.cdl");
   //   strcpy(filename, "d:\\vlsi\\foo.cdl");
   //   strcpy(filename,"z:\\vlsi\\pp_mem_maf.nt");
//   strcpy(filename,"z:\\vlsi\\puf.nt");
//   noPrefix = true;
//   stat = true;
//   statNoMem = true;
//   debug = true;
//   atpg = true;
//   subnames.push("z:\\vlsi\\atpg.v");
//   subnames.push("z:\\vlsi\\sub65nm.v");
   subnames.push("d:\\vlsi\\substitute.v");
//   assertionCheck = true;
#endif

   if (filename[0]==0)                                                         
      {
      printf("\nNothing to do\n");
      }
   
   substitutetype ignoreStructures(subnames.size());
   
   for (i=0; i<subnames.size(); i++)
      {
      ignoreStructures.filenames[i] = subnames[i];
      ReadSubstitute(subnames[i], ignoreStructures.wcs[i], explodeStructures, explodeUnderStructures, deleteStructures, noXStructures, xMergeStructures, prependPortStructures, stringPool, vddNames, vssNames);
      }
   if (strstr(filename,".cdl")!=NULL)
      wl = ReadCDLWirelist(filename, vddNames, vssNames);
   else
      wl = ReadWirelist(filename, vddNames, vssNames);
   if (structure!=NULL) {
     wl = FindWirelistStructure(structure);
   }
   if (wl==NULL) FATAL_ERROR;
   if (stat)
      {
	//printf("Publishing statts for SRAMNFETWL\n");
	//Statistics(wl, false, VT_SRAMNFETWL,	statMergeParallel, statOnlySupplyDevices, debug);
	//Statistics(wl, false, VT_SRAMNFETPD,	statMergeParallel, statOnlySupplyDevices, debug);
	//Statistics(wl, false, VT_SRAMPFETPU, 	statMergeParallel, statOnlySupplyDevices, debug);
	//printf("Publishing statts for SRAMLSRNFETPDB\n");
	//Statistics(wl, false, VT_SRAMLSRNFETPDB, 	statMergeParallel, statOnlySupplyDevices, debug);
	//printf("Publishing statts for SRAMLSRNFETWLB\n");
	//Statistics(wl, false, VT_SRAMLSRNFETWLB, 	statMergeParallel, statOnlySupplyDevices, debug);
	//printf("Publishing statts for SRAMLSRNFETRBB\n");
	//Statistics(wl, false, VT_SRAMLSRNFETRBB, 	statMergeParallel, statOnlySupplyDevices, debug);
	//printf("Publishing statts for VT_SRAMLSRPFETPUB\n");
	//Statistics(wl, false, VT_SRAMLSRPFETPUB, 	statMergeParallel, statOnlySupplyDevices, debug);
	
	//printf("Publishing statts for MEDIUM THICK OXIDE\n");
	//Statistics(wl, false, VT_MEDIUMTHICKOXIDE, statMergeParallel, statOnlySupplyDevices, debug);
	printf("Publishing statts for VT ALL\n");
	Statistics(wl, false, VT_ALL,	statMergeParallel, statOnlySupplyDevices, debug);		
	//Statistics(wl, false, VT_NOMINAL, .030,	statMergeParallel, statOnlySupplyDevices, debug);
	//Statistics(wl, false, VT_NOMINAL, .034,	statMergeParallel, statOnlySupplyDevices, debug);
	//Statistics(wl, false, VT_NOMINAL, .038,	statMergeParallel, statOnlySupplyDevices, debug);

	//Statistics(wl, false, VT_HIGH, .030,	statMergeParallel, statOnlySupplyDevices, debug);
	//Statistics(wl, false, VT_HIGH, .034,	statMergeParallel, statOnlySupplyDevices, debug);
	//Statistics(wl, false, VT_HIGH, .038,	statMergeParallel, statOnlySupplyDevices, debug);

	//Statistics(wl, false, VT_LOW_PLA, statMergeParallel, statOnlySupplyDevices, debug);
//      Statistics(wl, false, VT_NOMINAL, statMergeParallel, statOnlySupplyDevices, debug);
	//Statistics(wl, false, VT_NOMINAL_PLA, statMergeParallel, statOnlySupplyDevices, debug);
	//    Statistics(wl, false, VT_HIGH, 	statMergeParallel, statOnlySupplyDevices, debug);
	//Statistics(wl, false, VT_HIGH_PLA, 	statMergeParallel, statOnlySupplyDevices, debug);
//      Statistics(wl, false, VT_SRAM, 	statMergeParallel, statOnlySupplyDevices, debug);
	//    Statistics(wl, false, VT_ALL, 	statMergeParallel, statOnlySupplyDevices, debug);
      return 0;
      }
   if (statNoMem)
      {
	printf("Generating statistics for structure %s\n", wl->name);
	Statistics(wl,  true, VT_SUPERLOW,     statMergeParallel, statOnlySupplyDevices, debug);
	Statistics(wl,  true, VT_SUPERLOW_PLA2,statMergeParallel, statOnlySupplyDevices, debug);
	Statistics(wl,  true, VT_SUPERLOW_PLA4,statMergeParallel, statOnlySupplyDevices, debug);
	Statistics(wl,  true, VT_LOW,  	     statMergeParallel, statOnlySupplyDevices, debug);
	Statistics(wl,  true, VT_LOW_PLA2,     statMergeParallel, statOnlySupplyDevices, debug);
	Statistics(wl,  true, VT_LOW_PLA4,     statMergeParallel, statOnlySupplyDevices, debug);
	Statistics(wl,  true, VT_LOW_PLA8,     statMergeParallel, statOnlySupplyDevices, debug);
	Statistics(wl,  true, VT_NOMINAL,      statMergeParallel, statOnlySupplyDevices, debug);
	Statistics(wl,  true, VT_NOMINAL_PLA2, statMergeParallel, statOnlySupplyDevices, debug);
	Statistics(wl,  true, VT_NOMINAL_PLA4, statMergeParallel, statOnlySupplyDevices, debug);
	Statistics(wl,  true, VT_NOMINAL_PLA8, statMergeParallel, statOnlySupplyDevices, debug);
	Statistics(wl,  true, VT_HIGH, 	     statMergeParallel, statOnlySupplyDevices, debug);
	Statistics(wl,  true, VT_HIGH_PLA2,    statMergeParallel, statOnlySupplyDevices, debug);
	Statistics(wl,  true, VT_HIGH_PLA4,    statMergeParallel, statOnlySupplyDevices, debug);
	Statistics(wl,  true, VT_HIGH_PLA8,    statMergeParallel, statOnlySupplyDevices, debug);
	Statistics(wl,  true, VT_ALL, 	statMergeParallel, statOnlySupplyDevices, debug);
      return 0;
      }

   filetype fp;
   wl->WriteVerilog(ignoreStructures, explodeStructures, explodeUnderStructures, deleteStructures, noXStructures, xMergeStructures, prependPortStructures, destname, atpg, assertionCheck, debug, noPrefix, fp, makeMap);

   fp.fclose();
   return 0;
   }    
                             


void ReadSubstitute(const char *filename, wildcardtype &ignoreStructures, wildcardtype &explodeStructures, wildcardtype &explodeUnderStructures, wildcardtype &deleteStructures, wildcardtype &noXStructures, wildcardtype &xMerge, wildcardtype &prependPortStructures, stringPooltype &stringPool, arraytype<const char *> &vddNames, arraytype<const char *> &vssNames)
   {
   TAtype ta("ReadSubstitute()");
   int line=0;
   char buffer[2048];
   FILE *fptr = fopen(filename,"rt");
   bool prepend=false;

   if (fptr==NULL)
      {
      printf("\nI couldn't open %s for reading.",filename);
      return;
      }

   while (fgets(buffer, sizeof(buffer)-1, fptr) !=NULL)
      {
      line++;

//      if (strstr(buffer, "A2SDFFQX2A9TR")!=NULL) // erase me
//         printf("");
      if (strnicmp(buffer,"//prepend_IO_to_ports", 21)==0 || strnicmp(buffer,"// prepend_IO_to_ports", 22)==0)
         prepend = true;
      if (strnicmp(buffer,"//Dont_prepend_IO_to_ports", 26)==0 || strnicmp(buffer,"// Dont_prependIO_to_ports", 27)==0)
         prepend = false;

      if (strncmp(buffer,"module", 6)==0 || strncmp(buffer,"//module", 8)==0  || strncmp(buffer,"// module", 9)==0)
         {
         char *start = strstr(buffer,"module")+ 6, *end;
         while (*start == ' ' || *start=='\t')
            start++;
         end = start;
         while (*end !=' ' && *end!='\t' && *end!='\r' && *end!='\n' && *end!=0 && *end!='(')
            end++; 
         *end=0;
         ignoreStructures.Add(start);
         if (prepend)
            prependPortStructures.Add(start);

         printf("\nI will ignore structure -> %s",start);
         }
      if (strncmp(buffer,"unmodule", 8)==0 || strncmp(buffer,"//unmodule", 10)==0  || strncmp(buffer,"// unmodule", 11)==0)
         {
         char *start = strstr(buffer,"unmodule")+ 8, *end;
         while (*start == ' ' || *start=='\t')
            start++;
         end = start;
         while (*end !=' ' && *end!='\t' && *end!='\r' && *end!='\n' && *end!=0 && *end!='(')
            end++; 
         *end=0;
         
         ignoreStructures.Remove(start);
         printf("\nI will now not ignore structure -> %s",start);
         }
      if (strncmp(buffer,"//nox_cell", 10)==0  || strncmp(buffer,"// nox_cell", 11)==0)
         {
         char *start = strstr(buffer,"nox_cell")+ 8, *end;
         while (*start == ' ' || *start=='\t')
            start++;
         end = start;
         while (*end !=' ' && *end!='\t' && *end!='\r' && *end!='\n' && *end!=0 && *end!='(')
            end++; 
         *end=0;
         noXStructures.Add(start);
         printf("\nI will X analyze structure -> %s",start);
         }
      if (strncmp(buffer,"//explode_cell", 14)==0  || strncmp(buffer,"// explode_cell", 15)==0)
         {
         char *start = strstr(buffer,"explode_cell")+ 12, *end;
         while (*start == ' ' || *start=='\t')
            start++;
         end = start;
         while (*end !=' ' && *end!='\t' && *end!='\r' && *end!='\n' && *end!=0 && *end!='(')
            end++; 
         *end=0;
         explodeStructures.Add(start);
         printf("\nI will explode structure -> %s",start);
         }
      if (strncmp(buffer,"//explode_under", 14)==0  || strncmp(buffer,"// explode_under", 15)==0)
         {
         char *start = strstr(buffer,"explode_under")+ 13, *end;
         while (*start == ' ' || *start=='\t')
            start++;
         end = start;
         while (*end !=' ' && *end!='\t' && *end!='\r' && *end!='\n' && *end!=0 && *end!='(')
            end++; 
         *end=0;
         explodeUnderStructures.Add(start);
         printf("\nI will explode underneath structure -> %s",start);
         }
      if (strncmp(buffer,"//delete_cell", 13)==0  || strncmp(buffer,"// delete_cell", 14)==0)
         {
         char *start = strstr(buffer,"delete_cell")+ 11, *end;
         while (*start == ' ' || *start=='\t')
            start++;
         end = start;
         while (*end !=' ' && *end!='\t' && *end!='\r' && *end!='\n' && *end!=0 && *end!='(')
            end++; 
         *end=0;
         deleteStructures.Add(start);
         printf("\nI will delete structure -> %s",start);
         }
      if (strncmp(buffer,"//xmerge_cell", 13)==0  || strncmp(buffer,"// xmerge_cell", 14)==0)
         {
         char *start = strstr(buffer,"xmerge_cell")+ 11, *end;
         while (*start == ' ' || *start=='\t')
            start++;
         end = start;
         while (*end !=' ' && *end!='\t' && *end!='\r' && *end!='\n' && *end!=0 && *end!='(')
            end++; 
         *end=0;
         xMerge.Add(start);
         printf("\nI will aggressively merge the guts of structure -> %s",start);
         }

      if (strncmp(buffer,"//vdd_signal_name", 17)==0  || strncmp(buffer,"// vdd_signal_name", 18)==0)
         {
         char *start = strstr(buffer,"vdd_signal_name")+ 15, *end;
         while (*start == ' ' || *start=='\t')
            start++;
         end = start;
         while (*end !=' ' && *end!='\t' && *end!='\r' && *end!='\n' && *end!=0 && *end!='(')
            end++; 
         *end=0;
         vddNames.push(stringPool.NewString(start));
         printf("\nI treat all signals names that match '%s' as VDD",start);
         }
      if (strncmp(buffer,"//vss_signal_name", 17)==0  || strncmp(buffer,"// vss_signal_name", 18)==0)
         {
         char *start = strstr(buffer,"vss_signal_name")+ 15, *end;
         while (*start == ' ' || *start=='\t')
            start++;
         end = start;
         while (*end !=' ' && *end!='\t' && *end!='\r' && *end!='\n' && *end!=0 && *end!='(')
            end++; 
         *end=0;
         vssNames.push(stringPool.NewString(start));
         printf("\nI treat all signals names that match '%s' as VSS",start);
         }
      }
   fclose(fptr);
   }

void AppendFiles(filetype &fp, const char *subname)
   {
   TAtype ta("AppendFiles()");
   int line=0;
   char buffer[2048];
   FILE *source = fopen(subname,"rt");

   if (source == NULL)
      {
      printf("\nI couldn't open %s for reading.",subname);
      return;
      }
   while (fgets(buffer, sizeof(buffer)-1, source) !=NULL)
      {
      line++;
      fp.fputs(buffer);
      }
   fclose(source);
   }

