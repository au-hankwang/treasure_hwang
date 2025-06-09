#include "pch.h"
#include "sha256.h"
#include "helper.h"
#include "dvfs.h"
#include "miner.h"



void headertype::AsciiIn(const char* in) { // expects 160 characters
    vector<char> v;
    for (int i = 0; i < strlen(in); i++) {
        if (in[i] >= '0' && in[i] <= '9')
            v.push_back(in[i]);
        else if (in[i] >= 'a' && in[i] <= 'f')
            v.push_back(in[i]);
        else if (in[i] >= 'A' && in[i] <= 'F')
            v.push_back(in[i]);
        }

    AsciiIn(v);
    }

void headertype::AsciiIn(const vector<char>& in) { // expects 160 characters
    int i;
    if (in.size() < 160) FATAL_ERROR;
    for (i = 0; i < 160; i++) {
        const char& ch = in[i];
        int digit;
        if (ch >= '0' && ch <= '9')
            digit = ch - '0';
        else if (ch >= 'a' && ch <= 'f')
            digit = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F')
            digit = ch - 'A' + 10;
        else break;
        if (i % 2 == 0)
            raw[i / 2] = (digit << 4);
        else
            raw[i / 2] += digit;
        }
    }

void headertype::AsciiOut(vector<char>& out) const { // returns 160 characters + NULL
    int i;
    out.clear();
    for (i = 0; i < 80; i++) {
        unsigned char ch;

        ch = (raw[i] >> 4) & 0xf;
        if (ch <= 9)
            out.push_back(ch + '0');
        else
            out.push_back(ch - 10 + 'A');
        ch = raw[i] & 0xf;
        if (ch <= 9)
            out.push_back(ch + '0');
        else
            out.push_back(ch - 10 + 'A');
        }
    out.push_back(0);
    if (out.size() != 161) FATAL_ERROR;
    }

int headertype::ReturnZeroes() const
   {
   uint32 message[16], second_message[16];
   shadigesttype digest, second;
   int i;

   for (i = 0; i < 16; i++) message[i] = EndianSwap(words[i]);

   sha256ProcessBlock(digest, message);
   message[0] = EndianSwap(words[16]);
   message[1] = EndianSwap(words[17]);
   message[2] = EndianSwap(words[18]);
   message[3] = EndianSwap(words[19]);
   message[4] = 0x80000000;
   message[5] = 0;
   message[6] = 0;
   message[7] = 0;
   message[8] = 0;
   message[9] = 0;
   message[10] = 0;
   message[11] = 0;
   message[12] = 0;
   message[13] = 0;
   message[14] = 0;
   message[15] = 640;
   sha256ProcessBlock(digest, message);

   shadigesttype d2;
   second_message[0] = digest.a;
   second_message[1] = digest.b;
   second_message[2] = digest.c;
   second_message[3] = digest.d;
   second_message[4] = digest.e;
   second_message[5] = digest.f;
   second_message[6] = digest.g;
   second_message[7] = digest.h;
   second_message[8] = 0x80000000;
   second_message[9] = 0;
   second_message[10] = 0;
   second_message[11] = 0;
   second_message[12] = 0;
   second_message[13] = 0;
   second_message[14] = 0;
   second_message[15] = 256;
   sha256ProcessBlock(second, second_message);
   // now count the zeroes
   int count = 0;
   uint32 z;
   while (true) {
      if (count == 0) z = EndianSwap(second.h);
      else if (count == 32) z = EndianSwap(second.g);
      else if (count == 64) z = EndianSwap(second.f);
      else if (count == 96) break;
      if (z >> 31) break;
      z = z << 1;
      count++;
      }
   return count;
   }

bool headertype::RTL_Winner() const
   {
   uint32 message[20];
   shadigesttype digest, second;
   int i;

   for (i = 0; i < 20; i++) message[i] = EndianSwap(words[i]);

   sha256ProcessBlock(digest, message);

   return 0 == RTL2Hash(digest, message + 16);
   }



const int enginewords = (31 + 202) / 32;
uint32 configtype::Read(uint32 address) const {
   switch (address) {
      case E_UNIQUE: return chip_unique;
      case E_CHIPREVISION: return chip_revision;
      case E_ASICID: return asicid;
      case E_BAUD: return baud;
      case E_CLKCOUNT: return clk_count;
      case E_CLKCOUNT_64B: return clk_count_64b;
      case E_HASHCLK_COUNT: return hashclk_count;
      case E_BYTES_RECEIVED: return bytes_received;
      case E_COMERROR: return (comerror_crc << 16) | comerror_framing;
      case E_RSPERROR: return (rsperror_collision << 16) | rsperror_overrun;
      case E_VERSION_BOUND: return (versionbound_upper << 16) | versionbound_lower;
      case E_VERSION_SHIFT: return versionbound_shift & 0x1f;
      case E_HITCONFIG: return hitconfig_autoreporting & 1;
      case E_HASHCONFIG: return hashconfig;
      case E_STATCONFIG: return statconfig;
      case E_SEQUENCE: return (current_difficulty << 8) | (current_sequence);
      case E_HIT0: return hit0;
      case E_HIT1: return hit1;

      default:
         if (address >= E_ENGINEMASK && address < E_ENGINEMASK + enginewords)	 return engine_mask[address - E_ENGINEMASK];
         if (address >= E_BIST_RESULTS && address < E_BIST_RESULTS + enginewords)	 return bist_results[address - E_BIST_RESULTS];
         if (address >= E_CONTEXT0 && address < E_CONTEXT0 + 20)					 return context0.words[address - E_CONTEXT0];
         if (address >= E_CONTEXT1 && address < E_CONTEXT1 + 16)				 return context1.words[address - E_CONTEXT1];
         if (address >= E_CONTEXT2 && address < E_CONTEXT2 + 16)				 return context2.words[address - E_CONTEXT2];
         if (address >= E_CONTEXT3 && address < E_CONTEXT3 + 16)				 return context3.words[address - E_CONTEXT3];
         if (address >= E_HIT0_HEADER && address < E_HIT0_HEADER + 20)			 return hit0_header.words[address - E_HIT0_HEADER];
         if (address >= E_HIT1_HEADER && address < E_HIT1_HEADER + 20)			 return hit1_header.words[address - E_HIT1_HEADER];
         return 0xBAD0DEED;
      }
   }

void configtype::Write(int address, uint32 data) {
   switch (address) {
      case E_BAUD:			baud = data; break;
      case E_CLKCOUNT:		clk_count = data; break;
      case E_CLKCOUNT_64B:	clk_count_64b = data; break;
      case E_HASHCLK_COUNT:hashclk_count = data; break;
      case E_BYTES_RECEIVED: bytes_received = data; break;
      case E_COMERROR:		comerror_crc = data >> 16; comerror_framing = data; break;
      case E_RSPERROR:		rsperror_collision = data >> 16; rsperror_overrun = data; break;
      case E_VERSION_BOUND:versionbound_upper = data >> 16; versionbound_lower = data; break;
      case E_VERSION_SHIFT:versionbound_shift = data; break;
      case E_HITCONFIG:	hitconfig_autoreporting = data; break;
      case E_HASHCONFIG:	hashconfig = data; break;
      case E_STATCONFIG:	statconfig = data; break;
      case E_HIT0:			hit0 = hit0 & 0xffffff00; break;	   // nbits field is clear only
      case E_HIT1:			hit1 = hit1 & 0xffffff00; break;   // nbits field is clear only

      default:
         if (address >= E_ENGINEMASK && address < E_ENGINEMASK + enginewords)	engine_mask[address - E_ENGINEMASK] = data;
         if (address >= E_CONTEXT0 && address < E_CONTEXT0 + 20)				context0.words[address - E_CONTEXT0] = data;
         if (address >= E_CONTEXT1 && address < E_CONTEXT1 + 16)				context1.words[address - E_CONTEXT1] = data;
         if (address >= E_CONTEXT2 && address < E_CONTEXT2 + 16)				context2.words[address - E_CONTEXT2] = data;
         if (address >= E_CONTEXT3 && address < E_CONTEXT3 + 16)				context3.words[address - E_CONTEXT3] = data;
      }
   }

const char* configtype::Label(int addr) {
   static char buffer[64];
   const char* fields[] = { "Ver","Prev0","Prev1","Prev2","Prev3","Prev4","Prev5","Prev6","Prev7","Merkle0","Merkle1","Merkle2","Merkle3","Merkle4","Merkle5","Merkle6","Merkle7","Time","Diff","Nonce" };

   switch (addr) {
      case E_UNIQUE: return "Unique";
      case E_CHIPREVISION: return "ChipRevision";
      case E_ASICID: return "AsicID";
      case E_BAUD: return "Baud";
      case E_CLKCOUNT: return "ClkCount";
      case E_CLKCOUNT_64B: return "ClkCount64";
      case E_HASHCLK_COUNT: return "HashClkCount";
      case E_COMERROR: return "ComError";
      case E_RSPERROR: return "RspError";
      case E_VERSION_BOUND: return "VersionBound";
      case E_VERSION_SHIFT: return "VersionShift";
      case E_HITCONFIG: return "HitConfig";
      case E_HASHCONFIG: return "HashConfig";
      case E_STATCONFIG: return "StatConfig";
      case E_SEQUENCE: return "Sequence";
      case E_HIT0: return "Hit0";
      case E_HIT1: return "Hit0";
      default:
         strcpy(buffer, "Unknown");
         if (addr >= E_ENGINEMASK && addr < E_ENGINEMASK + enginewords)     sprintf(buffer, "EngineMask+%d[%d-%d]", addr - E_ENGINEMASK, 31 + 32 * (addr - E_ENGINEMASK), 0 + 32 * (addr - E_ENGINEMASK));
         if (addr >= E_BIST_RESULTS && addr < E_BIST_RESULTS + enginewords) sprintf(buffer, "BistResults+%d[%d-%d]", addr - E_BIST_RESULTS, 31 + 32 * (addr - E_BIST_RESULTS), 0 + 32 * (addr - E_BIST_RESULTS));
         if (addr >= E_CONTEXT0 && addr < E_CONTEXT0 + 20) {
            int i = addr - E_CONTEXT0;
            sprintf(buffer, "Context0+%d[%s]", i, fields[i]);
            }
         if (addr >= E_CONTEXT1 && addr < E_CONTEXT1 + 16) {
            int i = addr - E_CONTEXT1;
            sprintf(buffer, "Context1+%d[%s]", i, fields[i]);
            }
         if (addr >= E_CONTEXT2 && addr < E_CONTEXT2 + 16) {
            int i = addr - E_CONTEXT2;
            sprintf(buffer, "Context2+%d[%s]", i, fields[i]);
            }
         if (addr >= E_CONTEXT3 && addr < E_CONTEXT3 + 16) {
            int i = addr - E_CONTEXT3;
            sprintf(buffer, "Context3+%d[%s]", i, fields[i]);
            }
         if (addr >= E_HIT0_HEADER && addr < E_HIT0_HEADER + 20) {
            int i = addr - E_HIT0_HEADER;
            sprintf(buffer, "Hit0_Header+%d[%s]", i, fields[i]);
            }
         if (addr >= E_HIT1_HEADER && addr < E_HIT1_HEADER + 20) {
            int i = addr - E_HIT1_HEADER;
            sprintf(buffer, "Hit0_Header+%d[%s]", i, fields[i]);
            }
         return buffer;
      }
   }



void asictype::ProcessSerial(const vector<char>& command, vector<char>& output)
   {
   for (int ptr = 0; ptr < command.size(); ptr++) {
      bool hitreport = false;

      if (command.size() >= sizeof(command_cfgtype)) {
         command_cfgtype packet;
         memcpy(&packet, &command[ptr], sizeof(packet));
         bool meantforme = packet.command_unique == CMD_UNIQUE && (packet.id == config.asicid || (packet.command & CMD_BROADCAST));
         hitreport = (packet.command & CMD_RETURNHIT) && packet.IsCrcCorrect() && meantforme;
         if (packet.command_unique == CMD_UNIQUE && !packet.IsCrcCorrect())
            config.comerror_crc++;
         else {
            if (meantforme && (packet.command == CMD_READ || packet.command == CMD_READWRITE)) {
               response_cfgtype outpacket;
               outpacket.address = packet.address;
               outpacket.command = packet.command;
               outpacket.id = config.asicid;
               outpacket.data = config.Read(packet.address);
               outpacket.GenerateCrc();
               outpacket.SerializeOut(output);
               printf("Reading %s data=%x\n", configtype(0).Label(packet.address), outpacket.data);
               if (packet.command == CMD_READWRITE) {
                  config.Write(packet.address, packet.data);
                  printf("Writing %s data=%x\n", configtype(0).Label(packet.address), packet.data);
                  if (packet.address == 23) RunBist();
                  }
               ptr += sizeof(packet) - 1;
               config.bytes_received += sizeof(packet) - 1;
               }
            else if (meantforme && (packet.command & 0x3f) == CMD_WRITE) {
               config.Write(packet.address, packet.data);
               printf("Writing %s data=%x\n", configtype(0).Label(packet.address), packet.data);
               ptr += sizeof(packet) - 1;
               config.bytes_received += sizeof(packet) - 1;
               if (packet.address == 23) RunBist();
               }
            }
         }
      if (command.size() >= sizeof(command_loadtype)) {
         command_loadtype packet;
         memcpy(&packet, &command[ptr], sizeof(packet));
         bool meantforme = packet.command_unique == CMD_UNIQUE && (packet.id == config.asicid || (packet.command & CMD_BROADCAST));
         hitreport = (packet.command & CMD_RETURNHIT) && packet.IsCrcCorrect() && meantforme;
         if (packet.command_unique == CMD_UNIQUE && !packet.IsCrcCorrect())
            config.comerror_crc++;
         else {
            if (meantforme && (packet.command & 0x3f) == CMD_LOAD0) {
               for (int i = 0; i < 20; i++)
                  config.context0.words[i] = packet.header.words[i];
               vercount = config.versionbound_lower;
               timecount = 0;
               noncecount = 0;
               config.current_difficulty = packet.nbits;
               config.current_sequence = packet.sequence;
               sendcontext = true;
               }
            if (meantforme && ((packet.command & 0x3f) == CMD_LOAD0 || (packet.command & 0x3f) == CMD_LOAD1)) {
               for (int i = 0; i < 20; i++)
                  config.context1.words[i] = packet.header.words[i];
               sendcontext = true;
               }
            if (meantforme && ((packet.command & 0x3f) == CMD_LOAD0 || (packet.command & 0x3f) == CMD_LOAD2)) {
               for (int i = 0; i < 20; i++)
                  config.context2.words[i] = packet.header.words[i];
               sendcontext = true;
               }
            if (meantforme && ((packet.command & 0x3f) == CMD_LOAD0 || (packet.command & 0x3f) == CMD_LOAD3)) {
               for (int i = 0; i < 20; i++)
                  config.context3.words[i] = packet.header.words[i];
               sendcontext = true;
               }
            if ((packet.command & 0x3f) == CMD_LOAD0 || (packet.command & 0x3f) == CMD_LOAD1 || (packet.command & 0x3f) == CMD_LOAD2 || (packet.command & 0x3f) == CMD_LOAD3) {
               ptr += sizeof(packet) - 1;
               config.bytes_received += sizeof(packet) - 1;
               }
            }
         }
      if (hitreport && (config.hit0 & 0xff)) {
         response_hittype outpacket;
         outpacket.id = config.asicid;
         outpacket.nbits = config.hit0 & 0xff;
         outpacket.command = (((config.hit0 >> 8) & 0x3) + CMD_LOAD0) | CMD_RETURNHIT;
         outpacket.sequence = (config.hit0 >> 16) & 0xff;
         outpacket.header = config.hit0_header;
         outpacket.GenerateCrc();
         outpacket.SerializeOut(output);
         // pop the hits 1 entry
         config.hit0_header = config.hit1_header;
         config.hit0 = config.hit1;
         config.hit1 = config.hit1 & 0xffffff00;
         }
      config.bytes_received++;
      }
   }



void asictype::RunBist()
   {
   int k, sequence = 0;
   for (k = 0; k < NUM_ENGINES; k++)
      config.bist_results[k / 32] = 0;

   for (sequence = 0; sequence < 4; sequence++) {
      uint32 x[2];
      for (k = 0; k < 4; k++) {
         if ((sequence ^ k) == 0) x[0] = 0xffff0000, x[1] = 0x5555aaaa;
         if ((sequence ^ k) == 1) x[0] = 0x0000ffff, x[1] = 0xaaaa5555;
         if ((sequence ^ k) == 2) x[0] = 0x5555aaaa, x[1] = 0xffff0000;
         if ((sequence ^ k) == 3) x[0] = 0xaaaa5555, x[1] = 0x0000ffff;
         midstate[k].a = midstate[k].c = midstate[k].e = midstate[k].g = h3i[k].a = h3i[k].c = h3i[k].e = h3i[k].g = x[0];
         midstate[k].b = midstate[k].d = midstate[k].f = midstate[k].h = h3i[k].b = h3i[k].d = h3i[k].f = h3i[k].h = x[1];
         }

      if (sequence == 0) x[0] = 0xffff0000, x[1] = 0x5555aaaa;
      if (sequence == 1) x[0] = 0x0000ffff, x[1] = 0xaaaa5555;
      if (sequence == 2) x[0] = 0x5555aaaa, x[1] = 0xffff0000;
      if (sequence == 3) x[0] = 0xaaaa5555, x[1] = 0x0000ffff;
      chunk2_static[0] = x[0];
      chunk2_static[1] = x[1];
      chunk2_static[2] = x[0];
      for (int k = 0; k < NUM_ENGINES; k++) {
         uint32 misr = engines[0].Bist();
         if (sequence == 0 && misr != 0xf98abac5) config.bist_results[k / 32] |= 1 << (k & 31);
         if (sequence == 1 && misr != 0xede353e7) config.bist_results[k / 32] |= 1 << (k & 31);
         if (sequence == 2 && misr != 0x64007018) config.bist_results[k / 32] |= 1 << (k & 31);
         if (sequence == 3 && misr != 0xe82b1df7) config.bist_results[k / 32] |= 1 << (k & 31);
         }
      }
   }


// do 1 cycle of the high speed clock
void asictype::ExecuteCycle()
    {
    bool smallnonce = (config.hashconfig >> 15) & 1;
    bool tinynonce = parent.tinynonce && (config.hashconfig >> 15) & 1;

    if (sendcontext) {
        ComputeContext();
        noncecount = 0;
        }
    sendcontext = false;

    noncecount++;
    if (tinynonce && noncecount >= 0x100)
        noncecount = 0;
    else if (smallnonce && noncecount >= 0x8000)
        noncecount = 0;
    if (noncecount == 0) {
        sendcontext = true;
        vercount += 4;
        }
    if (sendcontext && vercount >= config.versionbound_upper) {
        vercount = config.versionbound_lower;
        timecount++;
        }

    //   VerifyHit(0xa7ad933f, 0);  // erase me

    bool hit = false, truehit = false;
    int i, ctxnum;
    uint32 hitnonce;
    for (i = 0; i < NUM_ENGINES; i++) {
        uint32 raw;
        int id = NUM_ENGINES != 4 ? i : i == 0 ? 0 : i == 1 ? 1 : i == 2 ? 6 : 7;
        engines[i].nonce++;
        if (smallnonce) {
            engines[i].nonce &= 0x7fff;
            if (parent.tinynonce) {
                engines[i].nonce &= 0x00ff;   // eraseme 
                engines[i].nonce |= 0x100;    // eraseme
                }
            }
        engines[i].nonce |= (id << 24);
        engines[i].nonce = 0xa7ad933f;  // erase me
        if (engines[i].ExecuteCycle(engines[i].nonce, ctxnum, raw)) {
            hitnonce = engines[i].nonce;
            hit = true;
            }
        }

    if (hit) {
        config.general_hit_count++;
        if ((config.statconfig & 0x1ff) == (hitnonce & 0x1ff))
            config.specific_hit_count++;
        truehit = VerifyHit(hitnonce, ctxnum);
        if (truehit) {
            config.general_truehit_count++;
            if ((config.statconfig & 0x1ff) == (hitnonce & 0x1ff))
                config.specific_truehit_count++;
            }
        }
    }

// do 1 cycle of the high speed clock
uint32 enginetype::Bist()
   {
   int i, ctx;
   uint32 nonce = 0xffffffff, misr = 0x0, raw;

   for (i = 0; i <= 16425; i++)
      {
      nonce = (nonce << 1) ^ ((nonce >> 31) & 1) ^ ((nonce >> 21) & 1) ^ ((nonce >> 1) & 1) ^ ((nonce >> 0) & 1);
      ExecuteCycle(nonce, ctx, raw);
      misr = (misr << 1) ^ ((misr >> 31) & 1) ^ ((misr >> 21) & 1) ^ ((misr >> 1) & 1) ^ ((misr >> 0) & 1);
      if (i >= 44)  // this is to match rtl
         misr = misr ^ raw;
      }
   return misr;
   }


void asictype::ComputeContext() {
   int k;
   uint32 message[16];

   for (k = 0; k < 4; k++) {
      headertype& ctx = k == 0 ? config.context0 : k == 1 ? config.context1 : k == 2 ? config.context2 : config.context3;
      if (config.versionbound_upper > config.versionbound_lower)
         ver[k] = EndianSwap(ctx.x.version + ((vercount + k) << config.versionbound_shift));
      else
         ver[k] = EndianSwap(ctx.x.version);
      message[0] = ver[k];
      for (int i = 1; i < 16; i++) message[i] = EndianSwap(ctx.words[i]);

      midstate[k] = shadigesttype();
      sha256ProcessBlock(midstate[k], message);
      chunk2_static[0] = EndianSwap(config.context0.x.merkle[7]);
      chunk2_static[1] = EndianSwap(config.context0.x.time + timecount);
      chunk2_static[2] = EndianSwap(config.context0.x.difficulty);
      h3i[k] = midstate[k];
      sha256Round(h3i[k], sha_k[0] + chunk2_static[0]);
      sha256Round(h3i[k], sha_k[1] + chunk2_static[1]);
      sha256Round(h3i[k], sha_k[2] + chunk2_static[2]);
      sha256Round(h3i[k], sha_k[3] + 0);
      h3i[k].h += h3i[k].d + 0xb956C25B;
      }
   }


bool enginetype::ExecuteCycle(const uint32 nonce, int& context, uint32& raw_result) {
   int i, k;
   shadigesttype d1;
   uint32 first_w[64], chunk2[16];
   bool hit = false;
   raw_result = 0;

   chunk2[0] = parent.chunk2_static[0];
   chunk2[1] = parent.chunk2_static[1];
   chunk2[2] = parent.chunk2_static[2];
   chunk2[3] = nonce;
   chunk2[4] = 0x80000000;
   chunk2[5] = 0;
   chunk2[6] = 0;
   chunk2[7] = 0;
   chunk2[8] = 0;
   chunk2[9] = 0;
   chunk2[10] = 0;
   chunk2[11] = 0;
   chunk2[12] = 0;
   chunk2[13] = 0;
   chunk2[14] = 0;
   chunk2[15] = 640;

   sha256Expander(first_w, chunk2);

   for (k = 0; k < 4; k++) {
      d1 = parent.h3i[k];
      d1.a += nonce;
      d1.e += nonce;
      d1.h -= d1.d + 0xb956C25B;
      for (i = 4; i < 64; i++) {
         sha256Round(d1, sha_k[i] + first_w[i]);
         }
      d1 += parent.midstate[k];

      uint32 second_message[16];
      uint32 second_w[64];
      shadigesttype d2;
      second_message[0] = d1.a;
      second_message[1] = d1.b;
      second_message[2] = d1.c;
      second_message[3] = d1.d;
      second_message[4] = d1.e;
      second_message[5] = d1.f;
      second_message[6] = d1.g;
      second_message[7] = d1.h;
      second_message[8] = 0x80000000;
      second_message[9] = 0;
      second_message[10] = 0;
      second_message[11] = 0;
      second_message[12] = 0;
      second_message[13] = 0;
      second_message[14] = 0;
      second_message[15] = 256;
      sha256Expander(second_w, second_message);

      for (i = 0; i < 64; i++) {
         sha256Round(d2, sha_k[i] + second_w[i]);
         }
      d2 += shadigesttype();
      raw_result ^= d2.h;
      if (d2.h == 0) {
         context = k;
         hit = true;
         }
      }
   return hit;
   }



bool asictype::VerifyHit(uint32 nonce, int ctxnum)
   {
   shadigesttype digest, second;
   uint32 message[16], second_message[16];

   headertype& ctx = ctxnum == 0 ? config.context0 : ctxnum == 1 ? config.context1 : ctxnum == 2 ? config.context2 : config.context3;
   message[0] = chunk2_static[0];
   message[1] = chunk2_static[1];
   message[2] = chunk2_static[2];
   message[3] = nonce;
   message[4] = 0x80000000;
   message[5] = 0;
   message[6] = 0;
   message[7] = 0;
   message[8] = 0;
   message[9] = 0;
   message[10] = 0;
   message[11] = 0;
   message[12] = 0;
   message[13] = 0;
   message[14] = 0;
   message[15] = 640;

   digest = ctxnum == 0 ? midstate[0] : ctxnum == 1 ? midstate[1] : ctxnum == 2 ? midstate[2] : midstate[3];
   sha256ProcessBlock(digest, message);

   second_message[0] = digest.a;
   second_message[1] = digest.b;
   second_message[2] = digest.c;
   second_message[3] = digest.d;
   second_message[4] = digest.e;
   second_message[5] = digest.f;
   second_message[6] = digest.g;
   second_message[7] = digest.h;
   second_message[8] = 0x80000000;
   second_message[9] = 0;
   second_message[10] = 0;
   second_message[11] = 0;
   second_message[12] = 0;
   second_message[13] = 0;
   second_message[14] = 0;
   second_message[15] = 256;

   sha256ProcessBlock(second, second_message);
   // now count the zeroes
   int count = 0;
   uint32 z;
   while (true) {
      if (count == 0) z = EndianSwap(second.h);
      else if (count == 32) z = EndianSwap(second.g);
      else if (count == 64) z = EndianSwap(second.f);
      else if (count == 96) break;
      if (z >> 31) break;
      z = z << 1;
      count++;
      }
   if (count >= config.current_difficulty) { // push a hit into hit0/1
      config.hit1 = config.hit0;
      config.hit1_header = config.hit0_header;
      config.hit0 = count + (ctxnum << 8) + (config.current_sequence << 16);
      config.hit0_header = (ctxnum == 0) ? config.context0 : (ctxnum == 1) ? config.context1 : (ctxnum == 2) ? config.context2 : config.context3;
      config.hit0_header.words[16] = EndianSwap(chunk2_static[0]);
      config.hit0_header.words[17] = EndianSwap(chunk2_static[1]);
      config.hit0_header.words[18] = EndianSwap(chunk2_static[2]);
      config.hit0_header.words[19] = EndianSwap(nonce);
      config.difficult_hit_count++;
      }
   return second.h == 0;
   }


// assemble the rs232 packet and send it in
void boardmodeltype::WriteConfig(int addr, unsigned int data, int id) {
   command_cfgtype cmdpacket;
   vector<char> transmit, receive;

   cmdpacket.command = CMD_WRITE;
   cmdpacket.id = id;
   cmdpacket.address = addr;
   cmdpacket.data = data;
   cmdpacket.GenerateCrc();
   cmdpacket.SerializeOut(transmit);

   ProcessSerial(transmit, receive);
   if (receive.size()) FATAL_ERROR;	   // receive data is unexpected
   }

// assemble the rs232 packet and send it in
uint32 boardmodeltype::ReadConfig(int addr, int id) {
   command_cfgtype cmdpacket;
   response_cfgtype resppacket;
   vector<char> transmit, receive;

   cmdpacket.command = CMD_READ;
   cmdpacket.id = id;
   cmdpacket.address = addr;
   cmdpacket.GenerateCrc();
   cmdpacket.SerializeOut(transmit);

   ProcessSerial(transmit, receive);
   if (receive.size() != sizeof(resppacket)) FATAL_ERROR;
   memcpy(&resppacket, &receive[0], sizeof(resppacket));
   if (resppacket.response_unique != RSP_UNIQUE) FATAL_ERROR;
   if (!resppacket.IsCrcCorrect()) FATAL_ERROR;
   return resppacket.data;
   }

// assemble the rs232 packet and send it in
bool boardmodeltype::ReturnHit(headertype& header, int& ctx, int& difficulty, int& sequence, int id) {  // return true if there's a hit
   command_cfgtype cmdpacket;
   response_hittype resppacket;
   vector<char> transmit, receive;

   cmdpacket.command = CMD_WRITE | CMD_RETURNHIT;
   cmdpacket.id = id;
   cmdpacket.address = 0;	  // the write to Aura won't effect anything but give us an opportunity to request a hit
   cmdpacket.GenerateCrc();
   cmdpacket.SerializeOut(transmit);

   ProcessSerial(transmit, receive);
   if (receive.size() == 0) return false;
   if (receive.size() != sizeof(resppacket)) FATAL_ERROR;
   memcpy(&resppacket, &receive[0], sizeof(resppacket));
   if (resppacket.hit_unique != HIT_UNIQUE) FATAL_ERROR;
   if (!resppacket.IsCrcCorrect()) FATAL_ERROR;
   header = resppacket.header;
   ctx = (resppacket.command - CMD_LOAD0) & 3;
   difficulty = resppacket.nbits;
   sequence = resppacket.sequence;
   return true;
   }


void boardmodeltype::SendJob(const headertype& header, const int nbits, const int id) {	 // message will be 80B header
   command_loadtype cmdpacket;
   response_cfgtype resppacket;
   vector<char> transmit, receive;

   cmdpacket.command = CMD_LOAD0 | CMD_RETURNHIT;
   cmdpacket.id = id;
   if (id < 0)
      cmdpacket.command = CMD_LOAD0 | CMD_BROADCAST;
   for (int i = 0; i < 80; i++)
      cmdpacket.header = header;
   cmdpacket.header.x.nonce = 0;
   cmdpacket.nbits = nbits;
   cmdpacket.GenerateCrc();
   cmdpacket.SerializeOut(transmit);

   ProcessSerial(transmit, receive);
   //   if (receive.size()) FATAL_ERROR;	   // receive data is unexpected
   }

void boardmodeltype::SendAsciiHeader(const char* message, const int nbits, const int id)	 // message will be 160B header in hex
   {
   int i;
   headertype header;
   if (strlen(message) != 160) FATAL_ERROR;
   for (i = 0; i < 160; i++) {
      const char& ch = message[i];
      int digit;
      if (ch >= '0' && ch <= '9')
         digit = ch - '0';
      else if (ch >= 'a' && ch <= 'f')
         digit = ch - 'a' + 10;
      else if (ch >= 'A' && ch <= 'F')
         digit = ch - 'A' + 10;
      else break;
      if (i % 2 == 0)
         header.raw[i / 2] = digit << 4;
      else
         header.raw[i / 2] += digit;
      }
   SendJob(header, nbits, id);
   }

bool boardmodeltype::CheckHit(int id) {
   return ReadConfig(216, id) & 0xff;
   }
