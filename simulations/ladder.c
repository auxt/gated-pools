#include <stdio.h>
#include <math.h>
#include <stdint.h>
static uint64_t s=88172645463325252ULL; static double u(){s^=s<<13;s^=s>>7;s^=s<<17;return ((s>>11)+0.5)/9007199254740992.0;}
static double g(){return sqrt(-2*log(u()))*cos(2*M_PI*u());}
int main(){ double s1=0,s2=0; long n=0; for(int k=0;k<200000;k++){ double x=0,m=0; for(int t=0;t<1000;t++){ x+=g(); if(x>m){ if(m>0||1){double H=x-m; s1+=H; s2+=H*H; n++;} m=x; } } }
 double r=s2/(2*s1); printf("n=%ld E[H]=%.4f ratio=%.4f\n",n,s1/n,r); }
