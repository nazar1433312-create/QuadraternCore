// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/stake_commitment.h>

#include <hash.h>

#include <algorithm>

namespace qtrn {

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
    static constexpr size_t kMinDataLen = 4 + 1 + CPubKey::COMPRESSED_SIZE + 1; // +1: signature must be non-empty

    for (const CTxOut& txout : coinbaseOutputs) {
        const CScript& script = txout.scriptPubKey;
        if (script.empty() || script[0] != OP_RETURN) continue;

        CScript::const_iterator pc = script.begin() + 1;
        opcodetype opcode;
        std::vector<unsigned char> data;
        if (!script.GetOp(pc, opcode, data)) continue;
        if (data.size() < kMinDataLen) continue;
        if (!std::equal(std::begin(STAKE_COMMITMENT_MAGIC), std::end(STAKE_COMMITMENT_MAGIC), data.begin())) continue;

        size_t pos = 4;
        const uint32_t attempt = data[pos++];

        const std::vector<unsigned char> pubkeyBytes(data.begin() + pos, data.begin() + pos + CPubKey::COMPRESSED_SIZE);
        pos += CPubKey::COMPRESSED_SIZE;
        CPubKey pubkey(pubkeyBytes.begin(), pubkeyBytes.end());
        if (!pubkey.IsValid() || pubkey.size() != CPubKey::COMPRESSED_SIZE) continue;

        std::vector<unsigned char> signature(data.begin() + pos, data.end());
        if (signature.empty()) continue;

        out.attempt = attempt;
        out.validatorPubKey = pubkey;
        out.signature = std::move(signature);
        return true;
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

} // namespace qtrn
