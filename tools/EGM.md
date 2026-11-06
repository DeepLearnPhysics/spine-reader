# tools

## Overview

Python developer tools for spine-reader. None of them is built into the C++
library. They are tied to SPINE and to the single committed
`schema/spine-*.json`. `gen` turns SPINE's object model into the generated C++
model. `fixtures` produces the golden HDF5 files and their decoded expectations,
which the C++ tests read. `release` holds the tooling for following SPINE
releases; today `scan_consumer_fields.py`, which lists the model fields a
CAFMaker build reads. `gen` and `fixtures` each register a ctest check:
`spine_reader_generated_model` (`gen/generate.py --check`) and
`spine_reader_golden_members` (`fixtures/make_member_list.py --check`).
`tests/CMakeLists.txt` registers them when CMake finds a Python 3 interpreter;
with `-DSPINE_READER_REQUIRE_PYTHON=ON` (CI turns it on) a missing interpreter
fails the configuration instead of leaving them out. Run the tools in the
LCG_110 view (`source
/cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh`), except
`scan_consumer_fields.py`, which needs only Python 3.9 and runs in the
CAFMaker's environment.

## Subdirectories & Boundaries

- gen: [./gen/EGM.md](./gen/EGM.md) - `dump_schema.py` (SPINE ->
  `schema/spine-<version>.json`), `generate.py` (JSON -> `include/spine/model/`,
  `include/spine/model.hpp`, `src/model/`; deterministic; `--check` exits 1 on
  stale files), and `check_names.py` (macro clash scan). Legacy notes:
  [./gen/README.md](./gen/README.md).
- fixtures: [./fixtures/EGM.md](./fixtures/EGM.md) - `make_fixtures.py` writes
  `tests/data/fixtures/<case>.h5` with SPINE's real writers (one `spine_worker.py`
  subprocess per write, litify or read call, each importing a single SPINE commit;
  `PYTHONHASHSEED=0`): the current set (`current_*`) with any tag, by default the
  model's, and the frozen cases with their own releases. It decodes them with the
  SPINE-free `decode_raw.py` into `<case>.expected.json`, cross-checks them, and
  records each case's SPINE tag and commit in `manifest.json`. `spine_checkout.py`
  resolves tags and clones commits. `make_member_list.py` writes or checks
  `tests/golden_members.inc` from the schema. Legacy notes:
  [./fixtures/README.md](./fixtures/README.md).
- release: [./release/EGM.md](./release/EGM.md) - `scan_consumer_fields.py`
  (compiles a CAFMaker's sources with `-fsyntax-only` against model headers whose
  members are marked `[[deprecated]]`, and lists the fields read; with
  `--contracts`, the ones that have no row in `tests/test_contracts.cpp`).
