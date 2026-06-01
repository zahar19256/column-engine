#include "../Execution/Executor.h"
#include "../Execution/Queries.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <unordered_map>

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

Batch MakeDateBatch() {
    Scheme scheme;
    scheme.Push_Back(SchemeNode{"event_date", ColumnType::Date});

    auto values = std::make_shared<DateColumn>();
    values->Push_Back(Data::ParseDate("2013-07-17"));
    values->Push_Back(Data::ParseDate("2013-07-15"));
    values->Push_Back(Data::ParseDate("2013-07-19"));

    Batch batch;
    batch.SetScheme(scheme);
    batch.AddColumn(values);
    batch.InitMsk();
    return batch;
}

Batch MakeMixedBatch() {
    Scheme scheme;
    scheme.Push_Back(SchemeNode{"value", ColumnType::Int64});
    scheme.Push_Back(SchemeNode{"name", ColumnType::String});

    auto values = std::make_shared<Int64Column>();
    values->Push_Back(10);
    values->Push_Back(20);
    values->Push_Back(30);
    values->Push_Back(40);

    auto names = std::make_shared<StringColumn>();
    names->Push_Back("a");
    names->Push_Back("b");
    names->Push_Back("c");
    names->Push_Back("d");

    Batch batch;
    batch.SetScheme(scheme);
    batch.AddColumn(values);
    batch.AddColumn(names);
    batch.InitMsk();
    return batch;
}

Batch MakeStringBatch() {
    Scheme scheme;
    scheme.Push_Back(SchemeNode{"name", ColumnType::String});

    auto names = std::make_shared<StringColumn>();
    names->Push_Back("");
    names->Push_Back("abc");
    names->Push_Back("x");

    Batch batch;
    batch.SetScheme(scheme);
    batch.AddColumn(names);
    batch.InitMsk();
    return batch;
}

Batch MakeGroupByBatch() {
    Scheme scheme;
    scheme.Push_Back(SchemeNode{"key", ColumnType::Int64});
    scheme.Push_Back(SchemeNode{"value", ColumnType::Int64});

    auto keys = std::make_shared<Int64Column>();
    keys->Push_Back(1);
    keys->Push_Back(2);
    keys->Push_Back(1);
    keys->Push_Back(2);
    keys->Push_Back(1);

    auto values = std::make_shared<Int64Column>();
    values->Push_Back(10);
    values->Push_Back(7);
    values->Push_Back(20);
    values->Push_Back(3);
    values->Push_Back(5);

    Batch batch;
    batch.SetScheme(scheme);
    batch.AddColumn(keys);
    batch.AddColumn(values);
    batch.InitMsk();
    return batch;
}

std::filesystem::path HitsSampleBelZPath() {
    std::filesystem::path current = std::filesystem::current_path();
    for (size_t depth = 0; depth < 8; ++depth) {
        const std::filesystem::path direct = current / "FileBasicTools/src/Tests/hits_sample.belZ";
        if (std::filesystem::exists(direct)) {
            return direct;
        }

        const std::filesystem::path from_engine_build = current / "../FileBasicTools/src/Tests/hits_sample.belZ";
        if (std::filesystem::exists(from_engine_build)) {
            return std::filesystem::weakly_canonical(from_engine_build);
        }

        const std::filesystem::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    return "FileBasicTools/src/Tests/hits_sample.belZ";
}

int64_t RunSingleInt64Aggregation(Aggregation::AggregationCall call, Batch input) {
    const std::string output_name = call.new_name.value_or("");

    GlobalAggregationExecutor executor({std::move(call)});
    executor.child = std::make_shared<SingleBatchExecutor>(std::move(input));

    Batch output;
    EXPECT_TRUE(executor.Next(output));
    EXPECT_FALSE(executor.Next(output));
    EXPECT_EQ(output.GetRows(), 1);

    auto result = As<Int64Column>(output.GetColumn(output_name));
    EXPECT_NE(result, nullptr);
    return result->At(0);
}

int64_t RunSingleDateAggregation(Aggregation::AggregationCall call, Batch input) {
    const std::string output_name = call.new_name.value_or("");

    GlobalAggregationExecutor executor({std::move(call)});
    executor.child = std::make_shared<SingleBatchExecutor>(std::move(input));

    Batch output;
    EXPECT_TRUE(executor.Next(output));
    EXPECT_FALSE(executor.Next(output));
    EXPECT_EQ(output.GetRows(), 1);

    auto result = As<DateColumn>(output.GetColumn(output_name));
    EXPECT_NE(result, nullptr);
    return result->At(0);
}

} // namespace

TEST(AggregationExecutor, SumsInputBatch) {
    Aggregation::AggregationCall call{
        .type = Aggregation::AggregationType::Sum,
        .expression = MakeColumnExpr("value", ColumnType::Int64),
        .new_name = "total",
        .input_type = ColumnType::Int64,
        .output_type = ColumnType::Int64,
    };

    EXPECT_EQ(RunSingleInt64Aggregation(call, MakeInt64Batch()), 60);
}

TEST(Column, AssignFillsTypedColumns) {
    Int8Column int8_values;
    int8_values.Assign(3, static_cast<int8_t>(7));
    ASSERT_EQ(int8_values.Size(), 3);
    EXPECT_EQ(int8_values.At(0), 7);
    EXPECT_EQ(int8_values.At(2), 7);

    Int16Column int16_values;
    int16_values.Assign(2, static_cast<int16_t>(300));
    ASSERT_EQ(int16_values.Size(), 2);
    EXPECT_EQ(int16_values.At(1), 300);

    Int32Column int32_values;
    int32_values.Assign(2, 40000);
    ASSERT_EQ(int32_values.Size(), 2);
    EXPECT_EQ(int32_values.At(1), 40000);

    Int64Column int64_values;
    int64_values.Assign(2, 5000000000LL);
    ASSERT_EQ(int64_values.Size(), 2);
    EXPECT_EQ(int64_values.At(1), 5000000000LL);

    Int128Column int128_values;
    int128_values.Assign(2, static_cast<__int128_t>(6000000000LL));
    ASSERT_EQ(int128_values.Size(), 2);
    EXPECT_EQ(int128_values.At(1), static_cast<__int128_t>(6000000000LL));

    DoubleColumn double_values;
    double_values.Assign(2, 1.5);
    ASSERT_EQ(double_values.Size(), 2);
    EXPECT_DOUBLE_EQ(double_values.At(1), 1.5);

    StringColumn string_values;
    string_values.Assign(2, "literal");
    ASSERT_EQ(string_values.Size(), 2);
    EXPECT_EQ(string_values.At(1), "literal");

    DateColumn date_values;
    date_values.Assign(2, Data::ParseDate("2013-07-15"));
    ASSERT_EQ(date_values.Size(), 2);
    EXPECT_EQ(date_values.At(1), Data::ParseDate("2013-07-15"));

    TimeStampColumn timestamp_values;
    timestamp_values.Assign(2, Data::ParseTimestamp("2013-07-15 10:11:12"));
    ASSERT_EQ(timestamp_values.Size(), 2);
    EXPECT_EQ(timestamp_values.At(1), Data::ParseTimestamp("2013-07-15 10:11:12"));
}

TEST(LiteralExpr, EvalBatchReturnsFilledTypedColumn) {
    LiteralExpr literal(int64_t{42}, ColumnType::Int16);

    const auto result = As<Int16Column>(literal.EvalBatch(MakeInt64Batch()));
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->Size(), 3);
    EXPECT_EQ(result->At(0), 42);
    EXPECT_EQ(result->At(2), 42);
}

TEST(LiteralExpr, InfersTypeAndRebuildsColumnPerBatch) {
    LiteralExpr string_literal(std::string("x"));

    const auto first = As<StringColumn>(string_literal.EvalBatch(MakeInt64Batch()));
    ASSERT_NE(first, nullptr);
    ASSERT_EQ(first->Size(), 3);
    EXPECT_EQ(first->At(0), "x");
    EXPECT_EQ(first->At(2), "x");

    boost::dynamic_bitset<> mask(1);
    mask.set();
    const auto second = As<StringColumn>(string_literal.EvalBatch(MakeInt64Batch(mask)));
    ASSERT_NE(second, nullptr);
    ASSERT_EQ(second->Size(), 3);
    EXPECT_EQ(second->At(1), "x");
}

TEST(UnaryExpr, LengthReturnsStringSizes) {
    auto length_expr = MakeLengthExpr(MakeColumnExpr("name", ColumnType::String));

    EXPECT_EQ(length_expr->GetType(), ColumnType::Int64);
    const auto result = As<Int64Column>(length_expr->EvalBatch(MakeStringBatch()));
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->Size(), 3);
    EXPECT_EQ(result->At(0), 0);
    EXPECT_EQ(result->At(1), 3);
    EXPECT_EQ(result->At(2), 1);
}

TEST(UnaryExpr, RegexpReplaceRewritesStringColumn) {
    Scheme scheme;
    scheme.Push_Back(SchemeNode{"referer", ColumnType::String});

    auto referers = std::make_shared<StringColumn>();
    referers->Push_Back("https://www.example.com/path");
    referers->Push_Back("http://openai.com/docs");
    referers->Push_Back("plain");

    Batch batch;
    batch.SetScheme(scheme);
    batch.AddColumn(referers);
    batch.InitMsk();

    const auto expression = MakeRegexpReplaceExpr(
        MakeColumnExpr("referer", ColumnType::String),
        R"(^https?://(?:www\.)?([^/]+)/.*$)",
        R"(\1)");

    EXPECT_EQ(expression->GetType(), ColumnType::String);
    const auto result = As<StringColumn>(expression->EvalBatch(batch));
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->Size(), 3);
    EXPECT_EQ(result->At(0), "example.com");
    EXPECT_EQ(result->At(1), "openai.com");
    EXPECT_EQ(result->At(2), "plain");
}

TEST(BinaryExpr, SubtractsLiteralFromColumn) {
    auto sub_expr = MakeSubExpr(
        MakeColumnExpr("value", ColumnType::Int64),
        MakeLiteralExpr(int64_t{3}, ColumnType::Int64),
        ColumnType::Int64);

    EXPECT_EQ(sub_expr->GetType(), ColumnType::Int64);
    const auto result = As<Int64Column>(sub_expr->EvalBatch(MakeInt64Batch()));
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->Size(), 3);
    EXPECT_EQ(result->At(0), 7);
    EXPECT_EQ(result->At(1), 17);
    EXPECT_EQ(result->At(2), 27);
}

TEST(CaseWhenExpr, SelectsFirstMatchingBranchAndElse) {
    std::vector<std::pair<std::shared_ptr<PredicateExpr> , std::shared_ptr<ScalarExpr>>> cases;
    cases.emplace_back(
        std::shared_ptr<PredicateExpr>(MakeFilter({"value" , "15" , Filters::OpType::Greater})) ,
        MakeColumnExpr("name", ColumnType::String));

    auto case_expr = MakeCaseWhenExpr(
        std::move(cases),
        MakeLiteralExpr(std::string(""), ColumnType::String),
        ColumnType::String);

    EXPECT_EQ(case_expr->GetType(), ColumnType::String);
    const auto result = As<StringColumn>(case_expr->EvalBatch(MakeMixedBatch()));
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(result->Size(), 4);
    EXPECT_EQ(result->At(0), "");
    EXPECT_EQ(result->At(1), "b");
    EXPECT_EQ(result->At(2), "c");
    EXPECT_EQ(result->At(3), "d");
}

TEST(AggregationExecutor, AppliesInputMask) {
    Aggregation::AggregationCall call{
        .type = Aggregation::AggregationType::Sum,
        .expression = MakeColumnExpr("value", ColumnType::Int64),
        .new_name = "masked_total",
        .input_type = ColumnType::Int64,
        .output_type = ColumnType::Int64,
    };

    boost::dynamic_bitset<> mask(3);
    mask[0] = true;
    mask[2] = true;

    EXPECT_EQ(RunSingleInt64Aggregation(call, MakeInt64Batch(mask)), 40);
}

TEST(AggregationExecutor, SumsScalarExpression) {
    Aggregation::AggregationCall call{
        .type = Aggregation::AggregationType::Sum,
        .expression = MakeLiteralExpr(int64_t{5}, ColumnType::Int64),
        .new_name = "literal_sum",
        .input_type = ColumnType::Int64,
        .output_type = ColumnType::Int64,
    };

    EXPECT_EQ(RunSingleInt64Aggregation(call, MakeInt64Batch()), 15);
}

TEST(AggregationExecutor, SumsLengthExpression) {
    Aggregation::AggregationCall call{
        .type = Aggregation::AggregationType::Sum,
        .expression = MakeLengthExpr(MakeColumnExpr("name", ColumnType::String)),
        .new_name = "name_length_sum",
        .input_type = ColumnType::Int64,
        .output_type = ColumnType::Int64,
    };

    EXPECT_EQ(RunSingleInt64Aggregation(call, MakeStringBatch()), 4);
}

TEST(AggregationExecutor, ComputesMinMaxAvgAndCount) {
    EXPECT_EQ(RunSingleInt64Aggregation({
                  .type = Aggregation::AggregationType::Min,
                  .expression = MakeColumnExpr("value", ColumnType::Int64),
                  .new_name = "min_value",
                  .input_type = ColumnType::Int64,
                  .output_type = ColumnType::Int64,
              },
              MakeInt64Batch()),
              10);

    EXPECT_EQ(RunSingleInt64Aggregation({
                  .type = Aggregation::AggregationType::Max,
                  .expression = MakeColumnExpr("value", ColumnType::Int64),
                  .new_name = "max_value",
                  .input_type = ColumnType::Int64,
                  .output_type = ColumnType::Int64,
              },
              MakeInt64Batch()),
              30);

    EXPECT_EQ(RunSingleInt64Aggregation({
                  .type = Aggregation::AggregationType::Avg,
                  .expression = MakeColumnExpr("value", ColumnType::Int64),
                  .new_name = "avg_value",
                  .input_type = ColumnType::Int64,
                  .output_type = ColumnType::Int64,
              },
              MakeInt64Batch()),
              20);

    EXPECT_EQ(RunSingleInt64Aggregation({
                  .type = Aggregation::AggregationType::Count,
                  .expression = MakeColumnExpr("value", ColumnType::Int64),
                  .new_name = "count_value",
                  .input_type = ColumnType::Int64,
                  .output_type = ColumnType::Int64,
              },
              MakeInt64Batch()),
              3);
}

TEST(AggregationExecutor, ComputesMinMaxForDateColumns) {
    EXPECT_EQ(RunSingleDateAggregation({
                  .type = Aggregation::AggregationType::Min,
                  .expression = MakeColumnExpr("event_date", ColumnType::Date),
                  .new_name = "min_date",
                  .input_type = ColumnType::Date,
                  .output_type = ColumnType::Unknown,
              },
              MakeDateBatch()),
              Data::ParseDate("2013-07-15"));

    EXPECT_EQ(RunSingleDateAggregation({
                  .type = Aggregation::AggregationType::Max,
                  .expression = MakeColumnExpr("event_date", ColumnType::Date),
                  .new_name = "max_date",
                  .input_type = ColumnType::Date,
                  .output_type = ColumnType::Unknown,
              },
              MakeDateBatch()),
              Data::ParseDate("2013-07-19"));
}

TEST(GroupByAggregationExecutor, GroupsByColumnAndComputesAggregates) {
    std::vector<std::shared_ptr<ScalarExpr>> group_by;
    group_by.push_back(MakeColumnExpr("key", ColumnType::Int64));

    std::vector<Aggregation::AggregationCall> calls;
    calls.push_back({
        .type = Aggregation::AggregationType::Count,
        .expression = nullptr,
        .new_name = "row_count",
        .input_type = ColumnType::Unknown,
        .output_type = ColumnType::Int64,
    });
    calls.push_back({
        .type = Aggregation::AggregationType::Sum,
        .expression = MakeColumnExpr("value", ColumnType::Int64),
        .new_name = "value_sum",
        .input_type = ColumnType::Int64,
        .output_type = ColumnType::Int64,
    });

    GroupByAggregationExecutor executor(std::move(group_by), std::move(calls));
    executor.child = std::make_shared<SingleBatchExecutor>(MakeGroupByBatch());

    Batch output;
    EXPECT_TRUE(executor.Next(output));
    EXPECT_FALSE(executor.Next(output));
    EXPECT_EQ(output.GetRows(), 2);

    auto keys = As<Int64Column>(output.GetColumn("key"));
    auto counts = As<Int64Column>(output.GetColumn("row_count"));
    auto sums = As<Int64Column>(output.GetColumn("value_sum"));
    ASSERT_NE(keys, nullptr);
    ASSERT_NE(counts, nullptr);
    ASSERT_NE(sums, nullptr);

    std::unordered_map<int64_t, std::pair<int64_t, int64_t>> grouped;
    for (size_t row = 0; row < output.GetRows(); ++row) {
        grouped[keys->At(row)] = {counts->At(row), sums->At(row)};
    }

    ASSERT_EQ(grouped.size(), 2);
    const std::pair<int64_t, int64_t> expected_key_1{3, 35};
    const std::pair<int64_t, int64_t> expected_key_2{2, 10};
    EXPECT_EQ(grouped[1], expected_key_1);
    EXPECT_EQ(grouped[2], expected_key_2);
}

TEST(ProjectionExecutor, ProjectsColumnsWithMatchingSchemeAndAliases) {
    Scheme scheme;
    scheme.Push_Back(SchemeNode{"a", ColumnType::Int64});
    scheme.Push_Back(SchemeNode{"b", ColumnType::Int64});

    auto a = std::make_shared<Int64Column>();
    a->Push_Back(10);
    a->Push_Back(20);

    auto b = std::make_shared<Int64Column>();
    b->Push_Back(100);
    b->Push_Back(200);

    Batch input;
    input.SetScheme(scheme);
    input.AddColumn(a);
    input.AddColumn(b);
    input.InitMsk();

    ProjectionExecutor executor({"b"}, {{"b", "answer"}});
    executor.child = std::make_shared<SingleBatchExecutor>(std::move(input));

    Batch output;
    ASSERT_TRUE(executor.Next(output));
    EXPECT_FALSE(executor.Next(output));
    EXPECT_EQ(output.Size(), 1);
    EXPECT_EQ(output.GetRows(), 2);
    EXPECT_EQ(output.GetScheme().Size(), 1);
    EXPECT_EQ(output.GetScheme().GetName(0), "answer");

    auto projected = As<Int64Column>(output.GetColumn("b"));
    auto alias = As<Int64Column>(output.GetColumn("answer"));
    ASSERT_NE(projected, nullptr);
    ASSERT_NE(alias, nullptr);
    EXPECT_EQ(projected->At(0), 100);
    EXPECT_EQ(alias->At(1), 200);
}

TEST(LimitExecutor, AppliesOffsetAndLimitInsideBatch) {
    LimitExecutor executor(2, 1);
    executor.child = std::make_shared<SingleBatchExecutor>(MakeMixedBatch());

    Batch output;
    ASSERT_TRUE(executor.Next(output));
    EXPECT_FALSE(executor.Next(output));
    EXPECT_EQ(output.GetRows(), 2);
    EXPECT_EQ(output.GetMsk().count(), 2);

    auto values = As<Int64Column>(output.GetColumn("value"));
    auto names = As<StringColumn>(output.GetColumn("name"));
    ASSERT_NE(values, nullptr);
    ASSERT_NE(names, nullptr);

    EXPECT_EQ(values->At(0), 20);
    EXPECT_EQ(values->At(1), 30);
    EXPECT_EQ(names->At(0), "b");
    EXPECT_EQ(names->At(1), "c");
}

TEST(OrderByExecutor, UsesEquivalenceClassesAcrossSortKeys) {
    Scheme scheme;
    scheme.Push_Back(SchemeNode{"a", ColumnType::Int64});
    scheme.Push_Back(SchemeNode{"b", ColumnType::Int64});

    auto first_key = std::make_shared<Int64Column>();
    first_key->Push_Back(1);
    first_key->Push_Back(1);
    first_key->Push_Back(1);
    first_key->Push_Back(2);

    auto second_key = std::make_shared<Int64Column>();
    second_key->Push_Back(10);
    second_key->Push_Back(30);
    second_key->Push_Back(20);
    second_key->Push_Back(99);

    Batch input;
    input.SetScheme(scheme);
    input.AddColumn(first_key);
    input.AddColumn(second_key);
    input.InitMsk();

    OrderByExecutor executor(
        {MakeColumnExpr("a", ColumnType::Int64), MakeColumnExpr("b", ColumnType::Int64)},
        2,
        {SortDirection::Asc, SortDirection::Desc});
    executor.child = std::make_shared<SingleBatchExecutor>(std::move(input));

    Batch output;
    ASSERT_TRUE(executor.Next(output));
    EXPECT_FALSE(executor.Next(output));
    EXPECT_EQ(output.GetRows(), 2);

    auto a = As<Int64Column>(output.GetColumn("a"));
    auto b = As<Int64Column>(output.GetColumn("b"));
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    EXPECT_EQ(a->At(0), 1);
    EXPECT_EQ(b->At(0), 30);
    EXPECT_EQ(a->At(1), 1);
    EXPECT_EQ(b->At(1), 20);
}

TEST(FilterExecutor, LikeMatchesSqlWildcards) {
    auto values = std::make_shared<StringColumn>();
    values->Push_Back("https://example.com/search");
    values->Push_Back("http://example.com/search");
    values->Push_Back("https://example.com/about");

    const boost::dynamic_bitset<> result = Filters::ColumnTypedFilter(
        *values,
        Filters::OpType::Like,
        std::string_view("https://%/search"));

    ASSERT_EQ(result.size(), 3);
    EXPECT_TRUE(result[0]);
    EXPECT_FALSE(result[1]);
    EXPECT_FALSE(result[2]);
}

TEST(FilterExecutor, LikeSupportsSingleCharacterWildcardAndEscapes) {
    auto values = std::make_shared<StringColumn>();
    values->Push_Back("ab");
    values->Push_Back("ac");
    values->Push_Back("a_");
    values->Push_Back("a%");

    const boost::dynamic_bitset<> single_char = Filters::ColumnTypedFilter(
        *values,
        Filters::OpType::Like,
        std::string_view("a_"));
    ASSERT_EQ(single_char.size(), 4);
    EXPECT_TRUE(single_char[0]);
    EXPECT_TRUE(single_char[1]);
    EXPECT_TRUE(single_char[2]);
    EXPECT_TRUE(single_char[3]);

    const boost::dynamic_bitset<> escaped_underscore = Filters::ColumnTypedFilter(
        *values,
        Filters::OpType::Like,
        std::string_view("a\\_"));
    ASSERT_EQ(escaped_underscore.size(), 4);
    EXPECT_FALSE(escaped_underscore[0]);
    EXPECT_FALSE(escaped_underscore[1]);
    EXPECT_TRUE(escaped_underscore[2]);
    EXPECT_FALSE(escaped_underscore[3]);

    const boost::dynamic_bitset<> escaped_percent = Filters::ColumnTypedFilter(
        *values,
        Filters::OpType::Like,
        std::string_view("a\\%"));
    ASSERT_EQ(escaped_percent.size(), 4);
    EXPECT_FALSE(escaped_percent[0]);
    EXPECT_FALSE(escaped_percent[1]);
    EXPECT_FALSE(escaped_percent[2]);
    EXPECT_TRUE(escaped_percent[3]);
}

TEST(FilterExecutor, LikeUsesSimpleStringFastPaths) {
    auto values = std::make_shared<StringColumn>();
    values->Push_Back("https://google.com/search");
    values->Push_Back("https://example.com/google");
    values->Push_Back("http://example.com");
    values->Push_Back("plain");

    const boost::dynamic_bitset<> contains = Filters::ColumnTypedFilter(
        *values,
        Filters::OpType::Like,
        std::string_view("%google%"));
    ASSERT_EQ(contains.size(), 4);
    EXPECT_TRUE(contains[0]);
    EXPECT_TRUE(contains[1]);
    EXPECT_FALSE(contains[2]);
    EXPECT_FALSE(contains[3]);

    const boost::dynamic_bitset<> starts_with = Filters::ColumnTypedFilter(
        *values,
        Filters::OpType::Like,
        std::string_view("https://%"));
    ASSERT_EQ(starts_with.size(), 4);
    EXPECT_TRUE(starts_with[0]);
    EXPECT_TRUE(starts_with[1]);
    EXPECT_FALSE(starts_with[2]);
    EXPECT_FALSE(starts_with[3]);

    const boost::dynamic_bitset<> ends_with = Filters::ColumnTypedFilter(
        *values,
        Filters::OpType::Like,
        std::string_view("%google"));
    ASSERT_EQ(ends_with.size(), 4);
    EXPECT_FALSE(ends_with[0]);
    EXPECT_TRUE(ends_with[1]);
    EXPECT_FALSE(ends_with[2]);
    EXPECT_FALSE(ends_with[3]);

    const boost::dynamic_bitset<> not_like = Filters::ColumnTypedFilter(
        *values,
        Filters::OpType::NotLike,
        std::string_view("%google%"));
    ASSERT_EQ(not_like.size(), 4);
    EXPECT_FALSE(not_like[0]);
    EXPECT_FALSE(not_like[1]);
    EXPECT_TRUE(not_like[2]);
    EXPECT_TRUE(not_like[3]);
}

TEST(FilterExecutor, LikeRejectsNonStringColumns) {
    auto values = std::make_shared<Int64Column>();
    values->Push_Back(10);

    EXPECT_THROW(Filters::ColumnFilter(values, Filters::OpType::Like, "1%"), std::runtime_error);
}

TEST(ClickBenchQueries, FirstQueryCountsRowsWithoutReadingColumns) {
    const std::filesystem::path belz_path = HitsSampleBelZPath();
    ASSERT_TRUE(std::filesystem::exists(belz_path));

    EXPECT_EQ(ClickBench::RunFirstQueryCount(belz_path.string()), 999977);

    BelZReader reader(belz_path.string());
    const size_t first_batch_rows = reader.GetMetaData().GetRow(0);
    Batch empty_projection;
    reader.ReadBatch(empty_projection, {});
    EXPECT_EQ(empty_projection.GetRows(), first_batch_rows);
    EXPECT_EQ(empty_projection.Size(), 0);
}

TEST(ClickBenchQueries, SecondQueryCountsRowsWithNonZeroAdvEngineID) {
    const std::filesystem::path belz_path = HitsSampleBelZPath();
    ASSERT_TRUE(std::filesystem::exists(belz_path));

    EXPECT_EQ(ClickBench::RunSecondQueryCount(belz_path.string()), 14174);
}

TEST(ClickBenchQueries, ThirdQueryComputesGlobalAggregates) {
    const std::filesystem::path belz_path = HitsSampleBelZPath();
    ASSERT_TRUE(std::filesystem::exists(belz_path));

    const ClickBench::Query3Result result = ClickBench::RunThirdQuery(belz_path.string());
    EXPECT_EQ(result.sum_adv_engine_id, 80778);
    EXPECT_EQ(result.count, 999977);
    EXPECT_EQ(result.avg_resolution_width, 1604);
}

TEST(ClickBenchQueries, FourthQueryComputesAvgUserID) {
    const std::filesystem::path belz_path = HitsSampleBelZPath();
    ASSERT_TRUE(std::filesystem::exists(belz_path));

    EXPECT_EQ(ClickBench::RunFourthQueryAvgUserID(belz_path.string()), 1948165676197850120);
}

TEST(ClickBenchQueries, FifthQueryCountsDistinctUserID) {
    const std::filesystem::path belz_path = HitsSampleBelZPath();
    ASSERT_TRUE(std::filesystem::exists(belz_path));

    EXPECT_EQ(ClickBench::RunFifthQueryDistinctUserID(belz_path.string()), 79842);
}

TEST(ClickBenchQueries, SixthQueryCountsDistinctSearchPhrase) {
    const std::filesystem::path belz_path = HitsSampleBelZPath();
    ASSERT_TRUE(std::filesystem::exists(belz_path));

    EXPECT_EQ(ClickBench::RunSixthQueryDistinctSearchPhrase(belz_path.string()), 18316);
}

TEST(ClickBenchQueries, SeventhQueryComputesEventDateMinMax) {
    const std::filesystem::path belz_path = HitsSampleBelZPath();
    ASSERT_TRUE(std::filesystem::exists(belz_path));

    const ClickBench::Query7Result result = ClickBench::RunSeventhQueryEventDateMinMax(belz_path.string());
    EXPECT_EQ(result.min_event_date, Data::ParseDate("2013-07-15"));
    EXPECT_EQ(result.max_event_date, Data::ParseDate("2013-07-15"));
}

TEST(ClickBenchQueries, NewQueryBuildersProduceBatches) {
    const std::filesystem::path belz_path = HitsSampleBelZPath();
    ASSERT_TRUE(std::filesystem::exists(belz_path));

    const std::string table_name = belz_path.string();

    Batch q9 = ClickBench::RunNinthQuery(table_name);
    EXPECT_LE(q9.GetRows(), 10);
    EXPECT_GE(q9.Size(), 2);

    Batch q13 = ClickBench::RunThirteenthQuery(table_name);
    EXPECT_LE(q13.GetRows(), 10);
    EXPECT_GE(q13.Size(), 2);

    Batch q19 = ClickBench::RunNineteenthQuery(table_name);
    EXPECT_LE(q19.GetRows(), 10);
    EXPECT_GE(q19.Size(), 4);

    Batch q25 = ClickBench::RunTwentyFifthQuery(table_name);
    EXPECT_LE(q25.GetRows(), 10);
    EXPECT_EQ(q25.Size(), 1);

    Batch q28 = ClickBench::RunTwentyEighthQuery(table_name);
    EXPECT_LE(q28.GetRows(), 25);
    EXPECT_GE(q28.Size(), 3);

    Batch q29 = ClickBench::RunTwentyNinthQuery(table_name);
    EXPECT_LE(q29.GetRows(), 25);
    EXPECT_GE(q29.Size(), 4);
    EXPECT_EQ(q29.GetScheme().GetName(0), "k");

    Batch q30 = ClickBench::RunThirtiethQuery(table_name);
    EXPECT_EQ(q30.GetRows(), 1);
    EXPECT_EQ(q30.Size(), ClickBench::kQuery30Columns);
    auto q30_first = As<Int64Column>(q30.GetColumn(0));
    auto q30_second = As<Int64Column>(q30.GetColumn(1));
    auto q30_last = As<Int64Column>(q30.GetColumn(ClickBench::kQuery30Columns - 1));
    ASSERT_NE(q30_first, nullptr);
    ASSERT_NE(q30_second, nullptr);
    ASSERT_NE(q30_last, nullptr);
    const int64_t rows = ClickBench::RunFirstQueryCount(table_name);
    EXPECT_EQ(q30_second->At(0) - q30_first->At(0), rows);
    EXPECT_EQ(q30_last->At(0) - q30_first->At(0), static_cast<int64_t>(ClickBench::kQuery30Columns - 1) * rows);

    Batch q34 = ClickBench::RunThirtyFourthQuery(table_name);
    EXPECT_LE(q34.GetRows(), 10);
    EXPECT_GE(q34.Size(), 2);

    Batch q36 = ClickBench::RunThirtySixthQuery(table_name);
    EXPECT_LE(q36.GetRows(), 10);
    EXPECT_GE(q36.Size(), 5);

    Batch q39 = ClickBench::RunThirtyNinthQuery(table_name);
    EXPECT_LE(q39.GetRows(), 10);
    EXPECT_GE(q39.Size(), 2);

    Batch q40 = ClickBench::RunFortiethQuery(table_name);
    EXPECT_LE(q40.GetRows(), 10);
    EXPECT_GE(q40.Size(), 6);

    Batch q43 = ClickBench::RunFortyThirdQuery(table_name);
    EXPECT_LE(q43.GetRows(), 10);
    EXPECT_EQ(q43.Size(), 2);
    EXPECT_EQ(q43.GetScheme().GetName(0), "M");
}
