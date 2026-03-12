#pragma once

#include <atomic>
#include <cstdint>

namespace advdb {

class MvccManager {
public:
    struct Snapshot {
        std::uint64_t txId{0};
        std::uint64_t snapshotVersion{0};
    };

    static MvccManager& instance();

    Snapshot begin(std::uint64_t txId);
    std::uint64_t commit(std::uint64_t txId);

    bool canReadCommitted(std::uint64_t rowVersion, std::uint64_t snapshotVersion) const;
    bool canReadUncommitted(std::uint64_t readerTxId, std::uint64_t ownerTxId) const;

private:
    MvccManager() = default;
    std::atomic<std::uint64_t> committedVersion_{0};
};

} // namespace advdb
