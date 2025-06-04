#ifndef __MINER_H_INCLUDED__
#define __MINER_H_INCLUDED__

#include "sha256.h"

class asictype;
class boardmodeltype;

inline uint32 EndianSwap(uint32 a) { return ((a >> 24) & 0xff) | ((a >> 8) & 0xff00) | ((a << 8) & 0xff0000) | ((a << 24) & 0xff000000); }

const bool PRINT_BYTE_STREAM = false;

const int NUM_ENGINES	   = 238;
const int CMD_UNIQUE	   = 0x12345678;
const int RSP_UNIQUE	   = 0xdac07654;
const int HIT_UNIQUE	   = 0xdac07654;

struct headerwordtype {
    uint32	version;
    uint32	prevblock[8];
    uint32	merkle[8];
    uint32	time;
    uint32	difficulty;	   // this field is used for the hash but not used for the difficulty screen
    uint32	nonce;
    };
union headertype {
   char			  raw[80];
   uint32		  words[20];
   headerwordtype x;
   headertype() { for (int i = 0; i < 20; i++) words[i] = 0; }
   void AsciiOut(vector<char>& out) const;// returns 160 characters + NULL
   void AsciiIn(const vector<char>& in);// expects 160 characters
   void AsciiIn(const char* in);// expects 160 characters
   int ReturnZeroes() const;
   bool RTL_Winner() const;
   int Intermediate(vector<unsigned>& internals) const;
   bool operator==(const headertype& right) const {
      for (int i = 0; i < 20; i++)
         if (words[i] != right.words[i]) return false;
      return true;
      }
   };



struct command_cfgtype {
   uint32	command_unique;
   uint32	id : 8;  // bits 0:7 of ID
   uint32	command : 8;
   uint32	id2 : 2;  // bits 8,9 of ID
   uint32	spare : 6;
   uint32	address : 8;
   // data unique to cfg
   uint32	data;
   uint32	crc;

   command_cfgtype() {
      if (sizeof(*this) != 16) FATAL_ERROR;
      memset(this, 0, sizeof(*this));
      command_unique = CMD_UNIQUE;
      }

   bool IsCrcCorrect() const {
      unsigned check = crc32(this, sizeof(*this) - 4);
      return check == crc;
      }
   void GenerateCrc() {
      crc = crc32(this, sizeof(*this) - 4);
      }
   void SerializeOut(vector<char>& out) const {
      bool print = PRINT_BYTE_STREAM;
      if (print) printf("CMD: ");
      for (int i = 0; i < sizeof(*this); i++) {
         unsigned char ch = ((char*)this)[i];
         if (print) printf("%2X ", ch);
         out.push_back(ch);
         }
      if (print) printf("\n");
      /*
                for (int i = 0; i < out.size(); i++) {
                       printf("\t@cb;    cb.tms <= 0;   cb.tdi <= %d; // byte[%d]\n", (out[i] >> 0) & 1, i);
                       printf("\t@cb;    cb.tms <= 0;   cb.tdi <= %d;\n", (out[i] >> 1) & 1);
                       printf("\t@cb;    cb.tms <= 0;   cb.tdi <= %d;\n", (out[i] >> 2) & 1);
                       printf("\t@cb;    cb.tms <= 0;   cb.tdi <= %d;\n", (out[i] >> 3) & 1);
                       printf("\t@cb;    cb.tms <= 0;   cb.tdi <= %d;\n", (out[i] >> 4) & 1);
                       printf("\t@cb;    cb.tms <= 0;   cb.tdi <= %d;\n", (out[i] >> 5) & 1);
                       printf("\t@cb;    cb.tms <= 0;   cb.tdi <= %d;\n", (out[i] >> 6) & 1);
                       printf("\t@cb;    cb.tms <= 0;   cb.tdi <= %d;\n", (out[i] >> 7) & 1);
                }*/
      }
   };

struct command_arptype {
   uint32	command_unique;
   uint32	id : 8;  // bits 0:7 of ID
   uint32	command : 8;
   uint32	id2 : 2;  // bits 8,9 of ID
   uint32	spare : 6;
   uint32	unused : 8;
   // data unique to arp
   uint32	addr0;        // low  32b of unique 64b identifier
   uint32	addr1;        // high 32b of unique 64b identifier
   uint32	mask0;        // low  32b of dont care mask(1 is dont care)
   uint32	mask1;        // high 32b of dont care mask(1 is dont care)
   uint32	crc;

   command_arptype() {
      if (sizeof(*this) != 28) FATAL_ERROR;
      memset(this, 0, sizeof(*this));
      command_unique = CMD_UNIQUE;
      }

   bool IsCrcCorrect() const {
      unsigned check = crc32(this, sizeof(*this) - 4);
      return check == crc;
      }
   void GenerateCrc() {
      crc = crc32(this, sizeof(*this) - 4);
      }
   void SerializeOut(vector<char>& out) const {
      bool print = PRINT_BYTE_STREAM;
      if (print) printf("CMD: ");
      for (int i = 0; i < sizeof(*this); i++) {
         unsigned char ch = ((char*)this)[i];
         if (print) printf("%2X ", ch);
         out.push_back(ch);
         }
      if (print) printf("\n");
      }
   };

struct command_loadtype {
   uint32	   command_unique;
   uint32	   id : 8;
   uint32	   command : 8;
   uint32	   nbits : 8;	// this will overwrite config.nbits for LOAD0
   uint32         id2 : 2;
   uint32	   sequence : 6;	// this is unused by asic but carried around to help software identify context of hit
   headertype     header;	        // 80B header, nonce field will be ignored for loads
   uint32	   crc;

   command_loadtype() {
      if (sizeof(*this) != 92) FATAL_ERROR;
      memset(this, 0, sizeof(*this));
      command_unique = CMD_UNIQUE;
      }

   bool IsCrcCorrect() const {
      unsigned check = crc32(this, sizeof(*this) - 4);
      return check == crc;
      }
   void GenerateCrc() {
      crc = crc32(this, sizeof(*this) - 4);
      }
   void SerializeOut(vector<char>& out) const {
      bool print = PRINT_BYTE_STREAM;
      if (print) printf("CMD: ");
      for (int i = 0; i < sizeof(*this); i++) {
         unsigned char ch = ((char*)this)[i];
         if (print) printf("%2X ", ch);
         out.push_back(ch);
         }
      if (print) printf("\n");
      /*      for (int i = 0; i < out.size(); i++) {
               printf("\t@cb;    cb.tms <= 0;   cb.tdi <= %d; // byte[%d]\n", (out[i] >> 0) & 1, i);
               printf("\t@cb;    cb.tms <= 0;   cb.tdi <= %d;\n", (out[i] >> 1) & 1);
               printf("\t@cb;    cb.tms <= 0;   cb.tdi <= %d;\n", (out[i] >> 2) & 1);
               printf("\t@cb;    cb.tms <= 0;   cb.tdi <= %d;\n", (out[i] >> 3) & 1);
               printf("\t@cb;    cb.tms <= 0;   cb.tdi <= %d;\n", (out[i] >> 4) & 1);
               printf("\t@cb;    cb.tms <= 0;   cb.tdi <= %d;\n", (out[i] >> 5) & 1);
               printf("\t@cb;    cb.tms <= 0;   cb.tdi <= %d;\n", (out[i] >> 6) & 1);
               printf("\t@cb;    cb.tms <= 0;   cb.tdi <= %d;\n", (out[i] >> 7) & 1);
               }*/
      }
   };

// note the response_cfgtype is the same size as command_cfgtype. 
struct response_cfgtype {
    uint32	   response_unique;
    uint32	   id         : 8;		// 7:0 id of the chip responding
    uint32	   command    : 8;		// copy of command field which caused the response(only [3:0] will be populated. [7:4] will be zero)
    uint32	   id2        : 2;              // bits 8,9 of ID
    uint32	   spare      : 6;		// copy of spare   field which caused the response
    uint32	   address    : 8;		// copy of address field which caused the response
    uint32	   data;
    uint32	   crc;

    response_cfgtype() {
        if (sizeof(*this) != 16) FATAL_ERROR;
        memset(this, 0, sizeof(*this));
        response_unique = RSP_UNIQUE;
        }

    bool IsCrcCorrect() const {
        unsigned check = crc32(this, sizeof(*this) - 4);
        return check == crc;
        }
    void GenerateCrc() {
        crc = crc32(this, sizeof(*this) - 4);
        }
    void SerializeOut(vector<char>& out) const {
        bool print = PRINT_BYTE_STREAM;
        if (print) printf("RSP: ");
        for (int i = 0; i < sizeof(*this); i++) {
            unsigned char ch = ((char*)this)[i];
            out.push_back(ch);
            if (print) printf("%2X ", ch);
            }
        if (print) printf("\n");
        }
    };

// note the response_arptype is the same size as command_arptype.
struct response_arptype {
   uint32	response_unique;
   uint32	id : 8;	// 7:0 id of the chip responding
   uint32	command : 8;	// copy of command field which caused the response(only [3:0] will be populated. [7:4] will be zero)
   uint32	id2 : 2;    // bits 8,9 of ID
   uint32	spare : 6;	// copy of spare   field which caused the response
   uint32	unused : 8;
   uint32	addr0;          // low 64b of unique address
   uint32	addr1;          // high 64b of unique address
   uint32	addr0_n;        // low 64b of unique address, 1's complement
   uint32	addr1_n;        // high 64b of unique address, 1's complement
   uint32	crc;

   response_arptype() {
      if (sizeof(*this) != 28) FATAL_ERROR;
      memset(this, 0, sizeof(*this));
      response_unique = RSP_UNIQUE;
      }

   bool IsCrcCorrect() const {
      unsigned check = crc32(this, sizeof(*this) - 4);
      return check == crc;
      }
   void GenerateCrc() {
      crc = crc32(this, sizeof(*this) - 4);
      }
   void SerializeOut(vector<char>& out) const {
      bool print = PRINT_BYTE_STREAM;
      if (print) printf("ARPRSP: ");
      for (int i = 0; i < sizeof(*this); i++) {
         unsigned char ch = ((char*)this)[i];
         out.push_back(ch);
         if (print) printf("%2X ", ch);
         }
      if (print) printf("\n");
      }
   };


// note the response_hittype is the same size as command_loadtype. 
struct response_hittype {
   uint32	   hit_unique;
   uint32	   id : 8;		  // 7:0 id of the chip responding
   uint32	   command : 8;		  // which of the 4 chunk1 values had the hit{CMD_LOAD0, CMD_LOAD1, CMD_LOAD2, CMD_LOAD3}, also CMD_RETURNHIT will be set as well
   uint32	   nbits : 8;		  // how many zeros were present, this field will be 0 if no hit
   uint32         id2 : 2;                // 9:8 id of the chip responding
   uint32	   sequence : 6;		  // sequence associated with context that caused the hit
   headertype     header;			  // 80B header, nonce field will be ignored for loads
   uint32	   crc;

   response_hittype() {
      if (sizeof(*this) != 23 * 4) FATAL_ERROR;
      memset(this, 0, sizeof(*this));
      hit_unique = HIT_UNIQUE;
      }

   bool IsCrcCorrect() const {
      unsigned check = crc32(this, sizeof(*this) - 4);
      return check == crc;
      }
   void GenerateCrc() {
      crc = crc32(this, sizeof(*this) - 4);
      }
   void SerializeOut(vector<char>& out) const {
      bool print = PRINT_BYTE_STREAM;
      if (print) printf("RSP: ");
      for (int i = 0; i < sizeof(*this); i++) {
         unsigned char ch = ((char*)this)[i];
         if (print) printf("%2X ", ch);
         out.push_back(ch);
         }
      if (print) printf("\n");
      }
   };


enum {
   E_UNIQUE=0,
   E_CHIPREVISION=1,
   E_ASICID=2,
   E_BAUD=3,
   E_CLKCOUNT=4,
   E_CLKCOUNT_64B=5,
   E_HASHCLK_COUNT=6,
   E_BYTES_RECEIVED=7,
   E_BUILD_DATE = 8,
   E_COMERROR=10,
   E_RSPERROR=11,
   E_DRIVESTRENGTH=9,
   E_PUD=10,
   E_MINER_CONFIG = 15,
   E_VERSION_BOUND=16,
   E_VERSION_SHIFT=17,
   E_SUMMARY = 18,
   E_HITCONFIG=19,
   E_HASHCONFIG=20,
   E_BIST_GOOD_SAMPLES = 21,
   E_BIST_THRESHOLD = 22,
   E_BIST=23,
   E_PLLCONFIG=24,
   E_PLLFREQ=25,
   E_HASHCLK2_COUNT=26,
   E_PLL2 = 27,
   E_IPCFG=28,
   E_TEMPCFG=29,
   E_TEMPERATURE=30,
   E_DVMCFG=31,
   E_VOLTAGE=32,
   E_DROCFG=33,
   E_DRO=34,
   E_THERMALTRIP=35,
   E_MAX_TEMP_SEEN = 36,
   E_SMALL_NONCE = 37,
   E_TESTCLKOUT=38,
   E_SPEED_DELAY=39,
   E_SPEED_UPPER_BOUND=40,
   E_SPEED_INCREMENT=41,
   E_ENGINEMASK=48,
   E_BIST_RESULTS=64,
   E_ENGINE_RESULTS=80,
   E_GENERAL_HIT_COUNT=96,
   E_GENERAL_TRUEHIT_COUNT = 97,
   E_SPECIFIC_HIT_COUNT = 98,
   E_SPECIFIC_TRUEHIT_COUNT = 99,
   E_DIFFICULT_HIT_COUNT = 100,
   E_DROPPED_HIT_COUNT = 101,
   E_DROPPED_DIFFICULT_COUNT = 102,
   E_DUTY_CYCLE = 104,
   E_CLOCK_RETARD = 106,
   E_CONTEXT0 = 128,
   E_SEQUENCE = 147,
   E_CONTEXT1 = 148,
   E_CONTEXT2 = 164,
   E_CONTEXT3 = 180,
   E_HIT0_HEADER = 196,
   E_HIT0 = 216,
   E_HIT1_HEADER = 218,
   E_HIT1 = 238,
   E_HIT2 = 239,
   };



   struct configtype {
      const uint32 chip_unique;
      const uint32 chip_revision;
      uint32	   asicid;
      uint32	   baud;
      uint32	   clk_count;
      uint32	   clk_count_64b;
      uint32	   hashclk_count;
      uint32	   bytes_received;
      uint16	   comerror_framing;
      uint16	   comerror_crc;
      uint16	   rsperror_overrun;
      uint16	   rsperror_collision;
      uint16	   versionbound_lower;
      uint16	   versionbound_upper;
      uint16	   versionbound_shift;
      uint8	   current_sequence;
      uint8	   current_difficulty;
      uint8	   hitconfig_autoreporting;
      uint32	   hashconfig;
      uint32	   engine_mask[16];
      uint32	   bist_results[16];
      uint32	   hit0;
      headertype  hit0_header;
      uint32	   hit1;
      headertype  hit1_header;
      uint32	   statconfig;
      uint32	   general_hit_count;
      uint32	   general_truehit_count;
      uint32	   specific_hit_count;
      uint32	   specific_truehit_count;
      uint32	   difficult_hit_count;
      headertype  context0;			// nonce will contain sequence not nonce
      headertype  context1;			// nonce, timestamp, difficulty, merkle[7] will be unused
      headertype  context2;			// nonce, timestamp, difficulty, merkle[7] will be unused
      headertype  context3;			// nonce, timestamp, difficulty, merkle[7] will be unused

      configtype(uint8 _id) : asicid(_id), chip_unique(((int)'A' << 0) + ((int)'u' << 8) + ((int)'r' << 16) + ((int)'a' << 24)), chip_revision(0)
         {
         baud = 0x36413641;
         clk_count = 0;
         clk_count_64b = 0;
         hashclk_count = 0;
         comerror_framing = 0;
         comerror_crc = 0;
         rsperror_overrun = 0;
         rsperror_collision = 0;
         versionbound_lower = 0;
         versionbound_upper = 0;
         versionbound_shift = 0;
         current_difficulty = 255;
         hitconfig_autoreporting = 0;
         hit0 = 0;
         hit1 = 0;
         statconfig = 0;
         hashconfig = 0;
         memset(engine_mask, 0, sizeof(engine_mask));
         general_hit_count = 0;
         general_truehit_count = 0;
         specific_hit_count = 0;
         specific_truehit_count = 0;
         difficult_hit_count = 0;
         }
      void Write(int address, uint32 data);
      void WriteContext(int num);
      uint32 Read(uint32 address) const;
      const char* Label(int address);
      };




class enginetype {
    int id;
    asictype& parent;
public:
    uint32 nonce;

    enginetype(int _id, asictype& _parent) : id(_id), parent(_parent) {
        nonce = 0;
        }
    bool ExecuteCycle(const uint32 nonce, int& ctxnum, uint32& raw_result);  // return true if possible hit
    uint32 Bist();
    };



class asictype {
   friend class enginetype;	  // let enginetype see our private variables
private:
   boardmodeltype& parent;
   configtype config;
   uint16 vercount, timecount;
   uint32 noncecount;
   shadigesttype h3i[4], midstate[4];
   uint32 ver[4], chunk2_static[3];
   bool sendcontext;
   vector<enginetype> engines;

   void ComputeContext();
   bool VerifyHit(uint32 hitnonce, int ctxnum);
public:

   asictype(uint8 _id, boardmodeltype& _parent) : config(_id), parent(_parent)
      {
      vercount = 0;
      timecount = 0;
      noncecount = 0;
      sendcontext = false;
      for (int i = 0; i < NUM_ENGINES; i++)
         engines.push_back(enginetype(i, *this));
      }
   void ChangeId(int _id) {
	  printf("Changing asic id from %x to %x\n", config.asicid,_id);
	  config.asicid = _id;
   }
   void ProcessSerial(const vector<char>& command, vector<char>& response);
   void ExecuteCycle();
   void RunBist();
};

class boardmodeltype {
   vector<asictype*> asics;
public:
   bool tinynonce;

   boardmodeltype()
      {
      int i;
      for (i = 0; i < 1; i++)
         asics.push_back(new asictype(i, *this));
      tinynonce = false;
      }
   void ProcessSerial(const vector<char>& command, vector<char>& response)
      {
      for (int i = 0; i < asics.size(); i++)
         asics[i]->ProcessSerial(command, response);
      }
   void ExecuteCycle() {
      for (int i = 0; i < asics.size(); i++)
         asics[i]->ExecuteCycle();
      }
   void ChangeAsicId(int _id) {
      asics[0]->ChangeId(_id);
      }
   void WriteConfig(int addr, unsigned int data, int id = 0);
   uint32 ReadConfig(int addr, int id = 0);
   void SendJob(const headertype& header, const int nbits, const int id = 0);	 // message will be 80B binary header, id=-1 for broadcast
   void SendAsciiHeader(const char* message, const int nbits, const int id = 0);	 // message will be 160B header in hex, id=-1 for broadcast
   bool CheckHit(int id = 0);  // return true if there's a hit
   bool ReturnHit(headertype& header, int& ctx, int& difficulty, int& sequence, int id = 0);  // return true if there's a hit
   void RunBist() {
      for (int i = 0; i < asics.size(); i++)
         asics[i]->RunBist();
      }

   };




#endif //__MINER_H_INCLUDED__


