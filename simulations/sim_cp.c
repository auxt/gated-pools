// Constant-product comparison on the hub basket (each risky token paired 50/50 with USDC).
// Target weights: risky_i = 0.5*w_core_i/0.85, USDC = 0.5 -> the same exposure for every design.
// Designs:
//  0 Public weighted 6-token pool, fee 30 bps (one curve over the whole basket)
//  1 Public CP pairs (token_i/USDC, 50/50), fee 30 bps
//  2 Public CP pairs, fee 5 bps
//  3 Gated weighted vault + anchored quote layer (band on basket drift, spread s)
//  4 Gated CP pairs: each pair anchored on the oracle, rebalanced to 50/50 inside the pair (band on pair drift)
//  5 Gated CP pairs + cross-pair rebalancing (moves value between pairs at the basket band; equivalent to a vault split into pairs)
// Every public arb pays external cost c; every gated rebalance pays spread s. Retail is uninformed, same stream for all designs.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#define NA 6
#define NR 5
#define ND 6
static uint64_t s4[4];
static inline uint64_t rotl(const uint64_t x,int k){return (x<<k)|(x>>(64-k));}
static uint64_t nxt(void){const uint64_t r=rotl(s4[1]*5,7)*9;const uint64_t t=s4[1]<<17;s4[2]^=s4[0];s4[3]^=s4[1];s4[1]^=s4[2];s4[0]^=s4[3];s4[2]^=t;s4[3]=rotl(s4[3],45);return r;}
static double unif(){return ((nxt()>>11)+0.5)*(1.0/9007199254740992.0);}
static int has=0; static double sp;
static double gauss(){ if(has){has=0;return sp;} double u=unif(),v=unif(); double r=sqrt(-2*log(u)); sp=r*sin(2*M_PI*v); has=1; return r*cos(2*M_PI*v);}
static double w[NA];
static double val(double*x,double*p){double V=0;for(int i=0;i<NA;i++)V+=x[i]*p[i];return V;}
// generic two-asset weighted curve: reserves *xi (token, weight wi) and *xn (USDC, weight wn)
static double arb2(double*xi,double*xn,double wi,double wn,double pi,double fee,double c){
  double g=1.0-fee;
  double P=(*xn/ *xi)*(wi/wn); double K=pow(*xn,wn)*pow(*xi,wi);
  double pb=pi*(1-c), ps=pi*(1+c);
  if(P/g < pb*(1-1e-12)){ double r=pb*g*wn/wi; double x_i=pow(K/pow(r,wn),1.0/(wi+wn)); double xneff=r*x_i; double dn=(xneff-*xn)/g; double no=(*xi-x_i)*pi; *xn+=dn; *xi=x_i; return no; }
  if(P*g > ps*(1+1e-12)){ double r=ps*wn/(g*wi); double xieff=pow(K/pow(r,wn),1.0/(wi+wn)); double x_n=r*xieff; double di=(xieff-*xi)/g; *xi+=di; *xn=x_n; return di*pi; }
  return 0;
}
// retail on a two-asset curve; returns overpayment (fee+impact) in USDC relative to oracle value
static double retail2(double*xi,double*xn,double wi,double wn,int dir,double A,double pi,double fee){
  double g=1-fee; double K=pow(*xn,wn)*pow(*xi,wi);
  if(dir>0){ double x_i=pow(K/pow(*xn+g*A,wn),1.0/wi); double out=*xi-x_i; *xn+=A; *xi=x_i; return A-out*pi; }
  else { double Q=A/pi; double x_n=pow(K/pow(*xi+g*Q,wi),1.0/wn); double out=*xn-x_n; *xi+=Q; *xn=x_n; return A-out; }
}
int main(int argc,char**argv){
  int npaths=atoi(argv[1]); double dt_s=atof(argv[2]); double days=atof(argv[3]); uint64_t seed=strtoull(argv[4],0,10);
  int jumps=atoi(argv[5]); double volx=atof(argv[6]);
  double meanSize=(argc>7)?atof(argv[7]):1e-4;
  double c=1e-3, sCL=1e-3, delta=0.025, fq=3e-3;
  double wcore[NA]={.30,.25,.15,.10,.05,.15};
  for(int i=0;i<NR;i++) w[i]=0.5*wcore[i]/0.85; w[5]=0.5;
  s4[0]=seed*0x9E3779B97F4A7C15ULL+1; s4[1]=seed^0xD1B54A32D192ED03ULL; s4[2]=0x8CB92BA72F3D8DD7ULL+seed; s4[3]=0xABCDEF1234567ULL^(seed<<7);
  for(int i=0;i<20;i++) nxt();
  double vol[NR]={.50,.65,.80,1.00,.80};
  double C[NR][NR]={{1,.80,.70,.50,.65},{.80,1,.75,.50,.75},{.70,.75,1,.50,.65},{.50,.50,.50,1,.45},{.65,.75,.65,.45,1}};
  double L[NR][NR]; memset(L,0,sizeof L);
  for(int i=0;i<NR;i++) for(int j=0;j<=i;j++){ double sum=C[i][j]; for(int k=0;k<j;k++) sum-=L[i][k]*L[j][k]; if(i==j) L[i][j]=sqrt(sum); else L[i][j]=sum/L[j][j]; }
  double lamC=26.0, lamI=12.0;
  double year=365.0*86400.0; double dt=dt_s/year; long nsteps=(long)(days*86400.0/dt_s); double T=nsteps*dt;
  double sigS=1.0; double muS=log(meanSize)-0.5*sigS*sigS;
  double ptrade=(volx/meanSize)*dt;
  double rw=0; for(int i=0;i<NR;i++) rw+=w[i];
  long sampleEvery=(long)(3600.0/dt_s); if(sampleEvery<1) sampleEvery=1;
  printf("path,T,lnCRP,rvol");
  for(int d=0;d<ND;d++) printf(",ln%d,ret%d,cost%d,turn%d,drift%d",d,d,d,d,d);
  printf("\n");
  for(int pth=0;pth<npaths;pth++){
    double p[NA]; for(int i=0;i<NA;i++) p[i]=1.0;
    double lnCRP=0;
    // weighted designs (0,3): holdings x[d][i]; pair designs (1,2,4,5): token qty a[d][i], usdc q[d][i] per pair
    double x[ND][NA]; double a[ND][NR], q[ND][NR];
    for(int d=0;d<ND;d++){ for(int i=0;i<NA;i++) x[d][i]=w[i]; for(int i=0;i<NR;i++){ a[d][i]=w[i]; q[d][i]=w[i]; } }
    double ret[ND],cost[ND],turn[ND],drift[ND]; memset(ret,0,sizeof ret); memset(cost,0,sizeof cost); memset(turn,0,sizeof turn); memset(drift,0,sizeof drift);
    long nsamp=0; double rvol=0;
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
      // design 0: public weighted pool, arbs per token against USDC (two passes)
      { double Vb=val(x[0],p); double no=0;
        for(int pass=0;pass<2;pass++) for(int i=0;i<NR;i++) no+=arb2(&x[0][i],&x[0][5],w[i],w[5],p[i],3e-3,c);
        double Va=val(x[0],p); cost[0]+=(Vb-Va)/Vb; turn[0]+=no/Vb; }
      // designs 1,2: public CP pairs
      for(int d=1;d<=2;d++){ double fee=(d==1)?3e-3:5e-4; double Vb=0,Va=0,no=0;
        for(int i=0;i<NR;i++) Vb+=a[d][i]*p[i]+q[d][i];
        for(int i=0;i<NR;i++) no+=arb2(&a[d][i],&q[d][i],0.5,0.5,p[i],fee,c);
        for(int i=0;i<NR;i++) Va+=a[d][i]*p[i]+q[d][i];
        cost[d]+=(Vb-Va)/Vb; turn[d]+=no/Vb; }
      // retail
      if(volx>0 && unif()<ptrade){
        double u=exp(muS+sigS*gauss()); double rr=unif()*rw, acc=0; int ti=0; for(int i=0;i<NR;i++){acc+=w[i]; if(rr<=acc){ti=i;break;}}
        int dir=(unif()<0.5)?1:-1; rvol+=u;
        // 0: public weighted
        { double V=val(x[0],p); double A=u*V; double ov=retail2(&x[0][ti],&x[0][5],w[ti],w[5],dir,A,p[ti],3e-3); ret[0]+=ov/V; }
        // 1,2: public CP pair ti
        for(int d=1;d<=2;d++){ double fee=(d==1)?3e-3:5e-4; double V=0; for(int i=0;i<NR;i++) V+=a[d][i]*p[i]+q[d][i]; double A=u*V;
          double ov=retail2(&a[d][ti],&q[d][ti],0.5,0.5,dir,A,p[ti],fee); ret[d]+=ov/V; }
        // 3: gated weighted vault, anchored quote on virtual reserves w*V
        { double *h=x[3]; double V=val(h,p); double A=u*V; double vi=w[ti]*V/p[ti], vn=w[5]*V; double bi=vi,bn=vn;
          double ov=retail2(&vi,&vn,w[ti],w[5],dir,A,p[ti],fq); h[ti]+=vi-bi; h[5]+=vn-bn; ret[3]+=ov/V; }
        // 4,5: gated CP pairs, anchored quote on the pair's own value
        for(int d=4;d<=5;d++){ double V=0; for(int i=0;i<NR;i++) V+=a[d][i]*p[i]+q[d][i]; double A=u*V;
          double Vi=a[d][ti]*p[ti]+q[d][ti]; double vi=0.5*Vi/p[ti], vn=0.5*Vi; double bi=vi,bn=vn;
          double ov=retail2(&vi,&vn,0.5,0.5,dir,A,p[ti],fq); a[d][ti]+=vi-bi; q[d][ti]+=vn-bn; ret[d]+=ov/V; }
      }
      // 3: gated vault band rebalancing on basket drift
      { double V=val(x[3],p); double D=0; for(int i=0;i<NA;i++) D+=fabs(x[3][i]*p[i]/V-w[i]); D*=0.5;
        if(D>delta){ turn[3]+=D; cost[3]+=sCL*D; double Vn=V*(1-sCL*D); for(int i=0;i<NA;i++) x[3][i]=w[i]*Vn/p[i]; } }
      // 4: gated CP pairs, each pair rebalanced to 50/50 when its own drift exceeds the band
      for(int d=4;d<=5;d++){ double V=0; for(int i=0;i<NR;i++) V+=a[d][i]*p[i]+q[d][i];
        for(int i=0;i<NR;i++){ double Vi=a[d][i]*p[i]+q[d][i]; double om=a[d][i]*p[i]/Vi; double Dp=fabs(om-0.5);
          if(Dp>delta){ double no=Dp*Vi; turn[d]+=no/V; cost[d]+=sCL*no/V; double Vin=Vi-sCL*no; a[d][i]=0.5*Vin/p[i]; q[d][i]=0.5*Vin; } }
        if(d==5){ // cross-pair: when basket drift exceeds band, resize pairs back to target at spread s
          V=0; for(int i=0;i<NR;i++) V+=a[d][i]*p[i]+q[d][i]; double D=0;
          for(int i=0;i<NR;i++){ D+=fabs(a[d][i]*p[i]/V-w[i]); }
          double us=0; for(int i=0;i<NR;i++) us+=q[d][i]; D+=fabs(us/V-w[5]); D*=0.5;
          if(D>delta){ turn[d]+=D; cost[d]+=sCL*D; double Vn=V*(1-sCL*D); for(int i=0;i<NR;i++){ a[d][i]=w[i]*Vn/p[i]; q[d][i]=w[i]*Vn; } } }
      }
      if(t%sampleEvery==0){ nsamp++;
        for(int d=0;d<ND;d++){ double D=0;
          if(d==0||d==3){ double V=val(x[d],p); for(int i=0;i<NA;i++) D+=fabs(x[d][i]*p[i]/V-w[i]); }
          else { double V=0,us=0; for(int i=0;i<NR;i++){ V+=a[d][i]*p[i]+q[d][i]; us+=q[d][i]; } for(int i=0;i<NR;i++) D+=fabs(a[d][i]*p[i]/V-w[i]); D+=fabs(us/V-w[5]); }
          drift[d]+=0.5*D; } }
    }
    printf("%d,%.6f,%.8f,%.8f",pth,T,lnCRP,rvol);
    for(int d=0;d<ND;d++){ double Vend;
      if(d==0||d==3) Vend=val(x[d],p); else { Vend=0; for(int i=0;i<NR;i++) Vend+=a[d][i]*p[i]+q[d][i]; }
      printf(",%.8f,%.8f,%.8f,%.6f,%.6f",log(Vend),ret[d],cost[d],turn[d],drift[d]/nsamp); }
    printf("\n"); fflush(stdout);
  }
  return 0;
}
