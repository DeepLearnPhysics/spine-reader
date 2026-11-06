#!/usr/bin/env python3
"""List the model fields one CAFMaker build reads.

The CAFMaker's sources are compiled again with ``-fsyntax-only``, against a copy of
spine-reader's ``include/`` in which every model member and every ``Field`` enumerator is marked
``[[deprecated("SPINEFIELD <Class>.<field>")]]``. Each deprecation warning that points into the
CAFMaker's source tree is one read: a member access or a ``has(Class::Field::name)`` query.
Warnings inside the model headers (the implicit copies of objects in ``Event::objects``) are
not reads and are left out.

    scan_consumer_fields.py --source SRC (--build DIR | --compile-commands FILE)
                            [--setup BASH] [--wrap COMMAND] [--contracts FILE] [--json FILE]

* ``--build DIR``: the build folder of a Makefile build; its commands are read with
  ``make -n -B``, which runs nothing. ``--compile-commands FILE``: a ``compile_commands.json``.
* ``--setup BASH``: bash lines run before ``make -n`` and the compiles, in the same shell, to set
  up the CAFMaker's environment (for example ``source setup.sh``). Their output goes to
  ``setup.log`` in the work folder.
* ``--wrap COMMAND``: a command that runs ``bash <script>`` in another environment, for example
  ``apptainer exec -B /cvmfs -B /tmp -B $HOME <SL7 image>``. The work folder (``--workdir``,
  default a new folder in the system temporary folder) must be visible there under the same path.
* ``--contracts tests/test_contracts.cpp``: also list the fields read that have no row in the
  contract table. A row is a line that starts with ``{"<Class>", "<field>",``.

The compiles run with ``LC_ALL=C`` (English diagnostics), ``-Wdeprecated-declarations``,
``-Wno-error`` and ``-fdiagnostics-color=never``; colour codes are stripped all the same. Output
and dependency flags (``-o``, ``-MD``, ``-MF`` ...) are removed, so the build folder is not
written to.

Run it by hand, in the CAFMaker's environment (through ``--setup`` and ``--wrap``): it runs the
CAFMaker's own compile commands. Python 3.9 or newer, standard library only.

Exit codes: 0 the scan ran (and, with ``--contracts``, every field read has a row); 1 some field
read has no row; 2 usage error; 3 the scan could not run or is incomplete (no compile command,
``make -n`` failed, a compile failed, or the model headers could not be marked).
"""

import argparse
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from collections import defaultdict
from pathlib import Path

EXIT_OK, EXIT_UNLISTED, EXIT_USAGE, EXIT_SCAN = 0, 1, 2, 3
TAG = "SPINEFIELD"
LAUNCHERS = {"ccache", "sccache", "distcc", "icecc"}
# flags that write files, with their argument, and without
DROP_WITH_ARG = {"-o", "-MF", "-MT", "-MQ"}
DROP_ALONE = {"-MD", "-MMD", "-MP"}
# appended last, so that they override the command's own -Werror or -Wno-deprecated-declarations
EXTRA_FLAGS = ["-fsyntax-only", "-Wdeprecated-declarations", "-Wno-error",
               "-fdiagnostics-color=never"]
ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
DIAG = re.compile(r"^(?P<file>[^:\s][^:]*):(?P<line>\d+):(?:\d+:)? "
                  r"(?P<kind>warning|error|fatal error): (?P<text>.*)$")
READ = re.compile(rf"is deprecated: {TAG} (?P<cls>\w+)\.(?P<field>\w+)")
ROW = re.compile(r'^\s*\{"(?P<cls>\w+)",\s*"(?P<field>\w+)",')


class ScanError(Exception):
    """The scan could not run, or its result would be incomplete."""


# ---------------------------------------------------------------------------------------------
# The marked headers
# ---------------------------------------------------------------------------------------------

def schema_fields(schema_path):
    """{class: [field, ...]} in the schema's order."""
    schema = json.loads(Path(schema_path).read_text())
    return {cls: [f["name"] for f in d["fields"]] for cls, d in schema["classes"].items()}


def mark_class(text, cls, fields):
    """Return ``text`` (one generated model header) with every member and ``Field`` enumerator
    of ``cls`` marked deprecated. Raises ScanError when one of them is not found exactly once."""
    lines = text.split("\n")
    start = next((i for i, l in enumerate(lines) if re.match(rf"^struct {cls}\b", l)), None)
    if start is None:
        raise ScanError(f"no 'struct {cls}' in its header")
    end = next(i for i in range(start, len(lines)) if lines[i] == "};")
    wanted = set(fields)
    members, enumerators = defaultdict(int), defaultdict(int)
    in_enum = False
    for i in range(start + 1, end):
        line = lines[i]
        if line.startswith("  enum class Field"):
            in_enum = True
            continue
        if in_enum:
            if line.strip() == "};":
                in_enum = False
                continue
            m = re.match(r"^    (\w+),$", line)
            if m and m.group(1) in wanted:
                name = m.group(1)
                lines[i] = f'    {name} [[deprecated("{TAG} {cls}.{name}")]],'
                enumerators[name] += 1
            continue
        # a member declaration: "  <type> <name> = ...;", "  <type> <name>{...};" or
        # "  <type> <name>;", possibly continued on the next lines
        m = re.match(r"^  (?!//)(?!\[\[)[A-Za-z_][\w:<>, ]*?[\w>] (\w+)(?= =|\{|;)", line)
        if m and m.group(1) in wanted:
            name = m.group(1)
            lines[i] = f'  [[deprecated("{TAG} {cls}.{name}")]] ' + line[2:]
            members[name] += 1
    for name in fields:
        if members[name] != 1 or enumerators[name] != 1:
            raise ScanError(f"{cls}.{name}: found {members[name]} member and {enumerators[name]} "
                            "enumerator declarations instead of one each; the generated header "
                            "layout changed")
    return "\n".join(lines)


def mark_headers(root, schema_path, work):
    """Copy ``<root>/include`` to ``<work>/include`` and mark the model there. Returns the copy."""
    target = work / "include"
    shutil.copytree(root / "include", target)
    model = target / "spine" / "model"
    headers = sorted(model.glob("*.hpp"))
    for cls, fields in schema_fields(schema_path).items():
        owners = [h for h in headers if re.search(rf"^struct {cls}\b", h.read_text(), re.M)]
        if len(owners) != 1:
            raise ScanError(f"{cls}: declared in {len(owners)} headers of {model}")
        owners[0].write_text(mark_class(owners[0].read_text(), cls, fields))
    return target


# ---------------------------------------------------------------------------------------------
# Compile commands
# ---------------------------------------------------------------------------------------------

def run_script(script, wrap, work, name):
    """Run ``bash <script>`` (through ``wrap``), with its output in ``<work>/<name>.log``."""
    path = work / f"{name}.sh"
    path.write_text(script)
    with open(work / f"{name}.log", "w") as log:
        proc = subprocess.run(wrap + ["bash", str(path)], stdout=log, stderr=subprocess.STDOUT)
    return proc.returncode


def preamble(setup, work):
    """The start of every script: C locale, then the caller's setup, its output in setup.log."""
    return ("export LC_ALL=C LANG=C\nset +eu\n"
            f"{{\n{setup or ':'}\n}} >> {shlex.quote(str(work / 'setup.log'))} 2>&1\n")


def under(path, root):
    """True if ``path`` is ``root`` or inside it (both real paths)."""
    return path == root or path.startswith(root + os.sep)


def source_of(tokens):
    """The file a compile command compiles: the argument of -c, or None."""
    if "-c" not in tokens:
        return None
    i = tokens.index("-c")
    if i + 1 < len(tokens) and not tokens[i + 1].startswith("-"):
        return tokens[i + 1]
    # "-c" alone: the source is the last argument that is not an option or an option's value
    for t in reversed(tokens):
        if not t.startswith("-") and re.search(r"\.(c|cc|cpp|cxx|C|c\+\+)$", t):
            return t
    return None


def commands_from_make(build, setup, wrap, work):
    """[(directory, tokens)] from ``make -n -B`` in ``build``."""
    out = work / "make-n.txt"
    script = preamble(setup, work) + (
        f"cd {shlex.quote(str(build))} || exit 1\n"
        f"make -n -B > {shlex.quote(str(out))}\n")
    code = run_script(script, wrap, work, "make-n")
    if code != 0:
        raise ScanError(f"make -n -B in {build} failed ({code}); see {work}/make-n.log")
    commands = []
    for line in out.read_text(errors="replace").splitlines():
        directory = str(build)
        m = re.match(r"^cd (\S+) && (.*)$", line)
        if m:
            directory, line = m.group(1), m.group(2)
        try:
            tokens = shlex.split(line)
        except ValueError:
            continue
        if tokens and source_of(tokens):
            commands.append((directory, tokens))
    return commands


def commands_from_database(path):
    """[(directory, tokens)] from a compile_commands.json."""
    commands = []
    for entry in json.loads(Path(path).read_text()):
        tokens = entry.get("arguments") or shlex.split(entry["command"])
        commands.append((entry["directory"], tokens))
    return commands


def select(commands, source_root, exclude):
    """The commands that compile a file of ``source_root`` outside ``exclude`` (real paths),
    each once."""
    kept, seen = [], set()
    for directory, tokens in commands:
        src = source_of(tokens)
        if src is None:
            continue
        path = os.path.realpath(os.path.join(directory, src))
        if not under(path, source_root) or any(under(path, e) for e in exclude):
            continue
        key = (directory, tuple(tokens))
        if key not in seen:
            seen.add(key)
            kept.append((directory, tokens))
    return kept


def syntax_only(tokens, include):
    """``tokens`` with the output and dependency flags removed, the marked headers first on the
    include path, and EXTRA_FLAGS last."""
    out, skip = [], False
    for t in tokens:
        if skip:
            skip = False
        elif t in DROP_WITH_ARG:
            skip = True
        elif t in DROP_ALONE or (t[:3] in ("-MF", "-MT", "-MQ") and len(t) > 3) or \
                (t.startswith("-o") and len(t) > 2):
            pass
        else:
            out.append(t)
    compiler = 2 if os.path.basename(out[0]) in LAUNCHERS else 1
    return out[:compiler] + [f"-I{include}"] + out[compiler:] + EXTRA_FLAGS


def compile_all(commands, include, setup, wrap, work):
    """Run every command; return the path of the diagnostics file and the failed commands."""
    diagnostics, status = work / "diagnostics.txt", work / "status.txt"
    lines = [preamble(setup, work)]
    for n, (directory, tokens) in enumerate(commands):
        d, s = shlex.quote(str(diagnostics)), shlex.quote(str(status))
        lines.append(f"echo '@@DIR {directory}' >> {d}\n"
                     f"( cd {shlex.quote(directory)} && "
                     f"{shlex.join(syntax_only(tokens, include))} ) 2>> {d}\n"
                     f"echo \"{n} $?\" >> {s}\n")
    for p in (diagnostics, status):
        p.write_text("")
    run_script("".join(lines), wrap, work, "compile")
    codes = dict(line.split() for line in status.read_text().splitlines() if line.strip())
    failed = [commands[n] for n in range(len(commands)) if codes.get(str(n)) != "0"]
    return diagnostics, failed


def parse(diagnostics, source_root, exclude):
    """({(class, field): [(file, line), ...]}, [error lines in the source root])."""
    reads, errors = defaultdict(list), []
    directory = "/"
    for raw in diagnostics.read_text(errors="replace").splitlines():
        line = ANSI.sub("", raw)
        if line.startswith("@@DIR "):
            directory = line[len("@@DIR "):]
            continue
        m = DIAG.match(line)
        if not m:
            continue
        path = os.path.realpath(os.path.join(directory, m.group("file")))
        if not under(path, source_root) or any(under(path, e) for e in exclude):
            continue
        read = READ.search(m.group("text"))
        if read:
            reads[(read.group("cls"), read.group("field"))].append(
                (os.path.relpath(path, source_root), int(m.group("line"))))
        elif m.group("kind") != "warning":
            errors.append(line)
    return reads, errors


def contract_rows(path):
    """{(class, field)} of the contract table in ``path``."""
    return {(m.group("cls"), m.group("field"))
            for m in map(ROW.match, Path(path).read_text().splitlines()) if m}


# ---------------------------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------------------------

def parse_args(argv):
    here = Path(__file__).resolve().parents[2]
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--source", required=True, help="the CAFMaker's source root")
    given = p.add_mutually_exclusive_group(required=True)
    given.add_argument("--build", help="build folder of a Makefile build (read with make -n -B)")
    given.add_argument("--compile-commands", help="a compile_commands.json")
    p.add_argument("--setup", default="", help="bash lines that set up the CAFMaker's environment")
    p.add_argument("--wrap", default="", help="command prefix that runs bash elsewhere "
                   "(for example an apptainer exec of an SL7 image)")
    p.add_argument("--root", default=str(here), help="spine-reader checkout (default: this one)")
    p.add_argument("--schema", help="schema JSON (default: the only schema/spine-*.json)")
    p.add_argument("--contracts", help="the contract table (tests/test_contracts.cpp): list the "
                   "fields read that have no row")
    p.add_argument("--name", help="label of the CAFMaker in the output (default: the source "
                   "folder's name)")
    p.add_argument("--workdir", help="work folder (default: a new temporary folder); it must be "
                   "visible under the same path inside --wrap")
    p.add_argument("--keep", action="store_true", help="keep the work folder")
    p.add_argument("--json", help="also write the result as JSON to this file")
    args = p.parse_args(argv)
    for name in ("source", "build"):
        value = getattr(args, name)
        if value and not Path(value).is_dir():
            p.error(f"--{name}: no folder {value}")
    for name in ("compile_commands", "contracts", "schema"):
        value = getattr(args, name)
        if value and not Path(value).is_file():
            p.error(f"--{name.replace('_', '-')}: no file {value}")
    root = Path(args.root).resolve()
    if args.schema is None:
        schemas = sorted((root / "schema").glob("spine-*.json"))
        if len(schemas) != 1:
            p.error(f"{root}/schema holds {len(schemas)} schemas; pass --schema")
        args.schema = str(schemas[0])
    if not (root / "include" / "spine" / "model").is_dir():
        p.error(f"--root: no include/spine/model in {root}")
    args.root = root
    return args


def scan(args, work):
    source_root = os.path.realpath(args.source)
    wrap = shlex.split(args.wrap)
    include = mark_headers(args.root, args.schema, work)
    if args.build:
        commands = commands_from_make(Path(args.build).resolve(), args.setup, wrap, work)
        exclude = [os.path.realpath(args.build)]
    else:
        commands = commands_from_database(args.compile_commands)
        exclude = [os.path.realpath(os.path.dirname(os.path.abspath(args.compile_commands)))]
    exclude = [e for e in exclude if e != source_root]
    commands = select(commands, source_root, exclude)
    if not commands:
        raise ScanError(f"no compile command for a file of {source_root}")
    diagnostics, failed = compile_all(commands, include, args.setup, wrap, work)
    reads, errors = parse(diagnostics, source_root, exclude)
    return commands, failed, reads, errors


def main(argv=None):
    args = parse_args(argv)
    name = args.name or Path(args.source).resolve().name
    work = Path(args.workdir).resolve() if args.workdir else \
        Path(tempfile.mkdtemp(prefix="scan_consumer_fields."))
    work.mkdir(parents=True, exist_ok=True)
    try:
        commands, failed, reads, errors = scan(args, work)
    except ScanError as e:
        print(f"scan_consumer_fields: {e} (work folder kept: {work})", file=sys.stderr)
        return EXIT_SCAN

    order = {(cls, f): (i, j) for i, (cls, fields) in enumerate(schema_fields(args.schema).items())
             for j, f in enumerate(fields)}
    read = sorted(reads, key=lambda k: order.get(k, (len(order), 0)))
    print(f"{name}: {len(read)} model fields read, from {len(commands)} compile commands")
    for key in read:
        uses = sorted(set(reads[key]))
        more = f" (+{len(uses) - 1})" if len(uses) > 1 else ""
        print(f"  {key[0]}.{key[1]}  {uses[0][0]}:{uses[0][1]}{more}")

    code = EXIT_OK
    unlisted = []
    if args.contracts:
        rows = contract_rows(args.contracts)
        unlisted = [k for k in read if k not in rows]
        print(f"{name}: {len(unlisted)} fields read with no row in {args.contracts}")
        for cls, field in unlisted:
            print(f"  {cls}.{field}")
        if unlisted:
            code = EXIT_UNLISTED
    if failed or errors:
        print(f"scan_consumer_fields: {len(failed)} compile commands failed and {len(errors)} "
              f"errors point into {args.source}: the list may be incomplete. First ones:",
              file=sys.stderr)
        for line in errors[:5]:
            print(f"  {line}", file=sys.stderr)
        for _, tokens in failed[:3]:
            print(f"  failed: {source_of(tokens)}", file=sys.stderr)
        code = EXIT_SCAN

    if args.json:
        Path(args.json).write_text(json.dumps({
            "name": name, "source": os.path.realpath(args.source), "schema": args.schema,
            "commands": len(commands), "failed_commands": len(failed), "errors": errors,
            "fields": {f"{c}.{f}": [f"{p}:{n}" for p, n in sorted(set(reads[(c, f)]))]
                       for c, f in read},
            "unlisted": [f"{c}.{f}" for c, f in unlisted],
        }, indent=1) + "\n")
    if code == EXIT_SCAN or args.keep:
        print(f"work folder: {work}", file=sys.stderr)
    else:
        shutil.rmtree(work, ignore_errors=True)
    return code


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
