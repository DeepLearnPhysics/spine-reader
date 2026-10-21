#!/usr/bin/env python3
"""Generate the C++ model of spine-reader from a schema dump of SPINE.

    python3 tools/gen/generate.py [--schema schema/spine-<version>.json] [--root .] [--check]

Reads only the JSON written by ``dump_schema.py`` (no SPINE import) and writes, under ``--root``:

    include/spine/model/common.hpp         NaN constants shared by the class headers
    include/spine/model/enums.hpp          one `enum class X : std::int64_t` per SPINE enum
    include/spine/model/<snake_class>.hpp  one struct per class
    include/spine/model.hpp                umbrella header and SPINE_READER_FOR_EACH_CLASS
    src/model/enums.cpp                    EnumDescriptors
    src/model/<snake_class>.cpp            field table, descriptor(), static_asserts
    src/model/field_check.hpp              compile-time checks used by the .cpp files

``--check`` writes nothing and exits 1 when a file on disk differs from what would be generated.
The output is deterministic: the same JSON always gives byte-identical files.
"""

import argparse
import glob
import json
import re
import struct
import sys
from pathlib import Path

WIDTH = 100

# Identifiers that must not be used as C++ enumerators or members because a standard or POSIX
# header defines them as macros (or they are keywords). The list is checked against the real
# macros of the system headers by tools/gen/check_names.py.
RESERVED = {
    # <cstdio>, <cstdlib>, <cstddef>, <cerrno>
    "NULL",
    "EOF",
    "BUFSIZ",
    "EXIT_SUCCESS",
    "EXIT_FAILURE",
    "RAND_MAX",
    "SEEK_SET",
    "SEEK_CUR",
    "SEEK_END",
    "FILENAME_MAX",
    "FOPEN_MAX",
    "TMP_MAX",
    "L_tmpnam",
    "errno",
    "stdin",
    "stdout",
    "stderr",
    "assert",
    "offsetof",
    # booleans
    "TRUE",
    "FALSE",
    "true",
    "false",
    # <cmath>, <math.h> (SVID error codes and constants)
    "DOMAIN",
    "SING",
    "OVERFLOW",
    "UNDERFLOW",
    "TLOSS",
    "PLOSS",
    "INFINITY",
    "NAN",
    "HUGE_VAL",
    "HUGE_VALF",
    "HUGE_VALL",
    "FP_NAN",
    "FP_INFINITE",
    "FP_ZERO",
    "FP_SUBNORMAL",
    "FP_NORMAL",
    "MATH_ERRNO",
    "MATH_ERREXCEPT",
    "math_errhandling",
    # <climits>, <cfloat>, <ctime>, <cstdint>
    "CHAR_BIT",
    "CHAR_MIN",
    "CHAR_MAX",
    "SCHAR_MIN",
    "SCHAR_MAX",
    "UCHAR_MAX",
    "SHRT_MIN",
    "SHRT_MAX",
    "USHRT_MAX",
    "INT_MIN",
    "INT_MAX",
    "UINT_MAX",
    "LONG_MIN",
    "LONG_MAX",
    "ULONG_MAX",
    "LLONG_MIN",
    "LLONG_MAX",
    "ULLONG_MAX",
    "CLOCKS_PER_SEC",
    # glibc / POSIX extras
    "unix",
    "linux",
    "major",
    "minor",
    "makedev",
    "I",
    "complex",
    "NDEBUG",
}
CXX_KEYWORDS = {
    "alignas",
    "alignof",
    "and",
    "and_eq",
    "asm",
    "auto",
    "bitand",
    "bitor",
    "bool",
    "break",
    "case",
    "catch",
    "char",
    "char8_t",
    "char16_t",
    "char32_t",
    "class",
    "compl",
    "concept",
    "const",
    "consteval",
    "constexpr",
    "constinit",
    "const_cast",
    "continue",
    "co_await",
    "co_return",
    "co_yield",
    "decltype",
    "default",
    "delete",
    "do",
    "double",
    "dynamic_cast",
    "else",
    "enum",
    "explicit",
    "export",
    "extern",
    "float",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "mutable",
    "namespace",
    "new",
    "noexcept",
    "not",
    "not_eq",
    "nullptr",
    "operator",
    "or",
    "or_eq",
    "private",
    "protected",
    "public",
    "register",
    "reinterpret_cast",
    "requires",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_assert",
    "static_cast",
    "struct",
    "switch",
    "template",
    "this",
    "thread_local",
    "throw",
    "try",
    "typedef",
    "typeid",
    "typename",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "wchar_t",
    "while",
    "xor",
    "xor_eq",
}
# Names a generated class already uses for itself.
CLASS_MEMBERS = {"Field", "has", "descriptor", "kNumFields", "present_"}

ELEMENT_CPP = {
    "bool": "bool",
    "int8": "std::int8_t",
    "int16": "std::int16_t",
    "int32": "std::int32_t",
    "int64": "std::int64_t",
    "uint8": "std::uint8_t",
    "uint16": "std::uint16_t",
    "uint32": "std::uint32_t",
    "uint64": "std::uint64_t",
    "float32": "float",
    "float64": "double",
    "string": "std::string",
}
SCALAR_TYPE = {
    "bool": "Bool",
    "int8": "Int8",
    "int16": "Int16",
    "int32": "Int32",
    "int64": "Int64",
    "uint8": "UInt8",
    "uint16": "UInt16",
    "uint32": "UInt32",
    "uint64": "UInt64",
    "float32": "Float32",
    "float64": "Float64",
    "string": "String",
}
FLAG_CPP = {
    "derived": "kDerived",
    "position": "kPosition",
    "vector": "kVector",
    "index": "kIndex",
    "categorical": "kCategorical",
    "pdg": "kPdg",
    "pdg_minus_one_valid": "kPdgMinusOneValid",
    "pointwise": "kPointwise",
    "lite_skip": "kLiteSkip",
}
# C++ names for a few enums, besides the SPINE name which is always available.
ENUM_ALIASES = {"ParticleShape": "Shape"}

IDENT = re.compile(r"[A-Za-z][A-Za-z0-9_]*")


class GenerationError(Exception):
    pass


# --------------------------------------------------------------------------------------
# Text helpers
# --------------------------------------------------------------------------------------


def snake_case(name):
    name = re.sub(r"(\d)D$", lambda m: m.group(1) + "d", name)
    name = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", name)
    name = re.sub(r"(?<=[A-Z])(?=[A-Z][a-z])", "_", name)
    return name.lower()


def check_identifier(name, what):
    if not IDENT.fullmatch(name) or "__" in name:
        raise GenerationError(f"{what} {name!r} is not a plain C++ identifier")


def wrap_words(text, width):
    """Greedy word wrap; a word longer than the width gets a line of its own."""
    lines, line = [], ""
    for word in text.split():
        if line and len(line) + 1 + len(word) > width:
            lines.append(line)
            line = word
        else:
            line = f"{line} {word}" if line else word
    if line:
        lines.append(line)
    return lines or [""]


def comment_lines(text, indent):
    prefix = " " * indent + "/// "
    return [prefix + line for line in wrap_words(clean_comment(text), WIDTH - len(prefix))]


def clean_comment(text):
    text = text.replace("*/", "* /").rstrip("\\ ")
    if not text.isascii():
        raise GenerationError(f"non-ASCII text in a comment: {text!r}")
    return text


def cpp_string(text):
    if not text.isascii():
        raise GenerationError(f"non-ASCII string literal: {text!r}")
    out = text.replace("\\", "\\\\").replace('"', '\\"').replace("??", "?\\?")
    out = out.replace("\n", "\\n").replace("\t", "\\t")
    return f'"{out}"'


def string_chunks(text, width):
    """A string literal as one or more adjacent literals, each at most `width` wide."""
    words = text.split(" ")
    chunks, line = [], ""
    for word in words:
        piece = word if not line else " " + word
        if line and len(cpp_string(line + piece)) > width:
            chunks.append(line)
            line, piece = "", word
        line += piece
    chunks.append(line)
    # Keep a trailing space on every chunk but the last so that the concatenation is exact.
    return [cpp_string(c + (" " if i < len(chunks) - 1 else "")) for i, c in enumerate(chunks)]


def fill(tokens, first_indent, next_indent, width=WIDTH):
    """Lay out comma-terminated tokens on lines of at most `width` columns."""
    lines, line = [], " " * first_indent
    for token in tokens:
        if line.strip() and len(line) + 1 + len(token) > width:
            lines.append(line.rstrip())
            line = " " * next_indent + token
        else:
            line += (" " if line.strip() else "") + token
    lines.append(line.rstrip())
    return lines


def float32_literal(value):
    """Shortest decimal that reads back as the same float32."""
    for digits in range(1, 10):
        text = f"{value:.{digits}g}"
        if (
            struct.unpack("f", struct.pack("f", float(text)))[0]
            == struct.unpack("f", struct.pack("f", value))[0]
        ):
            break
    if not any(c in text for c in ".eE"):
        text += ".0"
    return text + "f"


# --------------------------------------------------------------------------------------
# Enums
# --------------------------------------------------------------------------------------


def enumerator_name(name, taken):
    """C++ spelling of a SPINE enum member name; macro clashes get a trailing underscore."""
    check_identifier(name, "enum member")
    cpp = name + "_" if name in RESERVED or name in CXX_KEYWORDS else name
    if cpp in taken:
        raise GenerationError(f"enum member {name!r} collides with {cpp!r} after renaming")
    return cpp


class EnumInfo:
    def __init__(self, name, data):
        check_identifier(name, "enum")
        self.name = name
        self.doc = data["doc"]
        self.members = []  # (cpp_name, spine_name, value)
        taken = set()
        for member, value in data["members"]:
            cpp = enumerator_name(member, taken)
            taken.add(cpp)
            self.members.append((cpp, member, value))
        self.alias = ENUM_ALIASES.get(name)
        self.descriptor = f"k{name}Enum"
        self.by_spine_name = {m[1]: m for m in self.members}
        self.by_value = {}
        for m in self.members:
            self.by_value.setdefault(m[2], m)
        if data.get("aliases"):
            raise GenerationError(f"enum {name} has aliases {data['aliases']}; not supported")

    def spelling(self, value):
        member = self.by_value.get(value)
        if member is not None:
            return f"{self.name}::{member[0]}"
        return f"static_cast<{self.name}>({value})"


def emit_enums_header(schema, enums, banner):
    out = [banner, "#pragma once", ""]
    out += [
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "#include <spine/descriptor.hpp>",
        "",
        "namespace spine {",
        "",
        "// Enumerators have SPINE's names, except that a name which is a macro in a standard",
        "// header gets a trailing underscore (NULL -> NULL_). Descriptors keep SPINE's spelling.",
        "",
    ]
    for e in enums.values():
        out += comment_lines(f"{e.doc} (SPINE `{e.name}`)", 0)
        out.append(f"enum class {e.name} : std::int64_t {{")
        for cpp, spine_name, value in e.members:
            note = f"  // SPINE name: {spine_name}" if cpp != spine_name else ""
            out.append(f"  {cpp} = {value},{note}")
        out.append("};")
        if e.alias:
            out.append(f"using {e.alias} = {e.name};")
        out.append(f"extern const EnumDescriptor {e.descriptor};")
        out.append("")
    out += [
        "/// The descriptor of an enum generated here, e.g. `enum_descriptor<ParticlePID>()`.",
        "template <class E>",
        "const EnumDescriptor& enum_descriptor();",
        "",
    ]
    for e in enums.values():
        one = (
            f"inline const EnumDescriptor& enum_descriptor<{e.name}>() {{ return {e.descriptor}; }}"
        )
        out.append("template <>")
        if len(one) <= WIDTH:
            out.append(one)
        else:
            out += [
                f"inline const EnumDescriptor& enum_descriptor<{e.name}>() {{",
                f"  return {e.descriptor};",
                "}",
            ]
    out.append("")
    out.append("/// SPINE's name for `value`, or an empty view when it is not a member.")
    for e in enums.values():
        out += [
            f"inline std::string_view to_string({e.name} value) {{",
            f"  return {e.descriptor}.name_of(static_cast<std::int64_t>(value));",
            "}",
        ]
    out.append("")
    out += [
        "/// The enum SPINE uses to interpret `interaction_mode` / `interaction_type` for an",
        "/// `interaction_scheme`, or nullptr when the scheme has none (UNKNOWN or out of range).",
        "/// Those two fields are plain int64 in the model: the table depends on the scheme.",
        "const EnumDescriptor* interaction_mode_enum(NuInteractionScheme scheme);",
        "const EnumDescriptor* interaction_type_enum(NuInteractionScheme scheme);",
        "",
        "}  // namespace spine",
        "",
    ]
    return "\n".join(out)


def emit_enums_source(schema, enums, banner):
    out = [banner, "#include <spine/model/enums.hpp>", "", "#include <iterator>", ""]
    out += ["namespace spine {", "", "namespace {", ""]
    for e in enums.values():
        out.append(f"constexpr EnumMember k{e.name}Members[] = {{")
        for _, spine_name, value in e.members:
            out.append(f'    {{"{spine_name}", {value}}},')
        out.append("};")
        out.append("")
    out += ["}  // namespace", ""]
    for e in enums.values():
        prefix = f"const EnumDescriptor {e.descriptor}{{"
        out += fill(
            [f'{prefix}"{e.name}",', f"k{e.name}Members,", f"std::size(k{e.name}Members)}};"],
            0,
            len(prefix),
        )
    out.append("")
    scheme_enum = enums["NuInteractionScheme"]
    for attr in ("interaction_mode", "interaction_type"):
        table = schema["scheme_tables"][attr]
        out.append(f"const EnumDescriptor* {attr}_enum(NuInteractionScheme scheme) {{")
        out.append("  switch (scheme) {")
        for scheme_name, enum_name in table.items():
            member = scheme_enum.by_spine_name[scheme_name][0]
            out.append(f"    case NuInteractionScheme::{member}:")
            out.append(f"      return &{enums[enum_name].descriptor};")
        out += ["    default:", "      break;", "  }", "  return nullptr;", "}", ""]
    out.append("}  // namespace spine")
    out.append("")
    return "\n".join(out)


# --------------------------------------------------------------------------------------
# Classes
# --------------------------------------------------------------------------------------


class FieldInfo:
    def __init__(self, cls_name, data, enums):
        self.data = data
        self.name = data["name"]
        check_identifier(self.name, f"{cls_name} field")
        if self.name in RESERVED or self.name in CXX_KEYWORDS or self.name in CLASS_MEMBERS:
            raise GenerationError(f"{cls_name}.{self.name} cannot be a C++ member name")
        self.storage = data["storage"]
        self.element = data["element"]
        self.length = data["length"]
        self.enum = enums[data["enum"]] if data["enum"] else None
        if self.enum is not None and (self.element != "int64" or self.storage != "scalar"):
            raise GenerationError(f"{cls_name}.{self.name}: enums must be scalar int64")

    @property
    def element_cpp(self):
        return self.enum.name if self.enum else ELEMENT_CPP[self.element]

    @property
    def cpp_type(self):
        if self.storage == "fixed_array":
            return f"std::array<{self.element_cpp}, {self.length}>"
        if self.storage == "variable_array":
            return f"std::vector<{self.element_cpp}>"
        return self.element_cpp

    @property
    def count(self):
        return {"scalar": 1, "string": 0, "fixed_array": self.length, "variable_array": 0}[
            self.storage
        ]

    @property
    def variable(self):
        return self.storage in ("string", "variable_array")

    def scalar_literal(self, value):
        element = self.element
        if self.enum:
            return self.enum.spelling(value)
        if element == "bool":
            return "true" if value else "false"
        if element == "string":
            return cpp_string(value)
        if element in ("float32", "float64"):
            suffix = "f" if element == "float32" else "d"
            if value == "nan":
                return f"detail::kNaN{suffix}"
            if value in ("inf", "-inf"):
                limits = f"std::numeric_limits<{ELEMENT_CPP[element]}>::infinity()"
                return limits if value == "inf" else "-" + limits
            value = float(value)
            return float32_literal(value) if element == "float32" else repr(value)
        value = int(value)
        if element.startswith("u"):
            return f"{value}u" + ("l" if element == "uint64" else "")
        if element == "int64" and not -(2**31) <= value < 2**31:
            return f"{value}LL"
        return str(value)

    def initializer(self):
        """Text after the member name (` = ...`), or empty for a value-initialised member."""
        default = self.data["default"]
        if self.storage in ("scalar", "string"):
            if self.storage == "string" and default == "":
                return ""
            return f" = {self.scalar_literal(default)}"
        if not default:
            return ""
        literals = [self.scalar_literal(v) for v in default]
        if self.storage == "fixed_array" and len(literals) > 3 and len(set(literals)) == 1:
            return f" = detail::filled<{self.element_cpp}, {self.length}>({literals[0]})"
        return f" = {{{', '.join(literals)}}}"

    def doc_tags(self):
        tags = []
        if self.data["units"]:
            tags.append(f"[units: {self.data['units']}]")
        if "derived" in self.data["flags"]:
            tags.append("[derived]")
        if "lite_skip" in self.data["flags"]:
            tags.append("[lite_skip]")
        return tags

    def flags_expr(self):
        flags = [FLAG_CPP[f] for f in self.data["flags"]]
        return " | ".join(flags) if flags else "0"

    def table_row(self, cls_name):
        d = self.data
        enum_ptr = f"&{self.enum.descriptor}" if self.enum else "nullptr"
        head = [
            f'{{"{self.name}",',
            f"ScalarType::{SCALAR_TYPE[self.element]},",
            f"{self.count},",
            f"{'true' if self.variable else 'false'},",
            f"{self.flags_expr()},",
            f"{cpp_string(d['units'] or '')},",
            f"{enum_ptr},",
            f"{cpp_string(d['reference'] or '')},",
            f"{cpp_string(d['reference_space'] or '')},",
        ]
        lines = fill(head, 4, 5)
        doc = string_chunks(d["doc"], WIDTH - 6)
        lines += [" " * 5 + chunk for chunk in doc[:-1]]
        lines.append(" " * 5 + doc[-1] + ",")
        lines.append(f"     &detail::assign<&{cls_name}::{self.name}>}},")
        return lines


class ClassInfo:
    def __init__(self, name, data, enums):
        check_identifier(name, "class")
        self.name = name
        self.data = data
        self.snake = snake_case(name)
        self.fields = [FieldInfo(name, f, enums) for f in data["fields"]]
        names = [f.name for f in self.fields]
        if len(set(names)) != len(names):
            raise GenerationError(f"{name}: duplicate field names")
        self.enums_used = sorted({f.enum.name for f in self.fields if f.enum})
        if len(self.fields) > 65535:
            raise GenerationError(f"{name}: too many fields for a 16-bit Field enum")


def emit_class_header(cls, version, banner):
    n = len(cls.fields)
    d = cls.data
    out = [banner, "#pragma once", ""]
    out += ["#include <array>", "#include <cstddef>", "#include <cstdint>", "#include <limits>"]
    out += ["#include <string>", "#include <vector>", ""]
    out += ["#include <spine/descriptor.hpp>", "#include <spine/model/common.hpp>"]
    if cls.enums_used:
        out.append("#include <spine/model/enums.hpp>")
    out += ["#include <spine/object.hpp>", ""]
    out += ["namespace spine {", ""]
    summary = d["doc"] or cls.name
    text = f"{summary} SPINE `{d['module']}.{cls.name}`: {n} fields, in the order SPINE's HDF5 "
    text += (
        "writer stores them. A field the source file lacks keeps its default and has() is false."
    )
    if any(f.data["units"] == "instance" for f in cls.fields) and any(
        f.name == "units" for f in cls.fields
    ):
        text += " Fields with units 'instance' are in the unit named by the member `units`."
    out += comment_lines(text, 0)
    out.append(f"struct {cls.name} : Object<{n}> {{")
    out.append("  enum class Field : std::uint16_t {")
    out += [f"    {f.name}," for f in cls.fields]
    out.append("  };")
    out.append("")
    for f in cls.fields:
        text = f.data["doc"]
        tags = f.doc_tags()
        if tags:
            text = (text + " " if text else "") + " ".join(tags)
        if text:
            out += comment_lines(text, 2)
        out.append(f"  {f.cpp_type} {f.name}{f.initializer()};")
    out.append("")
    out.append("  bool has(Field f) const { return has_field(static_cast<std::size_t>(f)); }")
    out.append("  static const ClassDescriptor& descriptor();")
    out.append("};")
    out += ["", "}  // namespace spine", ""]
    return "\n".join(out)


def emit_class_source(cls, version, banner):
    out = [banner, f"#include <spine/model/{cls.snake}.hpp>", ""]
    out += ["#include <cstddef>", "#include <iterator>", ""]
    out += ['#include "model/field_check.hpp"', ""]
    out += ["namespace spine {", "", "namespace {", ""]
    out.append("constexpr FieldDescriptor kFields[] = {")
    for f in cls.fields:
        out += f.table_row(cls.name)
    out.append("};")
    out.append("")
    out.append(
        f'constexpr ClassDescriptor kDescriptor{{"{cls.name}", kFields, std::size(kFields), '
        f'"{version}"}};'
    )
    out.append("")
    out.append(
        "// The table has one row per Field enumerator, in order, and every member's C++ type"
    )
    out.append("// agrees with its row (element type, count, variable flag, enum or not).")
    out.append(f"static_assert(std::size(kFields) == {cls.name}::kNumFields);")
    out += fill(
        [
            f"static_assert(static_cast<std::size_t>({cls.name}::Field::{cls.fields[-1].name})",
            f"+ 1 == {cls.name}::kNumFields);",
        ],
        0,
        14,
    )
    for f in cls.fields:
        out.append(f"SPINE_MODEL_CHECK_FIELD({cls.name}, {f.name});")
    out += ["", "}  // namespace", ""]
    out.append(f"const ClassDescriptor& {cls.name}::descriptor() {{ return kDescriptor; }}")
    out += ["", "}  // namespace spine", ""]
    return "\n".join(out)


# --------------------------------------------------------------------------------------
# Static files
# --------------------------------------------------------------------------------------

COMMON_HPP = """\
#pragma once

#include <array>
#include <cstddef>
#include <limits>

namespace spine::detail {

// Default values of floating-point members that SPINE initialises to NaN.
inline constexpr float kNaNf = std::numeric_limits<float>::quiet_NaN();
inline constexpr double kNaNd = std::numeric_limits<double>::quiet_NaN();

/// A std::array with every element set to `value`.
template <class T, std::size_t N>
constexpr std::array<T, N> filled(T value) {
  std::array<T, N> out{};
  for (T& element : out) element = value;
  return out;
}

}  // namespace spine::detail
"""

FIELD_CHECK_HPP = """\
#pragma once

// Compile-time agreement between a generated class member and its FieldDescriptor.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <spine/descriptor.hpp>
#include <spine/detail/assign.hpp>

namespace spine::detail {

template <class T>
constexpr ScalarType element_type() {
  if constexpr (std::is_same_v<T, bool>) {
    return ScalarType::Bool;
  } else if constexpr (std::is_enum_v<T>) {
    static_assert(std::is_same_v<std::underlying_type_t<T>, std::int64_t>,
                  "SPINE enums are stored as int64");
    return ScalarType::Int64;
  } else if constexpr (std::is_same_v<T, std::int8_t>) {
    return ScalarType::Int8;
  } else if constexpr (std::is_same_v<T, std::int16_t>) {
    return ScalarType::Int16;
  } else if constexpr (std::is_same_v<T, std::int32_t>) {
    return ScalarType::Int32;
  } else if constexpr (std::is_same_v<T, std::int64_t>) {
    return ScalarType::Int64;
  } else if constexpr (std::is_same_v<T, std::uint8_t>) {
    return ScalarType::UInt8;
  } else if constexpr (std::is_same_v<T, std::uint16_t>) {
    return ScalarType::UInt16;
  } else if constexpr (std::is_same_v<T, std::uint32_t>) {
    return ScalarType::UInt32;
  } else if constexpr (std::is_same_v<T, std::uint64_t>) {
    return ScalarType::UInt64;
  } else if constexpr (std::is_same_v<T, float>) {
    return ScalarType::Float32;
  } else if constexpr (std::is_same_v<T, double>) {
    return ScalarType::Float64;
  } else if constexpr (std::is_same_v<T, std::string>) {
    return ScalarType::String;
  } else {
    static_assert(dependent_false<T>, "spine-reader: unsupported member type");
  }
}

/// How a member of type M must be described: element type, count, variable flag.
template <class M>
struct MemberShape {
  static constexpr ScalarType type = element_type<M>();
  static constexpr std::uint32_t count = std::is_same_v<M, std::string> ? 0 : 1;
  static constexpr bool variable = std::is_same_v<M, std::string>;
  static constexpr bool is_enum = std::is_enum_v<M>;
};
template <class E, std::size_t N>
struct MemberShape<std::array<E, N>> {
  static constexpr ScalarType type = element_type<E>();
  static constexpr std::uint32_t count = static_cast<std::uint32_t>(N);
  static constexpr bool variable = false;
  static constexpr bool is_enum = std::is_enum_v<E>;
};
template <class E, class A>
struct MemberShape<std::vector<E, A>> {
  static constexpr ScalarType type = element_type<E>();
  static constexpr std::uint32_t count = 0;
  static constexpr bool variable = true;
  static constexpr bool is_enum = std::is_enum_v<E>;
};

template <class M>
constexpr bool field_matches(const FieldDescriptor& d, std::string_view name) {
  using S = MemberShape<M>;
  // Pointer members (enumeration, assign) are left out: comparing them is not a constant
  // expression under -fsanitize=undefined. tests/test_model.cpp checks them at run time.
  return d.name == name && d.type == S::type && d.count == S::count && d.variable == S::variable;
}

}  // namespace spine::detail

// Inside the anonymous namespace of a generated .cpp that defines `kFields`.
#define SPINE_MODEL_CHECK_FIELD(Class, name)                                              \\
  static_assert(::spine::detail::field_matches<decltype(Class::name)>(                    \\
                    kFields[static_cast<std::size_t>(Class::Field::name)], #name),        \\
                #Class "::" #name " does not match its FieldDescriptor")
"""


def emit_umbrella(classes, version, banner):
    out = [banner, "#pragma once", ""]
    out += [
        "// All generated SPINE classes. SPINE_READER_FOR_EACH_CLASS(X) expands to X(Class) for",
        "// every class of the model, in the order of the schema dump.",
        "",
        "#include <spine/model/enums.hpp>",
    ]
    out += [f"#include <spine/model/{c.snake}.hpp>" for c in classes]
    out.append("")
    out.append(f'#define SPINE_READER_MODEL_SPINE_VERSION "{version}"')
    out.append("")
    entries = [f"X({c.name})" for c in classes]
    # Continuation lines of a macro end with a backslash.
    text = "#define SPINE_READER_FOR_EACH_CLASS(X) " + " ".join(entries)
    if len(text) <= WIDTH:
        out.append(text)
    else:
        rows, line = [], "#define SPINE_READER_FOR_EACH_CLASS(X)"
        for entry in entries:
            if len(line) + 1 + len(entry) > WIDTH - 2:
                rows.append(line)
                line = "    " + entry
            else:
                line += " " + entry
        rows.append(line)
        out.append(" \\\n".join(rows))
    out.append("")
    return "\n".join(out)


# --------------------------------------------------------------------------------------
# Driver
# --------------------------------------------------------------------------------------


def generate(schema):
    version = schema["spine_version"]
    banner = f"// GENERATED from SPINE {version} by tools/gen/generate.py. Do not edit."
    enums = {name: EnumInfo(name, data) for name, data in schema["enums"].items()}
    classes = [ClassInfo(name, data, enums) for name, data in schema["classes"].items()]
    if len({c.snake for c in classes}) != len(classes):
        raise GenerationError("two classes have the same snake_case file name")
    for name in ("NuInteractionScheme",):
        if name not in enums:
            raise GenerationError(f"the schema lacks the enum {name}")

    files = {}
    files["include/spine/model/common.hpp"] = banner + "\n" + COMMON_HPP
    files["include/spine/model/enums.hpp"] = emit_enums_header(schema, enums, banner)
    files["src/model/enums.cpp"] = emit_enums_source(schema, enums, banner)
    files["src/model/field_check.hpp"] = banner + "\n" + FIELD_CHECK_HPP
    for cls in classes:
        files[f"include/spine/model/{cls.snake}.hpp"] = emit_class_header(cls, version, banner)
        files[f"src/model/{cls.snake}.cpp"] = emit_class_source(cls, version, banner)
    files["include/spine/model.hpp"] = emit_umbrella(classes, version, banner)
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--schema", help="schema JSON (default: the only schema/spine-*.json)")
    parser.add_argument("--root", default=".", help="repository root to write under")
    parser.add_argument("--check", action="store_true", help="only check that files are current")
    args = parser.parse_args()
    root = Path(args.root)

    schema_path = args.schema
    if schema_path is None:
        found = sorted(glob.glob(str(root / "schema" / "spine-*.json")))
        if len(found) != 1:
            sys.exit(f"generate: pass --schema (found {len(found)} files in schema/)")
        schema_path = found[0]
    schema = json.loads(Path(schema_path).read_text())
    try:
        files = generate(schema)
    except GenerationError as err:
        sys.exit(f"generate: {err}")

    stale = []
    for sub in ("include/spine/model", "src/model"):
        for path in sorted((root / sub).glob("*")):
            rel = path.relative_to(root).as_posix()
            if path.suffix in (".hpp", ".cpp") and rel not in files:
                stale.append(rel)
                if not args.check:
                    path.unlink()
    for rel, text in sorted(files.items()):
        path = root / rel
        current = path.read_text() if path.exists() else None
        if current != text:
            stale.append(rel)
            if not args.check:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text)
    if args.check:
        for rel in stale:
            print(f"stale: {rel}")
        return 1 if stale else 0
    print(
        f"SPINE {schema['spine_version']}: {len(files)} files, {len(stale)} written "
        f"({len(schema['classes'])} classes, {len(schema['enums'])} enums)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
