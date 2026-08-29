// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// testnet-1: wire format for carrying the PoS validator's signature on a
// mined block (spec §6.2). This module only handles serializing, parsing and
// cryptographically verifying a commitment in isolation — it deliberately
// does not touch CBlock, merkle roots, or validation.cpp. See the note on
// signing-hash circularity below and in ROADMAP.md/PROGRESS.md for why that
// wiring is a separate, still-pending step.

#ifndef BITCOIN_QTRN_STAKE_COMMITMENT_H
#define BITCOIN_QTRN_STAKE_COMMITMENT_H

#include <primitives/block.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/script.h>
#include <uint256.h>

#include <cstdint>
#include <vector>

namespace qtrn {

// 4-byte marker identifying a Quadratern stake-validator commitment output,
// distinct from Litecoin/Bitcoin's own SegWit witness-commitment marker
// (0xaa 0x21 0xa9 0xed) so both can coexist in the same coinbase transaction
// (this chain activates segwit from genesis — see chainparams.cpp).
static constexpr unsigned char STAKE_COMMITMENT_MAGIC[4] = {0x51, 0x53, 0x54, 0x53}; // "QSTS"

//! Everything a node needs to check that the correct PoS validator signed off
//! on this block. Carried as a single OP_RETURN output in the block's
//! coinbase transaction — see BuildStakeCommitmentScript.
struct StakeCommitment {
    //! Which DeriveFallbackSeed() attempt selected this validator (0 = the
    //! block's primary seed, i.e. hash of the previous block; 1, 2, ... are
    //! liveness fallbacks — spec §6.2's 15-second timeout chain). Carried
    //! explicitly so a verifying node checks one specific seed instead of
    //! brute-forcing a range of attempts. Must fit in one byte (0-255) —
    //! there is no realistic scenario needing more than a handful of
    //! fallback rounds before something else is badly wrong with the network.
    uint32_t attempt = 0;
    //! Compressed pubkey of the winning validator. Must hash (via
    //! StakeValidatorIdFromPubKey, the same mapping the not-yet-built virtual
    //! staking pool module will use for StakeCandidate::id) to the id
    //! SelectStakeValidator() returned for this attempt's seed.
    CPubKey validatorPubKey;
    //! DER-encoded ECDSA signature (CKey::Sign's native output) over whatever
    //! signing hash the caller supplies to VerifyStakeCommitment. This module
    //! does not compute that hash itself.
    //!
    //! Why not just sign the block's own GetHash()? Because this commitment
    //! lives inside the coinbase transaction, which is part of the merkle
    //! root, which is part of the block header that GetHash() hashes —
    //! signing the final hash would mean the hash depends on the signature
    //! and the signature depends on the hash. The eventual caller (once this
    //! is wired into block assembly/validation) must instead sign a hash
    //! computed with this commitment's output excluded — the same
    //! "commit to a placeholder-stable value" trick SegWit's own witness
    //! commitment uses (see GenerateCoinbaseCommitment in validation.cpp).
    std::vector<unsigned char> signature;
};

//! Serializes `commitment` into an OP_RETURN scriptPubKey suitable for a
//! coinbase output. Returns an empty script if validatorPubKey isn't a valid
//! compressed key, `attempt` doesn't fit in a byte, or `signature` is empty —
//! callers must treat an empty result as a build failure.
CScript BuildStakeCommitmentScript(const StakeCommitment& commitment);

//! Finds and parses a Quadratern stake commitment among a coinbase
//! transaction's outputs. Returns false (leaving `out` untouched) if none is
//! present or the one found is malformed. "Not found" is not itself treated
//! as an error here — that judgment belongs to the eventual validation-rule
//! caller, once real block/candidate-list wiring exists.
bool FindStakeCommitment(const std::vector<CTxOut>& coinbaseOutputs, StakeCommitment& out);

//! Checks that `commitment.validatorPubKey` maps (via
//! StakeValidatorIdFromPubKey) to `expectedValidatorId`, and that
//! `commitment.signature` verifies under that pubkey over `signingHash`.
//! Does not know or care how signingHash was derived.
bool VerifyStakeCommitment(const StakeCommitment& commitment, const uint256& signingHash, const uint256& expectedValidatorId);

//! The hash function mapping a validator's pubkey to a StakeCandidate::id
//! (stake_selection.h) — exposed so this module and the virtual-staking-pool
//! module (candidate-list assembly, not yet built) agree on one mapping.
uint256 StakeValidatorIdFromPubKey(const CPubKey& pubKey);

//! The hash a stake commitment's signature actually covers. NOT block.GetHash()
//! — see the long comment on StakeCommitment::signature for why that would be
//! circular. Instead: take block.vtx[0] (the coinbase), strip out its stake
//! commitment output if one is present, recompute the block's merkle root
//! with that stripped coinbase substituted in place of the real one, and
//! hash a header built from the real header fields plus that merkle root.
//!
//! This one function serves both directions by construction: a miner calls
//! it on a block whose coinbase has no commitment yet (nothing to strip) to
//! get the hash the chosen validator must sign; a validating node calls it
//! on the fully assembled block it received (commitment present, now
//! stripped back out) to get the same hash back, to check against the
//! signature it found via FindStakeCommitment.
//!
//! Also zeroes `nNonce` before hashing — the nonce is found by PoW mining,
//! which can happen before or after the commitment is attached, and either
//! way the validator isn't vouching for a specific nonce (PoW itself already
//! secures that). Signing over the real, possibly-not-yet-found nonce would
//! mean a signature made before mining finished stopped verifying the moment
//! mining changed it.
uint256 ComputeStakeSigningHash(const CBlock& block);

} // namespace qtrn

#endif // BITCOIN_QTRN_STAKE_COMMITMENT_H
