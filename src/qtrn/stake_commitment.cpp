// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/stake_commitment.h>

#include <consensus/merkle.h>
#include <hash.h>

#include <algorithm>

namespace qtrn {

namespace {

// Shared by FindStakeCommitment (which needs the parsed content) and
// ComputeStakeSigningHash (which only needs to know "is this script the
// commitment, so I can strip it") — kept as one function so the two never
// silently disagree about what counts as a valid commitment output.
bool TryParseStakeCommitmentScript(const CScript& script, StakeCommitment& out)
{
    static constexpr size_t kMinDataLen = 4 + 1 + CPubKey::COMPRESSED_SIZE + 1; // +1: signature must be non-empty

    if (script.empty() || script[0] != OP_RETURN) return false;

    CScript::const_iterator pc = script.begin() + 1;
    opcodetype opcode;
    std::vector<unsigned char> data;
    if (!script.GetOp(pc, opcode, data)) return false;
    if (data.size() < kMinDataLen) return false;
    if (!std::equal(std::begin(STAKE_COMMITMENT_MAGIC), std::end(STAKE_COMMITMENT_MAGIC), data.begin())) return false;

    size_t pos = 4;
    const uint32_t attempt = data[pos++];

    const std::vector<unsigned char> pubkeyBytes(data.begin() + pos, data.begin() + pos + CPubKey::COMPRESSED_SIZE);
    pos += CPubKey::COMPRESSED_SIZE;
    CPubKey pubkey(pubkeyBytes.begin(), pubkeyBytes.end());
    if (!pubkey.IsValid() || pubkey.size() != CPubKey::COMPRESSED_SIZE) return false;

    std::vector<unsigned char> signature(data.begin() + pos, data.end());
    if (signature.empty()) return false;

    out.attempt = attempt;
    out.validatorPubKey = pubkey;
    out.signature = std::move(signature);
    return true;
}

} // namespace

CScript BuildStakeCommitmentScript(const StakeCommitment& commitment)
{
    if (!commitment.validatorPubKey.IsValid() || commitment.validatorPubKey.size() != CPubKey::COMPRESSED_SIZE) {
        return CScript();
    }
    if (commitment.attempt > 0xFF) return CScript();
    if (commitment.signature.empty()) return CScript();

    std::vector<unsigned char> data;
    data.reserve(4 + 1 + CPubKey::COMPRESSED_SIZE + commitment.signature.size());
    data.insert(data.end(), std::begin(STAKE_COMMITMENT_MAGIC), std::end(STAKE_COMMITMENT_MAGIC));
    data.push_back(static_cast<unsigned char>(commitment.attempt));
    data.insert(data.end(), commitment.validatorPubKey.begin(), commitment.validatorPubKey.end());
    data.insert(data.end(), commitment.signature.begin(), commitment.signature.end());

    return CScript() << OP_RETURN << data;
}

bool FindStakeCommitment(const std::vector<CTxOut>& coinbaseOutputs, StakeCommitment& out)
{
    for (const CTxOut& txout : coinbaseOutputs) {
        if (TryParseStakeCommitmentScript(txout.scriptPubKey, out)) return true;
    }
    return false;
}

uint256 StakeValidatorIdFromPubKey(const CPubKey& pubKey)
{
    return Hash(std::vector<unsigned char>(pubKey.begin(), pubKey.end()));
}

bool VerifyStakeCommitment(const StakeCommitment& commitment, const uint256& signingHash, const uint256& expectedValidatorId)
{
    if (!commitment.validatorPubKey.IsValid()) return false;
    if (StakeValidatorIdFromPubKey(commitment.validatorPubKey) != expectedValidatorId) return false;
    return commitment.validatorPubKey.Verify(signingHash, commitment.signature);
}

uint256 ComputeStakeSigningHash(const CBlock& block)
{
    assert(!block.vtx.empty());

    // Strip the commitment output out of a copy of the coinbase, if one is
    // there — a block being built by a miner won't have it yet (nothing to
    // strip, the loop below just won't find a match); a block being
    // validated will (that's the one case this actually does something).
    CMutableTransaction strippedCoinbase(*block.vtx[0]);
    StakeCommitment ignored;
    for (auto it = strippedCoinbase.vout.begin(); it != strippedCoinbase.vout.end(); ++it) {
        if (TryParseStakeCommitmentScript(it->scriptPubKey, ignored)) {
            strippedCoinbase.vout.erase(it);
            break; // BuildStakeCommitmentScript only ever produces one such output
        }
    }

    std::vector<uint256> leaves;
    leaves.resize(block.vtx.size());
    leaves[0] = CTransaction(strippedCoinbase).GetHash();
    for (size_t i = 1; i < block.vtx.size(); ++i) {
        leaves[i] = block.vtx[i]->GetHash();
    }

    CBlockHeader header(block.GetBlockHeader());
    header.hashMerkleRoot = ComputeMerkleRoot(std::move(leaves));
    return header.GetHash();
}

} // namespace qtrn
