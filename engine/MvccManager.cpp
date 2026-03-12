#include "MvccManager.h"

namespace advdb {

MvccManager& MvccManager::instance() {
    static MvccManager manager;
    return manager;
}

MvccManager::Snapshot MvccManager::begin(std::uint64_t txId) {
    return Snapshot{txId, committedVersion_.load()};
}

std::uint64_t MvccManager::commit(std::uint64_t) {
    return ++committedVersion_;
}

bool MvccManager::canReadCommitted(std::uint64_t rowVersion, std::uint64_t snapshotVersion) const {
    return rowVersion <= snapshotVersion;
}

bool MvccManager::canReadUncommitted(std::uint64_t readerTxId, std::uint64_t ownerTxId) const {
    return readerTxId != 0 && readerTxId == ownerTxId;
}

} // namespace advdb
