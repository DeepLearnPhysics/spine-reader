#!/bin/bash
# Build and test spine-reader the way the experiments' UPS stacks would: SL7, gcc 12.1 (e26),
# HDF5 1.12.2 (UPS hdf5 v1_12_2b). Run from an EL9 host with CVMFS and apptainer:
#
#   ci/sl7-ups.sh [extra cmake args]
#
# The build goes to build-sl7/ in the repository.

repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

if [[ -z "$SPINE_READER_IN_SL7" ]]; then
  exec apptainer exec -B /cvmfs -B /tmp -B "$repo" --env SPINE_READER_IN_SL7=1 \
    /cvmfs/singularity.opensciencegrid.org/fermilab/fnal-wn-sl7:latest \
    bash "$repo/ci/sl7-ups.sh" "$@"
fi

# UPS setup scripts return non-zero on success, so only fail from here on.
source /cvmfs/larsoft.opensciencegrid.org/setup_larsoft.sh >/dev/null 2>&1
setup gcc v12_1_0
setup cmake v3_22_2
setup hdf5 v1_12_2b -q e26:prof
set -e

cmake -S "$repo" -B "$repo/build-sl7" -DHDF5_ROOT="$HDF5_FQ_DIR" "$@"
cmake --build "$repo/build-sl7" -j8
"$repo/build-sl7/tests/spine_reader_tests"
