#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstring>

#include "CSVConvertor.h"
#include "Scheme.h"
namespace fs = std::filesystem;

TEST(CSVConvertor, hits_sample) {
    CSVConvertor engine;
    Scheme scheme;
    for (size_t i = 0; i < 105; ++i) {
        SchemeNode current;
        current.type = ColumnType::String;
        current.name = "OK";
        scheme.Push_Back(current);
    }
    engine.SetScheme(scheme);
    const fs::path project_root = fs::current_path().parent_path();
    fs::path data_path = project_root / "src/Tests/hits_sample.csv";
    engine.MakeBelZFormat(data_path.string(), "BENCHTIME.GO");
}