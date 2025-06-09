#include "pch.h"
#include "template.h"
#include "bdd.h"

#ifdef _MSC_VER
//  #pragma optimize("",off)
#endif

int bddtype::NewNode(int var, int low, int high)
   {
   int index;
   nodes.push(bddnodetype(var, low, high));
   index = nodes.size()-1;
   return index;
   }

void bddtype::Invert()
   {
   int i;

   for (i=2; i<nodes.size(); i++)
	  {
	  if (nodes[i].low ==1) nodes[i].low  = 0;
	  else if (nodes[i].low ==0) nodes[i].low  = 1;
	  if (nodes[i].high==1) nodes[i].high = 0;
	  else if (nodes[i].high==0) nodes[i].high = 1;
	  }
   if (root==1) root = 0;
   else if (root==0) root = 1;
   }

void bddtype::Print(const char *txt) const
   {
   int i;

   printf("%s",txt);

   printf("root    %4d\n",root);
   for (i=0; i<nodes.size(); i++)
	  {
	  const bddnodetype &n = nodes[i];
	  printf("%3d %6d  %4d %4d\n",i,n.var,n.low,n.high);
	  }
   }


void bddtype::EnumerateInputs(arraytype <int> &inputs) const
   {
   int i;
   inputs.clear();

   for (i=2; i<nodes.size(); i++)
	  {
	  const bddnodetype &n = nodes[i];

	  if (inputs.size()==0 || inputs.back()!=n.var)
	     inputs.push(n.var);
	  }
   inputs.RemoveDuplicates();
   }


void bddtype::Apply(const bddtype &_fbdd, const bddtype &_gbdd, bddoptype op)
   {
   TAtype ta("bddtype::Apply");
   if (&_fbdd==this || &_gbdd==this)
	  {
	  bddtype temp;
	  temp.Apply(_fbdd, _gbdd, op);
	  *this = temp;
	  return;
	  }

   fbdd = &_fbdd;
   gbdd = &_gbdd;

   root = ApplyStep(fbdd->root, gbdd->root, op);
   fghash.clear();
   resulthash.clear();
   fbdd = NULL;
   gbdd = NULL;
//   printf("%d nodes\n",nodes.size());
   }


// this recursive function will return the root node for the operation
int bddtype::ApplyStep(int f, int g, bddoptype op)
   {
   const bddnodetype &ff = fbdd->nodes[f];
   const bddnodetype &gg = gbdd->nodes[g];
   int low, high, var, index;

   if (op==BDD_AND)
      {
      if (f==0 || g==0) return 0;
      if (f==1 && g==1) return 1;
      }
   else if (op==BDD_OR)
      {
      if (f==1 || g==1) return 1;
      if (f==0 && g==0) return 0;
      }
   else if (op==BDD_ANDB)	 // bubble 2nd input(g)
      {
      if (f==0 || g==1) return 0;
      if (f==1 && g==0) return 1;
      }
   else if (op==BDD_ORB)	 // bubble 2nd input(g)
      {
      if (f==1 || g==0) return 1;
      if (f==0 && g==1) return 0;
      }
   else if (op==BDD_XOR)
      {
      if (f<=1 && g<=1) return f^g;
      }
   else FATAL_ERROR;

   if (fghash.Check(bddhashkeytype(f, g, -1), index))
      return index;
   
   if (ff.var == gg.var)
      {
      var  = ff.var;
      low  = ApplyStep(ff.low,  gg.low, op);
      high = ApplyStep(ff.high, gg.high, op);
      }
   else if (ff.var < gg.var)
      {
      var  = ff.var;
      low  = ApplyStep(ff.low,  g, op);
      high = ApplyStep(ff.high, g, op);
      }
   else 
      {
      var  = gg.var;
      low  = ApplyStep(f, gg.low, op);
      high = ApplyStep(f, gg.high, op);
      }
   if (low==high) 
      index = low;
   else   
      {
      if (!resulthash.Check(bddhashkeytype(low, high, var), index))
         {
         index = NewNode(var, low, high);
         resulthash.Add(bddhashkeytype(low, high, var), index);
         }
      }
   
   fghash.Add(bddhashkeytype(f, g, -1), index);
   return index;
   }


void bddtype::Reduction(const bddtype &_fbdd, bool low_high, int var)
   {
   TAtype ta("bddtype::Reduction");
   fbdd = &_fbdd;

   resulthash.clear();
   root = ReductionStep(fbdd->root, low_high, var);
   fghash.clear();
   resulthash.clear();
   fbdd = NULL;
   }

int bddtype::ReductionStep(int findex, bool low_high, int var)
   {
   const bddnodetype &n = fbdd->nodes[findex];
   int low=-1, high=-1, index=-1;

   if (findex<=1) return findex;
   if (n.var == var)
	  {
	  return ReductionStep(low_high ? n.high : n.low, low_high, var);
	  }
   
   if (fghash.Check(bddhashkeytype(findex, -1, -1), index))
      return index;

   low  = ReductionStep(n.low,  low_high, var);
   high = ReductionStep(n.high, low_high, var);

   if (low==high) 
      index = low;
   else   
      {
      if (!resulthash.Check(bddhashkeytype(low, high, n.var), index))
         {
         index = NewNode(n.var, low, high);
         resulthash.Add(bddhashkeytype(low, high, n.var), index);
         }
      }
   fghash.Add(bddhashkeytype(findex, -1, -1), index);
   
   return index;
   }


bool bddtype::Evaluate(__int64 data)
   {
   int index = root;

   while (index>1)
      {
      bddnodetype &n = nodes[index];

      if ((data>>n.var) &1)
         index = n.high;
      else
         index = n.low;
      }
   return index==1;
   }


// build a 8b adder

void BddTest()
   {
   const int bits=15;
   bddtype sum[bits], carry[bits+1], cin, a[bits], b[bits];
   int i;


   carry[0].ForceToPrimitive(0);
   for (i=0; i<bits; i++)
      {
      a[i].ForceToPrimitive(i*2+1);
      b[i].ForceToPrimitive(i*2+2);
      }

   for (i=0; i<bits; i++)
      {
      bddtype temp, temp2, temp3, temp4;
      temp.Apply(a[i], b[i], BDD_XOR);
      sum[i].Apply(temp, carry[i], BDD_XOR);

      temp.Apply(a[i],      b[i],     BDD_AND);
      temp2.Apply(b[i],     carry[i], BDD_AND);
      temp3.Apply(carry[i], a[i],     BDD_AND);
      temp4.Apply(temp, temp2, BDD_OR);
      carry[i+1].Apply(temp4, temp3, BDD_OR);

      printf("Sum[%d]   = %d nodes.\n",i,sum[i].Size());
      printf("Carry[%d] = %d nodes.\n",i,carry[i].Size());
      }

   // now exhaustively verify the adder
   int aa, bb;
   for (aa=0; aa<(1<<bits); aa++)
      for (bb=0; bb<(1<<bits); bb++)
         {
         __int64 s = aa+bb;
         __int64 data=0;
         for (i=0; i<bits; i++)
            {
            data |= ((aa>>i)&1)<<(i*2+1);
            data |= ((bb>>i)&1)<<(i*2+2);
            }


         if ((s>>(bits-1))&1)
            {
            if (!sum[bits-1].Evaluate(data))
               FATAL_ERROR;
            }
         else
            {
            if (sum[bits-1].Evaluate(data))
               FATAL_ERROR;
            }
         }
   }
