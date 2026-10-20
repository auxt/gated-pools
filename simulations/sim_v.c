// sim_v.c -- validation and stress-test simulator for Gated Pools v0.3
// Designs evaluated on common random numbers (same price path, same retail stream):
//   P(f)        public weighted (G3M) pool, fee f; arbitrageurs pay external cost c per dollar
//   G(d,din)    gated vault, no retail: rebalanced to drift din*d when drift D > d, spread s on notional
//   A0          gated vault + oracle-anchored quote layer (band aband, reset to centre), retail only
//   Ak          same + informed latency trader k (oracle lag, pull-oracle price selection, per-window cap)
// Benchmark: constant-weight portfolio rebalanced every block at true prices for free (CRP).
// Output: one CSV line per path and design.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#define MAXA 6
#define MAXD 40
#define HIST 128

static uint64_t S4[4];
static inline uint64_t rotl(const uint64_t x,int k){return (x<<k)|(x>>(64-k));}
static uint64_t nxt(void){const uint64_t r=rotl(S4[1]*5,7)*9;const uint64_t t=S4[1]<<17;S4[2]^=S4[0];S4[3]^=S4[1];S4[1]^=S4[2];S4[0]^=S4[3];S4[2]^=t;S4[3]=rotl(S4[3],45);return r;}
static double unif(void){return ((nxt()>>11)+0.5)*(1.0/9007199254740992.0);}
static int has=0; static double spare;
static double gauss(void){ if(has){has=0;return spare;} double u=unif(),v=unif(); double r=sqrt(-2*log(u)); spare=r*sin(2*M_PI*v); has=1; return r*cos(2*M_PI*v);}

static int NA,NR,NI;            // assets, risky assets, index of USDC (=NA-1)
static double w[MAXA], vol0[MAXA], Lch[MAXA][MAXA], Cr[MAXA][MAXA];

static double val(const double*x,const double*p){double V=0;for(int i=0;i<NA;i++)V+=x[i]*p[i];return V;}

// Public arbitrage of token i against USDC on a G3M with fee; arbitrageur hedges at p(1-c) / p(1+c).
// Trades to the edge of the no-arbitrage band. Returns notional at true price.
static double pub_arb(double*x,int i,double pi,double fee,double c){
  double g=1.0-fee, wi=w[i], wn=w[NI];
  double P=(x[NI]/x[i])*(wi/wn); double K=pow(x[NI],wn)*pow(x[i],wi);
  double pb=pi*(1-c), ps=pi*(1+c);
  if(P/g < pb*(1-1e-12)){ double r=pb*g*wn/wi; double xi=pow(K/pow(r,wn),1.0/(wi+wn)); double xneff=r*xi; double dn=(xneff-x[NI])/g; double no=(x[i]-xi)*pi; x[NI]+=dn; x[i]=xi; return no; }
  if(P*g > ps*(1+1e-12)){ double r=ps*wn/(g*wi); double xieff=pow(K/pow(r,wn),1.0/(wi+wn)); double xn=r*xieff; double di=(xieff-x[i])/g; x[i]+=di; x[NI]=xn; return di*pi; }
  return 0;
}
// Retail trade on a G3M curve (token i vs USDC). dir=+1 buys token i with A USDC, -1 sells token worth A.
// Returns what the trader overpays relative to true value (fee + impact), in USDC.
static double curve_trade(double*x,int i,int dir,double A,double pi,double fee){
  double wi=w[i], wn=w[NI]; double g=1-fee; double K=pow(x[NI],wn)*pow(x[i],wi);
  if(dir>0){ double xi=pow(K/pow(x[NI]+g*A,wn),1.0/wi); double out=x[i]-xi; x[NI]+=A; x[i]=xi; return A-out*pi; }
  else { double Q=A/pi; double xn=pow(K/pow(x[i]+g*Q,wi),1.0/wn); double out=x[NI]-xn; x[i]+=Q; x[NI]=xn; return A-out; }
}
// Informed trader on the quote curve (virtual reserves v): trades token i toward true price pt, capped at cap (USDC).
// Returns LP loss at true prices; *no = notional (USDC).
static double informed(double*v,int i,double pt,double fee,double cap,double*no){
  double g=1-fee, wi=w[i], wn=w[NI];
  double P=(v[NI]/v[i])*(wi/wn); double K=pow(v[NI],wn)*pow(v[i],wi);
  double vi0=v[i], vn0=v[NI];
  *no=0; if(cap<=0) return 0;
  if(P/g < pt*(1-1e-12)){
    double r=pt*g*wn/wi; double xi=pow(K/pow(r,wn),1.0/(wi+wn)); double xneff=r*xi; double dn=(xneff-v[NI])/g;
    if(dn>cap){ dn=cap; xi=pow(K/pow(v[NI]+g*dn,wn),1.0/wi); }
    v[NI]+=dn; v[i]=xi;
  } else if(P*g > pt*(1+1e-12)){
    double r=pt*wn/(g*wi); double xieff=pow(K/pow(r,wn),1.0/(wi+wn)); double xn=r*xieff; double di=(xieff-v[i])/g;
    if(di*pt>cap){ di=cap/pt; xn=pow(K/pow(v[i]+g*di,wi),1.0/wn); }
    v[i]+=di; v[NI]=xn;
  } else return 0;
  double dvi=v[i]-vi0, dvn=v[NI]-vn0; *no=fabs(dvn);
  return -(dvi*pt+dvn);
}

typedef struct { int type; double fee, d, din; int lag, sel; double q; char name[40]; } Design;
// type 0 public, 1 gated vault, 2 anchored (honest), 3 anchored with informed trader

static void setup_basket(int b){
  if(b==0){ NA=2; NR=1; w[0]=.5; w[1]=.5; vol0[0]=.65; vol0[1]=0; Lch[0][0]=1; Cr[0][0]=1; }
  else {
    NA=6; NR=5; double ww[6]={.30,.25,.15,.10,.05,.15}; double vv[5]={.50,.65,.80,1.00,.80};
    double C[5][5]={{1,.80,.70,.50,.65},{.80,1,.75,.50,.75},{.70,.75,1,.50,.65},{.50,.50,.50,1,.45},{.65,.75,.65,.45,1}};
    for(int i=0;i<6;i++) w[i]=ww[i]; for(int i=0;i<5;i++) vol0[i]=vv[i]; vol0[5]=0;
    memset(Lch,0,sizeof Lch); for(int i=0;i<5;i++) for(int j=0;j<5;j++) Cr[i][j]=C[i][j];
    for(int i=0;i<5;i++) for(int j=0;j<=i;j++){ double sum=C[i][j]; for(int k=0;k<j;k++) sum-=Lch[i][k]*Lch[j][k]; if(i==j) Lch[i][j]=sqrt(sum); else Lch[i][j]=sum/Lch[j][j]; }
  }
  NI=NA-1;
}

int main(int argc,char**argv){
  int basket=1, paths=10, jumps=0, regime=0, drift=0, rsel=0; uint64_t seed=1;
  double days=365, dt_s=12, volx=1, c=10e-4, s=10e-4, retail=0, msize=1e-4, qfee=30e-4, musd=0.6, kappa=2.0, aband=0.025;
  Design D[MAXD]; int nd=0;
  double fees[8]; int nf=0; double bands[8]; int nb=0; double dins[4]; int ndi=0;
  int alag[6], asel[6]; double aq[6]; int nat=0; int anch=0;
  for(int a=1;a<argc;a++){
    char k[64]; char v[256]; if(sscanf(argv[a],"%63[^=]=%255s",k,v)!=2) continue;
    if(!strcmp(k,"basket")) basket=atoi(v);
    else if(!strcmp(k,"paths")) paths=atoi(v);
    else if(!strcmp(k,"days")) days=atof(v);
    else if(!strcmp(k,"dt")) dt_s=atof(v);
    else if(!strcmp(k,"seed")) seed=strtoull(v,0,10);
    else if(!strcmp(k,"jumps")) jumps=atoi(v);
    else if(!strcmp(k,"regime")) regime=atoi(v);
    else if(!strcmp(k,"drift")) drift=atoi(v);
    else if(!strcmp(k,"musd")) musd=atof(v);
    else if(!strcmp(k,"kappa")) kappa=atof(v);
    else if(!strcmp(k,"volx")) volx=atof(v);
    else if(!strcmp(k,"c")) c=atof(v)*1e-4;
    else if(!strcmp(k,"s")) s=atof(v)*1e-4;
    else if(!strcmp(k,"retail")) retail=atof(v);
    else if(!strcmp(k,"msize")) msize=atof(v);
    else if(!strcmp(k,"qfee")) qfee=atof(v)*1e-4;
    else if(!strcmp(k,"aband")) aband=atof(v)*1e-2;
    else if(!strcmp(k,"anch")) anch=atoi(v);
    else if(!strcmp(k,"rsel")) rsel=atoi(v);
    else if(!strcmp(k,"fees")){ char*t=strtok(v,","); while(t){ fees[nf++]=atof(t)*1e-4; t=strtok(0,","); } }
    else if(!strcmp(k,"bands")){ char*t=strtok(v,","); while(t){ bands[nb++]=atof(t)*1e-2; t=strtok(0,","); } }
    else if(!strcmp(k,"dins")){ char*t=strtok(v,","); while(t){ dins[ndi++]=atof(t); t=strtok(0,","); } }
    else if(!strcmp(k,"atk")){ int l,se; double q; if(sscanf(v,"%d,%d,%lf",&l,&se,&q)==3){ alag[nat]=l; asel[nat]=se; aq[nat]=q*1e-2; nat++; } }
  }
  setup_basket(basket);
  if(ndi==0){ dins[0]=0; ndi=1; }
  for(int k=0;k<nf;k++){ Design d={0}; d.type=0; d.fee=fees[k]; snprintf(d.name,40,"P%g",fees[k]*1e4); D[nd++]=d; }
  for(int k=0;k<nb;k++) for(int j=0;j<ndi;j++){ Design d={0}; d.type=1; d.d=bands[k]; d.din=dins[j]; snprintf(d.name,40,"G%g_%g",bands[k]*100,dins[j]); D[nd++]=d; }
  if(anch||nat>0){ Design d={0}; d.type=2; d.fee=qfee; d.d=aband; snprintf(d.name,40,"A0"); D[nd++]=d; }
  for(int k=0;k<nat;k++){ Design d={0}; d.type=3; d.fee=qfee; d.d=aband; d.lag=alag[k]; d.sel=asel[k]; d.q=aq[k]; snprintf(d.name,40,"A%d_L%d_S%d_q%g",k+1,alag[k],asel[k],aq[k]*100); D[nd++]=d; }

  S4[0]=seed*0x9E3779B97F4A7C15ULL+1; S4[1]=seed^0xD1B54A32D192ED03ULL; S4[2]=0x8CB92BA72F3D8DD7ULL+seed; S4[3]=0xABCDEF1234567ULL^(seed<<7);
  for(int i=0;i<40;i++) nxt();

  double year=365.0*86400.0, dt=dt_s/year; long nsteps=(long)(days*86400.0/dt_s); double T=nsteps*dt;
  long hourly=(long)(3600.0/dt_s); if(hourly<1) hourly=1;
  double lamC=26.0, lamI=12.0;
  double sigS=1.0, muS=log(msize)-0.5*sigS*sigS, ptrade=(retail/msize)*dt;
  // regime: high-vol state mult 2*mL, low mL, with E[mult^2]=1; P(high)=0.2, mean high spell 7 days
  double mL=1.0/sqrt(0.8+0.2*4.0), lamHL=365.0/7.0, lamLH=lamHL*0.25;
  double rw=0; for(int i=0;i<NR;i++) rw+=w[i];

  printf("path,design,T,lnrel,turn,tradeloss,retail,latloss,meanD,meanX2,nreb,lnhodl,trk\n");
  static double hist[HIST][MAXA];
  for(int pth=0;pth<paths;pth++){
    double p[MAXA]; for(int i=0;i<NA;i++) p[i]=1.0;
    double lnCRP=0, x[MAXD][MAXA];
    for(int d=0;d<nd;d++) for(int i=0;i<NA;i++) x[d][i]=w[i];
    double trk[MAXD]={0}, turn[MAXD]={0}, tl[MAXD]={0}, ret[MAXD]={0}, lat[MAXD]={0}, sD[MAXD]={0}, sX2[MAXD]={0}, budget[MAXD]={0}; long nreb[MAXD]={0}; long nsamp=0;
    double mu[MAXA]={0}; if(drift==1) for(int i=0;i<NR;i++) mu[i]=musd*gauss();
    double y[MAXA]={0}; // log price for OU
    int hi=0;
    for(int h=0;h<HIST;h++) for(int i=0;i<NA;i++) hist[h][i]=1.0;
    for(long t=0;t<nsteps;t++){
      double mult=1.0;
      if(regime){ if(hi){ if(unif()<lamHL*dt) hi=0; } else { if(unif()<lamLH*dt) hi=1; } mult=hi?2*mL:mL; }
      { // control variate: expected log-return gap vs CRP over this step given current weights
        double Sg[MAXA][MAXA], m[MAXA];
        for(int i=0;i<NR;i++){ double bi=vol0[i]*volx/0.65; m[i]=(drift==1)?mu[i]:((drift==2)?-kappa*y[i]:0);
          for(int j=0;j<NR;j++){ double bj=vol0[j]*volx/0.65; Sg[i][j]=mult*mult*vol0[i]*vol0[j]*volx*volx*Cr[i][j];
            if(jumps){ Sg[i][j]+=lamC*(0.04*bi)*(0.04*bj); if(i==j) Sg[i][j]+=lamI*(0.06*volx)*(0.06*volx); } } }
        for(int d=0;d<nd;d++){ double V=val(x[d],p), om[MAXA]; for(int i=0;i<NR;i++) om[i]=x[d][i]*p[i]/V;
          double q=0, lin=0; for(int i=0;i<NR;i++){ lin+=(om[i]-w[i])*m[i]; for(int j=0;j<NR;j++) q+=om[i]*om[j]*Sg[i][j]-w[i]*w[j]*Sg[i][j]; }
          trk[d]+=(lin-0.5*q)*dt; }
      }
      double z[MAXA],e[MAXA];
      for(int i=0;i<NR;i++) z[i]=gauss();
      int cj=0; double cjz=0; if(jumps && unif()<lamC*dt){ cj=1; cjz=0.04*gauss(); }
      double gross=0;
      for(int i=0;i<NR;i++){
        e[i]=0; for(int k=0;k<=i;k++) e[i]+=Lch[i][k]*z[k];
        double sg=vol0[i]*volx*mult;
        double lr=-0.5*sg*sg*dt + sg*sqrt(dt)*e[i];
        if(drift==1) lr+=mu[i]*dt;
        if(drift==2){ double ynew=y[i]-kappa*y[i]*dt+lr; lr=ynew-y[i]; y[i]=ynew; }
        if(jumps){ double beta=vol0[i]*volx/0.65; double sc=0.04*beta, si=0.06*volx;
          lr -= (lamC*(exp(0.5*sc*sc)-1) + lamI*(exp(0.5*si*si)-1))*dt;
          if(cj) lr += beta*cjz;
          if(unif()<lamI*dt) lr += si*gauss(); }
        double r=exp(lr); gross+=w[i]*r; p[i]*=r;
      }
      gross+=w[NI]; lnCRP+=log(gross);
      for(int i=0;i<NA;i++) hist[t%HIST][i]=p[i];
      // retail draw (common to all designs)
      int rt=0, ti=0, dir=0; double u=0;
      if(retail>0 && unif()<ptrade){ rt=1; u=exp(muS+sigS*gauss()); double rr=unif()*rw, acc=0; ti=NR-1; for(int i=0;i<NR;i++){acc+=w[i]; if(rr<=acc){ti=i;break;}} dir=(unif()<0.5)?1:-1; }
      for(int d=0;d<nd;d++){
        double*xx=x[d];
        if(D[d].type==0){
          double Vb=val(xx,p), no=0;
          for(int pass=0;pass<2;pass++) for(int i=0;i<NR;i++) no+=fabs(pub_arb(xx,i,p[i],D[d].fee,c));
          double Va=val(xx,p); tl[d]+=(Vb-Va)/Vb; turn[d]+=no/Vb;
          if(rt){ double V=val(xx,p); ret[d]+=curve_trade(xx,ti,dir,u*V,p[ti],D[d].fee)/V; }
        } else {
          if(D[d].type==3){ // informed latency trader on the anchored quote, before retail
            budget[d]+=D[d].q*dt_s/12.0; if(budget[d]>D[d].q) budget[d]=D[d].q;
            double V=val(xx,p);
            for(int i=0;i<NR && budget[d]>1e-15;i++){
              // oracle centre available to the trader: lagged price, or best of a selection window
              double o;
              long l0=t-D[d].lag; if(l0<0) l0=0;
              if(D[d].sel>0){ double lo=1e300, hi2=-1e300; for(int j=0;j<=D[d].sel;j++){ long tt=l0-j; if(tt<0) tt=0; double h=hist[tt%HIST][i]; if(h<lo)lo=h; if(h>hi2)hi2=h; }
                o=(log(p[i]/lo)>log(hi2/p[i]))?lo:hi2; }
              else o=hist[l0%HIST][i];
              double v[MAXA]; for(int k=0;k<NA;k++) v[k]=w[k]*V/((k==i)?o:p[k]);
              double before_i=v[i], before_n=v[NI], no=0;
              double loss=informed(v,i,p[i],D[d].fee,budget[d]*V,&no);
              if(no>0 && loss>0){ xx[i]+=v[i]-before_i; xx[NI]+=v[NI]-before_n; lat[d]+=loss/V; budget[d]-=no/V; }
            }
          }
          if(rt && D[d].type>=2){ double V=val(xx,p); double v[MAXA]; for(int k=0;k<NA;k++) v[k]=w[k]*V/p[k];
            double bi=v[ti], bn=v[NI]; double ov=curve_trade(v,ti,dir,u*V,p[ti],D[d].fee);
            xx[ti]+=v[ti]-bi; xx[NI]+=v[NI]-bn; ret[d]+=ov/V; }
          // band rebalancing through the gate
          double V=val(xx,p), Dr=0; for(int i=0;i<NA;i++) Dr+=fabs(xx[i]*p[i]/V-w[i]); Dr*=0.5;
          if(Dr>D[d].d){
            double din=D[d].din*D[d].d; double lam=din/Dr; double tgt[MAXA], notional=Dr-din, wedge=0;
            for(int i=0;i<NA;i++){ double om=xx[i]*p[i]/V; tgt[i]=w[i]+lam*(om-w[i]); }
            if(rsel>0) for(int i=0;i<NR;i++){ double dv=(tgt[i]-xx[i]*p[i]/V); // >0 pool buys token i
                double lo=1e300, hi2=-1e300; for(int j=0;j<=rsel;j++){ long tt=t-j; if(tt<0) tt=0; double h=hist[tt%HIST][i]; if(h<lo)lo=h; if(h>hi2)hi2=h; }
                wedge+= (dv>0)? dv*(hi2/p[i]-1) : (-dv)*(1-lo/p[i]); }
            double cost=s*notional+wedge; double Vn=V*(1-cost);
            for(int i=0;i<NA;i++) xx[i]=tgt[i]*Vn/p[i];
            turn[d]+=notional; tl[d]+=cost; nreb[d]++;
          }
        }
      }
      if(t%hourly==0){ nsamp++; for(int d=0;d<nd;d++){ double V=val(x[d],p), Dr=0, X2=0; for(int i=0;i<NA;i++){ double e2=x[d][i]*p[i]/V-w[i]; Dr+=fabs(e2); X2+=e2*e2; } sD[d]+=0.5*Dr; sX2[d]+=X2; } }
    }
    double hodl=0; for(int i=0;i<NA;i++) hodl+=w[i]*p[i];
    for(int d=0;d<nd;d++){
      printf("%d,%s,%.6f,%.8f,%.6f,%.8f,%.8f,%.8f,%.6f,%.8f,%ld,%.8f,%.8f\n",pth,D[d].name,T,log(val(x[d],p))-lnCRP,turn[d],tl[d],ret[d],lat[d],sD[d]/nsamp,sX2[d]/nsamp,nreb[d],log(hodl)-lnCRP,trk[d]);
    }
    fflush(stdout);
  }
  return 0;
}
