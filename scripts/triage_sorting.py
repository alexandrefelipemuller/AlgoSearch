#!/usr/bin/env python3
"""Triage of a long sorting run's archive (solutions.json).

Re-executes every archived program OUTSIDE the engine (a Python port of
Interpreter.cpp's semantics: C truncating / and %, 200-deep call(), v0 = n,
v1/v2 seeded to 0/n when the run used --recursion-call-bias > 0) and reports:

  * generalization: random inputs far beyond the trained n (duplicates,
    negatives, sorted, reversed, n up to 256), all must sort;
  * cost: interpreter steps at n = 32/64/128/256 and the fitted exponent;
  * behavior class: hash of the full sequence of array/aux mutations on a
    fixed input set -- two programs in the same class perform the SAME
    swaps in the SAME order, i.e. are one algorithm modulo dead code and
    renaming. The smallest program per class is its representative.

Usage: scripts/triage_sorting.py RUN_DIR [--seed-recursion | --no-seed-recursion]
Writes RUN_DIR/triage.json and RUN_DIR/triage.md.
"""
import hashlib, json, math, multiprocessing, os, random, re, sys

sys.setrecursionlimit(100000)

class Abort(Exception):
    pass

# ---------------------------------------------------------------- parsing

TOK = re.compile(r'\s*(-?\d+|v\d+|arr|aux|call|swap|while|if|else|return|break|continue|==|!=|<=|>=|<<|>>|[-+*/%&|^<>()\[\]{};=,])')

def tokenize(src):
    out, pos = [], 0
    src = src.rstrip()
    while pos < len(src):
        m = TOK.match(src, pos)
        if not m:
            raise ValueError('bad token at %r' % src[pos:pos + 20])
        out.append(m.group(1))
        pos = m.end()
    return out

class Parser:
    def __init__(self, toks):
        self.t, self.i = toks, 0
    def peek(self):
        return self.t[self.i] if self.i < len(self.t) else None
    def eat(self, x=None):
        tok = self.t[self.i]
        if x is not None and tok != x:
            raise ValueError('expected %s got %s' % (x, tok))
        self.i += 1
        return tok
    def expr(self):
        tok = self.peek()
        if tok == '(':
            self.eat('(')
            a = self.expr()
            op = self.eat()
            b = self.expr()
            self.eat(')')
            return ('bin', op, a, b)
        if tok in ('arr', 'aux'):
            self.eat()
            self.eat('[')
            e = self.expr()
            self.eat(']')
            return (tok, e)
        if tok.startswith('v'):
            self.eat()
            return ('var', int(tok[1:]))
        self.eat()
        return ('const', int(tok))
    def block(self):
        self.eat('{')
        body = []
        while self.peek() != '}':
            body.append(self.stmt())
        self.eat('}')
        return body
    def stmt(self):
        tok = self.peek()
        if tok == 'if':
            self.eat(); self.eat('(')
            c = self.expr()
            self.eat(')')
            body = self.block()
            els = []
            if self.peek() == 'else':
                self.eat()
                els = self.block()
            return ('if', c, body, els)
        if tok == 'while':
            self.eat(); self.eat('(')
            c = self.expr()
            self.eat(')')
            return ('while', c, self.block())
        if tok in ('return', 'break', 'continue'):
            self.eat(); self.eat(';')
            return (tok,)
        if tok in ('call', 'swap'):
            self.eat(); self.eat('(')
            if tok == 'swap':
                self.eat('arr'); self.eat('['); a = self.expr(); self.eat(']'); self.eat(',')
                self.eat('arr'); self.eat('['); b = self.expr(); self.eat(']')
            else:
                a = self.expr(); self.eat(','); b = self.expr()
            self.eat(')'); self.eat(';')
            return (tok, a, b)
        if tok in ('arr', 'aux'):
            self.eat(); self.eat('[')
            idx = self.expr()
            self.eat(']'); self.eat('=')
            v = self.expr()
            self.eat(';')
            return ('set' + tok, idx, v)
        var = int(self.eat()[1:])
        self.eat('=')
        v = self.expr()
        self.eat(';')
        return ('assign', var, v)
    def program(self):
        body = []
        while self.peek() is not None:
            body.append(self.stmt())
        return body

# -------------------------------------------------------------- execution

def cdiv(a, b):
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q

class Machine:
    def __init__(self, prog, num_vars, seed_rec, step_limit, trace):
        self.prog, self.nv, self.seed_rec = prog, num_vars, seed_rec
        self.limit, self.trace = step_limit, trace
    def tick(self):
        self.steps += 1
        if self.steps > self.limit:
            raise Abort('timeout')
    def ev(self, e):
        self.tick()
        k = e[0]
        if k == 'const': return e[1]
        if k == 'var': return self.v[e[1]]
        if k in ('arr', 'aux'):
            i = self.ev(e[1])
            a = self.a if k == 'arr' else self.x
            if not 0 <= i < len(a): raise Abort('oob')
            return a[i]
        op, x = e[1], self.ev(e[2])
        y = self.ev(e[3])
        if op == '+': r = x + y
        elif op == '-': r = x - y
        elif op == '*': r = x * y
        elif op in ('/', '%'):
            if y == 0: raise Abort('div0')
            q = cdiv(x, y)
            r = q if op == '/' else x - q * y
        elif op == '<': return int(x < y)
        elif op == '>': return int(x > y)
        elif op == '<=': return int(x <= y)
        elif op == '>=': return int(x >= y)
        elif op == '==': return int(x == y)
        elif op == '!=': return int(x != y)
        elif op == '&': r = x & y
        elif op == '|': r = x | y
        elif op == '^': r = x ^ y
        elif op in ('<<', '>>'):
            if not 0 <= y < 32: raise Abort('shift')
            r = ((x << y) & 0xffffffff) if op == '<<' else x >> y
            if r >= 2**31: r -= 2**32
        if not -2**31 <= r < 2**31: raise Abort('overflow')
        return r
    def block(self, body):
        for s in body:
            f = self.st(s)
            if f: return f
        return None
    def st(self, s):
        self.tick()
        k = s[0]
        if k == 'assign':
            self.v[s[1]] = self.ev(s[2])
        elif k in ('setarr', 'setaux'):
            i = self.ev(s[1]); val = self.ev(s[2])
            a = self.a if k == 'setarr' else self.x
            if not 0 <= i < len(a): raise Abort('oob')
            if self.trace is not None and a[i] != val: self.trace.append((k[3], i, val))
            a[i] = val
        elif k == 'swap':
            i = self.ev(s[1]); j = self.ev(s[2])
            n = len(self.a)
            if not (0 <= i < n and 0 <= j < n): raise Abort('oob')
            self.a[i], self.a[j] = self.a[j], self.a[i]
            if self.trace is not None and i != j: self.trace.append(('s', min(i, j), max(i, j)))
        elif k == 'call':
            lo = self.ev(s[1]); hi = self.ev(s[2])
            if self.depth >= 200: raise Abort('depth')
            saved = self.v
            self.v = [0] * self.nv
            self.v[0] = len(self.a)
            if self.nv > 1: self.v[1] = lo
            if self.nv > 2: self.v[2] = hi
            self.depth += 1
            self.block(self.prog)  # return/break/continue end the callee frame
            self.depth -= 1
            self.v = saved
            self.calls += 1
        elif k == 'if':
            return self.block(s[2] if self.ev(s[1]) else s[3])
        elif k == 'while':
            while self.ev(s[1]):
                self.tick()
                f = self.block(s[2])
                if f == 'break': break
                if f == 'return': return f
        else:
            return s[0]
        return None
    def run(self, inp):
        self.a, self.x = list(inp), [0] * len(inp)
        self.v = [0] * self.nv
        if self.nv: self.v[0] = len(inp)
        if self.seed_rec and self.nv > 2: self.v[1], self.v[2] = 0, len(inp)
        self.steps = self.depth = self.calls = 0
        self.block(self.prog)
        return self.a

# ----------------------------------------------------------------- triage

def gen_tests(rng):
    tests = []
    for n in list(range(1, 9)) * 6 + [12, 17, 25, 33, 50, 64, 100, 128, 200, 256]:
        kind = rng.choice(['rand', 'dups', 'neg', 'sorted', 'rev', 'const'])
        if kind == 'rand': a = [rng.randint(0, 1000) for _ in range(n)]
        elif kind == 'dups': a = [rng.randint(0, 3) for _ in range(n)]
        elif kind == 'neg': a = [rng.randint(-500, 500) for _ in range(n)]
        elif kind == 'sorted': a = sorted(rng.randint(-50, 50) for _ in range(n))
        elif kind == 'rev': a = sorted((rng.randint(-50, 50) for _ in range(n)), reverse=True)
        else: a = [7] * n
        tests.append(a)
    return tests

def triage(sol, num_vars, seed_rec, tests, cost_inputs, fp_inputs):
    try:
        prog = Parser(tokenize(sol['program'])).program()
    except Exception as ex:
        return {'status': 'PARSE_ERROR', 'error': str(ex)}
    m = Machine(prog, num_vars, seed_rec, 5_000_000, None)
    for a in tests:
        try:
            if m.run(a) != sorted(a):
                return {'status': 'WRONG', 'fail_n': len(a), 'fail_input': a if len(a) <= 12 else None}
        except Abort as ex:
            return {'status': 'ABORT:' + str(ex), 'fail_n': len(a)}
    try:
        empty_ok = m.run([]) == []
    except Abort:
        empty_ok = False
    cost = {}
    for label, a in cost_inputs:
        m.run(a)
        cost[label] = m.steps
    calls = m.calls
    tr = []
    m.trace = tr
    for a in fp_inputs:
        tr.append(('|',))
        m.run(a)
    m.trace = None
    ns = [32, 64, 128, 256]
    ys = [math.log(cost['rand%d' % n]) for n in ns]
    xs = [math.log(n) for n in ns]
    xb, yb = sum(xs) / 4, sum(ys) / 4
    slope = sum((x - xb) * (y - yb) for x, y in zip(xs, ys)) / sum((x - xb) ** 2 for x in xs)
    src = sol['program']
    return {
        'status': 'OK', 'empty_ok': empty_ok,
        'exponent': round(slope, 2),
        'steps_rand256': cost['rand256'], 'steps_sorted256': cost['sorted256'], 'steps_rev256': cost['rev256'],
        'recursive_calls_rev256': calls,
        'behavior': hashlib.sha1(repr(tr).encode()).hexdigest()[:12],
        'uses_call': 'call(' in src, 'uses_aux': 'aux[' in src, 'uses_swap': 'swap(' in src,
        'loops': src.count('while ('), 'ifs': src.count('if ('),
    }

_ctx = None

def _work(s):
    r = triage(s, *_ctx)
    r.update(program_id=s['program_id'], size=s['program_size'], level_found=s['level_found'],
             final_status=s.get('final_status'), program=s['program'])
    return r

def main():
    run = sys.argv[1]
    seed_rec = '--no-seed-recursion' not in sys.argv
    sols = json.load(open(os.path.join(run, 'solutions.json')))
    num_vars = 8
    rng = random.Random(20261010)
    tests = gen_tests(rng)
    cost_inputs = []
    for n in (32, 64, 128, 256):
        cost_inputs.append(('rand%d' % n, [rng.randint(0, 10**6) for _ in range(n)]))
    cost_inputs += [('sorted256', list(range(256))), ('rev256', list(range(256, 0, -1)))]
    fp_inputs = [[rng.randint(0, 20) for _ in range(n)] for n in (5, 9, 16, 23)] + [list(range(12, 0, -1)), [3, 1, 3, 1, 2, 2, 0]]
    by_id = {}
    for s in sols:
        by_id.setdefault(s['program_id'], s)
    global _ctx
    _ctx = (num_vars, seed_rec, tests, cost_inputs, fp_inputs)
    with multiprocessing.Pool() as pool:
        results = pool.map(_work, sorted(by_id.values(), key=lambda s: s['program_id']), chunksize=4)
    classes = {}
    for r in results:
        if r['status'] == 'OK':
            classes.setdefault(r['behavior'], []).append(r)
    json.dump({'records': len(sols), 'distinct_ids': len(by_id), 'results': results}, open(os.path.join(run, 'triage.json'), 'w'), indent=1)

    out = ['# Sorting run triage: %s' % run, '',
           '- archived records: %d, distinct program_id: %d' % (len(sols), len(by_id))]
    st = {}
    for r in results:
        st[r['status']] = st.get(r['status'], 0) + 1
    out.append('- outcome on held-out tests (n up to 256): ' + ', '.join('%s=%d' % kv for kv in sorted(st.items())))
    out.append('- distinct behavior classes among generalizing programs: %d' % len(classes))
    out += ['', '| class | members | levels | exp | steps rand256 | sorted256 | rev256 | call | aux | rep size |', '|---|---|---|---|---|---|---|---|---|---|']
    reps = []
    for b, mem in sorted(classes.items(), key=lambda kv: (min(r['level_found'] for r in kv[1]))):
        rep = min(mem, key=lambda r: r['size'])
        reps.append((b, rep, mem))
        lv = sorted(r['level_found'] for r in mem)
        out.append('| %s | %d | %d-%d | %.2f | %d | %d | %d | %s | %s | %d |' % (
            b, len(mem), lv[0], lv[-1], rep['exponent'], rep['steps_rand256'], rep['steps_sorted256'],
            rep['steps_rev256'], rep['uses_call'], rep['uses_aux'], rep['size']))
    out += ['', '## Representatives (smallest program per class)', '']
    for b, rep, mem in reps:
        out += ['### %s (%d members, exponent %.2f)' % (b, len(mem), rep['exponent']), '', '```c', rep['program'].rstrip(), '```', '']
    open(os.path.join(run, 'triage.md'), 'w').write('\n'.join(out) + '\n')
    print('\n'.join(out[:8 + len(classes)]))

if __name__ == '__main__':
    main()
