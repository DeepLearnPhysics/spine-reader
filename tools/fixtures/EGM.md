# tools/fixtures

## Overview

Generates and checks the golden HDF5 test data in `tests/data/fixtures/`. Small
files are written by SPINE's own `HDF5Writer` (one `spine_worker.py` subprocess per
write, litify or read call; each subprocess imports a single SPINE commit).
They are then decoded by an independent h5py-only decoder into
`<case>.expected.json` and cross-checked against each other and against SPINE's
`HDF5Reader`. The cases have two roles: the current set (`current_*`), written by
one tag, by default the model's SPINE tag; and the frozen cases, written by old
releases and never rewritten by a new tag. `manifest.json` records each case's tag
and commit. The directory also generates `tests/golden_members.inc`, the per-class
member list from `schema/`. The hand-written [README.md](./README.md) is the
detailed reference for the cases, the manifest, the expected JSON format and the
findings about the real writers. This file is the contract summary; it does not
replace that README.

| file | role |
| --- | --- |
| `make_fixtures.py` | driver: resolves and clones SPINE commits, runs the workers, decodes, cross-checks, writes the manifest, prints sizes |
| `spine_checkout.py` | `resolve_tag()` and `ensure_clone()`; imports neither h5py nor numpy |
| `spine_worker.py` | the only module that imports SPINE; subcommands `write`, `litify`, `read` |
| `decode_raw.py` | independent V1 / V2 decoder (h5py + numpy, no SPINE); library and CLI |
| `content.py` | the synthetic events as plain Python dicts (no SPINE, no I/O) |
| `make_member_list.py` | writes or checks `tests/golden_members.inc` from `schema/spine-*.json` |

## Interface

### `make_fixtures.py` (CLI)

- `CURRENT_CASES`: `current_v1`, `current_v2`, `current_v1_lite`, `current_v2_lite`,
  `current_v2_fixed_only` (`derive="current_v2"`), `current_incomplete`, and the
  unshipped `current_exotic_v{1,2}` with `ship=False`. They take their tag from `--tag`.
- `FROZEN_CASES`: `v2root` (v0.16.3, `a9ad6c63ce5532429c26b3374ec73c7ecc2f1cea`) and
  `v0.11.1_v1` (v0.11.1, `b78a6d64f60ce0be5a24611541df19033620b95a`,
  `reader_fallback=True`), with their tag and commit. `CASES` is both.
- Optional case keys: fv, lite, products, n_events, derive, incomplete, exotic, ship
  (defaults to True), and for frozen cases tag, sha, reader_fallback.
- `PAIRS` (the cross-checks by role name), `OBJECT_LISTS`, `ENUM_COLUMNS`,
  `DEFAULT_OUT` (`<repo>/tests/data/fixtures`), `DEFAULT_SPINE_REPO`
  (`https://github.com/DeepLearnPhysics/spine`), `MANIFEST` (`manifest.json`), and the
  exit codes `EXIT_OK` 0, `EXIT_CHECK` 1, `EXIT_USAGE` 2, `EXIT_TOOLING` 3,
  `EXIT_ERROR` 4.
- Helpers: `role`, `committed_schema`, `load_schema`, `lite_dropped`, `run_worker`,
  `substitutions_of`, `diff_docs`, `layout_free`, `restrict`, `lite_problems`,
  `check_content`, `check_enums`, `reader_check`, `manifest_entry`, `read_manifest`,
  `write_manifest`, `merge_manifest`, `parse_args`, `Run`, `manifest_only`,
  `write_cases`, `main`. Errors: `UsageError`, `ToolError`, `WorkerError` (carries
  the worker's exit `code`).
- Flags:
  - `--out` (default `DEFAULT_OUT`)
  - `--tag vX.Y.Z` (default "v" + the `spine_version` of the schema in use)
  - `--sha SHA` (40 hex characters; given, the tag is not resolved again)
  - `--schema PATH` (default the single committed `schema/spine-*.json`; read for
    `lite_dropped`)
  - `--frozen` (also write the frozen cases)
  - `--spine-repo` (default `$SPINE_REPO`, else GitHub)
  - `--workdir` (default `$SPINE_FIXTURE_WORKDIR`, else `<tmp>/spine-reader-fixtures`;
    holds the `spine_<sha>` clones)
  - `--libver` (default `v110`)
  - `--only CASE...` and `--no-reader-check` (partial runs)
  - `--with-exotic`
  - `--report FILE`
  - `--artifacts-dir DIR`
  - `--manifest PATH` (default `<out>/manifest.json`)
  - `--manifest-base PATH` (default the committed `tests/data/fixtures/manifest.json`,
    read only)
  - `--manifest-only`
- Standard output: one JSON line `{spine_tag, spine_sha, cases, partial, failures,
  warnings, substitutions, manifest}` (`--manifest-only`: `{manifest, cases,
  partial}`).

### `spine_checkout.py` (library)

- `resolve_tag(repo, tag) -> sha`: `git ls-remote --tags <repo> refs/tags/<tag>
  refs/tags/<tag>^{}`; the peeled `^{}` commit when listed, else the plain one. Raises
  `LsRemoteError` when `ls-remote` fails and `TagNotFound` when the tag is absent.
- `ensure_clone(tag, sha, repo, workdir) -> <clone>/src`: clones into
  `<workdir>/spine_<sha>` and checks that `HEAD` is `sha`; raises `CheckoutError`.
- `SHA_RE`, and the errors `CheckoutError`, `LsRemoteError`, `TagNotFound` (both
  subclasses of `CheckoutError`).

### `spine_worker.py` (CLI, run only by `make_fixtures.run_worker`)

- `write`: `--out` (required), `--format-version` (default 1), `--lite`,
  `--incomplete` (skip `finalize()`), `--n-events`, `--exotic`,
  `--products {all,core,min}`, `--libver`, `--configure-enums`. `--exotic` ignores
  `--products` and `--n-events`. With `--configure-enums` it passes the tag's checked
  enums to `content.configure()` and prints each substitution on stderr as
  `SUBSTITUTION <json>`.
- `litify`: `--src`, `--out` (required), `--libver`. Runs
  `litify_hdf5(mode="fixed_only", keys=DEFAULT_LITE_KEYS)`.
- `read`: `--file`, `--out` (required). Dumps `spine_version`, `strict_open`,
  `num_entries`, `reader_version`, `raw`, and `classes` or `classes_error` as JSON.
- Exit codes: 0; 3 (`SpineApiError`: an import fails, the writer lacks a parameter it
  needs, or it cannot write the asked `format_version`); 1 any other error.
- `CORE_KEYS`, `KEYS` (core + `trigger`, `flashes`, `crthits`), `MIN_KEYS`
  (`run_info`, `reco_particles`), `ENUMS`, `DROPPED`.
- Helpers: `spine_api` (falls back to `spine.io.core.*` for v0.11.x), `tag_enums`,
  `make`, `update`, `build_event`, `exotic_batch`, `apply_libver` (a no-op for
  `earliest` or an empty value), `jsonify`.

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
- `configure(enums=None)`: takes `{enum name: {label: value}}` for `ParticleShape`,
  `ParticlePID`, `NuCurrentType` and `NuInteractionScheme` (`LABELS`), rebinds the
  label constants and rebuilds `PDG` and `MASS`; returns and stores the substitutions
  in `SUBSTITUTIONS`. `configure(None)` restores the built-in values.
- `NUM_EVENTS = 5`, `BATCHES = ((0, 2), (2, 5))`.
- Enum value constants (shape, pid, current, scheme; SPINE v1.4.0's by default),
  `LABELS`, `PDG`, `MASS`, `NAN`, `INF`, `SUBSTITUTIONS`.
- Builders: `reco_particle` (its `chi2_pid` defaults to `UNKNOWN` at call time),
  `truth_particle`, `reco_interaction`, `truth_interaction`.

### `make_member_list.py` (CLI)

- `render()`: builds the `GOLDEN_MEMBERS_<Class>(X)` X-macro text from the single
  `schema/spine-*.json`.
- `OUT = <repo>/tests/golden_members.inc`.
- `--check`: exits 1 if `OUT` is missing or stale and writes nothing.

## Invariants

- SPINE is imported only in `spine_worker.py`. There is one `spine_worker.py`
  subprocess per write, litify or read call. Each subprocess imports a single SPINE
  commit (`PYTHONPATH=<clone>/src`), so SPINE versions are never mixed in one process
  [make_fixtures.py:run_worker].
- A clone is named after its commit (`spine_<sha>`), and `ensure_clone` refuses a
  checkout whose `HEAD` is another commit, so a moved tag never reuses old code
  [spine_checkout.py:ensure_clone]. Frozen cases are checked out at their recorded
  commits.
- The role of a case is its name: `current_*` or frozen [make_fixtures.py:role]. The
  current set is written by `--tag`; frozen cases only with `--frozen`
  [make_fixtures.py:write_cases].
- Workers run with `PYTHONHASHSEED=0` (V1 `events` member order follows `set()`
  order) and `PYTHONDONTWRITEBYTECODE=1`. A nonzero worker exit raises `WorkerError`
  with the worker's exit code [make_fixtures.py:run_worker].
- Exit codes of `make_fixtures.py` [make_fixtures.py:main]: 0 success, warnings
  allowed; 1 a check failed; 2 a usage error (`UsageError`, or argparse); 3 a `write`
  or `litify` worker exited 3; 4 any other error (another worker exit, a
  `CheckoutError`, a missing h5py, an unexpected exception). A failed `read` worker
  is a check failure (1), not a tooling error.
- Cases with `derive` set (`current_v2_fixed_only` is litified from `current_v2`)
  are sorted after every other case, so in a full run their source is built first.
  With `--only`, the source `<out>/<case>.h5` must already exist [make_fixtures.py:write_cases].
- Cross-checks are not Python `assert`s. Every failure is collected, and the run
  ends with exit code 1 [make_fixtures.py:write_cases]. The checks:
  - Each written file has the asked `format_version`, and `info@complete` is false
    exactly for the case written incomplete.
  - `decode(v1)` equals `decode(v2)`, full and lite, after `layout_free` removes
    `case`, `layout` and `writer.format_version`.
  - lite equals full minus, per object product, exactly its class's `lite_dropped`
    fields in the schema [lite_problems].
  - `fixed_only` equals full minus the pooled columns.
  - The decoded JSON equals what SPINE's `HDF5Reader` of the writing tag returns. Only
    a case with `reader_fallback` (`v0.11.1_v1`) retries with the current tag's reader;
    for every other case a failing reader is a check failure. `--no-reader-check`
    skips this check, and with it the `--report` file.
  - A file with `writer.complete == False` must fail the strict open with
    `RuntimeError`; every other file must open.
  - A check whose pair of cases is missing is a failure, unless the run is partial
    (`--only` or `--no-reader-check`), where it is skipped.
- Content conventions (every case except the exotic cases and `products=min`)
  [make_fixtures.py:check_content]:
  - ids equal list positions.
  - `interaction_id`, `particle_ids` and `match_ids` are in range, and the
    references point back to each other.
  - `match_overlaps` has the same length as `match_ids` and is sorted descending.
- Coverage is checked only for the full cases (products `all`, 5 events, not
  derived): both bools, `nan`/`inf`/`-inf`, a non-ASCII string, empty and non-empty
  variable arrays, an empty event, and a reco interaction without particles
  [make_fixtures.py:check_content].
- Enums, current set only [make_fixtures.py:check_enums]: a stored value of an
  `ENUM_COLUMNS` column that the file's enum does not name is a failure; in the full
  cases a label no row stores (`unused_label`) and an enum column stored as a plain
  integer (`not_an_enum`) are warnings. The current set is written with the writing
  tag's enum values; a label the tag lacks becomes `UNKNOWN` and is a substitution
  [spine_worker.py:cmd_write, content.py:configure]. Frozen cases use the built-in
  values and are not enum-checked.
- Manifest [make_fixtures.py:merge_manifest]: the frozen entries of `--manifest-base`
  are carried over, and in a partial run its current entries too; the entries of the
  cases the run wrote replace theirs. Only shipped cases have entries. The top-level
  keys describe the run when it wrote a current case. `h5` and `expected` are file
  names. Warnings and substitutions are `{kind, name, detail}`.
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
  Without `configure()`, or after `configure(None)`, the content is that of SPINE
  v1.4.0's enum values.
- `make_member_list.py` (repository found from its own location) requires exactly
  one `schema/spine-*.json` and otherwise exits with `make_member_list: expected one
  schema/spine-*.json, found N` [make_member_list.py:render]. `tools/gen/generate.py`
  requires it only without `--schema`.

## Side Effects

- Writes `<out>/<case>.h5` and `<out>/<case>.expected.json` for the shipped cases,
  and the merged manifest to `--manifest`. Unshipped exotic cases go to a
  `tempfile.mkdtemp(prefix="spine-fixtures-")` scratch directory, which is removed at
  the end, unless `--with-exotic` (then `--out`) or `--artifacts-dir` (then
  `<dir>/unshipped/`, kept) is given. `--artifacts-dir` also keeps the reader output
  (`<dir>/reader/`) and the worker logs (`<dir>/logs/`). Writes `--report` if given
  [make_fixtures.py:write_cases].
- `--manifest-only` writes only the manifest [make_fixtures.py:manifest_only].
- Runs `git ls-remote --tags` to resolve a tag without `--sha`
  [spine_checkout.py:resolve_tag], and `git clone -q --branch <tag> [--depth 1] <repo>
  <workdir>/spine_<sha>` (through a `.partial-<pid>` folder, then renamed) for a
  missing clone. These need the network when `--spine-repo` is a URL
  [spine_checkout.py:ensure_clone].
- Runs `nice -n 10 python spine_worker.py ...` subprocesses with a copied
  environment [make_fixtures.py:run_worker].
- Progress, ok/FAIL/skip/note lines, warnings and the size summary go to stderr;
  the one-line JSON summary goes to stdout [make_fixtures.py:log, write_cases].
- `spine_worker.py write` deletes and rewrites `--out`. Inside the worker,
  `apply_libver` monkeypatches `h5py.File.__init__` to default `libver`. Fields
  that an old SPINE lacks are dropped and reported on stderr; so are the enum
  substitutions.
- `decode_raw.py` opens files read-only and writes JSON to `-o`, or to stdout.
- `make_member_list.py` writes `tests/golden_members.inc`, or with `--check`
  prints `stale: ...` and exits 1.
- `content.py` has no I/O and does not touch the global RNG; `configure()` rebinds
  its module-level constants.

## Verification

- Run (ctest registers `make_member_list.py --check` as `spine_reader_golden_members`):

  ```bash
  source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh
  cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure
  python3 tools/fixtures/make_member_list.py --check
  ```

- Regenerate and re-check the fixtures (clones SPINE commits):
  `python3 tools/fixtures/make_fixtures.py [--frozen]`. With the model's tag, every
  `.expected.json` and the manifest come out byte-identical (the `.h5` bytes carry
  timestamps).
