import numpy as np, math
from scipy.stats import norm
Y=365*86400
w=np.array([.30,.25,.15,.10,.05,.15]); s=np.array([.5,.65,.8,1.0,.8,0])
C=np.array([[1,.80,.70,.50,.65],[.80,1,.75,.50,.75],[.70,.75,1,.50,.65],[.50,.50,.50,1,.45],[.65,.75,.65,.45,1]])
S=np.zeros((6,6)); S[:5,:5]=np.outer(s[:5],s[:5])*C
A=w@np.diag(S); B=w@S@w
print("B1 sum w s^2 =",A, " terms",w[:5]*s[:5]**2," wSw=",B," vol=",math.sqrt(B)," gamma*=",0.5*(A-B))
print("   ETH gamma=",.65**2/8)
gi=0.5*w[:5]*(1-w[:5])*s[:5]**2; print("B2 gamma_i",gi, gi.sum(), " seff=",(gi*s[:5]).sum()/gi.sum())
sq=math.sqrt(12/Y); print("   sqrt(dt) 12s",sq," 2s",math.sqrt(2/Y)," 0.4s",math.sqrt(0.4/Y))
for sig in [.65,.7711]:
  for dt in [12,2,.4]: print("   rho sig",sig,"dt",dt, 0.5826*sig*math.sqrt(dt/Y)*1e4,"bps")
f,c=30e-4,10e-4
hp=math.log((1+c)/(1-f)); hm=-math.log((1-f)*(1-c)); print("B3 h+",hp,"h-",hm,"h",(hp+hm)/2)
h=(hp+hm)/2
for g,sig,name in [(.65**2/8,.65,'ETH'),(0.5*(A-B),.7711,'core')]:
  rho=0.5826*sig*sq; T=g/(h+rho); L=g*(c+rho)/(h+rho)
  print(f"B4/5 {name}: rho={rho*1e4:.3f}bps h+rho={(h+rho)*1e4:.3f} T={T:.3f} L={L*100:.4f}% hedge={g*c/(h+rho)*100:.4f}% rent={g*rho/(h+rho)*100:.4f}%  limit dt->0 {g*c/h*100:.3f}%")
# Milionis Ptrade
g=.65**2/8; Pt=1/(1+f*math.sqrt(2/(12/Y))/.65); print("B6 Milionis Ptrade",Pt,"loss",g*Pt*100, " ours c=0:", g*(0.5826*.65*sq)/(f+f*f/2+0.5826*.65*sq)*100)
# gated
KE=(.25*.65)**2; print("B7 K ETH",KE," T",KE/.025," L",10e-4*KE/.025*100, " din half T",KE/.0375)
print("   core K .026 T",.026/.025," L",.026/.025*10e-4*100, " smax L", .026/.025*30e-4*100)
print("B8 E[x2] d^2/6",.025**2/6," track noise ETH", .65*.025/math.sqrt(6)," drag w=.5", -.65**2*.025**2/12*100,"%")
# ratio, s*, f*
for name,g,K,sig in [('ETH',.65**2/8,KE,.65),('core',0.5*(A-B),.026,.7711)]:
  for dt,cc in [(12,10e-4),(12,0),(0.4,0),(0.4,2e-4)]:
    rho=0.5826*sig*math.sqrt(dt/Y); hh=0.5*math.log((1+cc)/(1-cc))-math.log(1-f)
    Lp=g*(cc+rho)/(hh+rho); Lg=10e-4*K/.025; r=10e-4*K/(g*.025)
    print(f"B9 {name} dt={dt} c={cc*1e4:.0f}: Lp={Lp*100:.3f} Lg={Lg*100:.3f} ratio={Lp/Lg:.2f} Tratio={g*.025/(K*(hh+rho)):.2f} cost/$ ratio={(cc+rho)/10e-4:.3f} s*={Lp*.025/K*1e4:.1f}bps f*={(cc+rho)*(1/r-1)*100:.2f}% deq={K*(hh+rho)/(2*g)*100:.3f}%")
# impact example
NAV=10e6; Aamt=10e3; wi,wn=.10,.15
dl=Aamt/NAV*(1/wn+1/wi); print("B10 move",dl)
# exact weighted math on virtual reserves at oracle price 1
xn=wn*NAV; xi=wi*NAV; g_=1-30e-4; K_=xn**wn*xi**wi
xi2=(K_/(xn+g_*Aamt)**wn)**(1/wi); out=xi-xi2; print("   paid over",Aamt-out,"fee",Aamt*30e-4,"impact",Aamt-out-Aamt*30e-4)
# latency gaussian
for sig,L,ff in [(.8,1,10e-4),(.8,3,10e-4),(.8,10,10e-4),(.8,2,30e-4),(.8,8,30e-4),(.8,8,10e-4)]:
  sl=sig*math.sqrt(L/Y); e=2*(sl*norm.pdf(ff/sl)-ff*(1-norm.cdf(ff/sl)))
  print(f"B11 sig={sig} L={L}s f={ff*1e4:.0f}: sigmaL={sl*1e4:.2f}bps leak/window per $ = {e:.3e}, per yr at q=0.1%/12s: {e*0.001*Y/12*100:.4f}%")
# jump leak rough
print("B11 jumps: market",26,"x tokens; idio",12*5)
# informed flow
for sig,H,lab in [(.75,1/8760,'1h'),(.75,1/365,'1d'),(1.0,1/365,'1d HYPE')]:
  e=sig*math.sqrt(H)*math.sqrt(2/math.pi); print(f"B12 {lab}: E|r|={e*100:.2f}% phi*={(30e-4+17e-4)/e:.2f}")
print("B13 worst", 30e-4*.2*100,"% per day; capacity per $100k", 100e3/(7*30e-4))
