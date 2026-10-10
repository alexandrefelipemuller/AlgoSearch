#include "problems/ListQueryProblem.hpp"

#include <algorithm>

namespace algosearch {

std::vector<int> ListQueryProblem::walk(const std::vector<int>& input, bool& cyclic) {
    int n = static_cast<int>(input.size()) - 1;
    std::vector<int> nodes;
    std::vector<char> seen(static_cast<size_t>(std::max(n, 0)), 0);
    cyclic = false;
    for (int cur = input.empty() ? n : input[0]; cur >= 0 && cur < n; cur = input[static_cast<size_t>(1 + cur)]) {
        if (seen[static_cast<size_t>(cur)]) {
            cyclic = true;
            break;
        }
        seen[static_cast<size_t>(cur)] = 1;
        nodes.push_back(cur);
    }
    return nodes;
}

int WalkLengthProblem::answer(const std::vector<int>& input) const {
    bool cyclic = false;
    std::vector<int> nodes = walk(input, cyclic);
    return cyclic ? static_cast<int>(input.size()) : static_cast<int>(nodes.size());
}

int VisitsZeroProblem::answer(const std::vector<int>& input) const {
    bool cyclic = false;
    std::vector<int> nodes = walk(input, cyclic);
    return std::find(nodes.begin(), nodes.end(), 0) != nodes.end() ? 1 : 0;
}

int CycleLengthProblem::answer(const std::vector<int>& input) const {
    bool cyclic = false;
    std::vector<int> nodes = walk(input, cyclic);
    if (!cyclic) return 0;
    int entry = input[static_cast<size_t>(1 + nodes.back())];  // the repeated node
    return static_cast<int>(nodes.end() - std::find(nodes.begin(), nodes.end(), entry));
}

int LastNodeProblem::answer(const std::vector<int>& input) const {
    bool cyclic = false;
    std::vector<int> nodes = walk(input, cyclic);
    int n = static_cast<int>(input.size()) - 1;
    return (cyclic || nodes.empty()) ? n : nodes.back();
}

int CycleEntryProblem::answer(const std::vector<int>& input) const {
    bool cyclic = false;
    std::vector<int> nodes = walk(input, cyclic);
    int n = static_cast<int>(input.size()) - 1;
    return cyclic ? input[static_cast<size_t>(1 + nodes.back())] : n;
}

int SecondLastNodeProblem::answer(const std::vector<int>& input) const {
    bool cyclic = false;
    std::vector<int> nodes = walk(input, cyclic);
    int n = static_cast<int>(input.size()) - 1;
    return (cyclic || nodes.size() < 2) ? n : nodes[nodes.size() - 2];
}

}  // namespace algosearch
