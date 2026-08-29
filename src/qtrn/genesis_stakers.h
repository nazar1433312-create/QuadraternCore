// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// testnet-1 scope only (see PROGRESS.md's "scope decision" note). This
// bridges Consensus::Params::genesisStakers (chainparams.cpp) to the
// stake_pool module. Meant to be deleted once testnet-2's real
// address-balance index replaces it as the candidate-list source.

#ifndef BITCOIN_QTRN_GENESIS_STAKERS_H
#define BITCOIN_QTRN_GENESIS_STAKERS_H

#include <amount.h>
#include <pubkey.h>
#include <qtrn/stake_pool.h>

#include <utility>
#include <vector>

namespace qtrn {

//! Converts the chain's fixed genesis staker list into StakeCandidateBalance
//! entries ready for AssembleStakeCandidates, using the same pubkey->id
//! mapping stake_commitment.h's StakeValidatorIdFromPubKey uses — so a
//! genesis staker's signed commitment verifies against exactly the id this
//! function assigns them.
std::vector<StakeCandidateBalance> GenesisStakersToBalances(const std::vector<std::pair<CPubKey, CAmount>>& genesisStakers);

} // namespace qtrn

#endif // BITCOIN_QTRN_GENESIS_STAKERS_H
