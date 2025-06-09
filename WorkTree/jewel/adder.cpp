#include "pch.h"
#include "helper.h"
#include "multithread.h"


enum e_gatetype  { E_NULL, E_BUF, E_INV, E_AND, E_OR, E_NAND, E_NOR, E_AO, E_OA, E_AOI, E_OAI, E_XOR, E_XNR };



class gatetype {
public:
   e_gatetype type;
   char name[16];
   int bit, level, z, a, b, c;
   int size, vt, timing;
   bool used;
   gatetype(const char *_name, e_gatetype _type,int _level, int _bit, int _z, int _a, int _b=-1, int _c=-1) : type(_type), level(_level), bit(_bit), z(_z),a(_a), b(_b), c(_c)
      {
      strcpy(name, _name);
      size = 1;
      vt = 0;
      }
   int InputLoading() const
      {
      if (type == E_NULL) return 0;
      if (type == E_XOR) return size * 2;
      if (type == E_XNR) return size * 2;
      return size;
      }
   int OutputLoading() const
      {
      return size / 3.0;
      }
   float Delay(float load) const
      {
      float size_mult = size == 2 ? 2.5 : 1.0;  // 2x has more drive and less variation

      load = MAXIMUM(load, 2);
      
      if (vt == 1)
         size_mult *= 0.7;
      if (vt == 2)
         size_mult *= 0.45;
      if (vt == 3)
         size_mult *= 0.25;

      if (type == E_XOR || type == E_XNR) return 2.0*load/ size_mult + 2;
      if (type==E_INV) return 1.0 * load / size_mult;
      if (type == E_BUF) return 1.0 * load / size_mult + 1;
      return 2.0 * load / size_mult;
      }
   };
class nodetype {
public:
   char name[16];
   int level, bit, lowbit;
   bool undriven, value, negative, constant;
   float load, arrival, reverse, slack;
   nodetype(const char* _name) {
      char* chptr;
      strcpy(name, _name);
      level = atoi(name + 1);
      if (name[0] == 's') level = 8;
      chptr = strchr(name, '[');
      if (chptr != NULL) {
         bit = atoi(chptr+1);
         }
      else {
         bit = atoi(name + 3);
         }
      lowbit = bit;
      chptr = strstr(name, "__");
      if (chptr!=NULL) lowbit = atoi(chptr + 2);
      undriven = true;
      negative = false;
      constant = false;
      }
   nodetype() {
      name[0] = 0;
      level = -1;
      bit = -1;
      lowbit = -1;
      undriven = true;
      negative = false;
      constant = false;
      }
   };

class addertype {
public:
   vector<gatetype> gates;
   vector<nodetype> nodes;
   bool cin, inn, onn, thirtyone;
   char modulename[256];
   addertype()
      {
      cin = inn = onn = thirtyone = false;
      }
   void AddGate(const char *name, e_gatetype type, int bit, const char* z, const char* a, const char* b = NULL, const char* c = NULL)
      {
      int i;
      int nodea = -1, nodeb = -1, nodec = -1;

      for (i=nodes.size() - 1; i >= 0; i--)
         {
         if (strcmp(nodes[i].name, a) == 0) break;
         }
      if (i < 0) FATAL_ERROR;
      nodea = i;
      if (b != NULL) {
         for (i = nodes.size() - 1; i >= 0; i--)
            {
            if (strcmp(nodes[i].name, b) == 0) break;
            }
         if (i < 0) FATAL_ERROR;
         nodeb = i;
         }
      if (c != NULL) {
         for (i = nodes.size() - 1; i >= 0; i--)
            {
            if (strcmp(nodes[i].name, c) == 0) break;
            }
         if (i < 0) FATAL_ERROR;
         nodec = i;
         }

      nodes.push_back(nodetype(z));
      gates.push_back(gatetype(name, type, nodes.back().level, bit, nodes.size() - 1, nodea, nodeb, nodec));
      }
   void DumpToFile(const char* filename);
   bool Verify(unsigned a, unsigned b);   // return true on error
   void PlaceConstants(unsigned increment);
   void Simplify();
   void Demorgan();
   void LeakageRecovery();
   float Delay();
   };

bool addertype::Verify(unsigned a, unsigned b)
   {
   int i;
   unsigned s = a + b + (cin?1:0);
   if (inn) s = ~a + ~b + (cin ? 1 : 0);
   if (onn) s = ~s;

   if (thirtyone) {
      a &= 0x7fffffff;
      b &= 0x7fffffff;
      s &= 0x7fffffff;
      }

   for (i = 0; i < nodes.size(); i++)
      {
      nodetype& n = nodes[i];
      if (n.name[0] == 'a')
         {
         n.undriven = false;
         n.value = (a >> n.bit) & 1;
         }
      else if (n.name[0] == 'b')
         {
         n.undriven = false;
         n.value = (b >> n.bit) & 1;
         }
      }
   for (int pass = 0; pass < 8; pass++) { // this is needed in case gates are ordered badly
      for (i = 0; i < gates.size(); i++)
         {
         const gatetype& g = gates[i];
         bool result = false;
         switch (g.type)
            {
            case E_NULL: FATAL_ERROR; break;
            case E_INV:  result = !nodes[g.a].value; break;
            case E_BUF:  result = nodes[g.a].value; break;
            case E_AND:  result = nodes[g.a].value && nodes[g.b].value && (g.c < 0 || nodes[g.c].value); break;
            case E_NAND: result = !(nodes[g.a].value && nodes[g.b].value && (g.c < 0 || nodes[g.c].value)); break;
            case E_OR:  result = nodes[g.a].value || nodes[g.b].value || (g.c >= 0 && nodes[g.c].value); break;
            case E_NOR:  result = !(nodes[g.a].value || nodes[g.b].value || (g.c >= 0 && nodes[g.c].value)); break;
            case E_XOR:  result = nodes[g.a].value ^ nodes[g.b].value ^ (g.c >= 0 && nodes[g.c].value); break;
            case E_XNR:  result = !(nodes[g.a].value ^ nodes[g.b].value ^ (g.c >= 0 && nodes[g.c].value)); break;
            case E_AO:  result = (nodes[g.a].value && nodes[g.b].value) || nodes[g.c].value; break;
            case E_OA:  result = (nodes[g.a].value || nodes[g.b].value) && nodes[g.c].value; break;
            case E_AOI:  result = !((nodes[g.a].value && nodes[g.b].value) || nodes[g.c].value); break;
            case E_OAI:  result = !((nodes[g.a].value || nodes[g.b].value) && nodes[g.c].value); break;
            }
         nodes[g.z].undriven = false;
         nodes[g.z].value = result;
         }
      }
   unsigned sum = 0;
   for (i = 0; i < nodes.size(); i++)
      {
      nodetype& n = nodes[i];
      if (n.name[0] == 's')
         {
         if (n.undriven) FATAL_ERROR;
         if (n.value)
            sum = sum | (1 << n.bit);
         }
      }
   if (sum != s)
      {
      printf("A=%x B=%x S=%x, actual=%x\n", a, b, s, sum);
      return true;
      }
   return false;
   }

void addertype::PlaceConstants(unsigned increment)
   {
   int i, k;

   for (i = 0; i < gates.size(); i++)
      {
      gatetype& g = gates[i];
      g.used = false;
      }
   for (i = 0; i < nodes.size(); i++)
      {
      nodetype& n = nodes[i];
      if (n.name[0]=='b')
         {
         n.constant = true;
         n.value = (increment >> n.bit) & 1;
         }
      }
   while (true) {
      bool done = true;
      for (i = 0; i < gates.size(); i++)
         {
         gatetype& g = gates[i];

         if (nodes[g.a].constant)
            {
            bool val = nodes[g.a].value;
            if ((g.type == E_AND && !val) || (g.type == E_OR && val))
               {
               nodes[g.z].constant = true;
               nodes[g.z].value = false;
               g.type = E_NULL;
               }
            if ((g.type == E_AND && val) || (g.type == E_OR && !val))
               {
               g.type = E_NULL;
               for (k = 0; k < gates.size(); k++)
                  {
                  gatetype& g2 = gates[k];
                  if (g2.a == g.z) g2.a = g.b;
                  if (g2.b>=0 && g2.b == g.z) g2.b = g.b;
                  if (g2.c >= 0 && g2.c == g.z) g2.c = g.b;
                  }
               }
            if ((g.type == E_XOR && !val) || (g.type == E_XNR && val))
               {
               if (g.level == 8) {
                  g.a = g.b;
                  g.b = -1;
                  g.type = E_BUF;
                  }
               else {
                  g.type = E_NULL;
                  for (k = 0; k < gates.size(); k++)
                     {
                     gatetype& g2 = gates[k];
                     if (g2.a == g.z) g2.a = g.b;
                     if (g2.b >= 0 && g2.b == g.z) g2.b = g.b;
                     if (g2.c >= 0 && g2.c == g.z) g2.c = g.b;
                     }
                  }
               }
            if ((g.type == E_XOR && val) || (g.type == E_XNR && !val))
               {
               g.type = E_INV;
               g.a = g.b;
               g.b = -1;
               }
            if (g.type == E_AO && val)
               {
               g.type = E_OR;
               g.a = g.b;
               g.b = g.c;
               g.c = -1;
               }
            if (g.type == E_AO && !val)
               {
               g.type = E_NULL;
               for (k = 0; k < gates.size(); k++)
                  {
                  gatetype& g2 = gates[k];
                  if (g2.a == g.z) g2.a = g.c;
                  if (g2.b >= 0 && g2.b == g.z) g2.b = g.c;
                  if (g2.c >= 0 && g2.c == g.z) g2.c = g.c;
                  }
               }
            if (g.type == E_OA && !val)
               {
               g.type = E_AND;
               g.a = g.b;
               g.b = g.c;
               g.c = -1;
               }
            if (g.type == E_OA && val)
               {
               g.type = E_NULL;
               for (k = 0; k < gates.size(); k++)
                  {
                  gatetype& g2 = gates[k];
                  if (g2.a == g.z) g2.a = g.c;
                  if (g2.b >= 0 && g2.b == g.z) g2.b = g.c;
                  if (g2.c >= 0 && g2.c == g.z) g2.c = g.c;
                  }
               }
            done = false;
            }
         if (g.b>=0 && nodes[g.b].constant)
            {
            bool val = nodes[g.b].value;
            if ((g.type == E_AND && !val) || (g.type == E_OR && val))
               {
               nodes[g.z].constant = true;
               nodes[g.z].value = val;
               g.type = E_NULL;
               }
            if ((g.type == E_AND && val) || (g.type == E_OR && !val))
               {
               g.type = E_NULL;
               for (k = 0; k < gates.size(); k++)
                  {
                  gatetype& g2 = gates[k];
                  if (g2.a == g.z) g2.a = g.a;
                  if (g2.b >= 0 && g2.b == g.z) g2.b = g.a;
                  if (g2.c >= 0 && g2.c == g.z) g2.c = g.a;
                  }
               }
            if ((g.type == E_XOR && !val) || (g.type == E_XNR && val))
               {
               g.type = E_NULL;
               for (k = 0; k < gates.size(); k++)
                  {
                  gatetype& g2 = gates[k];
                  if (g2.a == g.z) g2.a = g.a;
                  if (g2.b >= 0 && g2.b == g.z) g2.b = g.a;
                  if (g2.c >= 0 && g2.c == g.z) g2.c = g.a;
                  }
               }
            if ((g.type == E_XOR && val) || (g.type == E_XNR && !val))
               {
               g.type = E_INV;
               g.b = -1;
               }
            if (g.type == E_AO && val)
               {
               g.type = E_OR;
               g.b = g.c;
               g.c = -1;
               }
            if (g.type == E_AO && !val)
               {
               g.type = E_NULL;
               for (k = 0; k < gates.size(); k++)
                  {
                  gatetype& g2 = gates[k];
                  if (g2.a == g.z) g2.a = g.c;
                  if (g2.b >= 0 && g2.b == g.z) g2.b = g.c;
                  if (g2.c >= 0 && g2.c == g.z) g2.c = g.c;
                  }
               }
            if (g.type == E_OA && !val)
               {
               g.type = E_AND;
               g.b = g.c;
               g.c = -1;
               }
            if (g.type == E_OA && val)
               {
               g.type = E_NULL;
               for (k = 0; k < gates.size(); k++)
                  {
                  gatetype& g2 = gates[k];
                  if (g2.a == g.z) g2.a = g.c;
                  if (g2.b >= 0 && g2.b == g.z) g2.b = g.c;
                  if (g2.c >= 0 && g2.c == g.z) g2.c = g.c;
                  }
               }
            done = false;
            }
         if (g.c >= 0 && nodes[g.c].constant)
            {
            bool val = nodes[g.c].value;
            if (g.type == E_AO && !val)
               {
               g.type = E_AND;
               g.c = -1;
               }
            if (g.type == E_AO && val)
               {
               nodes[g.z].constant = true;
               nodes[g.z].value = val;
               }
            if (g.type == E_OA && val)
               {
               g.type = E_OR;
               g.c = -1;
               }
            if (g.type == E_OA && !val)
               {
               nodes[g.z].constant = true;
               nodes[g.z].value = val;
               }
            done = false;
            }
         }
         break;

      if (done) break;
      }
   }

void addertype::Simplify()
   {
   int i, l;

   for (i = 0; i < gates.size(); i++)
      {
      gatetype& g = gates[i];
      g.used = false;
      }
   for (i = 0; i < nodes.size(); i++)
      {
      nodetype& n = nodes[i];
      n.undriven = true;
      if (n.name[0] == 's') n.undriven = false;
      }
   for (l = 8; l >= 0; l--)
      {
      for (i = gates.size()-1; i>=0; i--)
         {
         gatetype& g = gates[i];
         if (!nodes[g.z].undriven) {
            g.used = true;
            nodes[g.a].undriven = false;
            if (g.b>=0) nodes[g.b].undriven = false;
            if (g.c >= 0) nodes[g.c].undriven = false;
            }
         }
      }

   vector<gatetype> clean;
   for (i = 0; i < gates.size(); i++)
      {
      gatetype& g = gates[i];
      if (g.used) clean.push_back(g);
      }
   gates = clean;
   }

float addertype::Delay()
   {
   int i;

   for (i = 0; i < nodes.size(); i++)
      {
      nodetype& n = nodes[i];
      n.load = 0;
      n.arrival = 0;
      n.slack = 0;
      }
   for (i = 0; i < gates.size(); i++)
      {
      gatetype& g = gates[i];
      nodes[g.a].load += g.InputLoading();
      if (g.b >= 0)
         nodes[g.b].load += g.InputLoading();
      if (g.c >= 0)
         nodes[g.c].load += g.InputLoading();
      nodes[g.z].load += g.OutputLoading();
      }

   while (true)
      {
      bool done = true;
      for (i = 0; i < gates.size(); i++)
         {
         const gatetype& g = gates[i];
         float delay = g.Delay(nodes[g.z].load);
         if (nodes[g.a].arrival + delay > nodes[g.z].arrival) {
            nodes[g.z].arrival = nodes[g.a].arrival + delay;
            done = false;
            }
         if (g.b>=0 && nodes[g.b].arrival + delay > nodes[g.z].arrival) {
            nodes[g.z].arrival = nodes[g.b].arrival + delay;
            done = false;
            }
         if (g.c >= 0 && nodes[g.c].arrival + delay > nodes[g.z].arrival) {
            nodes[g.z].arrival = nodes[g.c].arrival + delay;
            done = false;
            }
         }
      if (done) break;
      }
   float worst = 0.0;
   for (i = 0; i < nodes.size(); i++)
      {
      nodetype& n = nodes[i];
      worst = MAXIMUM(worst, n.arrival);
      n.reverse = 0;
      }
   for (i = 0; i < nodes.size(); i++)
      {
      nodetype& n = nodes[i];
      n.reverse = worst;
      }
   while (true)
      {
      bool done = true;
      for (i = gates.size()-1; i>=0; i--)
         {
         const gatetype& g = gates[i];
         float delay = g.Delay(nodes[g.z].load);
         if (nodes[g.z].reverse - delay < nodes[g.a].reverse) {
            nodes[g.a].reverse = nodes[g.z].reverse - delay;
            done = false;
            }
         if (g.b>=0 && nodes[g.z].reverse - delay < nodes[g.b].reverse) {
            nodes[g.b].reverse = nodes[g.z].reverse - delay;
            done = false;
            }
         if (g.c>=0 && nodes[g.z].reverse - delay < nodes[g.c].reverse) {
            nodes[g.c].reverse = nodes[g.z].reverse - delay;
            done = false;
            }
         }
      if (done) break;
      }


   return worst;
   }

void addertype::LeakageRecovery()
   {
   int i, pass;
   float worst = Delay();
  
   for (pass = 0; pass < 3; pass++) {
      for (i = 0; i < gates.size(); i++)
         {
         gatetype& g = gates[i];

         g.vt++;
         if (Delay() > worst) g.vt--;
         }
      }
   // now change the A1,A2 ordering 
   for (i = 0; i < gates.size(); i++)
      {
      gatetype& g = gates[i];
      if (g.type == E_AOI || g.type == E_AO || g.type == E_OAI || g.type == E_OA || g.type == E_NAND || g.type == E_NOR || g.type == E_AND || g.type == E_OR)
         {
         if (nodes[g.a].arrival < nodes[g.b].arrival)
            {
            int swap = g.a;
            g.a = g.b;
            g.b = swap;
            }
         }
      }
   }

void addertype::Demorgan()
   {
   int i, k, l;

   for (i = 0; i < nodes.size(); i++)
      {
      nodetype& n = nodes[i];
      if (n.name[0]=='a' || n.name[0] == 'b')
         n.negative = inn;
      else
         n.negative = false;
      }
   for (i = 0; i < gates.size(); i++)
      {
      gatetype& g = gates[i];
//      if (g.type == E_INV)
//         nodes[g.z].negative = !nodes[g.a].negative;
      }
   for (l = 0; l <=8; l++)
      {
      for (i = 0; i<gates.size(); i++)
         {
         if (l == 8) {
            if (gates[i].level == l) {

               if (gates[i].type == E_XOR || gates[i].type == E_XNR) {// I want to put the final xor/xnr on inverted props if available to take load off
                  for (k = 0; k < gates.size(); k++)
                     if (gates[k].type == E_INV && gates[k].a == gates[i].b)
                        {
                        gates[i].b = gates[k].z;
                        break;
                        }
                  }

               bool a_negative = nodes[gates[i].a].negative;
               bool b_negative = gates[i].b >=0 ? nodes[gates[i].b].negative : a_negative;
               if (gates[i].type == E_XOR)
                  {
                  if (a_negative ^ b_negative ^ onn) gates[i].type = E_XNR;
                  }
               else if (gates[i].type == E_XNR)
                  {
                  if (a_negative ^ b_negative ^ onn) gates[i].type = E_XOR;
                  }
               else if (gates[i].type == E_BUF)
                  {
                  if (a_negative ^ onn) gates[i].type = E_INV;
                  }
               else if (gates[i].type == E_NULL);
               else FATAL_ERROR;
               }
            }
         else if (gates[i].level == l)
            {
            bool a_negative = nodes[gates[i].a].negative;
            bool b_negative = gates[i].b>=0 ? nodes[gates[i].b].negative : a_negative;
            bool c_negative = gates[i].c >= 0 ? nodes[gates[i].c].negative : b_negative;

            if (a_negative != b_negative && c_negative != b_negative && gates[i].type == E_AO && (l != 6 || nodes[gates[i].z].bit != 28) && (l != 6 || nodes[gates[i].z].bit != 25))
               {
               char name[64], z[64], tmp[64];
               sprintf(name, "U%d_IP%d", gates[i].level - 1, nodes[gates[i].b].bit);
               sprintf(z, "%s_n", nodes[gates[i].b].name);
               strcpy(tmp, nodes[gates[i].b].name);
               k = -1;
               for (k = gates.size() - 1; k >= 0; k--)
                  if (gates[k].type == E_INV && gates[k].a == gates[i].b) break;
               if (k < 0) {
                  AddGate(name, E_INV, nodes[gates[i].b].bit, z, tmp);
                  k = gates.size() - 1;
                  nodes[gates.back().z].negative = !b_negative;
                  }
               gates[i].b = gates[k].z;
               b_negative = !b_negative;
               }
            if (a_negative != b_negative && gates[i].type==E_AO)
               {
               char name[64], z[64], tmp[64];
               a_negative = b_negative;
               sprintf(name, "U%d_IP%d", gates[i].level - 1, nodes[gates[i].a].bit);
               sprintf(z, "%s_n", nodes[gates[i].a].name);
               strcpy(tmp, nodes[gates[i].a].name);
               for (k = gates.size() - 1; k >= 0; k--)
                  if (0 == strcmp(nodes[gates[k].z].name, z)) break;
               if (k < 0) {
                  AddGate(name, E_INV, nodes[gates[i].a].bit, z, tmp);
                  nodes[gates.back().z].negative = !nodes[gates.back().a].negative;
                  gates[i].a = nodes.size() - 1;
                  }
               else
                  gates[i].a = gates[k].z;
               }
            if (c_negative != b_negative && gates[i].type == E_AO)
               {
               char name[64], z[64], tmp[64];
               c_negative = b_negative;
               sprintf(name, "U%d_IG%d", gates[i].level - 1, nodes[gates[i].c].bit);
               sprintf(z, "%s_n", nodes[gates[i].c].name);
               strcpy(tmp, nodes[gates[i].c].name);
               for (k = gates.size() - 1; k >= 0; k--)
                  if (0 == strcmp(nodes[gates[k].z].name, z)) break;
               if (k < 0) {
                  AddGate(name, E_INV, nodes[gates[i].c].bit, z, tmp);
                  nodes[gates.back().z].negative = !nodes[gates.back().a].negative;
                  gates[i].c = nodes.size() - 1;
                  }
               else
                  gates[i].c = gates[k].z;
               }
            if (a_negative != b_negative && gates[i].type == E_AND) {
               char name[64], z[64], tmp[64];
               a_negative = b_negative;
               sprintf(name, "U%d_IP%d", gates[i].level - 1, nodes[gates[i].a].bit);
               sprintf(z, "%s_n", nodes[gates[i].a].name);
               strcpy(tmp, nodes[gates[i].a].name);
               for (k = gates.size() - 1; k >= 0; k--)
                  if (0 == strcmp(nodes[gates[k].z].name, z)) break;
               if (k < 0) {
                  AddGate(name, E_INV, nodes[gates[i].a].bit, z, tmp);
                  nodes[gates.back().z].negative = !nodes[gates.back().a].negative;
                  gates[i].a = nodes.size() - 1;
                  }
               else
                  gates[i].a = gates[k].z;
               }

            if (a_negative) {
               if (gates[i].type == E_AND) gates[i].type = E_NOR;
               else if (gates[i].type == E_OR) gates[i].type = E_NAND;
               else if (gates[i].type == E_AO) gates[i].type = E_OAI;
               else if (gates[i].type == E_XOR) gates[i].type = E_XOR;
               else if (gates[i].type == E_INV) gates[i].type = E_BUF;
               else if (gates[i].type == E_BUF) gates[i].type = E_INV;
               nodes[gates[i].z].negative = false;
               }
            else {
               if (gates[i].type == E_AND) gates[i].type = E_NAND;
               else if (gates[i].type == E_OR) gates[i].type = E_NOR;
               else if (gates[i].type == E_AO) gates[i].type = E_AOI;
               else if (gates[i].type == E_XOR) gates[i].type = E_XNR;
               else if (gates[i].type == E_XNR) gates[i].type = E_XOR;
               else if (gates[i].type == E_INV) gates[i].type = E_BUF;
               else if (gates[i].type == E_BUF) gates[i].type = E_INV;
               nodes[gates[i].z].negative = true;
               }
            }
         }
      }
   nodes.push_back(nodetype("Unused"));
   for (i = 0; i < gates.size(); i++)
      {
      int driver;
      for (driver = gates.size() - 1; driver >= 0; driver--)
         if (gates[driver].z == gates[i].a) break;
      if (gates[i].type == E_INV && driver>=0)
         {
         for (k = gates.size()-1; k>=0; k--)
            {
            if (k != i && gates[k].a == gates[i].a) break;
            if (k != i && gates[k].b>=0 && gates[k].b == gates[i].a) break;
            if (k != i && gates[k].c >= 0 && gates[k].c == gates[i].a) break;
            }
         if (k < 0)  // the inverter input in unused anywhere else
            {
            if (gates[driver].type == E_NAND) {
               gates[driver].type = E_AND;
               gates[driver].z = gates[i].z;
               gates[i].z = nodes.size() - 1;
               gates.erase(gates.begin() + i);
               }
            if (gates[driver].type == E_NOR) {
               gates[driver].type = E_OR;
               gates[driver].z = gates[i].z;
               gates[i].z = nodes.size()-1;
               gates.erase(gates.begin() + i);
               }
            }
         }
      }
   for (i = 0; i < gates.size(); i++)
      {
      gatetype& g = gates[i];
      if (g.type == E_BUF && g.level<8)
         {
         g.type = E_NULL;
         for (k = 0; k < gates.size(); k++)
            {
            if (k != i && gates[k].a == g.z)
               gates[k].a = g.a;
            if (k != i && gates[k].b >= 0 && gates[k].b == g.z)
               gates[k].b = g.a;
            if (k != i && gates[k].c>=0 && gates[k].c == g.z)
               gates[k].c = g.a;
            }
         }
      }
   for (i = 0; i < nodes.size(); i++)
      {
      nodetype& n = nodes[i];
      if (strstr(n.name, "]_n") != NULL)
         {
         char* chptr = strchr(n.name, '[');
         *chptr = '_';
         chptr = strchr(n.name, ']');
         strcpy(chptr, "_n");
         }
      }
   }


void addertype::DumpToFile(const char* filename)
   {
   int i, k, l;
   int gatecount = 0, tcount = 0;
   float weighted = 0.0;
   char modulestring[64], tmp[256];
   FILE* fptr = fopen(filename, "wt");
   bool simple = false;

   if (fptr == NULL) FATAL_ERROR;
   printf("Writing %s\n", filename);
   if (thirtyone) {
      fprintf(fptr, "module %s(csa_co, csa_s, sum);\n", modulename);
      fprintf(fptr, "\tinput   [31:1] csa_co;\n");
      fprintf(fptr, "\tinput   [31:0] csa_s;\n");
      fprintf(fptr, "\toutput  [31:0] sum;\n\n");
      fprintf(fptr, "wire[30:0] a = csa_co[31:1];\n");
      fprintf(fptr, "wire[30:0] b = csa_s[31:1];\n");
      fprintf(fptr, "wire[30:0] s;\n\n");
      fprintf(fptr, "assign	    sum[31:1] = s[30:0];\n");
      if (onn ^ inn)
         fprintf(fptr, "au_inv #(.size(1),.vt(3))	      	U0_csa_0(.A(csa_s[0]), .X(sum[0]));\n\n");
      else
         fprintf(fptr, "au_buf #(.size(1),.vt(3))	      	U0_csa_0(.A(csa_s[0]), .X(sum[0]));\n\n");
      }
   else {
      if (strstr(filename, "incr") == NULL)
         fprintf(fptr, "module %s(a,b,s);\n", modulename);
      else
         fprintf(fptr, "module %s(a,s);\n", modulename);
      fprintf(fptr, "\tinput   [%d:0] a;\n", 31);
      if (strstr(filename,"incr")==NULL)
         fprintf(fptr, "\tinput   [%d:0] b;\n", 31);
      fprintf(fptr, "\toutput  [%d:0] s;\n", 31);
      }
for (i = 0; i < gates.size(); i++)
      {
      gatetype& g = gates[i];
      g.used = true;
      }
   for (l = 0; l <= 8; l++)
      {
      fprintf(fptr, "// Level %d\n", l);
      for (int pass = 0; pass < 2; pass++) {
         for (i = 0; i < gates.size(); i++)
            {
            gatetype& g = gates[i];

            if (i == 108 && pass == 0) 
               printf("");
            for (k = i + 1; k < gates.size() && g.used; k++) {
               gatetype& g2 = gates[MINIMUM(gates.size() - 1, k)];

               const char* modulename =
                  g.type == E_BUF ? "au_buf  " :
                  g.type == E_INV ? "au_inv  " :
                  g.type == E_AND ? "au_an2  " :
                  g.type == E_OR ? "au_or2  " :
                  g.type == E_NAND ? "au_nd2  " :
                  g.type == E_NOR ? "au_nr2  " :
                  g.type == E_XOR ? "au_xor2 " :
                  g.type == E_XNR ? "au_xnr2 " :
                  g.type == E_AO ? "au_ao21 " :
                  g.type == E_OA ? "au_oa21 " :
                  g.type == E_AOI ? "au_aoi21" :
                  g.type == E_OAI ? "au_oai21" :
                  "Unknown";
               sprintf(modulestring, "%-11s #(.size(%d),.vt(%d))", modulename, g.size, g.vt);
               if (!simple && g.level == l && g.type == E_XNR && g2.type == E_NAND && nodes[g2.z].bit == nodes[g.z].bit && g.size == 1 && g2.size == 1)
                  {
                  if (pass == 0) {
                     gatecount++;
                     int cnt = 10;
                     cnt *= g.size;
                     tcount += cnt;
                     weighted += cnt / (1 << g.vt);
                     sprintf(modulestring, "%s #(.vt(%d))", "au_xnr_nand", MINIMUM(g.vt,g2.vt));
                     fprintf(fptr, "%-30s%-10s(", modulestring, g.name);
                     sprintf(tmp, ".A(%s),", nodes[g.a].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".B(%s),", nodes[g.b].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".CON(%s),", nodes[g2.z].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".SN(%s));", nodes[g.z].name); fprintf(fptr, "%-18s", tmp);
                     if (nodes[g.z].load > 0 || nodes[g.z].arrival > 0) {
                        fprintf(fptr, "// load=%4.1f delay=%4.1f arrival=%4.1f slack=%4.1f \n", MAXIMUM(nodes[g.z].load, nodes[g2.z].load), MAXIMUM(g.Delay(nodes[g.z].load), g2.Delay(nodes[g2.z].load)), MAXIMUM(nodes[g.z].arrival, nodes[g2.z].arrival), MINIMUM(nodes[g.z].reverse - nodes[g.z].arrival, nodes[g2.z].reverse - nodes[g2.z].arrival));
                        }
                     else fprintf(fptr, "\n");
                     }
                  g.used = false;
                  g2.used = false;
                  }
               else if (!simple && g.level == l && g.type == E_XOR && g2.type == E_NOR && nodes[g2.z].bit == nodes[g.z].bit && g.size == 1 && g2.size == 1)
                  {
                  if (pass == 0) {
                     gatecount++;
                     int cnt = 10;
                     cnt *= g.size;
                     tcount += cnt;
                     weighted += cnt / (1 << g.vt);
                     sprintf(modulestring, "%s #(.vt(%d))", "au_xor_nor", MINIMUM(g.vt, g2.vt));
                     fprintf(fptr, "%-30s %-10s(", modulestring, g.name);
                     sprintf(tmp, ".A(%s),", nodes[g.a].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".B(%s),", nodes[g.b].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".CON(%s),", nodes[g2.z].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".SN(%s));", nodes[g.z].name); fprintf(fptr, "%-18s", tmp);
                     if (nodes[g.z].load > 0 || nodes[g.z].arrival > 0) {
                        fprintf(fptr, "// load=%4.1f delay=%4.1f arrival=%4.1f slack=%4.1f \n", MAXIMUM(nodes[g.z].load, nodes[g2.z].load), MAXIMUM(g.Delay(nodes[g.z].load), g2.Delay(nodes[g2.z].load)), MAXIMUM(nodes[g.z].arrival, nodes[g2.z].arrival), MINIMUM(nodes[g.z].reverse - nodes[g.z].arrival, nodes[g2.z].reverse - nodes[g2.z].arrival));
                        }
                     else fprintf(fptr, "\n");
                     }
                  g.used = false;
                  g2.used = false;
                  }
               else if (!simple && g.level == l && l == 1 && g.type == E_OAI && g2.type == E_NOR && (g.a == g2.a || g.a == g2.b) && g.size == 1 && g2.size == 1)
                  {
                  if (pass == 0) {
                     gatecount++;
                     int cnt = 10;
                     cnt *= g.size;
                     tcount += cnt;
                     weighted += cnt / (1 << g.vt);
                     sprintf(modulestring, "%s #(.vt(%d))", "au_oai_nor", MINIMUM(g.vt, g2.vt));
                     fprintf(fptr, "%-30s %-10s(", modulestring, g.name);
                     sprintf(tmp, ".SN1(%s),", nodes[g.a].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".CON0(%s),", nodes[g.b].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".CON1(%s),", nodes[g.c].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".SN0(%s),", g.a == g2.a ? nodes[g2.b].name : nodes[g2.a].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".X0(%s),", nodes[g2.z].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".X1(%s));", nodes[g.z].name); fprintf(fptr, "%-18s", tmp);
                     if (nodes[g.z].load > 0 || nodes[g.z].arrival > 0) {
                        fprintf(fptr, "// load=%4.1f delay=%4.1f arrival=%4.1f slack=%4.1f \n", MAXIMUM(nodes[g.z].load, nodes[g2.z].load), MAXIMUM(g.Delay(nodes[g.z].load), g2.Delay(nodes[g2.z].load)), MAXIMUM(nodes[g.z].arrival, nodes[g2.z].arrival), MINIMUM(nodes[g.z].reverse - nodes[g.z].arrival, nodes[g2.z].reverse - nodes[g2.z].arrival));
                        }
                     else fprintf(fptr, "\n");
                     }
                  g.used = false;
                  g2.used = false;
                  }
               else if (!simple && g.level == l && l == 1 && g.type == E_AOI && g2.type == E_NAND && (g.a == g2.a || g.a == g2.b) && g.size == 1 && g2.size == 1)
                  {
                  if (pass == 0) {
                     gatecount++;
                     int cnt = 10;
                     cnt *= g.size;
                     tcount += cnt;
                     weighted += cnt / (1 << g.vt);
                     sprintf(modulestring, "%s #(.vt(%d))", "au_aoi_nand", MINIMUM(g.vt, g2.vt));
                     fprintf(fptr, "%-30s %-10s(", modulestring, g.name);
                     sprintf(tmp, ".SN1(%s),", nodes[g.a].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".CON0(%s),", nodes[g.b].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".CON1(%s),", nodes[g.c].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".SN0(%s),", g.a == g2.a ? nodes[g2.b].name : nodes[g2.a].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".X0(%s),", nodes[g2.z].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".X1(%s));", nodes[g.z].name); fprintf(fptr, "%-18s", tmp);
                     if (nodes[g.z].load > 0 || nodes[g.z].arrival > 0) {
                        fprintf(fptr, "// load=%4.1f delay=%4.1f arrival=%4.1f slack=%4.1f \n", MAXIMUM(nodes[g.z].load, nodes[g2.z].load), MAXIMUM(g.Delay(nodes[g.z].load), g2.Delay(nodes[g2.z].load)), MAXIMUM(nodes[g.z].arrival, nodes[g2.z].arrival), MINIMUM(nodes[g.z].reverse - nodes[g.z].arrival, nodes[g2.z].reverse - nodes[g2.z].arrival));
                        }
                     else fprintf(fptr, "\n");
                     }
                  g.used = false;
                  g2.used = false;
                  }
               else if (!simple && g.level == l && (g.type == E_AOI || g.type == E_OAI) && g2.type == E_INV && g.z == g2.a)
                  {
                  if (pass == 0) {
                     gatecount++;
                     int cnt = 6 * g.size + 2;
                     tcount += cnt;
                     weighted += cnt / (1 << g.vt);
                     sprintf(modulestring, "%s #(.size(%d),.vt(%d))", g.type == E_AOI ? "au_ao21_inv" : "au_oa21_inv", g.size, MINIMUM(g.vt, g2.vt));
                     fprintf(fptr, "%-30s %-10s(", modulestring, g.name);
                     sprintf(tmp, ".A1(%s),", nodes[g.a].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".A2(%s),", nodes[g.b].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".B(%s),", nodes[g.c].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".XN(%s),", nodes[g.z].name); fprintf(fptr, "%-18s", tmp);
                     sprintf(tmp, ".X(%s));", nodes[g2.z].name); fprintf(fptr, "%-18s", tmp);
                     if (nodes[g.z].load > 0 || nodes[g.z].arrival > 0) {
                        fprintf(fptr, "// load=%4.1f delay=%4.1f arrival=%4.1f slack=%4.1f \n", nodes[g.z].load, g.Delay(nodes[g.z].load), nodes[g.z].arrival, nodes[g.z].reverse - nodes[g.z].arrival);
                        }
                     else fprintf(fptr, "\n");
                     }
                  g.used = false;
                  g2.used = false;
                  }
               else if (!simple && g.level == l && (g.type == E_NOR || g.type == E_NAND) && g2.type == E_INV && g.z == g2.a)
               {
               if (pass == 0) {
                  gatecount++;
                  int cnt = 6 * g.size + 2;
                  tcount += cnt;
                  weighted += cnt / (1 << g.vt);
                  sprintf(modulestring, "%s #(.size(%d),.vt(%d))", g.type == E_NOR ? "au_or2_inv" : "au_and2_inv", g.size, MINIMUM(g.vt, g2.vt));
                  fprintf(fptr, "%-30s %-10s(", modulestring, g.name);
                  sprintf(tmp, ".A1(%s),", nodes[g.a].name); fprintf(fptr, "%-18s", tmp);
                  sprintf(tmp, ".A2(%s),", nodes[g.b].name); fprintf(fptr, "%-18s", tmp);
                  sprintf(tmp, ".XN(%s),", nodes[g.z].name); fprintf(fptr, "%-18s", tmp);
                  sprintf(tmp, ".X(%s));", nodes[g2.z].name); fprintf(fptr, "%-18s", tmp);
                  if (nodes[g.z].load > 0 || nodes[g.z].arrival > 0) {
                     fprintf(fptr, "// load=%4.1f delay=%4.1f arrival=%4.1f slack=%4.1f \n", nodes[g.z].load, g.Delay(nodes[g.z].load), nodes[g.z].arrival, nodes[g.z].reverse - nodes[g.z].arrival);
                     }
                  else fprintf(fptr, "\n");
                  }
               g.used = false;
               g2.used = false;
                  }
               }
            if (g.used && g.level == l && ((pass == 0 && g.name[3] != 'p') || (pass != 0 && g.name[3] == 'p'))) {
               gatecount++;
               if (g.type == E_XOR || g.type == E_XNR) {
                  int cnt = 10;
                  cnt *= g.size;
                  tcount += cnt;
                  weighted += cnt / (1 << g.vt);
                  }
               else {
                  int cnt = g.c >= 0 ? 6 : g.b >= 0 ? 4 : 2;
                  cnt *= g.size;
                  tcount += cnt;
                  weighted += cnt / (1 << g.vt);
                  }
               if (g.type == E_BUF || g.type == E_AND || g.type == E_OR || g.type == E_AO || g.type == E_OA) {
                  tcount += 2;
                  weighted += 2 / (1 << g.vt);
                  }
               fprintf(fptr, "%-30s %-10s(", modulestring, g.name);
               if (g.b >= 0) {
                  sprintf(tmp, ".A1(%s),", nodes[g.a].name); fprintf(fptr, "%-18s", tmp);
                  sprintf(tmp, ".A2(%s),", nodes[g.b].name); fprintf(fptr, "%-18s", tmp);
                  }
               else {
                  sprintf(tmp, ".A (%s),", nodes[g.a].name); fprintf(fptr, "%-18s%18s", tmp, "");
                  }
               if (g.c >= 0) {
                  sprintf(tmp, ".B (%s),", nodes[g.c].name); fprintf(fptr, "%-14s", tmp);
                  }
               else
                  fprintf(fptr, "%14s", "");
               sprintf(tmp, ".X(%s));", nodes[g.z].name); fprintf(fptr, "%-18s", tmp);

//               fprintf(fptr, "gate[% d] ", i);
               if (!nodes[g.z].undriven)
                  fprintf(fptr, "// %c%s%s -> %c\n", nodes[g.a].value ? '1' : '0', g.b < 0 ? " " : nodes[g.b].value ? ",1" : ",0", g.c < 0 ? " " : nodes[g.c].value ? ",1" : ",0", nodes[g.z].value ? '1' : '0');
               else if (nodes[g.z].load > 0 || nodes[g.z].arrival > 0) {
                  fprintf(fptr, "// load=%4.1f delay=%4.1f arrival=%4.1f slack=%4.1f \n", nodes[g.z].load, g.Delay(nodes[g.z].load), nodes[g.z].arrival, nodes[g.z].reverse - nodes[g.z].arrival);
                  }
               else fprintf(fptr, "\n");
               }
            }
         }
      }
   fprintf(fptr, "// %d total gates, %d total transistors, %.1f%% leakage\n",gatecount,tcount,100.0*weighted/tcount);
   fprintf(fptr, "endmodule\n");
   fclose(fptr);
   }


void Adder(bool cin, bool inn, bool onn, bool thirtyone, unsigned increment=0, bool slow=false)
   {
   int i, k, l;
   char filename[256], z[64], a[64], b[64], name[64];
   addertype adder;
   //                                       | 

   const char* fast_level1 = "  x _x x _x x _x x _x x _x x _x x _x X ";   // 15
   const char* fast_level2 = " x  _xx  _xl  _xx  _x   _x   _x   _X   ";   // 11
   const char* fast_level3 = "    _    _x   _    _x   _    _X   _    ";   // 3
   const char* fast_level4 = "    _x   _    _    _X   _x   _    _    ";   // 3
   const char* fast_level5 = "    _x   _x   _x  x_  xx_  xx_  x _    ";   // 9
   const char* fast_level6 = " xxx_ xxx_ xxx_ xx _ x  _ x  _ x x_ x  ";   // 16
   const char* fast_level7 = "    _    _    _    _    _    _    _    ";   // 57


   const char* slow_level1 = "  x _x x _x x _x x _x x _x x _x x _x x ";   // 15
   const char* slow_level2 = "    _x   _x   _x   _x   _x   _x   _x   ";   // 7
   const char* slow_level3 = "    _    _x   _    _x   _    _X   _    ";   // 3
   const char* slow_level4 = "    _x   _    _    _X   _x   _    _    ";   // 3
   const char* slow_level5 = "    _x   _x   _x  x_  xx_  xx_  x _    ";   // 9
   const char* slow_level6 = "  xx_  xx_  xx_  x _ x  _ x  _ x x_ x  ";   // 12
   const char* slow_level7 = " x  _ x  _ x  _ x  _    _    _    _    ";   // 4
                                                                          // 53

/*
   const char* level1 = "  x _x x _x x _x x _x x _x x _x x _x x ";   // 15
   const char* level2 = "    _x   _x   _x   _x   _x   _x   _x   ";   // 7
   const char* level3 = "    _    _x   _    _x   _    _x   _    ";   // 3
   const char* level4 = "    _    _x   _    _x   _x x _  x _    ";   // 5
   const char* level5 = "    _x   _x   _x x _  xx_ x x_ x x_ x  ";   // 11
   const char* level6 = "  xx_  x _  xx_ x x_ x  _    _    _    ";   // 8
   const char* level7 = " x  _ x x_ x  _    _    _    _    _    ";   // 4
                                                                     // 53
*/
   const char* chptr;
   

   adder.cin = cin;
   adder.inn = inn;
   adder.onn = onn;
   adder.thirtyone = thirtyone;
   if (increment)
      sprintf(adder.modulename, "%s_increment_%X_ipp_%s", slow ? "slow" : "fast", increment, adder.onn ? "on" : "op");
   else
      sprintf(adder.modulename, "%s_adder_%d%s_%s_%s", slow ? "slow" : "fast", adder.thirtyone ? 31 : 32, adder.cin ? "_cin" : "", adder.inn ? "inn" : "ipp", adder.onn ? "on" : "op");

   for (i = 0; i < (adder.thirtyone ? 31 : 32); i++)
      {
      sprintf(a, "a[%d]", i);
      adder.nodes.push_back(a);
      }
   for (i = 0; i < (adder.thirtyone ? 31 : 32); i++)
      {
      sprintf(b, "b[%d]", i);
      adder.nodes.push_back(b);
      }
   for (i = 0; i < (adder.thirtyone ? 31 : 32); i++)
      {
      sprintf(a, "a[%d]", i);
      sprintf(b, "b[%d]", i);
      sprintf(z, "p0_%d__%d", i,i);
      sprintf(name, "pg_genx_%d", i);
      adder.AddGate(name, i == 0 && cin ? E_XNR : E_XOR, i, z, a, b);
      sprintf(name, "pg_geng_%d", i);
      sprintf(z, "g0_%d", i);
      adder.AddGate(name, i == 0 && cin ? E_OR : E_AND, i, z, a, b);
      }

   for (l = 1; l <= 7; l++) {
      if (slow)
         chptr = l==1 ? slow_level1 : l == 2 ? slow_level2 : l == 3 ? slow_level3 : l == 4 ? slow_level4 : l == 5 ? slow_level5 : l == 6 ? slow_level6 : slow_level7;
      else
         chptr = l == 1 ? fast_level1 : l == 2 ? fast_level2 : l == 3 ? fast_level3 : l == 4 ? fast_level4 : l == 5 ? fast_level5 : l == 6 ? fast_level6 : fast_level7;
      chptr += 0;
      for (i = 32-1; i >= 0; i--)
         {
         nodetype p0, p1, p2, p3, p4, g0, g1;
         for (k = 0; k < adder.nodes.size(); k++)
            {
            const nodetype n = adder.nodes[k];
            if (n.name[0] == 'g' && n.bit == i && (g0.level < n.level))
               g0 = n;
            if (n.name[0] == 'l' && n.bit == i && (g0.level < n.level))
               g0 = n;
            if (n.name[0] == 'g' && n.bit < i && (n.bit>g1.bit || n.level > g1.level) && (n.level == l-1 || n.level>0))
               g1 = n;
            if (n.name[0] == 'p' && n.bit == i && n.level == l-1)
               p3 = n;
            if (n.name[0] == 'p' && n.bit < i && n.level == l - 1 && p4.bit<n.bit)
               p4 = n;
            }
         for (k = 0; k < adder.nodes.size(); k++)
            {
            const nodetype n = adder.nodes[k];
            if (n.name[0] == 'p' && n.bit == i && n.lowbit == g1.bit+1 && (p0.level < n.level))
               p0 = n;
            if (n.name[0] == 'p' && n.bit == i && n.lowbit > g1.bit+1)
               p1 = n;
            if (n.name[0] == 'p' && n.bit <= g0.bit && n.lowbit == g1.bit+1 && n.lowbit <= g1.bit+1 && p2.bit<n.bit)
               p2 = n;
            }
         if (*chptr == '_') chptr++;

         if ((*chptr == 'X' || *chptr == 'x') && p0.bit<0) {
            sprintf(z, "p%d_%d__%d", l - 1, i, p2.lowbit);
            sprintf(name, "U%d_p%d", l-1, p1.bit);
            adder.AddGate(name, E_AND, p1.bit, z, p1.name, p2.name);
            p0 = adder.nodes.back();
            }

         if (*chptr == 'l') {
            sprintf(z, "l%d_%d", l, i);
            sprintf(name, "U%d_g%d_%d", l, g0.bit, g1.bit);
            adder.AddGate(name, E_AO, g0.bit, z, p0.name, g1.name, g0.name);
            }
         if (*chptr == 'X' || *chptr == 'x') {
            sprintf(z, "g%d_%d", l, i);
            sprintf(name, "U%d_g%d_%d", l, g0.bit, g1.bit);
            adder.AddGate(name,E_AO, g0.bit, z, p0.name, g1.name, g0.name);
            if (*chptr == 'X') adder.gates.back().size++;

            // this may be unneeded but add it here, it can be later deleted
            if (p3.bit >= 0 && p4.bit >= 0) {
               sprintf(z, "p%d_%d__%d", l, p3.bit, p4.lowbit);
               sprintf(name, "U%d_p%d", l, p3.bit);
               adder.AddGate(name, E_AND, p3.bit,z, p3.name, p4.name);
               }
            }
         chptr++;
         }
      }

   adder.AddGate("U_s0", E_BUF, 0, "s[0]", "p0_0__0");
   for (i = 1; i < (adder.thirtyone ? 31 : 32); i++)
      {
      nodetype g0;
      for (k = 0; k < adder.nodes.size(); k++)
         {
         const nodetype n = adder.nodes[k];
         if (n.name[0] == 'g' && n.bit == i-1 && (g0.level < n.level))
            g0 = n;
         }

      sprintf(b, "p0_%d__%d", i,i);
      sprintf(z, "s[%d]", i);
      sprintf(name, "U%d_s%d", l,i);
      adder.AddGate(name, E_XOR, i, z, g0.name, b);
      }

   if (increment)
      adder.PlaceConstants(increment);
   adder.Simplify();
   adder.Demorgan();
   adder.Simplify();
   adder.LeakageRecovery();
   adder.Delay();
   for (i = 0; i < adder.nodes.size(); i++) adder.nodes[i].undriven = true;
   sprintf(filename, "z:\\jewel\\src\\rtl\\miner_custom\\adders\\%s.v", adder.modulename);
//   sprintf(filename, "%s.v", adder.modulename);
   adder.DumpToFile(filename);
   adder.Verify(0, increment ? increment : 0);

   if (adder.Verify(0, increment ? increment : 0)) exit(-1);
   for (i = 0; i < 32; i++)
      if (adder.Verify((1 << i), increment ? increment : -1)) exit(-1);
   for (i = 0; i < 32; i++)
      if (adder.Verify(1 << i, increment ? increment : -2)) exit(-1);

   return;
   for (i = 0; i < (1 << 30); i++) {
      for (k = i; k < (1 << 30); k++)
         {
         adder.Verify(i, increment ? increment : k);
         }
      }
   }


void AdderWork()
   {

//   Adder(false, true, true, true);
//   return;
   Adder(false, false, true, false, -0x98c7e2a2);        // -constant2
   Adder(false, false, false, false, -0xcd2a11ae);       // -constant1
   Adder(false, false, false, false, -0xa4ce148b,true);  // -constant4
   Adder(false, false, true, false, 0xa54ff53a, true);   // H_NOUGHT[3]+2
   Adder(false, false, false, false, 0x0c2e12e0,true);   // constant3
   Adder(false, false, true, false, -0x08909ae5, true);  // -constant5
   Adder(false, false, true, false, -0x3c6ef372, true);  // -H_NOUGHT[2]
   Adder(false, false, true, false, 0xbb67ae85, true);   // H_NOUGHT[1]
   Adder(false, false, true, false, -0x6a09e667, true);  // -H_NOUGHT[0]
   Adder(true, false, false, true);
   Adder(false, true, true, true);
   Adder(false, false, true, true);
   Adder(false, true, false, true);
   Adder(false, false, false, true);
   Adder(false, false, false, false);
   Adder(false, false, true, false);
   return;


   int cin, inn, onn, thirtyone;
   for (cin=0; cin<=1; cin++)
      for (inn = 0; inn<= 1; inn++)
         for (onn = 0; onn<= 1; onn++)
            for (thirtyone = 0; thirtyone <= 1; thirtyone++)
               Adder(cin, inn,  onn, thirtyone);
   }