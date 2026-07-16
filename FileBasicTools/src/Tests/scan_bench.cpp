#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <iostream>

#include "BelZReader.h"
#include "CSVWriter.h"

namespace {

std::filesystem::path TestDir() {
    return std::filesystem::path(__FILE__).parent_path();
}

} // namespace

TEST(ScanBench, BelZToCsv) {
    const auto input_path = TestDir() / "hits_sample.belZ";
    const auto output_path = TestDir() / "hits_test.csv";

    BelZReader reader(input_path.string());
    CSVWriter writer(output_path, CSVWriter::PathMode::ExactPath);

    size_t batches = 0;
    size_t rows = 0;
    const auto started = std::chrono::steady_clock::now();

    while (!reader.Empty()) {
        Batch batch;
        reader.ReadBatch(batch);
        rows += batch.GetRows();
        writer.WriteBatch(batch);
        ++batches;
    }
    writer.Flush();

    const auto finished = std::chrono::steady_clock::now();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(finished - started).count();

    std::cout << "scan batches: " << batches << '\n';
    std::cout << "scan rows: " << rows << '\n';
    std::cout << "scan elapsed ms: " << elapsed_ms << '\n';

    EXPECT_GT(rows, 0u);
}
