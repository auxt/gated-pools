import an_v, numpy as np, math
from scipy import integrate
def pool(files):
    out={}
    for fn in files:
        D=an_v.load('v/'+fn+'.csv')
        for d,a in D.items():
            if not d.startswith('G'): continue
            if d not in out: out[d]={k:list(v) for k,v in a.items()}
            else:
                for k in a: out[d][k]+=list(a[k])
    return {d:{k:np.array(v) for k,v in a.items()} for d,a in out.items()}
def ex2(delta,din):
    # occupation density of BM started at +-din killed at +-delta (symmetric), Green's function on (-d,d)
    d=delta
    G=lambda a,x: (d-max(a,x))*(d+min(a,x))/(2*d)
    num=integrate.quad(lambda x: x*x*0.5*(G(din,x)+G(-din,x)),-d,d,points=[-din,din])[0]
    den=integrate.quad(lambda x: 0.5*(G(din,x)+G(-din,x)),-d,d,points=[-din,din])[0]
    return num/den
lines=[]
for b,lab,files,bands in [(0,'ETH',['e1_b0_c0','e1_b0_c2','e1_b0_c10','e1_b0_c20'],[0.5,1,2.5,5]),(1,'Core',['e1_b1_c0','e1_b1_c2','e1_b1_c10','e1_b1_c20'],[1,2.5,5])]:
    B=an_v.basket(b); K=B['K']
    P=pool(files)
    for dl in bands:
        for di in [0,0.5]:
            name=f"G{dl:g}_{di:g}"; a=P[name]; n=len(a['T'])
            Tt=K/(dl/100*(1+di)); Ts=a['turn'].mean()
            L=(a['tradeloss']-a['trk']); Lm=L.mean(); Le=L.std(ddof=1)/math.sqrt(n)
            sd=a['lnrel'].std(ddof=1)
            sdt=(0.65*math.sqrt(ex2(dl/100,di*dl/100)) if b==0 else float('nan'))
            print(f"{lab} d={dl} din={di} n={n} T={Ts:.3f} th={Tt:.3f} err={Ts/Tt-1:+.3f} L={100*Lm:.3f}±{100*Le:.3f} trade={100*a['tradeloss'].mean():.3f} trk={100*a['trk'].mean():+.4f} nreb={a['nreb'].mean():.0f} D={100*a['meanD'].mean():.2f} sd={100*sd:.2f} sdth={100*sdt:.2f}")
            dis='0' if di==0 else '$\\delta/2$'
            lines.append(f"{lab} & {dl:g} & {dis} & {Ts:.2f} & {Tt:.2f} & {100*a['tradeloss'].mean():.3f} & {a['nreb'].mean():.0f} & {100*sd:.2f}"+(f" ({100*sdt:.2f})" if b==0 else "")+"\\\\")
open('v/tab_gate.tex','w').write("\n".join(lines)+"\n")
print("\n".join(lines))
