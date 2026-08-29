# testnet-1 genesis staker keys

**Throwaway test keys. Zero real value. Never reuse these outside Quadratern testnet-1.**

These are the private keys behind the fixed genesis staker set hardcoded in
`CMainParams::genesisStakers` (`src/chainparams.cpp`) — see `PROGRESS.md` for
why testnet-1 uses a small fixed staker list instead of a full address-balance
index. They exist purely so the hybrid PoW+PoS mechanism can be exercised
against real chain data (mining a block, having one of these keys sign the
PoS commitment, broadcasting, verifying) before the real indexing/wallet
machinery exists.

Generated with `openssl ecparam -name secp256k1 -genkey`. Anyone who has these
private keys can sign as these validators on testnet-1 — that is expected and
fine, since testnet-1 coins are worthless by design. Do not fund these
addresses with anything you'd mind losing, and do not carry them forward to
testnet-2/3 or mainnet.

| # | Weight | Private key (hex) | Compressed pubkey (hex) |
|---|--------|--------------------|--------------------------|
| 1 | 500 QTRN (solo) | `907fd30b8d3184871321e3d66e67eebca91f692a7c833d07c6b841ff14d6c710` | `027de7a74750f99a7d861170c0bacd63ed64ecfee2129913c08260fb7ea6c833b3` |
| 2 |  30 QTRN (pooled) | `03cd69d294ff4dfb956a20cc2d4e91c7fcab5ea76c21c9724803ebb580bd5880` | `02b47087004663b4ec0aa263a19c90e1992b929847d3a5ce34bb9db79ce7d78016` |
| 3 |  40 QTRN (pooled) | `835c829227b957748d8ec896cf03eaccfb185528fd0b8f45fdcba46234b0639b` | `0398cc9ca0159dbbcc52f7fa73a622a2a517ba29c808211625d6a4c8cbb2de9c06` |

Staker 1 alone clears `SOLO_STAKE_MINIMUM` (100 QTRN) and is individually
selectable by id (`StakeValidatorIdFromPubKey`). Stakers 2 and 3 are
sub-threshold and fold into whichever virtual pool `DeterministicPoolId`
assigns them to (spec §6.3) — exercising both the solo and pooled code paths
against real chainparams data. See
`src/test/qtrn_genesis_stakers_tests.cpp` for the full pipeline test using
these exact keys.
