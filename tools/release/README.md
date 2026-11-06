# tools/release: following SPINE releases

When SPINE tags a release, `follow_spine.py` answers one question: does spine-reader need a new
release for it? It dumps the tag's schema, compares it with the model, writes and checks the
tag's own fixtures, describes the change, and builds and tests spine-reader with it. It changes
nothing in the repository unless `--apply` is given, and it never commits.

| Tool | What it does |
| --- | --- |
| `follow_spine.py` | The whole chain for one SPINE tag; one outcome and exit code |
| `classify.py` | Describes the change between two schemas and proposes a version bump |
| `scan_consumer_fields.py` | Lists the model fields a CAFMaker build reads, for the contract table |

## What you need

`follow_spine.py` needs the LCG_110 view, git, network access to the SPINE repository and a
C++ compiler:

```bash
source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh
```

In detail, it needs:

- Python 3.10 or newer (SPINE's own requirement), with numpy, h5py, yaml and scipy;
- git;
- CMake 3.21 or newer, for `ctest --output-junit`;
- a C++ compiler.

Stage `s0_checks` checks all of this before any network access, and reports
`environment_error` when something is missing.

## Running it

```bash
tools/release/follow_spine.py v1.4.1
```

It prints the artifacts folder, a new temporary folder unless `--artifacts-dir` is given. That
folder holds:

- `status.json`;
- `classification.md` and `classification.json`;
- one log per stage in `logs/`;
- the new schema in `schema/` and the new current set in `fixtures/`;
- the export and its build.

The last lines it prints name the outcome and give the command that reproduces the run.

Options:

- `--spine-repo URL` (default `https://github.com/DeepLearnPhysics/spine`, or `$SPINE_REPO`):
  a git URL or a local path.
- `--workdir DIR`: where the SPINE clones live, one per commit. It is shared with
  `tools/fixtures/make_fixtures.py` (default `$TMPDIR/spine-reader-fixtures`).
- `--jobs N`: parallel build jobs (default: the number of CPUs).
- `--repo-root DIR`: the spine-reader checkout (default: the one the script is in).
- `--baseline SCHEMA`, `--apply`: see below.

What a run does, stage by stage:

| Stage | What it does |
| --- | --- |
| `s0_checks` | The tag's form, `--apply`'s conditions, the environment |
| `s1_tag` | The tag's commit (`git ls-remote`). The model's own tag must still point at the schema's `spine_sha`. One clone per commit |
| `s2_dump` | `tools/gen/dump_schema.py --spine-sha`, without `--strict`. The tag must equal `v` + `spine.__version__` |
| `s3_compare` | **identical** when `generate.py` gives the same files for both schemas once the version is normalised, and `type_check` and `writer_format` are unchanged; otherwise **changed** |
| `s4_fixtures` | `make_fixtures.py --tag`: the tag's current set, written by its own writer. Every cross-check runs, and the tag's own reader reads the files back |
| `s5_classify` | `classify.py`: what changed, and the proposed bump. A replay stops here |
| `s6_build` | `git archive HEAD` into the artifacts folder, with the new current set and manifest. If changed, it adds the new schema, the regenerated model, `check_names.py` and `golden_members.inc`. Then it configures and builds |
| `s6_test` | ctest with `--output-junit`, without `SPINE_READER_REAL_FILES`. Then the regenerated `spine-dump` reads HEAD's previous `current_v1`/`current_v2`, and `classify.py` runs again with that text |
| `s8_apply` | With `--apply`, after `changed` only: the new schema, model and fixtures go into the working tree |

Uncommitted changes are not tested: the build uses `git archive HEAD`.

## Outcomes

| Exit | Outcome | What to do |
| --- | --- | --- |
| 0 | `identical` | Nothing. The tag needs no spine-reader release, and the working tree is unchanged. If `needs_attention` is true, read the warnings in `classification.md` (for example a class `dump_schema.py` does not list) |
| 10 | `changed` | Read `classification.md`, then rerun with `--apply` and open a pull request with the proposed bump in `project(VERSION)`. With `contracts_failed`, a unit, default or enum value a CAFMaker relies on changed: talk to the CAFMaker maintainers before editing `tests/test_contracts.cpp` |
| 20 | `not_followed` | Nothing. The tag is a pre-release (`v1.5.0rc1`) or older than v1.2.1 |
| 30 | `environment_error` | Fix the environment (the log of `s0_checks` or `s1_tag` says what is missing), or check the network and `--spine-repo` |
| 30 | `tag_moved` | The model's own tag is gone, or points to another commit than the schema's `spine_sha`. Ask the SPINE maintainers; never re-dump over it |
| 30 | `dump_failed` | `dump_schema.py` cannot read the tag. `failure_reason` is the exception class (`AttributeError`, ...); fix the dumper |
| 30 | `version_mismatch` | The tag and `spine.__version__` differ: a SPINE release mistake. Report it upstream |
| 30 | `generate_failed` | `generate.py` or `check_names.py` refuses the new schema: fix the generator |
| 30 | `fixture_tooling_failed` | A fixture worker cannot drive the tag's API (`make_fixtures.py` exit 3): adapt `tools/fixtures/spine_worker.py` |
| 30 | `fixture_check_failed` | A cross-check failed, or the tag's own reader disagrees with the files it wrote. See `logs/s4_fixtures.log` |
| 30 | `build_failed` | The regenerated model does not build. See `logs/s6_build.log` |
| 30 | `test_failed` | A ctest entry other than `spine_reader_contracts` failed in the export. `stages.s6_test.failed_tests` names them |
| 30 | `internal_error` | A bug in the tools. The stage log has the traceback |
| 2 | (usage error) | Bad arguments, a malformed tag, a tag that does not exist (other than the model's own), or `--apply` that cannot apply. No `status.json` is written |

`status.json` records:

- `status_format_version` (1), and the tag, its `spine_sha` and `spine_version`;
- the baseline;
- `outcome`, `exit_code`, `failed_stage`, `failure_detail` and `failure_reason`;
- `contracts_failed`, `needs_attention`, `replay` and `partial` (always false);
- `comparison`, `proposed_bump` and `change_kinds`;
- `stages`: each with its `status`, `duration_s` and `log`;
- the warnings and substitutions;
- `tools`: the Python, numpy, h5py, HDF5, CMake, compiler and git versions;
- the spine-reader commit;
- `reproduce`.

## Replays

A replay checks an old pair of tags without building or applying anything: it stops after
`s5_classify`. A run is a replay when it is given a `--baseline` other than the committed
schema, or a tag older than the model's:

```bash
tools/release/follow_spine.py v1.2.3 --baseline tools/release/tests/data/spine-1.2.2.json
```

Its outcome is `identical` or `changed`, with `replay: true`.
`tools/release/tests/data/spine-1.2.2.json` and `spine-1.2.3.json` are dumps of those tags made
with the current dumper. Older tags still write their own fixtures, so a replay of v1.2.1 or
v1.2.2 also checks them, with the enum labels those tags lack listed as substitutions.

## `--apply`

`--apply` needs three things:

- a clean working tree, with no changes and no untracked files;
- no `--baseline`;
- a tag newer than the model's.

It writes only after a `changed` outcome:

- the new `schema/spine-X.Y.Z.json`, replacing the old one (git keeps it);
- the regenerated model, through `generate.py --root`, which also deletes the files the
  generator no longer writes;
- `tests/golden_members.inc`;
- the new current set (`tests/data/fixtures/current_*`) and the merged manifest. The frozen
  cases keep their entries.

It does not edit `project(VERSION)`, and does not commit. It prints the proposed bump and the
follow-up list. `git checkout . && git clean -fd` undoes it.

## `classify.py`

```bash
tools/release/classify.py BASELINE NEW [--old-columns DIR --new-columns DIR] [--warnings FILE] \
    [--baseline-warnings FILE] [--effects FILE] --json OUT --md OUT
```

`follow_spine.py` runs it twice. It can also be run by hand on any two schema dumps. It lists the
change kinds it finds, and proposes the bump of the highest one:

| Change kind | Proposed bump |
| --- | --- |
| nothing | `none` |
| field or enum docs, module paths (`doc_changed`, `module_changed`) | `none`: the regenerated model may merge without a tag |
| fields, enum members, enums or scheme tables added | `patch`, provisional until decision D6 (strict mode under a patch release) |
| a field, class, enum, enum member or scheme table removed; a shape or element retype; a renumbered enum member | `minor`: a 0.x release's breaking change under `SameMinorVersion` |
| a default, unit, flag or reference changed; a field reordered; a class added; enum aliases; a scheme table changed; `type_check`; `writer_format`; a stored width or type in the decoded columns; columns no schema change explains | `person`: a person decides |
| any kind not listed | `person` |

The order is none < patch < minor < person. `needs_attention` is true when a class was added,
when a warning is new (by its `(kind, name)`), or when the fixtures needed substitutions. Exit
codes: 0, or 2 on bad input.

## `scan_consumer_fields.py`

When a CAFMaker starts reading a model field, the field needs a row in
`tests/test_contracts.cpp`. Run the scan on that CAFMaker's build, in its own environment:

```bash
python3 tools/release/scan_consumer_fields.py --name ND \
  --source ~/ND_CAFMaker --build ~/ND_CAFMaker/build \
  --setup 'cd ~/ND_CAFMaker && source ./ndcaf_setup_deps.sh prof' \
  --wrap 'apptainer exec -B /cvmfs -B /tmp -B /home /cvmfs/singularity.opensciencegrid.org/fermilab/fnal-wn-sl7:latest' \
  --contracts tests/test_contracts.cpp
```

It lists the fields the build reads, and the ones that have no row (exit 1). See
[EGM.md](./EGM.md) for its options.

## Tests

`spine_reader_release_tools` (ctest, Python 3.10 or newer) runs
`python3 -m unittest discover -s tools/release/tests -t tools/release`. The tests need neither
SPINE nor the network:

- `test_classify.py` checks every change kind on mutated copies of the frozen
  `tests/data/spine-1.4.0.json`, the precedence of the bumps, the real pair 1.2.2 → 1.2.3, and
  that every change kind has a row in the bump table.
- `test_outcomes.py` checks `outcome_for()`, `check_usage()` and `compare_schemas()`. Its
  `--apply` test needs the git binary.
