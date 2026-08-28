// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// testnet-1: virtual staking pool aggregation (spec §6.3). This module only
// implements the pure grouping/weighting logic — turning a flat list of
// per-owner balances into the candidate list SelectStakeValidator (see
// stake_selection.h) consumes. It deliberately does NOT walk the real UTXO
// set or decide how a balance owner's pool choice gets recorded on-chain —
// this chain has no address-balance index yet (only txindex/blockfilterindex
// exist, see src/index/), and building one is a separate, larger piece of
// work than the aggregation rule itself. See PROGRESS.md.

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

//! One contribution to the stake-candidate set before aggregation — e.g. one
//! UTXO's worth of a balance, or a wallet-level summary of several. Multiple
//! entries with the same ownerId are expected (a real address will usually
//! hold more than one UTXO) and get summed by AssembleStakeCandidates before
//! any solo/pool decision is made.
//!
//! `poolId`/`hasPool` express the owner's standing choice to participate in a
//! virtual pool (spec §6.3: wallet UI, "participate in virtual pool #N"). How
//! that choice is actually recorded on-chain (a registration transaction? a
//! wallet-local default with no on-chain footprint, since every node just
//! needs to agree on the rule, not on an announcement?) is intentionally out
//! of scope here — see the file-level comment above.
struct StakeCandidateBalance {
    uint256 ownerId;
    CAmount amount = 0;
    uint256 poolId;
    bool hasPool = false;
};

//! Groups `balances` by ownerId (summing amounts), then:
//!   - an owner whose summed total is >= SOLO_STAKE_MINIMUM always becomes
//!     their own candidate (id = ownerId, weight = total) — regardless of any
//!     declared pool. Pooling exists to bound per-block computation for the
//!     long tail of small holders; it isn't needed once a balance clears the
//!     solo threshold, so large balances stay individually identifiable
//!     rather than being silently folded away.
//!   - a sub-threshold owner with hasPool folds their total into that pool's
//!     running sum (id = poolId, weight = sum of all its members' totals).
//!   - a sub-threshold owner with no declared pool is dropped entirely: not
//!     a low-weight candidate, not a candidate at all. This is a deliberate
//!     asymmetry with SelectStakeValidator's own "non-positive weight never
//!     wins" handling — here the balance doesn't even count towards a pool's
//!     weight, since it was never aggregated into one.
//! If the same owner reports inconsistent pool choices across entries, the
//! first entry's choice for that owner wins — callers should tag every entry
//! for one owner identically (a real assembler should apply one wallet-level
//! choice uniformly across all of an owner's UTXOs, not decide per-UTXO).
std::vector<StakeCandidate> AssembleStakeCandidates(const std::vector<StakeCandidateBalance>& balances);

} // namespace qtrn

#endif // BITCOIN_QTRN_STAKE_POOL_H
