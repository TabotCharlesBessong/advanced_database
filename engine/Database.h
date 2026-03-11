#pragma once

#include <string>

class Database {
public:
    Database(const std::string& dbPath);
    ~Database();

    bool initialize();
    std::string getVersion();

private:
    std::string dbPath_;
};