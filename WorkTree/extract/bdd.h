#ifndef __BDD_H_INCLUDED__
#define __BDD_H_INCLUDED__
#include "template.h"



enum bddoptype {BDD_AND, BDD_OR, BDD_ANDB, BDD_ORB, BDD_XOR};



class bddnodetype{
public:
   int var;
   int low, high;   

   bddnodetype(int _var, int _low, int _high) : var(_var), low(_low),  high(_high)
      {
      }
   bddnodetype()
      {}
   bool operator==(const bddnodetype &right) const
      {
      if (var != right.var) return false;
      if (var < 0) return true;
      if (low  != right.low) return false;
      if (high != right.high) return false;
      return true;
      }
   };

struct bddhashkeytype{
   int a, b, var;
   operator __int64()
      {
      return ((__int64)a<<0) ^ ((__int64)b<<24) ^ ((__int64)var<<48) ^ ((__int64)var>>16);
      }
   bddhashkeytype(int _a, int _b, int _var) : a(_a), b(_b), var(_var)
      {}
   bddhashkeytype()
      {
      a=b==var-1;
      }
   };

class bddtype{
   arraytype <bddnodetype> nodes;
   int root;

   const bddtype *fbdd, *gbdd;  // needed for apply
   hashtype <bddhashkeytype, int> fghash, resulthash;
   
   int NewNode(int var, int low, int high);
   int ApplyStep(int f_index, int g_index, bddoptype op);
   int ReductionStep(int index, bool low_high, int var);

public:
   bddtype()
      {
      nodes.push(bddnodetype(MAX_INT, -1, -1));
      nodes.push(bddnodetype(MAX_INT, -1, -1));
      root = 0;
	  fbdd = gbdd = NULL;
      }
   ~bddtype()
	  {
	  if (fbdd!=NULL || gbdd!=NULL) FATAL_ERROR;
	  }
   int Size()
      {
      return nodes.size();
      }
   void ForceToPrimitive(int var)
      {
      if (var<0 || var==MAX_INT) FATAL_ERROR;
      root = NewNode(var, 0, 1);
      }
   void ForceToConstant(bool value)
      {
      root= value ? 1 : 0;
      }
   bool IsOne()
	  {
	  return root==1;
	  }
   bool IsZero()
	  {
	  return root==0;
	  }
   void Print(const char *txt) const;
   void Apply(const bddtype &fbdd, const bddtype &gbdd, bddoptype op);
   void Invert();
   void Reduction(const bddtype &_fbdd, bool low_high, int var);
   void EnumerateInputs(arraytype <int> &inputs) const;
   bool Evaluate(__int64 data);
   };




#endif
