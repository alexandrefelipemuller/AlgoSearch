#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "problems/Problem.hpp"

namespace algosearch {

// Section 126: seven problems of PSB2 (Helmuth and Kelly, GECCO 2021) whose inputs and outputs are integers or
// vectors of integers, run under the suite's own protocol. A run gets 200 training cases (every hand-chosen edge
// case, then random ones up to 200) and is judged on 2,000 unseen random cases, both from the official datasets
// (scripts/psb2_prepare.sh downloads them; ALGOSEARCH_PSB2_DIR points at them). The training set is returned for every
// level, so `--max-n 1` is one PSB2 run; the 2,000 test cases are the adversarial set, so final_status SUCCESS with
// all adversarial cases passed is PSB2's definition of success.
//
// Encoding: the program reads `arr` (length n in v0) and writes its answer at the front of `arr`.
//   fuel_cost, basement, paired_digits, luhn: arr = the input vector; arr[0] = the answer.
//   gcd: arr = {a, b}; arr[0] = gcd.
//   find_pair: arr = vector ++ {target}; arr[0], arr[1] = the first pair (i < j) summing to the target.
//   coin_sums: arr = {amount, 0, 0, 0}; arr[0..3] = pennies, nickels, dimes, quarters.
//   Section 132 (problems chosen after the pieces were frozen, to test them on problems not looked at):
//   cut_vector: arr = the vector; arr[0] = the length of output1 (output2 is the rest, so nothing is lost).
//   fizz_buzz: arr = {n}; arr[0] = n if neither, -1 for "Fizz", -2 for "Buzz", -3 for "FizzBuzz".
//   mastermind: arr = guess(4) ++ code(4) with the letters as their ASCII codes; arr[0] = right colour in the wrong
//     place, arr[1] = right colour in the right place.
//   Section 133 (screened for a non-trivial solve rate, then run as an ON/OFF test):
//   twitter: arr = the tweet as ASCII; arr[0] = its length n (1..140), -2 when n > 140 ("Too many characters"); an empty
//     tweet ("You didn't tweet") has no cell to write in and is dropped from the cases.
//   middle_character: arr = the string; arr[0] = the middle character, or arr[0], arr[1] = the two middle ones.
//   solve_boolean: arr = the expression as ASCII, evaluated left to right; arr[0] = 1 if true, else 0.
//   leaders: arr = the vector; arr[i] = 1 if arr[i] is a leader (at least everything to its right), else 0
//     (the official output lists the leaders in order, so the flags carry the same information).
// The reference functions are checked against every loaded official case; a mismatch is reported on stderr.
class Psb2Problem : public Problem {
public:
    enum class Kind { FuelCost, Basement, Gcd, FindPair, CoinSums, Luhn, PairedDigits, CutVector, FizzBuzz, Mastermind, Twitter, MiddleChar,
                      SolveBoolean, Leaders };

    // Loads <dir>/<name>/<name>-edge.json and <name>-random.json; the seed picks the sample (a different one each
    // run, as in PSB2).
    Psb2Problem(Kind kind, int numVars, uint64_t seed, const std::string& dir = "");
    // Builds from already-parsed official lines (used by tests).
    Psb2Problem(Kind kind, int numVars, uint64_t seed, const std::vector<std::string>& edgeLines,
                const std::vector<std::string>& randomLines, size_t trainSize = 200, size_t testSize = 2000);

    static std::string datasetName(Kind kind);
    // The expected leading output cells for `arr` (the encoded input).
    static std::vector<int> reference(Kind kind, const std::vector<int>& arr);

    std::vector<Instance> generateInstances(int level) const override { (void)level; return train_; }
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override { return level + 1; }
    std::vector<Instance> generateAdversarialInstances(int level) const override { (void)level; return test_; }
    bool supportsMagnitudeShift() const override { return false; }
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    double partialScore(const std::vector<int>& input, const std::vector<int>& output) const override;
    int numVars() const override { return numVars_; }
    Instance generateBenchmarkInstance(int n, uint64_t seed) const override;
    size_t mismatchesAgainstOfficial() const { return mismatches_; }

private:
    void build(const std::vector<std::string>& edgeLines, const std::vector<std::string>& randomLines, size_t trainSize,
               size_t testSize);
    bool parseCase(const std::string& line, Instance& out) const;

    Kind kind_;
    int numVars_;
    uint64_t seed_;
    std::vector<Instance> train_, test_;
    size_t mismatches_ = 0;
};

}  // namespace algosearch
