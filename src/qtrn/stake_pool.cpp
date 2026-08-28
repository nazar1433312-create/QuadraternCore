// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/stake_pool.h>

#include <map>

namespace qtrn {

namespace {

struct OwnerTotal {
    CAmount total = 0;
    uint256 poolId;
    bool hasPool = false;
};

} // namespace

std::vector<StakeCandidate> AssembleStakeCandidates(const std::vector<StakeCandidateBalance>& balances)
{
    // Pass 1: sum every owner's contributions, keeping the first-seen entry's
    // pool choice for that owner (see header comment on why later entries'
    // choices are ignored rather than merged/overridden).
    std::map<uint256, OwnerTotal> byOwner;
    for (const auto& b : balances) {
        auto insertResult = byOwner.emplace(b.ownerId, OwnerTotal());
        OwnerTotal& entry = insertResult.first->second;
        if (insertResult.second) { // true only the first time this ownerId is seen
            entry.poolId = b.poolId;
            entry.hasPool = b.hasPool;
        }
        entry.total += b.amount;
    }

    // Pass 2: split into solo candidates and pool running-totals.
    std::vector<StakeCandidate> result;
    std::map<uint256, CAmount> poolTotals;
    for (const auto& kv : byOwner) {
        const uint256& ownerId = kv.first;
        const OwnerTotal& data = kv.second;
        if (data.total >= SOLO_STAKE_MINIMUM) {
            result.push_back(StakeCandidate{ownerId, data.total});
        } else if (data.hasPool) {
            poolTotals[data.poolId] += data.total;
        }
        // else: sub-threshold, no declared pool -> not eligible, dropped.
    }

    // Pass 3: emit one candidate per pool.
    for (const auto& kv : poolTotals) {
        result.push_back(StakeCandidate{kv.first, kv.second});
    }

    return result;
}

} // namespace qtrn
