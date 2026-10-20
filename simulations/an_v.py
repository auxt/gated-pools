import csv, sys, glob, os, math, collections
import numpy as np
BETA=0.5826
YEAR=365*86400
def load(fn):
    rows=collections.defaultdict(list)
    for r in csv.DictReader(open(fn)):
        rows[r['design']].append(r)
    out={}
    for d,rs in rows.items():
        a={k:np.array([float(r[k]) for r in rs]) for k in rs[0] if k not in('design',)}
        for k in ('lnrel','turn','tradeloss','retail','latloss','trk','lnhodl'): a[k]=a[k]/a['T']
        a['nreb']=a['nreb']/a['T']
        out[d]=a
    return out
def ms(x): 
    x=np.asarray(x); return x.mean(), x.std(ddof=1)/math.sqrt(len(x)) if len(x)>1 else 0
def basket(b):
    if b==0:
        w=np.array([.5,.5]); s=np.array([.65,0]); C=np.array([[1.]])
    else:
        w=np.array([.30,.25,.15,.10,.05,.15]); s=np.array([.5,.65,.8,1.0,.8,0])
        C=np.array([[1,.80,.70,.50,.65],[.80,1,.75,.50,.75],[.70,.75,1,.50,.65],[.50,.50,.50,1,.45],[.65,.75,.65,.45,1]])
    n=len(w); S=np.zeros((n,n)); S[:n-1,:n-1]=np.outer(s[:n-1],s[:n-1])*C
    g=0.5*(w@np.diag(S)-w@S@w)
    gi=w[:n-1]*(1-w[:n-1])*s[:n-1]**2
    seff=(gi*s[:n-1]).sum()/gi.sum()
    K=2*w[0]*(1-w[0])*g if b==0 else 0.0260
    return dict(w=w,S=S,gamma=g,seff=seff,K=K)
def hband(f,c):
    return 0.5*(-math.log((1-f)*(1-c)) + math.log((1+c)/(1-f)))
def pub_pred(B,f,c,dt,volx=1.0):
    g=B['gamma']*volx**2; rho=BETA*B['seff']*volx*math.sqrt(dt/YEAR); h=hband(f,c)
    return dict(T=g/(h+rho), L=g*(c+rho)/(h+rho), rent=g*rho/(h+rho), rho=rho, h=h)
if __name__=='__main__':
    for fn in sys.argv[1:]:
        print('==',fn)
        D=load(fn)
        for d,a in D.items():
            m=lambda k: ms(a[k])
            l,le=m('lnrel'); t,te=m('turn'); tl,tle=m('tradeloss'); tr,tre=m('trk'); ret,rete=m('retail'); la,lae=m('latloss')
            cv=a['tradeloss']-a['trk']
            cvm,cve=ms(cv)
            print(f"{d:16s} lnrel {100*l:7.3f}±{100*le:.3f}  E[loss] {100*cvm:7.3f}±{100*cve:.3f}  turn {t:7.3f}±{te:.3f}  trade {100*tl:6.3f}  trk {100*tr:7.4f}  retail {100*ret:6.3f}  lat {100*la:6.3f}  D {a['meanD'].mean()*100:.3f}% X2 {a['meanX2'].mean():.2e} nreb {a['nreb'].mean():.1f} hodl {100*a['lnhodl'].mean():.2f}")
