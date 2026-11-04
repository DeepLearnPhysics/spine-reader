# spine/model

## Overview

**Generated code. Do not edit the headers here by hand.** Every `.hpp` in this directory is
written by `tools/gen/generate.py` from the SPINE schema dump `schema/spine-1.4.0.json`. Each
file starts with the banner `// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not
edit.` To change the model, update the schema dump and run
`python3 tools/gen/generate.py` from the repository root. `--check` writes nothing and exits 1
if the files on disk differ from what it would generate. The generator also writes the
matching field tables and `descriptor()` definitions in `src/model/*.cpp` and
`src/model/field_check.hpp`, and writes the umbrella header `include/spine/model.hpp`.

The generator deletes only stale `.hpp`/`.cpp` files in this directory and in `src/model`, so
this `EGM.md` is safe from regeneration.

The headers hold the C++ mirror of SPINE's output object model: one plain struct per SPINE
class, plus SPINE's enumerations.

## Interface

- Class structs (namespace `spine`), each `struct X : Object<N>` with N public data members:
  - `reco_particle.hpp`: `RecoParticle` (55 fields)
  - `truth_particle.hpp`: `TruthParticle` (98 fields)
  - `reco_interaction.hpp`: `RecoInteraction` (40 fields)
  - `truth_interaction.hpp`: `TruthInteraction` (79 fields)
  - `run_info.hpp`: `RunInfo` (3 fields: `run`, `subrun`, `event`)
  - `trigger.hpp`: `Trigger` (6 fields)
  - `image_meta3d.hpp`: `ImageMeta3D` (4 fields)
  - `flash.hpp`: `Flash` (14 fields)
  - `crt_hit.hpp`: `CRTHit` (14 fields)
  - `flash_hypothesis.hpp`: `FlashHypothesis` (8 fields)
- Every class struct has:
  - `enum class Field : std::uint16_t`, one enumerator per member, in the order SPINE's HDF5
    writer stores them.
  - `bool has(Field f) const`: whether the source file provided that field.
  - `static const ClassDescriptor& descriptor()`, defined in `src/model/<snake_class>.cpp`.
- `enums.hpp`: `enum class ... : std::int64_t` for `ParticleShape` (alias `Shape`),
  `ParticlePID`, `NuCurrentType`, `NuInteractionScheme`, `LArSoftNuInteractionType`,
  `GenieNuInteractionType`, `GiBUUNuInteractionType`, `NuWroNuInteractionType`, and
  `NeutNuInteractionType`. It also declares:
  - `extern const EnumDescriptor k<Name>Enum`, one per enum, defined in `src/model/enums.cpp`
  - `template <class E> const EnumDescriptor& enum_descriptor()`, specialised for each enum
  - `std::string_view to_string(E value)`, which returns SPINE's name or an empty view
  - `const EnumDescriptor* interaction_mode_enum(NuInteractionScheme)` and
    `interaction_type_enum(NuInteractionScheme)`
- `common.hpp` (namespace `spine::detail`): `kNaNf`, `kNaNd`, and
  `template <class T, std::size_t N> constexpr std::array<T, N> filled(T value)`.

## Invariants

- The order of the `Field` enumerators equals the order of `ClassDescriptor::fields`, so
  `ClassDescriptor::index_of(field)` equals the `Field` value (see `../descriptor.hpp`).
- Members default to SPINE's own defaults: `-1` for ids and indices, `detail::kNaNf` or
  `detail::kNaNd` for floats SPINE sets to NaN, `"cm"` for `units`, and `UNKNOWN` for enum
  members. `UNKNOWN` is -1 for every enum a member uses. The interaction-type enums, used only
  through `interaction_*_enum`, have their own `UNKNOWN` (for example -100 for GENIE). A field
  missing from the file keeps this default and `has()` returns false.
- Fields documented `[units: instance]` are in the unit named by the object's `units` member.
- Enumerator values are the integers SPINE stores in the file. A name that is a macro in a
  standard header gets a trailing underscore (`NULL` -> `NULL_`). Descriptors keep SPINE's
  spelling.
- `TruthInteraction::interaction_mode` and `interaction_type` are plain `std::int64_t`. Use
  `interaction_mode_enum(interaction_scheme)` / `interaction_type_enum(...)` to look up their
  meaning. These return nullptr for `UNKNOWN` or an out-of-range scheme.
- Member types are limited to what `../detail/assign.hpp` accepts: arithmetic, enum,
  `std::string`, `std::array<E, N>`, and `std::vector<E>`.

## Side Effects

- None. These are passive data structs and constant declarations, with no I/O and no global
  state.

## Verification

- Regeneration check: `python3 tools/gen/generate.py --check`
- Run (after `source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh`):
  `cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build --output-on-failure`
