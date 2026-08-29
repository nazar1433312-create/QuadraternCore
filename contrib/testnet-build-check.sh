#!/bin/bash
# Copyright (c) 2026 The Quadratern developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Incremental build+test check used throughout testnet-1/testnet-2
# development. Runs inside a throwaway ubuntu:24.04 Docker container with
# this repo bind-mounted at /repo — never touch the repo working tree while
# this is running (see "Уроки" in PROGRESS.md: editing files or the log file
# while a build is in flight has caused real problems before).
#
# Add every new qtrn_*_tests suite name to the --run_test= list below as it's
# created, so it actually gets exercised by this script.
#
# How to invoke (from the host, NOT inside this script):
#
#   docker rm -f quadratern-build >/dev/null 2>&1
#   cd ~/quadraterncore   # or wherever this repo is checked out
#   docker run --name quadratern-build --rm \
#     -v ~/quadraterncore:/repo \
#     -v $(pwd)/contrib/testnet-build-check.sh:/build_check.sh:ro \
#     ubuntu:24.04 bash /build_check.sh \
#     > /path/to/some/build.log 2>&1
#   echo "DOCKER_EXIT_CODE=$?" >> /path/to/some/build.log
#
# Run this in the background (run_in_background / &) and wait for the
# notification rather than polling — see PROGRESS.md for the full set of
# lessons learned about this workflow (mid-build edits, sed -i on a live log
# file, the false-positive exit-code issue, etc).

set -e
apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
  build-essential libtool autotools-dev automake pkg-config bsdmainutils python3 \
  libssl-dev libevent-dev libboost-dev libboost-system-dev libboost-filesystem-dev \
  libboost-test-dev libboost-thread-dev libdb5.3++-dev libsqlite3-dev libfmt-dev >/tmp/apt.log 2>&1
cd /repo
./autogen.sh
./configure --without-gui --disable-bench --with-incompatible-bdb
# Build only the test binary target, not the full `make` (litecoind/litecoin-cli/etc
# get exercised too, since test/test_litecoin links against the same
# libraries — building just this target is cheaper than a full `make`).
make -j6 -C src test/test_litecoin
echo "=== RUNNING qtrn_* SUITES ONLY (not the full upstream suite) ==="
./src/test/test_litecoin --run_test=qtrn_stake_selection_tests,qtrn_stake_commitment_tests,qtrn_stake_pool_tests,qtrn_genesis_stakers_tests,qtrn_stake_commitment_consensus_tests,qtrn_local_stake_signer_tests,qtrn_staked_block_tests,qtrn_pow_algo_tests,qtrn_algo_hash_tests,qtrn_lwma_difficulty_tests,qtrn_pow_retarget_tests --log_level=all
