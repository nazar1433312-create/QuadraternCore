// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/stake_pool.h>

#include <arith_uint256.h>
#include <hash.h>

#include <map>

namespace qtrn {

uint256 DeterministicPoolId(const uint256& ownerId)
{
    // Re-hash first so the pool index isn't just "the low bits of the owner
    // id verbatim" (no reason to rely on ownerId's own bit distribution).
    // Reduce mod N via the same trick stake_selection.cpp uses — arith_uint256
    // has no operator%, only + - * / and comparisons.
    const arith_uint256 hashed = UintToArith256(Hash(ownerId));
    const arith_uint256 n(NUM_VIRTUAL_POOLS);
    const arith_uint256 poolIndex = hashed - (hashed / n) * n;
    return ArithToUint256(poolIndex);
}

std::vector<StakeCandidate> AssembleStakeCandidates(const std::vector<StakeCandidateBalance>& balances)
{
    // Pass 1: sum every owner's contributions (a real address usually holds
    // more than one UTXO).
    std::map<uint256, CAmount> byOwner;
    for (const auto& b : balances) {
        byOwner[b.ownerId] += b.amount;
    }

    // Pass 2: split into solo candidates and pool running-totals.
    std::vector<StakeCandidate> result;
    std::map<uint256, CAmount> poolTotals;
    for (const auto& kv : byOwner) {
        const uint256& ownerId = kv.first;
        const CAmount total = kv.second;
        if (total <= 0) continue; // not eligible either way
        if (total >= SOLO_STAKE_MINIMUM) {
            result.push_back(StakeCandidate{ownerId, total});
        } else {
            poolTotals[DeterministicPoolId(ownerId)] += total;
        }
    }

    // Pass 3: emit one candidate per pool.
    for (const auto& kv : poolTotals) {
        result.push_back(StakeCandidate{kv.first, kv.second});
    }

    return result;
}

} // namespace qtrn
