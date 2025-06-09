#include <stdio.h>
#include <math.h>

#define max_cache 64
struct cachetype
   {
   double w;
   double s;
   } cache[max_cache];
int cache_top;


double fastsin(double w)
   {
   int i;
   double s;
   char neg = 0;

   if (w<0.0)
      {
      neg = 1;
      w = -w;
      }

   for (i=0; i<cache_top; i++)
      if (w==cache[i].w)
         return neg ? -cache[i].s : cache[i].s;
   s = sin(w);
   if (cache_top<max_cache)
      {
      cache[cache_top].w = w;
      cache[cache_top].s = s;
      cache_top++;
      printf("top=%d w=%.3f\n",cache_top,w);
      }
   return neg ? -s : s;
   }

