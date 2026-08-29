// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// testnet-2: which of the three parallel PoW algorithms (spec §6.1) mined a
// given block. Encoded in nVersion rather than a new header field, to keep
// the 80-byte header format unchanged — the same reasoning as SegWit's own
// version-bit signaling, just repurposed: real precedent for algo-in-version
// encoding exists in other multi-algo chains (Myriadcoin, early Verge).

#ifndef BITCOIN_QTRN_POW_ALGO_H
#define BITCOIN_QTRN_POW_ALGO_H

#include <cstdint>
#include <string>

namespace qtrn {

enum class PowAlgo {
    SHA256D = 0, // ASIC channel — same hash CBlockHeader::GetHash() already uses
    PROGPOW = 1, // GPU channel
    RANDOMX = 2, // CPU channel
    // value 3 is deliberately left unassigned (2 bits encode 4 values, only 3 used)
};

//! Bits 8-9 of nVersion carry the algo id. Chosen to avoid every bit position
//! already claimed by a real (if currently NEVER_ACTIVE) BIP9 deployment in
//! this tree (DEPLOYMENT_TAPROOT=2, DEPLOYMENT_MWEB=4, DEPLOYMENT_TESTDUMMY=28
//! — see chainparams.cpp) and the top-3-bit 0x20000000 BIP9 marker pattern
//! (versionbits.h) — so a block's algo tag never collides with BIP9
//! deployment-bit interpretation of the same nVersion value. Bits 8-9 must
//! never be assigned to a BIP9 deployment in the future.
static constexpr int32_t VERSION_ALGO_SHIFT = 8;
static constexpr int32_t VERSION_ALGO_MASK = 0x3 << VERSION_ALGO_SHIFT; // 0x00000300

//! Extracts the algo id from a block header's nVersion. Never fails —
//! callers that need to reject an invalid/unassigned value should compare
//! against IsValidPowAlgo() explicitly (this function alone can't distinguish
//! "value 3, unassigned" from "a real algo", it just decodes the 2 bits).
PowAlgo GetPowAlgo(int32_t nVersion);

//! Returns `nVersion` with bits 8-9 replaced to encode `algo`, all other bits
//! untouched (including the BIP9 top-bits marker and any deployment-signal
//! bits already set).
int32_t SetPowAlgo(int32_t nVersion, PowAlgo algo);

//! Whether `algo` is one of the three assigned values (rejects the
//! unassigned 4th 2-bit value some crafted/corrupted nVersion could carry).
bool IsValidPowAlgo(PowAlgo algo);

//! Short lowercase name ("sha256d"/"progpow"/"randomx"), for logging/RPC —
//! never empty, "unknown" for an invalid algo rather than throwing.
std::string PowAlgoName(PowAlgo algo);

} // namespace qtrn

#endif // BITCOIN_QTRN_POW_ALGO_H
