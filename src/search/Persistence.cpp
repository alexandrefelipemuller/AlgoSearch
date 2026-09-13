#include "search/Persistence.hpp"

#include <fstream>
#include <sstream>

namespace algosearch {

namespace {

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
            << "    \"program\": \"" << jsonEscape(r.programText) << "\",\n"
            << "    \"program_size\": " << r.programSize << ",\n"
            << "    \"level_reached\": " << r.levelReached << ",\n"
            << "    \"n_reached\": " << r.nReached << ",\n"
            << "    \"score\": " << r.score << ",\n"
            << "    \"total_tests\": " << r.totalTests << ",\n"
            << "    \"final_status\": \"" << jsonEscape(r.finalStatus) << "\",\n"
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
