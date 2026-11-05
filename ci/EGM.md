# ci

## Overview
Bash scripts that build and test spine-reader in configurations the default build does not
cover, plus the pieces of the GitHub Actions workflow (`.github/workflows/ci.yml`).
- `github-ci.sh` is what each CI job runs, and what the local dry run runs.
- `check_tag_version.sh` checks a release tag against `project(VERSION)`.
- `sanitize.sh` builds with sanitizers on EL9, using the CVMFS LCG_110 view.
- `sl7-ups.sh` builds inside an SL7 container with the UPS toolchain that the experiments'
  LArSoft stacks use.

Every script finds the repository from its own location, so it can be run from any working
directory.

## Interface
- `github-ci.sh <LCG view setup.sh> [build directory]`
  - Sources the given LCG view, then configures `<repo>/<build directory>` (default `build-ci`)
    with `-DSPINE_READER_REQUIRE_PYTHON=ON`, builds with `-j$(nproc)`, and runs
    `ctest --output-on-failure`. No build type is given, so the project default
    (`RelWithDebInfo`) applies, as in CI.
  - Prints the OS, the compiler (`$CXX`, set by the LCG view), CMake, Python and the view.
  - Exit codes: 2 on bad arguments; otherwise the first failing command's code.
- `check_tag_version.sh vX.Y.Z`
  - Reads `project(spine-reader VERSION X.Y.Z ...)` from `CMakeLists.txt`.
  - Exits 0 when the argument equals `vX.Y.Z`, 1 when it does not or when no version line is
    found, and 2 on bad arguments. Messages name both versions.
  - Run it before pushing a release tag. CI runs it again on every tag push.
- `.github/workflows/ci.yml` (in the repository's `.github/` folder)
  - Runs on every push (branches and tags), every pull request and manual dispatch, with
    `permissions: contents: read` and no secrets.
  - Job `hdf5-1_14`: `ubuntu-24.04`, LCG_110 `x86_64-ubuntu2404-gcc13-opt` (HDF5 1.14.6, gcc 13
    from the runner, CMake 3.30, Python 3.13). On a tag push it also runs
    `check_tag_version.sh "$GITHUB_REF_NAME"`.
  - Job `hdf5-1_12`: `ubuntu-22.04`, LCG_105 `x86_64-ubuntu2204-gcc11-opt` (HDF5 1.12.2, gcc 11
    from the runner, CMake 3.26, Python 3.9).
  - Each job checks out the repository, mounts `sft.cern.ch` with
    `cvmfs-contrib/github-action-cvmfs`, and runs `github-ci.sh` with its view. The two actions
    are pinned by commit SHA, with the version in a comment.
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
- The LCG views for Ubuntu use the system compiler, which GitHub's runners provide
  (`ubuntu-24.04`: gcc 13; `ubuntu-22.04`: gcc 11).
- `github-ci.sh` sources the view before it enables `set -euo pipefail`, because LCG setup
  scripts are not `-e`/`-u` clean.
- `sanitize.sh` runs `set -e` before it sources the LCG view, so a failed setup stops the script.
  `sl7-ups.sh` runs `set -e` only after the UPS `setup` calls, because those return non-zero
  even when they succeed. Keep that order.
- `sl7-ups.sh` runs only the `spine_reader_tests` binary. It does not run
  `ctest`, so ctest-only entries such as `spine_dump_output` and the
  Python-based generated-model checks are not run in the SL7 build.
- The values `ON` and `thread` that `sanitize.sh` passes must stay in step with
  the `SPINE_READER_SANITIZE` cache values (`OFF`, `ON`, `thread`) in the
  top-level `CMakeLists.txt`.
- The toolchain versions in `sl7-ups.sh` (gcc 12.1 / e26, HDF5 1.12.2) are the
  targets the UPS stacks build against. The top-level `CMakeLists.txt` requires
  `HDF5 1.12` or newer.
- A release tag is never moved or deleted. If the CI tag check fails after a push, the fix is
  a new patch release.

## Side Effects
- Create and overwrite the build directories `build-ci/` (or the one given to `github-ci.sh`),
  `build-asan/`, `build-tsan/` and `build-sl7/` in the repository root. `build*/` is ignored by
  git.
- Read from CVMFS: the LCG views, `setup_larsoft.sh` and UPS products, and the container
  images. `sl7-ups.sh` and the dry run below need `apptainer` on the host.
- The workflow runs on GitHub's hosted runners, with read-only access to the repository.

## Verification
- Run: `ci/sanitize.sh` (ASan and UBSan), `ci/sanitize.sh thread` (TSan), and
  `ci/sl7-ups.sh` (SL7 UPS toolchain).
- Dry run of the two CI jobs, in CERN's SFT build images on CVMFS (GitHub's runner images
  differ, so the first CI run is the final check):

  ```bash
  img=/cvmfs/unpacked.cern.ch/gitlab-registry.cern.ch/sft/docker
  apptainer exec -B /cvmfs -B /tmp $img/ubuntu24:latest bash -e ci/github-ci.sh \
    /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-ubuntu2404-gcc13-opt/setup.sh build-ci-1.14
  apptainer exec -B /cvmfs -B /tmp $img/ubuntu22:latest bash -e ci/github-ci.sh \
    /cvmfs/sft.cern.ch/lcg/views/LCG_105/x86_64-ubuntu2204-gcc11-opt/setup.sh build-ci-1.12
  ```
- Tag check: `ci/check_tag_version.sh v<project version>` exits 0; any other tag exits 1.
- The default build these scripts vary: `source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh && cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure`
