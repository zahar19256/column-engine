#include "../FileBasicTools/src/CSVConvertor.h"
#include "Execution/Queries.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

ColumnType ClickBenchColumnType(const std::string& name) {
    static const std::unordered_map<std::string, ColumnType> types = {
        {"WatchID", ColumnType::Int64},
        {"JavaEnable", ColumnType::Int16},
        {"Title", ColumnType::String},
        {"GoodEvent", ColumnType::Int16},
        {"EventTime", ColumnType::Timestamp},
        {"EventDate", ColumnType::Date},
        {"CounterID", ColumnType::Int64},
        {"ClientIP", ColumnType::Int64},
        {"RegionID", ColumnType::Int64},
        {"UserID", ColumnType::Int64},
        {"CounterClass", ColumnType::Int16},
        {"OS", ColumnType::Int16},
        {"UserAgent", ColumnType::Int16},
        {"URL", ColumnType::String},
        {"Referer", ColumnType::String},
        {"IsRefresh", ColumnType::Int16},
        {"RefererCategoryID", ColumnType::Int16},
        {"RefererRegionID", ColumnType::Int64},
        {"URLCategoryID", ColumnType::Int16},
        {"URLRegionID", ColumnType::Int64},
        {"ResolutionWidth", ColumnType::Int16},
        {"ResolutionHeight", ColumnType::Int16},
        {"ResolutionDepth", ColumnType::Int16},
        {"FlashMajor", ColumnType::Int16},
        {"FlashMinor", ColumnType::Int16},
        {"FlashMinor2", ColumnType::String},
        {"NetMajor", ColumnType::Int16},
        {"NetMinor", ColumnType::Int16},
        {"UserAgentMajor", ColumnType::Int16},
        {"UserAgentMinor", ColumnType::String},
        {"CookieEnable", ColumnType::Int16},
        {"JavascriptEnable", ColumnType::Int16},
        {"IsMobile", ColumnType::Int16},
        {"MobilePhone", ColumnType::Int16},
        {"MobilePhoneModel", ColumnType::String},
        {"Params", ColumnType::String},
        {"IPNetworkID", ColumnType::Int64},
        {"TraficSourceID", ColumnType::Int16},
        {"SearchEngineID", ColumnType::Int16},
        {"SearchPhrase", ColumnType::String},
        {"AdvEngineID", ColumnType::Int16},
        {"IsArtifical", ColumnType::Int16},
        {"WindowClientWidth", ColumnType::Int16},
        {"WindowClientHeight", ColumnType::Int16},
        {"ClientTimeZone", ColumnType::Int16},
        {"ClientEventTime", ColumnType::Timestamp},
        {"SilverlightVersion1", ColumnType::Int16},
        {"SilverlightVersion2", ColumnType::Int16},
        {"SilverlightVersion3", ColumnType::Int64},
        {"SilverlightVersion4", ColumnType::Int16},
        {"PageCharset", ColumnType::String},
        {"CodeVersion", ColumnType::Int64},
        {"IsLink", ColumnType::Int16},
        {"IsDownload", ColumnType::Int16},
        {"IsNotBounce", ColumnType::Int16},
        {"FUniqID", ColumnType::Int64},
        {"OriginalURL", ColumnType::String},
        {"HID", ColumnType::Int64},
        {"IsOldCounter", ColumnType::Int16},
        {"IsEvent", ColumnType::Int16},
        {"IsParameter", ColumnType::Int16},
        {"DontCountHits", ColumnType::Int16},
        {"WithHash", ColumnType::Int16},
        {"HitColor", ColumnType::String},
        {"LocalEventTime", ColumnType::Timestamp},
        {"Age", ColumnType::Int16},
        {"Sex", ColumnType::Int16},
        {"Income", ColumnType::Int16},
        {"Interests", ColumnType::Int16},
        {"Robotness", ColumnType::Int16},
        {"RemoteIP", ColumnType::Int64},
        {"WindowName", ColumnType::Int32},
        {"OpenerName", ColumnType::Int32},
        {"HistoryLength", ColumnType::Int16},
        {"BrowserLanguage", ColumnType::String},
        {"BrowserCountry", ColumnType::String},
        {"SocialNetwork", ColumnType::String},
        {"SocialAction", ColumnType::String},
        {"HTTPError", ColumnType::Int16},
        {"SendTiming", ColumnType::Int64},
        {"DNSTiming", ColumnType::Int64},
        {"ConnectTiming", ColumnType::Int64},
        {"ResponseStartTiming", ColumnType::Int64},
        {"ResponseEndTiming", ColumnType::Int64},
        {"FetchTiming", ColumnType::Int64},
        {"SocialSourceNetworkID", ColumnType::Int16},
        {"SocialSourcePage", ColumnType::String},
        {"ParamPrice", ColumnType::Int64},
        {"ParamOrderID", ColumnType::String},
        {"ParamCurrency", ColumnType::String},
        {"ParamCurrencyID", ColumnType::Int16},
        {"OpenstatServiceName", ColumnType::String},
        {"OpenstatCampaignID", ColumnType::String},
        {"OpenstatAdID", ColumnType::String},
        {"OpenstatSourceID", ColumnType::String},
        {"UTMSource", ColumnType::String},
        {"UTMMedium", ColumnType::String},
        {"UTMCampaign", ColumnType::String},
        {"UTMContent", ColumnType::String},
        {"UTMTerm", ColumnType::String},
        {"FromTag", ColumnType::String},
        {"HasGCLID", ColumnType::Int16},
        {"RefererHash", ColumnType::Int64},
        {"URLHash", ColumnType::Int64},
        {"CLID", ColumnType::Int64},
    };

    auto item = types.find(name);
    if (item == types.end()) {
        throw std::runtime_error("No ClickBench type for column: " + name);
    }
    return item->second;
}

Scheme MakeClickBenchScheme() {
    Scheme scheme;
    for (const auto& column : ClickBench::AllHitsColumns()) {
        scheme.Push_Back(SchemeNode{column, ClickBenchColumnType(column)});
    }
    return scheme;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "usage: clickbench_convert <input_csv> <output_belz>\n";
        return 2;
    }

    try {
        const std::filesystem::path input(argv[1]);
        const std::filesystem::path output(argv[2]);
        const auto input_size = std::filesystem::exists(input) ? std::filesystem::file_size(input) : 0;
        const auto start = std::chrono::steady_clock::now();

        CSVConvertor convertor;
        convertor.SetScheme(MakeClickBenchScheme());
        convertor.MakeBelZFormat(input.string(), "BENCHTIME.GO", output.string());

        const auto finish = std::chrono::steady_clock::now();
        const auto output_size = std::filesystem::exists(output) ? std::filesystem::file_size(output) : 0;
        const double total_ms = std::chrono::duration<double, std::milli>(finish - start).count();

        std::cout << "status,input_bytes,output_bytes,total_ms,file\n";
        std::cout << "ok," << input_size << ',' << output_size << ',' << total_ms << ',' << output.string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "clickbench_convert: " << error.what() << '\n';
        return 1;
    }
}
