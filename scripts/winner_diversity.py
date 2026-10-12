# FINDINGS.md section 106: distinct winner shapes per problem and arm in the
# section 101/103/104 joint batteries. Run from the repo root.
import glob,re,sys,collections
def prog(t):
    if not re.search(r'\] [1-9][0-9]* genuinely generalizing',t): return None
    p=t.split("fastest solution's program ===")[-1].strip()
    return p.split('\n\n')[0]
def exact(p):
    m={}
    return re.sub(r'v\d+',lambda x:m.setdefault(x.group(),'x%d'%len(m)),p)
def struct(p):
    out=[]
    for l in p.splitlines():
        s=l.strip(); ind=(len(l)-len(s))//2
        if s.startswith('while'): k='W'
        elif s.startswith('if'): k='I'
        elif s.startswith('} else'): k='E'
        elif s.startswith('swap'): k='S'
        elif s.startswith('arr['): k='A'
        elif s.startswith('}'): continue
        else: k='a'
        out.append(f'{ind}{k}')
    # collapse runs of identical scalar assigns
    r=[]; 
    for x in out:
        if not r or r[-1]!=x: r.append(x)
    return ' '.join(r)
tot={}
for d,arm in [('index_range_joint_battery_20261012','IRX'),('triple_joint_battery_20261012','TRX'),('relax_joint_battery_20261012','GRX')]:
    S=collections.defaultdict(lambda:{'base':[set(),set(),0],arm:[set(),set(),0]})
    for f in glob.glob(f'data/{d}/log_*.txt'):
        m=re.match(r'log_(base|%s)_(.+)_(\d+)\.txt'%arm,f.split('/')[-1])
        p=prog(open(f,errors='ignore').read())
        if p is None: continue
        e=S[m.group(2)][m.group(1)]; e[0].add(exact(p)); e[1].add(struct(p)); e[2]+=1
    sums=[0,0,0,0]; diff=[]
    for k,v in sorted(S.items()):
        b,a=v['base'],v[arm]
        sums[0]+=len(b[0]);sums[1]+=len(a[0]);sums[2]+=len(b[1]);sums[3]+=len(a[1])
        if len(b[1])!=len(a[1]): diff.append(f"{k}:{len(b[1])}->{len(a[1])}")
    print(f"== {d} ({arm}) problems={len(S)}")
    print(f"  exact distinct: base {sums[0]}  arm {sums[1]}")
    print(f"  structural distinct: base {sums[2]}  arm {sums[3]}")
    print("  structural diffs:", ' '.join(diff))
