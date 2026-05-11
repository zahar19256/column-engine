#include "../Execution/Executor.h"
#include "../Execution/Queries.h"
#include "../../FileBasicTools/src/CSVConvertor.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>

namespace {

class SingleBatchExecutor : public Executor {
public:
    explicit SingleBatchExecutor(Batch batch) : batch_(std::move(batch)) {
    }

    bool Next(Batch& data) override {
        if (done_) {
            return false;
        }

        data = batch_;
        done_ = true;
        return true;
    }

private:
    Batch batch_;
    bool done_ = false;
};

Batch MakeInt64Batch(boost::dynamic_bitset<> mask = boost::dynamic_bitset<>()) {
    Scheme scheme;
    scheme.Push_Back(SchemeNode{"value", ColumnType::Int64});

    auto values = std::make_shared<Int64Column>();
    values->Push_Back(10);
    values->Push_Back(20);
    values->Push_Back(30);

    Batch batch;
    batch.SetScheme(scheme);
    batch.AddColumn(values);
    if (mask.empty()) {
        batch.InitMsk();
    } else {
        batch.SetMsk(std::move(mask));
    }
    return batch;
}

int64_t RunSingleInt64Aggregation(Aggregation::AggregationCall call, Batch input) {
    const std::string output_name = call.new_name.value_or("");

    AggregationExecutor executor(call);
    executor.child = std::make_shared<SingleBatchExecutor>(std::move(input));

    Batch output;
    EXPECT_TRUE(executor.Next(output));
    EXPECT_FALSE(executor.Next(output));
    EXPECT_EQ(output.GetRows(), 1);

    auto result = As<Int64Column>(output.GetColumn(output_name));
    EXPECT_NE(result, nullptr);
    return result->At(0);
}

} // namespace

TEST(AggregationExecutor, SumsInputBatch) {
    Aggregation::AggregationCall call{
        .type = Aggregation::AggregationType::Sum,
        .column_name = "value",
        .new_name = "total",
        .input_type = ColumnType::Int64,
        .output_type = ColumnType::Int64,
    };

    EXPECT_EQ(RunSingleInt64Aggregation(call, MakeInt64Batch()), 60);
}

TEST(AggregationExecutor, AppliesInputMask) {
    Aggregation::AggregationCall call{
        .type = Aggregation::AggregationType::Sum,
        .column_name = "value",
        .new_name = "masked_total",
        .input_type = ColumnType::Int64,
        .output_type = ColumnType::Int64,
    };

    boost::dynamic_bitset<> mask(3);
    mask[0] = true;
    mask[2] = true;

    EXPECT_EQ(RunSingleInt64Aggregation(call, MakeInt64Batch(mask)), 40);
}

TEST(AggregationExecutor, ComputesMinMaxAvgAndCount) {
    EXPECT_EQ(RunSingleInt64Aggregation({
                  .type = Aggregation::AggregationType::Min,
                  .column_name = "value",
                  .new_name = "min_value",
                  .input_type = ColumnType::Int64,
                  .output_type = ColumnType::Int64,
              },
              MakeInt64Batch()),
              10);

    EXPECT_EQ(RunSingleInt64Aggregation({
                  .type = Aggregation::AggregationType::Max,
                  .column_name = "value",
                  .new_name = "max_value",
                  .input_type = ColumnType::Int64,
                  .output_type = ColumnType::Int64,
              },
              MakeInt64Batch()),
              30);

    EXPECT_EQ(RunSingleInt64Aggregation({
                  .type = Aggregation::AggregationType::Avg,
                  .column_name = "value",
                  .new_name = "avg_value",
                  .input_type = ColumnType::Int64,
                  .output_type = ColumnType::Int64,
              },
              MakeInt64Batch()),
              20);

    EXPECT_EQ(RunSingleInt64Aggregation({
                  .type = Aggregation::AggregationType::Count,
                  .column_name = "value",
                  .new_name = "count_value",
                  .input_type = ColumnType::Int64,
                  .output_type = ColumnType::Int64,
              },
              MakeInt64Batch()),
              3);
}

TEST(ClickBenchQueries, FirstQueryCountsRowsWithoutReadingColumns) {
    const std::filesystem::path csv_path = std::filesystem::temp_directory_path() / "column_engine_clickbench_q1.csv";
    std::filesystem::path belz_path = csv_path;
    belz_path.replace_extension(".belZ");

    {
        std::ofstream out(csv_path);
        ASSERT_TRUE(out.is_open());
        out << "1\n2\n3\n4\n";
    }

    Scheme scheme;
    scheme.Push_Back(SchemeNode{"id", ColumnType::Int64});

    CSVConvertor convertor;
    convertor.SetScheme(scheme);
    convertor.MakeBelZFormat(csv_path.string(), "BENCHTIME.GO");

    EXPECT_EQ(ClickBench::RunFirstQueryCount(belz_path.string()), 4);

    BelZReader reader(belz_path.string());
    Batch empty_projection;
    reader.ReadBatch(empty_projection, {});
    EXPECT_EQ(empty_projection.GetRows(), 4);
    EXPECT_EQ(empty_projection.Size(), 0);

    std::filesystem::remove(csv_path);
    std::filesystem::remove(belz_path);
}

TEST(ClickBenchQueries, SecondQueryCountsRowsWithNonZeroAdvEngineID) {
    const std::filesystem::path csv_path = std::filesystem::temp_directory_path() / "column_engine_clickbench_q2.csv";
    std::filesystem::path belz_path = csv_path;
    belz_path.replace_extension(".belZ");

    {
        std::ofstream out(csv_path);
        ASSERT_TRUE(out.is_open());
        out << "0\n";
        out << "1\n";
        out << "2\n";
        out << "0\n";
        out << "3\n";
    }

    Scheme scheme;
    scheme.Push_Back(SchemeNode{"AdvEngineID", ColumnType::Int64});

    CSVConvertor convertor;
    convertor.SetScheme(scheme);
    convertor.MakeBelZFormat(csv_path.string(), "BENCHTIME.GO");

    EXPECT_EQ(ClickBench::RunSecondQueryCount(belz_path.string()), 3);

    std::filesystem::remove(csv_path);
    std::filesystem::remove(belz_path);
}
