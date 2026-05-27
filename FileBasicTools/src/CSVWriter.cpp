#include "CSVWriter.h"
#include "Column.h"
#include "Scheme.h"
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
    fout_ << data;
}

void CSVWriter::WriteString(const std::string& data) {
    bool needs_quotes = false;
    for (char c : data) {
        if (c == ',' || c == '\n' || c == '"' || c == '\r') {
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes) {
        fout_.write(data.data() , data.size());
        return;
    }
    std::string output;
    output.reserve(data.size() + 2);
    output.push_back('"');
    for (char c : data) {
        if (c == '"') {
            output += "\"\"";
        } else {
            output.push_back(c);
        }
    }
    output.push_back('"');
    fout_.write(output.data() , output.size());
}

void CSVWriter::WriteDelimetr(char delimetr) {
    fout_.write(&delimetr , sizeof(delimetr));
}

void CSVWriter::WriteBatch(const Batch& batch) {
    if (batch.Empty()) {
        throw std::runtime_error("Try to write empty batch!");
    }
    size_t columns = batch.Size();
    size_t rows = batch.GetColumn(0)->Size();
    for (size_t row = 0; row < rows; ++row) {
        for (size_t column = 0; column < columns; ++column) {
            std::shared_ptr<Column> storage = batch.GetColumn(column);
            // TODO решить проблему и придумать как использовать WriteData пока что мы не можем от базового класса взять оператор квадратные скобки
            // TODO надо бы убрать As так как это долго
            if (batch.GetType(column) == ColumnType::Int64) {
                WriteInt64((*As<Int64Column>(storage))[row]);
                if (column + 1 < columns) {
                    WriteDelimetr();
                }
                continue;
            }
            if (batch.GetType(column) == ColumnType::String) {
                WriteString((*As<StringColumn>(storage))[row]); // TODO переписать на конкатенацию всех строк вывод одной большой и потом запись всех размеров
                if (column + 1 < columns) {
                    WriteDelimetr();
                }
                continue;
            }
            throw std::runtime_error("Unknown type if column in Write Batch");
        }
        fout_ << '\n';
    }
}
