// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/local_stake_signer.h>

#include <qtrn/stake_commitment.h>

#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

using namespace qtrn;

// CKey::MakeNewKey/Sign and CPubKey::Verify need the secp256k1 signing
// context, which BasicTestingSetup's constructor initializes via ECC_Start()
// — without it this crashes (seen as SIGABRT or a segfault depending on
// exactly which secp256k1 call hits the uninitialized context first).
BOOST_FIXTURE_TEST_SUITE(qtrn_local_stake_signer_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(empty_signer_has_no_keys)
{
    LocalStakeSigner signer;
    BOOST_CHECK(!signer.HasKeyFor(uint256S("01")));
}

BOOST_AUTO_TEST_CASE(added_key_is_found_by_its_own_id)
{
    CKey key;
    key.MakeNewKey(true);
    LocalStakeSigner signer;
    signer.AddKey(key);

    const uint256 id = StakeValidatorIdFromPubKey(key.GetPubKey());
    BOOST_CHECK(signer.HasKeyFor(id));
    BOOST_CHECK(!signer.HasKeyFor(uint256S("deadbeef"))); // unrelated id
}

BOOST_AUTO_TEST_CASE(sign_produces_a_verifiable_signature)
{
    CKey key;
    key.MakeNewKey(true);
    LocalStakeSigner signer;
    signer.AddKey(key);

    const uint256 id = StakeValidatorIdFromPubKey(key.GetPubKey());
    const uint256 signingHash = uint256S("aa11");

    std::vector<unsigned char> sig;
    CPubKey pubKey;
    BOOST_REQUIRE(signer.Sign(id, signingHash, sig, pubKey));
    BOOST_CHECK(pubKey == key.GetPubKey());
    BOOST_CHECK(pubKey.Verify(signingHash, sig));
}

BOOST_AUTO_TEST_CASE(sign_fails_for_unregistered_id)
{
    LocalStakeSigner signer;
    std::vector<unsigned char> sig;
    CPubKey pubKey;
    BOOST_CHECK(!signer.Sign(uint256S("01"), uint256S("aa11"), sig, pubKey));
}

BOOST_AUTO_TEST_CASE(find_signable_attempt_picks_the_first_locally_held_winner)
{
    CKey ourKey;
    ourKey.MakeNewKey(true);
    LocalStakeSigner signer;
    signer.AddKey(ourKey);
    const uint256 ourId = StakeValidatorIdFromPubKey(ourKey.GetPubKey());

    // A candidate set where our key holds a modest but non-trivial share, so
    // it wins often enough to find within a handful of attempts, but not
    // certainly on attempt 0 — exercises the attempt-search loop for real.
    const std::vector<StakeCandidate> candidates{
        {ourId, 30 * COIN},
        {uint256S("other"), 70 * COIN},
    };

    uint32_t attemptOut = 0;
    uint256 winnerIdOut;
    bool found = signer.TryFindSignableAttempt(uint256S("seed"), candidates, 200, attemptOut, winnerIdOut);
    BOOST_REQUIRE(found); // ~30% odds per attempt, 200 tries is overwhelming
    BOOST_CHECK(winnerIdOut == ourId);

    // The attempt found must actually make ourId the winner for that seed.
    const uint256 seed = (attemptOut == 0) ? uint256S("seed") : DeriveFallbackSeed(uint256S("seed"), attemptOut);
    const size_t idx = SelectStakeValidator(seed, candidates);
    BOOST_REQUIRE(idx < candidates.size());
    BOOST_CHECK(candidates[idx].id == ourId);
}

BOOST_AUTO_TEST_CASE(find_signable_attempt_fails_when_we_hold_no_relevant_key)
{
    LocalStakeSigner signer; // holds nothing
    const std::vector<StakeCandidate> candidates{
        {uint256S("someone-else"), 100 * COIN},
    };
    uint32_t attemptOut = 0;
    uint256 winnerIdOut;
    BOOST_CHECK(!signer.TryFindSignableAttempt(uint256S("seed"), candidates, 20, attemptOut, winnerIdOut));
}

BOOST_AUTO_TEST_SUITE_END()
