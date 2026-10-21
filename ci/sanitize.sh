#!/bin/bash
# Build and test spine-reader with sanitizers.
#
#   ci/sanitize.sh            AddressSanitizer + UBSan (+ LeakSanitizer, which comes with ASan);
#                             the build goes to build-asan/
#   ci/sanitize.sh thread     ThreadSanitizer; the build goes to build-tsan/
#
# Out-of-range reads that give the right answer by luck, such as one element past the end of a
# vector, and races that happen not to corrupt anything, only show up here.

repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
mode=${1:-address}
set -e
source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh
if [[ "$mode" == thread ]]; then
  build="$repo/build-tsan"
  sanitize=thread
else
  build="$repo/build-asan"
  sanitize=ON
fi
cmake -S "$repo" -B "$build" -DSPINE_READER_SANITIZE=$sanitize
cmake --build "$build" -j8
ctest --test-dir "$build" --output-on-failure
