# examples

## Overview
`spine-dump` is a small command-line program that prints what `spine::Reader`
reads from one or more SPINE HDF5 files. It is the only example in the
repository. It is a debugging aid, and its output is pinned by the
`spine_dump_output` ctest entry. It uses only the public API from
`include/spine/reader.hpp` and `include/spine/error.hpp` and links against the
`spine_reader` library.

## Interface
- `spine_dump.cpp:main`, built as the executable target `spine-dump`
  (`CMakeLists.txt`: `add_executable(spine-dump examples/spine_dump.cpp)`). It
  is built and installed to `${CMAKE_INSTALL_BINDIR}` only when the option
  `SPINE_READER_BUILD_TOOLS` is on. The option defaults to on when this is the
  top-level project.
- Command line: `spine-dump [--events N] file.h5 [more.h5 ...]`, or `spine-dump --version`.
  - `--version` prints `spine-dump: spine-reader <spine::version()> (model from SPINE
    <SPINE_READER_MODEL_SPINE_VERSION>)` and exits `0`, whatever other arguments are given;
    no file is opened.
  - `--events N` sets the number of events to print (default `5`). `N` is parsed
    with `std::strtoull`. A `--events` with nothing after it is treated as a
    file path.
  - Every other argument is treated as an input file path.
- Output on stdout, in this order:
  1. `reader.schema_report(f).summary()` for each file.
  2. `<num_events> events`.
  3. For each of the first `min(N, num_events)` events, one line:
     `event i (run R, subrun S, event E): ...`. The run part is printed only
     when `event.run_info()` is non-null. The line gives the counts of reco
     interactions, reco particles, truth interactions and truth particles.
  4. If the event has any reco particles, a line for the first one with its
     `id`, `pdg_code`, `ke` (MeV), `start_point` and `units`. When
     `event.interaction_of(p)` is non-null, this line also gives the size of
     `event.particles_of(*interaction)`.
- Exit codes: `0` on success. `2` with a usage message on stderr when no file is
  given. `1` when a `spine::Error` is thrown; `e.what()` goes to stderr and names
  the file.
- Library symbols used: `spine::Reader` (`num_files`, `schema_report`,
  `num_events`, `event`), `spine::SchemaReport::summary`, `spine::Event`
  (`run_info`, `reco_interactions`, `reco_particles`, `truth_interactions`,
  `truth_particles`, `interaction_of`, `particles_of`), `spine::RunInfo`,
  `spine::RecoParticle`, `spine::RecoInteraction`, `spine::Error`.

## Invariants
- The exact output format is tested. `tests/check_spine_dump.cmake` matches
  substrings such as `"6 events\nevent 0 (run 7, subrun 2, event 101): ..."`,
  checks the `--events` limit (`--events 1`, `3`, `99`; the default of 5 is not
  tested), checks `--version`, and checks exit codes `2` and `1`. It gets the
  expected spine-reader and model versions from `tests/CMakeLists.txt`
  (`-DREADER_VERSION`, `-DMODEL_VERSION`); the file versions it expects
  (`SPINE 1.4.0`) describe the frozen test files.
  Any change to the printed text, the line order or the exit codes must be made
  in that script too.
- Only `spine::Error` is caught. Any other exception ends the program
  (`std::terminate`).
- The program includes only the public headers under `include/spine/`
  (`spine/error.hpp`, `spine/reader.hpp`), so it also works as a check that the
  installed API is enough for a downstream user.

## Side Effects
- Opens the given HDF5 files read-only through `spine::Reader`. It writes no
  files.
- Writes to stdout and stderr only.

## Verification
- Run: `source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh && cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure -R spine_dump_output`
- Full suite: the same command without `-R spine_dump_output`.
