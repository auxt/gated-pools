import an_v, numpy as np, math, os, glob, json
V='v/'
def get(fn):
    return an_v.load(V+fn+'.csv') if os.path.exists(V+fn+'.csv') else None
res={}
rows=[]
allerr=[]
for b,lab in [(0,'ETH'),(1,'Core')]:
    B=an_v.basket(b)
    files={(12,0):f'e1_b{b}_c0',(12,2):f'e1_b{b}_c2',(12,10):f'e1_b{b}_c10',(12,20):f'e1_b{b}_c20',(2,0):f'e1_b{b}_dt2_c0',(2,10):f'e1_b{b}_dt2_c10',(0.4,0):f'e1_b{b}_dt04_c0',(0.4,10):f'e1_b{b}_dt04_c10'}
    for (dt,c),fn in files.items():
        D=get(fn)
        if D is None: continue
        for d,a in D.items():
            if not d.startswith('P'): continue
            f=float(d[1:])*1e-4
            p=an_v.pub_pred(B,f,c*1e-4,dt)
            Ls=(a['tradeloss']-a['trk']).mean(); Lse=(a['tradeloss']-a['trk']).std(ddof=1)/math.sqrt(len(a['T']))
            Ts=a['turn'].mean()
            res[(lab,dt,c,f)]=dict(Ts=Ts,Tt=p['T'],Ls=Ls,Lse=Lse,Lt=p['L'],lnrel=-a['lnrel'].mean(),lnse=a['lnrel'].std(ddof=1)/math.sqrt(len(a['T'])))
            allerr.append((lab,dt,c,f*1e4,Ts/p['T']-1,Ls/p['L']-1))
for e in allerr: print("%s dt=%s c=%s f=%s  Terr=%+.3f Lerr=%+.3f"%e)
for lab in ['ETH','Core']:
    E=[e for e in allerr if e[0]==lab]
    print(lab,"max|Terr|",max(abs(e[4]) for e in E),"max|Lerr|",max(abs(e[5]) for e in E))
    E2=[e for e in E if e[3]>=30]; print(lab,"f>=30: max|Terr|",max(abs(e[4]) for e in E2),"max|Lerr|",max(abs(e[5]) for e in E2))
json.dump({str(k):v for k,v in res.items()},open(V+'valid.json','w'),indent=0)
# LaTeX table
sel=[('ETH',12,0,5e-4),('ETH',12,10,5e-4),('ETH',12,0,30e-4),('ETH',12,10,30e-4),('ETH',12,0,100e-4),('ETH',12,10,100e-4),('ETH',2,0,30e-4),('ETH',2,10,30e-4),('ETH',0.4,0,30e-4),('ETH',0.4,10,30e-4),
     ('Core',12,0,5e-4),('Core',12,10,5e-4),('Core',12,0,30e-4),('Core',12,10,30e-4),('Core',12,0,100e-4),('Core',12,10,100e-4),('Core',2,0,30e-4),('Core',2,10,30e-4),('Core',0.4,0,5e-4),('Core',0.4,0,30e-4)]
out=[]
for k in sel:
    if k not in res: continue
    r=res[k]; lab,dt,c,f=k
    dts=('%g'%dt)
    out.append(f"{lab} & {dts} & {f*1e4:.0f} & {c} & {r['Ts']:.2f} & {r['Tt']:.2f} & {100*r['Ls']:.3f} & {100*r['Lt']:.3f}\\\\")
open(V+'tab_valid.tex','w').write("\n".join(out)+"\n")
print("\n".join(out))
