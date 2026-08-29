// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/staked_block.h>

#include <qtrn/stake_commitment.h>

#include <consensus/merkle.h>
#include <key.h>
#include <script/script.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

using namespace qtrn;

BOOST_FIXTURE_TEST_SUITE(qtrn_staked_block_tests, BasicTestingSetup)

namespace {

CBlock MakeTemplateBlock(const std::string& tag)
{
    CMutableTransaction coinbase;
    coinbase.nVersion = 1;
    coinbase.vin.resize(1);
    coinbase.vin[0].scriptSig = CScript() << std::vector<unsigned char>(tag.begin(), tag.end());
    coinbase.vout.resize(1);
    coinbase.vout[0].nValue = 100 * COIN;
    coinbase.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CBlock block;
    block.nVersion = 1;
    block.nTime = 1798348800;
    block.nBits = 0x1f00ffff;
    block.nNonce = 0;
    block.hashPrevBlock = uint256S("prevhash");
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));
    block.hashMerkleRoot = BlockMerkleRoot(block);
    return block;
}

} // namespace

BOOST_AUTO_TEST_CASE(builds_a_valid_commitment_when_signer_holds_the_winner)
{
    CKey key;
    key.MakeNewKey(true);
    const uint256 id = StakeValidatorIdFromPubKey(key.GetPubKey());

    LocalStakeSigner signer;
    signer.AddKey(key);

    const std::vector<StakeCandidate> candidates{{id, 500 * COIN}};
    const uint256 primarySeed = uint256S("prevhash-derived-seed");
    const CBlock templateBlock = MakeTemplateBlock("staked-block-test");

    CBlock outBlock;
    BOOST_REQUIRE(TryBuildStakedBlock(templateBlock, primarySeed, candidates, signer, 20, outBlock));

    // The result must carry a commitment that verifies against the
    // *template's* signing hash (nonce-independent, and the commitment
    // wasn't there yet when it was computed) using the same expected id
    // SelectStakeValidator would hand back for attempt 0 here (the sole
    // candidate always wins).
    StakeCommitment parsed;
    BOOST_REQUIRE(FindStakeCommitment(outBlock.vtx[0]->vout, parsed));
    BOOST_CHECK_EQUAL(parsed.attempt, 0u);
    BOOST_CHECK(parsed.validatorPubKey == key.GetPubKey());

    const uint256 signingHash = ComputeStakeSigningHash(templateBlock);
    const uint256 recomputed = ComputeStakeSigningHash(outBlock);
    BOOST_CHECK_EQUAL(signingHash.ToString(), recomputed.ToString());
    BOOST_CHECK(VerifyStakeCommitment(parsed, recomputed, id));
}

BOOST_AUTO_TEST_CASE(fails_when_signer_holds_no_usable_key)
{
    LocalStakeSigner emptySigner;
    const std::vector<StakeCandidate> candidates{{uint256S("someone-else"), 500 * COIN}};
    const CBlock templateBlock = MakeTemplateBlock("no-key-test");

    CBlock outBlock;
    BOOST_CHECK(!TryBuildStakedBlock(templateBlock, uint256S("seed"), candidates, emptySigner, 20, outBlock));
}

BOOST_AUTO_TEST_CASE(result_still_verifies_after_pow_mining_changes_nonce)
{
    // Regression coverage for the exact bug the consensus integration test
    // caught: signing must survive nNonce changing after the commitment is
    // attached (PoW mining happens after this function runs).
    CKey key;
    key.MakeNewKey(true);
    const uint256 id = StakeValidatorIdFromPubKey(key.GetPubKey());
    LocalStakeSigner signer;
    signer.AddKey(key);
    const std::vector<StakeCandidate> candidates{{id, 500 * COIN}};
    const CBlock templateBlock = MakeTemplateBlock("nonce-survives-test");

    CBlock outBlock;
    BOOST_REQUIRE(TryBuildStakedBlock(templateBlock, uint256S("seed"), candidates, signer, 20, outBlock));

    outBlock.nNonce = 999999; // simulate PoW mining having run

    StakeCommitment parsed;
    BOOST_REQUIRE(FindStakeCommitment(outBlock.vtx[0]->vout, parsed));
    const uint256 recomputed = ComputeStakeSigningHash(outBlock);
    BOOST_CHECK(VerifyStakeCommitment(parsed, recomputed, id));
}

BOOST_AUTO_TEST_SUITE_END()
