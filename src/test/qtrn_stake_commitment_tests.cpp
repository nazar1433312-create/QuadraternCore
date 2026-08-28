// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/stake_commitment.h>

#include <consensus/merkle.h>
#include <key.h>
#include <primitives/block.h>
#include <script/script.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

using namespace qtrn;

BOOST_FIXTURE_TEST_SUITE(qtrn_stake_commitment_tests, BasicTestingSetup)

namespace {

// Minimal coinbase-shaped block: one input carrying `tag` as scriptSig data
// (so callers can make two otherwise-identical blocks differ), one ordinary
// payout output, and — unless `withCommitment` — nothing else. Mirrors the
// coinbase-construction pattern chainparams.cpp uses for genesis blocks.
CBlock MakeTestBlock(const std::string& tag, const StakeCommitment* commitment = nullptr)
{
    CMutableTransaction coinbase;
    coinbase.nVersion = 1;
    coinbase.vin.resize(1);
    coinbase.vin[0].scriptSig = CScript() << std::vector<unsigned char>(tag.begin(), tag.end());
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
    block.hashPrevBlock = uint256S("abcd");
    block.vtx.push_back(MakeTransactionRef(std::move(coinbase)));
    block.hashMerkleRoot = BlockMerkleRoot(block);
    return block;
}

StakeCommitment MakeSignedCommitment(const CKey& key, uint32_t attempt, const uint256& signingHash)
{
    StakeCommitment c;
    c.attempt = attempt;
    c.validatorPubKey = key.GetPubKey();
    BOOST_REQUIRE(key.Sign(signingHash, c.signature));
    return c;
}

} // namespace

BOOST_AUTO_TEST_CASE(build_then_find_round_trips)
{
    CKey key;
    key.MakeNewKey(/*fCompressed=*/true);
    const uint256 signingHash = uint256S("aa11");

    const StakeCommitment original = MakeSignedCommitment(key, 3, signingHash);
    const CScript script = BuildStakeCommitmentScript(original);
    BOOST_REQUIRE(!script.empty());

    std::vector<CTxOut> outs;
    outs.emplace_back(0, script);

    StakeCommitment parsed;
    BOOST_REQUIRE(FindStakeCommitment(outs, parsed));
    BOOST_CHECK_EQUAL(parsed.attempt, 3u);
    BOOST_CHECK(parsed.validatorPubKey == original.validatorPubKey);
    BOOST_CHECK(parsed.signature == original.signature);
}

BOOST_AUTO_TEST_CASE(find_skips_unrelated_outputs_and_picks_the_real_one)
{
    CKey key;
    key.MakeNewKey(true);
    const uint256 signingHash = uint256S("bb22");
    const StakeCommitment original = MakeSignedCommitment(key, 0, signingHash);

    std::vector<CTxOut> outs;
    outs.emplace_back(0, CScript() << OP_RETURN << std::vector<unsigned char>{0xde, 0xad, 0xbe, 0xef}); // unrelated OP_RETURN
    outs.emplace_back(5000, CScript() << OP_DUP << OP_HASH160 << OP_EQUALVERIFY << OP_CHECKSIG);        // ordinary spend output
    outs.emplace_back(0, BuildStakeCommitmentScript(original));

    StakeCommitment parsed;
    BOOST_REQUIRE(FindStakeCommitment(outs, parsed));
    BOOST_CHECK(parsed.validatorPubKey == original.validatorPubKey);
}

BOOST_AUTO_TEST_CASE(find_fails_when_no_commitment_present)
{
    std::vector<CTxOut> outs;
    outs.emplace_back(5000, CScript() << OP_DUP << OP_HASH160 << OP_EQUALVERIFY << OP_CHECKSIG);
    StakeCommitment parsed;
    BOOST_CHECK(!FindStakeCommitment(outs, parsed));
}

BOOST_AUTO_TEST_CASE(build_rejects_malformed_input)
{
    CKey key;
    key.MakeNewKey(true);

    StakeCommitment noSig;
    noSig.validatorPubKey = key.GetPubKey();
    BOOST_CHECK(BuildStakeCommitmentScript(noSig).empty()); // empty signature

    StakeCommitment badAttempt;
    badAttempt.validatorPubKey = key.GetPubKey();
    badAttempt.signature = {0x01, 0x02};
    badAttempt.attempt = 0x100; // doesn't fit in a byte
    BOOST_CHECK(BuildStakeCommitmentScript(badAttempt).empty());

    StakeCommitment noKey;
    noKey.signature = {0x01, 0x02};
    BOOST_CHECK(BuildStakeCommitmentScript(noKey).empty()); // invalid pubkey
}

BOOST_AUTO_TEST_CASE(verify_accepts_correct_signature_and_id)
{
    CKey key;
    key.MakeNewKey(true);
    const uint256 signingHash = uint256S("cc33");
    const StakeCommitment commitment = MakeSignedCommitment(key, 0, signingHash);
    const uint256 expectedId = StakeValidatorIdFromPubKey(key.GetPubKey());

    BOOST_CHECK(VerifyStakeCommitment(commitment, signingHash, expectedId));
}

BOOST_AUTO_TEST_CASE(verify_rejects_wrong_signing_hash)
{
    CKey key;
    key.MakeNewKey(true);
    const StakeCommitment commitment = MakeSignedCommitment(key, 0, uint256S("dd44"));
    const uint256 expectedId = StakeValidatorIdFromPubKey(key.GetPubKey());

    // Signature was made over a different hash than the one presented here —
    // this is exactly the replay this scheme must prevent (see the note in
    // stake_commitment.h on why the signed hash can't just be the block hash).
    BOOST_CHECK(!VerifyStakeCommitment(commitment, uint256S("ee55"), expectedId));
}

BOOST_AUTO_TEST_CASE(verify_rejects_id_mismatch)
{
    CKey signerKey;
    signerKey.MakeNewKey(true);
    CKey differentCandidateKey;
    differentCandidateKey.MakeNewKey(true);

    const uint256 signingHash = uint256S("ff66");
    const StakeCommitment commitment = MakeSignedCommitment(signerKey, 0, signingHash);

    // A perfectly valid signature from someone who was never selected must
    // not verify against the id the network actually expected.
    const uint256 wrongExpectedId = StakeValidatorIdFromPubKey(differentCandidateKey.GetPubKey());
    BOOST_CHECK(!VerifyStakeCommitment(commitment, signingHash, wrongExpectedId));
}

BOOST_AUTO_TEST_CASE(verify_rejects_signature_from_wrong_key)
{
    CKey legitimateKey;
    legitimateKey.MakeNewKey(true);
    CKey attackerKey;
    attackerKey.MakeNewKey(true);

    const uint256 signingHash = uint256S("1a2b");
    // Attacker signs, but claims to be the legitimate (expected) validator by
    // putting the legitimate key's id as the expected target — VerifyStakeCommitment
    // must still catch this because the commitment's own pubkey (attacker's)
    // won't hash to that id either way; more directly: attacker's commitment
    // carries the attacker's own pubkey, so the id check alone already fails.
    StakeCommitment forged = MakeSignedCommitment(attackerKey, 0, signingHash);
    const uint256 legitimateId = StakeValidatorIdFromPubKey(legitimateKey.GetPubKey());

    BOOST_CHECK(!VerifyStakeCommitment(forged, signingHash, legitimateId));
}

BOOST_AUTO_TEST_CASE(signing_hash_ignores_the_commitment_output)
{
    CKey key;
    key.MakeNewKey(true);

    const CBlock withoutCommitment = MakeTestBlock("same-block");
    const uint256 hashBefore = ComputeStakeSigningHash(withoutCommitment);

    const StakeCommitment commitment = MakeSignedCommitment(key, 0, hashBefore);
    const CBlock withCommitment = MakeTestBlock("same-block", &commitment);
    const uint256 hashAfter = ComputeStakeSigningHash(withCommitment);

    // The whole point of stripping the commitment output before hashing:
    // adding it must not change what gets signed/verified against.
    BOOST_CHECK_EQUAL(hashBefore.ToString(), hashAfter.ToString());
    // Sanity: the commitment output really did change the block's actual
    // merkle root / GetHash() — this isn't passing merely because the two
    // blocks ended up identical.
    BOOST_CHECK(withoutCommitment.GetHash() != withCommitment.GetHash());
}

BOOST_AUTO_TEST_CASE(signing_hash_reflects_real_content_changes)
{
    const uint256 hashA = ComputeStakeSigningHash(MakeTestBlock("tag-a"));
    const uint256 hashB = ComputeStakeSigningHash(MakeTestBlock("tag-b"));
    BOOST_CHECK(hashA != hashB); // not an accidental constant
}

BOOST_AUTO_TEST_CASE(full_round_trip_sign_commit_recompute_verify)
{
    CKey key;
    key.MakeNewKey(true);

    const CBlock building = MakeTestBlock("round-trip");
    const uint256 signingHash = ComputeStakeSigningHash(building);
    const StakeCommitment commitment = MakeSignedCommitment(key, 0, signingHash);

    const CBlock assembled = MakeTestBlock("round-trip", &commitment);

    // A validating node's view: pull the commitment back out of the received
    // block, recompute the signing hash the same way, and verify.
    StakeCommitment parsed;
    BOOST_REQUIRE(FindStakeCommitment(assembled.vtx[0]->vout, parsed));
    const uint256 recomputedHash = ComputeStakeSigningHash(assembled);
    BOOST_CHECK_EQUAL(recomputedHash.ToString(), signingHash.ToString());

    const uint256 expectedId = StakeValidatorIdFromPubKey(key.GetPubKey());
    BOOST_CHECK(VerifyStakeCommitment(parsed, recomputedHash, expectedId));
}

BOOST_AUTO_TEST_SUITE_END()
