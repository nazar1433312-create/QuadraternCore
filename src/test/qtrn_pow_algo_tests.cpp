// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/pow_algo.h>

#include <boost/test/unit_test.hpp>

using namespace qtrn;

BOOST_AUTO_TEST_SUITE(qtrn_pow_algo_tests)

BOOST_AUTO_TEST_CASE(round_trips_each_algo)
{
    for (PowAlgo algo : {PowAlgo::SHA256D, PowAlgo::PROGPOW, PowAlgo::RANDOMX}) {
        const int32_t encoded = SetPowAlgo(0, algo);
        BOOST_CHECK(GetPowAlgo(encoded) == algo);
    }
}

BOOST_AUTO_TEST_CASE(preserves_bip9_top_bits_and_unrelated_deployment_bits)
{
    // 0x20000000 = BIP9 top-bits marker; bit 2 and bit 28 = real (if dormant)
    // deployment signal bits in this tree (DEPLOYMENT_TAPROOT, DEPLOYMENT_TESTDUMMY).
    const int32_t baseVersion = 0x20000000 | (1 << 2) | (1 << 28);
    const int32_t encoded = SetPowAlgo(baseVersion, PowAlgo::RANDOMX);

    BOOST_CHECK(GetPowAlgo(encoded) == PowAlgo::RANDOMX);
    BOOST_CHECK_EQUAL(encoded & 0x20000000, 0x20000000); // top-bits marker untouched
    BOOST_CHECK_EQUAL(encoded & (1 << 2), (1 << 2));      // unrelated deployment bit untouched
    BOOST_CHECK_EQUAL(encoded & (1 << 28), (1 << 28));    // unrelated deployment bit untouched
}

BOOST_AUTO_TEST_CASE(setting_algo_overwrites_any_previous_algo_bits)
{
    int32_t v = SetPowAlgo(0, PowAlgo::PROGPOW);
    v = SetPowAlgo(v, PowAlgo::SHA256D);
    BOOST_CHECK(GetPowAlgo(v) == PowAlgo::SHA256D);
}

BOOST_AUTO_TEST_CASE(the_unassigned_fourth_value_is_invalid)
{
    BOOST_CHECK(IsValidPowAlgo(PowAlgo::SHA256D));
    BOOST_CHECK(IsValidPowAlgo(PowAlgo::PROGPOW));
    BOOST_CHECK(IsValidPowAlgo(PowAlgo::RANDOMX));

    const int32_t allAlgoBitsSet = VERSION_ALGO_MASK; // decodes to the unassigned value 3
    BOOST_CHECK(!IsValidPowAlgo(GetPowAlgo(allAlgoBitsSet)));
}

BOOST_AUTO_TEST_CASE(names_are_short_and_non_empty)
{
    BOOST_CHECK_EQUAL(PowAlgoName(PowAlgo::SHA256D), "sha256d");
    BOOST_CHECK_EQUAL(PowAlgoName(PowAlgo::PROGPOW), "progpow");
    BOOST_CHECK_EQUAL(PowAlgoName(PowAlgo::RANDOMX), "randomx");
    BOOST_CHECK_EQUAL(PowAlgoName(GetPowAlgo(VERSION_ALGO_MASK)), "unknown");
}

BOOST_AUTO_TEST_SUITE_END()
