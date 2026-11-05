# spine-reader golden test data

`tests/data/fixtures/<case>.h5` are small HDF5 files written by **SPINE's own `HDF5Writer`**;
`tests/data/fixtures/<case>.expected.json` is what the files contain, decoded by
`decode_raw.py`, which uses plain h5py and nothing from SPINE. `tests/data/fixtures/manifest.json`
records, for people, the SPINE tag and commit that wrote each case; no test reads it.

| file | role |
| --- | --- |
| `make_fixtures.py` | writes the cases, decodes them, runs every cross-check (failures are collected; the run exits non-zero at the end), writes the manifest, prints sizes |
| `spine_checkout.py` | resolves a SPINE tag to its commit (`git ls-remote`) and clones that commit; no h5py or numpy |
| `decode_raw.py` | independent decoder: V1 (region references), V2 (offsets and pools); also a CLI |
| `spine_worker.py` | the only code that imports SPINE: builds objects, runs the writer / `litify_hdf5` / `HDF5Reader` of one SPINE version |
| `content.py` | the synthetic events as plain Python (no SPINE import) |
| `make_member_list.py` | writes `tests/golden_members.inc`, the member names of every model class, from `schema/`; `--check` is a ctest |

## Two roles: the current set and the frozen cases

* The **current set**, the cases named `current_*`, is written by **one SPINE tag**: by default the
  model's SPINE tag ("v" + the `spine_version` of the committed `schema/spine-*.json`), or the tag
  given with `--tag`. When the model moves to a new SPINE tag, the whole current set is rewritten
  by that tag.
* The **frozen cases** keep the old release that wrote them and are never rewritten by a new tag:
  `v2root` (SPINE v0.16.3, commit `a9ad6c63`) and `v0.11.1_v1` (SPINE v0.11.1, commit `b78a6d64`,
  the oldest supported writer). `--frozen` writes them again, to reproduce how they were made; it
  checks out the recorded commits.
* The golden test (`tests/test_golden.cpp`) lists `tests/data/fixtures/*.expected.json`, reads each
  case's flags from the file itself, and takes its role from its name. The tests that open every
  case skip a case whose `writer.complete` is `false` (`current_incomplete`); `null`, as in
  `v0.11.1_v1`, counts as complete.

**What counts as a change in content.** The decoded document includes the writer's SPINE version,
which counts as content; only the case name is ignored. After a run with a tag whose content
changed, the whole current set is committed, rewritten by that tag. A tag that changes nothing
commits nothing. Frozen cases are never rewritten.

## Regenerating

```bash
source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh   # python3 + numpy + h5py + yaml + scipy
python3 tools/fixtures/make_fixtures.py                    # the current set, with the model's tag
python3 tools/fixtures/make_fixtures.py --frozen           # ... and the frozen cases, with their own releases
python3 tools/fixtures/make_fixtures.py --tag v1.2.3 --out /tmp/f   # another tag, outside the repository
python3 tools/fixtures/make_fixtures.py --tag v1.4.1 --sha <commit> --schema <dump of v1.4.1>
# offline / with a local SPINE repository and clones:
python3 tools/fixtures/make_fixtures.py --spine-repo /path/to/spine --workdir /path/to/clones
python3 tools/fixtures/make_fixtures.py --only current_v1 current_v2   # a partial run (see below)
python3 tools/fixtures/make_fixtures.py --manifest-only --tag v1.4.0 --sha <commit>   # record existing files
python3 tools/fixtures/decode_raw.py FILE.h5 --case NAME [--lite] -o OUT.json   # decode any SPINE file
```

* **Tags and commits.** `--tag vX.Y.Z` names the tag that writes the current set; `--sha` gives its
  commit, which is then not resolved again. Without `--sha`, `spine_checkout.resolve_tag()` runs
  `git ls-remote --tags` and takes the peeled `^{}` commit of an annotated tag, or the commit of a
  lightweight one. Each commit is cloned once into `--workdir`, in a folder `spine_<sha>` named
  after the commit (`git clone --branch <tag>`, then a check that `HEAD` is that commit), so a tag
  moved to another commit never reuses old code. Missing clones come from `--spine-repo` (default
  GitHub).
* **`--schema PATH`** (default the committed `schema/spine-*.json`) is read for `lite_dropped`, the
  fields lite mode drops, class by class. When the schema's `spine_version` is not the current
  set's tag, the run warns (`schema_tag_mismatch`).
* SPINE needs **no torch / larcv / MinkowskiEngine** for any of this. It is imported as
  `PYTHONPATH=<clone>/src`, one subprocess per SPINE commit, so never mix versions in one process.
* The files are made with the HDF5 file-format bound `--libver v110` (see Sizes). `--libver earliest`
  gives h5py's default format.
* `PYTHONHASHSEED=0` is set for the workers: V1 `events` member order follows Python `set` order.
  `.h5` bytes are still not reproducible (object-header timestamps), the `.expected.json` are.
* `--with-exotic` also writes `current_exotic_v{1,2}` to `--out` (not shipped, see below).
* `--artifacts-dir DIR` keeps what a run otherwise deletes: the unshipped cases' `.h5` and decoded
  JSON (`DIR/unshipped/`), SPINE's reader output (`DIR/reader/<case>.<tag>.json`) and the worker
  logs (`DIR/logs/<case>.<step>.log`).
* **Partial runs.** `--only CASE...` and `--no-reader-check` are for development. A run that uses
  either is marked `partial: true` in the manifest and on standard output. In a partial run a
  cross-check whose pair of cases was not written is skipped; in a full run it is a failure.
  `--only` names frozen cases only together with `--frozen`.
* **Standard output** is one JSON line: `spine_tag`, `spine_sha`, `cases`, `partial`, `failures`,
  `warnings`, `substitutions` and `manifest`. Everything else (progress, `ok`/`FAIL` lines, notes,
  sizes) goes to standard error.
* **Exit codes:** 0 success, warnings allowed; 1 a check failed (a cross-check, the content checks,
  or the comparison with SPINE's own reader); 2 a usage error; 3 a worker could not drive SPINE's
  API (an import, a writer parameter, or a `format_version` the writer cannot write); 4 any other
  error (a clone, a worker that failed otherwise while writing or litifying, an exception).

## The manifest

`manifest.json` holds, at the top, the run that wrote the current set (`spine_tag`, `spine_sha`,
`spine_version`, `partial`, `warnings`, `substitutions`), and under `cases` one entry per shipped
case: `role` (`current` or `frozen`), `tag`, `sha`, `layout`, `lite`, `derived_from`, and the file
names `h5` and `expected`. Warnings and substitutions have the shape `{kind, name, detail}`.

* It was created once by `--manifest-only`, which records the files already in `--out` without
  writing any: each case's flags come from its `.expected.json`, its tag and commit from `--tag` and
  `--sha` (current set) or from the frozen table.
* Every later run merges. `--manifest-base` (default the committed
  `tests/data/fixtures/manifest.json`, read only) gives the entries carried over: those of the
  frozen cases, and in a partial run also those of the current cases the run did not write. The
  entries of the cases the run wrote replace theirs. The result goes to `--manifest` (default
  `<out>/manifest.json`), so a run into another `--out` keeps the frozen entries too.

## Enum values come from the writing tag (current set only)

The worker writes the current set with `--configure-enums`: it gives `content.configure()` the
members of `ParticleShape`, `ParticlePID`, `NuCurrentType` and `NuInteractionScheme` at the writing
tag, from `spine.constants.enums`. A label the content writes that the tag lacks is written as
`UNKNOWN` (-1 in all four, from v1.2.1 on) and listed as a substitution: SPINE v1.2.1 and v1.2.2, for
instance, have no `GIBUU`, `NUWRO` or `NEUT` scheme. After substitution:

* a value a current case stores that the file's enum does not name is a **failure**;
* a label the file defines that no row stores is a **warning** (`unused_label`), as is an enum
  column stored as a plain integer (`not_an_enum`).

The frozen cases keep the built-in values (SPINE v1.4.0's), so `--frozen` reproduces `v2root`, which
stores `interaction_scheme` values 2 and 3 under a three-label enum. Their enums are not checked.

## Cases

All values are synthetic but follow SPINE's conventions: ids are list positions inside the event, particles point
to their interaction (`interaction_id`), interactions to their particles (`particle_ids`), reco `match_ids` point
to truth ids of the same event and back, overlaps best first. Every product that CAFMakers read is a SPINE object
product (`RunInfo`, `ImageMeta3D`, `Trigger`, `Flash`, `CRTHit`, `Reco/TruthParticle`, `Reco/TruthInteraction`);
`index` is a scalar `array` product. Products with `source_file_*` provenance are not written (no `file_index` in
the batch): nothing in the CAFMakers reads them.

The sizes are those of the current set written by v1.4.0.

| case | writer | layout | products | events | bytes at v1.4.0 (.h5 / .json) | exercises |
| --- | --- | --- | --- | --- | --- | --- |
| `current_v1` | current tag, `format_version=1` | V1 | all 10 | 5 | 171,590 / 97,804 | region references + VLEN compound members, SEL_NONE regions, enums, bools, strings, `info@complete` |
| `current_v2` | current tag, `format_version=2` | V2Products | all 10 | 5 | 209,665 / 97,812 | `/products`, `event_offsets`, `fixed` + `variables/pool_i` + `_var_offsets_i`; **same objects as `current_v1`** |
| `current_v1_lite` | current tag, `lite=True` | V1 | all 10 | 5 | 162,000 / 91,862 | columns missing (`index`, `orig_index`, `index_adapt`, `index_g4`) while derived `size`... is still stored |
| `current_v2_lite` | current tag, `lite=True` | V2Products | all 10 | 5 | 207,105 / 91,870 | same, and V2 pool **renumbering** (a pool changes its dtype between full and lite) |
| `current_v2_fixed_only` | `litify_hdf5(current_v2, mode="fixed_only")` | V2Products | default 6 + `index` | 5 | 104,266 / 71,616 | no pools at all: empty `variables` groups, no `_var_offsets_*`, no strings (`units`, `topology`), no `match_ids`...; `info@litified` |
| `current_incomplete` | current tag, V1, never finalized | V1 | `run_info`, `reco_particles` | 2 | 52,294 / 15,990 | `info@complete = False` (the state SPINE leaves while writing) |
| `v2root` (frozen) | **v0.16.3**, `format_version=2` | V2Root | core 6 | 3 | 167,792 / 66,551 | V2 with the product groups at the file root (no `/products`), `info@format_version=2` |
| `v0.11.1_v1` (frozen) | **v0.11.1** | V1 | core 6 | 3 | 135,487 / 62,916 | oldest supported release: no `format_version`/`complete`/`spine_version`/`format` attributes, fewer fields (see Surprises), different enum labels and one different dtype |

(10 products = `index`, `run_info`, `meta`, `trigger`, `reco_particles`, `reco_interactions`, `truth_particles`,
`truth_interactions`, `flashes`, `crthits`. "core 6" = the six `run_info`, `meta`, `reco_*`, `truth_*` products
plus `index`.) Total shipped at v1.4.0: 1,806,620 bytes, plus the manifest.

`v2root` and `v0.11.1_v1` contain the first 3 events of the 5 below (same content, fields the old SPINE does not
have are dropped by `spine_worker.py`: it reports them on stderr).

Events (writer called twice: events 0-1, then 2-4, so appends and absolute offsets are exercised):

| ev | reco interactions : particles | truth interactions : particles | flashes | crthits | notes |
| --- | --- | --- | --- | --- | --- |
| 0 | 2 : 3 + 2 | 2 : 3 + 2 | 2 | 1 | shapes TRACK/SHOWER/MICHEL, µ/p/γ/e, flash matches, `"µIoni"`, `"π→µν"`, NaN pe in a flash, an empty `pe_per_ch` |
| 1 | 1 : 2 | 2 : 2 + 1 | 1 | 0 | `LARCV_UNKNOWN` and `UNKNOWN` shapes, pid `UNKNOWN`, `units="px"`, schemes GIBUU/NUWRO |
| 2 | none | none | 0 | 2 | **no particles and no interactions at all**; `+inf`, `-inf` in a CRT hit; `"µ-bottom"`; empty `feb_id` |
| 3 | 2 : 1 + 2 | 1 : 3 | 3 | 0 | DELTA/LOWE/GHOST shapes, KAON, `length=+inf`, `start_dedx=-inf`, scheme NEUT |
| 4 | 2 : 1 + **0** | 1 : 1 | 0 | 1 | an interaction without particles (all its arrays empty, `vertex` NaN), scheme UNKNOWN |

`make_fixtures.py` checks that the full cases contain both bool values, `nan`/`inf`/`-inf`, non-ASCII strings,
empty and non-empty variable arrays, an event with no object at all, an interaction without particles, and that
ids, `interaction_id`, `particle_ids` and `match_ids` obey the conventions above; the enum labels follow the rule
of the previous section. Floats are mostly on a dyadic grid (exact in float32 and float64, short in JSON);
`match_overlaps`, `pid_scores`, some energies and every *derived* float (`ke`, `p`, `momentum`, sums...) are
deliberately not.

### Not shipped, but generated and checked: `current_exotic_v{1,2}`

Non-object products (a `string`, a 2-D `array`, a `list`, a `multi_list` product, with empty events) written by
the current tag in both layouts. They exist so that `decode_raw.py` is tested on every product kind (V1
`list`/`multi_list` groups of region references, V2 `string`/`list`/`multi_list`); the CAF products never use
them, and they cost 190 kB at v1.4.0, so they are written to a scratch directory unless `--with-exotic` is given.

## Expected JSON format

```json
{ "case": "current_v1",
  "writer": {"spine_version": "1.4.0", "format_version": 1, "lite": false, "complete": true},
  "layout": "V1",                         // "V1" | "V2Root" | "V2Products"
  "num_events": 5,
  "products": { "<key>": { "kind": "objects", "class_name": "RecoParticle", "scalar": false,
      "columns": { "<name>": {"type": "float32", "count": 3, "variable": false, "enum": null} },
      "events": [ [ {"<column>": <value>, ...}, ... ], [], ... ] } } }
```

* `writer.lite` is writer knowledge (not stored in the file); `writer.complete` is **an addition to the brief**:
  `info@complete` as stored (`null` when the attribute is absent, as in v0.11.1 files).
* `kind` is `objects | array | string | list | multi_list`; `class_name` is `null` when absent; `scalar` is the
  stored attribute (one logical value per event, e.g. `run_info`, `index`).
* **Columns** are all stored columns (V2's `_var_offsets_<i>` helpers hidden), sorted by name. `type` is
  `bool | int8..int64 | uint8..uint64 | float32 | float64 | string`. SPINE's `{FALSE,TRUE}` enum is `bool` (with
  `enum: null`); any other HDF5 enum reports its base integer type and `enum` maps label to value. `count` is 1
  for scalars (and strings), N for fixed arrays, 0 for variable-length arrays (`variable: true`). V1 VLEN
  columns and V2 pool columns are described identically (a V2 string pool is `string`, count 1, not variable).
* **Values**: JSON numbers; `"nan"`, `"inf"`, `"-inf"` (strings) for the non-finite floats; enums are their
  integer; bools are `true`/`false`; arrays are lists; strings are UTF-8 text. float32 values are written as the
  exact double they widen to, so `(float)expected == stored` and `(double)stored == expected` both hold.
* `objects`: `events[e]` is the list of row dicts (`[]` for an empty event; a scalar product has one row).
* `array`, `string`: one column named `""`; `events[e]` is the list of row values (a scalar for a 1-wide column, a
  list for a 2-D array; a one-element list `[str]` for a string product).
* `list` (same-width arrays per event): one column `""` (the element type); `events[e]` is the list of elements,
  each the list of its row values.
* `multi_list` (a fixed number K of arrays of different widths per event): columns `element_0` ... `element_{K-1}`;
  `events[e]` is the list of K arrays, each the list of its row values.

### How `decode_raw.py` reads

* **V1**: `events` is a compound of `H5T_STD_REF_DSETREG` members, one per product; each is resolved with
  `h5r.get_region` and the `h5s` selection API (an empty event is `H5S_SEL_NONE`; anything else must be one block
  of whole rows). It checks that the per-event ranges tile the dataset in order. A product is `objects` (compound
  dataset), `array`/`string` (plain dataset) or a group: `index` 1-D -> `list` (`index` refs into `elements`),
  `index` 2-D -> `multi_list` (row of refs into `element_j`).
* **V2**: `event_offsets` (checked: starts at 0, non-decreasing, `N+1` long, ends at the data length) slice
  `values` / `fixed`; `variables/pool_i` (`@kind`, `@fields` = YAML list) are found through the `_var_offsets_i`
  column of each `fixed` row, and the pool spans are checked to be contiguous and in range. Products live in
  `/products` if it exists, else at the root (v0.16.x).

## Cross-checks (run by `make_fixtures.py`; failures make it exit 1)

They key on role names, so they run for every tag.

1. `decode(current_v1) == decode(current_v2)` and `decode(current_v1_lite) == decode(current_v2_lite)`: **no
   difference at all** except `case`, `layout` and `writer.format_version`. (At v1.4.0 no legitimate difference
   was found: the column types, enums, empty-event rows and every value are identical.)
2. `current_v1_lite` = `current_v1` minus, in each object product, exactly the fields its class's
   `lite_dropped` lists in the schema (at v1.4.0: `index`, `orig_index` of the reco and truth classes, plus
   `index_adapt`, `index_g4` of the truth classes). A lite file that drops more, or less, fails.
3. `current_v2_fixed_only` = `current_v2` restricted to its 7 products and with every pooled column removed.
4. `expected == SPINE's own HDF5Reader` of the writing tag (`build_classes=False`, i.e. the stored values) for
   every product, column and event of every case: at v1.4.0, 2745 / 2745 / 2637 / 2637 / 1999 / 393 values for
   the current set and 1619 / 1537 for `v2root` / `v0.11.1_v1`. The reader adds only its administrative entries
   (`index`, `file_index`, `file_entry_index`, `source_file_*`). Only `v0.11.1_v1` falls back to another reader,
   the current tag's (see below); for every other case a reader that fails, or disagrees, is a check failure.
5. The default `HDF5Reader` accepts every complete file and raises `RuntimeError("... is marked incomplete")` on
   `current_incomplete.h5`.
6. Each written file has the `format_version` asked for, and `info@complete` is false only for
   `current_incomplete`.
7. Content conventions and coverage (see Cases), and the enum rule of the current set.

A cross-check whose pair of cases is missing is a failure, unless the run is partial.

## Surprises and findings about the real writers

These were found with the current set written by v1.4.0 and the two frozen releases.

* **Sizes.** SPINE's layouts are one chunk per dataset (h5py auto-chunking: 6-21 KB for each `fixed`, 4 KB for each int32/float32
  pool, 8 KB for `index/values` and `events`), so the size is set by the number of datasets, not by the events: a 5-event V2
  file with 10 products is 306 kB with h5py's default file format, 210 kB with `libver=v110` (V1: 195 -> 172 kB).
  V2 is *larger* than V1 for small files. `--libver v110` changes only the HDF5 file-format bound (compact groups,
  extensible-array chunk indexes; needs libhdf5 >= 1.10); no SPINE logic, dtype, attribute or chunk shape differs.
  Even so, at v1.4.0, `current_v2` (209,665 B) and `current_v2_lite` (207,105 B) stay just above 200 kB.
* **Empty events.** V1: the region reference has `H5S_SEL_NONE` (0 points), not a zero-length hyperslab. V2: equal
  consecutive offsets (`event_offsets = [0 5 7 7 10 11]`). Scalar products (`run_info`, `meta`, `index`) are never empty.
  Empty lists need `ObjectList(objs, default=Cls())` in SPINE or the writer cannot infer the class.
* **Type choices follow Python values of the first object**: derived numpy scalars keep their width, so
  `truth_particles.ke` and `.mass` are **float32** (the class says float64), `reco_particles.momentum` and
  `truth_particles.reco_momentum` are **float64[3]** while the other `*momentum` are float32[3]; everything else is
  int64 / float64, bools are an int8 enum `{FALSE,TRUE}`, `crthits.feb_id` is a `uint8` VLEN, strings are VLEN UTF-8
  (V1) or UTF-8 bytes in a `uint8` pool (V2, offsets in bytes, tested with 2- and 3-byte characters).
* **Enum labels** are upper case in all three writers. v1.4.0: `chi2_pid` is an enum (as `pid`); v0.16.3 and v0.11.1:
  plain int64. `interaction_scheme`: 6 labels in v1.4.0 (UNKNOWN, LARSOFT, GENIE, GIBUU, NUWRO, NEUT), 3 in
  v0.16.3 and v0.11.1. Derived `pdg_code` of a reco particle with `pid = UNKNOWN` is `0` in v1.4.0, `-1` before.
* **Field sets** (stored columns, same content fed to every writer): v1.4.0 has RecoParticle 55, TruthParticle 98,
  RecoInteraction 40, TruthInteraction 79. v0.16.3 lacks only `depositions_q_sum` of the reco objects. v0.11.1 (52 / 94 /
  35 / 73) also lacks `best_match_id`, `best_match_overlap` (all four classes), `dir`, `flash_hypothesis_ids`
  (interactions), `reco_dir`, `time` (truth interaction), `first_step_t`, `last_step_t` (truth particle). The only
  dtype difference of v0.11.1 is `particle_ids` (int64 instead of int32 in both interaction classes).
* **Pool numbering is not stable**: in `truth_particles` / `truth_interactions` `pool_0` is the int32 pool in the
  full file and the *string* pool in the lite file (the first variable fields, `index_adapt`/`index_g4`, are gone).
* **lite** only drops the per-voxel index arrays; derived values such as `size`, `depositions_sum` are still stored
  as computed from the full objects. **fixed_only** drops *every* VLEN/string column, derived ones included
  (`topology`, `primary_particle_ids`, `module_ids`, `units`), keeps an empty `variables` group, adds
  `info@litified=True`, `litify_mode`, `litify_keys` and keeps only the requested keys plus `index`.
* **v2root** (v0.16.3): `format_version = 2` but product groups at the root and no `product_metadata`; the
  reader of that release reads it fine.
* **v0.11.1**: `info` carries only `cfg` and `version` (a group; `format_version`, `complete`, `spine_version` and
  `format` appear later). The v0.11.1 `HDF5Reader` **cannot read its own file** (`AttributeError: module
  'spine.data' has no attribute 'ImageMeta3D'`: the writer stores `class_name="ImageMeta3D"`, the reader resolves
  it in `spine.data`, which only exports `Meta`); the current tag's reader (v1.4.0 so far) reads it, and is used
  for the cross-check of that case only.
* **Incomplete files**: `with HDF5Writer(...)` calls `finalize()`; skipping it leaves `complete=False`. SPINE's
  reader refuses such a file unless `ignore_incomplete=True`. A genuinely interrupted V2 file (products longer than
  `events`) cannot be made reproducibly with the real writer and was not hand-crafted.
* **Rebuilding classes is lossy** (`HDF5Reader(build_classes=True)` re-derives instead of returning the stored
  derived values; nested objects `particles`, `depositions`, `points` are never stored). Against the stored values:
  `depositions_sum`, `depositions_q_sum`, `depositions_adapt_sum`, `depositions_g4_sum` become 0.0;
  interactions lose `num_primary_particles`, `primary_particle_ids`, `particle_counts`, `primary_particle_counts`,
  `topology` (stored `"1mu1p"` -> `""`); `truth_particles.ke`/`mass` differ at float32 rounding level (stored float32,
  recomputed float64); lite files also lose `size`, `size_adapt`, `size_g4`; fixed-only files also lose
  `best_match_id`, `best_match_overlap`, `num_particles`, `num_fragments`, `size`. A C++ reader should return the
  stored columns.
