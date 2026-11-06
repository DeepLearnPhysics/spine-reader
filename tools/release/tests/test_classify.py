"""Unit tests of classify.py: every change kind on mutated copies of a frozen 1.4.0 schema, the
bump precedence, the real pair 1.2.2 -> 1.2.3, and the bump table. No SPINE, no network."""

import contextlib
import copy
import io
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

import classify  # noqa: E402

DATA = HERE / "data"


def frozen(version="1.4.0"):
    """A copy of tools/release/tests/data/spine-<version>.json, never the committed schema."""
    return json.loads((DATA / f"spine-{version}.json").read_text())


BASE = frozen()


def mutated(change):
    """A copy of the frozen 1.4.0 schema with ``change(schema)`` applied."""
    schema = copy.deepcopy(BASE)
    change(schema)
    return schema


def field(schema, cls, name):
    return next(f for f in schema["classes"][cls]["fields"] if f["name"] == name)


def add_field(schema, cls="RecoParticle", at=10, name="new_score"):
    fields = schema["classes"][cls]["fields"]
    new = dict(field(schema, cls, "csda_ke"), name=name, doc="A new score")
    fields.insert(at, new)
    schema["classes"][cls]["num_fields"] = len(fields)


def remove_field(schema, cls="RecoParticle", name="crt_scores"):
    c = schema["classes"][cls]
    c["fields"] = [f for f in c["fields"] if f["name"] != name]
    c["num_fields"] = len(c["fields"])


def set_members(schema, enum, members):
    schema["enums"][enum]["members"] = members


# Each case: (description, change, expected change kinds, expected bump)
CASES = [
    ("a field added in the middle", add_field, {"field_added"}, "patch"),
    ("a field removed", remove_field, {"field_removed"}, "minor"),
    ("a shape retype",
     lambda s: field(s, "RecoParticle", "csda_ke").update(storage="fixed_array", length=3),
     {"shape_retyped"}, "minor"),
    ("an element retype",
     lambda s: field(s, "RecoParticle", "csda_ke").update(element="float32"),
     {"element_retyped"}, "minor"),
    ("a reordering",
     lambda s: s["classes"]["RunInfo"]["fields"].reverse(), {"field_reordered"}, "person"),
    ("an enum member added",
     lambda s: s["enums"]["ParticlePID"]["members"].append(["NEUTRON", 6]),
     {"enum_member_added"}, "patch"),
    ("an enum member removed",
     lambda s: s["enums"]["ParticlePID"]["members"].pop(), {"enum_member_removed"}, "minor"),
    ("an enum member renumbered",
     lambda s: set_members(s, "ParticlePID", [
         [n, 7 if n == "MUON" else v] for n, v in s["enums"]["ParticlePID"]["members"]]),
     {"enum_member_renumbered"}, "minor"),
    ("an enum alias added",
     lambda s: s["enums"]["ParticlePID"]["aliases"].append(["MU", 2, "MUON"]),
     {"enum_aliases_changed"}, "person"),
    ("an enum added",
     lambda s: s["enums"].__setitem__("NewEnum", copy.deepcopy(s["enums"]["NuCurrentType"])),
     {"enum_added"}, "patch"),
    ("an enum removed",
     lambda s: s["enums"].pop("NeutNuInteractionType"), {"enum_removed"}, "minor"),
    ("a scheme table added",
     lambda s: s["scheme_tables"]["interaction_mode"].__setitem__("MARLEY",
                                                                  "GenieNuInteractionType"),
     {"scheme_table_added"}, "patch"),
    ("a scheme table removed",
     lambda s: s["scheme_tables"]["interaction_type"].pop("NEUT"), {"scheme_table_removed"},
     "minor"),
    ("a scheme table changed",
     lambda s: s["scheme_tables"]["interaction_mode"].__setitem__("NUWRO",
                                                                  "NuWroNuInteractionType"),
     {"scheme_table_changed"}, "person"),
    ("a default changed",
     lambda s: field(s, "RecoParticle", "csda_ke").update(default=0.0), {"default_changed"},
     "person"),
    ("a unit changed",
     lambda s: field(s, "RecoParticle", "csda_ke").update(units="GeV"), {"units_changed"},
     "person"),
    ("a flag changed",
     lambda s: field(s, "RecoParticle", "csda_ke")["flags"].append("vector"), {"flags_changed"},
     "person"),
    ("a reference changed",
     lambda s: field(s, "RecoParticle", "interaction_id").update(reference_space="opposite"),
     {"reference_changed"}, "person"),
    ("a doc-only change",
     lambda s: field(s, "RecoParticle", "csda_ke").update(doc="Range-based kinetic energy"),
     {"doc_changed"}, "none"),
    ("a module-only change",
     lambda s: s["classes"]["RecoParticle"].update(module="spine.data.out.reco"),
     {"module_changed"}, "none"),
    ("a class added",
     lambda s: s["classes"].__setitem__("NewThing", copy.deepcopy(s["classes"]["RunInfo"])),
     {"class_added"}, "person"),
    ("a class removed", lambda s: s["classes"].pop("FlashHypothesis"), {"class_removed"},
     "minor"),
    ("a type_check entry",
     lambda s: s["type_check"]["mismatches"].append(
         {"class": "RecoParticle", "field": "csda_ke", "what": "writer dtype",
          "detail": "dtype: declared '<f8', writer '<f4'"}),
     {"type_check_changed"}, "person"),
    ("a writer_format change",
     lambda s: s["writer_format"].update(default_format_version=2), {"writer_format_changed"},
     "person"),
]
CHANGE = {description: change for description, change, _, _ in CASES}


class ChangeKinds(unittest.TestCase):
    def test_identical_schemas_have_no_change_kind(self):
        c = classify.classify(BASE, copy.deepcopy(BASE))
        self.assertEqual(c["change_kinds"], [])
        self.assertEqual(c["proposed_bump"], "none")
        self.assertFalse(c["needs_attention"])

    def test_each_change_kind(self):
        for description, change, kinds, bump in CASES:
            with self.subTest(description):
                c = classify.classify(BASE, mutated(change))
                self.assertEqual(set(c["change_kinds"]), kinds)
                self.assertEqual(c["proposed_bump"], bump)
                self.assertEqual([r["kind"] for r in c["reasons"]], sorted(kinds))

    def test_added_field_details(self):
        c = classify.classify(BASE, mutated(add_field))
        added = c["classes"]["RecoParticle"]["added"]
        self.assertEqual(added, [{"name": "new_score", "position": 10, "storage": "scalar",
                                  "element": "float64", "length": None}])
        self.assertFalse(c["classes"]["RecoParticle"]["reordered"])
        self.assertIn("the patch bump for additions is provisional until decision D6",
                      c["follow_up"])

    def test_retype_says_whether_the_shape_changed(self):
        shape = classify.classify(BASE, mutated(CHANGE["a shape retype"]))
        element = classify.classify(BASE, mutated(CHANGE["an element retype"]))
        shape, element = shape["classes"]["RecoParticle"], element["classes"]["RecoParticle"]
        self.assertTrue(shape["retyped"][0]["shape_changed"])
        self.assertFalse(element["retyped"][0]["shape_changed"])

    def test_doc_and_module_changes_are_module_only(self):
        for description, change, kinds, _ in CASES:
            with self.subTest(description):
                c = classify.classify(BASE, mutated(change))
                self.assertEqual(c["module_only"], kinds <= classify.DOC_ONLY)

    def test_the_highest_bump_wins(self):
        def both(s):
            add_field(s)
            field(s, "RecoParticle", "csda_ke").update(default=0.0)
        c = classify.classify(BASE, mutated(both))
        self.assertEqual(set(c["change_kinds"]), {"field_added", "default_changed"})
        self.assertEqual(c["proposed_bump"], "person")
        self.assertEqual(c["reasons"][0], {"kind": "default_changed", "bump": "person"})

        c = classify.classify(BASE, mutated(lambda s: (add_field(s), remove_field(s))))
        self.assertEqual(c["proposed_bump"], "minor")

    def test_the_real_pair_1_2_2_to_1_2_3(self):
        c = classify.classify(frozen("1.2.2"), frozen("1.2.3"))
        self.assertEqual(set(c["change_kinds"]),
                         {"enum_added", "enum_member_added", "scheme_table_added"})
        self.assertEqual(c["proposed_bump"], "patch")
        self.assertEqual([m["name"] for m in c["enums"]["NuInteractionScheme"]["added"]],
                         ["GIBUU", "NUWRO", "NEUT"])
        self.assertEqual(c["classes"], {})
        self.assertEqual(c["baseline"]["spine_sha"], "d4de053fd33fffddd031a3c8b58c10004baaee20")
        self.assertEqual(c["new"]["spine_sha"], "f745fef398f6198eef8c40904eb449729381c291")


def column(type_, count=1):
    return {"type": type_, "count": count, "variable": False, "enum": None}


# A decoded fixture in the format of tools/fixtures/decode_raw.py, reduced to one product. The
# tests use it, not tests/data/fixtures, which a run of follow_spine.py replaces.
DECODED = {
    "case": "current_v1", "layout": "V1", "num_events": 1,
    "products": {
        "reco_particles": {
            "kind": "objects", "class_name": "RecoParticle", "scalar": False,
            "columns": {"id": column("int64"), "csda_ke": column("float64"),
                        "start_point": column("float32", 3)},
            "events": [[{"id": 0, "csda_ke": 12.5, "start_point": [1.0, 2.0, 3.0]},
                        {"id": 1, "csda_ke": "nan", "start_point": ["nan", "nan", "nan"]}]],
        },
    },
}


class Columns(unittest.TestCase):
    """The column folders: stored-width drift, unexplained columns, default-only fields."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        self.old, self.new = root / "old", root / "new"
        self.old.mkdir()
        self.new.mkdir()
        self.doc = copy.deepcopy(DECODED)
        (self.old / "current_v1.expected.json").write_text(json.dumps(self.doc))

    def tearDown(self):
        self.tmp.cleanup()

    def write_new(self, change):
        doc = copy.deepcopy(self.doc)
        change(doc)
        (self.new / "current_v1.expected.json").write_text(json.dumps(doc))

    def run_classify(self, new_schema=None):
        return classify.classify(BASE, new_schema or copy.deepcopy(BASE), self.old, self.new)

    def test_same_columns(self):
        self.write_new(lambda d: None)
        c = self.run_classify()
        self.assertEqual(c["column_diffs"], [])
        self.assertEqual(c["change_kinds"], [])
        self.assertIn("FlashHypothesis", c["unwritten_classes"])
        self.assertNotIn("RecoParticle", c["unwritten_classes"])

    def test_a_stored_width_change(self):
        def widen(d):
            d["products"]["reco_particles"]["columns"]["start_point"]["type"] = "float64"
        self.write_new(widen)
        c = self.run_classify()
        self.assertEqual(c["change_kinds"], ["stored_width_changed"])
        self.assertEqual(c["proposed_bump"], "person")
        self.assertEqual(c["column_diffs"][0]["change"], "width")

    def test_a_stored_kind_change(self):
        def retype(d):
            d["products"]["reco_particles"]["columns"]["start_point"]["type"] = "int32"
        self.write_new(retype)
        self.assertEqual(self.run_classify()["change_kinds"], ["stored_type_changed"])

    def test_a_retype_the_schema_explains(self):
        def widen(d):
            d["products"]["reco_particles"]["columns"]["csda_ke"]["type"] = "float32"
        self.write_new(widen)
        c = self.run_classify(mutated(CHANGE["an element retype"]))
        self.assertEqual(c["change_kinds"], ["element_retyped"])
        self.assertTrue(c["column_diffs"][0]["explained_by_schema"])

    def test_a_column_no_field_explains(self):
        def drop(d):
            del d["products"]["reco_particles"]["columns"]["csda_ke"]
        self.write_new(drop)
        self.assertEqual(self.run_classify()["change_kinds"], ["stored_columns_changed"])

    def test_an_added_field_held_only_at_its_default(self):
        def add_column(d):
            p = d["products"]["reco_particles"]
            p["columns"]["new_score"] = dict(p["columns"]["csda_ke"])
            for rows in p["events"]:
                for row in rows:
                    row["new_score"] = "nan"
        self.write_new(add_column)
        c = self.run_classify(mutated(add_field))
        self.assertEqual(c["change_kinds"], ["field_added"])
        self.assertEqual(c["default_only_fields"], ["RecoParticle.new_score"])
        self.assertTrue(any("RecoParticle.new_score only at its default" in f
                            for f in c["follow_up"]))


class Warnings(unittest.TestCase):
    def test_new_warnings_and_substitutions_need_attention(self):
        unused = {"kind": "unused_label", "name": "truth_interactions.interaction_scheme",
                  "detail": "GIBUU"}
        c = classify.classify(BASE, copy.deepcopy(BASE), warnings=[unused])
        self.assertEqual(c["warnings_new"], [unused])
        self.assertTrue(c["needs_attention"])
        # the same warning in the baseline is not new
        c = classify.classify(BASE, copy.deepcopy(BASE), warnings=[unused],
                              baseline_warnings=[dict(unused, detail="other")])
        self.assertEqual(c["warnings_new"], [])
        self.assertFalse(c["needs_attention"])
        substitution = {"kind": "substitution", "name": "NuInteractionScheme.GIBUU",
                        "detail": "UNKNOWN"}
        c = classify.classify(BASE, copy.deepcopy(BASE), warnings=[substitution])
        self.assertEqual(c["substitutions"], [substitution])
        self.assertEqual(c["warnings_new"], [])
        self.assertTrue(c["needs_attention"])

    def test_a_schema_warning_is_new_by_kind_and_name(self):
        new = mutated(lambda s: s["warnings"].append(
            {"kind": "unlisted_class", "name": "TestRecord", "detail": "..."}))
        c = classify.classify(BASE, new)
        self.assertEqual([w["name"] for w in c["warnings_new"]], ["TestRecord"])
        self.assertTrue(c["needs_attention"])
        self.assertEqual(c["change_kinds"], [])  # a warning is not a change kind

    def test_a_class_added_needs_attention(self):
        c = classify.classify(BASE, mutated(CHANGE["a class added"]))
        self.assertTrue(c["needs_attention"])


class BumpTable(unittest.TestCase):
    def test_every_change_kind_has_a_row_and_every_row_is_reachable(self):
        reported = set()
        for _, change, _, _ in CASES:
            reported |= set(classify.classify(BASE, mutated(change))["change_kinds"])
        reported |= {"stored_width_changed", "stored_type_changed", "stored_columns_changed"}
        self.assertEqual(reported, set(classify.BUMP_TABLE))
        self.assertTrue(set(classify.BUMP_TABLE.values()) <= set(classify.BUMPS))

    def test_an_unknown_kind_is_a_person_decision(self):
        self.assertEqual(classify.bump_of("something_new"), "person")


class CommandLine(unittest.TestCase):
    def test_writes_json_and_markdown(self):
        with tempfile.TemporaryDirectory() as tmp:
            new = Path(tmp) / "new.json"
            new.write_text(json.dumps(mutated(add_field)))
            out, md = Path(tmp) / "c.json", Path(tmp) / "c.md"
            with contextlib.redirect_stdout(io.StringIO()):
                code = classify.main([str(DATA / "spine-1.4.0.json"), str(new), "--json",
                                      str(out), "--md", str(md)])
            self.assertEqual(code, 0)
            self.assertEqual(json.loads(out.read_text())["proposed_bump"], "patch")
            text = md.read_text()
            self.assertIn("added `new_score` at 10", text)
            self.assertIn("proposed bump: **patch**", text)

    def test_bad_input_exits_2(self):
        with tempfile.TemporaryDirectory() as tmp:
            bad = Path(tmp) / "bad.json"
            bad.write_text("{}")
            proc = subprocess.run([sys.executable, str(HERE.parent / "classify.py"),
                                   str(DATA / "spine-1.4.0.json"), str(bad), "--json",
                                   str(Path(tmp) / "c.json"), "--md", str(Path(tmp) / "c.md")],
                                  capture_output=True, text=True)
            self.assertEqual(proc.returncode, 2)
            self.assertIn("new schema has no 'spine_version'", proc.stderr)


if __name__ == "__main__":
    unittest.main()
