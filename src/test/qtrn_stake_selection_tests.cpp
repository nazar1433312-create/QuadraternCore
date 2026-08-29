// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/stake_selection.h>

#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <map>

using namespace qtrn;

BOOST_AUTO_TEST_SUITE(qtrn_stake_selection_tests)

BOOST_AUTO_TEST_CASE(empty_candidate_list_has_no_winner)
{
    const uint256 seed = uint256S("aa");
    std::vector<StakeCandidate> none;
    BOOST_CHECK_EQUAL(SelectStakeValidator(seed, none), none.size());
}

BOOST_AUTO_TEST_CASE(all_non_positive_weight_has_no_winner)
{
    const uint256 seed = uint256S("bb");
    std::vector<StakeCandidate> candidates{
        {uint256S("01"), 0},
        {uint256S("02"), -5}, // must never be selected or crash weight arithmetic
    };
    BOOST_CHECK_EQUAL(SelectStakeValidator(seed, candidates), candidates.size());
}

BOOST_AUTO_TEST_CASE(single_positive_candidate_always_wins)
{
    std::vector<StakeCandidate> candidates{{uint256S("01"), 42}};
    for (uint32_t attempt = 0; attempt < 20; ++attempt) {
        const uint256 seed = DeriveFallbackSeed(uint256S("cc"), attempt);
        BOOST_CHECK_EQUAL(SelectStakeValidator(seed, candidates), 0u);
    }
}

// The header documents that the winner must not depend on the order
// `candidates` is passed in — every node has to reach the same answer even
// if it assembled its candidate list differently. This is the property that
// makes the whole scheme independently verifiable rather than something one
// party could bias by controlling iteration order.
BOOST_AUTO_TEST_CASE(result_independent_of_input_order)
{
    const std::vector<StakeCandidate> original{
        {uint256S("10"), 100},
        {uint256S("05"), 250},
        {uint256S("20"), 40},
        {uint256S("01"), 900},
    };

    for (uint32_t attempt = 0; attempt < 30; ++attempt) {
        const uint256 seed = DeriveFallbackSeed(uint256S("dd"), attempt);

        std::vector<StakeCandidate> shuffled = original;
        // Deterministic "shuffle" that still varies per attempt, no <random> needed.
        std::rotate(shuffled.begin(), shuffled.begin() + (attempt % shuffled.size()), shuffled.end());

        const uint256& winnerIdOriginal = original[SelectStakeValidator(seed, original)].id;
        const uint256& winnerIdShuffled = shuffled[SelectStakeValidator(seed, shuffled)].id;
        BOOST_CHECK_EQUAL(winnerIdOriginal.ToString(), winnerIdShuffled.ToString());
    }
}

BOOST_AUTO_TEST_CASE(zero_weight_candidate_never_wins_among_positive_peers)
{
    std::vector<StakeCandidate> candidates{
        {uint256S("01"), 0},
        {uint256S("02"), 1000},
    };
    for (uint32_t attempt = 0; attempt < 50; ++attempt) {
        const uint256 seed = DeriveFallbackSeed(uint256S("ee"), attempt);
        BOOST_CHECK_EQUAL(SelectStakeValidator(seed, candidates), 1u);
    }
}

// Loose statistical sanity check, not an exact-probability assertion (that
// would make the test flaky): a candidate with ~9x the weight of its only
// rival should win noticeably more than half the draws, but the rival
// should still win sometimes. Bounds are generous on purpose.
BOOST_AUTO_TEST_CASE(selection_is_weight_sensitive)
{
    std::vector<StakeCandidate> candidates{
        {uint256S("01"), 900}, // heavy
        {uint256S("02"), 100}, // light
    };
    int heavyWins = 0;
    const int trials = 500;
    for (uint32_t attempt = 0; attempt < static_cast<uint32_t>(trials); ++attempt) {
        const uint256 seed = DeriveFallbackSeed(uint256S("ff"), attempt);
        if (SelectStakeValidator(seed, candidates) == 0u) ++heavyWins;
    }
    BOOST_CHECK(heavyWins > trials * 6 / 10);  // heavy candidate clearly favoured
    BOOST_CHECK(heavyWins < trials);           // but light candidate isn't starved entirely
}

BOOST_AUTO_TEST_CASE(fallback_seed_is_deterministic_and_varies_by_attempt)
{
    const uint256 base = uint256S("1234");
    BOOST_CHECK_EQUAL(DeriveFallbackSeed(base, 1).ToString(), DeriveFallbackSeed(base, 1).ToString());

    std::map<std::string, bool> seen;
    for (uint32_t attempt = 0; attempt < 100; ++attempt) {
        const std::string s = DeriveFallbackSeed(base, attempt).ToString();
        BOOST_CHECK(seen.find(s) == seen.end()); // no collisions across 100 attempts
        seen[s] = true;
    }
}

BOOST_AUTO_TEST_SUITE_END()
