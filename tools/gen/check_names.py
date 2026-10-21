#!/usr/bin/env python3
"""Check that no identifier of the generated model is a macro of a common system header.

    python3 tools/gen/check_names.py [--cxx g++] [--root .] [--include hdf5.h ...]

The compiler is asked for the macros defined after including a broad set of C, C++ and POSIX
headers (`-dM -E`, GNU mode, where `unix` and `linux` are macros too). Every identifier that
appears in the generated headers and sources, outside comments and string literals, is then looked
up in that list. A hit means a SPINE name needs renaming; add it to RESERVED in generate.py (the
enumerators and members listed there get a trailing underscore or are rejected).
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

HEADERS = """
cassert cctype cerrno cfenv cfloat cinttypes climits clocale cmath csetjmp csignal cstdarg cstddef
cstdint cstdio cstdlib cstring ctime cwchar cwctype atomic chrono filesystem fstream iostream limits
map memory mutex random regex string thread vector
sys/types.h sys/stat.h sys/sysmacros.h sys/time.h sys/resource.h sys/socket.h sys/wait.h
sys/select.h sys/ioctl.h sys/mman.h unistd.h fcntl.h termios.h pthread.h dirent.h netinet/in.h
arpa/inet.h regex.h limits.h math.h stdlib.h stdio.h string.h strings.h signal.h errno.h float.h
inttypes.h complex.h ctype.h locale.h wchar.h stdint.h assert.h time.h
""".split()

COMMENTS_AND_STRINGS = re.compile(
    r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'", re.S
)
IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
OWN_MACROS = re.compile(r"SPINE_[A-Z_]+|X")


def system_macros(cxx, extra):
    source = "".join(f"#if __has_include(<{h}>)\n#include <{h}>\n#endif\n" for h in HEADERS + extra)
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "all.cpp"
        path.write_text(source)
        out = subprocess.run(
            [cxx, "-std=gnu++17", "-dM", "-E", "-x", "c++", str(path)],
            check=True,
            stdout=subprocess.PIPE,
            universal_newlines=True,
        ).stdout
    return {m.group(1) for m in re.finditer(r"^#define (\w+)", out, re.M)}


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    parser.add_argument("--root", default=".")
    parser.add_argument("--include", action="append", default=[], help="extra header to include")
    args = parser.parse_args()
    root = Path(args.root)

    macros = system_macros(args.cxx, args.include)
    files = sorted(
        [*(root / "include/spine").glob("model/*.hpp"), root / "include/spine/model.hpp"]
        + [*(root / "src/model").glob("*")]
    )
    clashes = {}
    for path in files:
        text = COMMENTS_AND_STRINGS.sub(" ", path.read_text())
        for name in set(IDENT.findall(text)):
            if name in macros and not OWN_MACROS.fullmatch(name):
                clashes.setdefault(name, []).append(path.name)
    for name, where in sorted(clashes.items()):
        print(f"macro clash: {name} in {', '.join(sorted(where))}")
    print(f"{len(macros)} macros, {len(files)} files: {len(clashes)} clash(es)")
    return 1 if clashes else 0


if __name__ == "__main__":
    sys.exit(main())
