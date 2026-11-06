#!/bin/bash
# Build and test spine-reader the way the GitHub workflow (.github/workflows/ci.yml) does:
# source an LCG view, configure with SPINE_READER_REQUIRE_PYTHON=ON, build, run ctest.
# With --follow-spine DIR, then run tools/release/follow_spine.py on the model's own SPINE tag,
# with its artifacts in DIR, and require "identical" (exit 0).
# The same script is used for the local dry run (see ci/EGM.md).
#
#   ci/github-ci.sh [--follow-spine DIR] <LCG view setup.sh> [build directory, default build-ci]
set -uo pipefail

follow=
if [[ ${1:-} == --follow-spine ]]; then
  if [[ $# -lt 2 ]]; then
    echo "usage: $0 [--follow-spine DIR] <LCG view setup.sh> [build directory]" >&2
    exit 2
  fi
  follow=$2
  shift 2
fi
if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "usage: $0 [--follow-spine DIR] <LCG view setup.sh> [build directory]" >&2
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

if [[ -n $follow ]]; then
  # the model's SPINE version, from the one committed schema
  version=$(python3 -c 'import glob, json, sys
found = glob.glob(sys.argv[1] + "/schema/spine-*.json")
assert len(found) == 1, found
print(json.load(open(found[0]))["spine_version"])' "$repo")
  echo "== follow_spine.py v$version (artifacts in $follow)"
  "$repo/tools/release/follow_spine.py" "v$version" --artifacts-dir "$follow" --jobs "$(nproc)" ||
    { code=$?; echo "follow_spine.py v$version exited $code; identical (0) is required" >&2
      exit "$code"; }
fi
