#ifndef __SHA256_H_INCLUDED__
#define __SHA256_H_INCLUDED__

static const uint32 sha_k[64] =
{
   0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
   0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
   0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
   0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
   0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
   0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
   0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
   0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2
};


#define ROL32(A, n)   ( ((A) << (n)) | ( ((A)>>(32-(n))) & ( (1UL << (n)) - 1 ) ) )
#define ROR32(A, n)   ROL32( (A), 32-(n) )
#define SHR32(a, n) ((a) >> (n))
#define CH(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define SIGMA1(x) (ROR32(x, 2) ^ ROR32(x, 13) ^ ROR32(x, 22))
#define SIGMA2(x) (ROR32(x, 6) ^ ROR32(x, 11) ^ ROR32(x, 25))
#define SIGMA3(x) (ROR32(x, 7) ^ ROR32(x, 18) ^ SHR32(x, 3))
#define SIGMA4(x) (ROR32(x, 17) ^ ROR32(x, 19) ^ SHR32(x, 10))


struct shadigesttype {
   uint32 a, b, c, d, e, f, g, h;

   shadigesttype() {
      a = 0x6A09E667;
      b = 0xBB67AE85;
      c = 0x3C6EF372;
      d = 0xA54FF53A;
      e = 0x510E527F;
      f = 0x9B05688C;
      g = 0x1F83D9AB;
      h = 0x5BE0CD19;
      }
   bool operator!=(const shadigesttype& right) const
      {
      return!operator==(right);
      }
   bool operator==(const shadigesttype& right) const
      {
      return a == right.a &&
         b == right.b &&
         c == right.c &&
         d == right.d &&
         e == right.e &&
         f == right.f &&
         g == right.g &&
         h == right.h;
      }
   void operator+=(const shadigesttype& right)
      {
      a += right.a;
      b += right.b;
      c += right.c;
      d += right.d;
      e += right.e;
      f += right.f;
      g += right.g;
      h += right.h;
      }
   };

struct shasheduletype {
   uint32 w[16];

   shasheduletype() {
      for (int i = 0; i < 16; i++)
         w[i] = 0;
      }
   };


void sha256Round(shadigesttype& digest, uint32 kw);
void sha256ProcessBlock(shadigesttype& digest, const uint32* message);
void sha256Expander(uint32* w, const uint32* message);
void RTLsha256ProcessBlock(shadigesttype& digest, const uint32* message);
uint32 RTL2Hash(shadigesttype& digest, const uint32* message);

#endif //__SHA256_H_INCLUDED__