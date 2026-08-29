// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/lwma_difficulty.h>

#include <arith_uint256.h>

#include <boost/test/unit_test.hpp>

using namespace qtrn;

namespace {

const uint256 POW_LIMIT = uint256S("0000ffff00000000000000000000000000000000000000000000000000000000");
const int64_t TARGET_SPACING = 180; // 3 minutes, per-algo target

// The base target expressed as its own compact round-trip, so every sample
// built from it is internally consistent (compact form only preserves ~3
// significant bytes — comparing against a non-round-tripped value would be
// comparing apples to a slightly different orange).
uint32_t BaseCompact()
{
    return UintToArith256(POW_LIMIT).GetCompact();
}

std::vector<AlgoBlockSample> MakeSamples(int64_t startTime, int64_t solveTime, size_t count, uint32_t nBits = 0)
{
    if (nBits == 0) nBits = BaseCompact();
    std::vector<AlgoBlockSample> samples;
    samples.reserve(count);
    int64_t t = startTime;
    for (size_t i = 0; i < count; ++i) {
        samples.push_back({t, nBits});
        t += solveTime;
    }
    return samples;
}

arith_uint256 ToTarget(uint32_t nBits)
{
    arith_uint256 t;
    t.SetCompact(nBits);
    return t;
}

} // namespace

BOOST_AUTO_TEST_SUITE(qtrn_lwma_difficulty_tests)

BOOST_AUTO_TEST_CASE(too_few_samples_returns_powlimit)
{
    BOOST_CHECK_EQUAL(LwmaNextTarget({}, TARGET_SPACING, POW_LIMIT), UintToArith256(POW_LIMIT).GetCompact());
    BOOST_CHECK_EQUAL(LwmaNextTarget({{1000, BaseCompact()}}, TARGET_SPACING, POW_LIMIT), UintToArith256(POW_LIMIT).GetCompact());
}

BOOST_AUTO_TEST_CASE(is_deterministic)
{
    const auto samples = MakeSamples(1000, TARGET_SPACING, 30);
    BOOST_CHECK_EQUAL(LwmaNextTarget(samples, TARGET_SPACING, POW_LIMIT),
                       LwmaNextTarget(samples, TARGET_SPACING, POW_LIMIT));
}

BOOST_AUTO_TEST_CASE(on_target_solve_times_leave_difficulty_roughly_unchanged)
{
    // Every gap exactly equals targetSpacing: the network is producing
    // blocks exactly on schedule, so the target shouldn't move much.
    const auto samples = MakeSamples(1000, TARGET_SPACING, 30);
    const uint32_t next = LwmaNextTarget(samples, TARGET_SPACING, POW_LIMIT);
    const arith_uint256 nextTarget = ToTarget(next);
    const arith_uint256 base = ToTarget(BaseCompact());

    // Allow a little slack for compact-rounding, but it must land close to
    // unchanged — nowhere near the +15%/-10% clamp boundaries.
    BOOST_CHECK(nextTarget <= base + base / 20);  // within +5%
    BOOST_CHECK(nextTarget >= base - base / 20);  // within -5%
}

BOOST_AUTO_TEST_CASE(consistently_slow_blocks_loosen_the_target_up_to_the_clamp)
{
    // Blocks taking far longer than target => target should grow (easier),
    // clamped to at most +15% versus the last sample's own target.
    const auto samples = MakeSamples(1000, TARGET_SPACING * 20, 30);
    const uint32_t next = LwmaNextTarget(samples, TARGET_SPACING, POW_LIMIT);
    const arith_uint256 nextTarget = ToTarget(next);
    const arith_uint256 lastTarget = ToTarget(samples.back().nBits);

    BOOST_CHECK(nextTarget > lastTarget); // did loosen
    const arith_uint256 maxUp = lastTarget + (lastTarget * arith_uint256(LWMA_MAX_INCREASE_PERCENT)) / arith_uint256(100);
    BOOST_CHECK(nextTarget <= maxUp); // never past the clamp
}

BOOST_AUTO_TEST_CASE(consistently_fast_blocks_tighten_the_target_up_to_the_clamp)
{
    // Blocks arriving far faster than target => target should shrink
    // (harder), clamped to at most -10% versus the last sample's own target.
    const auto samples = MakeSamples(1000, TARGET_SPACING / 20, 30);
    const uint32_t next = LwmaNextTarget(samples, TARGET_SPACING, POW_LIMIT);
    const arith_uint256 nextTarget = ToTarget(next);
    const arith_uint256 lastTarget = ToTarget(samples.back().nBits);

    BOOST_CHECK(nextTarget < lastTarget); // did tighten
    const arith_uint256 maxDown = lastTarget - (lastTarget * arith_uint256(LWMA_MAX_DECREASE_PERCENT)) / arith_uint256(100);
    BOOST_CHECK(nextTarget >= maxDown); // never past the clamp
}

BOOST_AUTO_TEST_CASE(never_exceeds_powlimit_even_when_the_math_wants_looser)
{
    // Samples already sitting at powLimit, with slow solve times pushing for
    // an even looser (larger) target — must still clamp at powLimit.
    const auto samples = MakeSamples(1000, TARGET_SPACING * 20, 30, UintToArith256(POW_LIMIT).GetCompact());
    const uint32_t next = LwmaNextTarget(samples, TARGET_SPACING, POW_LIMIT);
    BOOST_CHECK(ToTarget(next) <= UintToArith256(POW_LIMIT));
}

BOOST_AUTO_TEST_CASE(malformed_nbits_falls_back_instead_of_crashing)
{
    // 0xff000000-style compact values decode as negative/overflowed in
    // Bitcoin's SetCompact — must not crash, must fall back sanely.
    std::vector<AlgoBlockSample> samples{
        {1000, 0xff123456},
        {1000 + TARGET_SPACING, 0xff123456},
    };
    uint32_t next = 0;
    BOOST_CHECK_NO_THROW(next = LwmaNextTarget(samples, TARGET_SPACING, POW_LIMIT));
    BOOST_CHECK(ToTarget(next) <= UintToArith256(POW_LIMIT));
}

BOOST_AUTO_TEST_CASE(respects_window_sized_input_at_the_documented_maximum)
{
    const auto samples = MakeSamples(1000, TARGET_SPACING, LWMA_WINDOW);
    uint32_t next = 0;
    BOOST_CHECK_NO_THROW(next = LwmaNextTarget(samples, TARGET_SPACING, POW_LIMIT));
    BOOST_CHECK(next != 0);
}

BOOST_AUTO_TEST_SUITE_END()
