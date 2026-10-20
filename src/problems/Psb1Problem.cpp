#include "problems/Psb1Problem.hpp"

#include <algorithm>

#include "core/Random.hpp"

namespace algosearch {

namespace {

std::vector<int> randomVector(Random& rng, int minLen, int maxLen, int lo, int hi) {
    std::vector<int> v(static_cast<size_t>(rng.uniformInt(minLen, maxLen)));
    for (int& x : v) x = rng.uniformInt(lo, hi);
    return v;
}

}  // namespace

std::string Psb1Problem::name(Kind kind) {
    switch (kind) {
        case Kind::LastIndexOfZero: return "last_index_of_zero";
        case Kind::CountOdds: return "count_odds";
        case Kind::NegativeToZero: return "negative_to_zero";
        case Kind::VectorsSummed: return "vectors_summed";
        case Kind::SumOfSquares: return "sum_of_squares";
        case Kind::CollatzSteps: return "collatz_steps";
        case Kind::SmallOrLarge: return "small_or_large";
        case Kind::MirrorImage: return "mirror_image";
        case Kind::Digits: return "digits";
        case Kind::Grade: return "grade";
    }
    return "";
}

std::vector<int> Psb1Problem::reference(Kind kind, const std::vector<int>& arr, bool medium) {
    switch (kind) {
        case Kind::LastIndexOfZero: {
            for (size_t i = arr.size(); i-- > 0;) {
                if (arr[i] == 0) return {static_cast<int>(i)};
            }
            return {-1};
        }
        case Kind::CountOdds: {
            int count = 0;
            for (int x : arr) count += x % 2 != 0;
            return {count};
        }
        case Kind::NegativeToZero: {
            std::vector<int> out = arr;
            for (int& x : out) x = std::max(x, 0);
            return out;
        }
        case Kind::VectorsSummed: {
            size_t half = arr.size() / 2;
            std::vector<int> out(half);
            for (size_t i = 0; i < half; ++i) out[i] = arr[i] + arr[half + i];
            return out;
        }
        case Kind::SumOfSquares: {
            int n = arr.empty() ? 0 : arr[0], sum = 0;
            for (int i = 1; i <= n; ++i) sum += i * i;
            return {sum};
        }
        case Kind::CollatzSteps: {
            long n = arr.empty() ? 1 : arr[0];
            int steps = 0;
            while (n > 1 && steps < 100000) {
                n = n % 2 == 0 ? n / 2 : 3 * n + 1;
                ++steps;
            }
            return {steps};
        }
        case Kind::SmallOrLarge: {
            int n = arr.empty() ? 0 : arr[0];
            return {n < (medium ? 10 : 1000) ? 1 : n >= (medium ? 20 : 2000) ? 2 : 0};
        }
        case Kind::MirrorImage: {
            size_t half = arr.size() / 2;
            for (size_t i = 0; i < half; ++i) {
                if (arr[i] != arr[arr.size() - 1 - i]) return {0};
            }
            return {1};
        }
        case Kind::Digits: {
            std::vector<int> out;
            int n = arr.empty() ? 0 : arr[0];
            while (n > 0) {
                out.push_back(n % 10);
                n /= 10;
            }
            return out;
        }
        case Kind::Grade: {
            if (arr.size() < 5) return {0};
            int s = arr[4];
            return {s >= arr[0] ? 4 : s >= arr[1] ? 3 : s >= arr[2] ? 2 : s >= arr[3] ? 1 : 0};
        }
    }
    return {0};
}

std::vector<int> Psb1Problem::randomInput(Kind kind, Random& rng, bool medium) {
    const int maxLen = medium ? 8 : 50, big = medium ? 20 : 1000;
    switch (kind) {
        case Kind::LastIndexOfZero: {
            std::vector<int> v = randomVector(rng, 1, maxLen, medium ? -5 : -50, medium ? 5 : 50);
            v[static_cast<size_t>(rng.uniformInt(0, static_cast<int>(v.size()) - 1))] = 0;
            return v;
        }
        case Kind::CountOdds:
        case Kind::NegativeToZero: return randomVector(rng, 1, maxLen, -big, big);
        case Kind::VectorsSummed: {
            std::vector<int> a = randomVector(rng, 1, maxLen, -big, big);
            std::vector<int> b = randomVector(rng, static_cast<int>(a.size()), static_cast<int>(a.size()), -big, big);
            a.insert(a.end(), b.begin(), b.end());
            return a;
        }
        case Kind::SumOfSquares: return {rng.uniformInt(1, medium ? 10 : 100)};
        case Kind::CollatzSteps: return {rng.uniformInt(1, medium ? 30 : 2000)};
        case Kind::SmallOrLarge: return {rng.uniformInt(medium ? -100 : -10000, medium ? 100 : 10000)};
        case Kind::MirrorImage: {
            std::vector<int> a = randomVector(rng, 1, maxLen, -big, big);
            std::vector<int> b(a.rbegin(), a.rend());
            if (rng.uniformInt(0, 1) == 0) {
                b[static_cast<size_t>(rng.uniformInt(0, static_cast<int>(b.size()) - 1))] = rng.uniformInt(-big, big);
            }
            a.insert(a.end(), b.begin(), b.end());
            return a;
        }
        case Kind::Digits: {
            std::vector<int> v(7, 0);
            v[0] = rng.uniformInt(1, medium ? 9999 : 999999);
            return v;
        }
        case Kind::Grade: {
            int t[4];
            do {
                for (int& x : t) x = rng.uniformInt(0, 100);
                std::sort(t, t + 4, [](int a, int b) { return a > b; });
            } while (t[0] == t[1] || t[1] == t[2] || t[2] == t[3]);
            return {t[0], t[1], t[2], t[3], rng.uniformInt(0, 100)};
        }
    }
    return {0};
}

Psb1Problem::Psb1Problem(Kind kind, int numVars, uint64_t seed, bool medium)
    : kind_(kind), medium_(medium), numVars_(numVars) {
    Random rng(seed);
    auto make = [&](size_t count, std::vector<Instance>& out) {
        while (out.size() < count) {
            Instance inst;
            inst.input = randomInput(kind_, rng, medium_);
            inst.expectedOutput = inst.input;
            std::vector<int> ref = reference(kind_, inst.input, medium_);
            for (size_t i = 0; i < ref.size(); ++i) inst.expectedOutput[i] = ref[i];
            out.push_back(std::move(inst));
        }
    };
    make(200, train_);
    make(2000, test_);
}

std::string Psb1Problem::describeLevel(int level) const {
    (void)level;
    return std::string(medium_ ? "PSB1-style (medium) " : "PSB1-style ") + name(kind_) + ", " + std::to_string(train_.size()) + " training cases";
}

bool Psb1Problem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size()) return false;
    std::vector<int> ref = reference(kind_, input, medium_);
    for (size_t i = 0; i < ref.size(); ++i) {
        if (output[i] != ref[i]) return false;
    }
    return true;
}

double Psb1Problem::partialScore(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size()) return 0.0;
    std::vector<int> ref = reference(kind_, input, medium_);
    size_t good = 0;
    for (size_t i = 0; i < ref.size(); ++i) good += output[i] == ref[i];
    return static_cast<double>(good) / static_cast<double>(ref.size());
}

Instance Psb1Problem::generateBenchmarkInstance(int n, uint64_t seed) const {
    (void)n;
    if (train_.empty()) return {{}, {}};
    return train_[seed % train_.size()];
}

}  // namespace algosearch
