#include "problems/Psb2Problem.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>

#include "core/Random.hpp"

namespace algosearch {

namespace {

// Values of one JSON line: key -> numbers (a scalar is one number, a vector its items, a digit string its digits).
std::map<std::string, std::vector<long>> parseLine(const std::string& line) {
    std::map<std::string, std::vector<long>> out;
    size_t i = 0;
    while (i < line.size()) {
        size_t q = line.find('"', i);
        if (q == std::string::npos) break;
        size_t e = line.find('"', q + 1);
        if (e == std::string::npos) break;
        std::string key = line.substr(q + 1, e - q - 1);
        size_t colon = line.find(':', e);
        if (colon == std::string::npos) break;
        size_t p = colon + 1;
        while (p < line.size() && line[p] == ' ') ++p;
        std::vector<long> values;
        if (p < line.size() && line[p] == '[') {
            size_t close = line.find(']', p);
            size_t k = p + 1;
            while (k < close) {
                while (k < close && !(std::isdigit(static_cast<unsigned char>(line[k])) || line[k] == '-')) ++k;
                if (k >= close) break;
                char* end = nullptr;
                values.push_back(std::strtol(line.c_str() + k, &end, 10));
                k = static_cast<size_t>(end - line.c_str());
            }
            i = close + 1;
        } else if (p < line.size() && line[p] == '"') {
            size_t close = line.find('"', p + 1);
            for (size_t k = p + 1; k < close; ++k) values.push_back(line[k] - '0');
            i = close + 1;
        } else {
            char* end = nullptr;
            values.push_back(std::strtol(line.c_str() + p, &end, 10));
            i = static_cast<size_t>(end - line.c_str());
        }
        out[key] = std::move(values);
    }
    return out;
}

std::vector<std::string> readLines(const std::string& path, size_t cap) {
    std::vector<std::string> lines;
    std::ifstream in(path);
    std::string line;
    while (lines.size() < cap && std::getline(in, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

}  // namespace

std::string Psb2Problem::datasetName(Kind kind) {
    switch (kind) {
        case Kind::FuelCost: return "fuel-cost";
        case Kind::Basement: return "basement";
        case Kind::Gcd: return "gcd";
        case Kind::FindPair: return "find-pair";
        case Kind::CoinSums: return "coin-sums";
        case Kind::Luhn: return "luhn";
        case Kind::PairedDigits: return "paired-digits";
    }
    return "";
}

std::vector<int> Psb2Problem::reference(Kind kind, const std::vector<int>& arr) {
    switch (kind) {
        case Kind::FuelCost: {
            long sum = 0;
            for (int x : arr) sum += x / 3 - 2;
            return {static_cast<int>(sum)};
        }
        case Kind::Basement: {
            long sum = 0;
            for (size_t i = 0; i < arr.size(); ++i) {
                sum += arr[i];
                if (sum < 0) return {static_cast<int>(i)};
            }
            return {-1};
        }
        case Kind::Gcd: {
            int a = arr.size() > 0 ? arr[0] : 0, b = arr.size() > 1 ? arr[1] : 0;
            while (b != 0) {
                int t = a % b;
                a = b;
                b = t;
            }
            return {a};
        }
        case Kind::FindPair: {
            if (arr.size() < 3) return {0, 0};
            int target = arr.back();
            for (size_t i = 0; i + 1 < arr.size() - 1; ++i) {
                for (size_t j = i + 1; j < arr.size() - 1; ++j) {
                    if (arr[i] + arr[j] == target) return {arr[i], arr[j]};
                }
            }
            return {0, 0};
        }
        case Kind::CoinSums: {
            int amount = arr.empty() ? 0 : arr[0];
            int quarters = amount / 25;
            amount %= 25;
            int dimes = amount / 10;
            amount %= 10;
            int nickels = amount / 5;
            amount %= 5;
            return {amount, nickels, dimes, quarters};
        }
        case Kind::Luhn: {
            int sum = 0;
            for (size_t i = 0; i < arr.size(); ++i) {
                int d = arr[i];
                if (i % 2 == 0) {
                    d *= 2;
                    if (d > 9) d -= 9;
                }
                sum += d;
            }
            return {sum};
        }
        case Kind::PairedDigits: {
            int sum = 0;
            for (size_t i = 0; i + 1 < arr.size(); ++i) {
                if (arr[i] == arr[i + 1]) sum += arr[i];
            }
            return {sum};
        }
    }
    return {0};
}

bool Psb2Problem::parseCase(const std::string& line, Instance& out) const {
    auto fields = parseLine(line);
    auto get = [&](const char* key) -> const std::vector<long>* {
        auto it = fields.find(key);
        return it == fields.end() ? nullptr : &it->second;
    };
    const auto* in1 = get("input1");
    if (!in1) return false;
    std::vector<int> arr;
    for (long v : *in1) arr.push_back(static_cast<int>(v));
    if (kind_ == Kind::FindPair) {
        const auto* in2 = get("input2");
        if (!in2 || in2->empty()) return false;
        arr.push_back(static_cast<int>((*in2)[0]));
    } else if (kind_ == Kind::Gcd) {
        const auto* in2 = get("input2");
        if (!in2 || in2->empty()) return false;
        arr.push_back(static_cast<int>((*in2)[0]));
    } else if (kind_ == Kind::CoinSums) {
        arr.resize(4, 0);
    }
    std::vector<int> expected;
    for (const char* key : {"output1", "output2", "output3", "output4"}) {
        const auto* o = get(key);
        if (!o) break;
        expected.push_back(static_cast<int>((*o).empty() ? 0 : (*o)[0]));
    }
    if (expected.empty() || expected.size() > arr.size()) return false;
    out.input = arr;
    out.expectedOutput = arr;
    for (size_t i = 0; i < expected.size(); ++i) out.expectedOutput[i] = expected[i];
    return true;
}

void Psb2Problem::build(const std::vector<std::string>& edgeLines, const std::vector<std::string>& randomLines,
                        size_t trainSize, size_t testSize) {
    std::vector<Instance> edge, random;
    for (const auto& line : edgeLines) {
        Instance inst;
        if (parseCase(line, inst)) edge.push_back(std::move(inst));
    }
    for (const auto& line : randomLines) {
        Instance inst;
        if (parseCase(line, inst)) random.push_back(std::move(inst));
    }
    for (const auto* set : {&edge, &random}) {
        for (const auto& inst : *set) {
            std::vector<int> ref = reference(kind_, inst.input);
            for (size_t i = 0; i < ref.size(); ++i) {
                if (inst.expectedOutput[i] != ref[i]) {
                    ++mismatches_;
                    break;
                }
            }
        }
    }
    if (mismatches_ > 0) {
        std::cerr << "warning: " << mismatches_ << " official PSB2 cases disagree with the reference of "
                  << datasetName(kind_) << "\n";
    }
    Random rng(seed_);
    auto sample = [&](size_t count) {
        std::vector<Instance> picked;
        std::vector<size_t> order(random.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        for (size_t i = 0; i < std::min(count, order.size()); ++i) {
            size_t j = i + static_cast<size_t>(rng.uniformInt(0, static_cast<int>(order.size() - i) - 1));
            std::swap(order[i], order[j]);
            picked.push_back(random[order[i]]);
        }
        return picked;
    };
    if (trainSize < edge.size()) {
        std::vector<Instance> shuffled = edge;
        for (size_t i = 0; i < trainSize; ++i) {
            size_t j = i + static_cast<size_t>(rng.uniformInt(0, static_cast<int>(shuffled.size() - i) - 1));
            std::swap(shuffled[i], shuffled[j]);
        }
        shuffled.resize(trainSize);
        train_ = std::move(shuffled);
    } else {
        train_ = edge;
        for (auto& inst : sample(trainSize - edge.size())) train_.push_back(std::move(inst));
    }
    test_ = sample(testSize);
}

Psb2Problem::Psb2Problem(Kind kind, int numVars, uint64_t seed, const std::string& dir)
    : kind_(kind), numVars_(numVars), seed_(seed) {
    std::string root = dir;
    if (root.empty()) {
        const char* env = std::getenv("ALGOSEARCH_PSB2_DIR");
        root = env ? env : "data/psb2";
    }
    std::string name = datasetName(kind);
    auto edge = readLines(root + "/" + name + "/" + name + "-edge.json", 100000);
    auto random = readLines(root + "/" + name + "/" + name + "-random.json", 50000);
    if (edge.empty() || random.empty()) {
        std::cerr << "error: PSB2 data for " << name << " not found under " << root
                  << " (run scripts/psb2_prepare.sh or set ALGOSEARCH_PSB2_DIR)\n";
    }
    build(edge, random, 200, 2000);
}

Psb2Problem::Psb2Problem(Kind kind, int numVars, uint64_t seed, const std::vector<std::string>& edgeLines,
                         const std::vector<std::string>& randomLines, size_t trainSize, size_t testSize)
    : kind_(kind), numVars_(numVars), seed_(seed) {
    build(edgeLines, randomLines, trainSize, testSize);
}

std::string Psb2Problem::describeLevel(int level) const {
    (void)level;
    return "PSB2 " + datasetName(kind_) + ", " + std::to_string(train_.size()) + " training cases";
}

bool Psb2Problem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size()) return false;
    std::vector<int> ref = reference(kind_, input);
    for (size_t i = 0; i < ref.size(); ++i) {
        if (output[i] != ref[i]) return false;
    }
    return true;
}

double Psb2Problem::partialScore(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size()) return 0.0;
    std::vector<int> ref = reference(kind_, input);
    size_t good = 0;
    for (size_t i = 0; i < ref.size(); ++i) good += output[i] == ref[i];
    return static_cast<double>(good) / static_cast<double>(ref.size());
}

Instance Psb2Problem::generateBenchmarkInstance(int n, uint64_t seed) const {
    (void)n;
    if (train_.empty()) return {{}, {}};
    return train_[seed % train_.size()];
}

}  // namespace algosearch
