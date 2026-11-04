# tools

## Overview

Python developer tools for spine-reader. None of them is built into the C++
library. Both subsystems are tied to SPINE and to the single committed
`schema/spine-*.json`. `gen` turns SPINE's object model into the generated C++
model. `fixtures` produces the golden HDF5 files and their decoded expectations,
which the C++ tests read. Each subsystem registers a ctest check:
`spine_reader_generated_model` (`gen/generate.py --check`) and
`spine_reader_golden_members` (`fixtures/make_member_list.py --check`), registered
only when CMake finds a Python 3 interpreter. Run them
in the LCG_110 view (`source
/cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh`).

## Subdirectories & Boundaries

- gen: [./gen/EGM.md](./gen/EGM.md) - `dump_schema.py` (SPINE ->
  `schema/spine-<version>.json`), `generate.py` (JSON -> `include/spine/model/`,
  `include/spine/model.hpp`, `src/model/`; deterministic; `--check` exits 1 on
  stale files), and `check_names.py` (macro clash scan). Legacy notes:
  [./gen/README.md](./gen/README.md).
- fixtures: [./fixtures/EGM.md](./fixtures/EGM.md) - `make_fixtures.py` writes
  `tests/data/fixtures/<case>.h5` with SPINE's real writers (one `spine_worker.py`
  subprocess per write, litify or read call, each importing a single SPINE tag;
  `PYTHONHASHSEED=0`), decodes them with the SPINE-free `decode_raw.py` into
  `<case>.expected.json`, and cross-checks them. `make_member_list.py` writes or
  checks `tests/golden_members.inc` from the schema. Legacy notes:
  [./fixtures/README.md](./fixtures/README.md).
