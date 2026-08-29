// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// testnet-1: virtual staking pool aggregation (spec §6.3). This module only
// implements the pure grouping/weighting logic — turning a flat list of
// per-owner balances into the candidate list SelectStakeValidator (see
// stake_selection.h) consumes. It deliberately does NOT walk the real UTXO
// set — this chain has no address-balance index yet (only txindex/
// blockfilterindex exist, see src/index/), and building one is a separate,
// larger piece of work than the aggregation rule itself. See PROGRESS.md.

#ifndef BITCOIN_QTRN_STAKE_POOL_H
#define BITCOIN_QTRN_STAKE_POOL_H

#include <amount.h>
#include <qtrn/stake_selection.h>
#include <uint256.h>

#include <vector>

namespace qtrn {

//! Minimum balance to stake solo, as one's own StakeCandidate (spec §6.3).
//! Below this, a balance must be folded into a virtual pool instead.
static const CAmount SOLO_STAKE_MINIMUM = 100 * COIN;

//! Fixed number of virtual pools (design decision: deterministic assignment,
//! no on-chain pool registration — see DeterministicPoolId).
static const uint32_t NUM_VIRTUAL_POOLS = 64;

//! Deterministically assigns any owner id to one of NUM_VIRTUAL_POOLS pools —
//! no on-chain registration transaction, no registry index. Every node
//! computes the same answer from the owner id alone.
//!
//! This is deliberately not a user choice. For a pool that pays out
//! proportionally to member weight, an owner's expected reward reduces to
//! (their balance / total network stake weight) regardless of which pool
//! they land in — the pool only affects variance (a smaller pool wins less
//! often but pays out more when it does), never expected value. Since
//! there's nothing to gain by landing in a particular pool, there's no
//! incentive to grind for one, and no reason to spend an on-chain
//! transaction (or build a registry index) just to let an owner pick.
uint256 DeterministicPoolId(const uint256& ownerId);

//! One contribution to the stake-candidate set before aggregation — e.g. one
//! UTXO's worth of a balance. Multiple entries with the same ownerId are
//! expected (a real address will usually hold more than one UTXO) and get
//! summed by AssembleStakeCandidates before any solo/pool decision is made.
struct StakeCandidateBalance {
    uint256 ownerId;
    CAmount amount = 0;
};

//! Groups `balances` by ownerId (summing amounts), then:
//!   - an owner whose summed total is >= SOLO_STAKE_MINIMUM becomes their own
//!     candidate (id = ownerId, weight = total). Pooling exists to bound
//!     per-block computation for the long tail of small holders; once a
//!     balance clears the solo threshold it stays individually identifiable
//!     rather than being folded into a pool.
//!   - a sub-threshold owner is folded into DeterministicPoolId(ownerId)'s
//!     running sum (weight = sum of all that pool's members' totals). There
//!     is no "opt out" — every sub-threshold balance lands in exactly one of
//!     the NUM_VIRTUAL_POOLS pools.
//!   - a non-positive total is dropped (not eligible either way).
std::vector<StakeCandidate> AssembleStakeCandidates(const std::vector<StakeCandidateBalance>& balances);

} // namespace qtrn

#endif // BITCOIN_QTRN_STAKE_POOL_H
