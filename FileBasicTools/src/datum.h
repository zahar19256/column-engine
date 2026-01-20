#pragma "once"
#include <memory>

class Datum : public std::enable_shared_from_this<Datum> {
public:
    virtual ~Datum() = default;
};

class Int64 : public Datum {
public:
    int64_t GetValue() {
        return value_;
    }
private:
    int64_t value_;
};

class String : public Datum {
public:
    std::string GetValue() {
        return value_;
    }
private:
    std::string value_;
};

template <class T>
std::shared_ptr<T> As(const std::shared_ptr<Datum>& obj) {
    return std::dynamic_pointer_cast<T>(obj);
}

template <class T>
bool Is(const std::shared_ptr<Datum>& obj) {
    return dynamic_cast<T*>(obj.get()) != nullptr;
}
