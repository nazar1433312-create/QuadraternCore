// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// testnet-2: dispatches a block header to the right PoW hash function for
// its algo (see pow_algo.h). SHA256D is real and final — it's the same
// SerializeHash CBlockHeader::GetHash() already uses. PROGPOW and RANDOMX
// are STUBS: deterministic, cheap placeholder hashes, not the real
// algorithms. They exist so the surrounding machinery (difficulty
// retargeting, fork-choice weighting, tie-break) can be built and tested now
// without first vendoring and trusting two large third-party libraries
// (Monero's librandomx, the ProgPoW reference implementation) — see
// PROGRESS-testnet-2.md for the reasoning. Swapping in the real algorithms
// later should only require replacing the two stub function bodies below;
// nothing that calls ComputeAlgoHash needs to change.
//
// HARD REQUIREMENT: a stub must never reach a genesis block, testnet
// launch, or mainnet. IsAlgoHashReal() lets callers assert this explicitly
// wherever it matters (chainparams validation, mainnet build flags, etc.)
// rather than relying on nobody forgetting.

#ifndef BITCOIN_QTRN_ALGO_HASH_H
#define BITCOIN_QTRN_ALGO_HASH_H

#include <primitives/block.h>
#include <qtrn/pow_algo.h>
#include <uint256.h>

namespace qtrn {

//! Computes the PoW hash for `header` under whatever algo GetPowAlgo(header.nVersion)
//! selects. Returns uint256() (null) for an invalid/unassigned algo id —
//! callers must treat that as an automatic PoW failure, same as any hash
//! that doesn't meet target.
uint256 ComputeAlgoHash(const CBlockHeader& header);

//! Whether `algo`'s ComputeAlgoHash path is the real, final algorithm
//! (currently true only for SHA256D) rather than a testnet-2 placeholder.
bool IsAlgoHashReal(PowAlgo algo);

} // namespace qtrn

#endif // BITCOIN_QTRN_ALGO_HASH_H
