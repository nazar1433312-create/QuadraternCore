// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// End-to-end test tying every qtrn module together against the real
// testnet-1 chainparams genesis stakers: assemble candidates -> select a
// validator -> sign with that validator's real private key -> commit into a
// block -> recompute the signing hash -> verify. If any module's contract
// drifted from another's expectations, this is where it would show up.

#include <qtrn/genesis_stakers.h>
#include <qtrn/stake_commitment.h>
#include <qtrn/stake_pool.h>
#include <qtrn/stake_selection.h>

#include <chainparams.h>
#include <consensus/merkle.h>
#include <key.h>
#include <primitives/block.h>
#include <script/script.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

using namespace qtrn;

BOOST_FIXTURE_TEST_SUITE(qtrn_genesis_stakers_tests, BasicTestingSetup)

namespace {

// The private keys corresponding to the compressed pubkeys hardcoded in
// CMainParams::genesisStakers (chainparams.cpp) — see
// contrib/testnet1-genesis-stakers.md. Throwaway testnet-1 test keys only.
CKey KeyFromHex(const std::string& hex)
{
    const std::vector<unsigned char> raw = ParseHex(hex);
    CKey key;
    key.Set(raw.begin(), raw.end(), /*fCompressedIn=*/true);
    return key;
}

std::vector<CKey> GenesisStakerKeys()
{
    return {
        KeyFromHex("907fd30b8d3184871321e3d66e67eebca91f692a7c833d07c6b841ff14d6c710"),
        KeyFromHex("03cd69d294ff4dfb956a20cc2d4e91c7fcab5ea76c21c9724803ebb580bd5880"),
        KeyFromHex("835c829227b957748d8ec896cf03eaccfb185528fd0b8f45fdcba46234b0639b"),
    };
}

CBlock MakeCandidateBlock(const uint256& prevHash, const StakeCommitment* commitment = nullptr)
{
    CMutableTransaction coinbase;
    coinbase.nVersion = 1;
    coinbase.vin.resize(1);
    coinbase.vin[0].scriptSig = CScript() << std::vector<unsigned char>{1, 2, 3};
    coinbase.vout.resize(1);
    coinbase.vout[0].nValue = 100 * COIN;
    coinbase.vout[0].scriptPubKey = CScript() << OP_TRUE;
    if (commitment) {
        coinbase.vout.emplace_back(0, BuildStakeCommitmentScript(*commitment));
        BOOST_REQUIRE(!coinbase.vout.back().scriptPubKey.empty());
    }

    CBlock block;
    block.nVersion = 1;
    block.nTime = 1798348800;
    block.nBits = 0x1f00ffff;
    block.nNonce = 0;
    block.hashPrevBlock = prevHash;
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));
    block.hashMerkleRoot = BlockMerkleRoot(block);
    return block;
}

} // namespace

BOOST_AUTO_TEST_CASE(genesis_stakers_convert_with_expected_solo_and_pool_split)
{
    const auto& stakers = Params().GetConsensus().genesisStakers;
    BOOST_REQUIRE_EQUAL(stakers.size(), 3u);

    const auto balances = GenesisStakersToBalances(stakers);
    const auto candidates = AssembleStakeCandidates(balances);

    // staker[0] = 500 QTRN (solo), staker[1]+staker[2] = 30+40 = 70 QTRN
    // (sub-threshold, pooled — one or two pool candidates depending on hash
    // luck, but never solo).
    BOOST_CHECK_EQUAL(stakers[0].second, 500 * COIN);
    BOOST_CHECK_EQUAL(stakers[1].second, 30 * COIN);
    BOOST_CHECK_EQUAL(stakers[2].second, 40 * COIN);

    const uint256 soloId = StakeValidatorIdFromPubKey(stakers[0].first);
    bool foundSolo = false;
    CAmount pooledTotal = 0;
    for (const auto& c : candidates) {
        if (c.id == soloId) {
            foundSolo = true;
            BOOST_CHECK_EQUAL(c.weight, 500 * COIN);
        } else {
            pooledTotal += c.weight;
        }
    }
    BOOST_CHECK(foundSolo);
    BOOST_CHECK_EQUAL(pooledTotal, 70 * COIN);
}

BOOST_AUTO_TEST_CASE(full_pipeline_select_sign_commit_verify_against_real_genesis_stakers)
{
    const auto& stakers = Params().GetConsensus().genesisStakers;
    const auto keys = GenesisStakerKeys();
    BOOST_REQUIRE_EQUAL(stakers.size(), keys.size());

    // Sanity: the throwaway private keys actually correspond to the
    // hardcoded pubkeys in chainparams.cpp — if this ever fails, the two
    // have drifted apart (e.g. chainparams regenerated with different keys).
    for (size_t i = 0; i < stakers.size(); ++i) {
        BOOST_REQUIRE(keys[i].GetPubKey() == stakers[i].first);
    }

    const auto candidates = AssembleStakeCandidates(GenesisStakersToBalances(stakers));
    BOOST_REQUIRE(!candidates.empty());

    // Only the solo staker (id = StakeValidatorIdFromPubKey(pubkey) directly)
    // has a single real key backing its candidate id; a pool candidate's id
    // doesn't correspond to any one private key. staker[0] holds 500 of the
    // 570 total QTRN weight (~88%), so a handful of seed attempts is more
    // than enough to land on it deterministically rather than trusting one
    // fixed seed's luck.
    const uint256 soloId = StakeValidatorIdFromPubKey(stakers[0].first);
    uint256 seed = uint256S("beef");
    size_t winnerIndex = SelectStakeValidator(seed, candidates);
    for (uint32_t attempt = 0; candidates[winnerIndex].id != soloId && attempt < 50; ++attempt) {
        seed = DeriveFallbackSeed(seed, attempt);
        winnerIndex = SelectStakeValidator(seed, candidates);
    }
    BOOST_REQUIRE(winnerIndex < candidates.size());
    const StakeCandidate& winner = candidates[winnerIndex];
    BOOST_REQUIRE(winner.id == soloId); // must have converged within 50 attempts at ~88% odds each

    const CBlock building = MakeCandidateBlock(uint256S("prevhash"));
    const uint256 signingHash = ComputeStakeSigningHash(building);

    StakeCommitment commitment;
    commitment.attempt = 0;
    commitment.validatorPubKey = keys[0].GetPubKey();
    BOOST_REQUIRE(keys[0].Sign(signingHash, commitment.signature));

    const CBlock assembled = MakeCandidateBlock(uint256S("prevhash"), &commitment);

    StakeCommitment parsed;
    BOOST_REQUIRE(FindStakeCommitment(assembled.vtx[0]->vout, parsed));
    const uint256 recomputedHash = ComputeStakeSigningHash(assembled);
    BOOST_CHECK_EQUAL(recomputedHash.ToString(), signingHash.ToString());
    BOOST_CHECK(VerifyStakeCommitment(parsed, recomputedHash, winner.id));
}

BOOST_AUTO_TEST_SUITE_END()
