#include "pch.h"
#include "template.h"
#include "multithread.h"
#include "circuit.h"      
#include "wirelist.h"

void ReadSubstitute(const char *filename, wildcardtype &ignoreStructures, wildcardtype &explodeStructures, wildcardtype &explodeUnderStructures, wildcardtype &deleteStructures, stringPooltype &stringPool, array<const char *> &vddNames, array<const char *> &vssNames);
void AppendFiles(filetype &fp, const char *subname);

double NGATE_CAPACITANCE    = 0.0;
double PGATE_CAPACITANCE    = 0.0;
double NDIFF_CAPACITANCE    = 0.0;
double PDIFF_CAPACITANCE    = 0.0;
   
int Work(int argc, char *argv[])
   {
   TAtype ta("Extract.exe main()");
   char filename[200]="";
   char subname[200]="";            
   const char *destname=NULL;
   int i;    
   wirelisttype *wl = NULL;
   wildcardtype ignoreStructures;
   wildcardtype explodeStructures;
   wildcardtype explodeUnderStructures;
   wildcardtype deleteStructures;
   array<const char *> vddNames, vssNames;
   stringPooltype stringPool;
   bool atpg = false, assertionCheck = false, stat=false, statNoMem=false, debug=false, makeMap=false;

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
      if (argv[i]!=NULL && i==2)
         strcpy(subname, argv[i]);
      }
#ifdef _MSC_VER
   strcpy(filename,"z:\\vlsi\\pp_l1i_mem.nt");
//   atpg = true;
//   stat = true;
//   statNoMem = true;
   debug = true;
   strcpy(subname,"z:\\vlsi\\substitute.v");
//   assertionCheck = true;
#endif

   if (filename[0]==0)                                                         
      {
      printf("\nEnter a .nt filename->");
      gets(filename);                                                                            
      }
   
   if (subname[0])
      ReadSubstitute(subname, ignoreStructures, explodeStructures, explodeUnderStructures, deleteStructures, stringPool, vddNames, vssNames);
   
   wl = ReadWirelist(filename, vddNames, vssNames);
   if (wl==NULL) FATAL_ERROR;
   if (stat)
      {
      Statistics(wl, false);
      return 0;
      }
   if (statNoMem)
      {
      Statistics(wl, true);
      return 0;
      }
/*
FILE *fptr=fopen("z:\\foo.txt.gz","wb");
unsigned char buffer[256];
memset(buffer, 0, sizeof(buffer));
fwrite(buffer, 10, 1, fptr);
memset(buffer, 0xff, sizeof(buffer));
buffer[0] = 1;
buffer[1] = 0x10;
buffer[2] = 0;
buffer[3] = 0xef;
buffer[4] = 0xff;
fwrite(buffer,20,1,fptr);
fclose(fptr);
*/

   filetype fp;
   wl->WriteVerilog(ignoreStructures, explodeStructures, explodeUnderStructures, deleteStructures, destname, atpg, assertionCheck, debug, fp, makeMap);

   fp.fclose();
   return 0;
   }    
                             


void ReadSubstitute(const char *filename, wildcardtype &ignoreStructures, wildcardtype &explodeStructures, wildcardtype &explodeUnderStructures, wildcardtype &deleteStructures, stringPooltype &stringPool, array<const char *> &vddNames, array<const char *> &vssNames)
   {
   TAtype ta("ReadSubstitute()");
   int line=0;
   char buffer[2048];
   FILE *fptr = fopen(filename,"rt");

   if (fptr==NULL)
      {
      printf("\nI couldn't open %s for reading.",filename);
      return;
      }

   while (fgets(buffer, sizeof(buffer)-1, fptr) !=NULL)
      {
      line++;

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
         printf("\nI will ignore structure -> %s",start);
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



