#pragma once

#include <vector>

#include "ast/Stmt.hpp"
#include "interpreter/Interpreter.hpp"
#include "problems/Problem.hpp"

namespace algosearch {

// Section 51, option (b) of section 50: fill a small TAIL after a program
// prefix by exhaustive enumeration instead of mutation -- the same move
// section 41 made for the TSP guard, aimed at section 50's near misses
// ("sort, then read arr[1]" that is right for n >= 2 but crashes at n = 1).
//
// A tail is one of
//   arr[0] = E;
//   if (v0 op k) { arr[0] = E1; } else { arr[0] = E2; }   -- a small-n guard
// with op in < <= == >= >, k in 1..3, and E a literal 0..2, a scalar v1..,
// or arr[I] for I in {0, 1, 2, n-1, n-2, n/2, (n-1)/2}.
//
// The prefix runs ONCE per instance (its final arr and scalars are kept);
// every tail is then evaluated directly on those end states, so the cost
// is one interpretation per instance plus cheap arithmetic, not one per
// (tail, instance). Guarded tails decompose: the best (E1, E2) for a guard is
// the best E1 on the instances where the guard holds plus the best E2 where
// it does not.
//
// Returns prefix + the tail solving the most `instances` (ties: an
// unguarded tail, then the earliest enumerated), or nullptr when the prefix
// completes on no instance.
Program fillTail(const Stmt& prefix, const std::vector<Instance>& instances, const Problem& problem,
                 const Interpreter& interp);

}  // namespace algosearch
