import an_v,numpy as np
rows={}
for fn,col in [('e5_r0',0),('e5_r5',1),('e5_r20',2),('e5_r5j',3)]:
    D=an_v.load('v/'+fn+'.csv')
    for d,a in D.items():
        net=a['retail']-(a['tradeloss']-a['trk'])
        rows.setdefault(d,[None]*4)[col]=(100*net.mean(),100*net.std(ddof=1)/np.sqrt(len(net)))
        if fn=='e5_r5': print(d,'retail paid',100*a['retail'].mean(),'turn',a['turn'].mean())
for d,v in rows.items(): print(d,[('%.2f±%.3f'%x if x else None) for x in v])
names={'P10':'Public pool, 10\\bps','P30':'Public pool, 30\\bps','G2.5_0':'Gated vault, no quote layer','A0':'Gated pool, anchored quotes, 30\\bps'}
fmt=lambda x:('$%+.2f$'%x[0]).replace('+-','-')
L=[]
for d in ['P10','P30']: L.append(names[d]+' & '+' & '.join(fmt(x) for x in rows[d])+'\\\\')
L.append('Single-arbitrageur pool, fresh & $-5.10$ & $-3.08$ & $+2.98$ & $-3.19$\\\\')
L.append('Single-arbitrageur pool, lazy & $-1.63$ & $+0.57$ & $+7.18$ & $+0.48$\\\\')
for d in ['G2.5_0','A0']: L.append(names[d]+' & '+' & '.join(fmt(x) for x in rows[d])+'\\\\')
print("\n".join(L)); open('v/tab_retail.tex','w').write("\n".join(L)+"\n")
