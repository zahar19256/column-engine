#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "BelZReader.h"
#include "CSVConvertor.h"

namespace fs = std::filesystem;

class HitsRoundTripTest : public ::testing::Test {
protected:
    fs::path csv_path = fs::temp_directory_path() / "column_engine_hits_roundtrip.csv";
    fs::path scheme_path = fs::temp_directory_path() / "column_engine_hits_roundtrip_scheme.csv";
    fs::path belz_path = fs::temp_directory_path() / "column_engine_hits_roundtrip.belZ";

    void SetUp() override {
        {
            std::ofstream scheme(scheme_path);
            ASSERT_TRUE(scheme.is_open());
            scheme << "AdvEngineID,int16\n";
            scheme << "ResolutionWidth,int16\n";
            scheme << "UserID,int64\n";
            scheme << "SearchPhrase,string\n";
            scheme << "EventDate,date\n";
            scheme << "EventTime,timestamp\n";
        }

        {
            std::ofstream csv(csv_path);
            ASSERT_TRUE(csv.is_open());
            csv << "0,800,100,alpha,2013-07-15,2013-07-15 10:47:34\n";
            csv << "1,1024,100,beta,2013-07-16,2013-07-16 11:00:00\n";
            csv << "2,1280,200,alpha,2013-07-17,2013-07-17 12:30:15\n";
            csv << "0,1366,300,,2013-07-18,2013-07-18 00:00:01\n";
            csv << "3,1920,300,gamma,2013-07-19,2013-07-19 23:59:59\n";
        }
    }

    void TearDown() override {
        if (fs::exists(csv_path)) {
            fs::remove(csv_path);
        }
        if (fs::exists(scheme_path)) {
            fs::remove(scheme_path);
        }
        if (fs::exists(belz_path)) {
            fs::remove(belz_path);
        }
    }
};

TEST_F(HitsRoundTripTest, ConvertsAndReadsProjectedClickBenchTypes) {
    CSVConvertor convertor;
    convertor.MakeBelZFormat(csv_path.string(), scheme_path.string());

    ASSERT_TRUE(fs::exists(belz_path));

    BelZReader reader(belz_path.string());
    EXPECT_EQ(reader.RowsCount(), 5);
    ASSERT_EQ(reader.GetScheme().Size(), 6);
    EXPECT_EQ(reader.GetScheme().GetType(0), ColumnType::Int16);
    EXPECT_EQ(reader.GetScheme().GetType(2), ColumnType::Int64);
    EXPECT_EQ(reader.GetScheme().GetType(3), ColumnType::String);
    EXPECT_EQ(reader.GetScheme().GetType(4), ColumnType::Date);
    EXPECT_EQ(reader.GetScheme().GetType(5), ColumnType::Timestamp);

    Batch batch;
    reader.ReadBatch(batch, {"AdvEngineID", "UserID", "SearchPhrase", "EventDate", "EventTime"});

    ASSERT_EQ(batch.GetRows(), 5);
    ASSERT_EQ(batch.Size(), 5);
    EXPECT_EQ(batch.GetType(0), ColumnType::Int16);
    EXPECT_EQ(batch.GetType(1), ColumnType::Int64);
    EXPECT_EQ(batch.GetType(2), ColumnType::String);
    EXPECT_EQ(batch.GetType(3), ColumnType::Date);
    EXPECT_EQ(batch.GetType(4), ColumnType::Timestamp);

    auto adv_engine = As<Int16Column>(batch.GetColumn("AdvEngineID"));
    auto user_id = As<Int64Column>(batch.GetColumn("UserID"));
    auto search_phrase = As<StringColumn>(batch.GetColumn("SearchPhrase"));
    auto event_date = As<DateColumn>(batch.GetColumn("EventDate"));
    auto event_time = As<TimeStampColumn>(batch.GetColumn("EventTime"));

    ASSERT_NE(adv_engine, nullptr);
    ASSERT_NE(user_id, nullptr);
    ASSERT_NE(search_phrase, nullptr);
    ASSERT_NE(event_date, nullptr);
    ASSERT_NE(event_time, nullptr);

    EXPECT_EQ(adv_engine->At(0), 0);
    EXPECT_EQ(adv_engine->At(4), 3);
    EXPECT_EQ(user_id->At(0), 100);
    EXPECT_EQ(user_id->At(4), 300);
    EXPECT_EQ(search_phrase->At_view(0), "alpha");
    EXPECT_EQ(search_phrase->At_view(3), "");
    EXPECT_EQ(event_date->At(0), Data::ParseDate("2013-07-15"));
    EXPECT_EQ(event_date->At(4), Data::ParseDate("2013-07-19"));
    EXPECT_EQ(event_time->At(0), Data::ParseTimestamp("2013-07-15 10:47:34"));
    EXPECT_EQ(event_time->At(4), Data::ParseTimestamp("2013-07-19 23:59:59"));
}
