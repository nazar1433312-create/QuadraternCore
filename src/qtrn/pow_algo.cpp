// Copyright (c) 2026 The Quadratern developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qtrn/pow_algo.h>

namespace qtrn {

PowAlgo GetPowAlgo(int32_t nVersion)
{
    return static_cast<PowAlgo>((nVersion & VERSION_ALGO_MASK) >> VERSION_ALGO_SHIFT);
}

int32_t SetPowAlgo(int32_t nVersion, PowAlgo algo)
{
    const int32_t cleared = nVersion & ~VERSION_ALGO_MASK;
    const int32_t bits = (static_cast<int32_t>(algo) << VERSION_ALGO_SHIFT) & VERSION_ALGO_MASK;
    return cleared | bits;
}

bool IsValidPowAlgo(PowAlgo algo)
{
    return algo == PowAlgo::SHA256D || algo == PowAlgo::PROGPOW || algo == PowAlgo::RANDOMX;
}

std::string PowAlgoName(PowAlgo algo)
{
    switch (algo) {
    case PowAlgo::SHA256D: return "sha256d";
    case PowAlgo::PROGPOW: return "progpow";
    case PowAlgo::RANDOMX: return "randomx";
    }
    return "unknown";
}

} // namespace qtrn
