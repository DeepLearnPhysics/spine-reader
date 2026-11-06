# tools/release

## Overview

Tooling for following SPINE releases. The hand-written [README.md](./README.md) is the guide:
the environment, the outcomes and what to do for each, replays, `--apply`.

- `follow_spine.py` runs the whole chain for one SPINE tag and ends in one outcome:
  - it dumps the tag's schema and compares it with the model;
  - it writes and checks the tag's fixtures;
  - it classifies the change;
  - it builds and tests an export of HEAD with the new schema and fixtures.

  CI runs it on the model's own tag and requires `identical`.
- `classify.py` describes the change between two schemas and proposes a version bump.
- `scan_consumer_fields.py` keeps the contract table of `tests/test_contracts.cpp` current. It
  lists the model fields one CAFMaker build reads, and, with `--contracts`, the ones that have no
  row. It runs the CAFMaker's own compile commands, so it is run by hand, in that CAFMaker's
  environment.
- `tests/`: the unit tests of the first two (ctest `spine_reader_release_tools`).

## Interface

### `follow_spine.py` (CLI; Python 3.10+; mode 100755)

```
follow_spine.py TAG [--baseline SCHEMA] [--apply] [--repo-root DIR] [--spine-repo URL]
                [--workdir DIR] [--artifacts-dir DIR] [--jobs N]
```

- Defaults:
  - `--repo-root`: the checkout the script is in;
  - `--spine-repo`: `$SPINE_REPO`, else `https://github.com/DeepLearnPhysics/spine`;
  - `--workdir`: `$SPINE_FIXTURE_WORKDIR`, else `$TMPDIR/spine-reader-fixtures`, shared with
    `make_fixtures.py`;
  - `--artifacts-dir`: a new `follow_spine-<tag>-*` temporary folder;
  - `--jobs`: the number of CPUs.
- Stages, in order: `s0_checks`, `s1_tag`, `s2_dump`, `s3_compare`, `s4_fixtures`,
  `s5_classify`, `s6_build`, `s6_test`, `s8_apply` (README.md describes each).
- Outcomes and exit codes:
  - `identical` 0;
  - `changed` 10, with `contracts_failed` true when `spine_reader_contracts` is the only
    failed ctest entry;
  - `not_followed` 20;
  - 30 for `environment_error`, `tag_moved`, `dump_failed`, `version_mismatch`,
    `generate_failed`, `fixture_tooling_failed`, `fixture_check_failed`, `build_failed`,
    `test_failed` and `internal_error`;
  - 2 for a usage error, which writes no `status.json`.
- `<artifacts>/status.json` keys:
  - `status_format_version` (1), `tag`, `spine_sha`, `spine_version`, `baseline {path,
    spine_version, spine_sha}`;
  - `outcome`, `exit_code`, `failed_stage`, `failure_detail`, `failure_reason`;
  - `contracts_failed`, `needs_attention`, `replay`, `partial` (false);
  - `comparison {result, files, type_check_changed, writer_format_changed, error}`,
    `proposed_bump`, `change_kinds`;
  - `stages {<name>: {status, duration_s, log, ...}}`, `warnings`, `substitutions`;
  - `tools {python, numpy, h5py, hdf5, cmake, compiler, git}`;
  - `spine_reader {commit, dirty}`, `artifacts`, `reproduce`.
- The other artifacts: `logs/<stage>.log`, `schema/spine-X.Y.Z.json`, `fixtures/` (the new
  current set and merged manifest), `head/fixtures/`, `classification.{json,md}`,
  `warnings.json`, `baseline-warnings.json`, `export/`, `build/`, `ctest-junit.xml`,
  `previous/`, `effects.txt`, `fixture-run/`.
- Functions the unit tests call, needing neither git nor the network nor SPINE:
  - `check_usage(args, repo)`;
  - `outcome_for(stages)`, which returns `(outcome, exit code, failed stage,
    contracts_failed)`;
  - `compare_schemas(baseline, new, generator=None)`;
  - `failed_tests(junit_path)`.

### `classify.py` (CLI and library; Python 3.10+, standard library only)

```
classify.py BASELINE NEW [--old-columns DIR --new-columns DIR] [--warnings FILE]
            [--baseline-warnings FILE] [--effects FILE] --json OUT --md OUT
```

- `classify(baseline, new, old_columns, new_columns, warnings, baseline_warnings, effects, ...)`
  returns these keys:
  - `baseline` and `new` `{path, spine_version, spine_sha}`;
  - `classes {<class>: {added, removed, retyped, reordered, changed}}`, `classes_added`,
    `classes_removed`;
  - `enums {<enum>: {added, removed, renumbered, aliases, doc_changed}}`, `enums_added`,
    `enums_removed`, `scheme_tables_changed`;
  - `type_check_changed`, `writer_format_changed`, `column_diffs`, `default_only_fields`,
    `unwritten_classes`;
  - `warnings_new`, `substitutions`, `effects_on_previous_files`;
  - `change_kinds`, `module_only`, `proposed_bump`, `reasons`, `follow_up`, `needs_attention`.
- `BUMP_TABLE` maps every change kind to `none`, `patch`, `minor` or `person`. `bump_of()` gives
  `person` for a kind the table lacks. The highest bump wins.
- `render_markdown(classification)`: the text of `classification.md`, ending with the checklist
  of `tools/gen/README.md`.
- Exit codes: 0 written; 2 bad input (an unreadable file, or a schema without `spine_version`,
  `classes`, `enums` or `scheme_tables`).

### `scan_consumer_fields.py` (CLI; Python 3.9+, standard library only)

```
scan_consumer_fields.py --source SRC (--build DIR | --compile-commands FILE)
                        [--setup BASH] [--wrap COMMAND] [--contracts FILE] [--name LABEL]
                        [--root DIR] [--schema FILE] [--workdir DIR] [--keep] [--json FILE]
```

- `--source`: the CAFMaker's source root. Only diagnostics that point into it count.
- `--build`: the build folder of a Makefile build; its compile commands are read from
  `make -n -B`, which runs nothing. `--compile-commands`: a `compile_commands.json` instead.
  Files under the build folder (or the folder of `compile_commands.json`) never count.
- `--setup`: bash lines run, in the same shell, before `make -n` and before the compiles (for
  example `source ./ndcaf_setup_deps.sh prof`). Their output goes to `setup.log`.
- `--wrap`: a command prefix that runs `bash <script>` elsewhere, for example
  `apptainer exec -B /cvmfs -B /tmp -B $HOME /cvmfs/singularity.opensciencegrid.org/fermilab/fnal-wn-sl7:latest`.
  The work folder must be visible there under the same path.
- `--contracts`: the contract table. A row is a line that starts with `{"<Class>", "<field>",`.
- `--root` (default: this checkout) and `--schema` (default: the one `schema/spine-*.json`):
  the headers to mark and the field names to mark in them.
- `--workdir` (default: a new folder in the system temporary folder), `--keep`: the work
  folder holds `include/` (the marked copy), `make-n.sh`, `compile.sh`, their logs,
  `setup.log`, `make-n.txt`, `diagnostics.txt` and `status.txt`. It is deleted after a
  successful run unless `--keep` is given.
- `--json`: also write `{name, source, schema, commands, failed_commands, errors, fields,
  unlisted}`; `fields` maps `Class.field` to its `file:line` uses.
- Standard output: `<name>: N model fields read, from M compile commands`, one line per field
  with its first use and the number of further uses, then, with `--contracts`,
  `<name>: K fields read with no row in <file>` and those fields.
- Exit codes: 0 the scan ran (and, with `--contracts`, every field read has a row); 1 some
  field read has no row; 2 usage error; 3 the scan could not run or is incomplete: no compile
  command for a file of the source root, `make -n` failed, a compile command failed or an error
  points into the source root, or the headers could not be marked.

Example, for the ND CAFMaker in the SL7 container:

```bash
python3 tools/release/scan_consumer_fields.py --name ND \
  --source ~/ND_CAFMaker --build ~/ND_CAFMaker/build \
  --setup 'cd ~/ND_CAFMaker && source ./ndcaf_setup_deps.sh prof' \
  --wrap 'apptainer exec -B /cvmfs -B /tmp -B /home /cvmfs/singularity.opensciencegrid.org/fermilab/fnal-wn-sl7:latest' \
  --contracts tests/test_contracts.cpp
```

## Invariants

- `follow_spine.py` never runs `git commit` and never edits `CMakeLists.txt`. Without
  `--apply` it writes only in the artifacts folder and the `--workdir` clones.
- Stage `s0_checks` runs before any network access:
  - a tag `vX.Y.Z` is followed;
  - `vX.Y.Z` followed by more characters is a pre-release, and is `not_followed`;
  - anything else is a usage error;
  - a tag below v1.2.1 is `not_followed`.
- `--apply` needs a clean working tree (no changes, no untracked files), no `--baseline`, and a
  tag newer than the model's; otherwise it is a usage error before any network access.
- A run is a replay when its `--baseline` is not the committed schema, or its tag is older than
  the model's. A replay stops after `s5_classify`. Without the default baseline, the
  classification has no column comparison, because HEAD's fixtures belong to the committed
  schema.
- `s1_tag`:
  - For the baseline's own version, a missing tag, or a commit other than the baseline's
    `spine_sha`, is `tag_moved`.
  - Another missing tag is a usage error.
  - A `git ls-remote` that fails is `environment_error`.
- `s3_compare` is identical exactly when `generate.generate()` gives the same files for the
  baseline and for the new schema with its `spine_version` set to the baseline's, and
  `type_check` and `writer_format` are equal. Warnings do not count.
- `s6_test`:
  - When only `spine_reader_contracts` fails and the comparison is `changed`, the outcome is
    `changed` with `contracts_failed`.
  - Any other failed entry, or a contract failure with an identical comparison, is
    `test_failed`.
  - `spine_reader_real_files` is skipped: `SPINE_READER_REAL_FILES` is removed from ctest's
    environment.
- The effects text and the second classification never change an outcome.
- `classify.py` never decides "identical". A warning is new when its `(kind, name)` is not among
  the baseline schema's warnings or `--baseline-warnings`. Substitutions (kind `substitution`)
  are listed apart.

- The marking:
  - The copy of `include/` marks every member of every schema class
    `[[deprecated("SPINEFIELD <Class>.<field>")]]`, and every `Field` enumerator likewise.
  - A member or enumerator that is not found exactly once in its class's header stops the scan
    (exit 3): the generated header layout changed and the tool must follow it.
- The compile commands:
  - Each command keeps the CAFMaker's flags.
  - It loses `-o` and the dependency flags (`-MD`, `-MMD`, `-MP`, `-MF`, `-MT`, `-MQ`), so
    nothing is written to the build folder.
  - It gets `-I<work>/include` right after the compiler (or after a `ccache`-style launcher),
    so the marked headers come before any other spine-reader include path.
  - It ends with `-fsyntax-only -Wdeprecated-declarations -Wno-error
    -fdiagnostics-color=never`, which override the CAFMaker's own `-Werror`.
  - Every command runs with `LC_ALL=C LANG=C`, from its own directory. Each command is run
    once, even if the build lists it twice.
- One read is one deprecation diagnostic (warning or error) whose location is in the source
  root and outside the build folder. Colour codes are stripped before parsing. Locations are
  resolved against the directory of the command that printed them. Diagnostics inside the
  marked headers are not reads: they come from the implicit copies of objects in
  `Event::objects` and from the library's own inline code.
- The list is the set of `Class.field` read, in schema order. A field that is only named in a
  template instantiated from the CAFMaker, with the use inside a spine-reader header, is not a
  read of that CAFMaker.

## Side Effects

- `follow_spine.py`:
  - It writes the artifacts folder.
  - It clones SPINE commits into `--workdir`, through `tools/fixtures/spine_checkout.py`.
  - It runs `dump_schema.py`, `make_fixtures.py`, `classify.py`, `generate.py`,
    `check_names.py`, `make_member_list.py`, CMake, the compiler and ctest as subprocesses.
  - It accesses the network through `git ls-remote` and `git clone` of `--spine-repo`.
  - With `--apply`, after `changed`, it also writes into the working tree of `--repo-root`:
    - `schema/spine-*.json`, replaced;
    - `include/spine/model/`, `include/spine/model.hpp` and `src/model/`, regenerated;
    - `tests/golden_members.inc`;
    - `tests/data/fixtures/current_*` and `manifest.json`.
- `classify.py` writes only `--json` and `--md`.

- Creates the work folder and writes the files listed under `--workdir`; deletes it after a
  successful run unless `--keep` is given, and keeps it when the scan fails.
- Runs `make -n -B` in the build folder and the CAFMaker's compilers, through `--wrap` when
  given. With `-fsyntax-only` and the output flags removed, they write nothing there. The
  `--setup` lines run as given, with the caller's permissions.
- No network access of its own.

## Verification

- `python3 -m unittest discover -s tools/release/tests -t tools/release` (ctest
  `spine_reader_release_tools`, registered for Python 3.10 or newer).
- `follow_spine.py v<model version>` gives `identical` (exit 0). CI runs it in the HDF5 1.14 job
  (`ci/github-ci.sh --follow-spine`).
- 2026-11-06, local SPINE tags in a scratch clone (`--spine-repo <clone>`) gave the outcomes of
  the release design's verification table. For example:
  - a field added: `changed`, patch;
  - a default, unit or field the CAFMakers read: `changed` with `contracts_failed`;
  - two enum values swapped: `test_failed`;
  - a reader that raises: `fixture_check_failed`;
  - `FieldMetadata.pdg` deleted: `dump_failed`;
  - `version.py` not updated: `version_mismatch`;
  - a renamed writer parameter: `fixture_tooling_failed`;
  - a moved v1.4.0: `tag_moved`.

  Also checked: the replays of v1.2.1, v1.2.2 and v1.2.3, and `--apply` on the added field in a
  clean clone.

- On 2026-11-06 the three ports were scanned in the SL7 container. Each was compared with the
  scan of 2026-11-05 from which the contract table was written:
  - ND (DUNE/ND_CAFMaker#213; its ML filler is unchanged since `fbda59e`): 47 fields;
  - SBN (justinjmueller/sbn_ml_cafmaker#8): 219 fields;
  - FD (DUNE/dune_ml_cafmaker#1): 31 fields.

  The three sets are those of that scan: 228 fields in all, 24 read by all three, each with a
  contract row.
- `python3 -m flake8 --max-line-length=100 tools/release/scan_consumer_fields.py`
