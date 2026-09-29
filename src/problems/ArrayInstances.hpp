#pragma once

#include <cstdint>
#include <vector>

namespace algosearch {

// Shared instance-generation building blocks for array-shaped problems
// (sorting, min, sum, contains, ...). Factored out so every problem gets
// the same train/held-out generalization discipline "for free" instead of
// each one reinventing (and possibly forgetting) it -- the mistake made
// with the first SortingProblem cut, which only ever produced permutations
// of {0..n-1} and let programs "solve" it by exploiting that shape rather
// than doing anything general.

// The "base" instance set for size n: exhaustive permutations of
// {0..n-1} when n is small enough to enumerate, otherwise
// `randomCount` random arrays with values in [0, 4n).
std::vector<std::vector<int>> generateBaseArrays(int n, int exhaustiveMaxN, int randomCount, uint64_t seed);

// "Structurally varied" arrays of size n that the base set above never
// produces: all-equal, heavy duplicates, negatives, already sorted,
// reversed, and a handful of wide-range random arrays. Called with two
// different `seedOffset` values by a problem's generateInstances() (folded
// into the scored/training set) and generateAdversarialInstances() (a
// genuinely separate, held-out sample) so a program can't pass one by
// having memorized the other.
std::vector<std::vector<int>> generateStructuralArrays(int n, uint64_t seed, uint64_t seedOffset, int wideRangeSamples);

// Every array of length n with values in [lo, hi] ((hi-lo+1)^n of them),
// for Problem::exhaustiveInputs. Callers keep n small.
std::vector<std::vector<int>> allArraysOver(int n, int lo, int hi);

// `count` arrays of length n with values drawn uniformly from [lo, hi].
std::vector<std::vector<int>> randomArraysOver(int n, int lo, int hi, int count, uint64_t seed);

}  // namespace algosearch
