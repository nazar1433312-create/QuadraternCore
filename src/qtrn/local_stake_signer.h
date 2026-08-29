// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// testnet-1 scope decision (see PROGRESS.md): the spec's real design (§6.2)
// has the miner ask the network's selected validator to sign remotely, with
// a 15-second timeout and liveness-fallback chain — that requires new P2P
// message types and async waiting inside block assembly, and is deferred to
// testnet-2/3. For testnet-1, a node that happens to locally hold one of the
// genesis stakers' private keys (contrib/testnet1-genesis-stakers.md) can
// sign for itself instead, with no network round-trip. This module is that
// local-signing path, and nothing else — it never talks to the network.

#ifndef BITCOIN_QTRN_LOCAL_STAKE_SIGNER_H
#define BITCOIN_QTRN_LOCAL_STAKE_SIGNER_H

#include <key.h>
#include <pubkey.h>
#include <qtrn/stake_selection.h>
#include <uint256.h>

#include <cstdint>
#include <map>
#include <vector>

namespace qtrn {

//! Holds whatever private keys this node happens to have on hand, indexed by
//! the StakeCandidate::id they sign for (StakeValidatorIdFromPubKey of the
//! key's own pubkey — see stake_commitment.h). Purely local and in-memory:
//! this class never persists, transmits, or logs a key.
class LocalStakeSigner
{
public:
    //! Registers `key` under StakeValidatorIdFromPubKey(key.GetPubKey()).
    //! Overwrites any previously-registered key for the same id.
    void AddKey(const CKey& key);

    //! Whether a previously-added key can sign for `candidateId`.
    bool HasKeyFor(const uint256& candidateId) const;

    //! Tries attempts 0..maxAttempt (inclusive) in order against
    //! SelectStakeValidator(seed-for-that-attempt, candidates); returns the
    //! first attempt whose winner this signer holds a key for, filling
    //! `attemptOut`/`winnerIdOut`. Returns false if none of the tried
    //! attempts are locally signable (e.g. every winner in range is a pool
    //! or another node's genesis staker).
    bool TryFindSignableAttempt(const uint256& primarySeed, const std::vector<StakeCandidate>& candidates, uint32_t maxAttempt, uint32_t& attemptOut, uint256& winnerIdOut) const;

    //! Signs `signingHash` with the key registered for `candidateId`, filling
    //! `sigOut`/`pubKeyOut`. Returns false if no such key is registered or
    //! signing itself fails.
    bool Sign(const uint256& candidateId, const uint256& signingHash, std::vector<unsigned char>& sigOut, CPubKey& pubKeyOut) const;

private:
    std::map<uint256, CKey> m_keysById;
};

} // namespace qtrn

#endif // BITCOIN_QTRN_LOCAL_STAKE_SIGNER_H
