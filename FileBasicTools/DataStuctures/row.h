#pragma "once"
#include <vector>

template <typename T>
class Row {
public:
    void Add(const T& value);
    void Add(T&& value);
    size_t Size() const;
    bool Empty() const;
    void Clear();
private:
    std::vector<T> data_;
};