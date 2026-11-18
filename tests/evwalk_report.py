# evwalk_report.py - summary of tests/run_evwalk.sh: per generation and walk variant, how many Pokemon legal before the
# walk are illegal after it (PKHeX), with the reasons. Usage: python3 -I tests/evwalk_report.py ~/wbx-evwalk
import sys, re, glob, collections
E = sys.argv[1]
res, why = {}, collections.defaultdict(list)
for f in glob.glob(E + '.g*.txt'):
    cur = None
    for line in open(f, encoding='utf-8'):
        m = re.match(r'(OK|ILLEGAL)\s+(\S+): (.*)', line)
        if m:
            cur = m.group(2); res[cur] = (m.group(1) == 'OK', m.group(3))
        elif cur and line.strip().startswith('Invalid'):
            why[cur].append(line.strip())
base = {k[:-len('_base.pkN')]: v for k, v in res.items() if '_base.' in k}
tot = collections.Counter(); bad = collections.Counter(); reasons = collections.Counter(); examples = collections.defaultdict(list)
for k, (ok, info) in res.items():
    if '_base.' in k: continue
    p = re.match(r'(s\d+_p\d+)_([a-z]+)_((?:cap)?x?\d+)\.pk(\d)', k)
    if not p: continue
    key, stat, var, gen = p.groups()
    if key not in base or not base[key][0]: continue   # only Pokemon legal before the walk
    tot[(gen, var)] += 1
    if not ok:
        bad[(gen, var)] += 1
        for r in why[k]: reasons[(gen, re.sub(r'\d+', 'N', r))] += 1
        if len(examples[gen]) < 6: examples[gen].append((k, info, why[k]))
legal_base = collections.Counter(); all_base = collections.Counter()
for k, (ok, info) in base.items():
    g = [r for r in res if r.startswith(k + '_base')][0][-1]
    all_base[g] += 1; legal_base[g] += ok
print('Pokemon legal before the walk, per gen:', {g: '%d of %d' % (legal_base[g], all_base[g]) for g in sorted(all_base)})
for gen in sorted({g for g, _ in tot}):
    print('== Gen', gen)
    for var in ['500', '2000', '5000', '20000', 'x1', 'x2', 'x6', 'cap5000', 'cap20000', 'capx1', 'capx2', 'capx6']:
        if tot[(gen, var)]: print('   %-6s illegal after walk: %d of %d' % (var, bad[(gen, var)], tot[(gen, var)]))
    for (g, r), c in reasons.most_common():
        if g == gen: print('   reason x%d: %s' % (c, r))
    for e in examples[gen]: print('   e.g.', e[0], e[1], e[2][:1])
