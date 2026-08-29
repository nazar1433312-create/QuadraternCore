// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// testnet-2: GetNextWorkRequired() wired up to per-algo LWMA
// (qtrn/lwma_difficulty.h) — this suite exercises the real production entry
// point (chain.h CBlockIndex history + pow.h), not just the isolated LWMA
// math (already covered by qtrn_lwma_difficulty_tests.cpp). The thing worth
// proving here specifically is the channel-isolation walk-back in pow.cpp:
// CollectAlgoSamples must only ever see blocks mined by the SAME algo as the
// block being built, skipping over interleaved blocks from the other two
// channels on the shared chain.

#include <chain.h>
#include <chainparams.h>
#include <pow.h>
#include <qtrn/pow_algo.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

using namespace qtrn;

namespace {

// Builds a synthetic chain of `count` blocks, cycling through `algos` in
// order (e.g. {SHA256D, PROGPOW} alternates every block) so the walk-back
// logic has real interleaving to skip over, exactly like three channels
// racing for the same chain slot would produce in practice.
std::vector<CBlockIndex> BuildInterleavedChain(int64_t startTime, int64_t solveTime, size_t count,
                                                const std::vector<PowAlgo>& algos, uint32_t nBits)
{
    std::vector<CBlockIndex> blocks(count);
    for (size_t i = 0; i < count; ++i) {
        blocks[i].pprev = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = static_cast<int>(i);
        blocks[i].nTime = startTime + static_cast<int64_t>(i) * solveTime;
        blocks[i].nBits = nBits;
        blocks[i].nVersion = SetPowAlgo(0, algos[i % algos.size()]);
    }
    return blocks;
}

CBlockHeader MakeCandidate(int64_t nTime, PowAlgo algo)
{
    CBlockHeader header;
    header.nVersion = SetPowAlgo(0, algo);
    header.nTime = nTime;
    return header;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(qtrn_pow_retarget_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(bring_up_returns_that_algos_powlimit_when_channel_has_no_history)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::TESTNET);
    const auto& params = chainParams->GetConsensus();

    // A one-block chain, all mined with SHA256D — a PROGPOW candidate has
    // zero history on its own channel, so LwmaNextTarget's bring-up rule
    // (samples.size() < 2) should hand back PROGPOW's own powLimit, not
    // SHA256D's (they happen to be equal in testnet params today, but the
    // point is which slot gets read).
    auto chain = BuildInterleavedChain(1000, params.nPowTargetSpacingPerAlgo, 1, {PowAlgo::SHA256D},
                                        UintToArith256(params.PowLimitFor(PowAlgo::SHA256D)).GetCompact());
    const CBlockHeader candidate = MakeCandidate(chain.back().nTime + params.nPowTargetSpacingPerAlgo, PowAlgo::PROGPOW);

    const unsigned int next = GetNextWorkRequired(&chain.back(), &candidate, params);
    BOOST_CHECK_EQUAL(next, UintToArith256(params.PowLimitFor(PowAlgo::PROGPOW)).GetCompact());
}

BOOST_AUTO_TEST_CASE(retarget_only_sees_same_algo_blocks_in_an_interleaved_chain)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::TESTNET);
    const auto& params = chainParams->GetConsensus();
    const uint32_t baseCompact = UintToArith256(params.PowLimitFor(PowAlgo::SHA256D)).GetCompact();

    // Three channels round-robin, each on-schedule for its OWN spacing —
    // consecutive same-algo blocks are 3 * nPowTargetSpacingPerAlgo apart in
    // wall-clock time (2 other-algo blocks land between them), which is
    // exactly what "channel keeping to its own schedule" looks like on one
    // shared chain. Enough rounds to fill a real window.
    const std::vector<PowAlgo> rotation{PowAlgo::SHA256D, PowAlgo::PROGPOW, PowAlgo::RANDOMX};
    auto chain = BuildInterleavedChain(1000, params.nPowTargetSpacingPerAlgo, 90, rotation, baseCompact);

    // Next block up is SHA256D again (chain.size()=90 is divisible by 3, so
    // the rotation lands back on SHA256D for slot 90).
    const CBlockHeader candidate = MakeCandidate(chain.back().nTime + params.nPowTargetSpacingPerAlgo, PowAlgo::SHA256D);
    const unsigned int next = GetNextWorkRequired(&chain.back(), &candidate, params);

    // Every SHA256D block in this chain is exactly nPowTargetSpacingPerAlgo
    // apart on ITS OWN channel (the interleaved PROGPOW/RANDOMX blocks are
    // invisible to that channel's LWMA) — on-schedule solve times should
    // leave the target roughly unchanged, the same tolerance band used in
    // qtrn_lwma_difficulty_tests.cpp's on-target case. If the walk-back
    // leaked other-algo blocks in, the mixed solve-time accounting would
    // push this well outside a tight band instead.
    arith_uint256 nextTarget;
    nextTarget.SetCompact(next);
    const arith_uint256 base = UintToArith256(params.PowLimitFor(PowAlgo::SHA256D));
    BOOST_CHECK(nextTarget <= base + base / 20);
    BOOST_CHECK(nextTarget >= base - base / 20);
}

BOOST_AUTO_TEST_CASE(no_retargeting_flag_bypasses_lwma_entirely)
{
    // Regtest: fPowNoRetargeting short-circuits to pindexLast->nBits
    // regardless of algo, same contract the old fixed-interval code gave.
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::REGTEST);
    const auto& params = chainParams->GetConsensus();
    BOOST_REQUIRE(params.fPowNoRetargeting);

    auto chain = BuildInterleavedChain(1000, params.nPowTargetSpacingPerAlgo, 5, {PowAlgo::RANDOMX}, 0x207fffff);
    const CBlockHeader candidate = MakeCandidate(chain.back().nTime + params.nPowTargetSpacingPerAlgo, PowAlgo::PROGPOW);

    BOOST_CHECK_EQUAL(GetNextWorkRequired(&chain.back(), &candidate, params), chain.back().nBits);
}

BOOST_AUTO_TEST_CASE(min_difficulty_rule_is_scoped_to_the_quiet_channel_only)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::TESTNET);
    const auto& params = chainParams->GetConsensus();
    BOOST_REQUIRE(params.fPowAllowMinDifficultyBlocks);

    // Only SHA256D has mined so far; a PROGPOW candidate arrives long after
    // (in PROGPOW-channel terms, there IS no prior PROGPOW block, so the
    // "quiet channel" gap is measured from... there's no last sample at
    // all, which the bring-up path already handles). This test instead
    // covers the case where a channel HAS history but has gone quiet: give
    // PROGPOW one old block, then let a lot of PROGPOW-channel time pass
    // (with SHA256D blocks continuing normally in between) before the next
    // PROGPOW candidate arrives.
    std::vector<CBlockIndex> blocks(3);
    blocks[0].pprev = nullptr;
    blocks[0].nHeight = 0;
    blocks[0].nTime = 1000;
    blocks[0].nVersion = SetPowAlgo(0, PowAlgo::PROGPOW);
    blocks[0].nBits = UintToArith256(params.PowLimitFor(PowAlgo::PROGPOW)).GetCompact();

    blocks[1].pprev = &blocks[0];
    blocks[1].nHeight = 1;
    blocks[1].nTime = 1000 + params.nPowTargetSpacingPerAlgo;
    blocks[1].nVersion = SetPowAlgo(0, PowAlgo::SHA256D);
    blocks[1].nBits = UintToArith256(params.PowLimitFor(PowAlgo::SHA256D)).GetCompact();

    blocks[2].pprev = &blocks[1];
    blocks[2].nHeight = 2;
    blocks[2].nTime = blocks[1].nTime + params.nPowTargetSpacingPerAlgo;
    blocks[2].nVersion = SetPowAlgo(0, PowAlgo::SHA256D);
    blocks[2].nBits = UintToArith256(params.PowLimitFor(PowAlgo::SHA256D)).GetCompact();

    // PROGPOW candidate arrives well over 2x its own target spacing after
    // PROGPOW's own last (and only) block — the SHA256D blocks in between
    // are on a different channel and must not count as "this channel is
    // still active".
    const int64_t gap = params.nPowTargetSpacingPerAlgo * 3;
    const CBlockHeader candidate = MakeCandidate(blocks[0].nTime + gap, PowAlgo::PROGPOW);

    const unsigned int next = GetNextWorkRequired(&blocks.back(), &candidate, params);
    BOOST_CHECK_EQUAL(next, UintToArith256(params.PowLimitFor(PowAlgo::PROGPOW)).GetCompact());
}

BOOST_AUTO_TEST_SUITE_END()
