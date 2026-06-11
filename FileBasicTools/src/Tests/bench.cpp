#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "CSVConvertor.h"
namespace fs = std::filesystem;

namespace {
fs::path FindFileBasicToolsRoot() {
    fs::path current = fs::current_path();
    for (size_t depth = 0; depth < 8; ++depth) {
        if (fs::exists(current / "src/Tests/hits_sample.csv") && fs::exists(current / "src/Tests/hits_scheme.csv")) {
            return current;
        }
        if (fs::exists(current / "FileBasicTools/src/Tests/hits_sample.csv") &&
            fs::exists(current / "FileBasicTools/src/Tests/hits_scheme.csv")) {
            return current / "FileBasicTools";
        }

        const fs::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    return fs::current_path();
}
} // namespace

TEST(CSVConvertor, hits_sample) {
    CSVConvertor engine;
    const fs::path project_root = FindFileBasicToolsRoot();
    fs::path data_path = project_root / "src/Tests/hits_sample.csv";
    fs::path scheme_path = project_root / "src/Tests/hits_scheme.csv";

    ASSERT_TRUE(fs::exists(data_path)) << "Missing benchmark data: " << data_path;
    ASSERT_TRUE(fs::exists(scheme_path)) << "Missing benchmark scheme: " << scheme_path;

    engine.MakeBelZFormat(data_path.string(), scheme_path.string());
}
