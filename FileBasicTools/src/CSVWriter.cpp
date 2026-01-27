#include "CSVWriter.h"
#include "Column.h"
#include <filesystem>
#include <memory>
#include <stdexcept>

CSVWriter::CSVWriter(const std::string& filePath) {
    std::filesystem::path src_path(filePath);
    std::string stem_name = src_path.stem().string();
    std::string new_filename = stem_name + "upd.csv";
    std::filesystem::path dest_path = src_path.parent_path() / new_filename;
    fout_.open(dest_path, std::ios::out); 
    if (!fout_.is_open()) {
        throw std::runtime_error("Failed to create file: " + dest_path.string());
    }
}

void CSVWriter::WriteInt64(int64_t data) {
    fout_.write(reinterpret_cast<char*>(&data) , sizeof(data));
}

void CSVWriter::WriteString(const std::string& data) {
    char quote = '"';
    fout_.write(&quote , sizeof(quote));
    for (auto to : data) {
        if (to == '"') {
            fout_.write(&quote , sizeof(quote));
            fout_.write(&quote , sizeof(quote));
        } else {
            fout_.write(&to, sizeof(to));
        }
    }
    fout_.write(&quote , sizeof(quote));
}

void CSVWriter::WriteDelimetr(char delimetr) {
    fout_.write(&delimetr , sizeof(delimetr));
}

template <typename T>
void CSVWriter::WriteData(const T& data , ColumnType type) {
    if (type == ColumnType::Int64) {
        WriteInt64(data);
        return;
    }
    if (type == ColumnType::String) {
        WriteString(data);
        return;
    }
    throw std::runtime_error("Try to write unknown type"+ std::to_string(static_cast<uint8_t>(type)));
}

void CSVWriter::WriteBatch(const Batch& batch) {
    size_t columns = batch.Size();
    size_t rows = batch.GetColumn(0)->Size();
    for (size_t row = 0; row < rows; ++row) {
        char endl = '\n';
        for (size_t column = 0; column < columns; ++column) {
            std::shared_ptr<Column> storage = batch.GetColumn(column);
            // TODO решить проблему и придумать как использовать WriteData пока что мы не можем от базового класса взять оператор квадратные скобки
            if (Is<Int64Column>(storage)) {
                WriteInt64((*As<Int64Column>(storage))[row]);
                if (column + 1 < columns) {
                    WriteDelimetr();
                }
            }
            if (Is<StringColumn>(storage)) {
                WriteString((*As<StringColumn>(storage))[row]);
                if (column + 1 < columns) {
                    WriteDelimetr();
                }
            }
            throw std::runtime_error("Unknown type if column in Write Batch");
        }
        fout_.write(&endl , sizeof(endl));
    }
}
