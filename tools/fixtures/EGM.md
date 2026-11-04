# tools/fixtures

## Overview

Generates and checks the golden HDF5 test data in `tests/data/fixtures/`. Small
files are written by SPINE's own `HDF5Writer` (one `spine_worker.py` subprocess per
write, litify or read call; each subprocess imports a single SPINE tag).
They are then decoded by an independent h5py-only decoder into
`<case>.expected.json` and cross-checked against each other and against SPINE's
`HDF5Reader`. The directory also generates `tests/golden_members.inc`, the
per-class member list from `schema/`. The hand-written
[README.md](./README.md) is the detailed reference for the cases, the expected
JSON format and the findings about the real writers. This file is the contract
summary; it does not replace that README.

| file | role |
| --- | --- |
| `make_fixtures.py` | driver: clones SPINE tags, runs the workers, decodes, cross-checks, prints sizes |
| `spine_worker.py` | the only module that imports SPINE; subcommands `write`, `litify`, `read` |
| `decode_raw.py` | independent V1 / V2 decoder (h5py + numpy, no SPINE); library and CLI |
| `content.py` | the synthetic events as plain Python dicts (no SPINE, no I/O) |
| `make_member_list.py` | writes or checks `tests/golden_members.inc` from `schema/spine-*.json` |

## Interface

### `make_fixtures.py` (CLI)

- `CASES`: 10 cases (`v1.4.0_v1`, `v1.4.0_v2`, `v1.4.0_v1_lite`, `v1.4.0_v2_lite`,
  `v2root`, `v0.11.1_v1`, `v1.4.0_v2_fixed_only`, `incomplete`, and the unshipped
  `v1.4.0_exotic_v{1,2}` with `ship=False`). Optional keys: tag, fv, lite,
  products, n_events, derive, incomplete, exotic, and ship (defaults to True).
- `LITE_SKIPPED = {index, orig_index, index_adapt, index_g4}`, `OBJECT_LISTS`,
  `ENUM_COLUMNS`, `DEFAULT_OUT` (`<repo>/tests/data/fixtures`), `DEFAULT_SPINE_REPO`
  (`https://github.com/DeepLearnPhysics/spine`).
- Helpers: `ensure_clone`, `run_worker`, `diff_docs`, `layout_free`, `restrict`,
  `check_content`, `reader_check`, `main`.
- Flags:
  - `--out` (default `DEFAULT_OUT`)
  - `--spine-repo` (default `$SPINE_REPO`, else GitHub)
  - `--workdir` (default `$SPINE_FIXTURE_WORKDIR`, else `<tmp>/spine-reader-fixtures`;
    holds the `spine_<tag>` clones)
  - `--libver` (default `v110`)
  - `--only CASE...`
  - `--with-exotic`
  - `--no-reader-check`
  - `--report FILE`

### `spine_worker.py` (CLI, run only by `make_fixtures.run_worker`)

- `write`: `--out` (required), `--format-version` (default 1), `--lite`,
  `--incomplete` (skip `finalize()`), `--n-events`, `--exotic`,
  `--products {all,core,min}`, `--libver`. Exits with `SPINE <v> cannot write
  format_version <n>` when the writer predates V2. `--exotic` ignores `--products` and
  `--n-events`.
- `litify`: `--src`, `--out` (required), `--libver`. Runs
  `litify_hdf5(mode="fixed_only", keys=DEFAULT_LITE_KEYS)`.
- `read`: `--file`, `--out` (required). Dumps `spine_version`, `strict_open`,
  `num_entries`, `reader_version`, `raw`, and `classes` or `classes_error` as JSON.
- `CORE_KEYS`, `KEYS` (core + `trigger`, `flashes`, `crthits`), `MIN_KEYS`
  (`run_info`, `reco_particles`), `DROPPED`.
- Helpers: `spine_api` (falls back to `spine.io.core.*` for v0.11.x), `make`,
  `update`, `build_event`, `exotic_batch`, `apply_libver` (a no-op for `earliest` or an
  empty value), `jsonify`.

### `decode_raw.py` (library + CLI)

- `decode(path, ...)`: returns `{case, writer, layout, num_events, products}`.
  `layout` is `V1`, `V2Root` or `V2Products`.
- `dumps(doc)`: diff-friendly JSON text, with one line per column and one line per
  object row.
- `DecodeError`, `KINDS = (objects, array, string, list, multi_list)`,
  `parse_fields` (V2 pool `fields` YAML list), `decode_v1`, `decode_v2`, and the
  `decode_v2_{array,objects,list,multi_list}` helpers.
- CLI: `decode_raw.py FILE [--case NAME] [--lite] [-o OUT]`. `--lite` only records
  `writer.lite = true`; it is not read from the file.

### `content.py` (library)

- `build_events()`: the 5 synthetic events, seeded with `random.Random(20261021)`.
- `exotic_events()`: 3 events of non-object products, seeded with `random.Random(7)`.
- `NUM_EVENTS = 5`, `BATCHES = ((0, 2), (2, 5))`.
- Enum value constants (shape, pid, current, scheme), `PDG`, `MASS`, `NAN`, `INF`.
- Builders: `reco_particle`, `truth_particle`, `reco_interaction`,
  `truth_interaction`.

### `make_member_list.py` (CLI)

- `render()`: builds the `GOLDEN_MEMBERS_<Class>(X)` X-macro text from the single
  `schema/spine-*.json`.
- `OUT = <repo>/tests/golden_members.inc`.
- `--check`: exits 1 if `OUT` is missing or stale and writes nothing.

## Invariants

- SPINE is imported only in `spine_worker.py`. There is one `spine_worker.py`
  subprocess per write, litify or read call. Each subprocess imports a single SPINE tag
  (`PYTHONPATH=<clone>/src`), so SPINE versions are never mixed in one process
  [make_fixtures.py:run_worker].
- Workers run with `PYTHONHASHSEED=0` (V1 `events` member order follows `set()`
  order) and `PYTHONDONTWRITEBYTECODE=1`. A nonzero worker exit raises
  `RuntimeError` [make_fixtures.py:run_worker].
- Cases with `derive` set (`v1.4.0_v2_fixed_only` is litified from `v1.4.0_v2`)
  are sorted after every other case, so in a full run their source is built first.
  With `--only`, the source `<out>/<case>.h5` must already exist [make_fixtures.py:main].
- Cross-checks are not Python `assert`s. Every failure is collected, and the run
  ends with `SystemExit("<n> cross-check(s) failed")` [make_fixtures.py:main].
  The checks:
  - `decode(v1)` equals `decode(v2)`, full and lite, after `layout_free` removes
    `case`, `layout` and `writer.format_version`.
  - lite equals full minus `LITE_SKIPPED`, and lite drops nothing else.
  - `fixed_only` equals full minus the pooled columns.
  - The decoded JSON equals what SPINE's `HDF5Reader` returns. If the writer tag's
    reader fails, the check notes it and retries with the v1.4.0 reader. It fails
    (`<name>: no SPINE reader could read it`) only if both fail. `--no-reader-check`
    skips this check, and with it the `--report` file.
  - A file with `writer.complete == False` must fail the strict open with
    `RuntimeError`; every other file must open.
  - A check runs only when both of its cases were built (`--only` may skip some).
- Content conventions (every case except the exotic cases and `products=min`)
  [make_fixtures.py:check_content]:
  - ids equal list positions.
  - `interaction_id`, `particle_ids` and `match_ids` are in range, and the
    references point back to each other.
  - `match_overlaps` has the same length as `match_ids` and is sorted descending.
- Coverage is checked only for the full cases (products `all`, 5 events, not
  derived): every `ENUM_COLUMNS` label, both bools, `nan`/`inf`/`-inf`, a
  non-ASCII string, empty and non-empty variable arrays, an empty event, and a
  reco interaction without particles [make_fixtures.py:check_content].
- The decoder validates the layout and raises `DecodeError` on any violation,
  including [decode_raw.py]:
  - V1 regions are a selection with no points, or one whole-row hyperslab block,
    and the per-event spans tile the dataset in order [region_rows, check_tiling].
  - V2 offsets start at 0, never decrease and are `N+1` long [offsets]. The last
    offset equals the data length [callers of offsets].
  - Pool spans are contiguous and in range, and every `variables/pool_i` has a
    `_var_offsets_i` helper [decode_v2_objects].
  - `format_version` is 1 or 2, and every product has `num_events` entries
    [decode].
  - Unreferenced V1 nodes, unknown product or pool kinds, a column both fixed and
    pooled, or a helper column inconsistent with its pool are rejected.
- Value encoding in the expected JSON [decode_raw.py:special, _c]:
  - Non-finite floats are the strings `"nan"`, `"inf"` and `"-inf"`; JSON is
    dumped with `allow_nan=False`.
  - The `{FALSE,TRUE}` enum maps to `bool`.
  - Products and row keys are sorted.
- `content.py` floats are mostly on dyadic grids (exact in float32 and float64).
  `build_events` asserts `len(events) == NUM_EVENTS` [content.py:_r, build_events].
- `make_member_list.py` (repository found from its own location) requires exactly
  one `schema/spine-*.json` and otherwise exits with `make_member_list: expected one
  schema/spine-*.json, found N` [make_member_list.py:render]. `tools/gen/generate.py`
  requires it only without `--schema`.

## Side Effects

- Writes `<out>/<case>.h5` and `<out>/<case>.expected.json` for the shipped cases.
  Unshipped exotic cases go to a `tempfile.mkdtemp(prefix="spine-fixtures-")`
  scratch directory, which is removed at the end, unless `--with-exotic` is given.
  Writes `--report` if given [make_fixtures.py:main].
- Runs `git clone -q --branch <tag> [--depth 1] <repo> <workdir>/spine_<tag>` for a
  missing clone. This needs the network when `--spine-repo` is a URL
  [make_fixtures.py:ensure_clone].
- Runs `nice -n 10 python spine_worker.py ...` subprocesses with a copied
  environment [make_fixtures.py:run_worker].
- All progress, ok/FAIL lines and the size summary go to stderr
  [make_fixtures.py:log].
- `spine_worker.py write` deletes and rewrites `--out`. Inside the worker,
  `apply_libver` monkeypatches `h5py.File.__init__` to default `libver`. Fields
  that an old SPINE lacks are dropped and reported on stderr.
- `decode_raw.py` opens files read-only and writes JSON to `-o`, or to stdout.
- `make_member_list.py` writes `tests/golden_members.inc`, or with `--check`
  prints `stale: ...` and exits 1.
- `content.py` has no I/O and does not touch the global RNG.

## Verification

- Run (ctest registers `make_member_list.py --check` as `spine_reader_golden_members`):

  ```bash
  source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh
  cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure
  python3 tools/fixtures/make_member_list.py --check
  ```

- Regenerate and re-check the fixtures (clones SPINE tags):
  `python3 tools/fixtures/make_fixtures.py`.
