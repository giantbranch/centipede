#!/bin/bash

# Copyright 2022 Centipede Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

set -eu

# Code under repo is checked out to ${KOKORO_ARTIFACTS_DIR}/github.
# The final directory name in this path is determined by the scm name specified
# in the job configuration.
cd "${KOKORO_ARTIFACTS_DIR}/github/centipede/.kokoro/gcp_ubuntu"

DOCKER_IMAGE=debian

docker run \
  --security-opt seccomp=unconfined \
  -v ${KOKORO_ARTIFACTS_DIR}/github/centipede:/app \
  -v ${KOKORO_ARTIFACTS_DIR}:/kokoro \
  --env KOKORO_ARTIFACTS_DIR=/kokoro \
  -w /app \
  "${DOCKER_IMAGE}" \
  /app/.kokoro/gcp_ubuntu/run_tests_inside_docker.sh
