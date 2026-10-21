# spine-reader golden test data

`tests/data/fixtures/<case>.h5` are small HDF5 files written by **SPINE's own `HDF5Writer`**;
`tests/data/fixtures/<case>.expected.json` is what the files contain, decoded by
`decode_raw.py`, which uses plain h5py and nothing from SPINE.

| file | role |
| --- | --- |
| `make_fixtures.py` | writes the cases, decodes them, runs every cross-check (asserted), prints sizes |
| `decode_raw.py` | independent decoder: V1 (region references), V2 (offsets and pools); also a CLI |
| `spine_worker.py` | the only code that imports SPINE: builds objects, runs the writer / `litify_hdf5` / `HDF5Reader` of one SPINE version |
| `content.py` | the synthetic events as plain Python (no SPINE import) |
| `make_member_list.py` | writes `tests/golden_members.inc`, the member names of every model class, from `schema/`; `--check` is a ctest |

## Regenerating

```bash
source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh   # python3 + numpy + h5py + yaml + scipy
python3 tools/fixtures/make_fixtures.py                                       # clones the SPINE tags it needs
# offline / with a local SPINE repository and checkouts:
python3 tools/fixtures/make_fixtures.py --spine-repo /path/to/spine --workdir /path/to/clones
python3 tools/fixtures/make_fixtures.py --only v1.4.0_v1 v1.4.0_v2           # a subset (some cross-checks skip)
python3 tools/fixtures/decode_raw.py FILE.h5 --case NAME [--lite] -o OUT.json # decode any SPINE file
```

* SPINE needs **no torch / larcv / MinkowskiEngine** for any of this. It is imported as
  `PYTHONPATH=<clone>/src`, one subprocess per SPINE version, so never mix versions in one process.
* SPINE versions needed: **v1.4.0** (a, b, c, f, g), **v0.16.3** (d), **v0.11.1** (e).
  `--workdir` holds the clones `spine_<tag>` (`git clone --branch <tag>`); missing ones are cloned from
  `--spine-repo` (default GitHub). The v1.4.0 reader also reads the v0.11.1 file (see Surprises).
* The files are made with the HDF5 file-format bound `--libver v110` (see Sizes). `--libver earliest`
  gives h5py's default format.
* `PYTHONHASHSEED=0` is set for the workers: V1 `events` member order follows Python `set` order.
  `.h5` bytes are still not reproducible (object-header timestamps), the `.expected.json` are.
* `--with-exotic` also writes `v1.4.0_exotic_v{1,2}` (not shipped, see below).

## Cases

All values are synthetic but follow SPINE's conventions: ids are list positions inside the event, particles point
to their interaction (`interaction_id`), interactions to their particles (`particle_ids`), reco `match_ids` point
to truth ids of the same event and back, overlaps best first. Every product that CAFMakers read is a SPINE object
product (`RunInfo`, `ImageMeta3D`, `Trigger`, `Flash`, `CRTHit`, `Reco/TruthParticle`, `Reco/TruthInteraction`);
`index` is a scalar `array` product. Products with `source_file_*` provenance are not written (no `file_index` in
the batch): nothing in the CAFMakers reads them.

| case | writer | layout | products | events | bytes (.h5 / .json) | exercises |
| --- | --- | --- | --- | --- | --- | --- |
| `v1.4.0_v1` | v1.4.0, `format_version=1` | V1 | all 10 | 5 | 171,590 / 97,803 | region references + VLEN compound members, SEL_NONE regions, enums, bools, strings, `info@complete` |
| `v1.4.0_v2` | v1.4.0, `format_version=2` | V2Products | all 10 | 5 | 209,665 / 97,811 | `/products`, `event_offsets`, `fixed` + `variables/pool_i` + `_var_offsets_i`; **same objects as `v1.4.0_v1`** |
| `v1.4.0_v1_lite` | v1.4.0, `lite=True` | V1 | all 10 | 5 | 162,000 / 91,861 | columns missing (`index`, `orig_index`, `index_adapt`, `index_g4`) while derived `size`... is still stored |
| `v1.4.0_v2_lite` | v1.4.0, `lite=True` | V2Products | all 10 | 5 | 207,105 / 91,869 | same, and V2 pool **renumbering** (a pool changes its dtype between full and lite) |
| `v2root` | **v0.16.3**, `format_version=2` | V2Root | core 6 | 3 | 167,792 / 66,551 | V2 with the product groups at the file root (no `/products`), `info@format_version=2` |
| `v0.11.1_v1` | **v0.11.1** | V1 | core 6 | 3 | 135,487 / 62,916 | oldest supported release: no `format_version`/`complete`/`spine_version`/`format` attributes, fewer fields (see Surprises), different enum labels and one different dtype |
| `v1.4.0_v2_fixed_only` | `litify_hdf5(v1.4.0_v2, mode="fixed_only")` | V2Products | default 6 + `index` | 5 | 104,266 / 71,615 | no pools at all: empty `variables` groups, no `_var_offsets_*`, no strings (`units`, `topology`), no `match_ids`...; `info@litified` |
| `incomplete` | v1.4.0, V1, never finalized | V1 | `run_info`, `reco_particles` | 2 | 52,294 / 15,982 | `info@complete = False` (the state SPINE leaves while writing) |

(10 products = `index`, `run_info`, `meta`, `trigger`, `reco_particles`, `reco_interactions`, `truth_particles`,
`truth_interactions`, `flashes`, `crthits`. "core 6" = the six `run_info`, `meta`, `reco_*`, `truth_*` products
plus `index`.) Total shipped: 1,806,607 bytes.

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

`make_fixtures.py` asserts that the full cases contain every label of `shape`, `pid`, `chi2_pid`, `current_type`,
`interaction_scheme`, both bool values, `nan`/`inf`/`-inf`, non-ASCII strings, empty and non-empty variable arrays,
an event with no object at all, an interaction without particles, and that ids, `interaction_id`, `particle_ids` and
`match_ids` obey the conventions above. Floats are mostly on a dyadic grid (exact in float32 and float64, short in
JSON); `match_overlaps`, `pid_scores`, some energies and every *derived* float (`ke`, `p`, `momentum`, sums...)
are deliberately not.

### Not shipped, but generated and checked: `v1.4.0_exotic_v{1,2}`

Non-object products (a `string`, a 2-D `array`, a `list`, a `multi_list` product, with empty events) written by
v1.4.0 in both layouts. They exist so that `decode_raw.py` is tested on every product kind (V1 `list`/`multi_list`
groups of region references, V2 `string`/`list`/`multi_list`); the CAF products never use them, and they cost
190 kB, so they are written to a temporary directory unless `--with-exotic` is given.

## Expected JSON format

```json
{ "case": "v1.4.0_v1",
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

## Cross-checks (asserted by `make_fixtures.py`)

1. `decode(v1.4.0_v1) == decode(v1.4.0_v2)` and `decode(v1.4.0_v1_lite) == decode(v1.4.0_v2_lite)`: **no
   difference at all** except `case`, `layout` and `writer.format_version`. (No legitimate difference was found:
   the column types, enums, empty-event rows and every value are identical.)
2. `lite` = `full` minus exactly `index`, `orig_index` (reco and truth) and `index_adapt`, `index_g4` (truth).
3. `fixed_only` = the full V2 file restricted to its 7 products and with every pooled column removed.
4. `expected == SPINE's own HDF5Reader` of the writing version (`build_classes=False`, i.e. the stored values) for
   every product, column and event of every case: 2745 / 2745 / 2637 / 2637 / 1619 / 1537 / 1999 / 393 values
   (the v0.11.1 file with the v1.4.0 reader, see below). The reader adds only its administrative entries
   (`index`, `file_index`, `file_entry_index`, `source_file_*`).
5. The default `HDF5Reader` accepts every complete file and raises `RuntimeError("... is marked incomplete")` on
   `incomplete.h5`.
6. Content conventions and coverage (see Cases).

## Surprises and findings about the real writers

* **Sizes.** SPINE's layouts are one chunk per dataset (h5py auto-chunking: 6-21 KB for each `fixed`, 4 KB for each int32/float32
  pool, 8 KB for `index/values` and `events`), so the size is set by the number of datasets, not by the events: a 5-event V2
  file with 10 products is 306 kB with h5py's default file format, 210 kB with `libver=v110` (V1: 195 -> 172 kB).
  V2 is *larger* than V1 for small files. `--libver v110` changes only the HDF5 file-format bound (compact groups,
  extensible-array chunk indexes; needs libhdf5 >= 1.10); no SPINE logic, dtype, attribute or chunk shape differs.
  Even so `v1.4.0_v2` (209,665 B) and `v1.4.0_v2_lite` (207,105 B) stay just above 200 kB.
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
  it in `spine.data`, which only exports `Meta`); the v1.4.0 reader reads it (used for the cross-check).
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
