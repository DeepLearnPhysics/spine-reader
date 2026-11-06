#!/usr/bin/env python3
"""Follow one SPINE tag: dump its schema, compare it with the model, write and check its
fixtures, classify the change, then build and test spine-reader with it.

    follow_spine.py TAG [--baseline SCHEMA] [--apply] [--repo-root DIR] [--spine-repo URL]
                    [--workdir DIR] [--artifacts-dir DIR] [--jobs N]

The stages (tools/release/README.md has the whole story):

* s0_checks: the tag form, ``--apply``'s conditions and the environment; no network.
* s1_tag: the tag's commit (``git ls-remote``) and a checkout of it (tools/fixtures).
* s2_dump: ``tools/gen/dump_schema.py`` on that checkout.
* s3_compare: identical (the generated C++ is the same once the version is normalised, and
  ``type_check`` and ``writer_format`` are unchanged) or changed.
* s4_fixtures: ``tools/fixtures/make_fixtures.py`` writes the tag's current set with its own
  writer, runs every cross-check and reads the files back with the tag's own reader.
* s5_classify: ``classify.py``. A replay (a ``--baseline`` other than the committed schema, or a
  tag older than the model's) stops here.
* s6_build and s6_test: a ``git archive HEAD`` export in the artifacts folder, with the new
  current set (and, if changed, the new schema and the regenerated model), built and tested.
  Then the regenerated ``spine-dump`` reads HEAD's previous ``current_v1``/``current_v2``, and
  ``classify.py`` runs again with that text.
* s8_apply (``--apply``, after ``changed`` only): what s6 overlaid is written into the
  repository's working tree. Nothing is committed and ``project(VERSION)`` is not edited.

Exit codes: 0 identical; 10 changed (``contracts_failed`` says whether
``spine_reader_contracts`` failed); 20 not followed (a pre-release tag, or one below v1.2.1); 30
a failure (``failed_stage`` names the stage); 2 usage error. Every outcome but a usage error
writes ``status.json`` in the artifacts folder.

The caller sets up the LCG view: Python 3.10 or newer with numpy, h5py, yaml and scipy, git,
CMake 3.21 or newer and a C++ compiler, plus network access to the SPINE repository.
"""

import argparse
import importlib.util
import io
import json
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import traceback
import xml.etree.ElementTree as ElementTree
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_REPO = HERE.parents[1]
sys.path.insert(0, str(DEFAULT_REPO / "tools" / "fixtures"))
import spine_checkout  # noqa: E402  (imports neither h5py nor numpy)

STATUS_FORMAT_VERSION = 1
DEFAULT_SPINE_REPO = "https://github.com/DeepLearnPhysics/spine"
DEFAULT_WORKDIR = os.path.join(tempfile.gettempdir(), "spine-reader-fixtures")
TAG_RE = re.compile(r"^v(\d+)\.(\d+)\.(\d+)$")
PRERELEASE_RE = re.compile(r"^v\d+\.\d+\.\d+\S+$")
OLDEST_FOLLOWED = (1, 2, 1)
MIN_CMAKE = (3, 21)
MODULES = ("numpy", "h5py", "yaml", "scipy")

STAGES = ["s0_checks", "s1_tag", "s2_dump", "s3_compare", "s4_fixtures", "s5_classify",
          "s6_build", "s6_test", "s8_apply"]
FAILURES = ["environment_error", "tag_moved", "dump_failed", "version_mismatch",
            "generate_failed", "fixture_tooling_failed", "fixture_check_failed", "build_failed",
            "test_failed", "internal_error"]
EXIT_CODES = {"identical": 0, "changed": 10, "not_followed": 20, "usage_error": 2,
              **{f: 30 for f in FAILURES}}
CONTRACTS_TEST = "spine_reader_contracts"


class UsageError(Exception):
    """Bad arguments: exit 2, and no status.json."""


class StageFailure(Exception):
    """A stage ended in a failure outcome."""

    def __init__(self, outcome, detail, reason=None):
        super().__init__(outcome, detail, reason)
        self.outcome, self.detail, self.reason = outcome, detail, reason

    def __str__(self):
        return f"{self.outcome}: {self.detail}"


# ---------------------------------------------------------------------------------------------
# Pure functions (the unit tests call these; they need neither git, the network, nor SPINE)
# ---------------------------------------------------------------------------------------------

def parse_tag(tag):
    """(major, minor, patch) of a final tag vX.Y.Z, or None."""
    m = TAG_RE.match(tag)
    return tuple(int(x) for x in m.groups()) if m else None


def version_tuple(text):
    return tuple(int(x) for x in text.split("."))


def committed_schema(repo):
    """The one schema/spine-*.json of ``repo``."""
    found = sorted(Path(repo, "schema").glob("spine-*.json"))
    if len(found) != 1:
        raise UsageError(f"{repo}/schema holds {len(found)} spine-*.json files, not one")
    return found[0]


def git(repo, *args, check=True):
    proc = subprocess.run(["git", "-C", str(repo), *args], capture_output=True, text=True)
    if check and proc.returncode != 0:
        raise UsageError(f"git {' '.join(args)} in {repo} failed: {proc.stderr.strip()}")
    return proc.stdout


def check_usage(args, repo):
    """The checks of s0 that need no network. Raises UsageError; returns a dict with the tag's
    version, the baseline, the model's version, ``replay`` and ``not_followed`` (a reason or
    None)."""
    out = {"tag": args.tag, "version": None, "not_followed": None}
    version = parse_tag(args.tag)
    if version is None:
        if PRERELEASE_RE.match(args.tag):
            out["not_followed"] = f"{args.tag} is a pre-release tag"
            return out
        raise UsageError(f"{args.tag!r} is not a tag of the form vX.Y.Z")
    out["version"] = version
    if version < OLDEST_FOLLOWED:
        out["not_followed"] = (f"{args.tag} is older than v{'.'.join(map(str, OLDEST_FOLLOWED))}, "
                               "which the dumper cannot import")
        return out
    committed = committed_schema(repo)
    try:
        model = json.loads(committed.read_text())
        out["model_version"] = version_tuple(model["spine_version"])
    except (ValueError, KeyError, AttributeError) as e:
        raise UsageError(f"{committed}: not a schema dump ({type(e).__name__}: {e})") from e
    baseline = Path(args.baseline).resolve() if args.baseline else committed.resolve()
    if not baseline.is_file():
        raise UsageError(f"--baseline: no file {args.baseline}")
    try:
        out["baseline"] = json.loads(baseline.read_text())
    except ValueError as e:
        raise UsageError(f"--baseline {baseline}: not JSON ({e})") from e
    if "spine_version" not in out["baseline"]:
        raise UsageError(f"--baseline {baseline}: no spine_version")
    out["baseline_path"] = baseline
    out["replay"] = baseline != committed.resolve() or version < out["model_version"]
    if args.apply:
        if out["replay"]:
            raise UsageError("--apply follows the model forward: it takes no --baseline and no tag "
                             "older than the model's")
        if version <= out["model_version"]:
            raise UsageError(f"--apply needs a tag newer than the model's "
                             f"(v{model['spine_version']})")
        if git(repo, "status", "--porcelain").strip():
            raise UsageError(f"--apply needs a clean working tree in {repo} (git status shows "
                             "changes or untracked files)")
    return out


def outcome_for(stages):
    """(outcome, exit code, failed stage, contracts_failed) from the stage results.

    ``stages`` maps a stage name to a dict with ``status`` (``passed``, ``failed`` or
    ``skipped``) and, for a failed stage, ``outcome``. ``s0_checks`` may carry
    ``not_followed``; ``s3_compare`` carries ``comparison`` (identical or changed);
    ``s6_test`` carries ``failed_tests``.
    """
    comparison = stages.get("s3_compare", {}).get("comparison")
    for name in STAGES:
        result = stages.get(name)
        if result is None:
            continue
        if result.get("not_followed"):
            return "not_followed", EXIT_CODES["not_followed"], None, False
        if result.get("status") != "failed":
            continue
        if name == "s6_test":
            failed = set(result.get("failed_tests", []))
            if failed == {CONTRACTS_TEST} and comparison == "changed":
                return "changed", EXIT_CODES["changed"], None, True
            return "test_failed", EXIT_CODES["test_failed"], name, False
        outcome = result.get("outcome", "internal_error")
        if outcome not in EXIT_CODES:
            outcome = "internal_error"
        return outcome, EXIT_CODES[outcome], name, False
    if comparison in ("identical", "changed"):
        return comparison, EXIT_CODES[comparison], None, False
    return "internal_error", EXIT_CODES["internal_error"], None, False


def load_generator(repo):
    """tools/gen/generate.py of ``repo``, imported as a module."""
    path = Path(repo, "tools", "gen", "generate.py")
    spec = importlib.util.spec_from_file_location("spine_reader_generate", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def compare_schemas(baseline, new, generator=None):
    """s3: ``identical`` when the generated files of both schemas are the same once the new
    schema's spine_version is set to the baseline's, and type_check and writer_format are
    unchanged; otherwise ``changed``. ``generate_failed`` when the generator refuses either
    schema (with ``error``)."""
    generator = generator or load_generator(DEFAULT_REPO)
    out = {"result": None, "files": [], "type_check_changed": False,
           "writer_format_changed": False, "error": None}
    normalised = dict(new, spine_version=baseline["spine_version"])
    try:
        old_files = generator.generate(baseline)
        new_files = generator.generate(normalised)
    except generator.GenerationError as e:
        out["result"], out["error"] = "generate_failed", str(e)
        return out
    out["files"] = sorted(p for p in set(old_files) | set(new_files)
                          if old_files.get(p) != new_files.get(p))
    out["type_check_changed"] = baseline.get("type_check") != new.get("type_check")
    out["writer_format_changed"] = baseline.get("writer_format") != new.get("writer_format")
    changed = out["files"] or out["type_check_changed"] or out["writer_format_changed"]
    out["result"] = "changed" if changed else "identical"
    return out


def failed_tests(junit_path):
    """Names of the failed tests of a ctest JUnit file."""
    root = ElementTree.parse(junit_path).getroot()
    names = []
    for case in root.iter("testcase"):
        if case.get("status") == "fail" or case.find("failure") is not None:
            names.append(case.get("name"))
    return names


# ---------------------------------------------------------------------------------------------
# The run
# ---------------------------------------------------------------------------------------------

class Run:
    def __init__(self, args, repo, artifacts):
        self.args, self.repo, self.artifacts = args, repo, artifacts
        self.logs = artifacts / "logs"
        self.logs.mkdir(parents=True, exist_ok=True)
        self.stages = {}
        self.info = {"warnings": [], "substitutions": []}

    def log_path(self, stage):
        return self.logs / f"{stage}.log"

    def say(self, stage, text):
        with open(self.log_path(stage), "a") as fh:
            fh.write(text.rstrip("\n") + "\n")

    def run(self, stage, cmd, env=None, cwd=None, stdout_file=None):
        """Run ``cmd``, appending its output to the stage's log. Returns (code, stdout, stderr)."""
        self.say(stage, "$ " + shlex.join(str(c) for c in cmd))
        proc = subprocess.run([str(c) for c in cmd], capture_output=True, env=env, cwd=cwd)
        if stdout_file is not None:
            Path(stdout_file).write_bytes(proc.stdout)
            stdout = ""
        else:
            stdout = proc.stdout.decode(errors="replace")
        stderr = proc.stderr.decode(errors="replace")
        with open(self.log_path(stage), "a") as fh:
            if stdout:
                fh.write(stdout if stdout.endswith("\n") else stdout + "\n")
            if stderr:
                fh.write(stderr if stderr.endswith("\n") else stderr + "\n")
            fh.write(f"[exit {proc.returncode}]\n")
        return proc.returncode, stdout, stderr

    def stage(self, name, function, *args):
        """Run one stage; record its status, duration and log. Returns the function's value, or
        None when the stage failed."""
        start = time.monotonic()
        print(f"follow_spine: {name} ...", flush=True)
        result = {"status": "passed", "log": str(self.log_path(name).relative_to(self.artifacts))}
        self.log_path(name).touch()
        value = None
        try:
            value = function(*args)
            if isinstance(value, dict):
                result.update({k: v for k, v in value.items() if k in
                               ("comparison", "failed_tests", "not_followed", "detail")})
        except UsageError:
            raise
        except StageFailure as e:
            result.update(status="failed", outcome=e.outcome, detail=e.detail)
            if e.reason:
                result["reason"] = e.reason
            if isinstance(e, TestsFailed):
                result["failed_tests"] = e.names
            self.say(name, f"FAILED: {e}")
        except Exception as e:  # anything unexpected is internal_error
            result.update(status="failed", outcome="internal_error",
                          detail=f"{type(e).__name__}: {e}")
            self.say(name, traceback.format_exc())
        result["duration_s"] = round(time.monotonic() - start, 3)
        self.stages[name] = result
        if result["status"] == "failed":
            print(f"follow_spine: {name} failed: {result['outcome']}: {result['detail']}",
                  flush=True)
            return None
        return value

    # --- s0 ------------------------------------------------------------------------------------

    def s0_environment(self):
        problems = []
        if sys.version_info < (3, 10):
            problems.append(f"Python {platform.python_version()}: SPINE needs 3.10 or newer")
        for module in MODULES:
            if importlib.util.find_spec(module) is None:
                problems.append(f"no Python module {module}")
        for tool in ("git", "cmake", "ctest"):
            if shutil.which(tool) is None:
                problems.append(f"no {tool} on PATH")
        if shutil.which("cmake"):
            text = subprocess.run(["cmake", "--version"], capture_output=True, text=True).stdout
            m = re.search(r"(\d+)\.(\d+)", text)
            if not m or (int(m.group(1)), int(m.group(2))) < MIN_CMAKE:
                problems.append(f"CMake {m.group(0) if m else '?'}: 3.21 or newer is needed "
                                "(ctest --output-junit)")
        cxx = os.environ.get("CXX") or shutil.which("c++") or shutil.which("g++")
        if not cxx or not shutil.which(cxx.split()[0]):
            problems.append("no C++ compiler (CXX, c++ or g++)")
        self.info["tools"] = self.tool_versions(cxx)
        for p in problems:
            self.say("s0_checks", p)
        if problems:
            raise StageFailure("environment_error", "; ".join(problems))
        return {}

    def tool_versions(self, cxx):
        def first_line(cmd):
            try:
                text = subprocess.run(cmd, capture_output=True, text=True, timeout=60).stdout
                return text.splitlines()[0].strip() if text.strip() else None
            except (OSError, subprocess.SubprocessError):
                return None
        probe = ("import json, numpy, h5py; print(json.dumps({'numpy': numpy.__version__, "
                 "'h5py': h5py.__version__, 'hdf5': h5py.version.hdf5_version}))")
        versions = {"python": platform.python_version()}
        try:
            proc = subprocess.run([sys.executable, "-c", probe], capture_output=True, text=True,
                                  timeout=120)
            versions.update(json.loads(proc.stdout) if proc.returncode == 0 else {})
        except (OSError, ValueError, subprocess.SubprocessError):
            pass
        versions["cmake"] = first_line(["cmake", "--version"]) if shutil.which("cmake") else None
        versions["compiler"] = first_line(shlex.split(cxx) + ["--version"]) if cxx else None
        versions["git"] = first_line(["git", "--version"]) if shutil.which("git") else None
        return versions

    # --- s1 ------------------------------------------------------------------------------------

    def s1_tag(self, usage):
        tag, baseline = self.args.tag, usage["baseline"]
        own = tag == "v" + baseline["spine_version"]
        try:
            sha = spine_checkout.resolve_tag(self.args.spine_repo, tag)
        except spine_checkout.LsRemoteError as e:
            raise StageFailure("environment_error", str(e)) from e
        except spine_checkout.TagNotFound as e:
            if own:
                raise StageFailure("tag_moved", f"{tag}, the baseline's own tag, is gone from "
                                                f"{self.args.spine_repo}") from e
            raise UsageError(str(e)) from e
        self.say("s1_tag", f"{tag} -> {sha}")
        recorded = baseline.get("spine_sha")
        if own and recorded and sha != recorded:
            raise StageFailure("tag_moved", f"{tag} points to {sha}, but the baseline was dumped "
                                            f"from {recorded}")
        if own and not recorded:
            self.say("s1_tag", "the baseline records no spine_sha: the tag is not checked")
        try:
            src = spine_checkout.ensure_clone(tag, sha, self.args.spine_repo, self.args.workdir)
        except spine_checkout.CheckoutError as e:
            raise StageFailure("environment_error", str(e)) from e
        self.say("s1_tag", f"checkout: {src}")
        return {"sha": sha, "src": src}

    # --- s2 ------------------------------------------------------------------------------------

    def s2_dump(self, tag, sha, src):
        out_dir = self.artifacts / "schema"
        shutil.rmtree(out_dir, ignore_errors=True)
        env = dict(os.environ, PYTHONHASHSEED="0")
        code, _, stderr = self.run("s2_dump", [
            sys.executable, self.repo / "tools/gen/dump_schema.py", "--spine-src", src,
            "--spine-sha", sha, "--out-dir", out_dir], env=env)
        if code != 0:
            lines = [x for x in stderr.splitlines() if x.startswith("dump_schema: ")]
            m = re.match(r"^dump_schema: (\w+):", lines[-1]) if lines else None
            detail = lines[-1][len("dump_schema: "):] if lines else stderr.strip()[-500:]
            raise StageFailure("dump_failed", detail or f"exit {code}",
                               reason=m.group(1) if m else f"exit {code}")
        found = sorted(out_dir.glob("spine-*.json"))
        if len(found) != 1:
            raise StageFailure("internal_error", f"the dump wrote {len(found)} schema files")
        new = json.loads(found[0].read_text())
        if tag != "v" + new["spine_version"]:
            raise StageFailure("version_mismatch", f"{tag} says spine.__version__ is "
                                                   f"{new['spine_version']}")
        self.info["schema_warnings"] = new.get("warnings", [])
        return {"path": found[0], "schema": new, "spine_version": new["spine_version"]}

    # --- s3 ------------------------------------------------------------------------------------

    def s3_compare(self, baseline, new):
        result = compare_schemas(baseline, new, load_generator(self.repo))
        self.say("s3_compare", json.dumps(result, indent=1))
        if result["result"] == "generate_failed":
            raise StageFailure("generate_failed", result["error"])
        self.info["comparison"] = result
        return {"comparison": result["result"], **result}

    # --- s4 ------------------------------------------------------------------------------------

    def head_files(self):
        """HEAD's manifest and current_*.expected.json, in <artifacts>/head."""
        head = self.artifacts / "head"
        (head / "fixtures").mkdir(parents=True, exist_ok=True)
        names = git(self.repo, "ls-tree", "--name-only", "HEAD", "tests/data/fixtures/").split()
        for name in names:
            base = os.path.basename(name)
            if base == "manifest.json" or (base.startswith("current_")
                                           and base.endswith(".expected.json")):
                data = subprocess.run(["git", "-C", str(self.repo), "show", f"HEAD:{name}"],
                                      capture_output=True, check=True).stdout
                (head / "fixtures" / base).write_bytes(data)
        return head / "fixtures"

    def s4_fixtures(self, tag, sha, schema_path):
        head = self.head_files()
        out = self.artifacts / "fixtures"
        shutil.rmtree(out, ignore_errors=True)
        code, stdout, _ = self.run("s4_fixtures", [
            sys.executable, self.repo / "tools/fixtures/make_fixtures.py", "--tag", tag,
            "--sha", sha, "--schema", schema_path, "--out", out,
            "--manifest-base", head / "manifest.json", "--spine-repo", self.args.spine_repo,
            "--workdir", self.args.workdir, "--artifacts-dir", self.artifacts / "fixture-run"])
        summary = {}
        for line in reversed(stdout.splitlines()):
            if line.startswith("{"):
                try:
                    summary = json.loads(line)
                except ValueError:
                    pass
                break
        self.info["warnings"] = summary.get("warnings", [])
        self.info["substitutions"] = summary.get("substitutions", [])
        if code == 3:
            raise StageFailure("fixture_tooling_failed", "a worker could not drive SPINE's API")
        if code == 1:
            raise StageFailure("fixture_check_failed",
                               f"{summary.get('failures', '?')} check(s) failed")
        if code != 0:
            raise StageFailure("internal_error", f"make_fixtures.py exited {code}")
        return {"fixtures": out, "head": head}

    # --- s5 ------------------------------------------------------------------------------------

    def classify(self, stage, baseline_path, new_path, old_columns, new_columns, effects=None):
        head_manifest = json.loads((self.artifacts / "head/fixtures/manifest.json").read_text())
        warnings = self.artifacts / "warnings.json"
        warnings.write_text(json.dumps(self.info["warnings"] + self.info["substitutions"],
                                       indent=1) + "\n")
        baseline_warnings = self.artifacts / "baseline-warnings.json"
        baseline_warnings.write_text(json.dumps(head_manifest.get("warnings", [])
                                                + head_manifest.get("substitutions", []),
                                                indent=1) + "\n")
        cmd = [sys.executable, self.repo / "tools/release/classify.py", baseline_path, new_path,
               "--warnings", warnings, "--baseline-warnings", baseline_warnings,
               "--json", self.artifacts / "classification.json",
               "--md", self.artifacts / "classification.md"]
        if old_columns is not None:
            cmd += ["--old-columns", old_columns, "--new-columns", new_columns]
        if effects is not None:
            cmd += ["--effects", effects]
        code, _, stderr = self.run(stage, cmd)
        if code != 0:
            raise StageFailure("internal_error", f"classify.py exited {code}: {stderr.strip()}")
        self.info["classification"] = json.loads(
            (self.artifacts / "classification.json").read_text())
        return {}

    # --- s6 ------------------------------------------------------------------------------------

    def s6_build(self, changed, new_schema_path, fixtures):
        export = self.artifacts / "export"
        shutil.rmtree(export, ignore_errors=True)
        export.mkdir(parents=True)
        archive = subprocess.run(["git", "-C", str(self.repo), "archive", "--format=tar", "HEAD"],
                                 capture_output=True, check=True).stdout
        safe = {"filter": "data"} if hasattr(tarfile, "data_filter") else {}
        with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
            tar.extractall(export, **safe)
        self.say("s6_build", f"exported HEAD ({git(self.repo, 'rev-parse', 'HEAD').strip()}) "
                             f"to {export}")
        target = export / "tests/data/fixtures"
        for old in target.glob("current_*"):
            old.unlink()
        for new in sorted(Path(fixtures).iterdir()):
            if new.name == "manifest.json" or new.name.startswith("current_"):
                shutil.copy2(new, target / new.name)
        self.say("s6_build", "overlaid the new current set and the merged manifest")
        if changed:
            for old in (export / "schema").glob("spine-*.json"):
                old.unlink()
            shutil.copy2(new_schema_path, export / "schema" / Path(new_schema_path).name)
            for cmd in ([sys.executable, export / "tools/gen/generate.py", "--root", export],
                        [sys.executable, export / "tools/gen/check_names.py", "--root", export],
                        [sys.executable, export / "tools/fixtures/make_member_list.py"]):
                code, _, stderr = self.run("s6_build", cmd)
                if code != 0:
                    raise StageFailure("generate_failed", f"{Path(cmd[1]).name} exited {code}: "
                                                          f"{stderr.strip()[-500:]}")
        build = self.artifacts / "build"
        shutil.rmtree(build, ignore_errors=True)
        for cmd in (["cmake", "-S", export, "-B", build, "-DSPINE_READER_REQUIRE_PYTHON=ON",
                     f"-DPython3_EXECUTABLE={sys.executable}"],
                    ["cmake", "--build", build, "-j", str(self.args.jobs)]):
            code, _, _ = self.run("s6_build", cmd)
            if code != 0:
                raise StageFailure("build_failed", f"{shlex.join(str(c) for c in cmd[:2])} "
                                                   f"exited {code}")
        return {"export": export, "build": build}

    def s6_test(self, build):
        junit = self.artifacts / "ctest-junit.xml"
        env = {k: v for k, v in os.environ.items() if k != "SPINE_READER_REAL_FILES"}
        code, _, _ = self.run("s6_test", ["ctest", "--test-dir", build, "--output-on-failure",
                                          "--output-junit", junit], env=env)
        failed = failed_tests(junit) if junit.exists() else []
        self.say("s6_test", f"failed tests: {failed or 'none'}")
        if failed:
            raise TestsFailed(failed)
        if code != 0:
            raise StageFailure("test_failed", f"ctest exited {code} and no test is marked failed")
        return {"failed_tests": []}

    def effects(self, build):
        """What the regenerated spine-dump prints for HEAD's previous current_v1 and v2."""
        previous = self.artifacts / "previous"
        previous.mkdir(exist_ok=True)
        files = []
        for name in ("current_v1.h5", "current_v2.h5"):
            data = subprocess.run(["git", "-C", str(self.repo), "show",
                                   f"HEAD:tests/data/fixtures/{name}"],
                                  capture_output=True, check=True).stdout
            (previous / name).write_bytes(data)
            files.append(previous / name)
        dumps = sorted(p for p in Path(build).rglob("spine-dump") if p.is_file())
        text = self.artifacts / "effects.txt"
        if not dumps:
            text.write_text("spine-dump was not built\n")
            return text
        code, stdout, stderr = self.run("s6_test", [dumps[0], "--events", "1", *files])
        text.write_text(f"$ spine-dump --events 1 <HEAD's current_v1.h5> <HEAD's current_v2.h5>"
                        f"\n{stdout}{stderr}[exit {code}]\n")
        return text

    # --- s8 ------------------------------------------------------------------------------------

    def s8_apply(self, new_schema_path, fixtures):
        repo = self.repo
        for old in (repo / "schema").glob("spine-*.json"):
            old.unlink()
        shutil.copy2(new_schema_path, repo / "schema" / Path(new_schema_path).name)
        for cmd in ([sys.executable, repo / "tools/gen/generate.py", "--root", repo],
                    [sys.executable, repo / "tools/fixtures/make_member_list.py"]):
            code, _, stderr = self.run("s8_apply", cmd)
            if code != 0:
                raise StageFailure("internal_error", f"{Path(cmd[1]).name} exited {code}: "
                                                     f"{stderr.strip()[-500:]}")
        target = repo / "tests/data/fixtures"
        for old in target.glob("current_*"):
            old.unlink()
        for new in sorted(Path(fixtures).iterdir()):
            if new.name == "manifest.json" or new.name.startswith("current_"):
                shutil.copy2(new, target / new.name)
        self.say("s8_apply", git(repo, "status", "--short"))
        return {}


class TestsFailed(StageFailure):
    """s6_test: these ctest entries failed."""

    def __init__(self, names):
        super().__init__("test_failed", "failed: " + ", ".join(names))
        self.names = names


def parse_args(argv):
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("tag", help="the SPINE tag, vX.Y.Z")
    p.add_argument("--baseline", help="schema to compare with (default: the committed one); "
                   "another one makes the run a replay")
    p.add_argument("--apply", action="store_true",
                   help="after 'changed', write the new schema, model and fixtures into the tree")
    p.add_argument("--repo-root", default=str(DEFAULT_REPO),
                   help="the spine-reader checkout (default: the one this script is in)")
    p.add_argument("--spine-repo", default=os.environ.get("SPINE_REPO", DEFAULT_SPINE_REPO),
                   help="git URL or path of the SPINE repository")
    p.add_argument("--workdir", default=os.environ.get("SPINE_FIXTURE_WORKDIR", DEFAULT_WORKDIR),
                   help="where the SPINE clones live (shared with make_fixtures.py)")
    p.add_argument("--artifacts-dir", help="where the artifacts, the export and its build go "
                   "(default: a new temporary folder)")
    p.add_argument("--jobs", type=int, default=os.cpu_count() or 1,
                   help="parallel build jobs (default: the number of CPUs)")
    return p.parse_args(argv)


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    args = parse_args(argv)
    repo = Path(args.repo_root).resolve()
    try:
        if not (repo / "tools/release/follow_spine.py").is_file():
            raise UsageError(f"--repo-root: {repo} is not a spine-reader checkout")
        usage = check_usage(args, repo)
    except UsageError as e:
        print(f"follow_spine: usage error: {e}", file=sys.stderr)
        return EXIT_CODES["usage_error"]

    artifacts = Path(args.artifacts_dir).resolve() if args.artifacts_dir else \
        Path(tempfile.mkdtemp(prefix=f"follow_spine-{args.tag}-"))
    artifacts.mkdir(parents=True, exist_ok=True)
    print(f"follow_spine: {args.tag}; artifacts in {artifacts}", flush=True)
    run = Run(args, repo, artifacts)
    reproduce = shlex.join([str(Path(__file__).resolve()), *argv])
    try:
        status = follow(run, usage)
    except UsageError as e:
        print(f"follow_spine: usage error: {e}", file=sys.stderr)
        return EXIT_CODES["usage_error"]
    status["reproduce"] = reproduce
    (artifacts / "status.json").write_text(json.dumps(status, indent=1, default=str) + "\n")
    report(status, artifacts)
    return status["exit_code"]


def follow(run, usage):
    """All the stages; returns the status.json document."""
    args = run.args
    status = {"status_format_version": STATUS_FORMAT_VERSION, "tag": args.tag,
              "spine_sha": None, "spine_version": None, "replay": bool(usage.get("replay")),
              "partial": False}
    if "baseline" in usage:
        b = usage["baseline"]
        status["baseline"] = {"path": str(usage["baseline_path"]),
                              "spine_version": b.get("spine_version"),
                              "spine_sha": b.get("spine_sha")}
    stages = run.stages

    def finish():
        outcome, code, failed_stage, contracts = outcome_for(stages)
        classification = run.info.get("classification", {})
        status.update(
            outcome=outcome, exit_code=code, failed_stage=failed_stage,
            failure_detail=stages[failed_stage].get("detail") if failed_stage else None,
            failure_reason=stages[failed_stage].get("reason") if failed_stage else None,
            contracts_failed=contracts,
            needs_attention=bool(classification.get("needs_attention")),
            comparison=run.info.get("comparison"),
            proposed_bump=classification.get("proposed_bump"),
            change_kinds=classification.get("change_kinds"),
            stages=stages, warnings=run.info.get("schema_warnings", []) + run.info["warnings"],
            substitutions=run.info["substitutions"], tools=run.info.get("tools"),
            spine_reader={"commit": git(run.repo, "rev-parse", "HEAD", check=False).strip(),
                          "dirty": bool(git(run.repo, "status", "--porcelain",
                                            check=False).strip())},
            artifacts=str(run.artifacts))
        return status

    if usage["not_followed"]:
        stages["s0_checks"] = {"status": "passed", "not_followed": usage["not_followed"],
                               "duration_s": 0.0, "log": None}
        run.info["tools"] = None
        return finish()
    if run.stage("s0_checks", run.s0_environment) is None:
        return finish()
    tag = run.stage("s1_tag", run.s1_tag, usage)
    if tag is None:
        return finish()
    status["spine_sha"] = tag["sha"]
    dump = run.stage("s2_dump", run.s2_dump, args.tag, tag["sha"], tag["src"])
    if dump is None:
        return finish()
    status["spine_version"] = dump["spine_version"]
    compare = run.stage("s3_compare", run.s3_compare, usage["baseline"], dump["schema"])
    if compare is None:
        return finish()
    fixtures = run.stage("s4_fixtures", run.s4_fixtures, args.tag, tag["sha"], dump["path"])
    if fixtures is None:
        return finish()
    # columns are compared only with HEAD's set, which belongs to the committed baseline
    default_baseline = usage["baseline_path"] == committed_schema(run.repo).resolve()
    old_columns = fixtures["head"] if default_baseline else None
    if run.stage("s5_classify", run.classify, "s5_classify", usage["baseline_path"],
                 dump["path"], old_columns, fixtures["fixtures"]) is None:
        return finish()
    if usage["replay"]:
        return finish()
    changed = compare["comparison"] == "changed"
    built = run.stage("s6_build", run.s6_build, changed, dump["path"], fixtures["fixtures"])
    if built is None:
        return finish()
    run.stage("s6_test", run.s6_test, built["build"])
    try:  # informational: a failure here changes no outcome
        effects = run.effects(built["build"])
        run.classify("s6_test", usage["baseline_path"], dump["path"], old_columns,
                     fixtures["fixtures"], effects)
    except Exception as e:
        run.say("s6_test", f"the effects or the second classification failed: "
                           f"{type(e).__name__}: {e}")
    outcome = outcome_for(stages)[0]
    if args.apply and outcome == "changed":
        run.stage("s8_apply", run.s8_apply, dump["path"], fixtures["fixtures"])
    elif args.apply:
        print(f"follow_spine: --apply: nothing written ({outcome})", flush=True)
    return finish()


def report(status, artifacts):
    print(f"follow_spine: {status['tag']} -> {status['outcome']} (exit {status['exit_code']})"
          + (f", failed at {status['failed_stage']}: {status['failure_detail']}"
             if status["failed_stage"] else "")
          + (", contracts failed" if status["contracts_failed"] else "")
          + (", replay" if status["replay"] else "")
          + (", needs attention" if status["needs_attention"] else ""))
    if status.get("proposed_bump"):
        print(f"follow_spine: proposed bump: {status['proposed_bump']} "
              f"({', '.join(status.get('change_kinds') or []) or 'no change kind'})")
    if (artifacts / "classification.md").exists():
        print(f"follow_spine: classification: {artifacts / 'classification.md'}")
    if status["stages"].get("s8_apply", {}).get("status") == "passed":
        classification = json.loads((artifacts / "classification.json").read_text())
        print("follow_spine: --apply wrote the new schema, model, golden_members.inc, current "
              "set and manifest; nothing is committed. Follow-up:")
        for item in classification.get("follow_up", []):
            print(f"  - {item}")
    print(f"follow_spine: status: {artifacts / 'status.json'}")
    print(f"follow_spine: reproduce: {status['reproduce']}")


if __name__ == "__main__":
    sys.exit(main())
