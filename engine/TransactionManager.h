#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string>
#include <vector>

#include "Value.h"

namespace advdb {

class TransactionManager {
public:
    using ApplyInsertFn = std::function<bool(const std::string&, const Row&, std::string&)>;

    bool initialize(const std::string& walPath,
                    ApplyInsertFn applyInsertFn,
                    std::string& error);

    bool begin(std::string& error);
    bool stageInsert(const std::string& tableName, const Row& row, std::string& error);
    bool commit(std::string& error);
    bool rollback(std::string& error);

    bool inTransaction() const;
    std::uint64_t currentTransactionId() const;
    std::vector<Row> pendingRowsForTable(const std::string& tableName) const;

private:
    struct PendingInsert {
        std::string tableName;
        Row row;
    };

    struct ParsedTxn {
        std::vector<PendingInsert> inserts;
        bool hasCommit{false};
        bool hasApplied{false};
        bool hasRollback{false};
    };

    static std::string escape(const std::string& value);
    static std::string unescape(const std::string& value);

    static std::string encodeRow(const Row& row);
    static bool decodeRow(const std::string& encoded, Row& row, std::string& error);

    static bool splitFirst(const std::string& input,
                           char delimiter,
                           std::string& left,
                           std::string& right);

    bool appendWalLine(const std::string& line, std::string& error) const;
    bool runRecovery(std::string& error);

    std::string walPath_;
    ApplyInsertFn applyInsertFn_;

    bool active_{false};
    std::uint64_t currentTxId_{0};
    std::uint64_t nextTxId_{1};
    std::uint64_t snapshotVersion_{0};
    std::vector<PendingInsert> pendingInserts_;
};

} // namespace advdb
