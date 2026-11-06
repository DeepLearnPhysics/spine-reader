"""Unit tests of follow_spine.py's pure parts: outcome_for(), check_usage() and
compare_schemas(). No SPINE and no network; the --apply test needs the git binary."""

import argparse
import contextlib
import copy
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

import follow_spine  # noqa: E402

DATA = HERE / "data"
BASE = json.loads((DATA / "spine-1.4.0.json").read_text())


def passed(**extra):
    return {"status": "passed", **extra}


def failed(outcome, **extra):
    return {"status": "failed", "outcome": outcome, **extra}


def through(last, comparison="identical"):
    """Stage results with every stage up to ``last`` passed."""
    stages = {}
    for name in follow_spine.STAGES:
        stages[name] = passed()
        if name == "s3_compare":
            stages[name]["comparison"] = comparison
        if name == last:
            break
    return stages


class OutcomeFor(unittest.TestCase):
    def test_identical_and_changed(self):
        self.assertEqual(follow_spine.outcome_for(through("s6_test")),
                         ("identical", 0, None, False))
        self.assertEqual(follow_spine.outcome_for(through("s6_test", "changed")),
                         ("changed", 10, None, False))
        # a replay stops after s5_classify
        self.assertEqual(follow_spine.outcome_for(through("s5_classify", "changed")),
                         ("changed", 10, None, False))

    def test_only_the_contracts_failed(self):
        stages = through("s6_test", "changed")
        stages["s6_test"] = failed("test_failed", failed_tests=["spine_reader_contracts"])
        self.assertEqual(follow_spine.outcome_for(stages), ("changed", 10, None, True))
        # with an identical comparison, a contract failure is a test failure
        stages["s3_compare"]["comparison"] = "identical"
        self.assertEqual(follow_spine.outcome_for(stages), ("test_failed", 30, "s6_test", False))

    def test_other_tests_failed(self):
        stages = through("s6_test", "changed")
        stages["s6_test"] = failed("test_failed", failed_tests=["spine_reader_contracts",
                                                                "spine_reader_tests"])
        self.assertEqual(follow_spine.outcome_for(stages), ("test_failed", 30, "s6_test", False))
        stages["s6_test"] = failed("test_failed", failed_tests=[])
        self.assertEqual(follow_spine.outcome_for(stages), ("test_failed", 30, "s6_test", False))

    def test_not_followed(self):
        stages = {"s0_checks": passed(not_followed="a pre-release tag")}
        self.assertEqual(follow_spine.outcome_for(stages), ("not_followed", 20, None, False))

    def test_each_failure_names_its_stage(self):
        cases = [("s0_checks", "environment_error"), ("s1_tag", "environment_error"),
                 ("s1_tag", "tag_moved"), ("s2_dump", "dump_failed"),
                 ("s2_dump", "version_mismatch"), ("s3_compare", "generate_failed"),
                 ("s4_fixtures", "fixture_tooling_failed"),
                 ("s4_fixtures", "fixture_check_failed"), ("s4_fixtures", "internal_error"),
                 ("s5_classify", "internal_error"), ("s6_build", "generate_failed"),
                 ("s6_build", "build_failed"), ("s8_apply", "internal_error")]
        for stage, outcome in cases:
            with self.subTest(stage=stage, outcome=outcome):
                stages = through(stage, "changed")
                stages[stage] = failed(outcome)
                self.assertEqual(follow_spine.outcome_for(stages), (outcome, 30, stage, False))

    def test_every_outcome_has_its_exit_code(self):
        codes = follow_spine.EXIT_CODES
        self.assertEqual((codes["identical"], codes["changed"], codes["not_followed"],
                          codes["usage_error"]), (0, 10, 20, 2))
        self.assertEqual({codes[f] for f in follow_spine.FAILURES}, {30})
        self.assertEqual(len(follow_spine.FAILURES), 10)

    def test_an_unknown_failure_or_no_comparison_is_internal(self):
        stages = through("s2_dump")
        stages["s2_dump"] = failed("something_else")
        self.assertEqual(follow_spine.outcome_for(stages)[0], "internal_error")
        self.assertEqual(follow_spine.outcome_for({})[0], "internal_error")


def namespace(tag, baseline=None, apply=False):
    return argparse.Namespace(tag=tag, baseline=baseline, apply=apply)


class CheckUsage(unittest.TestCase):
    """check_usage() on a scratch repository holding only schema/spine-1.4.0.json."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.repo = Path(cls.tmp.name) / "repo"
        (cls.repo / "schema").mkdir(parents=True)
        shutil.copy2(DATA / "spine-1.4.0.json", cls.repo / "schema" / "spine-1.4.0.json")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def check(self, *args, **kwargs):
        return follow_spine.check_usage(namespace(*args, **kwargs), self.repo)

    def test_malformed_tags(self):
        for tag in ("1.4.0", "v1.4", "latest", "v1.4.0 ", "V1.4.0"):
            with self.subTest(tag=tag):
                with self.assertRaises(follow_spine.UsageError):
                    self.check(tag)

    def test_tags_not_followed(self):
        for tag in ("v1.4.0rc1", "v1.5.0-beta", "v1.4.0.1", "v1.2.0", "v0.16.3"):
            with self.subTest(tag=tag):
                self.assertTrue(self.check(tag)["not_followed"])

    def test_replays(self):
        self.assertFalse(self.check("v1.4.0")["replay"])
        self.assertFalse(self.check("v1.4.1")["replay"])
        self.assertTrue(self.check("v1.2.3")["replay"])  # older than the model
        self.assertTrue(self.check("v1.2.3", baseline=str(DATA / "spine-1.2.2.json"))["replay"])
        with self.assertRaises(follow_spine.UsageError):
            self.check("v1.2.3", baseline=str(DATA / "no-such.json"))

    def test_apply_with_a_replay_or_an_old_tag(self):
        for args in (("v1.4.1",), ("v1.4.0",), ("v1.2.3",)):
            kwargs = {"baseline": str(DATA / "spine-1.2.2.json")} if args == ("v1.4.1",) else {}
            with self.subTest(args=args, **kwargs):
                with self.assertRaises(follow_spine.UsageError):
                    self.check(*args, apply=True, **kwargs)


@unittest.skipUnless(shutil.which("git"), "needs the git binary")
class ApplyNeedsACleanTree(unittest.TestCase):
    def test_dirty_and_clean_trees(self):
        env = dict(os.environ, GIT_AUTHOR_NAME="test", GIT_AUTHOR_EMAIL="test@example.org",
                   GIT_COMMITTER_NAME="test", GIT_COMMITTER_EMAIL="test@example.org",
                   GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM="1")
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            (repo / "schema").mkdir()
            shutil.copy2(DATA / "spine-1.4.0.json", repo / "schema" / "spine-1.4.0.json")

            def git(*args):
                subprocess.run(["git", "-C", str(repo), *args], check=True, env=env,
                               capture_output=True)
            git("init", "-q")
            git("add", "schema")
            git("commit", "-q", "-m", "schema")
            args = namespace("v1.4.1", apply=True)
            self.assertFalse(follow_spine.check_usage(args, repo)["replay"])  # clean: accepted
            (repo / "untracked.txt").write_text("x")
            with self.assertRaises(follow_spine.UsageError):
                follow_spine.check_usage(args, repo)
            (repo / "untracked.txt").unlink()
            schema = repo / "schema" / "spine-1.4.0.json"
            schema.write_text(schema.read_text() + "\n")  # a modified tracked file
            with self.assertRaises(follow_spine.UsageError):
                follow_spine.check_usage(args, repo)


class CompareSchemas(unittest.TestCase):
    def compare(self, change):
        new = copy.deepcopy(BASE)
        change(new)
        return follow_spine.compare_schemas(BASE, new)

    def test_identical_after_normalising_the_version(self):
        result = self.compare(lambda s: s.update(spine_version="1.4.1", spine_sha="0" * 40))
        self.assertEqual(result["result"], "identical")
        self.assertEqual(result["files"], [])

    def test_a_type_check_change_alone(self):
        result = self.compare(lambda s: s["type_check"]["mismatches"].append(
            {"class": "RecoParticle", "field": "id", "what": "writer dtype", "detail": "x"}))
        self.assertEqual(result["result"], "changed")
        self.assertTrue(result["type_check_changed"])
        self.assertEqual(result["files"], [])

    def test_a_writer_format_change_alone(self):
        result = self.compare(lambda s: s["writer_format"].update(default_format_version=2))
        self.assertEqual(result["result"], "changed")
        self.assertTrue(result["writer_format_changed"])
        self.assertEqual(result["files"], [])

    def test_a_warning_alone_is_identical(self):
        result = self.compare(lambda s: s["warnings"].append(
            {"kind": "unlisted_class", "name": "TestRecord", "detail": "..."}))
        self.assertEqual(result["result"], "identical")

    def test_a_field_added(self):
        def add(s):
            fields = s["classes"]["RunInfo"]["fields"]
            fields.append(dict(fields[0], name="lumi"))
            s["classes"]["RunInfo"]["num_fields"] = len(fields)
        result = self.compare(add)
        self.assertEqual(result["result"], "changed")
        self.assertIn("include/spine/model/run_info.hpp", result["files"])

    def test_a_schema_the_generator_refuses(self):
        def non_ascii(s):
            s["classes"]["RunInfo"]["fields"][0]["doc"] = "Run number — from LArSoft"
        result = self.compare(non_ascii)
        self.assertEqual(result["result"], "generate_failed")
        self.assertIn("ASCII", result["error"])


class Helpers(unittest.TestCase):
    def test_failed_tests_of_a_junit_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            junit = Path(tmp) / "junit.xml"
            junit.write_text(
                '<?xml version="1.0"?><testsuite>'
                '<testcase name="spine_reader_tests" status="run"/>'
                '<testcase name="spine_reader_contracts" status="fail">'
                '<failure message="Failed"/></testcase>'
                '<testcase name="spine_reader_real_files" status="notrun"><skipped/></testcase>'
                '</testsuite>')
            self.assertEqual(follow_spine.failed_tests(junit), ["spine_reader_contracts"])

    def test_a_usage_error_writes_no_status(self):
        with tempfile.TemporaryDirectory() as tmp:
            artifacts = Path(tmp) / "artifacts"
            with contextlib.redirect_stderr(io.StringIO()) as err:
                code = follow_spine.main(["1.4.0", "--artifacts-dir", str(artifacts)])
            self.assertEqual(code, 2)
            self.assertIn("usage error", err.getvalue())
            self.assertFalse(artifacts.exists())

    def test_a_tag_not_followed_writes_a_status(self):
        with tempfile.TemporaryDirectory() as tmp:
            artifacts = Path(tmp) / "artifacts"
            with contextlib.redirect_stdout(io.StringIO()):
                code = follow_spine.main(["v1.4.0rc1", "--artifacts-dir", str(artifacts)])
            self.assertEqual(code, 20)
            status = json.loads((artifacts / "status.json").read_text())
            self.assertEqual((status["outcome"], status["exit_code"]), ("not_followed", 20))
            self.assertEqual(status["status_format_version"], 1)


if __name__ == "__main__":
    unittest.main()
