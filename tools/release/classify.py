#!/usr/bin/env python3
"""Describe the change between two schema dumps of SPINE, and propose a version bump.

    classify.py BASELINE NEW [--old-columns DIR --new-columns DIR] [--warnings FILE]
                [--baseline-warnings FILE] [--effects FILE] --json OUT --md OUT

* BASELINE and NEW are ``schema/spine-*.json`` files written by ``tools/gen/dump_schema.py``.
* ``--old-columns`` and ``--new-columns`` are folders of ``*.expected.json`` files (the decoded
  fixtures): the committed current set and the new tag's. The cases both folders hold are
  compared column by column; that is where a change of a stored width shows. Without them there
  is no column comparison.
* ``--warnings`` is a JSON list of ``{kind, name, detail}``: the fixture run's warnings and
  substitutions (``kind`` ``"substitution"``). The new schema's own ``warnings`` are added to it.
  ``--baseline-warnings`` is the same for the baseline (the committed manifest's), added to the
  baseline schema's. A warning is new when its ``(kind, name)`` is not among the baseline's.
* ``--effects`` is text embedded verbatim: what the regenerated ``spine-dump`` prints for the
  previous current set.

The classifier never decides "identical": ``follow_spine.py`` does that by generating the C++
from both schemas. This tool describes what differs, kind by kind, and proposes the bump of the
highest kind found, in the order none < patch < minor < person (BUMP_TABLE; a kind it does not
list is "person"). A person writes the bump into ``project(VERSION)``.

Exit codes: 0 written; 2 bad input (an unreadable file or a missing key).
"""

import argparse
import json
import sys
from pathlib import Path

BUMPS = ["none", "patch", "minor", "person"]

# Every change kind the classifier reports, with the bump it proposes. A kind missing here is
# "person" (bump_of). test_classify.py checks that each kind classify() can report has a row.
BUMP_TABLE = {
    # comments and descriptor doc strings only: the regenerated model may merge without a tag
    "doc_changed": "none",
    "module_changed": "none",
    # additions (patch is provisional until the strict-mode decision, D6)
    "field_added": "patch",
    "enum_member_added": "patch",
    "enum_added": "patch",
    "scheme_table_added": "patch",
    # removals and retypes: a minor release, the breaking change of a 0.x library
    "field_removed": "minor",
    "class_removed": "minor",
    "enum_member_removed": "minor",
    "enum_removed": "minor",
    "scheme_table_removed": "minor",
    "shape_retyped": "minor",
    "element_retyped": "minor",
    "enum_member_renumbered": "minor",
    # a person decides
    "default_changed": "person",
    "units_changed": "person",
    "flags_changed": "person",
    "reference_changed": "person",
    "field_reordered": "person",
    "class_added": "person",
    "enum_aliases_changed": "person",
    "scheme_table_changed": "person",
    "type_check_changed": "person",
    "writer_format_changed": "person",
    "stored_width_changed": "person",
    "stored_type_changed": "person",
    "stored_columns_changed": "person",
}

TYPE_KEYS = ("storage", "element", "length", "enum")
CHANGED_KEYS = {  # field key -> change kind
    "default": "default_changed",
    "units": "units_changed",
    "flags": "flags_changed",
    "reference": "reference_changed",
    "reference_space": "reference_changed",
    "doc": "doc_changed",
}
MODULE_KEYS = ("defined_in", "doc_source")
DOC_ONLY = {"doc_changed", "module_changed"}


class InputError(Exception):
    """An input file cannot be read or lacks a key."""


def bump_of(kind):
    return BUMP_TABLE.get(kind, "person")


def kind_of_type(element):
    if element in ("bool", "string"):
        return element
    if element.startswith(("int", "uint")):
        return "integer"
    if element.startswith("float"):
        return "float"
    return element


# ---------------------------------------------------------------------------------------------
# Schemas
# ---------------------------------------------------------------------------------------------

def diff_class(old, new):
    """The changes of one class present in both schemas."""
    old_fields = {f["name"]: (i, f) for i, f in enumerate(old["fields"])}
    new_fields = {f["name"]: (i, f) for i, f in enumerate(new["fields"])}
    out = {"added": [], "removed": [], "retyped": [], "reordered": False, "changed": []}
    for name, (i, f) in new_fields.items():
        if name not in old_fields:
            out["added"].append({"name": name, "position": i, "storage": f["storage"],
                                 "element": f["element"], "length": f["length"]})
    out["removed"] = [name for name in old_fields if name not in new_fields]
    common_old = [n for n in old_fields if n in new_fields]
    common_new = [n for n in new_fields if n in old_fields]
    out["reordered"] = common_old != common_new
    for name in common_old:
        a, b = old_fields[name][1], new_fields[name][1]
        type_a = {k: a.get(k) for k in TYPE_KEYS}
        type_b = {k: b.get(k) for k in TYPE_KEYS}
        if type_a != type_b:
            out["retyped"].append({
                "name": name, "from": type_a, "to": type_b,
                "shape_changed": (a["storage"], a["length"]) != (b["storage"], b["length"]),
            })
        for key in list(CHANGED_KEYS) + list(MODULE_KEYS):
            if a.get(key) != b.get(key):
                out["changed"].append({"name": name, "key": key, "from": a.get(key),
                                       "to": b.get(key)})
    for key in ("module", "mro", "role", "doc"):
        if old.get(key) != new.get(key):
            out["changed"].append({"name": None, "key": key, "from": old.get(key),
                                   "to": new.get(key)})
    return out


def class_kinds(diff):
    """The change kinds of one class diff."""
    kinds = set()
    if diff["added"]:
        kinds.add("field_added")
    if diff["removed"]:
        kinds.add("field_removed")
    if diff["reordered"]:
        kinds.add("field_reordered")
    for r in diff["retyped"]:
        kinds.add("shape_retyped" if r["shape_changed"] else "element_retyped")
    for c in diff["changed"]:
        if c["name"] is None:
            kinds.add("doc_changed" if c["key"] in ("role", "doc") else "module_changed")
        elif c["key"] in MODULE_KEYS:
            kinds.add("module_changed")
        else:
            kinds.add(CHANGED_KEYS[c["key"]])
    return kinds


def diff_enum(old, new):
    members_a, members_b = dict(map(tuple, old["members"])), dict(map(tuple, new["members"]))
    out = {
        "added": [{"name": n, "value": v} for n, v in members_b.items() if n not in members_a],
        "removed": [{"name": n, "value": v} for n, v in members_a.items() if n not in members_b],
        "renumbered": [{"name": n, "from": members_a[n], "to": v}
                       for n, v in members_b.items() if n in members_a and members_a[n] != v],
        "aliases": None,
        "doc_changed": old.get("doc") != new.get("doc") or old.get("module") != new.get("module"),
    }
    if old.get("aliases", []) != new.get("aliases", []):
        out["aliases"] = {"from": old.get("aliases", []), "to": new.get("aliases", [])}
    return out


def diff_scheme_tables(old, new):
    out = []
    for attr in sorted(set(old) | set(new)):
        a, b = old.get(attr, {}), new.get(attr, {})
        for scheme in sorted(set(a) | set(b)):
            if a.get(scheme) != b.get(scheme):
                out.append({"attribute": attr, "scheme": scheme, "from": a.get(scheme),
                            "to": b.get(scheme)})
    return out


def diff_writer_format(old, new):
    if old is None or new is None:
        return [] if old == new else [{"key": "*", "from": old, "to": new}]
    return [{"key": k, "from": old.get(k), "to": new.get(k)}
            for k in sorted(set(old) | set(new)) if old.get(k) != new.get(k)]


def diff_type_check(old, new):
    def entries(tc):
        return {json.dumps(m, sort_keys=True) for m in (tc or {}).get("mismatches", [])}
    a, b = entries(old), entries(new)
    if a == b:
        return None
    return {"added": [json.loads(x) for x in sorted(b - a)],
            "removed": [json.loads(x) for x in sorted(a - b)]}


# ---------------------------------------------------------------------------------------------
# Decoded columns
# ---------------------------------------------------------------------------------------------

def read_columns(folder):
    """{case: {product: {"class_name", "columns": {name: info}}}} of the *.expected.json files."""
    out = {}
    for path in sorted(Path(folder).glob("*.expected.json")):
        doc = load_json(path)
        out[path.name[: -len(".expected.json")]] = {
            key: {"class_name": p.get("class_name"), "columns": p.get("columns", {})}
            for key, p in doc.get("products", {}).items()
        }
    return out


def diff_columns(old, new, retyped, added, removed):
    """Column differences of the cases both folders hold. ``retyped``, ``added`` and ``removed``
    are sets of (class, field) the schema diff explains."""
    diffs, kinds = [], set()
    for case in sorted(set(old) & set(new)):
        for product in sorted(set(old[case]) | set(new[case])):
            a, b = old[case].get(product), new[case].get(product)
            if a is None or b is None:
                diffs.append({"case": case, "product": product,
                              "change": "product added" if a is None else "product removed"})
                kinds.add("stored_columns_changed")
                continue
            cls = b["class_name"] or a["class_name"]
            for name in sorted(set(a["columns"]) | set(b["columns"])):
                ca, cb = a["columns"].get(name), b["columns"].get(name)
                if ca is None or cb is None:
                    explained = (cls, name) in (added if ca is None else removed)
                    diffs.append({"case": case, "product": product, "column": name,
                                  "change": "column added" if ca is None else "column removed",
                                  "explained_by_schema": explained})
                    if not explained:
                        kinds.add("stored_columns_changed")
                    continue
                for key in ("type", "count", "variable", "enum"):
                    if ca.get(key) == cb.get(key):
                        continue
                    if key == "type" and kind_of_type(ca["type"]) == kind_of_type(cb["type"]):
                        change = "width"
                    else:
                        change = key
                    explained = (cls, name) in retyped
                    diffs.append({"case": case, "product": product, "column": name,
                                  "change": change, "from": ca.get(key), "to": cb.get(key),
                                  "explained_by_schema": explained})
                    if key == "enum" or explained:
                        continue  # enum labels follow the enum diff
                    kinds.add("stored_width_changed" if change == "width"
                              else "stored_type_changed")
    return diffs, kinds


def default_only(new_columns, new_schema, fields):
    """Of ``fields`` ((class, field) pairs), those that no row of any case holds at a value other
    than the schema's default: the fixtures test them only at their default."""
    varied = set()
    defaults = {(c, f["name"]): f["default"] for c, d in new_schema["classes"].items()
                for f in d["fields"]}
    for path in sorted(Path(new_columns).glob("*.expected.json")):
        doc = load_json(path)
        for product in doc.get("products", {}).values():
            if product.get("kind") != "objects":
                continue
            cls = product.get("class_name")
            for rows in product.get("events", []):
                for row in rows:
                    for key in fields:
                        if key[0] == cls and key[1] in row and row[key[1]] != defaults.get(key):
                            varied.add(key)
    return [f"{c}.{n}" for (c, n) in fields if (c, n) not in varied]


def written_classes(columns):
    return {p["class_name"] for case in columns.values() for p in case.values() if p["class_name"]}


# ---------------------------------------------------------------------------------------------
# The classification
# ---------------------------------------------------------------------------------------------

def load_json(path):
    try:
        return json.loads(Path(path).read_text())
    except (OSError, ValueError) as e:
        raise InputError(f"{path}: {e}") from e


def warning_key(w):
    return (w.get("kind"), w.get("name"))


def classify(baseline, new, old_columns=None, new_columns=None, warnings=(),
             baseline_warnings=(), effects=None, baseline_path=None, new_path=None):
    """The classification of the change from schema ``baseline`` to ``new`` (dicts)."""
    for doc, label in ((baseline, "baseline"), (new, "new")):
        for key in ("spine_version", "classes", "enums", "scheme_tables"):
            if key not in doc:
                raise InputError(f"{label} schema has no '{key}'")
    kinds = set()
    out = {
        "baseline": {"path": baseline_path, "spine_version": baseline["spine_version"],
                     "spine_sha": baseline.get("spine_sha")},
        "new": {"path": new_path, "spine_version": new["spine_version"],
                "spine_sha": new.get("spine_sha")},
    }

    # classes and fields
    old_classes, new_classes = baseline["classes"], new["classes"]
    out["classes_added"] = [c for c in new_classes if c not in old_classes]
    out["classes_removed"] = [c for c in old_classes if c not in new_classes]
    if out["classes_added"]:
        kinds.add("class_added")
    if out["classes_removed"]:
        kinds.add("class_removed")
    out["classes"] = {}
    added, removed, retyped = set(), set(), set()
    for name in old_classes:
        if name not in new_classes:
            continue
        diff = diff_class(old_classes[name], new_classes[name])
        kinds |= class_kinds(diff)
        added |= {(name, f["name"]) for f in diff["added"]}
        removed |= {(name, f) for f in diff["removed"]}
        retyped |= {(name, r["name"]) for r in diff["retyped"]}
        if any(diff[k] for k in ("added", "removed", "retyped", "reordered", "changed")):
            out["classes"][name] = diff
    for name in out["classes_added"]:
        added |= {(name, f["name"]) for f in new_classes[name]["fields"]}
    for name in out["classes_removed"]:
        removed |= {(name, f["name"]) for f in old_classes[name]["fields"]}

    # enums and scheme tables
    old_enums, new_enums = baseline["enums"], new["enums"]
    out["enums_added"] = [e for e in new_enums if e not in old_enums]
    out["enums_removed"] = [e for e in old_enums if e not in new_enums]
    if out["enums_added"]:
        kinds.add("enum_added")
    if out["enums_removed"]:
        kinds.add("enum_removed")
    out["enums"] = {}
    for name in old_enums:
        if name not in new_enums:
            continue
        diff = diff_enum(old_enums[name], new_enums[name])
        for key, kind in (("added", "enum_member_added"), ("removed", "enum_member_removed"),
                          ("renumbered", "enum_member_renumbered"),
                          ("aliases", "enum_aliases_changed"), ("doc_changed", "doc_changed")):
            if diff[key]:
                kinds.add(kind)
        if any(diff.values()):
            out["enums"][name] = diff
    out["scheme_tables_changed"] = diff_scheme_tables(baseline["scheme_tables"],
                                                      new["scheme_tables"])
    for change in out["scheme_tables_changed"]:
        if change["from"] is None:
            kinds.add("scheme_table_added")
        elif change["to"] is None:
            kinds.add("scheme_table_removed")
        else:
            kinds.add("scheme_table_changed")

    # what the dump records beyond the model
    out["type_check_changed"] = diff_type_check(baseline.get("type_check"), new.get("type_check"))
    if out["type_check_changed"]:
        kinds.add("type_check_changed")
    out["writer_format_changed"] = diff_writer_format(baseline.get("writer_format"),
                                                      new.get("writer_format"))
    if out["writer_format_changed"]:
        kinds.add("writer_format_changed")

    # decoded columns
    out["column_diffs"] = []
    out["default_only_fields"] = []
    out["unwritten_classes"] = []
    if old_columns and new_columns:
        out["column_diffs"], column_kinds = diff_columns(
            read_columns(old_columns), read_columns(new_columns), retyped, added, removed)
        kinds |= column_kinds
    if new_columns:
        out["default_only_fields"] = default_only(new_columns, new, sorted(added))
        written = written_classes(read_columns(new_columns))
        out["unwritten_classes"] = [c for c in new_classes if c not in written]

    # warnings and substitutions
    new_warnings = list(new.get("warnings", [])) + list(warnings)
    known = {warning_key(w) for w in list(baseline.get("warnings", [])) + list(baseline_warnings)}
    out["warnings_new"], seen = [], set()
    for w in new_warnings:
        key = warning_key(w)
        if key not in known and key not in seen and w.get("kind") != "substitution":
            out["warnings_new"].append(w)
            seen.add(key)
    out["substitutions"] = [w for w in warnings if w.get("kind") == "substitution"]
    out["effects_on_previous_files"] = effects

    # the bump
    out["change_kinds"] = sorted(kinds)
    out["module_only"] = bool(kinds) and kinds <= DOC_ONLY
    bump = max((bump_of(k) for k in kinds), key=BUMPS.index, default="none")
    out["proposed_bump"] = bump
    out["reasons"] = [{"kind": k, "bump": bump_of(k)}
                      for k in sorted(kinds, key=lambda k: (-BUMPS.index(bump_of(k)), k))]
    out["needs_attention"] = bool(out["classes_added"] or out["warnings_new"]
                                  or out["substitutions"])
    out["follow_up"] = follow_up(out, added)
    return out


def follow_up(out, added):
    items = []
    for field in out["default_only_fields"]:
        items.append(f"the fixtures hold {field} only at its default: set it in "
                     "tools/fixtures/content.py")
    for cls in out["unwritten_classes"]:
        items.append(f"no current fixture writes {cls}: its fields are not checked against a file")
    for w in out["warnings_new"]:
        items.append(f"new warning {w.get('kind')} {w.get('name')}: {w.get('detail', '')}".rstrip())
    for s in out["substitutions"]:
        items.append(f"substitution {s.get('name')}: {s.get('detail', '')}".rstrip())
    if added:
        items.append("a CAFMaker that starts reading an added field needs a row in "
                     "tests/test_contracts.cpp (tools/release/scan_consumer_fields.py)")
        items.append("after an added field, ReaderOptions::strict refuses files of the previous "
                     "SPINE release (decision D6 is open)")
    if out["proposed_bump"] == "patch":
        items.append("the patch bump for additions is provisional until decision D6")
    if out["classes_added"]:
        items.append("a new class needs a product name in the Reader and, if a CAFMaker reads "
                     "it, contract rows: " + ", ".join(out["classes_added"]))
    return items


# ---------------------------------------------------------------------------------------------
# Markdown
# ---------------------------------------------------------------------------------------------

def render_markdown(c):
    b, n = c["baseline"], c["new"]
    lines = [f"# SPINE {b['spine_version']} -> {n['spine_version']}", ""]
    lines.append(f"- baseline: `{b['path']}` (SPINE {b['spine_version']}, `{b['spine_sha']}`)")
    lines.append(f"- new: `{n['path']}` (SPINE {n['spine_version']}, `{n['spine_sha']}`)")
    lines.append(f"- proposed bump: **{c['proposed_bump']}**"
                 + ("" if c["proposed_bump"] != "person" else " (a person decides)"))
    lines.append(f"- needs attention: {'yes' if c['needs_attention'] else 'no'}")
    lines.append("")
    lines.append("## Why")
    lines.append("")
    if c["reasons"]:
        lines += [f"- `{r['kind']}`: {r['bump']}" for r in c["reasons"]]
    else:
        lines.append("- no change kind found")
    lines.append("")

    lines.append("## Classes and fields")
    lines.append("")
    for key, label in (("classes_added", "classes added"), ("classes_removed", "classes removed")):
        if c[key]:
            lines.append(f"- {label}: " + ", ".join(f"`{x}`" for x in c[key]))
    for cls, d in c["classes"].items():
        lines.append(f"- `{cls}`")
        for f in d["added"]:
            length = f"[{f['length']}]" if f["length"] else ""
            lines.append(f"  - added `{f['name']}` at {f['position']}: {f['storage']} "
                         f"{f['element']}{length}")
        for f in d["removed"]:
            lines.append(f"  - removed `{f}`")
        for r in d["retyped"]:
            lines.append(f"  - retyped `{r['name']}`: {r['from']} -> {r['to']}"
                         + (" (shape)" if r["shape_changed"] else ""))
        if d["reordered"]:
            lines.append("  - field order changed")
        for ch in d["changed"]:
            where = f"`{ch['name']}` " if ch["name"] else "class "
            lines.append(f"  - {where}{ch['key']}: {json.dumps(ch['from'])} -> "
                         f"{json.dumps(ch['to'])}")
    if not (c["classes"] or c["classes_added"] or c["classes_removed"]):
        lines.append("- none")
    lines.append("")

    lines.append("## Enums")
    lines.append("")
    for key, label in (("enums_added", "enums added"), ("enums_removed", "enums removed")):
        if c[key]:
            lines.append(f"- {label}: " + ", ".join(f"`{x}`" for x in c[key]))
    for name, d in c["enums"].items():
        parts = []
        if d["added"]:
            parts.append("added " + ", ".join(f"{m['name']}={m['value']}" for m in d["added"]))
        if d["removed"]:
            parts.append("removed " + ", ".join(m["name"] for m in d["removed"]))
        if d["renumbered"]:
            renumbered = [f"{m['name']} {m['from']}->{m['to']}" for m in d["renumbered"]]
            parts.append("renumbered " + ", ".join(renumbered))
        if d["aliases"]:
            parts.append(f"aliases {d['aliases']['from']} -> {d['aliases']['to']}")
        if d["doc_changed"]:
            parts.append("doc or module changed")
        lines.append(f"- `{name}`: " + "; ".join(parts))
    for t in c["scheme_tables_changed"]:
        lines.append(f"- scheme table {t['attribute']}[{t['scheme']}]: {t['from']} -> {t['to']}")
    if not (c["enums"] or c["enums_added"] or c["enums_removed"] or c["scheme_tables_changed"]):
        lines.append("- none")
    lines.append("")

    lines.append("## Writer and type check")
    lines.append("")
    if c["writer_format_changed"]:
        lines += [f"- writer_format {w['key']}: {json.dumps(w['from'])} -> {json.dumps(w['to'])}"
                  for w in c["writer_format_changed"]]
    else:
        lines.append("- writer_format unchanged")
    if c["type_check_changed"]:
        tc = c["type_check_changed"]
        lines.append(f"- type_check: {len(tc['added'])} mismatch(es) added, "
                     f"{len(tc['removed'])} removed")
        lines += [f"  - {m.get('class')}.{m.get('field')}: {m.get('detail')}" for m in tc["added"]]
    else:
        lines.append("- type_check unchanged")
    lines.append("")

    lines.append("## Decoded columns of the current set")
    lines.append("")
    explained = [d for d in c["column_diffs"] if d.get("explained_by_schema")]
    for d in c["column_diffs"]:
        if d.get("explained_by_schema"):
            continue
        what = d["change"]
        if "from" in d:
            what += f" {d['from']} -> {d['to']}"
        col = f".{d['column']}" if "column" in d else ""
        lines.append(f"- {d['case']} {d['product']}{col}: {what}")
    if explained:
        columns = sorted({d["column"] for d in explained})
        lines.append(f"- {len(explained)} difference(s) the schema changes above explain "
                     f"({', '.join(columns)}; all listed in classification.json)")
    if not c["column_diffs"]:
        lines.append("- no difference (or no column folders given)")
    lines.append("")

    if c["follow_up"]:
        lines.append("## Follow-up")
        lines.append("")
        lines += [f"- [ ] {item}" for item in c["follow_up"]]
        lines.append("")

    if c["effects_on_previous_files"]:
        lines.append("## The regenerated spine-dump on the previous current set")
        lines.append("")
        lines.append("```text")
        lines.append(c["effects_on_previous_files"].rstrip("\n"))
        lines.append("```")
        lines.append("")

    lines.append("## Checklist (tools/gen/README.md, What to check afterwards)")
    lines.append("")
    lines += [
        "- [ ] the type check printed by `dump_schema.py` says `0 mismatch(es)`",
        "- [ ] `git diff schema/`: every difference above is expected",
        "- [ ] `python3 tools/gen/check_names.py` passes",
        "- [ ] build and ctest; if `spine_reader_contracts` fails, talk to the CAFMaker "
        "maintainers before editing `tests/test_contracts.cpp`",
        "- [ ] gcc 12, C++17 in the SL7 container (`ci/sl7-ups.sh`)",
        "- [ ] docs and units (`doc_source` in the JSON)",
        f"- [ ] write the bump (proposed: {c['proposed_bump']}) into `project(VERSION)`",
    ]
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------------------------

def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("baseline")
    p.add_argument("new")
    p.add_argument("--old-columns")
    p.add_argument("--new-columns")
    p.add_argument("--warnings")
    p.add_argument("--baseline-warnings")
    p.add_argument("--effects")
    p.add_argument("--json", required=True)
    p.add_argument("--md", required=True)
    args = p.parse_args(argv)
    if bool(args.old_columns) != bool(args.new_columns):
        p.error("--old-columns and --new-columns go together")
    try:
        for folder in (args.old_columns, args.new_columns):
            if folder and not Path(folder).is_dir():
                raise InputError(f"no folder {folder}")
        warnings = load_json(args.warnings) if args.warnings else []
        baseline_warnings = load_json(args.baseline_warnings) if args.baseline_warnings else []
        if not isinstance(warnings, list) or not isinstance(baseline_warnings, list):
            raise InputError("--warnings and --baseline-warnings hold JSON lists")
        effects = None
        if args.effects:
            try:
                effects = Path(args.effects).read_text()
            except OSError as e:
                raise InputError(f"{args.effects}: {e}") from e
        result = classify(load_json(args.baseline), load_json(args.new), args.old_columns,
                          args.new_columns, warnings, baseline_warnings, effects,
                          args.baseline, args.new)
    except (InputError, KeyError, TypeError) as e:
        print(f"classify: {type(e).__name__}: {e}", file=sys.stderr)
        return 2
    Path(args.json).write_text(json.dumps(result, indent=1) + "\n")
    Path(args.md).write_text(render_markdown(result))
    print(f"classify: SPINE {result['baseline']['spine_version']} -> "
          f"{result['new']['spine_version']}: proposed bump {result['proposed_bump']}"
          f" ({', '.join(result['change_kinds']) or 'no change kind'})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
