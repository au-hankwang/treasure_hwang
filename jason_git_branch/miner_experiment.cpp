#include "pch.h"
#include "helper.h"
#include "miner.h"
#include "dvfs.h"
#include "serial.h"
#include "usb.h"
#include "test.h"
#include "multithread.h"


struct counttype {
   int zero, one;
   bool ignore;

   bool Significant()
      {
      if (ignore) return false;
      if (zero < (zero + one) * 0.4) return true;
      if (one < (zero + one) * 0.4) return true;
      return false;
      }
   counttype() {
      zero = one = 0;
      ignore = false;
      }
   };

void sha256ProcessBlockInternal(shadigesttype& digest, uint32* message, vector<unsigned>& internals)
   {
   int i;
   uint32 w[64];
   shadigesttype startdigest = digest;

   for (i = 0; i < 16; i++)
      w[i] = message[i];
   for (i = 16; i < 64; i++)
      w[i] = SIGMA4(w[i - 2]) + w[i - 7] + SIGMA3(w[i - 15]) + w[i - 16];

   for (i = 0; i < 64; i++) {
      sha256Round(digest, sha_k[i] + w[i]);
      internals.push_back(digest.a);
      internals.push_back(digest.b);
      internals.push_back(digest.c);
      internals.push_back(digest.d);
      internals.push_back(digest.e);
      internals.push_back(digest.f);
      internals.push_back(digest.g);
      internals.push_back(digest.h);
      }
   digest += startdigest;
   }

void CountProcess(const vector<unsigned>& internals, vector<counttype>& count)
   {
   int i, k;

   for (i = 0; i < internals.size(); i++)
      {
      for (k = 0; k < 32; k++)
         {
         if ((internals[i] >> k) & 1)
            count[i * 32 + k].one++;
         else
            count[i * 32 + k].zero++;
         }
      }
   }
void CountProcessLogic(const vector<unsigned>& internals, vector<counttype>& count, int method)
   {
   int i, k;

   for (i = 0; i < internals.size(); i++)
      {
      for (k = 0; k < 32; k++)
         {
         if ((internals[i] >> k) & 1)
            count[i * 32 + k].one++;
         else
            count[i * 32 + k].zero++;
         }
      }
   }

int headertype::Intermediate(vector<unsigned>& internals) const
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

   internals.push_back(digest.a);
   internals.push_back(digest.b);
   internals.push_back(digest.c);
   internals.push_back(digest.d);
   internals.push_back(digest.e);
   internals.push_back(digest.f);
   internals.push_back(digest.g);
   internals.push_back(digest.h);

   sha256ProcessBlockInternal(second, second_message, internals);
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

/*
void MinerExperiment()
   {
   vector<headertype> headers;
   vector<counttype> hit(16640), nohit(16640);
   vector<counttype> logic0(16640 * 32), logic1(16640 * 32), logic2(16640 * 32), logic3(16640 * 32), logic4(16640 * 32);
   int i;

   ReadHeaders(headers, "block_headers.txt");
   for (i = 0; i<headers.size(); i++)
      {
      headertype h = headers[i];
      int zeroes = h.ReturnZeroes();
      if (zeroes >= 40)
         {
         vector<unsigned> internals, i2;
         h.Intermediate(internals);
         CountProcess(internals, hit);
         CountProcessLogic(internals, logic0, 0);
         CountProcessLogic(internals, logic1, 1);
         CountProcessLogic(internals, logic2, 2);
         CountProcessLogic(internals, logic3, 3);
         CountProcessLogic(internals, logic4, 4);
         h.x.nonce++;
         h.Intermediate(i2);
         CountProcess(i2, nohit);
         }
      }
   for (i = 0; i < nohit.size(); i++) 
      hit[i].ignore = !nohit[i].zero || !nohit[i].one;
   for (i = 0; i < nohit.size(); i++)
      if (hit[i].Significant())
         {
         int round = i / (32 * 8);
         int letter = (i % (32 * 8))/32;
         int bit = (i % (32 * 8))%32;
         printf("digest[%d].%c.[%d] (%d,%d)\n", i / 32 / 8, 'A' + letter, bit,hit[i].zero, hit[i].one);
         }
   }
*/


