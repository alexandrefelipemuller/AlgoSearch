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
