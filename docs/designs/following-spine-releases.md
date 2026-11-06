# Following SPINE releases in spine-reader: stage 0 (local tooling) and stage 1 (CI)

*   **Author:** Pierre Granger
*   **Status:** Implemented. The six pull requests of the plan were merged as
    DeepLearnPhysics/spine-reader pull requests 1 to 6 (2026-11-05 and 2026-11-06), and v0.1.0
    was tagged on the merge commit of pull request 2. The 16 open points of the stage 9 re-audit
    (R1-R16) were fixed in them; the table at the end says where. Where the code differs from the
    plan, the section "As built" at the end says so, and it is the one that is right.
*   **Date:** 2026-11-05
*   **Predecessor:** the design brief `concepts_brief.md` (the author's local notes, not in the
    repository). The decisions and findings this document relies on are summarised in Appendix A,
    so it can be read without the brief.
*   **Companion file:** `following-spine-releases.cafmaker-fields.tsv`, next to this document,
    lists the 228 model fields that the three CAFMakers read (Appendix B).
*   **Line numbers** in code references refer to spine-reader main at commit 6e2d9ee, before
    the implementation.

--------------------------------------------------------------------------------

## Overview

**spine-reader** is a C++17 library that reads the HDF5 files written by **SPINE**, the
DeepLearnPhysics machine-learning reconstruction package written in Python. Three **CAFMakers**
use it: the SBN, DUNE FD and DUNE ND programs that turn SPINE output into CAF analysis files. Its
C++ object model is generated from SPINE's own field metadata at one SPINE release (1.4.0
today), and the generated code is committed:

*   `tools/gen/dump_schema.py` imports SPINE at a tag and writes `schema/spine-X.Y.Z.json`;
*   `tools/gen/generate.py` turns that JSON into `include/spine/model/` and `src/model/`.

**Releases.**

*   A **SPINE release** is a final tag `vX.Y.Z` in SPINE's repository.
*   A **spine-reader release** is a git tag `vX.Y.Z` made from `project(VERSION)` in
    `CMakeLists.txt`.

SPINE releases often: 25 final tags between 20 July and 5 October 2026, several on the same
day. Of the five consecutive pairs of final tags from v1.2.1 to v1.4.0, four are identical in the
sense of decision D2 (defined in G5). The fifth, v1.2.3, adds enum members.

The end goal is that every new SPINE release is checked automatically, and that a person is
needed only to merge a pull request when the schema changes. The design brief splits the work
into five stages. This document covers the first two:

*   **Stage 0, local.**
    *   Make "follow a SPINE release" one command that works for any final tag from v1.2.1 on,
        and ends in one of a few fixed outcomes.
    *   Keep the test suite green when a release only adds things.
    *   Give the library a version of its own, and cut the first release, `v0.1.0`.
*   **Stage 1, CI.** Build and test spine-reader on GitHub for every push and pull request. CI
    lands first, so every later pull request and the first tag are checked by it.

Stages 2 to 4 are listed under Open Questions and Future Improvements, and will be designed once
stages 0 and 1 work:

*   the workflow that scans SPINE's tags;
*   that workflow's writes to the repository (records, pull requests, tags);
*   optional changes in SPINE.

**Conventions.**

*   `tagdump/` and `fieldscan/` are the author's local evidence folders, not in the repository.
    *   `tagdump/` holds the per-tag schema dumps, the comparison prototype `cmp.py`, the mutation
        script `sim/mutate.py` and the logs of the regeneration simulations.
    *   `fieldscan/` holds the CAFMaker field scan of Appendix B and the CI dry run of part 7.
*   SPINE source references are links pinned to the v1.4.0 commit `4726e068`.
*   IDs such as D2 or A11 refer to Appendix A.

## The Problem

Following a SPINE release today is a manual procedure in `tools/gen/README.md`, and the code
around it assumes one SPINE version, 1.4.0. The design review found that this blocks any
automation, and also makes the manual procedure fragile:

*   **The library has no version of its own.**
    *   The CMake package version comes only from `project(spine-reader VERSION 0.1.0 ...)`
        (`CMakeLists.txt:2`).
    *   A git tag does not change it, there are no tags yet, and nothing in the library or in
        `spine-dump` reports a spine-reader version.
    *   CAFMaker users therefore cannot be told "SPINE Y needs spine-reader X": there is no X to
        point at, and no way to see which X a job used.
*   **The tests fail on changes that are expected.**
    *   Regenerating the model for a tag that changes nothing in the schema fails 3 of the 5 ctest
        entries: two because the SPINE version `"1.4.0"` is written into tests, one because the
        generated member list `tests/golden_members.inc` is stale.
    *   Adding one field also fails the hand-written field counts and indices. Those counts do not
        check the generator, which checks its own output at compile time (`generate.py:677-687`).
        They only detect that the schema changed, which the schema diff already shows.
    *   A red run therefore does not tell a reviewer whether something broke.
*   **The fixture check cannot test a new tag.** The fixture files are written by SPINE's own
    writer and decoded without SPINE.
    *   Their cases, cross-checks and the C++ golden case list are fixed to v1.4.0
        (`make_fixtures.py:43-57, 418-455`; `test_golden.cpp:32-35`).
    *   If the new tag's own reader fails, the script falls back to the v1.4.0 reader and passes
        (`make_fixtures.py:465-474`).
    *   The list of fields lite mode drops is hard-coded (`make_fixtures.py:61`).
*   **"Did anything change?" has no precise answer.** A raw diff of the schema JSON cannot say what
    kind of change happened: a module move looks like a field change. It also misses stored-width
    drift, a change of the float or integer width SPINE writes. No tool says which kind of change
    happened.
*   **Nothing builds spine-reader on GitHub.** There is no CI. The HDF5 that apt installs on stock
    GitHub Ubuntu runners is 1.10, which `find_package(HDF5 1.12 ...)` (`CMakeLists.txt:30`)
    rejects.

The proposed solution has five parts:

*   add CI on GitHub runners that get a supported HDF5 from CVMFS, CERN's read-only software
    distribution file system;
*   give the library a single version source and expose it;
*   make the tests independent of the SPINE version, replace the count-based change detectors with
    checks against the files' own columns (D4), and turn the fields the CAFMakers read into
    explicit contract tests;
*   make the fixture tooling take any tag;
*   add a classifier and one command with fixed outcomes.

## Goals

The table after the goals lists, for each goal, the part of the Technical Plan that delivers it
and the check that proves it.

### Stage 0

*   **G1. One version source, reported by the library, `summary()` and `spine-dump` (D1).**
    *   `project(VERSION)` is the only place the spine-reader version is written.
    *   `spine::version()` returns it.
    *   `SchemaReport::summary()` and `spine-dump --version` print it next to the model's SPINE
        version.
    *   A `v0.1.0` tag exists on the commit that introduces this, with the 1.4.0 model. It was
        made after CI passed on that commit.
*   **G2. Expected changes keep the suite green (D4).**
    *   A run on a tag identical to the model (G5) passes every ctest entry with no test edited.
        So does a regeneration whose only differences are version strings.
    *   A tag that only adds fields or enum members passes every ctest entry, once the new tag's
        fixtures are in place. A new `NuInteractionScheme` member must come with its interaction
        tables in SPINE's `Neutrino.resolve_enum`: without them the dumper cannot tell which enum
        reads `interaction_mode` and `interaction_type` under that scheme, and stops
        (`dump_failed`, "cannot identify the enum of interaction_mode under ...").
    *   For any of the 228 fields a CAFMaker reads (Appendix B), a change of unit or default, or
        the removal of the field, fails the consumer contract tests (ctest entry
        `spine_reader_contracts`). The run reports `changed` with `contracts_failed`.
    *   Other kinds of change behave as follows; the classifier marks every one "minor" or "a
        person decides":
        *   renumbering an enum member fails the contract tests (`changed` with
            `contracts_failed`). If two labels swap values, the frozen fixtures no longer bind
            that field, and the golden test fails (`test_failed`);
        *   retyping a field or changing its shape may fail other tests or the build;
        *   removing a field that test or example code names fails the build;
        *   removing a field that no code names and no CAFMaker reads, adding a class, or
            changing a flag or reference passes every test.
*   **G3. The model is checked against the files, not against counts (D4).** The existing golden
    test also checks:
    *   for each current full fixture, every field of a class it contains has a column, and no
        column is outside the model;
    *   for current lite fixtures, the missing fields are exactly the class's `lite_skip` fields,
        and no column is outside the model;
    *   element kinds agree (bool, integer, float, string). Widths are reported, not failed.

    Classes that no current fixture writes are reported as not checked.
*   **G4. The fixture check works for any final tag from v1.2.1 on.**
    *   Given a tag, the fixture tooling writes that tag's current set with its own writer.
    *   It runs every cross-check or fails, and uses that tag's own reader with no fallback.
    *   For tags at or after the model's, the C++ golden test reads what was written.
    *   Only tags newer than the model's are ever applied to the repository.
*   **G5. Changes are classified, and "identical (D2)" is exact.**
    *   A tag is **identical** to the model when:
        *   `generate.generate()` gives the same files for the baseline schema and for the new
            schema with its `spine_version` set to the baseline's;
        *   `type_check` (the dumper's comparison of declared types with the writer's dtypes) is
            unchanged;
        *   `writer_format` (the writer's format versions) is unchanged.
    *   Every other case is described per kind by a classifier, which proposes a version bump from
        a table that covers every kind.
*   **G6. One command, fixed outcomes.**
    *   `tools/release/follow_spine.py vX.Y.Z` runs the whole chain.
    *   It exits 0 (identical), 10 (changed), 20 (not followed), 30 (failure) or 2 (usage error).
    *   For every outcome except a usage error, it writes a `status.json` that names the outcome
        and the stage that failed.
*   **G7. Fixtures are named by role and committed only on content change (D10).**
    *   A run on an identical tag commits nothing.
    *   A schema change rewrites the current set.
    *   Frozen fixtures are never rewritten.
*   **G8. The `EGM.md` contracts say what is true.** These are the per-folder files that state
    each folder's interface, invariants and side effects, for people and coding assistants.
    *   They state no literal model version or field count.
    *   The release tooling has its own `EGM.md`.

### Stage 1

*   **G9. CI on every push and pull request, from the first pull request on.**
    *   A GitHub Actions workflow builds spine-reader and runs ctest in two jobs, each on a stock
        Ubuntu runner with CVMFS and an LCG software view: LCG_110 `x86_64-ubuntu2404-gcc13-opt`
        (HDF5 1.14.6) and LCG_105 `x86_64-ubuntu2204-gcc11-opt` (HDF5 1.12.2).
    *   Missing Python fails the configuration.
    *   A tag push fails unless the tag equals `v` + `project(VERSION)`.
    *   From pull request 6 on, the 1.14 job also runs `follow_spine.py` on the model's tag and
        requires `identical`.

### Goals, plan parts and checks

| Goal | Delivered by | Checked by (Verification Plan) |
| --- | --- | --- |
| G1 | Technical Plan part 1; the tag check in part 7 | the `--version` check in `check_spine_dump.cmake`; `ci/check_tag_version.sh`; `git rev-parse v0.1.0^{commit}` names the merge commit of pull request 2, and CI is green on it |
| G2 | part 2 | the relabel regeneration; local tags v1.4.99 (field added), v1.4.98 (contract default changed), v1.4.97 (contract field removed), v1.4.92 (unit changed), v1.4.91 (enum member added), v1.4.90 (labels swapped), v1.4.88 (unread field removed) |
| G3 | part 2 | `spine_reader_tests` on the current fixtures; the unit test of the column check on a doctored product; the v1.4.99 run |
| G4 | part 3 | the replay of v1.2.3; the runs on v1.2.1 and v1.2.2; v1.4.99; v1.4.86 (reader fails) |
| G5 | parts 4 and 5; data-flow S3 | `test_classify.py`; `test_outcomes.py` on `type_check` and `writer_format` changes; the replays |
| G6 | part 6 | `test_outcomes.py`; the local-tag runs that reach each outcome |
| G7 | part 3; `--apply` in part 6 | `git status` after `--apply` on v1.4.99 and after an identical run |
| G8 | the `EGM.md` items of each pull request | review of the listed items, plus a grep for literal versions and counts |
| G9 | part 7 | the local dry run (done); the first CI run on pull request 1; configuring without Python fails; the `follow_spine` step from pull request 6 |

## Non-Goals

*   **Stages 2 to 4.** There is no scheduled scan of SPINE's tags, and no workflow that opens pull
    requests, writes records or pushes tags. There is no GitHub App, and no change in SPINE's
    repository. (Brief stages 2-4, decision D5.)
*   **The compatibility record.** Its format and meaning (D3) belong to stage 2. Stage 0 writes a
    `status.json` per run and commits no records.
*   **A JSON Schema for `status.json`.** It will be written when stage 2, its reader, is designed.
*   **Packaging.** A spine-reader release is a git tag only: no Spack, UPS or CVMFS build.
*   **Changes to the CAFMakers.** Their logs will show the new first line of `summary()`, but no
    CAFMaker code changes here.
*   **SPINE tags older than v1.2.1.** The dumper cannot import them. The entry point reports
    `not_followed` (exit 20) and does nothing else.
*   **The gcc 12 / SL7 check on GitHub.** It stays a manual step (`ci/sl7-ups.sh`), the old Scientific
    Linux 7 container with the UPS products the ND CAFMaker builds with.
*   **Moving the schema export into SPINE** (D9).
*   **Choosing the next version automatically.** The classifier proposes a bump, and a person
    writes it into `project(VERSION)`.
*   **A build identifier besides the release number.** Between releases, builds of main report the
    last release number. Production builds use tags.
*   **Tests of the defaults and units of fields no CAFMaker reads.** They only detect SPINE
    changes, and the classifier reports every default and unit change as "a person decides".

## Open Questions

Decided and recorded in Appendix A, not to be reopened here: D1, D2, D4, D5 and D10. The choices
made while drafting and reviewing are recorded under Alternatives.

These are left to stages 2 to 4:

*   **D3. The record.** Where the per-tag facts live, their format, and the exact meaning of each
    column in `SchemaReport` terms. In particular, whether unknown enum labels and same-shape
    retypes count against a release.
*   **D6. Strict mode under a patch release.** After an added field, `ReaderOptions::strict`
    refuses files from the previous SPINE release (`src/reader.cpp:66-68`). None of the three
    CAFMaker ports passes `strict` today. The options:
    *   strict requires the exact SPINE version;
    *   strict accepts files that lack only fields newer than the file's SPINE version;
    *   every added field is a minor bump.

    Until D6 is decided, the classifier's "additions are patch" rule is provisional.
*   **D7. Which tags are followed.** Whether only final `vX.Y.Z` tags are followed, and how
    backports and older tags are handled beyond the check-only replay of part 6.
*   **D8. Owners.** CODEOWNERS for spine-reader, and which parts the SPINE maintainers own.
*   **D9. Schema export ownership.** To be revisited after the next `FieldMetadata` change that
    breaks the dumper.
*   **D11. Release notes.** An annotated tag message or a CHANGES file, and an acknowledgement
    from the CAFMaker maintainers before a minor release.

## Critical User Journeys (CUJs)

### CUJ 1: A new SPINE tag is identical to the model (D2)

*   **Given**: main has the model for SPINE 1.4.0, and SPINE tags v1.4.1 with an identical schema
    (G5).
*   **When**: a maintainer runs `tools/release/follow_spine.py v1.4.1`.
*   **Then**:
    *   The outcome is `identical` (exit 0).
    *   `status.json` records the tag, its commit SHA, the comparison, and the result of writing
        and reading v1.4.1's own files in every layout.
    *   The working tree is unchanged.

### CUJ 2: A new SPINE tag adds fields or enum members

*   **Given**: the same main, and a SPINE tag that adds enum members, as v1.2.3 did.
*   **When**: the maintainer runs the entry point on that tag.
*   **Then**:
    *   The outcome is `changed` (exit 10), not a failure.
    *   The classification lists each added member and proposes a patch bump, provisional until
        D6. It also lists follow-up work, for example "the fixtures do not use the label GIBUU".
    *   After the regeneration, every ctest entry passes with no test edited.

### CUJ 3: A new SPINE tag breaks the tooling or the files

*   **Given**: a SPINE tag whose `FieldMetadata` no longer has an attribute the dumper reads, or
    whose files spine-reader cannot read correctly.
*   **When**: the maintainer runs the entry point on it.
*   **Then**:
    *   The outcome is a failure (exit 30).
    *   `status.json` names it: `dump_failed` in the first case; `test_failed`, from the golden
        test in the temporary build, in the second.
    *   The logs say what failed, and the command that reproduces the run is printed.

### CUJ 4: A CAFMaker user checks which spine-reader read a file

*   **Given**: a CAFMaker job log that prints `schema_report().summary()`.
*   **When**: the user reads the report.
*   **Then**:
    *   It shows the SPINE version that wrote the file, the spine-reader version, and the SPINE
        version the model came from.
    *   For a production build from a tag, the spine-reader version names that release.
    *   Between releases, builds of main show the last release number.

### CUJ 5: A contributor opens a pull request on spine-reader

*   **Given**: a pull request on DeepLearnPhysics/spine-reader.
*   **When**: it is opened or updated.
*   **Then**: CI builds and tests it with HDF5 1.12 and 1.14, and reports a failure if any test
    fails or Python is missing.

### CUJ 6: A maintainer turns a schema change into a pull request

*   **Given**: a run on a newer SPINE tag ended `changed`, and the working tree is clean.
*   **When**: the maintainer runs the same command with `--apply`.
*   **Then**:
    *   The new schema, the regenerated model, `tests/golden_members.inc`, the current fixtures
        and the manifest are written into the working tree.
    *   The maintainer reads `classification.md`, writes the proposed bump into
        `project(VERSION)`, commits and opens a pull request.

## Technical Plan (Architecture)

Part 1 changes the library's public interface; parts 2 to 7 change only tests, tools and CI.
Parts 1 to 6 are stage 0, and part 7 is stage 1. Part 7 lands first, except for its
`follow_spine` step.

### Data flow of one run of the entry point

```text
 tools/release/follow_spine.py TAG [--baseline schema.json] [--apply] [--repo-root DIR]
   |
   | S0 checks that need no network:
   |    tag form: ^v\d+\.\d+\.\d+$ followed; ^v\d+\.\d+\.\d+\S+$ (pre-release) -> not_followed;
   |    anything else -> usage_error; below v1.2.1 -> not_followed
   |    --apply: needs a clean working tree, a default baseline, and a tag newer than the
   |    model's                                                    -> usage_error
   |    environment: Python modules numpy, h5py, yaml, scipy; git; CMake >= 3.21; a C++
   |    compiler                                                   -> environment_error
   | S1 resolve the tag with git ls-remote: refs/tags/T^{} if listed (annotated tag), else
   |    refs/tags/T (lightweight tag); ls-remote fails             -> environment_error
   |    for the baseline's own version: tag absent, or a SHA other than its spine_sha
   |                                                               -> tag_moved
   |    for any other version: tag absent                          -> usage_error
   v
 SPINE checkout at that SHA (tools/fixtures/spine_checkout.py, one cache under --workdir)
   |
   | S2 dump_schema.py --spine-sha SHA (no --strict)       -> artifacts/schema/spine-X.Y.Z.json
   |    any non-zero exit                                  -> dump_failed (sub-reason in status)
   |    tag != "v" + spine.__version__                     -> version_mismatch
   v
 S3 compare with the baseline (default: the committed schema/spine-*.json)
   |    generate.generate(baseline) vs generate.generate(new, spine_version := baseline's),
   |    compared in memory, plus type_check and writer_format     -> identical | changed
   |    GenerationError                                           -> generate_failed
   v
 S4 make_fixtures.py --tag TAG --sha SHA --schema <new dump> --out artifacts/fixtures
   |    writes the current set with that tag's writer; decode_raw.py (no SPINE);
   |    every cross-check runs; that tag's own reader, no fallback
   |    exit 3 (worker cannot drive SPINE's API)            -> fixture_tooling_failed
   |    exit 1 (a cross-check or the comparison with SPINE's own reader fails)
   |                                                        -> fixture_check_failed
   |    exit 2 or 4                                         -> internal_error
   v
 S5 classify.py (first run): baseline and new schema, committed and new expected.json, warnings
   |                                                    -> artifacts/classification.{json,md}
   |    replay (non-default baseline, or tag older than the model's): stop here
   v
 S6 temporary export of HEAD (git archive HEAD, outside the repository; uncommitted changes
   |    are not tested)
   |    overlay: the new current set and the merged manifest; if changed, also
   |    generate.py --root <export> --schema <new>, the new schema replacing the old one,
   |    check_names.py --root <export>                      -> generate_failed
   |    and the export's own make_member_list.py
   |    configure (SPINE_READER_REQUIRE_PYTHON=ON) + build  -> build_failed
   |    ctest, with SPINE_READER_REAL_FILES cleared
   |       only spine_reader_contracts fails                -> changed, contracts_failed
   |       any other entry fails                            -> test_failed
   |    the export's spine-dump on HEAD's previous current_v1 and current_v2 (git show)
   |    classify.py (second run) with --effects             -> classification.{json,md}
   v
 S7 outcome -> exit code; artifacts/status.json with the reproduce command
   |
   | S8 --apply (changed only): the S6 overlay is written into the working tree
   v
 person: reads classification.md, writes the bump into project(VERSION), commits
```

An unexpected exception anywhere gives `internal_error`. A CMake configuration failure in S6 is
`build_failed`, with the CMake log.

### 1. The library's own version (G1, D1)

*   **`spine::version()`.** It is declared in a new hand-written header
    `include/spine/version.hpp`, and defined in the library from a compile definition set to
    `${PROJECT_VERSION}`. `project(VERSION)` stays the only place the number is written.
    `reader.hpp` includes the header, so every user of `Reader` has it.
*   **The first line of `summary()`** changes from
    `SPINE 1.4.1, format_version 1 (V1), read with model 1.4.0` to
    `SPINE 1.4.1, format_version 1 (V1); read by spine-reader 0.1.0 (model from SPINE 1.4.0)`.
    *   The new wording names both programs, so a model older than the file reads as a normal lag
        rather than a mismatch.
    *   `SchemaReport` keeps its layout: `summary()` calls `spine::version()`.
    *   This is the only change the CAFMakers' logs show.
*   **`spine-dump --version`** prints the same pair and exits 0, whatever other arguments are
    given.
*   **Tagging a release.**
    *   The author runs `ci/check_tag_version.sh vX.Y.Z` before pushing. It refuses a tag that
        differs from `v` + `project(VERSION)`.
    *   Only then is the tag pushed, on a merge commit on which CI has passed.
    *   CI repeats the check on the tag push, as a backstop. A tag is never moved or deleted, so a
        red backstop means: publish a fixed release with the next patch number, and mark the
        wrong tag as withdrawn in the README's "Versions" section.
*   **The first release.** `v0.1.0` is made this way on the merge commit of pull request 2. The
    model stays at SPINE 1.4.0, so `v0.1.0` reads exactly what main reads today.

### 2. Tests that survive expected changes (G2, G3, D4)

The tests fall into three groups (finding A4):

| Group | What it is | What happens to it |
| --- | --- | --- |
| Incidental assertions | They mention the model but test something else: literal model versions in summary text; completeness of frozen test files; literal defaults and enum numbers in tests of other behaviour; positional field checks; the field count in the binder test | rewritten so that they do not depend on the schema |
| Change detectors | Field counts, the last-field table, enumerator indices, enum member counts, the literal schema version, defaults and units of fields no CAFMaker reads, flag and reference expectations, the exact class list | removed; the generator's own compile-time checks and the golden test cover consistency |
| Consumer contracts | Units and defaults of the 228 fields the CAFMakers read, enum values, scheme tables, and the presence of every class a CAFMaker reads | a hand-written table, in their own ctest entry |

*   **Incidental assertions.**
    *   The summary text is built from `spine::version()` and `SPINE_READER_MODEL_SPINE_VERSION`.
        The file versions in expected strings ("SPINE 1.4.0, format_version 1") describe frozen
        test files and stay literal.
    *   Completeness checks on frozen test files check `mismatched` and `extra`, and the fields
        the file does have, instead of `complete()`.
    *   Tests of other behaviour compare with a default-constructed object
        (`RecoParticle{}.interaction_id`), never with a literal default. They write enum values as
        `static_cast<std::int64_t>(ParticleShape::TRACK)`, never as a literal number.
    *   "Field i has the name of enumerator i" is checked for every field of every class through
        `golden_members.inc`, instead of for a few fields by position.
    *   The binder test's count uses `kNumFields`.
*   **The check against the files' columns (G3).**
    *   It is added to the existing golden test, which already compares, for every object product
        of every fixture, the model's fields with the decoded columns, and requires that nothing
        is mismatched (`test_golden.cpp:305-349`). The new part is a function,
        `column_problems(descriptor, product, role)`, that returns the list of problems so that it
        can also be unit-tested. For the current cases it requires:
        *   full fixtures: nothing missing, nothing extra;
        *   lite fixtures: the missing fields are exactly the class's `lite_skip` fields, and
            nothing is extra;
        *   every field: the element kind of the model agrees with the decoded column type. Width
            differences are printed (`MESSAGE`) but do not fail, because SPINE's writer picks
            float widths from the first object's Python value. Today `truth_particles.ke` and
            `.mass` are stored as float32 against a float64 model, and `reco_particles.momentum`
            and `truth_particles.reco_momentum` as float64[3] against float32[3]
            (`tools/fixtures/README.md:158-160`).
    *   Model classes that no current fixture writes (today `FlashHypothesis`) are printed as "not
        checked against a file".
    *   `current_v2_fixed_only` is excluded from the missing and extra checks by name, because its
        columns are a subset by design. `current_incomplete` is excluded through its
        `writer.complete` flag (part 3).
    *   Field order is not compared. Binding is by name, and the dumper already refuses a schema
        whose order differs from the writer's (`dump_schema.py:388-389`).
*   **Consumer contract tests (`spine_reader_contracts`).** Option (a) of the review (see
    Alternatives J and K), built as one table rather than one hand-written block per field
    (Alternative Q).
    *   **The table.** `tests/test_contracts.cpp` holds one row per field a CAFMaker reads:
        class, field, unit, the default as a JSON literal, and the CAFMakers that read it.
        *   The 228 rows are those of Appendix B, written once from SPINE 1.4.0's values (for
            example `{"RecoParticle", "csda_ke", "MeV", "\"nan\"", "ND SBN"}`).
        *   After that, a row changes only by hand.
    *   **The check.**
        *   For each class, the test walks `golden_members.inc` (the generated list of members,
            already used by the golden test). For each member that has a row, it compares the
            unit with the descriptor's `units`, and the member of a default-constructed object
            with the JSON default, using the golden test's `equal_member()`. That function moves
            to a shared header, `tests/member_compare.hpp`.
        *   A row whose field the model no longer has is a failure ("listed for ND, SBN, but not
            in the model"), so the removal of a field a CAFMaker reads gives `contracts_failed`.
        *   Every class that has rows must exist in the model.
    *   **Where they run.** The file is compiled into the existing test binary, with test cases
        named `contract: ...`. ctest runs them as their own entry, `spine_reader_tests
        --test-case=contract*`, in the same way `spine_reader_real_files` is split off today
        (`tests/CMakeLists.txt:14, 21-22`). `ci/sl7-ups.sh`, which runs the whole binary, keeps
        running them.
    *   **Also moved here:** the enum value checks and the scheme-table checks.
    *   **Keeping the table current.** `tools/release/scan_consumer_fields.py` repeats the scan of
        Appendix B on a CAFMaker build, and lists the fields it reads that have no row (Risks R12).
    *   **Deleted** as change detectors:
        *   the defaults of fields no CAFMaker reads;
        *   the flag and reference expectations (`test_model.cpp:279-320`). The golden test checks
            `kLiteSkip` against the lite files;
        *   the exact, ordered class list.
    *   **Kept in `test_model.cpp`:**
        *   the member-type checks and the descriptor-type checks, because they check the C++
            mapping and the generator;
        *   the enumeration-pointer checks;
        *   the checks of reserved-name renaming (`NULL_`) and the `Shape` alias.
*   **What each kind of SPINE change does to the suite:**

| SPINE change | Result in the temporary build |
| --- | --- |
| identical (D2) | every entry passes |
| fields or enum members added | every entry passes, once the new tag's fixtures are in place |
| unit or default of a field a CAFMaker reads changed | `spine_reader_contracts` fails: `changed` with `contracts_failed` |
| a field a CAFMaker reads removed | `spine_reader_contracts` fails: `changed` with `contracts_failed`; if test or example code names the member, the build fails first: `build_failed` |
| enum member renumbered | `spine_reader_contracts` fails: `changed` with `contracts_failed`; if two labels swap values, the frozen fixtures no longer bind the field and the golden test fails: `test_failed` |
| a field no CAFMaker reads and no code names removed | every entry passes; only the classifier's minor bump flags it |
| a class added; a flag or reference changed | every entry passes; the classifier marks it "a person decides" (a class sets `needs_attention`) |
| retype, shape change, removed enum member | may fail other tests or the build (code that uses exact member types, frozen fixtures read with the new model): a failure, with the classification saying why |

### 3. Fixture tooling for any tag (G4, G7, D10)

*   **The cases.**
    *   The **current set** is written by the model's SPINE tag:
        *   `current_v1` and `current_v2`: full files in format_version 1 (region references) and
            2 (offsets and pools);
        *   `current_v1_lite` and `current_v2_lite`: the writer's lite mode, which drops the
            per-voxel index fields;
        *   `current_v2_fixed_only`: derived from `current_v2`, keeping only the fixed-size
            columns;
        *   `current_incomplete`: a file whose writer was not finalised, with few products. The
            library refuses to open it unless told to.
    *   The **frozen cases** are never rewritten:
        *   `v2root`: written by v0.16.3 (`a9ad6c63`), a format_version 2 file with its products at
            the file root;
        *   `v0.11.1_v1`: the oldest supported writer, v0.11.1 (`b78a6d64`).
    *   **Renames.** The current cases are renamed once from `v1.4.0_*` (`v1.4.0_v1` becomes
        `current_v1`, and so on); `incomplete` becomes `current_incomplete`.
    *   **Exotic cases.** `current_exotic_v1` and `current_exotic_v2` test the decoder on non-object
        products. They are written only to a scratch folder, and are not committed unless
        `--with-exotic` (kept) is given.
*   **How the golden test finds the cases.**
    *   It lists `tests/data/fixtures/*.expected.json` and reads each case's flags (layout, lite,
        complete, writer version) from the file itself.
    *   The tests that open every case skip those whose `writer.complete` is `false`, which today is
        `current_incomplete`. A `null` value, as in `v0.11.1_v1`, is not skipped. That file keeps
        its own test, which opens it by name.
    *   A case's role comes from its name: `current_*` or frozen.
    *   `test_reader.cpp` and `test_reader_locking.cpp` use the role names.
*   **The manifest.** `tests/data/fixtures/manifest.json` records, for people, each case's SPINE
    tag and commit SHA. No test reads it.
    *   It is created once by `make_fixtures.py --manifest-only`, which records the existing files
        without rewriting them.
    *   Every later write merges: entries of frozen cases are carried over from the existing
        manifest, and entries of the cases written by the run are replaced.
*   **`make_fixtures.py --tag vX.Y.Z --sha SHA --schema PATH`** writes the current set with that
    tag's writer.
    *   The default tag and schema are the model's. With `--sha`, the tag is not resolved again.
    *   Tags are resolved and cloned by a new module, `tools/fixtures/spine_checkout.py`. It holds
        `resolve_tag()`, which applies the rule of S1, and `ensure_clone()`, moved from
        `make_fixtures.py:71-79`. It imports neither h5py nor numpy, so `follow_spine.py` can use it
        before S0 has checked them. Clones go into folders named after the commit SHA, so a moved
        tag never reuses old code.
    *   Frozen cases are written only with `--frozen`, to reproduce how they were made.
    *   The cross-checks (V1 against V2, lite against full, fixed-only against full) key on role
        names, so they run for every tag. A cross-check whose pair of cases is missing is a
        failure.
    *   **Partial runs.** `--only` and `--no-reader-check` stay, for development. A run that uses
        either is marked `partial: true` in the manifest and on standard output, and
        `follow_spine.py` never passes them.
    *   The comparison with SPINE's own reader uses the writing tag's reader. A fallback to the
        current tag's reader is kept only for `v0.11.1_v1`, whose reader cannot resolve
        `ImageMeta3D`.
    *   The list of fields lite mode drops comes from the schema's `lite_dropped`, not from a
        constant.
    *   `--artifacts-dir` keeps the `.h5` files, the decoded JSON, the reader output and the logs
        that are deleted today.
*   **What counts as a change in content (D10).**
    *   The decoded document includes the writer's SPINE version, and that counts as content.
        Only the case name is ignored.
    *   After a `changed` run, the whole current set is rewritten by the new tag. After an
        `identical` run, nothing is committed. Frozen cases are never rewritten.
    *   The saving comes from committing nothing on identical tags (4 of the 5 recent pairs) and
        from the frozen cases.
*   **Enum values come from the tag, for the current set only.**
    *   For current cases, `spine_worker.py` passes the tag's own members of the checked enums
        (`ParticleShape`, `ParticlePID`, `NuCurrentType`, `NuInteractionScheme`) to
        `content.configure()`.
    *   A label the content wants that the tag lacks is written as `UNKNOWN` and listed as a
        substitution. Every checked enum defines `UNKNOWN` from v1.2.1 on.
    *   After substitution, a written value that the file's enum map does not name is a failure.
        A label the file defines that the content never writes is a warning.
    *   Frozen cases keep today's constants, so `--frozen` reproduces `v2root`, which stores
        `interaction_scheme` values 2 and 3 under a three-label enum.
    *   Substitutions and warnings go into the manifest, `status.json` and the classification's
        follow-up list.
    *   This is what lets v1.2.1 and v1.2.2, which lack the newer neutrino schemes, be written at
        all. v1.2.3 already works today.

### 4. The dumper records what D2 and A11 need (G5)

*   **New option and keys.** `dump_schema.py` gains `--spine-sha`, and the schema JSON gains:
    *   `spine_sha`;
    *   `writer_format`, read with `getattr(HDF5Writer, name, None)` so that a missing attribute
        is recorded as `null` rather than raising:
        *   `format_name`, `legacy_format_version`, `current_format_version` and
            `supported_format_versions` (`writer.py:67-70` at v1.4.0);
        *   `default_format_version`, the default of the `format_version` parameter of
            `HDF5Writer.__init__`, or `null` when the parameter does not exist.

        This is how a change of the writer's default layout is detected.
    *   `warnings`, a list of objects `{"kind": ..., "name": ...}`:
        *   `unmapped_attribute`: a `FieldMetadata` attribute outside the mapped set. The mapped
            set is the nine `FLAG_NAMES`, plus `return_type`, `length`, `dtype`, `units`, `enum`,
            `reference`, `reference_space` and `skip`. At v1.4.0 the only unmapped attribute is
            `cat`.
        *   `unlisted_class`: a class in `vars(spine.data)` that has an `attr_metadata` method and
            is in neither `DEFAULT_CLASSES` nor a new `EXCLUDED_CLASSES` list.

        The classifier compares warnings with the baseline's by the pair (kind, name).
*   **Errors.**
    *   Any exception while importing or reading SPINE is reported as one line,
        `dump_schema: <exception class>: <message>`, for example `dump_schema: AttributeError:
        ...` or `dump_schema: RuntimeError: ...`. The dumper then exits 1, as for strict type
        mismatches.
    *   `follow_spine.py` runs the dumper without `--strict`, maps any non-zero exit to
        `dump_failed`, takes the exception class as the sub-reason, and compares `type_check` in
        S3.
*   **The committed schema.** The 1.4.0 schema is re-dumped once with the new keys. `generate.py`
    ignores them, so the generated C++ does not change.
*   **Replay baselines.** The baselines used by replays and tests, the dumps of v1.2.2
    (`d4de053f`) and v1.2.3 (`f745fef3`), live under `tools/release/tests/data/`. Git already
    keeps every committed schema.

### 5. The classifier (G5)

`tools/release/classify.py` describes a change. It never decides "identical"; that is S3.

*   **Runs.**
    *   It runs at S5, without `--effects`.
    *   If S6 is reached, it runs again with `--effects` and overwrites the output.
*   **Inputs:**
    *   the baseline and new schemas;
    *   optionally, the committed `current_*.expected.json` (old columns) and the new tag's
        `expected.json` from the artifacts (new columns), both as folders. Without them, there is
        no column diff;
    *   optionally, `--warnings FILE`: a JSON list of the warnings of the new schema, the
        manifest's warnings and its substitutions;
    *   optionally, `--effects FILE`: the text the export's `spine-dump` prints for HEAD's
        previous `current_v1` and `current_v2`, embedded verbatim.
*   **Output keys** (JSON; the Markdown shows the same content):
    *   `baseline` and `new`: `{path, spine_version, spine_sha}`;
    *   `classes`: per class, `added` (name, position, storage, element, length), `removed`,
        `retyped` (from, to, `shape_changed`), `reordered`, and `changed` (key, from, to) for
        default, units, flags, reference and doc;
    *   `classes_added`, `classes_removed`;
    *   `enums`: per enum, `added`, `removed`, `renumbered` and `aliases`; plus `enums_added`,
        `enums_removed` and `scheme_tables_changed`;
    *   `type_check_changed`, `writer_format_changed`, `module_only`;
    *   `column_diffs`, which is where stored-width drift shows;
    *   `warnings_new`, the warnings that are new relative to the baseline;
    *   `default_only_fields`, `unwritten_classes`;
    *   `effects_on_previous_files`, the `--effects` text;
    *   `proposed_bump` (`none`, `patch`, `minor` or `person`), `reasons` (the rows of the bump
        table that matched), `follow_up` (the follow-up list) and `needs_attention`.
*   **The proposed bump**, from a table that covers every kind; a unit test checks that every
    classifier key has a row. When several kinds match, the highest wins, in the order none <
    patch < minor < person.

| Change kind | Proposed bump |
| --- | --- |
| nothing (identical) | none |
| comments and descriptor doc strings only (module path, field docs) | none: the regenerated model may merge without a tag. An emptied doc fails `check_structure` (`test_model.cpp:113`), which gives `test_failed` |
| fields, enum members, enums or scheme tables added | patch (provisional until D6) |
| removal of a field, enum member or class; shape or element retype; renumbered enum member | minor |
| a stored width changed in the decoded columns (stored-width drift) | a person decides |
| default, unit, flags, reference, `type_check`, `writer_format`, class added, field reordering | a person decides |
| any kind not listed above | a person decides |

A minor release is how a 0.x library signals a breaking change under `SameMinorVersion`:
`find_package(SpineReader 0.1)` accepts 0.1.z only. The Markdown ends with the checklist of
`tools/gen/README.md` ("What to check afterwards"), whose step 4 pull request 4 rewrites to match
these tests.

### 6. The entry point (G6)

`tools/release/follow_spine.py` is an executable Python script (`#!/usr/bin/env python3`, mode
100755). The caller sets up the LCG_110 view (Python 3.10 or newer is needed for SPINE). The
caller also needs git, network access to GitHub, CMake 3.21 or newer (for `ctest --output-junit`)
and a C++ compiler.

| Exit code | Outcomes (`status.json` `outcome`) | Meaning |
| --- | --- | --- |
| 0 | `identical` | identical (D2); the new tag's files read correctly |
| 10 | `changed` (with `contracts_failed` true or false) | the schema changed; the regenerated copy builds; only the contract tests may fail |
| 20 | `not_followed` | a pre-release tag, or a tag below v1.2.1 |
| 30 | `environment_error`, `tag_moved`, `dump_failed`, `version_mismatch`, `generate_failed`, `fixture_tooling_failed`, `fixture_check_failed`, `build_failed`, `test_failed`, `internal_error` | something needs a person; `failed_stage` names the stage |
| 2 | `usage_error` | bad arguments or tag form, a tag that does not exist (other than the model's own), or `--apply` on a dirty tree, with a replay or with an older tag; no `status.json` |

*   **`status.json`** records:
    *   `status_format_version` (1);
    *   the tag, SHA and `__version__`, and the baseline;
    *   `outcome`, `failed_stage`, `contracts_failed`, `needs_attention`, `replay` and `partial`
        (always false for `follow_spine.py`);
    *   `stages`: one entry per stage, named `s0_checks`, `s1_tag`, `s2_dump`, `s3_compare`,
        `s4_fixtures`, `s5_classify`, `s6_build`, `s6_test` or `s8_apply`, each with its status,
        duration and log path;
    *   the warnings and substitutions;
    *   the tool versions (Python, numpy, h5py, HDF5, CMake, compiler; finding A19);
    *   the spine-reader commit, and the command that reproduces the run.
*   **Where it works.**
    *   `--repo-root` names the spine-reader checkout. The default is the repository the script
        lives in.
    *   The artifacts, the export and its build go into a new temporary folder outside the
        repository, whose path is printed. `--artifacts-dir` overrides it.
    *   `--workdir` holds the SPINE clones. The default is the one `make_fixtures.py` uses today
        (`$TMPDIR/spine-reader-fixtures`).
    *   `--spine-repo` (default SPINE's GitHub URL), `--workdir` and `--jobs N` (parallel build
        jobs; default the number of CPUs) are passed on to `make_fixtures.py` and CMake.
*   **Code the unit tests call**, so that they need neither git, nor the network, nor SPINE:
    *   `check_usage(args, repo)`: the checks of S0 that need no network;
    *   `compare_schemas(baseline, new)`: S3;
    *   `outcome_for(stage_results)`: the outcome and exit code.
*   **Replay.** With a non-default `--baseline`, or a tag older than the model's, the run stops
    after S5. It has classified the change and checked the fixtures, and builds and applies
    nothing. `status.json` says `replay: true`.
*   **`--apply`.**
    *   It is checked in S0, before any network access.
    *   After a `changed` outcome, it writes into the working tree what S6 overlaid:
        *   the new schema, replacing the old `schema/spine-*.json` (git keeps the old one);
        *   the regenerated model, through `generate.py --root <repo>`, which already deletes the
            files the generator no longer writes (`generate.py:895-910`);
        *   `tests/golden_members.inc`, through the repository's own `make_member_list.py`;
        *   the current set and the merged manifest.
    *   It does not edit `project(VERSION)`, and does not commit. It prints the proposed bump and
        the follow-up list.
    *   The tree must be clean before it runs, so `git checkout . && git clean -fd` undoes it.

### 7. CI for spine-reader (G9, stage 1)

`.github/workflows/ci.yml` runs on every push (branches and tags), every pull request and on
manual dispatch, with `permissions: contents: read` and no secrets.

*   **Actions and steps.**
    *   CVMFS is mounted with the `cvmfs-contrib/github-action-cvmfs` action, for `sft.cern.ch`.
        That action, `actions/checkout` and `actions/upload-artifact` are each pinned by commit
        SHA.
    *   Each job sources its LCG view and runs configure, build and ctest in the same `run:` step,
        because every step starts a new shell.
    *   Both jobs configure with `SPINE_READER_REQUIRE_PYTHON=ON`, so a missing Python fails
        instead of silently dropping the Python-based tests (`tests/CMakeLists.txt:33-45`).
    *   The workflow lands in pull request 1. Its `follow_spine` step is added in pull request 6.
*   **Job `hdf5-1.14`** on `ubuntu-24.04`:
    *   sources the LCG_110 `x86_64-ubuntu2404-gcc13-opt` view (HDF5 1.14.6, gcc 13 from the
        runner, CMake 3.30, Python 3.13);
    *   configures, builds and runs ctest;
    *   from pull request 6 on, runs `tools/release/follow_spine.py v<model version>` and requires
        exit 0. That is the stage 0 exit criterion, checked on every push from then on. The model
        version comes from the committed schema. The tag must resolve to the schema's `spine_sha`
        (S1), so CI only ever runs that recorded SPINE commit;
    *   uploads the artifacts when it fails.
*   **Job `hdf5-1.12`** on `ubuntu-22.04`:
    *   sources the LCG_105 `x86_64-ubuntu2204-gcc11-opt` view (HDF5 1.12.2, gcc 11 from the
        runner, CMake 3.26, Python 3.9);
    *   configures, builds and runs ctest;
    *   `generate.py --check` and `make_member_list.py --check` both run under Python 3.9.
        The release-tool unit tests are registered only for Python 3.10 or newer, so they run in
        the 1.14 job.
*   **Tag pushes.** On a tag push, the 1.14 job also runs `ci/check_tag_version.sh` as a backstop
    (part 1).
*   **The dry run, done on 2026-11-05.** The LCG views for Ubuntu use the system compiler, so
    both jobs were reproduced locally in CERN's SFT build images on CVMFS
    (`/cvmfs/unpacked.cern.ch/gitlab-registry.cern.ch/sft/docker/ubuntu22:latest` and
    `ubuntu24:latest`), with apptainer. On main at 6e2d9ee:
    *   LCG_105 with gcc 11.4: the build succeeds, and every ctest entry passes;
    *   LCG_110 with gcc 13.3: the build succeeds, and every ctest entry passes;
    *   under LCG_110, a fresh dump of SPINE v1.4.0 is byte-identical to the committed schema.

    `ci/EGM.md` documents the command, so that the dry run can be repeated before any change to
    the workflow. GitHub's runner images are not the SFT images; the first CI run is the final
    check.

## Alternatives Considered

Alternatives A to H were weighed in the design brief. The rest come from drafting and from the
design review.

### Alternative A: Release spine-reader at every SPINE tag

*   **Pros**: The model's SPINE version always matches the newest SPINE. There is no separate
    record to keep.
*   **Cons**: Four of the five recent pairs of final tags were identical (D2). Each such release
    would change only version strings, and the CAFMakers would update their pins for nothing.
*   **Reason for Rejection**: Rejected by author: releases without content. An identical tag is
    checked (and recorded in stage 2) instead.

### Alternative B: Check every tag but record nothing

*   **Pros**: Needs no write access, and nothing beyond the check to maintain.
*   **Cons**: The only trace is in Actions logs, kept 90 days by default. Nothing answers "which
    spine-reader reads files from SPINE Y" when a CAFMaker picks a version to pin.
*   **Reason for Rejection**: Rejected by author: the author reports that CAFMaker users need a
    lasting record of which spine-reader version a given SPINE version's files require.

### Alternative C: Carry the version in the git tag alone, or in a pull-request label

*   **Pros**: No CMake edit per release; the tag is the release.
*   **Cons**:
    *   `find_package` and `SameMinorVersion` read only `project(VERSION)`
        (`CMakeLists.txt:2, 66-67`).
    *   A tag `v0.2.0` on a tree that says 0.1.0 installs a package that
        `find_package(SpineReader 0.1)` accepts, which defeats a minor bump.
    *   A label is a third source that can disagree with both.
*   **Reason for Rejection**: Rejected by author after the design review (D1): `project(VERSION)`
    is the only source, tags are made from it, and they are checked before and after pushing.

### Alternative D: The reviewer edits the hand-written counts in each schema pull request

*   **Pros**: No new test code. The person who merges has looked at every count.
*   **Cons**: The counts only detect that the schema changed (see The Problem). Every
    schema-change pull request is red until edited, so a red run says nothing about real
    failures. SPINE maintainers would end up editing C++ test files.
*   **Reason for Rejection**: Rejected by author after the design review (D4): replaced by the check
    against the files' columns plus the consumer contract tests.

### Alternative E: The bot rewrites the counts from the new schema

*   **Pros**: The pull request is green with no person editing tests.
*   **Cons**: A count computed from the same schema the model is generated from tests nothing.
*   **Reason for Rejection**: Rejected by author: the counts become tautological.

### Alternative F: Decide "changed" from a raw diff of the schema JSON

*   **Pros**: Simple; the prototype `cmp.py` exists.
*   **Cons**:
    *   It reports every byte difference, including a module move that changes only header
        comments, without saying which kind of change it is.
    *   The prototype also ignores `type_check` and `writer`, cannot tell an added enum member
        from a renumbered one, and always exits 0.
*   **Reason for Rejection**: Rejected by author after the design review (D2):
    *   "Identical" is decided by comparing generated code, plus `type_check` and
        `writer_format`.
    *   Both methods detect a module move. The difference is that the classifier then says what
        kind of change it is, and proposes no release for a comment-only change.
    *   The classifier starts from `cmp.py`.

### Alternative G: Regenerate and commit every fixture on every run

*   **Pros**: The fixtures always come from the newest run, with no comparison logic.
*   **Cons**: Fixture bytes are not reproducible: the same tag gives different `.h5` bytes with the
    same decoded content. Rewriting on every run would commit about 1.8 MB per run, which git
    keeps forever.
*   **Reason for Rejection**: Rejected by author after the design review (D10):
    *   Nothing is committed on identical tags.
    *   Frozen cases are never rewritten.
    *   The current set is rewritten only on a schema change.
    *   Role names remove the per-tag renames in tests.

### Alternative H: Trigger from SPINE's release workflow first

*   **Pros**:
    *   There is no delay between a SPINE release and the check.
    *   The author reports that the SPINE maintainers agreed to take part (no public record
        yet).
*   **Cons**:
    *   GitHub's documentation for `repository_dispatch` requires a token with write access to the
        target repository's contents. That token would be stored in SPINE.
    *   The job would also run SPINE code with unpinned dependencies while holding write tokens.
*   **Reason for Rejection**: Rejected by author after the design review (D5): a scheduled scan from
    spine-reader comes first (stage 2), and the dispatch may come later as a hint only.

### Alternative I: Compare element widths strictly in the column check

*   **Pros**: Catches every difference between the C++ member type and the stored type.
*   **Cons**: Widths vary with the data (see part 2), and four fields already differ in the
    committed fixtures, so the test would fail on the current files.
*   **Reason for Rejection**: Rejected by author: kind and shape are compared strictly, widths are
    printed, and stored-width drift between tags is shown by the classifier's column diff.

### Alternative J: Consumer contract tests that also catch retypes

*   **Pros**: A retype of a field a CAFMaker reads would also end as `changed` with
    `contracts_failed`, rather than as a failure.
*   **Cons**: It would need all of these:
    *   the frozen fixtures checked at core level only;
    *   every exact-type comparison outside the contracts rewritten (`test_model.cpp:531, 539,
        565`; `test_reader.cpp:492-495`; `examples/spine_dump.cpp:51-53`);
    *   a separate contracts target, and a keep-going build that maps "only that target failed".
*   **Reason for Rejection**: Rejected by author in the design review, which chose option (a):
    units, defaults and enum values only. Retypes are rare, and the classifier already marks each
    one "minor". Listed as a future improvement if that changes.

### Alternative K: No consumer contract outcome

*   **Pros**: Simplest. The contract tests become ordinary tests, and the proposed bump is the
    only "a person must look" signal.
*   **Cons**: A unit or default change compiles cleanly and silently changes CAF values (SBN and FD
    keep defaults for missing fields, FD converts MeV to GeV). It deserves a test-based signal.
*   **Reason for Rejection**: Rejected by author in the design review: option (a) keeps that signal
    for little code.

### Alternative L: Entry point that only checks, always applies, or writes a patch file

*   **Pros**:
    *   Check only never touches the working tree.
    *   Always apply is the least code.
    *   A patch file (`git diff --binary` of HEAD against the overlaid export) needs no copy
        logic.
*   **Cons**:
    *   Check only leaves the copy to a person.
    *   Always apply edits the tree on every schema-change run.
    *   A patch file needs a second command, and the refusal of a dirty tree moves to `git apply`.
*   **Reason for Rejection**: Rejected by author, and confirmed in the design review: check by
    default, and `--apply` on request, only on a clean tree.

### Alternative M: CI in a built AlmaLinux 9 image, or with conda-forge

*   **Pros**:
    *   A built image is self-contained (`hdf5-devel` 1.12.1 from dnf, HDF5 1.14 from source).
    *   conda-forge offers HDF5 1.12 and 1.14 as packages, with no image and no CERN dependency.
*   **Cons**:
    *   An image or a cached HDF5 build must be maintained.
    *   conda-forge's compilers and HDF5 builds match nothing used in production.
*   **Reason for Rejection**: Rejected by author. The chosen views come from the same LCG releases
    as development, but in their Ubuntu builds: development uses `x86_64-el9-gcc13-opt`. The dry
    run of part 7 shows that the difference does not change the build, the tests or the dump.

### Alternative N: An EL9 container with the runner's CVMFS

*   **Pros**:
    *   The same EL9 views as development: LCG_110 `x86_64-el9-gcc13-opt` (HDF5 1.14.6) and
        LCG_105 `x86_64-el9-gcc13-opt` (HDF5 1.12.2).
    *   It removes the dependency on `ubuntu-22.04`.
*   **Cons**:
    *   It adds a container layer.
    *   Bind-mounting the runner's CVMFS into a job container on GitHub-hosted runners is
        unconfirmed.
*   **Reason for Rejection**: Rejected by author in the design review, for now. It is the planned
    fallback if risk R4 happens, or if R3 ever does.

### Alternative O: Stock Ubuntu runners with apt's HDF5, or SPINE's published image

*   **Pros**: No setup step.
*   **Cons**:
    *   Both provide HDF5 1.10.
    *   SPINE's image can take hours to appear after a tag ([docker-publish.yml](https://github.com/DeepLearnPhysics/spine/blob/4726e068f17e433e2fec068dc8ee498fa9a0658f/.github/workflows/docker-publish.yml), `timeout-minutes: 360`).
*   **Reason for Rejection**: `CMakeLists.txt:30` refuses HDF5 1.10, so the build cannot be
    configured.

### Alternative P: The larger stage 0 of the first draft

*   **Pros**: These items are more general, and some would be reused by stage 2:
    *   a separate column-check test with a recorded column order;
    *   a fixture manifest read by C++;
    *   a `schema/history/` archive;
    *   eleven exit codes and a JSON Schema for `status.json`;
    *   a generated version header with macros and a `reader_version` member in `SchemaReport`;
    *   a hand-kept list of ctest entries;
    *   a text-file list of contract fields;
    *   a shell wrapper around the entry point.
*   **Cons**:
    *   Each item repeats existing code (the golden test, `expected.json`, git,
        `generate.generate()`), or fixes an interface before its user exists.
    *   The text list could not check defaults, since descriptors carry none.
    *   None is needed by stage 0's checks.
*   **Reason for Rejection**: Rejected by author after the design review.

### Alternative Q: One hand-written test block per CAFMaker field

*   **Pros**: Each check reads as plain C++, and a removed field fails the build, which is
    impossible to miss.
*   **Cons**:
    *   The CAFMakers read 228 fields (Appendix B), so this means 228 blocks. Most of them would
        repeat the logic of `equal_member()`, which the golden test already has.
    *   A removal would break the build of the whole test binary rather than fail one entry.
*   **Reason for Rejection**: Rejected by author while addressing stage 9: a table checked through
    `golden_members.inc` and `equal_member()` gives the same checks, and reports a removal as a
    contract failure.

### Deferred, not rejected: SPINE exports its own schema (D9)

Moving the schema export into SPINE would remove the dumper's dependence on SPINE internals, and
put the part that breaks with its owners. It would also move spine-reader's choices
(`DEFAULT_CLASSES`, `CLASS_ROLE`) into SPINE before the need is proven. The author keeps the
dumper in spine-reader for now, and revisits this after the next `FieldMetadata` change that
breaks it.

## Risks & Mitigations

| Id | Risk | Impact | Mitigation | Owner or trigger |
| --- | --- | --- | --- | --- |
| R1 | CVMFS (a Stratum-1 server) or GitHub is unreachable | CI goes red for reasons unrelated to the change | in CI, the step that sources the LCG view fails before any spine-reader code runs, and its log names the cause; locally, `environment_error`; re-run | spine-reader owner |
| R2 | The model's SPINE tag is moved or deleted | CI would run code that was never recorded | S1 compares the resolved SHA with the schema's `spine_sha`, and a moved or missing tag gives `tag_moved` before any SPINE code runs | spine-reader owner |
| R3 | The Ubuntu build of LCG_110 dumps the schema differently from the EL9 build the schema was committed with | `type_check` or `writer_format` differ, and CI's `identical` check fails | not seen in the dry run (the dumps are byte-identical); if it ever happens, switch to Alternative N | CI's `identical` check fails under the Ubuntu view |
| R4 | GitHub retires the `ubuntu-22.04` runner | the HDF5 1.12 job is lost | move that job to Alternative N with the LCG_105 EL9 view, or dnf `hdf5-devel` 1.12.1 | runner deprecation notice |
| R5 | An LCG view is removed from CVMFS | CI configuration fails | update the view names, and repeat the dry run | CI failure |
| R6 | A stored width changes, and HDF5 converts it silently on read (int64 to int32, float64 to float32) | silent loss of range or precision | the column check compares kind only, so the classifier's column diff reports the width change, and the bump table marks it "a person decides" | classification |
| R7 | SPINE retires `format_version=1`, or renames writer parameters | every tag ends in `fixture_tooling_failed` | keep the last V1 `current_v1` as a frozen case, and update the worker | first failure |
| R8 | A `FieldMetadata` change breaks the dumper | `dump_failed` | fix the dumper; revisit D9 | first failure |
| R9 | Classes no fixture writes (`FlashHypothesis`), new fields exercised only at their default, and defaults of fields no CAFMaker reads | errors in those fields go unseen | listed in every classification; default changes are marked "a person decides"; Future Improvements | classification |
| R10 | A retype, a shape change, a swap of enum values, or the removal of a member that test code names ends as a failure, not `changed` | stage 2 would open an issue instead of a pull request | the classification is still written, and the change is marked minor | stage 2 design |
| R11 | `--apply` fails halfway | a partly written tree | the tree must be clean before `--apply` runs; `git checkout . && git clean -fd` restores it | the person running it |
| R12 | The contract table goes stale when a CAFMaker starts reading a new field, or when the open CAFMaker pull requests change before they merge | a unit or default change on that field gives no signal, which is the silent CAF change Alternative K was rejected for | `scan_consumer_fields.py` lists the fields a CAFMaker build reads that have no row; run it when a CAFMaker changes what it reads, and once after the three pull requests merge | the spine-reader owner, until D8 assigns owners; trigger: a CAFMaker change that reads a new SPINE field |

## Non-Functional Requirements

### Security

*   The CI workflow has `permissions: contents: read`, uses no secrets, and pins every action by
    commit SHA.
*   The only SPINE code CI runs is the model's own tag, and only when it resolves to the commit
    SHA recorded in the schema.
*   `follow_spine.py` runs SPINE code from the tag it is given, in the user's environment. Running
    it on an unknown tag is the same trust decision as `pip install` of that SPINE release.
*   Tags are checked against the forms of S0 before any use. They are passed to subprocesses as
    arguments, never through a shell string.

### Performance

Measured on the development machine (LCG_110 EL9 view, 48 cores):

*   a schema dump takes a few seconds;
*   two fixtures take 17 s, including the SPINE clone;
*   a build takes 41-50 s;
*   ctest takes a few seconds.

A full run on a 4-core GitHub runner is estimated, not measured, at 3 to 6 minutes, scaled from
those numbers, plus up to 2 minutes for the first CVMFS access. The 1.14 job builds twice, once
for ctest and once in `follow_spine`'s export; this is accepted. The first CI run will give the
real figure. Nothing here runs in the library's read path.

### Compatibility

*   `SchemaReport` keeps its layout. The library gains one function, `spine::version()`, and one
    C++17 header.
*   The first line of `summary()` changes. The CAFMakers' logs show the new text. SBN's A/B
    comparison script (`ab_report.py`), which compares standard output with a baseline, needs a
    new baseline once.
*   No tagged release exists yet. Builds pinned to a commit before pull request 2 are unaffected
    until they update.

### Rollout and rollback

*   The work lands as six pull requests, in the order of the Detailed Implementation Plan. CI
    comes first, so every later pull request is checked by it.
*   The author tags `v0.1.0` on the merge commit of pull request 2, after CI passes on it and
    after `ci/check_tag_version.sh v0.1.0` passes locally.
*   Rollback is a revert, latest first: pull request 4 depends on 3, and the CI step of 6 depends
    on 5. A published tag is never moved or deleted: a bad release is followed by a fixed one and
    marked withdrawn in the README.
*   The fixture renames use `git mv`, so their history is kept.

## Additional Resources

*   The design brief and the findings note: the author's local notes, not in the repository. They
    hold the measured schema history and the regeneration simulations. Appendix A summarises what
    this document uses from them.
*   `tagdump/` and `fieldscan/`, also local: per-tag schema dumps, failure logs, `cmp.py`,
    `sim/mutate.py`, the CAFMaker field scan scripts, and the CI dry-run script and logs.
*   The current manual procedure: `tools/gen/README.md` ("Regenerate for a new SPINE release",
    "What to check afterwards") and `tools/fixtures/README.md`.
*   SPINE at v1.4.0:
    *   [publish.yml](https://github.com/DeepLearnPhysics/spine/blob/4726e068f17e433e2fec068dc8ee498fa9a0658f/.github/workflows/publish.yml);
    *   [pyproject.toml](https://github.com/DeepLearnPhysics/spine/blob/4726e068f17e433e2fec068dc8ee498fa9a0658f/pyproject.toml);
    *   [field.py](https://github.com/DeepLearnPhysics/spine/blob/4726e068f17e433e2fec068dc8ee498fa9a0658f/src/spine/data/field.py);
    *   [writer.py](https://github.com/DeepLearnPhysics/spine/blob/4726e068f17e433e2fec068dc8ee498fa9a0658f/src/spine/io/write/hdf5/writer.py).

## Future Improvements

*   **Stages 2 to 4** (design brief):
    *   a scheduled scan of SPINE's tags, first in shadow mode;
    *   then with writes: records, bot pull requests, and tags after CI passes on the merge
        commit;
    *   then the optional SPINE-side pieces: a non-blocking schema diff in SPINE's pull requests,
        and a dispatch used only as a hint.

    Each needs the open decisions D3, D6, D7, D8 and D11, and the JSON Schema for `status.json`.
*   **`spine-dump --check`:** exit non-zero when a file has missing, mismatched or not-in-model
    fields or unknown enum labels, so a CAFMaker can test a build against a file in one command
    (A21).
*   **Fixtures that test new fields beyond their default:** the worker fills the fields
    `content.py` does not know with values other than the default, and a `FlashHypothesis` product
    is written (A17).
*   **A lint test on the `EGM.md` contracts** that fails when one names a literal model version
    (A24).
*   **Shared-library policy:** set a SOVERSION, or document "rebuild on every update" (A13).
*   **Alternative N**, if risk R3 or R4 happens.
*   **Consumer contract tests that also catch retypes** (Alternative J), if retypes become
    frequent.
*   **`scan_consumer_fields.py` in the CAFMakers' own CI**, so that a CAFMaker pull request that
    reads a new field reminds its author to add a contract row.

--------------------------------------------------------------------------------

## Detailed Implementation Plan

Paths are relative to the spine-reader repository root. The work is six pull requests, in this
order. Each one leaves every ctest entry green, and from pull request 1 on, CI checks that.
Proposed changes are written in the present tense.

### Pull request 1: CI without the `follow_spine` step (Technical Plan part 7)

#### 1. `.github/workflows/ci.yml` (Create)

*   **Type / Scope**: GitHub Actions workflow.
*   **Proposed Changes**:
    *   Triggers: `push` (branches and tags), `pull_request`, `workflow_dispatch`. Permissions:
        `contents: read`; no secrets.
    *   `actions/checkout`, `cvmfs-contrib/github-action-cvmfs` (with
        `cvmfs_repositories: sft.cern.ch`) and `actions/upload-artifact` are each pinned by
        commit SHA, with the version in a comment.
    *   Job `hdf5-1.14` on `ubuntu-24.04`: one `run:` step that sources the LCG_110
        `x86_64-ubuntu2404-gcc13-opt` view, configures with `-DSPINE_READER_REQUIRE_PYTHON=ON`,
        builds, and runs `ctest --test-dir build --output-on-failure`. On a tag push
        (`startsWith(github.ref, 'refs/tags/')`), a second step runs
        `ci/check_tag_version.sh "$GITHUB_REF_NAME"`.
    *   Job `hdf5-1.12` on `ubuntu-22.04`: the same `run:` step with the LCG_105
        `x86_64-ubuntu2204-gcc11-opt` view.

#### 2. `ci/check_tag_version.sh` (Create, mode 100755)

*   **Type / Scope**: Release helper, run locally before pushing a tag, and in CI.
*   **Proposed Changes**: reads `project(spine-reader VERSION X.Y.Z ...)` from `CMakeLists.txt`,
    and exits 1 with a message naming both versions unless its argument equals `vX.Y.Z`.

#### 3. `tests/CMakeLists.txt` (Modify)

*   **Type / Scope**: Test registration.
*   **Proposed Changes**: `option(SPINE_READER_REQUIRE_PYTHON "fail if Python 3 is missing" OFF)`.
    When it is on, `find_package(Python3 REQUIRED COMPONENTS Interpreter)` replaces the `QUIET`
    form at line 33.

#### 4. `ci/EGM.md`, `EGM.md` and `README.md` (Modify)

*   **Type / Scope**: `EGM.md` contracts and documentation.
*   **Proposed Changes**:
    *   `ci/EGM.md` documents the workflow and `check_tag_version.sh`. It also documents the local
        dry run of both jobs: the apptainer command, the SFT Ubuntu images, and the view to
        source in each.
    *   Root `EGM.md`: the list of the root folder's contents (28-29) gains `.github/`, and
        "Global Side Effects" gains "CI runs on GitHub with read-only permissions".
    *   The README's "Tests" section mentions CI and `SPINE_READER_REQUIRE_PYTHON`.

### Pull request 2: the library's own version (part 1)

#### 5. `include/spine/version.hpp` (Create)

*   **Type / Scope**: Public header, hand-written, C++17.
*   **Proposed Changes**: declares `namespace spine { const char* version() noexcept; }`, with the
    comment "release number of this spine-reader library (project(VERSION)); between releases,
    builds of main report the last release". It is installed with the other headers by the
    existing `install(DIRECTORY include/spine ...)`.

#### 6. `src/version.cpp` (Create)

*   **Type / Scope**: Library source.
*   **Proposed Changes**: defines `spine::version()`, returning `SPINE_READER_VERSION_STRING`.
    The existing `GLOB` of `src/*.cpp` picks it up.

#### 7. `CMakeLists.txt` (Modify)

*   **Type / Scope**: Build configuration.
*   **Proposed Changes**:
    *   `target_compile_definitions(spine_reader PRIVATE
        SPINE_READER_VERSION_STRING="${PROJECT_VERSION}")`.
    *   `project(spine-reader VERSION 0.1.0 ...)` stays as it is.

#### 8. `include/spine/reader.hpp` (Modify)

*   **Type / Scope**: Public header.
*   **Proposed Changes**: includes `<spine/version.hpp>`.

#### 9. `src/schema_report.cpp` (Modify)

*   **Type / Scope**: Library source.
*   **Proposed Changes**: the first line of `summary()` after the path becomes `SPINE <file
    version or "unknown">, format_version <n> (<layout>); read by spine-reader
    <spine::version()> (model from SPINE <model version>)`. The product lines are unchanged.

#### 10. `examples/spine_dump.cpp` (Modify)

*   **Type / Scope**: Command-line tool.
*   **Proposed Changes**: `--version` prints `spine-dump: spine-reader <spine::version()> (model
    from SPINE <model version>)` and exits 0, whatever other arguments are given. The usage line
    lists `--version`.

#### 11. `tests/test_reader.cpp` (Modify)

*   **Type / Scope**: Unit test.
*   **Proposed Changes**: the expected summary lines at 560-561 and 569-570 use the new wording,
    built from `spine::version()` and `SPINE_READER_MODEL_SPINE_VERSION`. The file versions
    ("SPINE 1.4.0", "SPINE unknown") stay literal.

#### 12. `tests/CMakeLists.txt` and `tests/check_spine_dump.cmake` (Modify)

*   **Type / Scope**: Test registration and test script.
*   **Proposed Changes**:
    *   `tests/CMakeLists.txt` reads the model version from `include/spine/model.hpp`:
        *   `file(STRINGS ... REGEX "#define SPINE_READER_MODEL_SPINE_VERSION")`, then
            `string(REGEX REPLACE ".*\"([^\"]*)\".*" "\\1" ...)`;
        *   `model.hpp` is added to `CMAKE_CONFIGURE_DEPENDS`, so that a regenerated model
            triggers a new configuration.
    *   It passes `-DREADER_VERSION=${PROJECT_VERSION}` and `-DMODEL_VERSION=<that value>` to
        `check_spine_dump.cmake`.
    *   `check_spine_dump.cmake` lines 34-35 use the new wording with those variables. A new
        `execute_process` call runs `spine-dump --version`, and checks that it prints both versions
        and exits 0. This also checks that the compile definition reaches the library.

#### 13. `README.md`, `EGM.md`, `include/EGM.md`, `include/spine/EGM.md`, `src/EGM.md`, `examples/EGM.md` (Modify)

*   **Type / Scope**: Documentation and `EGM.md` contracts.
*   **Proposed Changes**:
    *   The README gains a "Versions" section:
        *   a release is a tag `vX.Y.Z` made from `project(VERSION)`, checked with
            `ci/check_tag_version.sh` before pushing;
        *   `spine::version()` and `spine-dump --version` print it;
        *   `find_package(SpineReader X.Y)` accepts any X.Y.z (`SameMinorVersion`);
        *   between releases, builds of main report the last release;
        *   withdrawn tags, if any, are listed there.
    *   `README.md:6` stays. It describes the writer layouts SPINE uses, not the model version.
    *   `include/EGM.md:17` and `include/spine/EGM.md:48` lose their literal model versions.
    *   Per file:
        *   `include/spine/EGM.md` lists `version.hpp` and `spine::version()`;
        *   `src/EGM.md` lists `src/version.cpp` and the new first line of `summary()`;
        *   `examples/EGM.md` lists `--version`;
        *   root `EGM.md` lists the version as part of the public interface.

After this pull request merges and CI passes on the merge commit, the author runs
`ci/check_tag_version.sh v0.1.0` and pushes the tag `v0.1.0`.

### Pull request 3: fixture tooling for any tag (part 3)

#### 14. `tools/fixtures/spine_checkout.py` (Create)

*   **Type / Scope**: Python module without h5py or numpy.
*   **Proposed Changes**:
    *   `resolve_tag(repo, tag)`: runs `git ls-remote --tags <repo> <tag> <tag>^{}`, and returns
        the `^{}` commit when listed, else the plain one. It raises a distinct error when
        `ls-remote` fails, and another when the tag is absent.
    *   `ensure_clone(tag, sha, repo, workdir)`: moved from `make_fixtures.py:71-79`. It clones
        into `<workdir>/spine_<sha>` and checks that `HEAD` is `sha`.

#### 15. `tools/fixtures/make_fixtures.py` (Modify)

*   **Type / Scope**: Fixture generator.
*   **Proposed Changes**:
    *   **Cases.** `CASES` becomes role-named:
        *   `current_v1`, `current_v2`, `current_v1_lite`, `current_v2_lite`,
            `current_v2_fixed_only` (derived from `current_v2`) and `current_incomplete`, plus
            `current_exotic_v1` and `current_exotic_v2` (scratch only unless `--with-exotic`), all
            take the tag from `--tag`;
        *   `v2root` (v0.16.3) and `v0.11.1_v1` keep their tag, and are written only with
            `--frozen`.
    *   **New options.**
        *   `--tag vX.Y.Z`, default "v" plus the committed schema's `spine_version`;
        *   `--sha SHA`, the commit; when given, the tag is not resolved again;
        *   `--schema PATH`, default the committed schema, read for `lite_dropped`;
        *   `--frozen`;
        *   `--artifacts-dir DIR`;
        *   `--manifest PATH`, default `<out>/manifest.json`;
        *   `--manifest-only`, which records the existing files of `--out` without writing any,
            taking each case's flags from its `.expected.json`, and its tag and SHA from
            `--tag`, `--sha` or the frozen table (v0.16.3 `a9ad6c63ce5532429c26b3374ec73c7ecc2f1cea`,
            v0.11.1 `b78a6d64f60ce0be5a24611541df19033620b95a`).
    *   **Checkout.** It uses `spine_checkout.py`.
    *   **Lite list.** `LITE_SKIPPED` (line 61) is replaced by the union of `lite_dropped` over the
        classes in `--schema`.
    *   **Cross-checks** (418-455): they key on role names. A cross-check whose pair of cases is
        missing is a failure. `--only` and `--no-reader-check` stay, but mark the run `partial`
        in the manifest and on standard output.
    *   **Reader comparison** (465-474): it uses only the writing tag's reader. The fallback stays
        for `v0.11.1_v1` only.
    *   **Enum check** (199-204):
        *   a written value that the file's enum map does not name is a failure;
        *   a label the file defines that the content never writes is a warning.
    *   **Manifest.**
        *   Top-level keys: `spine_tag`, `spine_sha`, `spine_version`, `partial`, `warnings` and
            `substitutions`.
        *   Per case: `role`, `tag`, `sha`, `layout`, `lite`, `derived_from`, `h5` and
            `expected`.
        *   Frozen entries are carried over from the existing manifest, and the cases written by
            the run replace theirs.
    *   **Exit codes:**
        *   0: success, with warnings allowed;
        *   1: a check failure;
        *   2: a usage error;
        *   3: the worker could not drive SPINE's API (worker exit 3);
        *   4: any other error, including a worker exit 1.

        `run_worker` (89-90) passes the worker's exit code on instead of turning every failure
        into the same `RuntimeError`.

#### 16. `tools/fixtures/spine_worker.py` (Modify)

*   **Type / Scope**: Subprocess that imports SPINE.
*   **Proposed Changes**:
    *   The `write` subcommand gains `--configure-enums`, which `make_fixtures.py` passes for
        current cases only. With it, the worker collects the members of the checked enums at that
        tag from `spine.constants.enums` and passes them to `content.configure()`. The
        substitutions go to standard error, where `make_fixtures.py` collects them.
    *   Exit code 3 when SPINE's API cannot be driven: an import error, or an unknown writer
        parameter, including the branch at 200-203 ("cannot write format_version"). Exit 1 for
        other errors.

#### 17. `tools/fixtures/content.py` (Modify)

*   **Type / Scope**: SPINE-free fixture content.
*   **Proposed Changes**:
    *   `configure(enums)` takes `{enum name: {label: value}}` for `ParticleShape` (`SHOWER` to
        `LARCV_UNKNOWN`), `ParticlePID` (`PHOTON` to `KAON`), `NuCurrentType` (`CC`, `NC`) and
        `NuInteractionScheme` (`LARSOFT` to `NEUT`). `UNKNOWN` stays -1 in all four.
    *   It rebuilds `PDG` and `MASS` (37-38), which are keyed by PID value.
    *   `chi2_pid=UNKNOWN` (line 79) becomes a `None` default, resolved when the function is
        called.
    *   A label the tag lacks becomes `UNKNOWN`, recorded in a module-level list of
        substitutions.
    *   Without `configure()`, today's values are kept, which is what frozen cases use.

#### 18. `tests/data/fixtures/` (Rename, Modify and Create)

*   **Type / Scope**: Test data.
*   **Proposed Changes**:
    *   `git mv` of these files (`.h5` and `.expected.json`):
        *   `v1.4.0_v1` → `current_v1`;
        *   `v1.4.0_v2` → `current_v2`;
        *   `v1.4.0_v1_lite` → `current_v1_lite`;
        *   `v1.4.0_v2_lite` → `current_v2_lite`;
        *   `v1.4.0_v2_fixed_only` → `current_v2_fixed_only`;
        *   `incomplete` → `current_incomplete`.
    *   The `.h5` bytes do not change. In each renamed `.expected.json`, the `case` value is set to
        the new name.
    *   `manifest.json` is created with `make_fixtures.py --manifest-only --tag v1.4.0 --sha
        4726e068f17e433e2fec068dc8ee498fa9a0658f`.

#### 19. `tests/test_golden.cpp` (Modify)

*   **Type / Scope**: Golden test.
*   **Proposed Changes**:
    *   `kCases` (32-35) is replaced by a listing of `fixtures/*.expected.json` with
        `std::filesystem`. Each case's flags are read from its file, and its role from its name
        prefix.
    *   These test cases skip cases whose `writer.complete` is `false` (not `null`):
        *   the core-level loop (from 155);
        *   "the reader fills every member of every model object as the fixtures say" (305);
        *   "event relations agree with the ids the fixtures hold" (384).
    *   "incomplete files are refused unless allowed" (192-196) opens `current_incomplete` by name.

#### 20. `tests/test_reader.cpp` and `tests/test_reader_locking.cpp` (Modify)

*   **Type / Scope**: Unit tests.
*   **Proposed Changes**: these fixture names become role names:
    *   `test_reader.cpp:587-592`;
    *   `test_reader.cpp:612` (`incomplete.h5` → `current_incomplete.h5`);
    *   `test_reader_locking.cpp:68, 72`.

#### 21. `tools/fixtures/README.md` and `tools/fixtures/EGM.md` (Modify)

*   **Type / Scope**: Documentation and `EGM.md` contract.
*   **Proposed Changes**:
    *   They describe:
        *   the cases and their roles;
        *   the manifest and its merge rule;
        *   the new options, the partial runs and the exit codes;
        *   the enum rule for current and frozen cases;
        *   the D10 policy.
    *   The literal `1.4.0` mentions (22 in the README, and `EGM.md:28-30, 102, 113`) become "the
        model's SPINE tag", except where they describe the frozen cases' history.

### Pull request 4: tests that survive expected changes (part 2)

#### 22. `tests/member_compare.hpp` (Create)

*   **Type / Scope**: Test helper header.
*   **Proposed Changes**: holds `is_std_array`, `is_std_vector` and `equal_member()`, moved from
    `test_golden.cpp:208-247`, and `all_nan()`, moved from `test_model.cpp:50-53`.
    `test_golden.cpp`, `test_model.cpp` and `test_contracts.cpp` include it.

#### 23. `tests/test_model.cpp` (Modify)

*   **Type / Scope**: Unit test.
*   **Proposed Changes**, by line range of the current file:
    *   **1-49, includes and usings:** kept; includes `member_compare.hpp` and
        `golden_members.inc`.
    *   **50-53, `all_nan`:** moved to `member_compare.hpp`.
    *   **55-72, `fixed_column` and `index_of`:** kept.
    *   **74-92, `Last` and `LAST_FIELD`:** removed.
    *   **94-141, `check_structure`:** kept without its `expected_fields` parameter. Its three uses
        of `Last<C>::value` (103, 134, 137) become `num_fields - 1`.
    *   **148-164, the class-list test:** the exact `expected` list is removed. The test keeps
        `CHECK(described == names)` and a uniqueness check.
    *   **166-177, field counts:** each call becomes `check_structure<C>("Name")`.
    *   **179-206, field i and enumerator i:** replaced by one test that walks
        `GOLDEN_MEMBERS_<Class>` for every class, and checks
        `d.fields[static_cast<std::size_t>(C::Field::name)].name == #name`.
    *   **208-271, member types and descriptor types:** kept.
    *   **273-329, flags, units, references and enums:** the flag and reference expectations
        (279-320) are removed. The unit checks are removed, because the contract table checks the
        units of CAFMaker fields. The enumeration-pointer checks (322-328) are kept.
    *   **331-501, defaults:** removed. The contract table checks the defaults of CAFMaker fields.
    *   **503-566, the assign test:** kept, with literals replaced:
        *   `p.interaction_id == -1` (514) becomes `== RecoParticle{}.interaction_id`;
        *   `p.pid == UNKNOWN` (519) becomes `== RecoParticle{}.pid`;
        *   `all_nan(p.end_point)` (532) stays, as `all_nan` of a default object is the default;
        *   `run.run == -1` and `run.event == -1` (556-557) become comparisons with `RunInfo{}`;
        *   the assigned values 1 and 2, expected as `TRACK` and `MUON` (518, 523), are written as
            `static_cast<std::int64_t>(ParticleShape::TRACK)` and
            `static_cast<std::int64_t>(ParticlePID::MUON)`.
    *   **572-614, `check_enum` and the round-trip test:** kept without the member counts
        (605-613).
    *   **616-633, enum values:** moved to item 24.
    *   **635-641, the `NULL_` rename and the `Shape` alias:** kept, as their own test case.
    *   **644-671, scheme tables:** moved to item 24.

#### 24. `tests/test_contracts.cpp` (Create)

*   **Type / Scope**: Consumer contract tests, compiled into the `spine_reader_tests` binary
    through the `test_*.cpp` glob.
*   **Proposed Changes**:
    *   Every test case name starts with `contract: `.
    *   **The table.** The 228 rows of Appendix B, one per field: class, field, unit, default as a
        JSON literal, readers. They are copied once from the companion TSV and SPINE 1.4.0's
        schema, and edited only by hand afterwards. A header comment says so: "What the CAFMakers
        rely on. A failure means a maintainer reviews the SPINE change with the CAFMaker
        maintainers before a release."
    *   **The check.** One test case per class:
        *   it walks `GOLDEN_MEMBERS_<Class>`;
        *   for each member with a row, it checks `descriptor().find(name)->units` against the
            unit, and `equal_member(Class{}.name, json::parse(default))`;
        *   a row whose field the class no longer has fails, with "listed for <readers>, not in
            the model";
        *   a row whose class is not in `SPINE_READER_FOR_EACH_CLASS` fails.
    *   **Moved here:** the enum values (`test_model.cpp:616-633`) and the scheme tables (644-671).

#### 25. `tests/test_golden.cpp` (Modify)

*   **Type / Scope**: Golden test.
*   **Proposed Changes**:
    *   It includes `member_compare.hpp` instead of defining `equal_member()`.
    *   **New function** `column_problems(descriptor, product, role)`, which returns a list of
        problems:
        *   full current cases: a model field with no column, or a column outside the model;
        *   lite current cases: missing fields other than the class's `kLiteSkip` fields, or a
            column outside the model;
        *   every field: an element kind (bool, integer, float, string) that differs.

        Width differences are returned separately as notes.
    *   "the reader fills every member of every model object as the fixtures say" (305-349) calls
        it for every `current_*` case except `current_v2_fixed_only`, and checks that the list is
        empty. Notes go into `MESSAGE` lines. Classes in no current case are printed.
    *   **New test case:** `column_problems` on a copy of `current_v1`'s `reco_particles` product
        with the `csda_ke` column removed reports exactly that column.

#### 26. `tests/test_reader.cpp` (Modify)

*   **Type / Scope**: Unit test.
*   **Proposed Changes**:
    *   **505**, "the old class name Meta is read as ImageMeta3D", which today is
        `CHECK(meta->complete())`, becomes:
        *   `CHECK(meta->mismatched.empty())` and `CHECK(meta->extra.empty())`;
        *   a check that `lower`, `upper`, `size` and `count` are not in `meta->missing`.
    *   **562:** the expected product line is built from the report's own `complete()`.
    *   **608**, the strict-mode test with `only_runs`: it opens `current_v1_lite.h5` instead of
        the frozen `v0.11.1_v1.h5`. Lite files lack fields in other products, but never in
        `run_info`, and the current set is written by the model's own tag.
    *   **670-673:** the expected mismatch text is built from
        `static_cast<std::int64_t>(ParticleShape::SHOWER)` and `::TRACK`.
    *   **676:** becomes `RecoParticle::kNumFields - 5 - 2`, keeping the comment "5 bound, 2
        mismatched" for `edge_model_binding_v1.h5`.
    *   **716-718 and 734-737:** compare with `RecoParticle{}` members instead of literal
        defaults.

#### 27. `tests/check_spine_dump.cmake` (Modify)

*   **Type / Scope**: Test script.
*   **Proposed Changes**: line 36 checks the `meta [Meta]: ` prefix only.

#### 28. `tests/CMakeLists.txt` (Modify)

*   **Type / Scope**: Test registration.
*   **Proposed Changes**:
    *   The `spine_reader_tests` entry (line 14) passes
        `"--test-case-exclude=real files*,contract*"`.
    *   `add_test(NAME spine_reader_contracts COMMAND spine_reader_tests "--test-case=contract*")`.

#### 29. `tools/release/scan_consumer_fields.py` (Create)

*   **Type / Scope**: Python 3.10+ tool, standard library only.
*   **Proposed Changes**: repeats the scan of Appendix B for one CAFMaker build.
    *   **Input:** either the build folder of a Makefile build, whose commands it reads with
        `make -n -B`, or a `compile_commands.json`; plus the CAFMaker's source root.
    *   **The scan.**
        *   It copies `include/` to a temporary folder, and marks every model member and `Field`
            enumerator `[[deprecated("SPINEFIELD <Class>.<field>")]]`, using the committed
            schema's field names.
        *   It re-runs each compile command with `-fsyntax-only` and the marked headers first on
            the include path, inside whatever environment the caller has set up.
        *   It strips colour codes, and keeps the deprecation warnings and errors that point into
            the source root.
    *   **Output:** the fields read, with their first location. With `--contracts
        tests/test_contracts.cpp`, it lists the fields that have no row.
    *   It runs the CAFMaker's commands, so it is run by hand, in that CAFMaker's environment.

#### 30. `include/spine/model/EGM.md`, `src/model/EGM.md`, root `EGM.md`, `tools/EGM.md`, `README.md`, `tools/gen/README.md` (Modify); `tools/release/EGM.md` (Create)

*   **Type / Scope**: `EGM.md` contracts and documentation.
*   **Proposed Changes**:
    *   `include/spine/model/EGM.md:6-7, 23-32` and `src/model/EGM.md:7, 30`: the literal model
        version and the field counts are removed. The text says the fields come from the schema,
        and the golden test checks them against the fixtures' columns.
        `include/spine/model/EGM.md:38-41`, the list of enums, stays.
    *   Root `EGM.md:111-116`: the list of ctest entries gains `spine_reader_contracts`, and the
        exclusion of `spine_reader_tests` becomes `real files*` and `contract*`.
    *   `tools/EGM.md:9-12`: it describes the registration of the Python checks with
        `SPINE_READER_REQUIRE_PYTHON`, and adds `release/` to the index.
    *   `tools/release/EGM.md` (Create): documents `scan_consumer_fields.py`. Pull request 6 adds
        the rest.
    *   The README's "Tests" table gains `spine_reader_contracts`, what it checks, and the rule
        that a contract row is added when a CAFMaker starts reading a field, found with
        `scan_consumer_fields.py` (R12).
    *   `tools/gen/README.md`, step 4 of "What to check afterwards" (45-49): the requests for
        field counts, `LAST_FIELD`, enum member counts and regenerated default checks are removed.
        It says instead: build and run ctest; if `spine_reader_contracts` fails, talk to the
        CAFMaker maintainers before editing the contract row.

### Pull request 5: the dumper records what D2 and A11 need (part 4)

#### 31. `tools/gen/dump_schema.py` (Modify)

*   **Type / Scope**: Schema dumper.
*   **Proposed Changes**:
    *   New option `--spine-sha`.
    *   New keys `spine_sha`, `writer_format` and `warnings`, read exactly as part 4 says.
    *   `EXCLUDED_CLASSES` (new) holds the classes that the comment at lines 42-44 lists as
        excluded (`RecoFragment`, `TruthFragment`, `Neutrino`, `ImageMeta2D`), plus larcv's
        `Particle`.
    *   `main()` is wrapped so that any exception prints `dump_schema: <exception class>:
        <message>` and exits 1.

#### 32. `schema/spine-1.4.0.json` (Modify)

*   **Type / Scope**: Committed schema.
*   **Proposed Changes**: re-dumped with the new dumper, with `--spine-sha
    4726e068f17e433e2fec068dc8ee498fa9a0658f`. `generate.py --check` still passes.

#### 33. `tools/gen/README.md` and `tools/gen/EGM.md` (Modify)

*   **Type / Scope**: Documentation and `EGM.md` contract.
*   **Proposed Changes**:
    *   They document the new keys and the one-line errors. `tools/gen/EGM.md:114-120`, which
        says what the dumper stops on, is updated to match.
    *   After `tools/gen/EGM.md:81-82`, the line "the model version names the schema it was
        generated from, not the newest SPINE it reads" is added.

### Pull request 6: classifier, entry point and the CI `follow_spine` step (parts 5, 6 and 7)

#### 34. `tools/release/classify.py` (Create)

*   **Type / Scope**: Python 3.10+ script, standard library only.
*   **Proposed Changes**:
    *   Command line: `classify.py BASELINE NEW [--old-columns DIR --new-columns DIR] [--warnings
        FILE] [--effects FILE] --json OUT --md OUT`.
    *   Its output keys, the bump table and the precedence are those of part 5. The bump table is
        one dictionary, with "person" as the default.
    *   It starts from the prototype `tagdump/cmp.py`.
    *   It exits 0, or 2 on bad input. `follow_spine.py` maps exit 2 to `internal_error`.

#### 35. `tools/release/follow_spine.py` (Create, mode 100755)

*   **Type / Scope**: Executable Python 3.10+ entry point.
*   **Proposed Changes**:
    *   Arguments: `TAG`. Options:
        *   `--baseline PATH`, `--apply`, `--repo-root DIR`;
        *   `--spine-repo URL`, `--workdir DIR`, `--artifacts-dir DIR` and `--jobs N`, with the
            defaults of part 6.
    *   **It runs S0 to S8 of the data flow:**
        *   S0, with `check_usage()`, before any network access;
        *   S1 and the checkout, with `spine_checkout.py`, imported at the top because it needs
            neither h5py nor numpy. `make_fixtures.py` runs as a subprocess;
        *   S3, with `compare_schemas()`, which imports `tools/gen/generate.py` as a module;
        *   S6 in the temporary folder:
            *   `git archive HEAD`;
            *   the overlay;
            *   `generate.py --root`, then `check_names.py --root`, then the export's
                `make_member_list.py`;
            *   CMake with `-DSPINE_READER_REQUIRE_PYTHON=ON` and
                `-DPython3_EXECUTABLE=<sys.executable>`;
            *   ctest with `--output-junit`, with `SPINE_READER_REAL_FILES` removed from its
                environment;
            *   the export's `spine-dump` on HEAD's `current_v1.h5` and `current_v2.h5`, taken
                with `git show HEAD:tests/data/fixtures/<file>`;
            *   the second classifier run.
    *   It maps the results with `outcome_for()`, and writes `status.json` and one log per stage.
    *   It never runs `git commit`, and never edits `CMakeLists.txt`.

#### 36. `tools/release/tests/data/spine-1.2.2.json` and `spine-1.2.3.json` (Create)

*   **Type / Scope**: Test data.
*   **Proposed Changes**: dumps of v1.2.2 (`d4de053fd33fffddd031a3c8b58c10004baaee20`) and v1.2.3
    (`f745fef398f6198eef8c40904eb449729381c291`) made with the new dumper, about 200 kB each.
    1.2.2 is the baseline of the v1.2.3 replay, and together they are the real pair in
    `test_classify.py`.

#### 37. `tools/release/tests/test_classify.py` (Create)

*   **Type / Scope**: Python unit tests (`unittest`, no SPINE, no network).
*   **Proposed Changes**:
    *   Each test file starts by adding `tools/release` (its parent folder) to `sys.path`, so it
        can import the scripts.
    *   Mutated copies of `schema/spine-1.4.0.json` cover these change kinds, each with the
        expected classification and bump:
        *   fields: one added in the middle, one removed, a shape retype, an element retype, a
            reordering;
        *   enums: a member added, removed, renumbered or aliased; an enum added;
        *   a scheme table added;
        *   a default, unit, flag or reference change;
        *   a doc-only change and a module-only change;
        *   a class added and a class removed;
        *   a `type_check` entry and a `writer_format` change;
        *   a stored-width change in the column folders.
    *   Precedence: a default change together with an addition gives "person".
    *   The real pair 1.2.2 → 1.2.3 gives enum additions and a patch bump.
    *   Every classifier key has a row in the bump table.
    *   The mutation helper starts from the prototype `tagdump/sim/mutate.py`. It works on copies:
        the prototype deletes the schema in place.

#### 38. `tools/release/tests/test_outcomes.py` (Create)

*   **Type / Scope**: Python unit tests (no SPINE, no network).
*   **Proposed Changes**:
    *   `outcome_for()` gives the right outcome and exit code for every outcome of part 6.
    *   `check_usage()` gives `usage_error` for:
        *   a malformed tag;
        *   `--apply` on a dirty tree, in a temporary git repository whose commits set
            `GIT_AUTHOR_NAME`, `GIT_AUTHOR_EMAIL` and the committer equivalents in the environment;
        *   `--apply` with a replay.
    *   `compare_schemas()` gives `changed` for a mutated `type_check` and for a mutated
        `writer_format` alone, and `generate_failed` for a schema with a non-ASCII doc, which
        `generate.py` refuses (`generate.py:289-301`).

#### 39. `tests/CMakeLists.txt` (Modify)

*   **Type / Scope**: Test registration.
*   **Proposed Changes**: when the Python found is 3.10 or newer, adds
    `spine_reader_release_tools`: `${Python3_EXECUTABLE} -m unittest discover -s
    ${PROJECT_SOURCE_DIR}/tools/release/tests -t ${PROJECT_SOURCE_DIR}/tools/release`.

#### 40. `.github/workflows/ci.yml` and `ci/EGM.md` (Modify)

*   **Type / Scope**: GitHub Actions workflow and its contract.
*   **Proposed Changes**:
    *   Job `hdf5-1.14` gains, in its `run:` step after ctest:
        *   reading `spine_version` from `schema/spine-*.json` with Python;
        *   `tools/release/follow_spine.py v<spine_version> --artifacts-dir "$RUNNER_TEMP/artifacts"`,
            which must exit 0.
    *   The job uploads `$RUNNER_TEMP/artifacts` when it fails.
    *   `ci/EGM.md` documents the step.

#### 41. `tools/release/README.md` (Create); `tools/release/EGM.md`, `EGM.md`, `README.md`, `tools/gen/README.md` (Modify)

*   **Type / Scope**: Documentation and `EGM.md` contracts.
*   **Proposed Changes**:
    *   `tools/release/README.md` covers:
        *   the environment needed;
        *   how to run `follow_spine.py`, the outcome table, and what to do for each outcome;
        *   how to replay with `--baseline`;
        *   what `--apply` writes;
        *   how to run `scan_consumer_fields.py`.
    *   `tools/release/EGM.md` adds the commands, the outcomes and exit codes, and `status.json`'s
        fields. Its side effects: none without `--apply`; with it, the files of part 6.
    *   Root `EGM.md`:
        *   it loses its literal model versions (25, 29, 47);
        *   "Global Side Effects" gains: `follow_spine.py --apply` writes the files of part 6;
        *   the list of ctest entries (111-116) gains `spine_reader_release_tools`.
    *   The README's "Tests" table gains `spine_reader_release_tools`.
    *   In `tools/gen/README.md`:
        *   "Regenerate for a new SPINE release" points to `tools/release/follow_spine.py`. The
            manual commands stay, as the explanation of what the script does.
        *   Step 4 of the checklist adds: read `classification.md` first.

## Verification Plan

Each item names the goal it checks. "Local tag vX" means a tag vX, lightweight unless said
otherwise, on a commit in a local SPINE clone in a scratch folder. The run uses that clone through
`--spine-repo <clone>`. Every local tag sets SPINE's `version.py` to its own number, except
v1.4.95.

*   **CI (G9):**
    *   The local dry run of both jobs in the SFT Ubuntu images is done (part 7). It is repeated
        before opening pull request 1, and again before pull request 6.
    *   The first CI run, on pull request 1, is green on both jobs.
    *   Configuring with `-DSPINE_READER_REQUIRE_PYTHON=ON` and Python hidden (`-DPython3_EXECUTABLE=/nonexistent`)
        fails.
    *   From pull request 6 on, the `follow_spine` step passes on every push.
*   **Build, locally (G9):**
    *   `cmake -S . -B build && cmake --build build -j8` under LCG_110 EL9 (HDF5 1.14.6, gcc 13)
        and under LCG_105 EL9 (HDF5 1.12.2).
    *   `ci/sl7-ups.sh` once, by hand, to check that the headers compile with gcc 12 in C++17 mode.
    *   The SBN and FD CAFMaker ports build against pull request 2 without code changes. ND is
        built by its own CI (DUNE/ND_CAFMaker#213), which pins a spine-reader commit.
*   **Unit tests (G2, G5, G6):**
    *   Under LCG_110, ctest registers seven entries and runs six: `spine_reader_real_files` is
        skipped without real files. Under LCG_105, it registers six (no
        `spine_reader_release_tools`) and runs five.
    *   `test_classify.py` and `test_outcomes.py` run as part of `spine_reader_release_tools`.
*   **Version (G1):**
    *   The `--version` check in `check_spine_dump.cmake` passes.
    *   `ci/check_tag_version.sh v0.1.0` passes, and `v0.1.1` fails.
    *   After pull request 2 merges, CI is green on its merge commit, and
        `git rev-parse v0.1.0^{commit}` prints that commit.
    *   The tag push of `v0.1.0` runs the CI backstop, which passes.
*   **Expected changes (G2, G3, G4):**
    *   **Relabel.** Set the committed schema's `spine_version` to 1.4.1, regenerate the model in an
        export, and run ctest: every entry passes with no test edited.
    *   **The column check.** Its unit test on the doctored `reco_particles` product (item 25)
        reports the removed column.
    *   **Local tags:** the table below.

| Local tag | What it changes | Expected result |
| --- | --- | --- |
| v1.4.99 | adds a `float` field with a default to `RecoParticle` | `changed` (10), `contracts_failed` false; one field added, patch bump (provisional); the golden test sees the field in the model and the files |
| v1.4.98 | changes the default of `RecoParticle.csda_ke` (read by ND and SBN) | `changed` (10), `contracts_failed` true; "a person decides" |
| v1.4.97 | removes `RecoParticle.csda_ke` | `changed` (10), `contracts_failed` true, because the contract row has no field; minor bump. No test or example code names `csda_ke`, so the build passes |
| v1.4.92 | changes the unit of `RecoParticle.csda_ke` | `changed` (10), `contracts_failed` true |
| v1.4.91 | adds a member to `NuInteractionScheme`, with its tables in `Neutrino.resolve_enum` | `changed` (10), every entry passes after regeneration; the follow-up list notes the label the fixtures do not use |
| v1.4.90 | swaps the values of `ParticleShape.SHOWER` and `TRACK` | `test_failed` (30): the frozen fixtures no longer bind `shape` |
| v1.4.88 | removes `RecoParticle.crt_scores`, which no CAFMaker reads and no model-level test names (`test_core_columns.cpp:94` reads it from a frozen core test file) | `changed` (10), `contracts_failed` false; minor bump |
| v1.4.87 | adds a class with field metadata to `spine.data` | `identical` (0) with `needs_attention` true, and an `unlisted_class` warning |
| v1.4.86 | makes SPINE's `HDF5Reader` raise when reading | `fixture_check_failed` (30); no other reader is tried |

*   **Replays and older tags (G4, G5):**
    *   `follow_spine.py v1.2.3 --baseline tools/release/tests/data/spine-1.2.2.json` gives
        `changed` (10) with `replay` true. The enum additions are listed, with a patch bump.
    *   `follow_spine.py v1.2.1` and `v1.2.2` give `changed` (10) with `replay` true, and write
        their fixtures with the substitutions listed.
*   **Outcomes (G6):**
    *   `follow_spine.py v1.4.0` gives `identical` (0).
    *   `v1.2.0` and a local tag `v1.4.0rc1` give `not_followed` (20).
    *   `1.4.0`, without the `v`, and a version absent from the local clone (for example `v1.4.80`)
        give `usage_error` (2).
    *   A `--spine-repo` that `git ls-remote` cannot read gives `environment_error` (30).
    *   Re-pointing `v1.4.0` in a local clone gives `tag_moved` (30), because the resolved SHA is no
        longer the schema's `spine_sha`.
    *   Annotated and lightweight local tags both resolve (S1).
    *   The local tags in the table below each give a failure (30).
    *   `test_outcomes.py` covers `test_failed`, `environment_error`, `internal_error`,
        `generate_failed` and the usage errors.

| Local tag | What it changes | `outcome` |
| --- | --- | --- |
| v1.4.96 | deletes the `pdg` attribute from `FieldMetadata` | `dump_failed` |
| v1.4.95 | leaves `version.py` at 1.4.0 | `version_mismatch` |
| v1.4.94 | renames the writer's `format_version` parameter | `fixture_tooling_failed` (worker exit 3) |
| v1.4.93 | drops one column from the V2 writer only | `fixture_check_failed` |

*   **Fixture policy (G7):**
    *   After `--apply` on v1.4.99 in a scratch clone of spine-reader, `git status` shows only the
        schema, the model, `golden_members.inc`, the `current_*` files and the manifest. The
        manifest still lists the frozen cases.
    *   After the `identical` run on v1.4.0, `git status` is clean.
*   **`EGM.md` contracts (G8):**
    *   The reviewer of each pull request checks its `EGM.md` items (4, 13, 21, 30, 33, 40, 41).
    *   `grep -rnE '1\.4\.0|[0-9]+ fields' --include=EGM.md .` shows only lines about the frozen
        cases' history.
*   **Static analysis:**
    *   `-Wall -Wextra -Wpedantic` produce no warnings on the library.
    *   `python3 tools/gen/check_names.py` passes.

## Appendix A: decisions and findings this design relies on

**Decisions** (author, recorded in the design brief):

*   **D1.** `project(VERSION)` is the only version source. Tags are made from it after CI passes.
*   **D2.** "Identical" means the generated model is unchanged after normalising the version, and
    `type_check` and the writer's format versions are unchanged.
*   **D3.** The compatibility record (open, stage 2).
*   **D4.** The hand-written change detectors are replaced by a check against the files' columns,
    and the consumer contract tests stay hand-written in their own ctest entry.
*   **D5.** A scheduled tag scan comes first. A dispatch from SPINE may come later, as a hint only.
*   **D6.** Strict mode under a patch release (open, stage 2).
*   **D7.** Which tags are followed (open, stage 2).
*   **D8.** Owners (open, stage 2).
*   **D9.** Schema export ownership (deferred).
*   **D10.** Fixtures are named by role, historical cases are frozen, and a `.h5` file is
    committed only when its decoded content changes.
*   **D11.** Release notes (open, stage 2).

**Findings of the brief cited here:**

*   **A4.** The table of test expectations: incidental, change detectors, consumer contracts.
    Reproduced in part 2.
*   **A11.** Some changes give no signal: unmapped `FieldMetadata` attributes, unlisted classes,
    a change of the writer's default layout, stored-width drift.
*   **A13.** A patch release can still break users. After an added field, strict mode refuses
    older files, and shared builds have no SOVERSION.
*   **A17.** New fields are tested only at their default; `FlashHypothesis` is never written.
*   **A19.** The dump depends on the Python environment, so tool versions are recorded.
*   **A21.** CAFMaker maintainers need a lookup path and `spine-dump --check`.
*   **A24.** The `EGM.md` contracts state literal versions and counts.

**Choices made or confirmed in the design review** are recorded under Alternatives C, D, F, G,
H, J, K, L, N, P and Q.

## Appendix B: the model fields the CAFMakers read

**How the list was made (2026-11-05).**

*   **The scan.** Each port's sources were compiled with `-fsyntax-only`, using the port's own
    compile commands and SL7 environment, against a copy of the model headers in which every
    member and every `Field` enumerator is marked `[[deprecated]]`. Each warning that points into
    the port's own sources is one read.
*   **What it includes.** The scan finds direct member reads and `has(Class::Field::name)`
    queries. It ignores the warnings inside the model headers, which come from the implicit copy
    of objects in `Event::objects`.
*   **Cross-check.** For ND, the result equals the port's own `FillerInputs()` list exactly: 47
    fields on both sides.
*   **The ports scanned:**
    *   ND: DUNE/ND_CAFMaker#213 at `fbda59e`;
    *   SBN: justinjmueller/sbn_ml_cafmaker#8 at `6cc8bb6`;
    *   FD: DUNE/dune_ml_cafmaker#1 at `666443e`.
*   **The companion file.** `following-spine-releases.cafmaker-fields.tsv` lists every field, with
    its unit and default from `schema/spine-1.4.0.json`, the CAFMakers that read it, and the first
    source location in each port. The repeatable tool is `scan_consumer_fields.py` (item 29).

**Summary:**

*   228 of the model's 321 fields are read by at least one CAFMaker. ND reads 47, SBN 219 and FD
    31; 24 fields are read by all three.
*   Classes read: `RecoParticle`, `TruthParticle`, `RecoInteraction`, `TruthInteraction`,
    `RunInfo`, `Trigger` and `Flash`.
*   Classes no CAFMaker reads: `ImageMeta3D`, `CRTHit` and `FlashHypothesis`.

| Class | Read | By all three | By two | By one only |
| --- | --- | --- | --- | --- |
| `RecoParticle` | 44 of 55 | `is_contained`, `match_ids`, `match_overlaps`, `is_primary`, `start_point`, `end_point`, `pdg_code`, `momentum` | ND SBN: `id`, `interaction_id`, `calo_ke`, `csda_ke`, `mcs_ke`, `shape`, `start_dir`, `end_dir`, `mass`; SBN FD: `ke` | SBN: 26 more |
| `TruthParticle` | 83 of 98 | `id`, `interaction_id`, `track_id`, `pdg_code`, `energy_init`, `position`, `end_position`, `momentum` | ND SBN: `is_primary`, `orig_interaction_id` | SBN: 73 more |
| `RecoInteraction` | 28 of 40 | `id`, `match_ids`, `match_overlaps`, `vertex` | ND SBN: `flash_total_pe`, `flash_hypo_pe`; SBN FD: `particle_ids` | SBN: 21 more |
| `TruthInteraction` | 61 of 79 | `id` | ND SBN: `orig_id`; SBN FD: `particle_ids`, `vertex`, `pdg_code`, `energy_init`, `momentum` | SBN: 54 more |
| `RunInfo` | 3 of 3 | `run`, `subrun`, `event` | | |
| `Trigger` | 4 of 6 | | | ND: `id`, `type`, `time_s`, `time_ns` |
| `Flash` | 5 of 14 | | | ND: `id`, `volume_id`, `time`, `time_width`, `total_pe` |

## Stage 9 findings and where they are addressed (2026-11-05)

| Id | Finding | Addressed in |
| --- | --- | --- |
| P1 | The CAFMaker field list was missing | Appendix B and the companion TSV; the contract table (part 2, item 24); `scan_consumer_fields.py` (item 29) |
| P2 | Kept tests compared CAFMaker fields with literal defaults and enum numbers | part 2, incidental assertions; items 23 (514-557) and 26 (670-673, 716-718, 734-737) |
| P3 | Additive changes still failed `test_reader.cpp:505, 608` and `test_model.cpp:198-200` | item 26 (505, 608); item 23 (179-206 replaced by an all-fields check) |
| P4 | Item 21's line ranges broke code and left lines unassigned | item 23: every range of `test_model.cpp`, with `all_nan` moved to `member_compare.hpp` |
| P5 | The manifest could not be created or kept up to date | part 3; item 15 (`--manifest-only`, the merge rule, the frozen SHAs); item 18 |
| P6 | Exit codes were not mapped from worker to outcome | data flow S4; item 15 (exit codes, `run_worker`); item 16 (branch 200-203 exits 3) |
| P7 | The dumper's new keys were underspecified | part 4 (the attributes read, the `null` fallback, the warning shape, the mapped set, the class enumeration, the error line) |
| P8 | The classifier's interface was open | part 5 (two runs, inputs, keys, precedence); item 34 |
| P9 | The mechanics of `follow_spine.py` were left open | part 6 (`--repo-root`, functions, stage names, folders); `spine_checkout.py` (item 14); item 35 |
| P10 | Lightweight tags have no `^{}` line | data flow S1; `resolve_tag()` (item 14); Verification |
| P11 | The S0 module check was incomplete | data flow S0 (numpy, h5py, yaml, scipy; CMake ≥ 3.21) |
| P12 | Enum substitution would break frozen cases | part 3 (current set only); item 16 (`--configure-enums`) |
| P13 | `--only` and `--no-reader-check` conflicted | part 3 (partial runs); item 15 |
| P14 | The Verification Plan did not prove several claims | Verification Plan (new local tags, the column-check unit test, `type_check` and `writer_format`, the reader failure, `git rev-parse`, entry counts) |
| P15 | Documentation updates were missing | items 4, 13, 21, 30, 33, 40 and 41 |
| P16 | No fallback for a first CI failure | part 7: the dry run of both jobs passed, and the R3 dump was byte-identical; repeated before pull requests 1 and 6 |
| P17 | Item 23's lite check lacked "nothing extra" | G3; part 2; item 25 |

The nits of the stage 9 report are addressed in the same items. These include the line
references, the CMake regex and configure dependency, the action pinning and single `run:` step,
the artifacts kept outside the checkout, the `content.py` mechanics, keeping `README.md:6`, the
locations of the prototypes, the unit-test imports, the revert order, and the wording.

## Open points after the stage 9 re-audit (2026-11-05)

The re-audit of the revised plan ended the review with these points open. They were recorded as
accepted trade-offs of the design document, to be fixed when each pull request was written; the
full report is in the author's local notes (`implementation_readiness_report.md`, re-audit
section). Each would have made a stated outcome or a ctest entry fail if the plan had been
followed literally. All 16 are fixed; the last column says how.

| Id | Open point | Pull request | Fixed |
| --- | --- | --- | --- |
| R1 | Item 23 must also delete `test_model.cpp:100` (`schema_version == "1.4.0"`), which sits in the kept range 94-141; otherwise every regenerated model fails | 4 | the literal schema version check is gone |
| R2 | Item 23: keep line 315 (`ti`, used by the kept line 326); line 137 needs `static_cast<typename C::Field>(C::kNumFields - 1)`; delete the tautological line 103 | 4 | `ti` kept; the field-count checks use `kNumFields`; line 103 deleted |
| R3 | NaN defaults need a NaN-aware `same_default()` in `member_compare.hpp` (reuse `same_overlaps`, `test_reader.cpp:94-102`), used at `test_model.cpp:532` and `test_reader.cpp:734-735` | 4 | `nan_equal()` in `tests/member_compare.hpp`, also behind `same_overlaps()` |
| R4 | The column-check unit test (item 25) must pick its column at run time, not `csda_ke`, which v1.4.97 removes | 4 | the column is the first model field the product has, picked at run time |
| R5 | The release-tool tests (items 37-38) must mutate a frozen `tools/release/tests/data/spine-1.4.0.json`, not `schema/spine-1.4.0.json`, which S6 and `--apply` replace | 6 | the tests mutate `tools/release/tests/data/spine-1.4.0.json` |
| R6 | Create an empty `tools/release/tests/__init__.py`, which `unittest discover -s … -t …` requires | 6 | `tools/release/tests/__init__.py` |
| R7 | A failed `read` worker is a reader-check failure (`make_fixtures.py` exit 1, `fixture_check_failed`); only `write` and `litify` worker failures map to exit 4 | 3 | a failed `read` worker exits 1 |
| R8 | A new `NuInteractionScheme` member needs its tables in `Neutrino.resolve_enum`, or the dump stops (`dump_schema.py:341-371`); local tag v1.4.91 must add them, or use a `ParticlePID` member; say so in G2 | 6 | v1.4.91 adds the tables; G2 says so |
| R9 | The S4 manifest needs a read-only base (`--manifest-base`, default the committed manifest) so frozen entries survive; `h5` and `expected` are file names | 3, 6 | `--manifest-base` in pull request 3; `follow_spine.py` passes HEAD's manifest |
| R10 | The contract enum checks must list every member of `ParticleShape`, `ParticlePID` and `NuCurrentType`, and decide on `NuInteractionScheme` and the interaction tables; add a local tag renumbering `ParticlePID.MUON` | 4 | every member of the three enums; the rest is not a contract (As built) |
| R11 | `needs_attention` is true when `classes_added`, `warnings_new` or the substitutions are non-empty; warnings and substitutions share the shape `{kind, name, detail}` | 6 | warnings and substitutions are `{kind, name, detail}`; `needs_attention` as stated |
| R12 | `scan_consumer_fields.py` needs a command line, a row-parsing rule, the prototype's flags (`LC_ALL=C`, `-Wdeprecated-declarations`, `cd … &&`, source-root filter), and a bash compile step for the CAFMakers' SL7 environment; verify it on ND at `fbda59e` (47 rows) | 4 | done; the scan of the three ports gives Appendix B exactly |
| R13 | The lite check must be two-way: the missing fields equal the class's `kLiteSkip` fields | 4 | missing fields equal the `lite_skip` ones, both ways |
| R14 | "Every classifier key has a row" must name the change-kind keys; the others are informational | 6 | a test checks that every change kind has a row, and every row is reachable |
| R15 | Part 6: the unit tests need neither the network nor SPINE; the `--apply` test needs the `git` binary | 6 | the `--apply` test skips without git |
| R16 | Item 21: case names become role names, measured v1.4.0 facts stay literal ("at v1.4.0"), only current-set text says "the model's SPINE tag", frozen history stays | 3 | done in the fixture README and `EGM.md` |

The re-audit also listed nits (contract-table literals as `json.dumps` of the schema default,
`EXCLUDED_CLASSES` additions, `follow_spine.py` imports, CI shell options, documentation
indexes, and wording); they are in the same report.

## As built (2026-11-06)

The six pull requests were merged in the order of the plan, as DeepLearnPhysics/spine-reader
pull requests 1 to 6. v0.1.0 was tagged on the merge commit of pull request 2 (`101e2db`). This
section records where the code differs from the plan above; where the two differ, this section
is right.

**Pull request 1 (CI).** `ci/github-ci.sh` holds each job's commands, so that CI and the local
dry run run the same script; each job's single `run:` step calls it.

**Pull request 2 (the library's version).** As planned.

**Pull request 3 (fixture tooling).** As planned, with R7, R9 and R16. In addition,
`make_fixtures.py` exits 4 on any unexpected exception (with the traceback) and reports a missing
h5py in one line, so that no Python error reads as exit 1, a check failure.

**Pull request 4 (tests that survive expected changes).**

*   The NaN-aware comparison of R3 is named `nan_equal()`. `tests/member_compare.hpp` also has
    `member_json()`, which prints a member in the contract table's form in failure messages.
*   The contract table is checked in one test case that loops over the table's classes, rather
    than in one test case per class.
*   R10. `NuInteractionScheme`, the five interaction-type enums and the scheme tables are not
    contracts:
    *   no CAFMaker reads `interaction_scheme`, or calls `interaction_mode_enum()` or
        `interaction_type_enum()`;
    *   SBN stores `interaction_mode` and `interaction_type` as SPINE's numbers.

    `test_model.cpp` keeps a structural check of the tables instead: each scheme selects one of
    the generated tables or none, and a value outside the enum selects none. Renumbering
    `ParticlePID.MUON` was checked on a doctored schema: only `spine_reader_contracts` fails.
*   Two more change detectors were removed, which the A4 table had missed:
    *   "every field has a doc" in `check_structure`. SPINE can add a field without a docstring,
        and the local tag v1.4.99 does.
    *   `test_core_real.cpp` required a real file to have every model field. It now does so only
        for files written by the model's own SPINE version.
*   `scan_consumer_fields.py` needs Python 3.9, not 3.10, so that it runs with the host's Python
    beside the CAFMaker's SL7 container.
    *   Its warning flags (`-Wno-error` and the others) go after the CAFMaker's own; the ND build
        uses `-Werror`, which otherwise wins.
    *   On the three ports it lists the fields of Appendix B exactly: ND 47, SBN 219, FD 31, 228
        in all.

**Pull request 5 (the dumper).**

*   `EXCLUDED_CLASSES` also holds `ParticleLabel` and the base classes `DataBase` and
    `PosDataBase`, which `spine.data` exports with `attr_metadata` at 1.4.0. A name bound to a
    listed class (`Meta` is `ImageMeta3D`) counts as listed.
*   Each warning is `{kind, name, detail}`, the shape of the fixture tooling's substitutions
    (R11).
*   An unknown `--classes` name raises `LookupError`, so it gives the same one-line error as the
    other failures.

**Pull request 6 (classifier, entry point, CI step).**

*   `classify.py` takes `--baseline-warnings`: the committed manifest's warnings and
    substitutions. Together with the baseline schema's own warnings, they make the set a new
    warning is compared with. Without them, the fixtures' warnings would be new on every run.
*   The change kinds have names (`field_added`, `default_changed`, ...). The output lists them in
    `change_kinds`, and the matched rows of the bump table in `reasons`. Three kinds come from the
    decoded columns: `stored_width_changed`, `stored_type_changed`, and `stored_columns_changed`
    (a column that no schema change explains).
*   `status.json` also has `failure_reason` (for `dump_failed`, the dumper's exception class) and
    `change_kinds`.
*   The CI step is `ci/github-ci.sh --follow-spine DIR`, inside the 1.14 job's single step,
    because only that script's shell has the LCG view.
*   The release-tool tests use their own data, never `tests/data/fixtures`, which a run of
    `follow_spine.py` replaces in its export.
*   `tools/gen/check_names.py` scanned every file of `src/model/`, including `EGM.md`, whose "I/O"
    matched the macro `I`. It had failed since the `EGM.md` files were added, without anyone
    noticing, because no ctest entry runs it. It now scans the `.hpp` and `.cpp` files only.

**Verification.** Every outcome of the two local-tag tables of the Verification Plan was reached
with local tags in a scratch clone of SPINE. So were:

*   the replays: v1.2.1, v1.2.2, and v1.2.3 against the 1.2.2 baseline;
*   the moved tag, the pre-release tag and the usage errors;
*   `--apply` on v1.4.99 in a clean clone.

CI runs `follow_spine.py v1.4.0` on every push and requires `identical`. On GitHub's runner the
step takes about 3.5 minutes.

--------------------------------------------------------------------------------

*Drafted with EGM*
