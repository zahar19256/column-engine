#include "Execution/Queries.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

constexpr const char* kAutoRebuiltEnv = "CLICKBENCH_RUNNER_AUTOREBUILT";
constexpr const char* kSkipAutoRebuildEnv = "CLICKBENCH_RUNNER_SKIP_REBUILD";

int RunBuild(const std::filesystem::path& build_dir) {
    const std::string build_dir_string = build_dir.string();
    pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error("fork failed before auto rebuild: " + std::string(strerror(errno)));
    }
    if (pid == 0) {
        execlp(
            "cmake" ,
            "cmake" ,
            "--build" ,
            build_dir_string.c_str() ,
            "--target" ,
            "clickbench_runner" ,
            nullptr);
        std::cerr << "execlp(cmake) failed: " << strerror(errno) << '\n';
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid , &status , 0) < 0) {
        throw std::runtime_error("waitpid failed during auto rebuild: " + std::string(strerror(errno)));
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

void AutoRebuildAndReexec(int argc , char* argv[]) {
    if (getenv(kAutoRebuiltEnv) != nullptr || getenv(kSkipAutoRebuildEnv) != nullptr) {
        return;
    }

    const std::filesystem::path executable = std::filesystem::absolute(argv[0]);
    const std::filesystem::path build_dir = executable.parent_path();

    std::cerr << "clickbench_runner: auto rebuild " << build_dir.string() << '\n';
    const int build_status = RunBuild(build_dir);
    if (build_status != 0) {
        throw std::runtime_error("clickbench_runner auto rebuild failed with code " + std::to_string(build_status));
    }

    setenv(kAutoRebuiltEnv , "1" , 1);
    execv(executable.c_str() , argv);
    throw std::runtime_error("execv failed after auto rebuild: " + std::string(strerror(errno)));
}

std::filesystem::path DefaultHitsSamplePath() {
    std::filesystem::path current = std::filesystem::current_path();
    for (size_t depth = 0; depth < 8; ++depth) {
        const std::filesystem::path candidate = current / "FileBasicTools/src/Tests/hits_sample.belZ";
        if (std::filesystem::exists(candidate)) {
            return candidate;
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

std::string Int128ToString(__int128_t value) {
    if (value == 0) {
        return "0";
    }

    bool negative = value < 0;
    if (negative) {
        value = -value;
    }

    std::string result;
    while (value != 0) {
        result.push_back(static_cast<char>('0' + value % 10));
        value /= 10;
    }
    if (negative) {
        result.push_back('-');
    }
    std::reverse(result.begin() , result.end());
    return result;
}

std::string DateToString(int64_t days) {
    days += 719468;
    const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned day_of_era = static_cast<unsigned>(days - era * 146097);
    const unsigned year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
    int year = static_cast<int>(year_of_era) + static_cast<int>(era) * 400;
    const unsigned day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    const unsigned month_part = (5 * day_of_year + 2) / 153;
    const unsigned day = day_of_year - (153 * month_part + 2) / 5 + 1;
    const int month = static_cast<int>(month_part) + (month_part < 10 ? 3 : -9);
    year += month <= 2;

    std::ostringstream out;
    out << std::setfill('0') << std::setw(4) << year << '-'
        << std::setw(2) << month << '-'
        << std::setw(2) << day;
    return out.str();
}

std::string TimestampToString(int64_t timestamp) {
    int64_t days = timestamp / 86400;
    int64_t seconds = timestamp % 86400;
    if (seconds < 0) {
        seconds += 86400;
        --days;
    }

    const int64_t hours = seconds / 3600;
    seconds %= 3600;
    const int64_t minutes = seconds / 60;
    seconds %= 60;

    std::ostringstream out;
    out << DateToString(days) << ' '
        << std::setfill('0') << std::setw(2) << hours << ':'
        << std::setw(2) << minutes << ':'
        << std::setw(2) << seconds;
    return out.str();
}

void WriteEscaped(std::ofstream& out , const std::string& value) {
    bool needs_quotes = false;
    for (const char item : value) {
        if (item == ',' || item == '\n' || item == '\r' || item == '"') {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes) {
        out << value;
        return;
    }

    out << '"';
    for (const char item : value) {
        if (item == '"') {
            out << "\"\"";
        } else {
            out << item;
        }
    }
    out << '"';
}

void WriteValue(std::ofstream& out , const std::shared_ptr<Column>& column , size_t row) {
    switch(column->GetType()) {
        case ColumnType::Int8:
            out << static_cast<int64_t>(As<Int8Column>(column)->At(row));
            return;
        case ColumnType::Int16:
            out << As<Int16Column>(column)->At(row);
            return;
        case ColumnType::Int32:
            out << As<Int32Column>(column)->At(row);
            return;
        case ColumnType::Int64:
            out << As<Int64Column>(column)->At(row);
            return;
        case ColumnType::Int128:
            out << Int128ToString(As<Int128Column>(column)->At(row));
            return;
        case ColumnType::Double:
            out << std::setprecision(17) << As<DoubleColumn>(column)->At(row);
            return;
        case ColumnType::String:
            WriteEscaped(out , std::string(As<StringColumn>(column)->At(row)));
            return;
        case ColumnType::Date:
            out << DateToString(As<DateColumn>(column)->At(row));
            return;
        case ColumnType::Timestamp:
            out << TimestampToString(As<TimeStampColumn>(column)->At(row));
            return;
        case ColumnType::Unknown:
            break;
    }
    throw std::runtime_error("Unknown type in clickbench csv writer!");
}

class BatchCsvWriter {
public:
    explicit BatchCsvWriter(const std::filesystem::path& path) {
        out_.open(path , std::ios::out);
        if (!out_.is_open()) {
            throw std::runtime_error("Can't open csv file: " + path.string());
        }
    }

    void WriteBatch(const Batch& batch) {
        if (!header_written_) {
            WriteHeader(batch);
            header_written_ = true;
        }

        const auto& mask = batch.GetMsk();
        const bool use_mask = !mask.empty() && mask.count() != mask.size();
        for (size_t row = 0; row < batch.GetRows(); ++row) {
            if (use_mask && !mask[row]) {
                continue;
            }
            for (size_t column = 0; column < batch.Size(); ++column) {
                if (column != 0) {
                    out_ << ',';
                }
                WriteValue(out_ , batch.GetColumn(column) , row);
            }
            out_ << '\n';
            ++rows_;
        }
    }

    size_t Rows() const {
        return rows_;
    }

private:
    void WriteHeader(const Batch& batch) {
        for (size_t column = 0; column < batch.Size(); ++column) {
            if (column != 0) {
                out_ << ',';
            }
            WriteEscaped(out_ , batch.GetScheme().GetName(column));
        }
        if (batch.Size() != 0) {
            out_ << '\n';
        }
    }

    std::ofstream out_;
    bool header_written_ = false;
    size_t rows_ = 0;
};

Batch MakeInt64Result(const std::string& name , int64_t value) {
    Scheme scheme;
    scheme.Push_Back(SchemeNode{name , ColumnType::Int64});

    auto column = std::make_shared<Int64Column>();
    column->Push_Back(value);

    Batch result;
    result.SetScheme(scheme);
    result.AddColumn(std::move(column));
    result.InitMsk();
    return result;
}

Batch MakeQuery3Result(const ClickBench::Query3Result& value) {
    Scheme scheme;
    scheme.Push_Back(SchemeNode{ClickBench::kQuery3SumAdvEngineIdColumn , ColumnType::Int64});
    scheme.Push_Back(SchemeNode{ClickBench::kQuery3CountColumn , ColumnType::Int64});
    scheme.Push_Back(SchemeNode{ClickBench::kQuery3AvgResolutionWidthColumn , ColumnType::Int16});

    auto sum_adv_engine_id = std::make_shared<Int64Column>();
    sum_adv_engine_id->Push_Back(value.sum_adv_engine_id);

    auto count = std::make_shared<Int64Column>();
    count->Push_Back(value.count);

    auto avg_resolution_width = std::make_shared<Int16Column>();
    avg_resolution_width->Push_Back(value.avg_resolution_width);

    Batch result;
    result.SetScheme(scheme);
    result.AddColumn(std::move(sum_adv_engine_id));
    result.AddColumn(std::move(count));
    result.AddColumn(std::move(avg_resolution_width));
    result.InitMsk();
    return result;
}

Batch MakeQuery7Result(const ClickBench::Query7Result& value) {
    Scheme scheme;
    scheme.Push_Back(SchemeNode{ClickBench::kQuery7MinEventDateColumn , ColumnType::Date});
    scheme.Push_Back(SchemeNode{ClickBench::kQuery7MaxEventDateColumn , ColumnType::Date});

    auto min_event_date = std::make_shared<DateColumn>();
    min_event_date->Push_Back(value.min_event_date);

    auto max_event_date = std::make_shared<DateColumn>();
    max_event_date->Push_Back(value.max_event_date);

    Batch result;
    result.SetScheme(scheme);
    result.AddColumn(std::move(min_event_date));
    result.AddColumn(std::move(max_event_date));
    result.InitMsk();
    return result;
}

struct BenchRun {
    size_t rows = 0;
    double query_ms = 0.0;
    double read_ms = 0.0;
    double write_ms = 0.0;
};

BenchRun WriteBatchResult(Batch output , const std::filesystem::path& file , double query_ms) {
    const auto start = std::chrono::steady_clock::now();
    BatchCsvWriter writer(file);
    writer.WriteBatch(output);
    const auto finish = std::chrono::steady_clock::now();
    return BenchRun{
        .rows = writer.Rows(),
        .query_ms = query_ms,
        .write_ms = std::chrono::duration<double , std::milli>(finish - start).count(),
    };
}

template <typename Func>
BenchRun RunBatchQuery(Func func , const std::filesystem::path& file) {
    const auto start = std::chrono::steady_clock::now();
    Batch output = func();
    const auto finish = std::chrono::steady_clock::now();
    return WriteBatchResult(std::move(output) , file , std::chrono::duration<double , std::milli>(finish - start).count());
}

BenchRun RunPlan(std::unique_ptr<LogicPlaner::QueryNode> query , const std::filesystem::path& file) {
    auto executor = ClickBench::BuildExecutor(*query);
    BatchCsvWriter writer(file);
    Batch output;
    BenchRun result;
    while (true) {
        const auto query_start = std::chrono::steady_clock::now();
        const bool has_data = executor->Next(output);
        const auto query_finish = std::chrono::steady_clock::now();
        result.query_ms += std::chrono::duration<double , std::milli>(query_finish - query_start).count();
        if (!has_data) {
            break;
        }

        const auto write_start = std::chrono::steady_clock::now();
        writer.WriteBatch(output);
        const auto write_finish = std::chrono::steady_clock::now();
        result.write_ms += std::chrono::duration<double , std::milli>(write_finish - write_start).count();
    }
    result.rows = writer.Rows();
    result.read_ms = executor->ReadMillis();
    return result;
}

struct BenchQuery {
    size_t number;
    std::function<BenchRun(const std::string& , const std::filesystem::path&)> run;
};

struct QueryPlanInfo {
    size_t number;
    std::function<std::unique_ptr<LogicPlaner::QueryNode>(const std::string&)> build;
};

std::vector<QueryPlanInfo> MakePlanInfos(const std::set<size_t>& only_queries) {
    std::vector<QueryPlanInfo> result = {
        {2 , ClickBench::MakeSecondQueryPlan},
        {3 , ClickBench::MakeThirdQueryPlan},
        {4 , ClickBench::MakeFourthQueryPlan},
        {5 , ClickBench::MakeFifthQueryPlan},
        {6 , ClickBench::MakeSixthQueryPlan},
        {7 , ClickBench::MakeSeventhQueryPlan},
        {8 , ClickBench::MakeEighthQueryPlan},
        {9 , ClickBench::MakeNinthQueryPlan},
        {10 , ClickBench::MakeTenthQueryPlan},
        {11 , ClickBench::MakeEleventhQueryPlan},
        {12 , ClickBench::MakeTwelfthQueryPlan},
        {13 , ClickBench::MakeThirteenthQueryPlan},
        {14 , ClickBench::MakeFourteenthQueryPlan},
        {15 , ClickBench::MakeFifteenthQueryPlan},
        {16 , ClickBench::MakeSixteenthQueryPlan},
        {17 , ClickBench::MakeSeventeenthQueryPlan},
        {18 , ClickBench::MakeEighteenthQueryPlan},
        {19 , ClickBench::MakeNineteenthQueryPlan},
        {20 , ClickBench::MakeTwentiethQueryPlan},
        {21 , ClickBench::MakeTwentyFirstQueryPlan},
        {22 , ClickBench::MakeTwentySecondQueryPlan},
        {23 , ClickBench::MakeTwentyThirdQueryPlan},
        {24 , ClickBench::MakeTwentyFourthQueryPlan},
        {25 , ClickBench::MakeTwentyFifthQueryPlan},
        {26 , ClickBench::MakeTwentySixthQueryPlan},
        {27 , ClickBench::MakeTwentySeventhQueryPlan},
        {28 , ClickBench::MakeTwentyEighthQueryPlan},
        {31 , ClickBench::MakeThirtyFirstQueryPlan},
        {32 , ClickBench::MakeThirtySecondQueryPlan},
        {33 , ClickBench::MakeThirtyThirdQueryPlan},
        {34 , ClickBench::MakeThirtyFourthQueryPlan},
        {35 , ClickBench::MakeThirtyFifthQueryPlan},
        {36 , ClickBench::MakeThirtySixthQueryPlan},
        {37 , ClickBench::MakeThirtySeventhQueryPlan},
        {38 , ClickBench::MakeThirtyEighthQueryPlan},
        {39 , ClickBench::MakeThirtyNinthQueryPlan},
        {40 , ClickBench::MakeFortiethQueryPlan},
        {41 , ClickBench::MakeFortyFirstQueryPlan},
        {42 , ClickBench::MakeFortySecondQueryPlan},
    };

    if (only_queries.empty()) {
        return result;
    }

    std::vector<QueryPlanInfo> filtered;
    for (auto& query : result) {
        if (only_queries.contains(query.number)) {
            filtered.push_back(std::move(query));
        }
    }
    return filtered;
}

std::vector<BenchQuery> MakeQueries(const std::set<size_t>& only_queries) {
    std::vector<BenchQuery> result = {
        {1 , [](const std::string& table_name , const std::filesystem::path& file) {
            return RunBatchQuery([&]() {
                return MakeInt64Result(ClickBench::kQuery1ResultColumn , ClickBench::RunFirstQueryCount(table_name));
            } , file);
        }},
        {2 , [](const std::string& table_name , const std::filesystem::path& file) {
            return RunBatchQuery([&]() {
                return MakeInt64Result(ClickBench::kQuery2ResultColumn , ClickBench::RunSecondQueryCount(table_name));
            } , file);
        }},
        {3 , [](const std::string& table_name , const std::filesystem::path& file) {
            return RunBatchQuery([&]() {
                return MakeQuery3Result(ClickBench::RunThirdQuery(table_name));
            } , file);
        }},
        {4 , [](const std::string& table_name , const std::filesystem::path& file) {
            return RunBatchQuery([&]() {
                return MakeInt64Result(ClickBench::kQuery4ResultColumn , ClickBench::RunFourthQueryAvgUserID(table_name));
            } , file);
        }},
        {5 , [](const std::string& table_name , const std::filesystem::path& file) {
            return RunBatchQuery([&]() {
                return MakeInt64Result(ClickBench::kQuery5ResultColumn , ClickBench::RunFifthQueryDistinctUserID(table_name));
            } , file);
        }},
        {6 , [](const std::string& table_name , const std::filesystem::path& file) {
            return RunBatchQuery([&]() {
                return MakeInt64Result(ClickBench::kQuery6ResultColumn , ClickBench::RunSixthQueryDistinctSearchPhrase(table_name));
            } , file);
        }},
        {7 , [](const std::string& table_name , const std::filesystem::path& file) {
            return RunBatchQuery([&]() {
                return MakeQuery7Result(ClickBench::RunSeventhQueryEventDateMinMax(table_name));
            } , file);
        }},
        {8 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeEighthQueryPlan(table_name) , file); }},
        {9 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeNinthQueryPlan(table_name) , file); }},
        {10 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeTenthQueryPlan(table_name) , file); }},
        {11 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeEleventhQueryPlan(table_name) , file); }},
        {12 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeTwelfthQueryPlan(table_name) , file); }},
        {13 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeThirteenthQueryPlan(table_name) , file); }},
        {14 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeFourteenthQueryPlan(table_name) , file); }},
        {15 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeFifteenthQueryPlan(table_name) , file); }},
        {16 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeSixteenthQueryPlan(table_name) , file); }},
        {17 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeSeventeenthQueryPlan(table_name) , file); }},
        {18 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeEighteenthQueryPlan(table_name) , file); }},
        {19 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeNineteenthQueryPlan(table_name) , file); }},
        {20 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeTwentiethQueryPlan(table_name) , file); }},
        {21 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeTwentyFirstQueryPlan(table_name) , file); }},
        {22 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeTwentySecondQueryPlan(table_name) , file); }},
        {23 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeTwentyThirdQueryPlan(table_name) , file); }},
        {24 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeTwentyFourthQueryPlan(table_name) , file); }},
        {25 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeTwentyFifthQueryPlan(table_name) , file); }},
        {26 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeTwentySixthQueryPlan(table_name) , file); }},
        {27 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeTwentySeventhQueryPlan(table_name) , file); }},
        {28 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeTwentyEighthQueryPlan(table_name) , file); }},
        {30 , [](const std::string& table_name , const std::filesystem::path& file) {
            return RunBatchQuery([&]() {
                return ClickBench::RunThirtiethQuery(table_name);
            } , file);
        }},
        {31 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeThirtyFirstQueryPlan(table_name) , file); }},
        {32 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeThirtySecondQueryPlan(table_name) , file); }},
        {33 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeThirtyThirdQueryPlan(table_name) , file); }},
        {34 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeThirtyFourthQueryPlan(table_name) , file); }},
        {35 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeThirtyFifthQueryPlan(table_name) , file); }},
        {36 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeThirtySixthQueryPlan(table_name) , file); }},
        {37 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeThirtySeventhQueryPlan(table_name) , file); }},
        {38 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeThirtyEighthQueryPlan(table_name) , file); }},
        {39 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeThirtyNinthQueryPlan(table_name) , file); }},
        {40 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeFortiethQueryPlan(table_name) , file); }},
        {41 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeFortyFirstQueryPlan(table_name) , file); }},
        {42 , [](const std::string& table_name , const std::filesystem::path& file) { return RunPlan(ClickBench::MakeFortySecondQueryPlan(table_name) , file); }},
    };

    if (only_queries.empty()) {
        return result;
    }

    std::vector<BenchQuery> filtered;
    for (auto& query : result) {
        if (only_queries.contains(query.number)) {
            filtered.push_back(std::move(query));
        }
    }
    return filtered;
}

void CollectScanColumns(const LogicPlaner::QueryNode& query , std::vector<std::vector<std::string>>& scans) {
    if (const auto* scan = dynamic_cast<const LogicPlaner::ScanNode*>(&query)) {
        scans.push_back(scan->ColumnNames());
    }
    for (const auto& child : query.next_nodes_) {
        CollectScanColumns(*child , scans);
    }
}

void PrintColumns(std::vector<std::string> columns) {
    std::sort(columns.begin() , columns.end());
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i != 0) {
            std::cout << '|';
        }
        std::cout << columns[i];
    }
}

void ExplainQueries(const std::set<size_t>& only_queries) {
    const std::string table_name = "clickbench.belZ";
    std::cout << "query,scan,columns_count,columns\n";

    if (only_queries.empty() || only_queries.contains(1)) {
        std::cout << "1,0,0,\n";
    }

    for (const auto& query_info : MakePlanInfos(only_queries)) {
        auto query = query_info.build(table_name);
        std::vector<std::vector<std::string>> scans;
        CollectScanColumns(*query , scans);
        for (size_t scan_index = 0; scan_index < scans.size(); ++scan_index) {
            std::cout << query_info.number << ','
                      << scan_index << ','
                      << scans[scan_index].size() << ',';
            PrintColumns(std::move(scans[scan_index]));
            std::cout << '\n';
        }
    }

    if (only_queries.empty() || only_queries.contains(30)) {
        std::cout << "30,0,1,ResolutionWidth\n";
    }
}

struct QueryResult {
    size_t number = 0;
    bool ok = false;
    size_t rows = 0;
    double query_ms = 0.0;
    double read_ms = 0.0;
    double write_ms = 0.0;
    double total_ms = 0.0;
    std::filesystem::path file;
    std::string error;
};

void WriteSummary(const std::vector<QueryResult>& results , const std::filesystem::path& path) {
    std::ofstream out(path , std::ios::out);
    if (!out.is_open()) {
        throw std::runtime_error("Can't open summary file: " + path.string());
    }

    out << "query,status,rows,query_ms,write_ms,total_ms,file,read_ms,error\n";
    for (const auto& result : results) {
        out << result.number << ','
            << (result.ok ? "ok" : "error") << ','
            << result.rows << ','
            << std::fixed << std::setprecision(3) << result.query_ms << ','
            << result.write_ms << ','
            << result.total_ms << ',';
        WriteEscaped(out , result.file.string());
        out << ',' << result.read_ms << ',';
        WriteEscaped(out , result.error);
        out << '\n';
    }
}

} // namespace

int main(int argc , char* argv[]) {
    try {
        AutoRebuildAndReexec(argc , argv);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    if (argc >= 2 && std::string(argv[1]) == "--help") {
        std::cerr << "Usage: clickbench_runner [path/to/hits.belZ] [output_dir] [query_numbers...]\n";
        std::cerr << "       clickbench_runner --explain [query_numbers...]\n";
        return 0;
    }
    if (argc >= 2 && std::string(argv[1]) == "--explain") {
        std::set<size_t> only_queries;
        for (int i = 2; i < argc; ++i) {
            only_queries.insert(std::stoull(argv[i]));
        }
        ExplainQueries(only_queries);
        return 0;
    }
    if (argc > 64) {
        std::cerr << "Usage: clickbench_runner [path/to/hits.belZ] [output_dir] [query_numbers...]\n";
        return 1;
    }

    try {
        const std::string table_name = argc >= 2 ? argv[1] : DefaultHitsSamplePath().string();
        const std::filesystem::path output_dir = argc >= 3 ? argv[2] : "clickbench_results";
        std::set<size_t> only_queries;
        for (int i = 3; i < argc; ++i) {
            only_queries.insert(std::stoull(argv[i]));
        }
        std::filesystem::create_directories(output_dir);

        std::vector<QueryResult> results;
        const auto queries = MakeQueries(only_queries);
        results.reserve(queries.size());

        std::cout << "query,status,rows,query_ms,write_ms,total_ms,file,read_ms\n";
        for (const auto& query : queries) {
            QueryResult result;
            result.number = query.number;
            result.file = output_dir / ("q" + std::to_string(query.number) + ".csv");

            const auto start = std::chrono::steady_clock::now();
            try {
                const BenchRun bench_run = query.run(table_name , result.file);
                const auto finish = std::chrono::steady_clock::now();
                result.rows = bench_run.rows;
                result.query_ms = bench_run.query_ms;
                result.read_ms = bench_run.read_ms;
                result.write_ms = bench_run.write_ms;
                result.total_ms = std::chrono::duration<double , std::milli>(finish - start).count();
                result.ok = true;
            } catch (const std::exception& error) {
                const auto finish = std::chrono::steady_clock::now();
                result.total_ms = std::chrono::duration<double , std::milli>(finish - start).count();
                result.error = error.what();

                std::ofstream empty(result.file , std::ios::out);
                if (!empty.is_open()) {
                    throw;
                }
            }

            std::cout << result.number << ','
                      << (result.ok ? "ok" : "error") << ','
                      << result.rows << ','
                      << std::fixed << std::setprecision(3) << result.query_ms << ','
                      << result.write_ms << ','
                      << result.total_ms << ','
                      << result.file.string() << ','
                      << result.read_ms << '\n';
            if (!result.ok) {
                std::cerr << "q" << result.number << ": " << result.error << '\n';
            }
            results.push_back(std::move(result));
        }

        WriteSummary(results , output_dir / "summary.csv");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
