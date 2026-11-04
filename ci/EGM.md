# ci

## Overview
Two bash scripts that build and test spine-reader in configurations that the
default build does not cover. `sanitize.sh` builds with sanitizers on EL9 using
the CVMFS LCG_110 view. `sl7-ups.sh` builds inside an SL7 container with the
UPS toolchain that the experiments' LArSoft stacks use. Both scripts find the
repository from their own location, so they can be run from any working
directory.

## Interface
- `sanitize.sh [mode]`
  - With no argument (`mode=address`, the default) it configures with
    `-DSPINE_READER_SANITIZE=ON`, which enables ASan and UBSan with
    `-fno-sanitize-recover=undefined` (see `CMakeLists.txt`). It builds in
    `<repo>/build-asan`.
  - With `thread` it configures with `-DSPINE_READER_SANITIZE=thread` and builds
    in `<repo>/build-tsan`. Any other argument is treated like the default.
  - Steps: source `/cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh`,
    then `cmake -S`, `cmake --build -j8`, `ctest --output-on-failure`.
- `sl7-ups.sh [extra cmake args]`
  - When run on the host, it re-runs itself through `apptainer exec` in
    `/cvmfs/singularity.opensciencegrid.org/fermilab/fnal-wn-sl7:latest`. It
    bind-mounts `/cvmfs`, `/tmp` and the repository, and sets
    `SPINE_READER_IN_SL7=1` so that the second run does not re-launch the
    container.
  - Inside the container it sources `setup_larsoft.sh` and runs
    `setup gcc v12_1_0`, `setup cmake v3_22_2` and
    `setup hdf5 v1_12_2b -q e26:prof`. It then configures `<repo>/build-sl7`
    with `-DHDF5_ROOT="$HDF5_FQ_DIR"` plus any arguments passed to the script,
    builds with `-j8`, and runs `build-sl7/tests/spine_reader_tests` directly.

## Invariants
- `sanitize.sh` runs `set -e` before it sources the LCG view, so a failed setup
  stops the script. `sl7-ups.sh` runs `set -e` only after the UPS `setup` calls,
  because those return non-zero even when they succeed. Keep that order.
- `sl7-ups.sh` runs only the `spine_reader_tests` binary. It does not run
  `ctest`, so ctest-only entries such as `spine_dump_output` and the
  Python-based generated-model checks are not run in the SL7 build.
- The values `ON` and `thread` that `sanitize.sh` passes must stay in step with
  the `SPINE_READER_SANITIZE` cache values (`OFF`, `ON`, `thread`) in the
  top-level `CMakeLists.txt`.
- The toolchain versions in `sl7-ups.sh` (gcc 12.1 / e26, HDF5 1.12.2) are the
  targets the UPS stacks build against. The top-level `CMakeLists.txt` requires
  `HDF5 1.12` or newer.

## Side Effects
- Create and overwrite the build directories `build-asan/`, `build-tsan/` and
  `build-sl7/` in the repository root.
- Read from CVMFS: the LCG view, `setup_larsoft.sh` and UPS products, and the
  SL7 container image. `sl7-ups.sh` needs `apptainer` on the host.

## Verification
- Run: `ci/sanitize.sh` (ASan and UBSan), `ci/sanitize.sh thread` (TSan), and
  `ci/sl7-ups.sh` (SL7 UPS toolchain).
- The default build these scripts vary: `source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh && cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure`
