#pragma once

#include <cstdint>
#include <string>

#include "problems/Problem.hpp"

namespace algosearch {

// Section 65: single-hole ablation of a hand-written skeleton. Finds a
// reference program built by skeleton `name` that solves `problem` on
// levels startLevel..trainMax (and on the next three levels), then, for
// every If/While condition and every assignment right-hand side in it,
// replaces that one expression by a hole filled by exhaustive enumeration
// (atoms: the program's scalars, literals 0..2, arr[v], arr[0..2]; values: atom or
// atom op atom with op in + - * / %; conditions: atom or atom +/- atom
// compared with an atom). Candidates are deduplicated by their values on
// the states the reference visits at that site, run on training, ranked by
// steps, and the site is classified recovered / reach-only / unreached by
// held-out correctness (see FINDINGS.md section 65). Prints a per-site
// report and a summary line.
void runSkeletonAblation(const std::string& name, const Problem& problem, int startLevel, int trainMax, int numVars,
                         uint64_t seed, unsigned threads);

}  // namespace algosearch
