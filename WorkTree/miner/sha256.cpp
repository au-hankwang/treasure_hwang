#include "pch.h"
#include "sha256.h"
#include "helper.h"



void sha256Expander(uint32* w, const uint32* message) {
   int i;
   for (i = 0; i < 16; i++)
      w[i] = message[i];
   for (i = 16; i < 64; i++)
      w[i] = SIGMA4(w[i - 2]) + w[i - 7] + SIGMA3(w[i - 15]) + w[i - 16];
   }



void sha256Round(shadigesttype& digest, uint32 kw) {
   uint32& a = digest.a;
   uint32& b = digest.b;
   uint32& c = digest.c;
   uint32& d = digest.d;
   uint32& e = digest.e;
   uint32& f = digest.f;
   uint32& g = digest.g;
   uint32& h = digest.h;

   uint32 temp1 = h + SIGMA2(e) + CH(e, f, g) + kw;
   uint32 temp2 = SIGMA1(a) + MAJ(a, b, c);

//   printf("A=%8X B=%8X C=%8X D=%8X E=%8X H=%8X kw=%8X temp1=%8X temp2=%8X ^A=%8X ^E=%8X\n", a, b, c, d, e, h, kw, temp1, temp2,temp1+temp2,d+temp1);

   h = g;
   g = f;
   f = e;
   e = d + temp1;
   d = c;
   c = b;
   b = a;
   a = temp1 + temp2;
   }


void sha256ExpanderRound(shasheduletype &schedule) {
   uint32 w16;
   int i;
   
   w16 = SIGMA4(schedule.w[14]) + schedule.w[9] + SIGMA3(schedule.w[1]) + schedule.w[0];
   for (i=0; i<15; i++)
      schedule.w[i] = schedule.w[i+1];
   schedule.w[15] = w16;
   }

void sha256ExpanderRoundSpecial(shasheduletype& schedule) {
   uint32 w16;
   int i;

   w16 = SIGMA4(schedule.w[14]) | schedule.w[9] | SIGMA3(schedule.w[1]) | schedule.w[0];
   for (i = 1; i < 32; i++)
      w16 = w16 | (w16 << 1);
   for (i = 0; i < 15; i++)
      schedule.w[i] = schedule.w[i + 1];
   schedule.w[15] = w16;
   }


void sha256ExpanderRound2(shasheduletype& schedule) {
   uint32 w16, w17;
   int i;

   w16 = SIGMA4(schedule.w[14]) + schedule.w[9] + SIGMA3(schedule.w[1]) + schedule.w[0];
   w17 = SIGMA4(schedule.w[15]) + schedule.w[10] + SIGMA3(schedule.w[2]) + schedule.w[1];
   for (i = 0; i < 14; i++)
      schedule.w[i] = schedule.w[i + 2];
   schedule.w[14] = w16;
   schedule.w[15] = w17;
   }


void ShaReference()
   {
   shadigesttype digest[65];
   shasheduletype schedule[65];
   int seed = 0;
   int stages, cycle;

   randomcpy(seed, &schedule[0], sizeof(schedule[0]));

   for (cycle = 0; cycle < 100; cycle++) {
      schedule[0].w[0] = cycle; // treat this as nonce
      for (stages = 63; stages >= 0; stages--) {
         // this is effectively the flops between stages
         schedule[stages + 1] = schedule[stages];
         digest[stages + 1] = digest[stages];
         sha256ExpanderRound(schedule[stages + 1]);
         sha256Round(digest[stages + 1], schedule[stages].w[0]);
         }
      }
   printf("After 100 cycles(nonce=100-64) output.a is %X\n", digest[64].a);
   }



void Constants() {
   int i, j, k;

   for (j = 31; j < 32; j++) {
      shasheduletype s, s2;
      s.w[0] = -1; // merkle
      s.w[1] = -1; // time
      s.w[2] = -1; // difficulty
      s.w[3] = -1; // nonce
      s2.w[3] = 1 << j;
      int total = 0, saved = 0;
      for (i = 0; i < 64; i++) {
         sha256ExpanderRoundSpecial(s);
         sha256ExpanderRoundSpecial(s2);
         int nontrivial = 0;
         int notcommon = 0;
         for (k = 0; k < 16; k++) {
            nontrivial += PopCount(s.w[k]);
            notcommon += PopCount(s2.w[k]);
            saved += PopCount(s.w[k] & ~s2.w[k]);
            }
         total += nontrivial;
         printf("round %d non-constants=%d, non-common=%d, total=%d, saved=%d\n", i, nontrivial, notcommon, total, saved);
         }
      printf("j=%d saved=%d %.2f%%\n", j, saved, 100.0 * saved / (total * 2));
      }
   }



void sha256ProcessBlock(shadigesttype& digest, const uint32* message)
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
      printf("i=%2d A=%8X E=%8X  W=%8X\n", i, digest.a, digest.e, w[i]);
      }

   digest += startdigest;
   }

void RTLsha256Round(int round, shadigesttype& digest, uint32 kw) {
   shadigesttype old = digest;
   uint32& a = digest.a;
   uint32& b = digest.b;
   uint32& c = digest.c;
   uint32& d = digest.d;
   uint32& e = digest.e;
   uint32& f = digest.f;
   uint32& g = digest.g;
   uint32& h = digest.h;
   uint32 temp1 = h + SIGMA2(e) + CH(e, f, g);
   uint32 temp2 = SIGMA1(a) + MAJ(a, b, c);
   uint32 x = 0, y=0;

   if (round == 0) x = old.c;
   else if (round == 1) x = old.b;
   else if (round < 63) x = old.a;
   y = round == 1 ? old.d : old.c;

   h = x + old.g + kw;
   g = old.f;
   f = old.e;
   e = temp1;
   d = old.e - y;
   c = old.b;
   b = old.a;
   a = temp2 + old.d;

   if (round == 0) {
      a = old.a;
      b = old.b;
      c = old.c;
      d = old.d;
      }
   if (round == 1) {
      a = old.a;
      b = old.b;
      c = old.c;
      }
   if (round >= 64) {
      h = old.h;
      g = old.g;
      f = old.f;
      e = old.e;
      }
   if (round == 65)
      d = old.c;
//   printf("A=%8X B=%8X C=%8X D=%8X E=%8X F=%8X G=%8X H=%8X  KW=%8X\n", digest.a, digest.b, digest.c, digest.d, digest.e, digest.f, digest.g, digest.h, kw);
   }


void RTLsha256ProcessBlock(shadigesttype& digest, const uint32* message)
   {
   int i;
   uint32 w[64];
   shadigesttype startdigest = digest;
   shadigesttype reference = digest;

   for (i = 0; i < 16; i++)
      w[i] = message[i];
   for (i = 16; i < 64; i++)
      w[i] = SIGMA4(w[i - 2]) + w[i - 7] + SIGMA3(w[i - 15]) + w[i - 16];

   digest.h += sha_k[0] + w[0] + digest.d;
   
   for (i = 0; i < 66; i++) {
      if (i<64)
         sha256Round(reference, sha_k[i] + w[i]);
      if (i < 63)
         RTLsha256Round(i, digest, sha_k[i + 1] + w[i + 1]);
      else
         RTLsha256Round(i, digest, 0);
      printf("i=%2d A=%8X B=%8X C=%8X D=%8X E=%8X/%8X  W=%8X\n", i, digest.a, digest.b, digest.c, digest.d, digest.e, reference.e, w[i]);
      }
   digest += startdigest;
   }


uint32 RTL2Hash(shadigesttype& digest, const uint32* message)
   {
   int i;
   uint32 nonce, w[64], w2[64];
   shadigesttype startdigest = digest, h3i;
   shadigesttype reference = digest;
   uint32 w16_19[4];

//   nonce = message[3] | 0x80000000;
   nonce = message[3];

   h3i = digest;  // digest from the first sha containing version, prev block, merkle root
   h3i.h += sha_k[0] + message[0] + h3i.d;
   RTLsha256Round(0, h3i, sha_k[1] + message[1]);
   RTLsha256Round(1, h3i, sha_k[2] + message[2]);
   RTLsha256Round(2, h3i, sha_k[3]);
   RTLsha256Round(3, h3i, sha_k[4] + 0x80000000);

   w16_19[0] = SIGMA3(message[1]) + message[0];
   w16_19[1] = SIGMA4(640) + SIGMA3(message[2]) + message[1];
   w16_19[2] = SIGMA4(w16_19[0]) + message[2];
   w16_19[3] = SIGMA4(w16_19[1]) + SIGMA3(0x80000000);

   for (i = 0; i < 3; i++)
      printf("message[%d]=%8x\n", i, message[i]);
   for (i=0; i<4; i++)
      printf("w16_19[%d]=%8x\n",i, w16_19[i]);
   printf("Nonce=%8x\n", nonce);
   printf("Midstate A=%8X B=%8X C=%8X D=%8X E=%8X F=%8X G=%8X H=%8X\n", digest.a, digest.b, digest.c, digest.d, digest.e, digest.f, digest.g, digest.h);
   printf("H3I      A=%8X B=%8X C=%8X D=%8X E=%8X F=%8X G=%8X H=%8X\n", h3i.a, h3i.b, h3i.c, h3i.d, h3i.e, h3i.f, h3i.g, h3i.h);

   w[0] = 0;
   w[1] = 0;
   w[2] = 0;
   w[3] = 0;
   w[4] = 0;
   w[5] = 0;
   w[6] = 0;
   w[7] = 0;
   w[8] = 0;
   w[9] = 0;
   w[10] = 640;
   w[11] = w16_19[0];
   w[12] = w16_19[1];
   w[13] = w16_19[2] + SIGMA3(nonce);
   w[14] = w16_19[3] + nonce;
   w[15] = SIGMA4(w16_19[2] + SIGMA3(nonce)) ^ 0x80000000;

   for (i = 16; i < 64; i++)
      w[i] = SIGMA4(w[i - 2]) + w[i - 7] + SIGMA3(w[i - 15]) + w[i - 16];

   digest = h3i;
   digest.e = h3i.e + nonce;
   sha256Round(reference, sha_k[0] + message[0]);
   sha256Round(reference, sha_k[1] + message[1]);
   sha256Round(reference, sha_k[2] + message[2]);
   sha256Round(reference, sha_k[3] + nonce);

   printf("i=%2d A=%8X B=%8X C=%8X D=%8X E=%8X F=%8X G=%8X H=%8X\n", 4, digest.a, digest.b, digest.c, digest.d, digest.e, digest.f, digest.g, digest.h);
   for (i = 4; i < 66; i++) {
      if (i == 4)
         sha256Round(reference, sha_k[i] + 0x80000000);
      else if (i < 64)
         sha256Round(reference, sha_k[i] + w[i-4-1]);

      if (i < 63)
         RTLsha256Round(i, digest, sha_k[i + 1] + w[i -4]);
      else
         RTLsha256Round(i, digest, 0);
      printf("i=%2d A=%8X B=%8X C=%8X D=%8X E=%8X F=%8X G=%8X H=%8X  W=%8X\n", i+1, digest.a, digest.b, digest.c, digest.d, digest.e, digest.f, digest.g, digest.h, w[i + 1 - 4]);
      }
   if (reference != digest) 
      FATAL_ERROR;
   digest += startdigest;
   printf("\nA=%8X B=%8X C=%8X D=%8X E=%8X F=%8X G=%8X H=%8X\n", digest.a, digest.b, digest.c, digest.d, digest.e, digest.f, digest.g, digest.h);

   shadigesttype d2;
   w2[0] = digest.a;
   w2[1] = digest.b;
   w2[2] = digest.c;
   w2[3] = digest.d;
   w2[4] = digest.e;
   w2[5] = digest.f;
   w2[6] = digest.g;
   w2[7] = digest.h;
   w2[8] = 0x80000000;
   w2[9] = 0;
   w2[10] = 0;
   w2[11] = 0;
   w2[12] = 0;
   w2[13] = 0;
   w2[14] = 0;
   w2[15] = 256;

   for (i = 16; i < 64; i++)
      w2[i] = SIGMA4(w2[i - 2]) + w2[i - 7] + SIGMA3(w2[i - 15]) + w2[i - 16];

   reference = shadigesttype();
   d2.h = shadigesttype().d + shadigesttype().h + sha_k[0] + digest.a;
   printf("i=%2d A=%8X B=%8X C=%8X D=%8X E=%8X F=%8X G=%8X H=%8X\n", 0, d2.a, d2.b, d2.c, d2.d, d2.e, d2.f, d2.g, d2.h);
   for (i = 0; i < 61; i++) {
      sha256Round(reference, sha_k[i] + w2[i]);
      RTLsha256Round(i, d2, sha_k[i + 1] + w2[i + 1]);
      printf("i=%2d A=%8X B=%8X C=%8X D=%8X E=%8X F=%8X G=%8X H=%8X  W=%8X\n", i + 1, d2.a, d2.b, d2.c, d2.d, d2.e, d2.f, d2.g, d2.h, w2[i + 1]);
      }
   if (reference.e != d2.e) FATAL_ERROR;
   d2.e += shadigesttype().h;
   if (d2.e != 0) FATAL_ERROR;
   return d2.e;
   }
