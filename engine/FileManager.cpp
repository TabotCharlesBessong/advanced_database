#include "FileManager.h"

#include <filesystem>
#include <utility>

namespace advdb {

FileManager::FileManager(std::string filePath) : filePath_(std::move(filePath)) {}

FileManager::~FileManager() {
    close();
}

bool FileManager::open(bool createIfMissing) {
    if (file_.is_open()) {
        return true;
    }

    if (createIfMissing && !std::filesystem::exists(filePath_)) {
        std::ofstream createFile(filePath_, std::ios::binary);
        if (!createFile) {
            return false;
        }
    }

    file_.open(filePath_, std::ios::in | std::ios::out | std::ios::binary);
    return file_.is_open();
}

void FileManager::close() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool FileManager::isOpen() const {
    return file_.is_open();
}

bool FileManager::readPage(std::uint32_t pageId, std::vector<std::uint8_t>& outBuffer, std::uint32_t pageSize) {
    if (!file_.is_open() || outBuffer.size() != pageSize) {
        return false;
    }

    const std::streamoff offset = static_cast<std::streamoff>(pageId) * static_cast<std::streamoff>(pageSize);
    file_.seekg(0, std::ios::end);
    const std::streamoff fileSize = file_.tellg();
    if (offset + static_cast<std::streamoff>(pageSize) > fileSize) {
        return false;
    }

    file_.seekg(offset, std::ios::beg);
    file_.read(reinterpret_cast<char*>(outBuffer.data()), static_cast<std::streamsize>(pageSize));
    return file_.good();
}

bool FileManager::writePage(std::uint32_t pageId, const std::vector<std::uint8_t>& buffer, std::uint32_t pageSize) {
    if (!file_.is_open() || buffer.size() != pageSize) {
        return false;
    }

    const std::streamoff offset = static_cast<std::streamoff>(pageId) * static_cast<std::streamoff>(pageSize);
    file_.seekp(offset, std::ios::beg);
    file_.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(pageSize));
    return file_.good();
}

bool FileManager::flush() {
    if (!file_.is_open()) {
        return false;
    }
    file_.flush();
    return file_.good();
}

} // namespace advdb
