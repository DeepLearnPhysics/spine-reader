# tools/release

## Overview

Tooling for following SPINE releases. Today it holds `scan_consumer_fields.py`, which keeps the
contract table of `tests/test_contracts.cpp` current: it lists the model fields one CAFMaker
build reads, and, with `--contracts`, the ones that have no row in the table. A row is added
when a CAFMaker starts reading a field. The tool runs the CAFMaker's own compile commands, so it
is run by hand, in that CAFMaker's environment; no ctest entry runs it.

## Interface

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

- Creates the work folder and writes the files listed under `--workdir`; deletes it after a
  successful run unless `--keep` is given, and keeps it when the scan fails.
- Runs `make -n -B` in the build folder and the CAFMaker's compilers, through `--wrap` when
  given. With `-fsyntax-only` and the output flags removed, they write nothing there. The
  `--setup` lines run as given, with the caller's permissions.
- No network access of its own.

## Verification

- On 2026-11-06 the three ports were scanned in the SL7 container. Each was compared with the
  scan of 2026-11-05 from which the contract table was written:
  - ND (DUNE/ND_CAFMaker#213; its ML filler is unchanged since `fbda59e`): 47 fields;
  - SBN (justinjmueller/sbn_ml_cafmaker#8): 219 fields;
  - FD (DUNE/dune_ml_cafmaker#1): 31 fields.

  The three sets are those of that scan: 228 fields in all, 24 read by all three, each with a
  contract row.
- `python3 -m flake8 --max-line-length=100 tools/release/scan_consumer_fields.py`
