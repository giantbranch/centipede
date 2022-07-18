#!/bin/bash

# Copyright 2022 The Centipede Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Run a short fuzzing session for one puzzle and check the outcome.
# This script is executed under the name run_S_PUZZLE_NAME, where S is a single
# digit representing the seed, so we get the seed and puzzle name from $0.
# Every puzzle must have one or more lines containing "RUN:<program-text>"
# This script will execute <program-text> directly in the current context.
# <program-text> can use the functions defined in this file, see USER_FUNCTIONS.

set -eu -o pipefail

readonly seed_and_puzzle_name="${0#*puzzles/run_}"
readonly seed="${seed_and_puzzle_name:0:1}"
readonly puzzle_name="${seed_and_puzzle_name:2}"
readonly puzzle_source_name="${puzzle_name}.cc"
readonly centipede_dir="${TEST_SRCDIR}/centipede"
readonly puzzle_path="${centipede_dir}/puzzles/${puzzle_name}_centipede"
readonly puzzle_source_path="${centipede_dir}/puzzles/${puzzle_source_name}"

readonly centipede="${centipede_dir}/centipede"
readonly workdir="${TEST_TMPDIR}/workdir"
readonly log="${TEST_TMPDIR}/log"
readonly script="${TEST_TMPDIR}/script"

# Read the configuration from the puzzle source.
grep 'RUN:' "${puzzle_source_path}" | sed 's/^.*RUN://' > "${script}"
echo ======== SCRIPT
cat "${script}"
echo ======== END SCRIPT

##################################### USER_FUNCTIONS

# Runs Centipede with additional parameters in $@,
# saves the result in log, cats the log.
# Expects Centipde to exit with failure.
function Run() {
  echo "======== Run $*"
  rm -rf "${workdir}"
  mkdir "${workdir}"
  "${centipede}" \
    --alsologtostderr \
    --workdir "${workdir}" \
    --binary "${puzzle_path}" \
    --seed="${seed}" \
    --num_runs=1000000 \
    --timeout=10 \
    --exit_on_crash \
    "$@" \
    > "${log}" 2>&1 && exit 1  # Centipede must exit with failure.
  cat "${log}"
}

# Checks that $1 is the solution for the puzzle.
function SolutionIs() {
  echo ====== SolutionIs: $1
  grep "input bytes: $1" "${log}"
}

# Expects that Centipde found a timeout.
function ExpectTimeout() {
  echo ======= ExpectTimeout
  grep "timeout of .* seconds exceeded; exiting" "${log}"
  grep "failure description: timeout-exceeded" "${log}"
}

# Expects that Centipede found a OOM.
function ExpectOOM() {
  echo ======= ExpectOOM
  grep "failure description: out-of-memory" "${log}"
}

# Expects that $1 is found in the log.
function ExpectInLog() {
  echo ======= ExpectInLog: $1
  grep "$1" "${log}"
}

##################################### end USER_FUNCTIONS

# shellcheck disable=SC1090
source "${script}"

echo PASS