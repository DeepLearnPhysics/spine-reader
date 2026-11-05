#!/bin/bash
# Build and test spine-reader the way the GitHub workflow (.github/workflows/ci.yml) does:
# source an LCG view, configure with SPINE_READER_REQUIRE_PYTHON=ON, build, run ctest.
# The same script is used for the local dry run (see ci/EGM.md).
#
#   ci/github-ci.sh <LCG view setup.sh> [build directory, default build-ci]
set -uo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "usage: $0 <LCG view setup.sh> [build directory]" >&2
  exit 2
fi
view=$1
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build=$repo/${2:-build-ci}

# LCG setup scripts are not -e / -u clean: source the view before strict mode.
set +eu
# shellcheck disable=SC1090
source "$view"
set -euo pipefail

echo "== $(. /etc/os-release && echo "$PRETTY_NAME") | $("${CXX:-g++}" --version | head -1)"
echo "== $(cmake --version | head -1) | $(python3 --version) | view $view"

cmake -S "$repo" -B "$build" -DSPINE_READER_REQUIRE_PYTHON=ON
cmake --build "$build" -j"$(nproc)"
ctest --test-dir "$build" --output-on-failure
