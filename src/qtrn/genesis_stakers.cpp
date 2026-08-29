// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/genesis_stakers.h>

#include <qtrn/stake_commitment.h>

namespace qtrn {

std::vector<StakeCandidateBalance> GenesisStakersToBalances(const std::vector<std::pair<CPubKey, CAmount>>& genesisStakers)
{
    std::vector<StakeCandidateBalance> result;
    result.reserve(genesisStakers.size());
    for (const auto& staker : genesisStakers) {
        StakeCandidateBalance b;
        b.ownerId = StakeValidatorIdFromPubKey(staker.first);
        b.amount = staker.second;
        result.push_back(b);
    }
    return result;
}

} // namespace qtrn
