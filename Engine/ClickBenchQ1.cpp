#include "Execution/Queries.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace {
std::filesystem::path DefaultHitsSamplePath() {
    std::filesystem::path current = std::filesystem::current_path();
    for (size_t depth = 0; depth < 8; ++depth) {
        const std::filesystem::path candidate = current / "FileBasicTools/src/Tests/hits_sample.belZ";
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }

        const std::filesystem::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    return "FileBasicTools/src/Tests/hits_sample.belZ";
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc > 2) {
        std::cerr << "Usage: clickbench_q1 [path/to/hits_sample.belZ]\n";
        return 1;
    }

    try {
        const std::string table_name = argc == 2 ? argv[1] : DefaultHitsSamplePath().string();
        std::cout << ClickBench::RunFirstQueryCount(table_name) << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
