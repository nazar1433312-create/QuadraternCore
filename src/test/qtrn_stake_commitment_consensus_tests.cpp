// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Exercises the actual consensus rule wired into validation.cpp's
// ContextualCheckBlock (CheckStakeCommitment) through the real block
// acceptance path (ProcessNewBlock) — not just the qtrn:: building blocks in
// isolation. Uses CMainParams (TestingSetup's default network) rather than
// regtest, because regtest's shared TestChain100Setup fixture mines blocks
// with no stake commitment and would break under this rule (its
// consensus.genesisStakers is deliberately left empty — see chainparams.cpp).

#include <qtrn/genesis_stakers.h>
#include <qtrn/stake_commitment.h>
#include <qtrn/stake_pool.h>
#include <qtrn/stake_selection.h>

#include <chainparams.h>
#include <consensus/merkle.h>
#include <key.h>
#include <miner.h>
#include <pow.h>
#include <script/script.h>
#include <test/util/setup_common.h>
#include <txmempool.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

using namespace qtrn;

BOOST_FIXTURE_TEST_SUITE(qtrn_stake_commitment_consensus_tests, TestingSetup)

namespace {

CKey KeyFromHex(const std::string& hex)
{
    const std::vector<unsigned char> raw = ParseHex(hex);
    CKey key;
    key.Set(raw.begin(), raw.end(), /*fCompressedIn=*/true);
    return key;
}

// Mines a block template on the current tip via the real BlockAssembler,
// optionally appends a stake commitment, mines real PoW for it, and submits
// it through the real ProcessNewBlock path. Returns whether the tip advanced
// to this block — the actual thing CheckStakeCommitment is supposed to gate.
bool BuildAndSubmit(const StakeCommitment* commitment)
{
    const CChainParams& chainparams = Params();
    CTxMemPool emptyPool;
    CBlock block = BlockAssembler(emptyPool, chainparams).CreateNewBlock(CScript() << OP_TRUE)->block;
    RegenerateCommitments(block);

    if (commitment) {
        CMutableTransaction coinbase(*block.vtx[0]);
        coinbase.vout.emplace_back(0, BuildStakeCommitmentScript(*commitment));
        BOOST_REQUIRE(!coinbase.vout.back().scriptPubKey.empty());
        block.vtx[0] = MakeTransactionRef(std::move(coinbase));
        block.hashMerkleRoot = BlockMerkleRoot(block);
    }

    while (!CheckProofOfWork(block.GetPoWHash(), block.nBits, chainparams.GetConsensus())) {
        ++block.nNonce;
    }

    const uint256 tipBefore = ::ChainActive().Tip()->GetBlockHash();
    auto shared_pblock = std::make_shared<const CBlock>(block);
    Assert(m_node.chainman)->ProcessNewBlock(chainparams, shared_pblock, /*fForceProcessing=*/true, nullptr);
    const uint256 tipAfter = ::ChainActive().Tip()->GetBlockHash();

    return tipAfter == block.GetHash() && tipAfter != tipBefore;
}

} // namespace

BOOST_AUTO_TEST_CASE(block_with_no_stake_commitment_is_rejected)
{
    BOOST_CHECK(!BuildAndSubmit(nullptr));
}

BOOST_AUTO_TEST_CASE(block_with_wrong_signature_is_rejected)
{
    const auto& stakers = Params().GetConsensus().genesisStakers;
    BOOST_REQUIRE(!stakers.empty());

    // A validly-formed commitment, but signed by a key that isn't the
    // selected validator for this seed/attempt at all.
    CKey wrongKey;
    wrongKey.MakeNewKey(true);

    StakeCommitment bogus;
    bogus.attempt = 0;
    bogus.validatorPubKey = wrongKey.GetPubKey();
    BOOST_REQUIRE(wrongKey.Sign(uint256S("deadbeef"), bogus.signature)); // signs the wrong hash on top of being the wrong key

    BOOST_CHECK(!BuildAndSubmit(&bogus));
}

BOOST_AUTO_TEST_CASE(block_with_valid_commitment_is_accepted)
{
    const auto& stakers = Params().GetConsensus().genesisStakers;
    BOOST_REQUIRE_EQUAL(stakers.size(), 3u);
    const std::vector<CKey> keys = {
        KeyFromHex("907fd30b8d3184871321e3d66e67eebca91f692a7c833d07c6b841ff14d6c710"),
        KeyFromHex("03cd69d294ff4dfb956a20cc2d4e91c7fcab5ea76c21c9724803ebb580bd5880"),
        KeyFromHex("835c829227b957748d8ec896cf03eaccfb185528fd0b8f45fdcba46234b0639b"),
    };
    for (size_t i = 0; i < stakers.size(); ++i) {
        BOOST_REQUIRE(keys[i].GetPubKey() == stakers[i].first);
    }

    const auto candidates = AssembleStakeCandidates(GenesisStakersToBalances(stakers));
    const uint256 soloId = StakeValidatorIdFromPubKey(stakers[0].first);
    const uint256 primarySeed = ::ChainActive().Tip()->GetBlockHash();

    // Find the smallest attempt (within the consensus rule's accepted range)
    // for which the solo staker — the only candidate we hold a real private
    // key for — is the selected validator.
    uint32_t winningAttempt = 0;
    bool found = false;
    for (uint32_t attempt = 0; attempt <= 20; ++attempt) {
        const uint256 seed = (attempt == 0) ? primarySeed : DeriveFallbackSeed(primarySeed, attempt);
        const size_t idx = SelectStakeValidator(seed, candidates);
        if (idx < candidates.size() && candidates[idx].id == soloId) {
            winningAttempt = attempt;
            found = true;
            break;
        }
    }
    BOOST_REQUIRE(found); // ~88% odds per attempt; 20 tries is overwhelmingly enough

    // Build the exact same block template CheckStakeCommitment will see, to
    // compute the same signing hash it will recompute during validation.
    const CChainParams& chainparams = Params();
    CTxMemPool emptyPool;
    CBlock block = BlockAssembler(emptyPool, chainparams).CreateNewBlock(CScript() << OP_TRUE)->block;
    RegenerateCommitments(block);
    const uint256 signingHash = ComputeStakeSigningHash(block);

    StakeCommitment commitment;
    commitment.attempt = winningAttempt;
    commitment.validatorPubKey = keys[0].GetPubKey();
    BOOST_REQUIRE(keys[0].Sign(signingHash, commitment.signature));

    BOOST_CHECK(BuildAndSubmit(&commitment));
}

BOOST_AUTO_TEST_SUITE_END()
