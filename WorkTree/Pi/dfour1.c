#include <math.h>
#define SWAP(a,b) tempr=(a);(a)=(b);(b)=tempr

double fastsin(double w);


void dfour1(double data[], unsigned long nn, int isign)
   {
   unsigned long n,mmax,m,j,istep,i;
   double wtemp,wr,wpr,wpi,wi,theta;
   double tempr,tempi;

   n=nn << 1;
   j=1;
   for (i=1;i<n;i+=2) {
      if (j > i) {
         SWAP(data[j],data[i]);
         SWAP(data[j+1],data[i+1]);
         }                 
      m=n >> 1;
      while (m >= 2 && j > m) {
         j -= m;
         m >>= 1;
         }
      j += m;
      }
   mmax=2;
   while (n > mmax) {
      istep=mmax << 1;
      theta=isign*(6.28318530717959/mmax);
      wtemp=fastsin(0.5*theta);
      wpr = -2.0*wtemp*wtemp;
      wpi=fastsin(theta);
      //        printf("%.2f=sin(%.2f)\n",wpi,theta);
      wr=1.0;
      wi=0.0;
      for (m=1;m<mmax;m+=2) {
         for (i=m;i<=n;i+=istep) 
            {
            double j0, j1;
            j=i+mmax;
            j0=data[j], j1=data[j+1];
            tempr=wr*j0 - wi*j1;
            tempi=wr*j1 + wi*j0;
            data[j]=data[i]-tempr;
            data[j+1]=data[i+1]-tempi;
            data[i] += tempr;
            data[i+1] += tempi;
            }
         wr=(wtemp=wr)*wpr-wi*wpi+wr;
         wi=wi*wpr+wtemp*wpi+wi;
         }
      mmax=istep;
      }
   }
#undef SWAP
