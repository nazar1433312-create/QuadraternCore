// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/lwma_difficulty.h>

#include <arith_uint256.h>

#include <algorithm>

namespace qtrn {

namespace {

arith_uint256 DecodeTargetOrFallback(uint32_t nBits, const arith_uint256& fallback)
{
    bool fNegative = false;
    bool fOverflow = false;
    arith_uint256 target;
    target.SetCompact(nBits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || target == 0) return fallback; // malformed input — never crash/UB on it
    return target;
}

arith_uint256 ClampToPowLimit(arith_uint256 target, const arith_uint256& powLimit)
{
    if (target == 0) target = 1; // a null target would mean "infinitely easy" — nonsensical
    if (target > powLimit) target = powLimit;
    return target;
}

} // namespace

uint32_t LwmaNextTarget(const std::vector<AlgoBlockSample>& samples, int64_t targetSpacing, const uint256& powLimit)
{
    const arith_uint256 powLimitArith = UintToArith256(powLimit);

    // Bring-up (no history yet) or not enough samples to measure even one
    // solve-time interval: nothing to retarget from.
    if (samples.size() < 2) {
        return powLimitArith.GetCompact();
    }

    const int64_t n = static_cast<int64_t>(samples.size()); // caller caps this at LWMA_WINDOW

    // Average target across all n samples (each sample's own recorded
    // difficulty, independent of the solve-time-interval accounting below).
    arith_uint256 sumTarget = 0;
    for (const auto& sample : samples) {
        sumTarget += DecodeTargetOrFallback(sample.nBits, powLimitArith);
    }
    const arith_uint256 avgTarget = sumTarget / static_cast<uint64_t>(n);

    // Weighted solve-time average over the (n-1) gaps between consecutive
    // samples — weight = gap index, 1..(n-1), so the most recent gap counts
    // most. Each gap is clamped to [1, 6x target] against timestamp
    // manipulation, the standard LWMA anti-manipulation bound.
    const int64_t maxSolveTime = targetSpacing * 6;
    int64_t sumWeightedSolvetimes = 0;
    for (int64_t i = 1; i < n; ++i) {
        int64_t solveTime = samples[i].nTime - samples[i - 1].nTime;
        solveTime = std::max<int64_t>(1, std::min<int64_t>(maxSolveTime, solveTime));
        sumWeightedSolvetimes += solveTime * i;
    }
    const int64_t sumWeights = (n - 1) * n / 2;

    // next_target = avg_target * (sum of weighted solvetimes) / (targetSpacing * sum of weights)
    // — a higher-than-target weighted average solve time means blocks have
    // been arriving too slowly, so the target grows (easier); lower means
    // too fast, so it shrinks (harder).
    arith_uint256 nextTarget = avgTarget * arith_uint256(static_cast<uint64_t>(sumWeightedSolvetimes));
    nextTarget /= arith_uint256(static_cast<uint64_t>(targetSpacing * sumWeights));

    // Clamp the step versus the most recent sample's own target — spec
    // decision: +15% looser / -10% tighter, max, regardless of what the raw
    // LWMA math above produced.
    const arith_uint256 lastTarget = DecodeTargetOrFallback(samples.back().nBits, powLimitArith);
    const arith_uint256 maxUp = lastTarget + (lastTarget * arith_uint256(LWMA_MAX_INCREASE_PERCENT)) / arith_uint256(100);
    const arith_uint256 maxDown = lastTarget - (lastTarget * arith_uint256(LWMA_MAX_DECREASE_PERCENT)) / arith_uint256(100);

    if (nextTarget > maxUp) nextTarget = maxUp;
    if (nextTarget < maxDown) nextTarget = maxDown;

    return ClampToPowLimit(nextTarget, powLimitArith).GetCompact();
}

} // namespace qtrn
