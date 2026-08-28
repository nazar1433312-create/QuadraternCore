// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// testnet-1: PoS validator selection (spec §6.2/§6.3).
//
// A mined block only becomes valid once a randomly selected staker signs it.
// The selection is deterministic and verifiable by any node: everyone who
// knows the previous block's hash and the current stake weights can compute
// the same answer independently, so no party chooses or reveals it.
//
// This module is intentionally self-contained (no dependency on validation.cpp,
// net_processing.cpp, or the wallet) so it can be written, reviewed and unit
// tested in isolation before anything wires it into block-acceptance rules.
// That wiring — the actual consensus rule "no signature => invalid block",
// plus the wire format for carrying the signature — is a deliberate follow-up
// step, not bundled into this pass. See ROADMAP.md.

#ifndef BITCOIN_QTRN_STAKE_SELECTION_H
#define BITCOIN_QTRN_STAKE_SELECTION_H

#include <amount.h>
#include <uint256.h>

#include <cstdint>
#include <vector>

namespace qtrn {

//! One candidate in the stake-weighted validator lottery. `weight` is the
//! candidate's effective stake: either a wallet's own balance, or a virtual
//! pool's aggregated weight (spec §6.3), always taken from the balance
//! snapshot as of the end of block N-1 — never a live/current balance — so
//! that a deposit landing in the block being validated cannot buy a share of
//! that same block's selection (front-running fix from the spec review).
struct StakeCandidate {
    uint256 id;       // hash of the staking address/pool identifier
    CAmount weight;    // snapshotted stake weight, in satoshi-equivalent Quad units
};

//! Deterministically selects one candidate, weighted by `weight`, using
//! `seedHash` (the previous block's hash, or its `attempt`-th fallback
//! derivation — see DeriveFallbackSeed below) as the sole source of
//! randomness. Returns candidates.size() if `candidates` is empty or all
//! weights are non-positive (caller must treat that as "no eligible
//! validator").
//!
//! The result does NOT depend on the order `candidates` is passed in —
//! internally, candidates are walked in a canonical order (ascending by
//! `id`) before the weighted draw, so callers on different nodes can never
//! diverge just because they assembled the vector differently. Same
//! seedHash + same (id, weight) set always yields the same winner on every
//! node — this is what makes the selection independently verifiable rather
//! than something any single party reveals.
size_t SelectStakeValidator(const uint256& seedHash, const std::vector<StakeCandidate>& candidates);

//! Liveness fallback (spec §6.2): if the validator selected with the block's
//! primary seed does not produce a valid signature within the 15-second
//! window, the network derives the next seed via this function (`attempt` =
//! 1, 2, 3, ...) and re-runs SelectStakeValidator with it to pick a
//! stand-in, instead of stalling the chain on one unresponsive staker.
uint256 DeriveFallbackSeed(const uint256& seedHash, uint32_t attempt);

} // namespace qtrn

#endif // BITCOIN_QTRN_STAKE_SELECTION_H
