#include "LockManager.h"

#include <chrono>

namespace advdb {

LockManager& LockManager::instance() {
    static LockManager manager;
    return manager;
}

bool LockManager::lockShared(std::uint64_t txId, const std::string& resource, std::string& error) {
    return acquire(txId, resource, LockMode::Shared, error);
}

bool LockManager::lockExclusive(std::uint64_t txId, const std::string& resource, std::string& error) {
    return acquire(txId, resource, LockMode::Exclusive, error);
}

void LockManager::unlockAll(std::uint64_t txId) {
    std::unique_lock<std::mutex> lock(mutex_);

    const auto ownedIt = ownedResources_.find(txId);
    if (ownedIt != ownedResources_.end()) {
        for (const std::string& resource : ownedIt->second) {
            auto resourceIt = resources_.find(resource);
            if (resourceIt == resources_.end()) {
                continue;
            }

            resourceIt->second.sharedOwners.erase(txId);
            if (resourceIt->second.exclusiveOwner == txId) {
                resourceIt->second.exclusiveOwner = 0;
            }

            if (resourceIt->second.sharedOwners.empty() && resourceIt->second.exclusiveOwner == 0) {
                resources_.erase(resourceIt);
            }
        }
        ownedResources_.erase(ownedIt);
    }

    removeWaitEdges(txId);
    cv_.notify_all();
}

bool LockManager::acquire(std::uint64_t txId,
                          const std::string& resource,
                          LockMode mode,
                          std::string& error) {
    std::unique_lock<std::mutex> lock(mutex_);
    ResourceState& state = resources_[resource];

    while (true) {
        if (canGrant(txId, state, mode)) {
            if (mode == LockMode::Shared) {
                state.sharedOwners.insert(txId);
            } else {
                state.sharedOwners.erase(txId);
                state.exclusiveOwner = txId;
            }
            ownedResources_[txId].insert(resource);
            waitsFor_.erase(txId);
            return true;
        }

        waitsFor_[txId] = blockersFor(txId, state, mode);
        if (hasDeadlock(txId)) {
            waitsFor_.erase(txId);
            error = "Deadlock detected while acquiring lock on resource: " + resource;
            return false;
        }

        cv_.wait_for(lock, std::chrono::milliseconds(25));
    }
}

bool LockManager::canGrant(std::uint64_t txId,
                           const ResourceState& state,
                           LockMode mode) const {
    if (mode == LockMode::Shared) {
        return state.exclusiveOwner == 0 || state.exclusiveOwner == txId;
    }

    if (state.exclusiveOwner != 0 && state.exclusiveOwner != txId) {
        return false;
    }
    if (state.sharedOwners.empty()) {
        return true;
    }
    return state.sharedOwners.size() == 1 && state.sharedOwners.count(txId) == 1;
}

std::set<std::uint64_t> LockManager::blockersFor(std::uint64_t txId,
                                                 const ResourceState& state,
                                                 LockMode mode) const {
    std::set<std::uint64_t> blockers;

    if (mode == LockMode::Shared) {
        if (state.exclusiveOwner != 0 && state.exclusiveOwner != txId) {
            blockers.insert(state.exclusiveOwner);
        }
        return blockers;
    }

    if (state.exclusiveOwner != 0 && state.exclusiveOwner != txId) {
        blockers.insert(state.exclusiveOwner);
    }
    for (std::uint64_t owner : state.sharedOwners) {
        if (owner != txId) {
            blockers.insert(owner);
        }
    }
    return blockers;
}

bool LockManager::hasDeadlock(std::uint64_t txId) const {
    std::set<std::uint64_t> visited;
    return dfsHasCycle(txId, txId, visited);
}

bool LockManager::dfsHasCycle(std::uint64_t current,
                              std::uint64_t target,
                              std::set<std::uint64_t>& visited) const {
    const auto it = waitsFor_.find(current);
    if (it == waitsFor_.end()) {
        return false;
    }

    for (std::uint64_t next : it->second) {
        if (next == target && current != target) {
            return true;
        }
        if (!visited.insert(next).second) {
            continue;
        }
        if (dfsHasCycle(next, target, visited)) {
            return true;
        }
    }

    return false;
}

void LockManager::removeWaitEdges(std::uint64_t txId) {
    waitsFor_.erase(txId);
    for (auto& entry : waitsFor_) {
        entry.second.erase(txId);
    }
}

} // namespace advdb
