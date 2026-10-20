// Design comparison with retail flow:
// D0 Public G3M f=10bps, D1 Public G3M f=30bps (public arbs pay external cost c)
// D2 SAP-fresh: G3M curve f=30bps for retail, exclusive wallet syncs token when |gap|>g1, pays exec cost c, remits profit
// D3 SAP-lazy: same, g2
// D4 CLIP: band delta, spread s, no retail
// D5 Hybrid: CLIP + retail at oracle +/- fv
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#define NA 6
#define NR 5
#define ND 7
static uint64_t s[4];
static inline uint64_t rotl(const uint64_t x,int k){return (x<<k)|(x>>(64-k));}
static uint64_t nxt(void){const uint64_t r=rotl(s[1]*5,7)*9;const uint64_t t=s[1]<<17;s[2]^=s[0];s[3]^=s[1];s[1]^=s[2];s[0]^=s[3];s[2]^=t;s[3]=rotl(s[3],45);return r;}
static double unif(){return ((nxt()>>11)+0.5)*(1.0/9007199254740992.0);}
static int has=0; static double sp;
static double gauss(){ if(has){has=0;return sp;} double u=unif(),v=unif(); double r=sqrt(-2*log(u)); sp=r*sin(2*M_PI*v); has=1; return r*cos(2*M_PI*v);}
static double w[NA]={.30,.25,.15,.10,.05,.15};
static double val(double*x,double*p){double V=0;for(int i=0;i<NA;i++)V+=x[i]*p[i];return V;}
// public arb on token i vs USDC with fee f and external cost c; returns notional traded
static double pub_arb(double*xx,int i,double pi,double fee,double c){
  double g=1.0-fee, wi=w[i], wn=w[5];
  double P=(xx[5]/xx[i])*(wi/wn); double K=pow(xx[5],wn)*pow(xx[i],wi);
  double pb=pi*(1-c), ps=pi*(1+c);
  if(P/g < pb*(1-1e-12)){ double r=pb*g*wn/wi; double xi=pow(K/pow(r,wn),1.0/(wi+wn)); double xneff=r*xi; double dn=(xneff-xx[5])/g; double no=(xx[i]-xi)*pi; xx[5]+=dn; xx[i]=xi; return no; }
  if(P*g > ps*(1+1e-12)){ double r=ps*wn/(g*wi); double xieff=pow(K/pow(r,wn),1.0/(wi+wn)); double xn=r*xieff; double di=(xieff-xx[i])/g; xx[i]+=di; xx[5]=xn; return di*pi; }
  return 0;
}
// SAP wallet sync of token i to exact price, exec cost c, remit; returns notional
static double sap_sync(double*xx,int i,double*p,double gth,double c,double*costacc){
  double wi=w[i], wn=w[5]; double pi=p[i];
  double P=(xx[5]/xx[i])*(wi/wn);
  if(fabs(log(P/pi))<=gth) return 0;
  double K=pow(xx[5],wn)*pow(xx[i],wi);
  double r=pi*wn/wi; double xi=pow(K/pow(r,wn),1.0/(wi+wn)); double xn=r*xi;
  double Vb=val(xx,p);
  double notional=fabs(xi-xx[i])*pi;
  double pi_profit=(xx[i]*pi+xx[5])-(xi*pi+xn);
  xx[i]=xi; xx[5]=xn;
  double Va=val(xx,p);
  double cost=c*notional; *costacc+=cost/Vb;
  double sc=(Va+pi_profit-cost)/Va; for(int k=0;k<NA;k++) xx[k]*=sc;
  return notional;
}
// retail on curve: dir=+1 buy token i with A usdc; -1 sell token worth A
static double retail_curve(double*xx,int i,int dir,double A,double*p,double fee){
  double wi=w[i], wn=w[5]; double g=1-fee; double K=pow(xx[5],wn)*pow(xx[i],wi); double over;
  if(dir>0){ double xi=pow(K/pow(xx[5]+g*A,wn),1.0/wi); double out=xx[i]-xi; xx[5]+=A; xx[i]=xi; over=A-out*p[i]; }
  else { double Q=A/p[i]; double xn=pow(K/pow(xx[i]+g*Q,wi),1.0/wn); double out=xx[5]-xn; xx[i]+=Q; xx[5]=xn; over=A-out; }
  return over;
}
int main(int argc,char**argv){
  int npaths=atoi(argv[1]); double dt_s=atof(argv[2]); double days=atof(argv[3]); uint64_t seed=strtoull(argv[4],0,10);
  int jumps=atoi(argv[5]); double volx=atof(argv[6]);
  double c=1e-3, sCL=1e-3, delta=0.025, fv=3e-3, g1=3e-3, g2=1e-2;
  double fees[2]={1e-3,3e-3};
  s[0]=seed*0x9E3779B97F4A7C15ULL+1; s[1]=seed^0xD1B54A32D192ED03ULL; s[2]=0x8CB92BA72F3D8DD7ULL+seed; s[3]=0xABCDEF1234567ULL^(seed<<7);
  for(int i=0;i<20;i++) nxt();
  double vol[NR]={.50,.65,.80,1.00,.80};
  double C[NR][NR]={{1,.80,.70,.50,.65},{.80,1,.75,.50,.75},{.70,.75,1,.50,.65},{.50,.50,.50,1,.45},{.65,.75,.65,.45,1}};
  double L[NR][NR]; memset(L,0,sizeof L);
  for(int i=0;i<NR;i++) for(int j=0;j<=i;j++){ double sum=C[i][j]; for(int k=0;k<j;k++) sum-=L[i][k]*L[j][k]; if(i==j) L[i][j]=sqrt(sum); else L[i][j]=sum/L[j][j]; }
  double lamC=26.0, lamI=12.0;
  double year=365.0*86400.0; double dt=dt_s/year; long nsteps=(long)(days*86400.0/dt_s); double T=nsteps*dt;
  double meanSize=(argc>7)?atof(argv[7]):1e-4, sigS=1.0; double muS=log(meanSize)-0.5*sigS*sigS;
  double ptrade=(volx/meanSize)*dt;
  double rw=0; for(int i=0;i<NR;i++) rw+=w[i];
  printf("path,T,lnCRP");
  for(int d=0;d<ND;d++) printf(",ln%d,ret%d,cost%d,turn%d",d,d,d,d);
  printf("\n");
  for(int pth=0;pth<npaths;pth++){
    double p[NA]; for(int i=0;i<NA;i++) p[i]=1.0;
    double lnCRP=0; double x[ND][NA];
    for(int d=0;d<ND;d++) for(int i=0;i<NA;i++) x[d][i]=w[i];
    double ret[ND],cost[ND],turn[ND]; memset(ret,0,sizeof ret); memset(cost,0,sizeof cost); memset(turn,0,sizeof turn);
    for(long t=0;t<nsteps;t++){
      double z[NR],e[NR];
      for(int i=0;i<NR;i++) z[i]=gauss();
      int cj=0; double cjz=0; if(jumps && unif()<lamC*dt){ cj=1; cjz=0.04*gauss(); }
      double gross=0;
      for(int i=0;i<NR;i++){ e[i]=0; for(int k=0;k<=i;k++) e[i]+=L[i][k]*z[k];
        double lr=-0.5*vol[i]*vol[i]*dt + vol[i]*sqrt(dt)*e[i];
        if(jumps){ double beta=vol[i]/0.65; double sc=0.04*beta, si=0.06;
          lr -= (lamC*(exp(0.5*sc*sc)-1) + lamI*(exp(0.5*si*si)-1))*dt;
          if(cj) lr += beta*cjz;
          if(unif()<lamI*dt) lr += si*gauss(); }
        double r=exp(lr); gross+=w[i]*r; p[i]*=r; }
      gross+=w[5]; lnCRP+=log(gross);
      // public pools: arbs first
      for(int d=0;d<2;d++){ double Vb=val(x[d],p); double no=0;
        for(int pass=0;pass<2;pass++) for(int i=0;i<NR;i++) no+=pub_arb(x[d],i,p[i],fees[d],c);
        double Va=val(x[d],p); cost[d]+=(Vb-Va)/Vb; turn[d]+=no/Vb; }
      // SAP wallets
      for(int d=2;d<4;d++){ double gth=(d==2)?g1:g2; double Vb=val(x[d],p); double no=0; double dummy=0;
        for(int i=0;i<NR;i++) no+=sap_sync(x[d],i,p,gth,c,&dummy);
        double Va=val(x[d],p); cost[d]+=(Vb-Va)/Vb; turn[d]+=no/Vb; }
      // retail
      if(volx>0 && unif()<ptrade){
        double u=exp(muS+sigS*gauss()); double rr=unif()*rw, acc=0; int ti=0; for(int i=0;i<NR;i++){acc+=w[i]; if(rr<=acc){ti=i;break;}}
        int dir=(unif()<0.5)?1:-1;
        for(int d=0;d<4;d++){ double V=val(x[d],p); double fee=(d==0)?fees[0]:fees[1]; double ov=retail_curve(x[d],ti,dir,u*V,p,fee); ret[d]+=ov/V; }
        { double *h=x[5]; double V=val(h,p); double A=u*V;
          if(dir>0){ h[5]+=A; h[ti]-=A*(1-fv)/p[ti]; } else { h[ti]+=A/p[ti]; h[5]-=A*(1-fv); }
          ret[5]+=fv*A/V; }
        { double *h=x[6]; double V=val(h,p); double A=u*V; double v[NA]; for(int k=0;k<NA;k++) v[k]=w[k]*V/p[k];
          double before_i=v[ti], before_n=v[5]; double ov=retail_curve(v,ti,dir,A,p,fees[1]);
          h[ti]+=v[ti]-before_i; h[5]+=v[5]-before_n; ret[6]+=ov/V; }
      }
      // CLIP and hybrid band rebalancing
      for(int d=4;d<7;d++){ double V=val(x[d],p); double D=0; for(int i=0;i<NA;i++) D+=fabs(x[d][i]*p[i]/V-w[i]); D*=0.5;
        if(D>delta){ turn[d]+=D; cost[d]+=sCL*D; double Vn=V*(1-sCL*D); for(int i=0;i<NA;i++) x[d][i]=w[i]*Vn/p[i]; } }
    }
    printf("%d,%.6f,%.8f",pth,T,lnCRP);
    for(int d=0;d<ND;d++) printf(",%.8f,%.8f,%.8f,%.6f",log(val(x[d],p)),ret[d],cost[d],turn[d]);
    printf("\n"); fflush(stdout);
  }
  return 0;
}
