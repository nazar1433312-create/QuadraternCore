// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/algo_hash.h>

#include <boost/test/unit_test.hpp>

using namespace qtrn;

namespace {

CBlockHeader MakeHeader(PowAlgo algo, uint32_t nonce = 0)
{
    CBlockHeader header;
    header.nVersion = SetPowAlgo(0x20000000, algo);
    header.hashPrevBlock = uint256S("prevhash");
    header.hashMerkleRoot = uint256S("merkleroot");
    header.nTime = 1798348800;
    header.nBits = 0x1f00ffff;
    header.nNonce = nonce;
    return header;
}

} // namespace

BOOST_AUTO_TEST_SUITE(qtrn_algo_hash_tests)

BOOST_AUTO_TEST_CASE(sha256d_matches_the_headers_own_get_hash)
{
    const CBlockHeader header = MakeHeader(PowAlgo::SHA256D);
    BOOST_CHECK_EQUAL(ComputeAlgoHash(header).ToString(), header.GetHash().ToString());
}

BOOST_AUTO_TEST_CASE(is_deterministic_for_every_algo)
{
    for (PowAlgo algo : {PowAlgo::SHA256D, PowAlgo::PROGPOW, PowAlgo::RANDOMX}) {
        const CBlockHeader header = MakeHeader(algo);
        BOOST_CHECK_EQUAL(ComputeAlgoHash(header).ToString(), ComputeAlgoHash(header).ToString());
    }
}

BOOST_AUTO_TEST_CASE(different_algos_never_collide_on_identical_header_content)
{
    // Same header fields, only the algo tag in nVersion differs.
    const uint256 hashSha = ComputeAlgoHash(MakeHeader(PowAlgo::SHA256D));
    const uint256 hashProgpow = ComputeAlgoHash(MakeHeader(PowAlgo::PROGPOW));
    const uint256 hashRandomx = ComputeAlgoHash(MakeHeader(PowAlgo::RANDOMX));

    BOOST_CHECK(hashSha != hashProgpow);
    BOOST_CHECK(hashSha != hashRandomx);
    BOOST_CHECK(hashProgpow != hashRandomx);
}

BOOST_AUTO_TEST_CASE(reflects_real_content_changes)
{
    const uint256 a = ComputeAlgoHash(MakeHeader(PowAlgo::PROGPOW, /*nonce=*/1));
    const uint256 b = ComputeAlgoHash(MakeHeader(PowAlgo::PROGPOW, /*nonce=*/2));
    BOOST_CHECK(a != b); // not an accidental constant
}

BOOST_AUTO_TEST_CASE(unassigned_algo_id_yields_null_hash)
{
    CBlockHeader header = MakeHeader(PowAlgo::SHA256D);
    header.nVersion |= VERSION_ALGO_MASK; // both algo bits set = unassigned value 3
    BOOST_CHECK(ComputeAlgoHash(header).IsNull());
}

BOOST_AUTO_TEST_CASE(only_sha256d_is_marked_real)
{
    BOOST_CHECK(IsAlgoHashReal(PowAlgo::SHA256D));
    BOOST_CHECK(!IsAlgoHashReal(PowAlgo::PROGPOW));
    BOOST_CHECK(!IsAlgoHashReal(PowAlgo::RANDOMX));
}

BOOST_AUTO_TEST_SUITE_END()
