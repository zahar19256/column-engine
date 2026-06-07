#pragma once
#include "Scheme.h"
#include "Column.h"
#include "Row.h"
#include "Utility.h"
#include <vector>
#include <memory>
#include <boost/dynamic_bitset/dynamic_bitset.hpp>

class Batch {
public:
    Batch() = default;
    Batch(const StringBacket& chunk , const Scheme& scheme) : scheme_(scheme) {
        ChunkToBatch(chunk);
    }
    void Init(const Scheme& scheme , bool convert = false);
    void Init();
    void InitMsk();
    void SetScheme(const Scheme& scheme);
    void SetRows(size_t rows);
    void SetMsk(const boost::dynamic_bitset<>& msk);
    void SetMsk(boost::dynamic_bitset<>&& msk);
    void ApplyMsk(const boost::dynamic_bitset<>& msk);
    void AddColumn(std::shared_ptr<Column> column);
    void AddRowFromCSV(const StringBacket& val);
    void AddAlias(const std::string& current_name , const std::string& alias);
    void MergeBatches(Batch& result , Batch& new_data);

    ColumnType GetType(size_t index) const;
    const Scheme& GetScheme() const;
    const boost::dynamic_bitset<>& GetMsk() const;
    Utility::StringArena* GetStringArena();
    const Utility::StringArena* GetStringArena() const;
    size_t GetRows() const;
    std::shared_ptr<Column> GetColumn(size_t index) const;
    std::shared_ptr<Column> GetColumn(const std::string& column_name) const;

    size_t Size() const;
    bool Empty() const;
    void Clear();
    void Reset();
private:
    void ChunkToBatch(const StringBacket& chunk);
    Scheme scheme_;
    Utility::StringArena string_arena_;
    std::vector <std::shared_ptr<Column>> data_;
    boost::dynamic_bitset<> mask_;
    bool has_mask_ = false;
    size_t rows_ = 0;
};
