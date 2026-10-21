#!/usr/bin/env python3
"""Writes tests/golden_members.inc: the member names of every generated model class, as X-macros.

    python3 tools/fixtures/make_member_list.py           # write
    python3 tools/fixtures/make_member_list.py --check   # exit 1 if the file is stale (a ctest)

tests/test_golden.cpp expands GOLDEN_MEMBERS_<Class>(X) to compare every member of every object
the Reader returns with the value h5py decoded from the fixture, so that a field whose table
row assigns the wrong member, or whose type or default is wrong, cannot go unnoticed. The names
come from the committed schema, which was dumped from SPINE: the list is not the model's own.
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tests" / "golden_members.inc"


def render():
    schema_files = sorted((ROOT / "schema").glob("spine-*.json"))
    if len(schema_files) != 1:
        sys.exit(f"make_member_list: expected one schema/spine-*.json, found {len(schema_files)}")
    schema = json.loads(schema_files[0].read_text())
    lines = [
        f"// GENERATED from schema/{schema_files[0].name} by tools/fixtures/make_member_list.py. "
        "Do not edit.",
        "// The members of each model class, in SPINE's order, for tests/test_golden.cpp.",
    ]
    for cls, info in schema["classes"].items():
        lines.append("")
        lines.append(f"#define GOLDEN_MEMBERS_{cls}(X) \\")
        row = "   "
        names = [f["name"] for f in info["fields"]]
        for i, name in enumerate(names):
            item = f" X({name})"
            if len(row) + len(item) > 94:
                lines.append(row + " \\")
                row = "   "
            row += item
        lines.append(row)
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    text = render()
    if args.check:
        if not OUT.exists() or OUT.read_text() != text:
            print(f"stale: {OUT.relative_to(ROOT)}")
            return 1
        return 0
    OUT.write_text(text)
    print(f"wrote {OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
