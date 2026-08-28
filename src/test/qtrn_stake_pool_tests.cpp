// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/stake_pool.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>

using namespace qtrn;

namespace {

const CAmount ABOVE_THRESHOLD = SOLO_STAKE_MINIMUM + COIN;
const CAmount BELOW_THRESHOLD = SOLO_STAKE_MINIMUM - COIN;

const StakeCandidate* FindCandidate(const std::vector<StakeCandidate>& candidates, const uint256& id)
{
    auto it = std::find_if(candidates.begin(), candidates.end(),
                            [&](const StakeCandidate& c) { return c.id == id; });
    return it == candidates.end() ? nullptr : &*it;
}

} // namespace

BOOST_AUTO_TEST_SUITE(qtrn_stake_pool_tests)

BOOST_AUTO_TEST_CASE(empty_input_yields_empty_output)
{
    BOOST_CHECK(AssembleStakeCandidates({}).empty());
}

BOOST_AUTO_TEST_CASE(solo_eligible_balance_becomes_its_own_candidate)
{
    StakeCandidateBalance b;
    b.ownerId = uint256S("01");
    b.amount = ABOVE_THRESHOLD;

    const auto result = AssembleStakeCandidates({b});
    BOOST_REQUIRE_EQUAL(result.size(), 1u);
    BOOST_CHECK(result[0].id == b.ownerId);
    BOOST_CHECK_EQUAL(result[0].weight, ABOVE_THRESHOLD);
}

BOOST_AUTO_TEST_CASE(multiple_utxos_for_the_same_owner_are_summed)
{
    const uint256 owner = uint256S("02");
    StakeCandidateBalance a, b, c;
    a.ownerId = b.ownerId = c.ownerId = owner;
    a.amount = 40 * COIN;
    b.amount = 40 * COIN;
    c.amount = 30 * COIN; // 110 total, clears the 100 QTRN solo threshold

    const auto result = AssembleStakeCandidates({a, b, c});
    BOOST_REQUIRE_EQUAL(result.size(), 1u);
    BOOST_CHECK(result[0].id == owner);
    BOOST_CHECK_EQUAL(result[0].weight, 110 * COIN);
}

BOOST_AUTO_TEST_CASE(sub_threshold_balance_with_no_pool_is_dropped)
{
    StakeCandidateBalance b;
    b.ownerId = uint256S("03");
    b.amount = BELOW_THRESHOLD;
    b.hasPool = false;

    BOOST_CHECK(AssembleStakeCandidates({b}).empty());
}

BOOST_AUTO_TEST_CASE(sub_threshold_balances_fold_into_their_declared_pool)
{
    const uint256 pool = uint256S("aa");
    StakeCandidateBalance a, b;
    a.ownerId = uint256S("04");
    a.amount = 10 * COIN;
    a.hasPool = true;
    a.poolId = pool;
    b.ownerId = uint256S("05");
    b.amount = 15 * COIN;
    b.hasPool = true;
    b.poolId = pool;

    const auto result = AssembleStakeCandidates({a, b});
    BOOST_REQUIRE_EQUAL(result.size(), 1u);
    BOOST_CHECK(result[0].id == pool);
    BOOST_CHECK_EQUAL(result[0].weight, 25 * COIN);
}

BOOST_AUTO_TEST_CASE(different_pools_stay_separate)
{
    const uint256 poolA = uint256S("aa");
    const uint256 poolB = uint256S("bb");
    StakeCandidateBalance a, b;
    a.ownerId = uint256S("06");
    a.amount = 10 * COIN;
    a.hasPool = true;
    a.poolId = poolA;
    b.ownerId = uint256S("07");
    b.amount = 20 * COIN;
    b.hasPool = true;
    b.poolId = poolB;

    const auto result = AssembleStakeCandidates({a, b});
    BOOST_REQUIRE_EQUAL(result.size(), 2u);
    const StakeCandidate* candA = FindCandidate(result, poolA);
    const StakeCandidate* candB = FindCandidate(result, poolB);
    BOOST_REQUIRE(candA && candB);
    BOOST_CHECK_EQUAL(candA->weight, 10 * COIN);
    BOOST_CHECK_EQUAL(candB->weight, 20 * COIN);
}

BOOST_AUTO_TEST_CASE(solo_eligibility_overrides_a_declared_pool)
{
    // Large balances stay individually identifiable even if the owner also
    // declared a pool — see the header's rationale (pooling exists to bound
    // computation for small holders, not to hide large ones).
    const uint256 pool = uint256S("aa");
    StakeCandidateBalance b;
    b.ownerId = uint256S("08");
    b.amount = ABOVE_THRESHOLD;
    b.hasPool = true;
    b.poolId = pool;

    const auto result = AssembleStakeCandidates({b});
    BOOST_REQUIRE_EQUAL(result.size(), 1u);
    BOOST_CHECK(result[0].id == b.ownerId); // solo candidate, not folded into `pool`
    BOOST_CHECK(FindCandidate(result, pool) == nullptr);
}

BOOST_AUTO_TEST_CASE(aggregated_total_can_cross_the_solo_threshold)
{
    // Neither individual entry clears the threshold, but their sum does —
    // the decision must be made on the summed total, not per-entry.
    const uint256 owner = uint256S("09");
    const uint256 pool = uint256S("aa");
    StakeCandidateBalance a, b;
    a.ownerId = b.ownerId = owner;
    a.amount = 60 * COIN;
    a.hasPool = true;
    a.poolId = pool;
    b.amount = 60 * COIN; // 120 total, crosses SOLO_STAKE_MINIMUM (100)

    const auto result = AssembleStakeCandidates({a, b});
    BOOST_REQUIRE_EQUAL(result.size(), 1u);
    BOOST_CHECK(result[0].id == owner);
    BOOST_CHECK_EQUAL(result[0].weight, 120 * COIN);
}

BOOST_AUTO_TEST_CASE(first_entrys_pool_choice_wins_for_a_given_owner)
{
    const uint256 owner = uint256S("0a");
    const uint256 firstPool = uint256S("aa");
    const uint256 secondPool = uint256S("bb");
    StakeCandidateBalance a, b;
    a.ownerId = b.ownerId = owner;
    a.amount = 5 * COIN;
    a.hasPool = true;
    a.poolId = firstPool;
    b.amount = 5 * COIN;
    b.hasPool = true;
    b.poolId = secondPool; // ignored — `a` was first for this owner

    const auto result = AssembleStakeCandidates({a, b});
    BOOST_REQUIRE_EQUAL(result.size(), 1u);
    BOOST_CHECK(result[0].id == firstPool);
    BOOST_CHECK_EQUAL(result[0].weight, 10 * COIN);
}

BOOST_AUTO_TEST_CASE(mixed_realistic_scenario)
{
    const uint256 poolX = uint256S("aa");
    std::vector<StakeCandidateBalance> balances;

    StakeCandidateBalance whale;
    whale.ownerId = uint256S("10");
    whale.amount = 5000 * COIN;
    balances.push_back(whale);

    StakeCandidateBalance small1;
    small1.ownerId = uint256S("11");
    small1.amount = 3 * COIN;
    small1.hasPool = true;
    small1.poolId = poolX;
    balances.push_back(small1);

    StakeCandidateBalance small2;
    small2.ownerId = uint256S("12");
    small2.amount = 7 * COIN;
    small2.hasPool = true;
    small2.poolId = poolX;
    balances.push_back(small2);

    StakeCandidateBalance dust;
    dust.ownerId = uint256S("13");
    dust.amount = 1 * COIN;
    dust.hasPool = false; // never opted into a pool
    balances.push_back(dust);

    const auto result = AssembleStakeCandidates(balances);
    BOOST_REQUIRE_EQUAL(result.size(), 2u); // whale + pool, dust dropped
    const StakeCandidate* whaleCand = FindCandidate(result, whale.ownerId);
    const StakeCandidate* poolCand = FindCandidate(result, poolX);
    BOOST_REQUIRE(whaleCand && poolCand);
    BOOST_CHECK_EQUAL(whaleCand->weight, 5000 * COIN);
    BOOST_CHECK_EQUAL(poolCand->weight, 10 * COIN);
    BOOST_CHECK(FindCandidate(result, dust.ownerId) == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
