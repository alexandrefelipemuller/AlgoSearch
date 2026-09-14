#include "search/Persistence.hpp"

#include <fstream>
#include <sstream>

namespace algosearch {

namespace {

// "X/Y" ratio formatting matching the human-readable "score 1/1" style
// used elsewhere in this project's logging -- used for training_score and
// heldout_score so they read the same way in both places. "?/?" when the
// denominator isn't meaningful (e.g. no adversarial instances existed).
std::string ratioString(int num, int den) {
    if (den < 0) return "?/?";
    return std::to_string(num) + "/" + std::to_string(den);
}

std::string jsonEscape(const std::string& s) {
    std::ostringstream out;
    for (char c : s) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\t': out << "\\t"; break;
            default: out << c;
        }
    }
    return out.str();
}

}  // namespace

void saveSolutions(const std::string& path, const std::vector<SolutionRecord>& records) {
    std::ofstream out(path);
    out << "[\n";
    for (size_t i = 0; i < records.size(); ++i) {
        const auto& r = records[i];
        out << "  {\n"
            << "    \"program_id\": \"" << jsonEscape(r.programId) << "\",\n"
            << "    \"program\": \"" << jsonEscape(r.programText) << "\",\n"
            << "    \"program_size\": " << r.programSize << ",\n"
            << "    \"level_found\": " << r.levelReached << ",\n"
            << "    \"level_reached\": " << r.levelReached << ",\n"
            << "    \"n_reached\": " << r.nReached << ",\n"
            << "    \"first_level\": " << r.firstLevel << ",\n"
            << "    \"confirmed_levels\": " << r.confirmedLevels << ",\n"
            << "    \"score\": " << r.score << ",\n"
            << "    \"total_tests\": " << r.totalTests << ",\n"
            << "    \"training_score\": \"" << ratioString(r.score, r.totalTests) << "\",\n"
            << "    \"heldout_score\": \"" << ratioString(r.adversarialPassed, r.adversarialTotal) << "\",\n"
            << "    \"final_status\": \"" << jsonEscape(r.finalStatus) << "\",\n"
            << "    \"adversarial_passed\": " << r.adversarialPassed << ",\n"
            << "    \"adversarial_total\": " << r.adversarialTotal << ",\n"
            << "    \"beam_diversity\": " << r.beamDiversity << ",\n"
            << "    \"beam_size\": " << r.beamSize << ",\n"
            << "    \"seed\": " << r.seed << ",\n"
            << "    \"config\": {\n";
        for (size_t j = 0; j < r.config.size(); ++j) {
            out << "      \"" << jsonEscape(r.config[j].first) << "\": \"" << jsonEscape(r.config[j].second) << "\"";
            if (j + 1 < r.config.size()) out << ",";
            out << "\n";
        }
        out << "    }\n";
        out << "  }";
        if (i + 1 < records.size()) out << ",";
        out << "\n";
    }
    out << "]\n";
}

}  // namespace algosearch
