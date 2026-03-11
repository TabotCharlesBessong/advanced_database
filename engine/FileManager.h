#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace advdb {

class FileManager {
public:
    explicit FileManager(std::string filePath);
    ~FileManager();

    bool open(bool createIfMissing = true);
    void close();
    bool isOpen() const;

    bool readPage(std::uint32_t pageId, std::vector<std::uint8_t>& outBuffer, std::uint32_t pageSize);
    bool writePage(std::uint32_t pageId, const std::vector<std::uint8_t>& buffer, std::uint32_t pageSize);
    bool flush();

private:
    std::string filePath_;
    std::fstream file_;
};

} // namespace advdb
