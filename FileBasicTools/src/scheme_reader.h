#pragma "once"
#include <string>
#include <vector>
#include "datum.h"
#include "file_tools.h"

std::shared_ptr<Datum> DatumConvertor(const std::string& type);

class SchemeReader {
public:
    struct Node {
        std::string name;
        std::shared_ptr<Datum> data;
    };
    void ReadScheme(const std::string& filePath);
    std::vector<Node> GetScheme();
private:
    std::vector<Node> colomns_;
};