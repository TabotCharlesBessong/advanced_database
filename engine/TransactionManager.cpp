#include "TransactionManager.h"

#include <atomic>
#include <fstream>
#include <set>
#include <map>
#include <sstream>

#include "LockManager.h"
#include "MvccManager.h"

namespace advdb {

namespace {

std::atomic<std::uint64_t> gNextTransactionId{1};

} // namespace

bool TransactionManager::initialize(const std::string& walPath,
                                    ApplyInsertFn applyInsertFn,
                                    std::string& error) {
    walPath_ = walPath;
    applyInsertFn_ = std::move(applyInsertFn);

    std::ofstream createIfMissing(walPath_, std::ios::app);
    if (!createIfMissing) {
        error = "Failed to open WAL file: " + walPath_;
        return false;
    }

    return runRecovery(error);
}

bool TransactionManager::begin(std::string& error) {
    if (active_) {
        error = "Transaction already active";
        return false;
    }

    active_ = true;
    currentTxId_ = gNextTransactionId.fetch_add(1);
    nextTxId_ = currentTxId_ + 1;
    snapshotVersion_ = MvccManager::instance().begin(currentTxId_).snapshotVersion;
    pendingInserts_.clear();

    return appendWalLine("BEGIN " + std::to_string(currentTxId_), error);
}

bool TransactionManager::stageInsert(const std::string& tableName, const Row& row, std::string& error) {
    if (!active_) {
        error = "No active transaction";
        return false;
    }

    pendingInserts_.push_back(PendingInsert{tableName, row});
    return appendWalLine("INSERT " + std::to_string(currentTxId_) + " " + tableName + " " + encodeRow(row), error);
}

bool TransactionManager::commit(std::string& error) {
    if (!active_) {
        error = "No active transaction";
        return false;
    }

    const std::uint64_t txId = currentTxId_;

    std::set<std::string> touchedTables;
    for (const PendingInsert& insert : pendingInserts_) {
        touchedTables.insert(insert.tableName);
    }

    for (const std::string& tableName : touchedTables) {
        if (!LockManager::instance().lockExclusive(txId, "table:" + tableName, error)) {
            std::string rollbackError;
            rollback(rollbackError);
            return false;
        }
    }

    if (!appendWalLine("COMMIT " + std::to_string(txId), error)) {
        LockManager::instance().unlockAll(txId);
        return false;
    }

    for (const PendingInsert& insert : pendingInserts_) {
        if (!applyInsertFn_(insert.tableName, insert.row, error)) {
            LockManager::instance().unlockAll(txId);
            active_ = false;
            pendingInserts_.clear();
            currentTxId_ = 0;
            snapshotVersion_ = 0;
            return false;
        }
    }

    MvccManager::instance().commit(txId);

    if (!appendWalLine("APPLIED " + std::to_string(txId), error)) {
        LockManager::instance().unlockAll(txId);
        return false;
    }

    LockManager::instance().unlockAll(txId);
    active_ = false;
    pendingInserts_.clear();
    currentTxId_ = 0;
    snapshotVersion_ = 0;
    return true;
}

bool TransactionManager::rollback(std::string& error) {
    if (!active_) {
        error = "No active transaction";
        return false;
    }

    const std::uint64_t txId = currentTxId_;
    if (!appendWalLine("ROLLBACK " + std::to_string(txId), error)) {
        return false;
    }

    LockManager::instance().unlockAll(txId);
    active_ = false;
    pendingInserts_.clear();
    currentTxId_ = 0;
    snapshotVersion_ = 0;
    return true;
}

bool TransactionManager::inTransaction() const {
    return active_;
}

std::uint64_t TransactionManager::currentTransactionId() const {
    return currentTxId_;
}

std::vector<Row> TransactionManager::pendingRowsForTable(const std::string& tableName) const {
    std::vector<Row> rows;
    for (const PendingInsert& insert : pendingInserts_) {
        if (insert.tableName == tableName && MvccManager::instance().canReadUncommitted(currentTxId_, currentTxId_)) {
            rows.push_back(insert.row);
        }
    }
    return rows;
}

std::string TransactionManager::escape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        if (c == '\\' || c == '|' || c == ':' || c == '\n' || c == '\r' || c == '\t') {
            out.push_back('\\');
            switch (c) {
                case '\n': out.push_back('n'); break;
                case '\r': out.push_back('r'); break;
                case '\t': out.push_back('t'); break;
                default: out.push_back(c); break;
            }
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string TransactionManager::unescape(const std::string& value) {
    std::string out;
    out.reserve(value.size());

    bool escaping = false;
    for (char c : value) {
        if (!escaping) {
            if (c == '\\') {
                escaping = true;
            } else {
                out.push_back(c);
            }
            continue;
        }

        switch (c) {
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            default: out.push_back(c); break;
        }
        escaping = false;
    }

    if (escaping) {
        out.push_back('\\');
    }

    return out;
}

std::string TransactionManager::encodeRow(const Row& row) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < row.size(); ++i) {
        if (i > 0) {
            oss << '|';
        }

        if (row[i].isNull()) {
            oss << "N";
        } else if (row[i].isInt()) {
            oss << "I:" << row[i].intVal;
        } else if (row[i].isString()) {
            oss << "S:" << escape(row[i].strVal);
        } else {
            oss << "N";
        }
    }
    return oss.str();
}

bool TransactionManager::decodeRow(const std::string& encoded, Row& row, std::string& error) {
    row.clear();

    std::vector<std::string> parts;
    std::string token;
    bool escaping = false;

    for (char c : encoded) {
        if (!escaping && c == '|') {
            parts.push_back(token);
            token.clear();
            continue;
        }

        if (c == '\\' && !escaping) {
            escaping = true;
            token.push_back(c);
            continue;
        }

        token.push_back(c);
        escaping = false;
    }
    parts.push_back(token);

    for (const std::string& part : parts) {
        if (part == "N") {
            row.push_back(Value::makeNull());
            continue;
        }

        if (part.rfind("I:", 0) == 0) {
            try {
                row.push_back(Value::makeInt(std::stoll(part.substr(2))));
            } catch (...) {
                error = "Invalid INT payload in WAL";
                return false;
            }
            continue;
        }

        if (part.rfind("S:", 0) == 0) {
            row.push_back(Value::makeString(unescape(part.substr(2))));
            continue;
        }

        error = "Invalid row payload in WAL";
        return false;
    }

    return true;
}

bool TransactionManager::splitFirst(const std::string& input,
                                    char delimiter,
                                    std::string& left,
                                    std::string& right) {
    const std::size_t pos = input.find(delimiter);
    if (pos == std::string::npos) {
        return false;
    }
    left = input.substr(0, pos);
    right = input.substr(pos + 1);
    return true;
}

bool TransactionManager::appendWalLine(const std::string& line, std::string& error) const {
    std::ofstream out(walPath_, std::ios::app);
    if (!out) {
        error = "Failed writing WAL";
        return false;
    }

    out << line << '\n';
    if (!out.good()) {
        error = "Failed flushing WAL";
        return false;
    }

    return true;
}

bool TransactionManager::runRecovery(std::string& error) {
    std::ifstream in(walPath_);
    if (!in) {
        return true;
    }

    std::map<std::uint64_t, ParsedTxn> txns;
    std::string line;

    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }

        std::istringstream iss(line);
        std::string type;
        std::uint64_t txId = 0;
        iss >> type >> txId;
        if (!iss || txId == 0) {
            continue;
        }

        if (txId >= nextTxId_) {
            nextTxId_ = txId + 1;
        }

        ParsedTxn& txn = txns[txId];
        if (type == "COMMIT") {
            txn.hasCommit = true;
            continue;
        }
        if (type == "APPLIED") {
            txn.hasApplied = true;
            continue;
        }
        if (type == "ROLLBACK") {
            txn.hasRollback = true;
            continue;
        }
        if (type != "INSERT") {
            continue;
        }

        std::string rest;
        std::getline(iss, rest);
        if (!rest.empty() && rest[0] == ' ') {
            rest.erase(0, 1);
        }

        std::string tableName;
        std::string rowPayload;
        if (!splitFirst(rest, ' ', tableName, rowPayload)) {
            continue;
        }

        Row row;
        if (!decodeRow(rowPayload, row, error)) {
            return false;
        }

        txn.inserts.push_back(PendingInsert{tableName, std::move(row)});
    }

    for (const auto& entry : txns) {
        const std::uint64_t txId = entry.first;
        const ParsedTxn& txn = entry.second;

        if (!txn.hasCommit || txn.hasApplied || txn.hasRollback) {
            continue;
        }

        for (const PendingInsert& insert : txn.inserts) {
            if (!applyInsertFn_(insert.tableName, insert.row, error)) {
                return false;
            }
        }

        if (!appendWalLine("APPLIED " + std::to_string(txId), error)) {
            return false;
        }
    }

    return true;
}

} // namespace advdb
