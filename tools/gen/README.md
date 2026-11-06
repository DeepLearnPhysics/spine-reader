# tools/gen: the SPINE object model generator

The C++ classes in `include/spine/model/` and their field tables in `src/model/` are generated
from SPINE's own `FieldMetadata` at a pinned release. Nothing under those two directories is
edited by hand.

```
SPINE clone at a tag --dump_schema.py--> schema/spine-<version>.json --generate.py--> C++
```

## Regenerate for a new SPINE release

```bash
source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh
git clone -q --branch vX.Y.Z <SPINE repository> /some/dir/spine-X.Y.Z

PYTHONPATH=/some/dir/spine-X.Y.Z/src python3 tools/gen/dump_schema.py \
    --spine-sha $(git -C /some/dir/spine-X.Y.Z rev-parse HEAD)              # -> schema/spine-X.Y.Z.json
python3 tools/gen/generate.py --schema schema/spine-X.Y.Z.json           # -> include/spine/model*, src/model
```

Run python without `-I` (the LCG view is found through `PYTHONPATH`). `dump_schema.py` needs numpy
and h5py, and imports `spine.data` and `HDF5Writer`, which do not need torch. `generate.py` reads
only the JSON. Delete the old `schema/spine-*.json` (`generate.py` without `--schema` wants exactly
one): the model describes one release.

`generate.py --check` writes nothing and exits 1 if a committed generated file is stale. Output is
deterministic, so a diff after regeneration is a real change.

`dump_schema.py` exits 1 when SPINE cannot be imported or read, after one line on stderr:
`dump_schema: <exception class>: <message>` (for example `dump_schema: AttributeError:
'FieldMetadata' object has no attribute 'pdg'`). It writes nothing then. With `--strict`, a type
mismatch also exits 1.

## What to check afterwards

1. **The type check printed by `dump_schema.py`.** Each field's declared type (annotation, or
   `FieldMetadata.dtype` and `length`) is compared with the dtype `HDF5Writer.get_object_dtype`
   gives for a default-constructed object. It must say `0 mismatch(es)`; a mismatch is listed
   under `type_check` in the JSON. The same section also lists any field whose dataclass default
   differs from the value on an instance. `--strict` makes mismatches an error.
2. **`git diff schema/`.** Fields added, removed or retyped, enum members, new `FieldMetadata`
   flags. A change of `writer_format` (for example a new default layout) or a new entry under
   `warnings` (an attribute the dumper does not map, a class it does not list) has no effect on the
   generated C++, and needs a look all the same. A class SPINE started storing is added to `DEFAULT_CLASSES` in `dump_schema.py`; the
   header, the source and the `SPINE_READER_FOR_EACH_CLASS` entry then appear by themselves. A
   field or class SPINE removed disappears and `generate.py` deletes the stale files.
3. **`python3 tools/gen/check_names.py`.** No identifier of the model may be a macro of a common C,
   C++ or POSIX header (`NULL`, `DOMAIN`, `OVERFLOW`, `major`, ...). `generate.py` renames a clashing
   enumerator by a trailing underscore (`NULL` -> `NULL_`; the descriptor keeps `"NULL"`) from its
   `RESERVED` list; if the checker finds a new clash, add the name there. `--include` adds headers
   (HDF5, ROOT) to the scan.
4. **Build and test.** `cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build`.
   The `static_assert`s in `src/model/*.cpp` fail the build if a member's C++ type disagrees with
   its descriptor row. No test holds field counts, defaults or enum member counts: they follow
   the schema. Write the new tag's fixtures first (`tools/fixtures/make_fixtures.py --tag <tag>`),
   since the golden test checks the model's fields against their columns. If
   `spine_reader_contracts` fails, SPINE changed a unit, a default or an enum value that a
   CAFMaker relies on: talk to the CAFMaker maintainers before editing its row in
   `tests/test_contracts.cpp`.
5. **gcc 12, C++17** in the SL7 container (`-fsyntax-only` of every `src/model/*.cpp` and header
   with `-Wall -Wextra -Wpedantic -Werror` is enough).
6. **Docs and units.** `doc_source` in the JSON says which class docstring a field's description
   came from; SPINE copies some docs between classes (for example `TruthInteraction.id` says
   "Index of the neutrino"). Missing docs are empty strings; the generated member then has no
   description, only its tags (for example `/// [units: MeV]`).

## What is in the JSON

Top level:

| key | meaning |
| --- | --- |
| `schema_format`, `generator`, `notes` | the format of this file (1), the tool, and notes on reading it |
| `spine_version`, `spine_sha` | `spine.__version__`, and the commit given with `--spine-sha` (`null` without it) |
| `classes`, `enums`, `scheme_tables` | the model: what `generate.py` reads |
| `type_check` | the declared types against `HDF5Writer.get_object_dtype`, and dataclass defaults against instance values |
| `writer_format` | `format_name`, `legacy_format_version`, `current_format_version` and `supported_format_versions` of `HDF5Writer`, and `default_format_version`, the default of its `format_version` parameter; each is `null` when SPINE has no such attribute or parameter |
| `warnings` | `{kind, name, detail}` for what the dumper sees but does not map: `unmapped_attribute` (a `FieldMetadata` attribute outside `MAPPED_ATTRIBUTES`; at 1.4.0, `cat`) and `unlisted_class` (a class of `spine.data` with `attr_metadata` in neither `DEFAULT_CLASSES` nor `EXCLUDED_CLASSES`) |

`generate.py` ignores `spine_sha`, `type_check`, `writer_format` and `warnings`: they describe the
dump, for comparing two dumps, and do not change the generated C++.

`classes.<Name>.fields[]`, in the order of `as_dict(lite=False)` (the order of the HDF5 compound
members), minus fields with `skip=True`:

| key | meaning |
| --- | --- |
| `name`, `defined_in` | attribute name and the class of the MRO that declares it |
| `storage` | `scalar`, `fixed_array`, `variable_array` or `string` |
| `element`, `length` | `bool`, `int32`, `int64`, `uint8`, `float32`, `float64`, `string`; length of a fixed array |
| `flags` | any of `derived` (a `stored_property`), `position`, `vector`, `index`, `categorical`, `pdg`, `pdg_minus_one_valid`, `pointwise`, `lite_skip` |
| `units` | `MeV`, `ns`, ...; `instance` means "the unit named by the object's own `units` member" |
| `enum`, `enum_members` | enum class and its members (not set for `interaction_mode` and `interaction_type`) |
| `reference`, `reference_space` | collection an id points into and the relation |
| `default`, `default_source` | value of the SPINE default; `nan`, `inf`, `-inf` are strings. Derived fields take the value of the property on a default object (`default_instance`) |
| `doc`, `doc_source` | one line from the class docstrings |
| `writer` | what `get_object_dtype` produced, for the cross-check |

`interaction_mode` and `interaction_type` are plain `int64`: SPINE reads them through a table that
depends on `interaction_scheme`. The tables are recorded under `scheme_tables` (found by calling
`Neutrino.resolve_enum`, not copied), and the generated `interaction_mode_enum(scheme)` and
`interaction_type_enum(scheme)` return the matching `EnumDescriptor`.

## Type mapping

Declared types, not what one file happens to contain.

| SPINE | C++ | descriptor (`type`, `count`, `variable`) |
| --- | --- | --- |
| `int` | `std::int64_t` | `Int64`, 1, false |
| `float`, `np.float64` | `double` | `Float64`, 1, false |
| `np.float32` | `float` | `Float32` |
| `np.int32`, `np.int64`, `np.ubyte` | `std::int32_t`, `std::int64_t`, `std::uint8_t` | `Int32`, `Int64`, `UInt8` |
| `bool` | `bool` (never an enum) | `Bool`, 1, false |
| `str` | `std::string` | `String`, 0, true |
| array with `length=N` | `std::array<T, N>` | element type, N, false |
| array without `length` | `std::vector<T>` | element type, 0, true |
| `FieldMetadata.enum` | the generated `enum class X : std::int64_t` | `Int64`, 1, false, `enumeration` set |

Defaults are SPINE's: `-1`, NaN (`detail::kNaNf`, `detail::kNaNd`, spelled with
`std::numeric_limits`), arrays filled with those, empty vectors, the default string, the enum
member with the default value. `spine::Shape` is an alias of `spine::ParticleShape`.
