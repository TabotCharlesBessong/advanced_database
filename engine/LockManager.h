#pragma once

#include <cstdint>
#include <condition_variable>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

namespace advdb {

enum class LockMode {
    Shared,
    Exclusive
};

class LockManager {
public:
    static LockManager& instance();

    bool lockShared(std::uint64_t txId, const std::string& resource, std::string& error);
    bool lockExclusive(std::uint64_t txId, const std::string& resource, std::string& error);
    void unlockAll(std::uint64_t txId);

private:
    struct ResourceState {
        std::set<std::uint64_t> sharedOwners;
        std::uint64_t exclusiveOwner{0};
    };

    LockManager() = default;

    bool acquire(std::uint64_t txId,
                 const std::string& resource,
                 LockMode mode,
                 std::string& error);
    bool canGrant(std::uint64_t txId,
                  const ResourceState& state,
                  LockMode mode) const;
    std::set<std::uint64_t> blockersFor(std::uint64_t txId,
                                        const ResourceState& state,
                                        LockMode mode) const;
    bool hasDeadlock(std::uint64_t txId) const;
    bool dfsHasCycle(std::uint64_t current,
                     std::uint64_t target,
                     std::set<std::uint64_t>& visited) const;
    void removeWaitEdges(std::uint64_t txId);

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::unordered_map<std::string, ResourceState> resources_;
    std::unordered_map<std::uint64_t, std::set<std::string>> ownedResources_;
    std::unordered_map<std::uint64_t, std::set<std::uint64_t>> waitsFor_;
};

} // namespace advdb
