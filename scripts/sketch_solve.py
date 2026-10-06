#!/usr/bin/env python3
"""FINDINGS.md section 68: fill every hole of a scrambled skeleton at once
with Z3 (program sketching, CEGIS).

Input: the JSON written by `sketch_bridge export` (scripts/sketch_bridge.cpp).
Output: a fills file for `sketch_bridge verify`, one line per hole:
    value <atom|+|-|*|/|%> A B
    cond  <atom|+|-> A B <op> C

Semantics follow the Mini-C interpreter: 32-bit ints, truncating division,
division by zero (or INT_MIN / -1) and out-of-range array reads/writes are
errors, scalars start at 0 except v0 = n. While loops are unrolled up to U
iterations; still running after U counts as a failure. Holes range over
section 65's grammar (search/HoleGrammar.hpp): atoms are the scalars,
literals 0..2, arr[scalar], arr[0..2]; values are an atom or atom op atom
(op in + - * / %); conditions are (atom | atom +/- atom) cmp atom.

CEGIS: training instances whose input values are all <= --max-value form the
pool; solve on a few, run the candidate concretely on the whole pool, add
the first failing instance, repeat.

The solver works at --width bits (default 12, enough for pool values
<= --max-value and their products here); every candidate it returns is run
concretely at 32 bits, so a width artifact can only cost an extra CEGIS round.

Usage: sketch_solve.py sketch.json fills.txt [--unroll 16] [--max-value 12]
       [--timeout 600] [--width 12]
"""

import argparse
import json
import sys
import time

import z3

INT_MIN = -(2**31)
VALUE_FORMS = ["atom", "+", "-", "*", "/", "%"]
COND_FORMS = ["atom", "+", "-"]
CMP_OPS = ["<", ">", "==", "<=", ">=", "!="]


# ---------------------------------------------------------------- symbolic

class State:
    def __init__(self, vars_, arr, err):
        self.vars = vars_  # dict index -> BitVec
        self.arr = arr  # list of BitVec
        self.err = err  # Bool

    def merge(self, take, other):
        """If(take, other, self), elementwise."""
        return State({k: z3.If(take, other.vars[k], v) for k, v in self.vars.items()},
                     [z3.If(take, o, s) for s, o in zip(self.arr, other.arr)],
                     z3.If(take, other.err, self.err))


WIDTH = 32  # solver bit width (--width); concrete checks always use 32 bits


def bv(x):
    return z3.BitVecVal(x, WIDTH)


class Encoder:
    def __init__(self, sketch, unroll):
        self.vars = sketch["vars"]
        self.unroll = unroll
        self.nholes = sketch["holes"]
        self.program = sketch["program"]
        self.natoms = len(self.vars) + 3 + len(self.vars) + 3
        # Hole choice variables.
        self.h = []
        for i in range(self.nholes):
            # Small bitvectors, not Ints, so the whole problem stays in QF_BV.
            self.h.append({k: z3.BitVec(f"h{i}_{k}", 8) for k in ("form", "a", "b", "op", "c")})

    def domain(self, kinds):
        """Range constraints on the hole variables (kinds: hole id -> 'value'|'cond')."""
        cs = []
        for i, t in kinds.items():
            h = self.h[i]
            nf = len(VALUE_FORMS) if t == "value" else len(COND_FORMS)
            cs += [z3.ULT(h["form"], nf), z3.ULT(h["a"], self.natoms), z3.ULT(h["b"], self.natoms)]
            if t == "cond":
                cs += [z3.ULT(h["op"], len(CMP_OPS)), z3.ULT(h["c"], self.natoms)]
        return cs

    def read(self, st, idx):
        """arr[idx] for a BitVec idx: (value, out-of-range)."""
        n = len(st.arr)
        val = bv(0)
        for k in reversed(range(n)):
            val = z3.If(idx == k, st.arr[k], val)
        oob = z3.Or(idx < 0, idx >= n)
        return val, oob

    def atom(self, st, sel):
        """Value of atom number `sel` (an Int) on state st: (value, error)."""
        vals = []
        for v in self.vars:
            vals.append((st.vars[v], z3.BoolVal(False)))
        for c in range(3):
            vals.append((bv(c), z3.BoolVal(False)))
        for v in self.vars:
            vals.append(self.read(st, st.vars[v]))
        for c in range(3):
            vals.append(self.read(st, bv(c)))
        value, err = vals[-1]
        for k in reversed(range(len(vals) - 1)):
            value = z3.If(sel == k, vals[k][0], value)
            err = z3.If(sel == k, vals[k][1], err)
        return value, err

    def arith(self, op, a, b):
        """(value, error) of a op b with Mini-C semantics."""
        if op == "+":
            return a + b, z3.BoolVal(False)
        if op == "-":
            return a - b, z3.BoolVal(False)
        if op == "*":
            return a * b, z3.BoolVal(False)
        bad = z3.Or(b == 0, z3.And(a == bv(-(2 ** (WIDTH - 1))), b == bv(-1)))
        if op == "/":
            return z3.If(bad, bv(0), a / b), bad  # z3 '/' on BitVec is signed, truncating
        return z3.If(bad, bv(0), z3.SRem(a, b)), bad

    def compare(self, op, a, b):
        return {"<": a < b, ">": a > b, "==": a == b, "<=": a <= b, ">=": a >= b, "!=": a != b}[op]

    def hole_value(self, st, i):
        h = self.h[i]
        a, ea = self.atom(st, h["a"])
        b, eb = self.atom(st, h["b"])
        value, err = a, ea
        for f, op in enumerate(VALUE_FORMS[1:], start=1):
            r, e = self.arith(op, a, b)
            value = z3.If(h["form"] == f, r, value)
            err = z3.If(h["form"] == f, z3.Or(ea, eb, e), err)
        return value, err

    def hole_cond(self, st, i):
        h = self.h[i]
        a, ea = self.atom(st, h["a"])
        b, eb = self.atom(st, h["b"])
        c, ec = self.atom(st, h["c"])
        lhs = z3.If(h["form"] == 1, a + b, z3.If(h["form"] == 2, a - b, a))
        err = z3.Or(ea, ec, z3.And(h["form"] != 0, eb))
        result = self.compare(CMP_OPS[-1], lhs, c)
        for k in reversed(range(len(CMP_OPS) - 1)):
            result = z3.If(h["op"] == k, self.compare(CMP_OPS[k], lhs, c), result)
        return result, err

    def expr(self, st, e, as_bool=False):
        k = e["k"]
        if k == "hole":
            if e["t"] == "cond":
                return self.hole_cond(st, e["id"])
            return self.hole_value(st, e["id"])
        if k == "const":
            return bv(e["v"]), z3.BoolVal(False)
        if k == "var":
            return st.vars[e["i"]], z3.BoolVal(False)
        if k == "arr":
            idx, ei = self.expr(st, e["i"])
            val, oob = self.read(st, idx)
            return val, z3.Or(ei, oob)
        l, el = self.expr(st, e["l"])
        r, er = self.expr(st, e["r"])
        if e["op"] in CMP_OPS:
            return self.compare(e["op"], l, r), z3.Or(el, er)
        v, ex = self.arith(e["op"], l, r)
        return v, z3.Or(el, er, ex)

    def cond(self, st, e):
        c, err = self.expr(st, e)
        if not z3.is_bool(c):
            c = c != 0
        return c, err

    def stmts(self, st, body):
        for s in body:
            st = self.stmt(st, s)
        return st

    def stmt(self, st, s):
        k = s["k"]
        if k == "block":
            return self.stmts(st, s["b"])
        if k == "assign":
            v, err = self.expr(st, s["v"])
            vars_ = dict(st.vars)
            vars_[s["i"]] = v
            return State(vars_, st.arr, z3.Or(st.err, err))
        if k == "assignArr":
            idx, ei = self.expr(st, s["i"])
            v, ev = self.expr(st, s["v"])
            arr = [z3.If(idx == j, v, a) for j, a in enumerate(st.arr)]
            oob = z3.Or(idx < 0, idx >= len(st.arr))
            return State(st.vars, arr, z3.Or(st.err, ei, ev, oob))
        if k == "if":
            c, err = self.cond(st, s["c"])
            st = State(st.vars, st.arr, z3.Or(st.err, err))
            then_st = self.stmts(st, s["b"])
            else_st = self.stmts(st, s["e"])
            return else_st.merge(c, then_st)
        if k == "while":
            active = z3.BoolVal(True)
            for _ in range(self.unroll):
                c, err = self.cond(st, s["c"])
                st = State(st.vars, st.arr, z3.Or(st.err, z3.And(active, err)))
                take = z3.And(active, c)
                st = st.merge(take, self.stmts(st, s["b"]))
                active = take
            c, err = self.cond(st, s["c"])
            return State(st.vars, st.arr, z3.Or(st.err, z3.And(active, z3.Or(c, err))))
        raise ValueError(f"unsupported statement {k}")

    def instance(self, inst):
        """Constraint: the program is correct on this instance."""
        n = len(inst["in"])
        vars_ = {v: bv(0) for v in self.vars}
        vars_[0] = bv(n)
        st = State(vars_, [bv(x) for x in inst["in"]], z3.BoolVal(False))
        st = self.stmts(st, self.program)
        return z3.And(z3.Not(st.err), st.arr[0] == bv(inst["out"]))


# ---------------------------------------------------------------- concrete

class Fail(Exception):
    pass


def wrap(x):
    x &= 0xFFFFFFFF
    return x - (1 << 32) if x >= 1 << 31 else x


def c_div(a, b, op):
    if b == 0 or (a == INT_MIN and b == -1):
        raise Fail()
    q = abs(a) // abs(b)
    q = q if (a >= 0) == (b >= 0) else -q
    return q if op == "/" else a - q * b


class Concrete:
    """Runs the sketch with concrete fills (a model)."""

    def __init__(self, sketch, fills, step_limit=200000):
        self.vars = sketch["vars"]
        self.program = sketch["program"]
        self.fills = fills
        self.step_limit = step_limit

    def atom(self, env, i):
        nv = len(self.vars)
        if i < nv:
            return env["v"][self.vars[i]]
        if i < nv + 3:
            return i - nv
        if i < 2 * nv + 3:
            return self.read(env, env["v"][self.vars[i - nv - 3]])
        return self.read(env, i - 2 * nv - 3)

    def read(self, env, idx):
        if idx < 0 or idx >= len(env["a"]):
            raise Fail()
        return env["a"][idx]

    def arith(self, op, a, b):
        if op == "+":
            return wrap(a + b)
        if op == "-":
            return wrap(a - b)
        if op == "*":
            return wrap(a * b)
        return c_div(a, b, op)

    def expr(self, env, e):
        k = e["k"]
        if k == "hole":
            f = self.fills[e["id"]]
            a = self.atom(env, f["a"])
            if e["t"] == "value":
                return a if f["form"] == "atom" else self.arith(f["form"], a, self.atom(env, f["b"]))
            lhs = a if f["form"] == "atom" else self.arith(f["form"], a, self.atom(env, f["b"]))
            return int(cmp(f["op"], lhs, self.atom(env, f["c"])))
        if k == "const":
            return e["v"]
        if k == "var":
            return env["v"][e["i"]]
        if k == "arr":
            return self.read(env, self.expr(env, e["i"]))
        l, r = self.expr(env, e["l"]), self.expr(env, e["r"])
        if e["op"] in CMP_OPS:
            return int(cmp(e["op"], l, r))
        return self.arith(e["op"], l, r)

    def tick(self, env):
        env["steps"] += 1
        if env["steps"] > self.step_limit:
            raise Fail()

    def run_stmts(self, env, body):
        for s in body:
            self.run(env, s)

    def run(self, env, s):
        self.tick(env)
        k = s["k"]
        if k == "block":
            self.run_stmts(env, s["b"])
        elif k == "assign":
            env["v"][s["i"]] = self.expr(env, s["v"])
        elif k == "assignArr":
            idx = self.expr(env, s["i"])
            val = self.expr(env, s["v"])
            if idx < 0 or idx >= len(env["a"]):
                raise Fail()
            env["a"][idx] = val
        elif k == "if":
            self.run_stmts(env, s["b"] if self.expr(env, s["c"]) else s["e"])
        elif k == "while":
            while self.expr(env, s["c"]):
                self.tick(env)
                self.run_stmts(env, s["b"])

    def correct(self, inst):
        env = {"v": {v: 0 for v in self.vars}, "a": list(inst["in"]), "steps": 0}
        env["v"][0] = len(inst["in"])
        try:
            self.run_stmts(env, self.program)
        except (Fail, RecursionError):
            return False
        return env["a"][0] == inst["out"]


def cmp(op, a, b):
    return {"<": a < b, ">": a > b, "==": a == b, "<=": a <= b, ">=": a >= b, "!=": a != b}[op]


# ---------------------------------------------------------------- CEGIS

def hole_kinds(program):
    kinds = {}

    def walk_e(e):
        if e["k"] == "hole":
            kinds[e["id"]] = e["t"]
        for key in ("l", "r", "i"):
            if isinstance(e.get(key), dict):
                walk_e(e[key])

    def walk_s(s):
        for key in ("v", "c", "i"):
            if isinstance(s.get(key), dict):
                walk_e(s[key])
        for key in ("b", "e"):
            for c in s.get(key, []):
                walk_s(c)

    for s in program:
        walk_s(s)
    return kinds


def fills_from_model(model, enc, kinds):
    fills = {}
    for i, t in kinds.items():
        h = enc.h[i]
        get = lambda k: model.eval(h[k], model_completion=True).as_long()
        f = {"a": get("a"), "b": get("b")}
        if t == "value":
            f["form"] = VALUE_FORMS[get("form")]
        else:
            f["form"] = COND_FORMS[get("form")]
            f["op"] = CMP_OPS[get("op")]
            f["c"] = get("c")
        fills[i] = f
    return fills


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("sketch")
    ap.add_argument("fills")
    ap.add_argument("--unroll", type=int, default=16)
    ap.add_argument("--max-value", type=int, default=12)
    ap.add_argument("--timeout", type=int, default=600)
    ap.add_argument("--width", type=int, default=12,
                    help="solver bit width; enough for the pool's values, every candidate is re-checked at 32 bits")
    args = ap.parse_args()
    global WIDTH
    WIDTH = args.width
    sketch = json.load(open(args.sketch))
    kinds = hole_kinds(sketch["program"])
    pool = [i for i in sketch["train"] if all(abs(x) <= args.max_value for x in i["in"] + [i["out"]])]
    enc = Encoder(sketch, args.unroll)
    solver = z3.SolverFor("QF_BV")
    solver.add(*enc.domain(kinds))
    start = time.time()
    used = [pool[0], pool[len(pool) // 2], pool[-1]]
    for inst in used:
        solver.add(enc.instance(inst))
    for it in range(200):
        remaining = args.timeout - (time.time() - start)
        if remaining <= 0:
            print(f"RESULT TIMEOUT after {it} iterations, {len(used)} instances, {time.time() - start:.1f}s")
            return 2
        solver.set("timeout", int(remaining * 1000))
        r = solver.check()
        if r == z3.unsat:
            print(f"RESULT UNSAT after {it} iterations, {len(used)} instances, {time.time() - start:.1f}s")
            return 1
        if r != z3.sat:
            print(f"RESULT TIMEOUT after {it} iterations, {len(used)} instances, {time.time() - start:.1f}s")
            return 2
        fills = fills_from_model(solver.model(), enc, kinds)
        runner = Concrete(sketch, fills)
        failing = next((i for i in pool if not runner.correct(i)), None)
        if failing is None:
            with open(args.fills, "w") as f:
                for i in sorted(fills):
                    h = fills[i]
                    if kinds[i] == "value":
                        f.write(f"value {h['form']} {h['a']} {h['b']}\n")
                    else:
                        f.write(f"cond {h['form']} {h['a']} {h['b']} {h['op']} {h['c']}\n")
            print(f"RESULT SAT after {it + 1} iterations, {len(used)} instances "
                  f"(pool {len(pool)}), {time.time() - start:.1f}s")
            return 0
        used.append(failing)
        solver.add(enc.instance(failing))
    print("RESULT TIMEOUT iteration cap")
    return 2


if __name__ == "__main__":
    sys.exit(main())
