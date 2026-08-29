// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <qtrn/lwma_difficulty.h>
#include <qtrn/pow_algo.h>
#include <uint256.h>

#include <algorithm>

namespace {

// Walks back from pindexLast collecting up to LWMA_WINDOW most recent blocks
// mined with the same algo as `algo`, oldest-to-newest (LwmaNextTarget's
// required order) — the three PoW channels race for the same chain slot
// (spec §6.1, PROGRESS-testnet-2.md) but each retargets from its own
// algo-only block history, never mixing in another channel's solve times.
std::vector<qtrn::AlgoBlockSample> CollectAlgoSamples(const CBlockIndex* pindexLast, qtrn::PowAlgo algo)
{
    std::vector<qtrn::AlgoBlockSample> samples;
    samples.reserve(qtrn::LWMA_WINDOW);
    for (const CBlockIndex* pindex = pindexLast; pindex && samples.size() < qtrn::LWMA_WINDOW; pindex = pindex->pprev) {
        if (qtrn::GetPowAlgo(pindex->nVersion) == algo) {
            samples.push_back({pindex->GetBlockTime(), pindex->nBits});
        }
    }
    std::reverse(samples.begin(), samples.end());
    return samples;
}

} // namespace

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);

    if (params.fPowNoRetargeting) {
        // regtest: keep mining trivially cheap, no algo-aware retargeting.
        return pindexLast->nBits;
    }

    const qtrn::PowAlgo algo = qtrn::GetPowAlgo(pblock->nVersion);
    // An invalid (unassigned 4th) algo id has no per-algo powLimit slot to
    // read — fall back to the chain-wide limit rather than reading out of
    // bounds; ContextualCheckBlockHeader rejects such a block on other
    // grounds regardless; the caller-facing behavior of this function must
    // still never crash on it.
    const uint256& powLimitForAlgo = qtrn::IsValidPowAlgo(algo) ? params.PowLimitFor(algo) : params.powLimit;

    const auto samples = CollectAlgoSamples(pindexLast, algo);

    if (params.fPowAllowMinDifficultyBlocks && !samples.empty() &&
        pblock->GetBlockTime() > samples.back().nTime + params.nPowTargetSpacingPerAlgo * 2) {
        // Testnet bring-up rule, scoped to one algo's own channel: if this
        // channel in particular has gone quiet (no miner on it right now),
        // allow it to mine at its own floor difficulty instead of stalling
        // that channel while the other two keep producing blocks normally.
        return UintToArith256(powLimitForAlgo).GetCompact();
    }

    return qtrn::LwmaNextTarget(samples, params.nPowTargetSpacingPerAlgo, powLimitForAlgo);
}

// Inherited Litecoin's fixed-interval retargeting — GetNextWorkRequired no
// longer calls this (replaced by per-algo LWMA above); kept only because
// test/pow_tests.cpp and test/fuzz/pow.cpp still exercise it directly
// against the legacy formula.
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    arith_uint256 bnNew;
    arith_uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
    bnOld = bnNew;
    // Litecoin: intermediate uint256 can overflow by 1 bit
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    bool fShift = bnNew.bits() > bnPowLimit.bits() - 1;
    if (fShift)
        bnNew >>= 1;
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;
    if (fShift)
        bnNew <<= 1;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
