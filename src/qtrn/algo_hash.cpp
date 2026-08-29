// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/algo_hash.h>

#include <hash.h>

#include <string>

namespace qtrn {

namespace {

// STUB — see algo_hash.h. Domain-separated (distinct fixed prefix per algo)
// so the three algos never collide on the same header content, which is all
// the surrounding difficulty/fork-choice code needs from them for now.
uint256 ProgPowHash_STUB(const CBlockHeader& header)
{
    CHashWriter writer(SER_GETHASH, 0);
    writer << std::string("QTRN_PROGPOW_STUB_V1") << header;
    return writer.GetHash();
}

uint256 RandomXHash_STUB(const CBlockHeader& header)
{
    CHashWriter writer(SER_GETHASH, 0);
    writer << std::string("QTRN_RANDOMX_STUB_V1") << header;
    return writer.GetHash();
}

} // namespace

uint256 ComputeAlgoHash(const CBlockHeader& header)
{
    switch (GetPowAlgo(header.nVersion)) {
    case PowAlgo::SHA256D: return header.GetHash(); // real, final — SerializeHash, same as today
    case PowAlgo::PROGPOW: return ProgPowHash_STUB(header);
    case PowAlgo::RANDOMX: return RandomXHash_STUB(header);
    }
    return uint256(); // unassigned algo id — treat as automatic PoW failure
}

bool IsAlgoHashReal(PowAlgo algo)
{
    return algo == PowAlgo::SHA256D;
}

} // namespace qtrn
