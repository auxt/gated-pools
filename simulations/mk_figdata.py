# Write pgfplots data files for Figures 2 and 3 (theory curves and simulation points)
import an_v, numpy as np, json, os
out='../paper/fig/'
res={eval(k):v for k,v in json.load(open('v/valid.json')).items()}
fs=np.logspace(0,2,61)*1e-4
for b,lab in [(0,'eth'),(1,'core')]:
    B=an_v.basket(b)
    with open(out+f'law_{lab}.dat','w') as fh:
        fh.write('f c0 c2 c10 c20\n')
        for f in fs:
            fh.write(f"{f*1e4:.4f} "+" ".join(f"{an_v.pub_pred(B,f,c*1e-4,12)['L']*100:.5f}" for c in [0,2,10,20])+"\n")
    for c in [0,2,10,20]:
        pts=sorted((k[3],v['Ls']) for k,v in res.items() if k[0]==('ETH' if b==0 else 'Core') and k[1]==12 and k[2]==c)
        with open(out+f'sim_{lab}_c{c}.dat','w') as fh:
            fh.write('f L\n')
            for f,L in pts: fh.write(f"{f*1e4:.1f} {L*100:.5f}\n")
    print(lab,'gamma',B['gamma'],'gated10',10e-4*B['K']/0.025*100,'gated30',30e-4*B['K']/0.025*100)
B=an_v.basket(1); K=B['K']
cs=np.linspace(0,20,81)*1e-4
with open(out+'breakeven.dat','w') as fh:
    fh.write('c f30dt12 f30dt2 f30dt04 f5dt12 f5dt2 f5dt04\n')
    for c in cs:
        vals=[an_v.pub_pred(B,f,c,dt)['L']*0.025/K*1e4 for f in [30e-4,5e-4] for dt in [12,2,0.4]]
        fh.write(f"{c*1e4:.2f} "+" ".join(f"{v:.3f}" for v in vals)+"\n")
print('ok')
