// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/local_stake_signer.h>

#include <qtrn/stake_commitment.h>

namespace qtrn {

LocalStakeSigner g_local_stake_signer;

void LocalStakeSigner::AddKey(const CKey& key)
{
    m_keysById[StakeValidatorIdFromPubKey(key.GetPubKey())] = key;
}

bool LocalStakeSigner::HasKeyFor(const uint256& candidateId) const
{
    return m_keysById.count(candidateId) != 0;
}

bool LocalStakeSigner::TryFindSignableAttempt(const uint256& primarySeed, const std::vector<StakeCandidate>& candidates, uint32_t maxAttempt, uint32_t& attemptOut, uint256& winnerIdOut) const
{
    for (uint32_t attempt = 0; attempt <= maxAttempt; ++attempt) {
        const uint256 seed = (attempt == 0) ? primarySeed : DeriveFallbackSeed(primarySeed, attempt);
        const size_t idx = SelectStakeValidator(seed, candidates);
        if (idx >= candidates.size()) continue; // no eligible candidate at all for this seed
        if (HasKeyFor(candidates[idx].id)) {
            attemptOut = attempt;
            winnerIdOut = candidates[idx].id;
            return true;
        }
    }
    return false;
}

bool LocalStakeSigner::Sign(const uint256& candidateId, const uint256& signingHash, std::vector<unsigned char>& sigOut, CPubKey& pubKeyOut) const
{
    const auto it = m_keysById.find(candidateId);
    if (it == m_keysById.end()) return false;
    if (!it->second.Sign(signingHash, sigOut)) return false;
    pubKeyOut = it->second.GetPubKey();
    return true;
}

} // namespace qtrn
