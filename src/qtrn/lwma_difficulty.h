// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// testnet-2: per-algorithm difficulty retargeting (spec §6.1 design
// decisions — see PROGRESS-testnet-2.md). Zawy-style LWMA (linear weighted
// moving average of solve times, weighted toward recent blocks) computed
// over ONE algorithm's own last N blocks — never mixed with another
// algorithm's blocks, since each of the three channels retargets
// independently. Deliberately kept free of CBlockIndex/chain-state types so
// it's testable with plain synthetic sample data, the same pattern used for
// stake_selection.h etc. in testnet-1.

#ifndef BITCOIN_QTRN_LWMA_DIFFICULTY_H
#define BITCOIN_QTRN_LWMA_DIFFICULTY_H

#include <uint256.h>

#include <cstdint>
#include <vector>

namespace qtrn {

//! One historical block of a single PoW algorithm's own chain of blocks —
//! never blocks from a different algorithm. Oldest-to-newest ordering is the
//! caller's responsibility; LwmaNextTarget assumes `samples.back()` is the
//! most recent.
struct AlgoBlockSample {
    int64_t nTime;
    uint32_t nBits; // that block's compact difficulty target
};

//! Fixed design parameters (spec decision, see PROGRESS-testnet-2.md):
//! 120-block window, and the maximum a single retarget step may move the
//! target versus the immediately preceding block's — +15% looser (easier),
//! -10% tighter (harder). Expressed directly as target-value bounds (not
//! difficulty, which is the inverse) since that's what every comparison in
//! this codebase already operates on.
static constexpr size_t LWMA_WINDOW = 120;
static constexpr int64_t LWMA_MAX_INCREASE_PERCENT = 15; // target may grow at most +15% per block
static constexpr int64_t LWMA_MAX_DECREASE_PERCENT = 10; // target may shrink at most -10% per block

//! Computes the next compact target for one algorithm's channel from that
//! algorithm's own recent block history (oldest-to-newest in `samples`,
//! already capped by the caller to at most LWMA_WINDOW entries — passing
//! more than that is the caller's bug, not silently handled here).
//!
//! `targetSpacing` is that algorithm's own individual target interval in
//! seconds (spec: overall chain block_time × number of channels — see
//! Consensus::Params::nPowTargetSpacingPerAlgo).
//!
//! Returns `powLimit`'s compact form when `samples` has fewer than 2 entries
//! (bring-up: not enough history yet for this channel to measure even one
//! solve-time interval — e.g. right after genesis before this algo has
//! mined a second block). Never returns a target outside [some positive
//! value, powLimit] — always clamped to at least as hard as powLimit allows.
uint32_t LwmaNextTarget(const std::vector<AlgoBlockSample>& samples, int64_t targetSpacing, const uint256& powLimit);

} // namespace qtrn

#endif // BITCOIN_QTRN_LWMA_DIFFICULTY_H
