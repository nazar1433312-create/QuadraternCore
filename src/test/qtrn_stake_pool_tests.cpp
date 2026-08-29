// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/stake_pool.h>

#include <hash.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <set>

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

BOOST_AUTO_TEST_CASE(non_positive_total_is_dropped)
{
    StakeCandidateBalance b;
    b.ownerId = uint256S("03");
    b.amount = 0;
    BOOST_CHECK(AssembleStakeCandidates({b}).empty());
}

BOOST_AUTO_TEST_CASE(sub_threshold_balance_folds_into_its_deterministic_pool)
{
    StakeCandidateBalance b;
    b.ownerId = uint256S("04");
    b.amount = BELOW_THRESHOLD;

    const auto result = AssembleStakeCandidates({b});
    BOOST_REQUIRE_EQUAL(result.size(), 1u);
    BOOST_CHECK(result[0].id == DeterministicPoolId(b.ownerId));
    BOOST_CHECK_EQUAL(result[0].weight, BELOW_THRESHOLD);
}

BOOST_AUTO_TEST_CASE(aggregated_total_can_cross_the_solo_threshold)
{
    // Neither individual entry clears the threshold, but their sum does —
    // the decision must be made on the summed total, not per-entry.
    const uint256 owner = uint256S("05");
    StakeCandidateBalance a, b;
    a.ownerId = b.ownerId = owner;
    a.amount = 60 * COIN;
    b.amount = 60 * COIN; // 120 total, crosses SOLO_STAKE_MINIMUM (100)

    const auto result = AssembleStakeCandidates({a, b});
    BOOST_REQUIRE_EQUAL(result.size(), 1u);
    BOOST_CHECK(result[0].id == owner);
    BOOST_CHECK_EQUAL(result[0].weight, 120 * COIN);
}

BOOST_AUTO_TEST_CASE(different_owners_in_the_same_pool_are_summed_together)
{
    // Two owners that happen to hash into the same pool must combine their
    // weight there — same mechanics as two owners sharing a declared pool
    // would, just driven by the hash instead of a choice. Find a second
    // owner landing in the first owner's pool by re-hashing until it does.
    const uint256 ownerA = uint256S("06");
    const uint256 poolOfA = DeterministicPoolId(ownerA);
    uint256 ownerB = uint256S("07");
    for (uint32_t i = 0; DeterministicPoolId(ownerB) != poolOfA && i < 10000; ++i) {
        ownerB = Hash(ownerB);
    }
    BOOST_REQUIRE(DeterministicPoolId(ownerB) == poolOfA); // sanity: search converged

    StakeCandidateBalance a, b;
    a.ownerId = ownerA;
    a.amount = 5 * COIN;
    b.ownerId = ownerB;
    b.amount = 7 * COIN;

    const auto result = AssembleStakeCandidates({a, b});
    BOOST_REQUIRE_EQUAL(result.size(), 1u);
    BOOST_CHECK(result[0].id == poolOfA);
    BOOST_CHECK_EQUAL(result[0].weight, 12 * COIN);
}

BOOST_AUTO_TEST_CASE(deterministic_pool_id_is_stable_and_in_range)
{
    const uint256 owner = uint256S("08");
    const uint256 first = DeterministicPoolId(owner);
    const uint256 second = DeterministicPoolId(owner);
    BOOST_CHECK(first == second); // same input -> same output, every time

    // Every possible pool id must correspond to one of NUM_VIRTUAL_POOLS
    // small integer indices — spot-check by regenerating from many owners
    // and confirming the *set* of distinct ids seen never exceeds N.
    std::set<uint256> seen;
    for (int i = 0; i < 5000; ++i) {
        seen.insert(DeterministicPoolId(uint256S(std::to_string(i))));
    }
    BOOST_CHECK(seen.size() <= NUM_VIRTUAL_POOLS);
}

BOOST_AUTO_TEST_CASE(solo_eligible_balance_never_gets_pooled)
{
    // A solo-eligible balance is its own candidate under its own ownerId —
    // DeterministicPoolId is never consulted for it at all.
    StakeCandidateBalance b;
    b.ownerId = uint256S("09");
    b.amount = ABOVE_THRESHOLD;

    const auto result = AssembleStakeCandidates({b});
    BOOST_REQUIRE_EQUAL(result.size(), 1u);
    BOOST_CHECK(result[0].id == b.ownerId);
}

BOOST_AUTO_TEST_CASE(mixed_realistic_scenario)
{
    std::vector<StakeCandidateBalance> balances;

    StakeCandidateBalance whale;
    whale.ownerId = uint256S("10");
    whale.amount = 5000 * COIN;
    balances.push_back(whale);

    StakeCandidateBalance small1;
    small1.ownerId = uint256S("11");
    small1.amount = 3 * COIN;
    balances.push_back(small1);

    StakeCandidateBalance small2;
    small2.ownerId = uint256S("12");
    small2.amount = 7 * COIN;
    balances.push_back(small2);

    const auto result = AssembleStakeCandidates(balances);
    const StakeCandidate* whaleCand = FindCandidate(result, whale.ownerId);
    BOOST_REQUIRE(whaleCand);
    BOOST_CHECK_EQUAL(whaleCand->weight, 5000 * COIN);

    // The two small balances land in one or two pools depending on hash
    // luck, but their combined weight must be conserved either way.
    CAmount pooledTotal = 0;
    for (const auto& c : result) {
        if (c.id != whale.ownerId) pooledTotal += c.weight;
    }
    BOOST_CHECK_EQUAL(pooledTotal, 10 * COIN);
}

BOOST_AUTO_TEST_SUITE_END()
