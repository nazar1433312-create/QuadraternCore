// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/stake_selection.h>

#include <arith_uint256.h>
#include <hash.h>

#include <algorithm>

namespace qtrn {

namespace {

// Consensus-critical: every node must walk candidates in the same order
// regardless of how the caller assembled the list, or two honest nodes could
// compute different winners from identical inputs. Canonical order = id
// ascending — ids are content-addressed hashes, so this is a stable total
// order (callers must not pass duplicate ids; behaviour is otherwise
// undefined for the duplicate).
std::vector<const StakeCandidate*> CanonicalOrder(const std::vector<StakeCandidate>& candidates)
{
    std::vector<const StakeCandidate*> ordered;
    ordered.reserve(candidates.size());
    for (const auto& c : candidates) ordered.push_back(&c);
    std::sort(ordered.begin(), ordered.end(), [](const StakeCandidate* a, const StakeCandidate* b) {
        return a->id < b->id;
    });
    return ordered;
}

} // namespace

size_t SelectStakeValidator(const uint256& seedHash, const std::vector<StakeCandidate>& candidates)
{
    if (candidates.empty()) return candidates.size();

    const std::vector<const StakeCandidate*> ordered = CanonicalOrder(candidates);

    arith_uint256 totalWeight = 0;
    for (const auto* c : ordered) {
        if (c->weight <= 0) continue; // non-positive weight never contributes and never wins
        totalWeight += arith_uint256(static_cast<uint64_t>(c->weight));
    }
    if (totalWeight == 0) return candidates.size(); // no positively-weighted candidate

    // seedHash is the sole source of randomness; reduce it into [0, totalWeight)
    // to get this round's winning ticket. arith_uint256 has no operator% (only
    // */ and /=), so compute it via the standard identity a % b = a - (a/b)*b —
    // exact here since base_uint division is unsigned truncating division.
    const arith_uint256 rand = UintToArith256(seedHash);
    const arith_uint256 ticket = rand - (rand / totalWeight) * totalWeight;

    arith_uint256 cumulative = 0;
    for (const auto* c : ordered) {
        if (c->weight <= 0) continue;
        cumulative += arith_uint256(static_cast<uint64_t>(c->weight));
        if (ticket < cumulative) {
            // `candidates` is not reallocated for the lifetime of `ordered` (it's
            // a const ref the caller still owns), so pointer arithmetic against
            // its backing storage safely recovers the original index.
            return static_cast<size_t>(c - candidates.data());
        }
    }

    // Unreachable: ticket < totalWeight always holds, and cumulative reaches
    // totalWeight on the last positively-weighted candidate.
    return candidates.size();
}

uint256 DeriveFallbackSeed(const uint256& seedHash, uint32_t attempt)
{
    // spec §6.2 "hash+1" fallback: hashed with the attempt counter rather than
    // added as a raw integer, so consecutive fallback seeds don't sit close to
    // each other as 256-bit integers (which would let an adversary reason
    // about attempt N+1's outcome from attempt N's).
    CHashWriter writer(SER_GETHASH, 0);
    writer << seedHash << attempt;
    return writer.GetHash();
}

} // namespace qtrn
