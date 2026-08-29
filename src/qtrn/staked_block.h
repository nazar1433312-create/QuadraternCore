// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QTRN_STAKED_BLOCK_H
#define BITCOIN_QTRN_STAKED_BLOCK_H

#include <primitives/block.h>
#include <qtrn/local_stake_signer.h>
#include <qtrn/stake_selection.h>
#include <uint256.h>

#include <cstdint>
#include <vector>

namespace qtrn {

//! Given a freshly-built block template (e.g. from
//! BlockAssembler::CreateNewBlock — no stake commitment yet) and the current
//! candidate set, tries to produce a copy with a valid stake commitment
//! attached (coinbase output added, merkle root recomputed) using a key
//! `signer` holds locally — see local_stake_signer.h for why this is
//! local-only rather than a network round-trip in testnet-1.
//!
//! Does not touch nNonce or do any PoW mining — that still happens after
//! this, on the returned block's final coinbase/merkle root (safe to do in
//! either order relative to signing — see ComputeStakeSigningHash's
//! nNonce-independence note).
//!
//! Returns false (leaving `outBlock` untouched) if no candidate within
//! maxAttempt is locally signable.
bool TryBuildStakedBlock(const CBlock& templateBlock, const uint256& primarySeed, const std::vector<StakeCandidate>& candidates, const LocalStakeSigner& signer, uint32_t maxAttempt, CBlock& outBlock);

} // namespace qtrn

#endif // BITCOIN_QTRN_STAKED_BLOCK_H
