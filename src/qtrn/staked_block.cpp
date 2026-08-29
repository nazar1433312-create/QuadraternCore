// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/staked_block.h>

#include <consensus/merkle.h>
#include <qtrn/stake_commitment.h>

namespace qtrn {

bool TryBuildStakedBlock(const CBlock& templateBlock, const uint256& primarySeed, const std::vector<StakeCandidate>& candidates, const LocalStakeSigner& signer, uint32_t maxAttempt, CBlock& outBlock)
{
    uint32_t attempt;
    uint256 winnerId;
    if (!signer.TryFindSignableAttempt(primarySeed, candidates, maxAttempt, attempt, winnerId)) {
        return false;
    }

    const uint256 signingHash = ComputeStakeSigningHash(templateBlock);

    StakeCommitment commitment;
    commitment.attempt = attempt;
    if (!signer.Sign(winnerId, signingHash, commitment.signature, commitment.validatorPubKey)) {
        return false; // shouldn't happen: TryFindSignableAttempt already confirmed a key exists
    }

    CMutableTransaction coinbase(*templateBlock.vtx[0]);
    coinbase.vout.emplace_back(0, BuildStakeCommitmentScript(commitment));
    if (coinbase.vout.back().scriptPubKey.empty()) return false; // malformed commitment, shouldn't happen

    outBlock = templateBlock;
    outBlock.vtx[0] = MakeTransactionRef(std::move(coinbase));
    outBlock.hashMerkleRoot = BlockMerkleRoot(outBlock);
    return true;
}

} // namespace qtrn
