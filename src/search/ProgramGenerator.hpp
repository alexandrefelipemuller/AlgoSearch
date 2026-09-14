#pragma once

#include "ast/Stmt.hpp"
#include "core/Random.hpp"

namespace algosearch {

// Bounds respected while generating/growing programs, to avoid unbounded
// AST explosion during random construction.
struct GenConfig {
    int numVars = 4;
    int maxExprDepth = 3;
    int maxStatementsPerBlock = 4;
    int maxBlockDepth = 3;   // nesting depth of if/while
    int maxLoops = 2;        // max number of while-loops in a whole program
    int maxConst = 3;        // generated integer literals are in [0, maxConst]

    // Grammar-level bias, in [0, 1], toward directly emitting a canonical
    // "counting loop" skeleton --
    //   acc = 0 (or arr[0]);  i = 0;  while (i < n) { <body>; i = i + 1; }
    // -- instead of a fully unstructured random statement, whenever a loop
    // is being generated. <body> is itself one of a few canonical
    // accumulation shapes (running sum, running min/max via if, target
    // membership check, adjacent-swap) rather than arbitrary code, so the
    // *skeleton* that real array algorithms need (an index that starts at
    // 0, is compared against n, and increments exactly once) is handed to
    // the generator directly with higher probability, rather than left to
    // emerge from unbiased mutation alone. 0.0 (default) disables it and
    // reproduces the original unbiased generator exactly.
    double loopAccumulatorBias = 0.0;
};

// Generates a brand-new random program (a Block) with approximately
// `targetSize` AST nodes (generation stops once the size is reached or
// generation limits are hit, so actual size may be slightly smaller).
Program generateRandomProgram(Random& rng, int targetSize, const GenConfig& cfg);

// Clones `seed` and grows it by inserting freshly generated statements into
// randomly chosen blocks until its size reaches (at least) `targetSize`, or
// no more growth is possible. Used by beam search to expand promising
// candidates from one program size to the next.
Program growProgram(const Stmt& seed, int targetSize, Random& rng, const GenConfig& cfg);

}  // namespace algosearch
