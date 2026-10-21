// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not edit.
#pragma once

#include <cstdint>
#include <string_view>

#include <spine/descriptor.hpp>

namespace spine {

// Enumerators have SPINE's names, except that a name which is a macro in a standard
// header gets a trailing underscore (NULL -> NULL_). Descriptors keep SPINE's spelling.

/// Enumerates semantic voxel/particle categories. (SPINE `ParticleShape`)
enum class ParticleShape : std::int64_t {
  UNKNOWN = -1,
  SHOWER = 0,
  TRACK = 1,
  MICHEL = 2,
  DELTA = 3,
  LOWE = 4,
  GHOST = 5,
  LARCV_UNKNOWN = 6,
};
using Shape = ParticleShape;
extern const EnumDescriptor kParticleShapeEnum;

/// Enumerates canonical SPINE particle species IDs. (SPINE `ParticlePID`)
enum class ParticlePID : std::int64_t {
  UNKNOWN = -1,
  PHOTON = 0,
  ELECTRON = 1,
  MUON = 2,
  PION = 3,
  PROTON = 4,
  KAON = 5,
};
extern const EnumDescriptor kParticlePIDEnum;

/// Enumerates neutrino current types. (SPINE `NuCurrentType`)
enum class NuCurrentType : std::int64_t {
  UNKNOWN = -1,
  CC = 0,
  NC = 1,
};
extern const EnumDescriptor kNuCurrentTypeEnum;

/// Enumerates source conventions for neutrino interaction code fields. (SPINE
/// `NuInteractionScheme`)
enum class NuInteractionScheme : std::int64_t {
  UNKNOWN = -1,
  LARSOFT = 0,
  GENIE = 1,
  GIBUU = 2,
  NUWRO = 3,
  NEUT = 4,
};
extern const EnumDescriptor kNuInteractionSchemeEnum;

/// Enumerates LArSoft neutrino interaction categories. (SPINE `LArSoftNuInteractionType`)
enum class LArSoftNuInteractionType : std::int64_t {
  UNKNOWN = -1,
  QE = 0,
  RES = 1,
  DIS = 2,
  COH = 3,
  COHELASTIC = 4,
  ELECTRONSCATTERING = 5,
  IMDANNIHILATION = 6,
  INVERSEBETADECAY = 7,
  GLASHOWRESONANCE = 8,
  AMNUGAMMA = 9,
  MEC = 10,
  DIFFRACTIVE = 11,
  EM = 12,
  WEAKMIX = 13,
  NUANCEOFFSET = 1000,
  CCQE = 1001,
  NCQE = 1002,
  RESCCNUPROTONPIPLUS = 1003,
  RESCCNUNEUTRONPI0 = 1004,
  RESCCNUNEUTRONPIPLUS = 1005,
  RESNCNUPROTONPI0 = 1006,
  RESNCNUPROTONPIPLUS = 1007,
  RESNCNUNEUTRONPI0 = 1008,
  RESNCNUNEUTRONPIMINUS = 1009,
  RESCCNUBARNEUTRONPIMINUS = 1010,
  RESCCNUBARPROTONPI0 = 1011,
  RESCCNUBARPROTONPIMINUS = 1012,
  RESNCNUBARPROTONPI0 = 1013,
  RESNCNUBARPROTONPIPLUS = 1014,
  RESNCNUBARNEUTRONPI0 = 1015,
  RESNCNUBARNEUTRONPIMINUS = 1016,
  RESCCNUDELTAPLUSPIPLUS = 1017,
  RESCCNUDELTA2PLUSPIMINUS = 1021,
  RESCCNUBARDELTA0PIMINUS = 1028,
  RESCCNUBARDELTAMINUSPIPLUS = 1032,
  RESCCNUPROTONRHOPLUS = 1039,
  RESCCNUNEUTRONRHOPLUS = 1041,
  RESCCNUBARNEUTRONRHOMINUS = 1046,
  RESCCNUBARNEUTRONRHO0 = 1048,
  RESCCNUSIGMAPLUSKAONPLUS = 1053,
  RESCCNUSIGMAPLUSKAON0 = 1055,
  RESCCNUBARSIGMAMINUSKAON0 = 1060,
  RESCCNUBARSIGMA0KAON0 = 1062,
  RESCCNUPROTONETA = 1067,
  RESCCNUBARNEUTRONETA = 1070,
  RESCCNUKAONPLUSLAMBDA0 = 1073,
  RESCCNUBARKAON0LAMBDA0 = 1076,
  RESCCNUPROTONPIPLUSPIMINUS = 1079,
  RESCCNUPROTONPI0PI0 = 1080,
  RESCCNUBARNEUTRONPIPLUSPIMINUS = 1085,
  RESCCNUBARNEUTRONPI0PI0 = 1086,
  RESCCNUBARPROTONPI0PI0 = 1090,
  CCDIS = 1091,
  NCDIS = 1092,
  UNUSED1 = 1093,
  UNUSED2 = 1094,
  CCQEHYPERON = 1095,
  NCCOH = 1096,
  CCCOH = 1097,
  NUELECTRONELASTIC = 1098,
  INVERSEMUDECAY = 1099,
  MEC2P2H = 1100,
};
extern const EnumDescriptor kLArSoftNuInteractionTypeEnum;

/// Enumerates GENIE scattering categories. (SPINE `GenieNuInteractionType`)
enum class GenieNuInteractionType : std::int64_t {
  UNKNOWN = -100,
  NULL_ = 0,  // SPINE name: NULL
  QE = 1,
  SINGLEKAON = 2,
  DIS = 3,
  RES = 4,
  COH = 5,
  DIFFRACTIVE = 6,
  NUELECTRONELASTIC = 7,
  INVERSEMUDECAY = 8,
  AMNUGAMMA = 9,
  MEC = 10,
  COHELASTIC = 11,
  INVERSEBETADECAY = 12,
  GLASHOWRESONANCE = 13,
  IMDANNIHILATION = 14,
  PHOTONCOHERENT = 15,
  PHOTONRESONANCE = 16,
  SINGLEPION = 17,
  DARKMATTERELASTIC = 101,
  DARKMATTERDIS = 102,
  DARKMATTERELECTRON = 103,
  NORM = 104,
};
extern const EnumDescriptor kGenieNuInteractionTypeEnum;

/// Enumerates GiBUU NuHepMC process identifiers. (SPINE `GiBUUNuInteractionType`)
enum class GiBUUNuInteractionType : std::int64_t {
  UNKNOWN = -1,
  UNRECOGNIZED = 0,
  CCQE = 200,
  NCQE = 250,
  CC_2P2H_QE = 300,
  CC_2P2H_DELTA = 301,
  NC_2P2H_QE = 350,
  NC_2P2H_DELTA = 351,
  CC_RES_DELTA = 400,
  CC_RES_OTHER = 401,
  NC_RES_DELTA = 450,
  NC_RES_OTHER = 451,
  CC_BKGD_NEUTRON = 500,
  CC_BKGD_PROTON = 501,
  CC_BKGD_2PI = 502,
  NC_BKGD_NEUTRON = 550,
  NC_BKGD_PROTON = 551,
  NC_BKGD_2PI = 552,
  CC_DIS = 600,
  NC_DIS = 650,
};
extern const EnumDescriptor kGiBUUNuInteractionTypeEnum;

/// Enumerates NuWro RooTracker interaction channel identifiers. (SPINE `NuWroNuInteractionType`)
enum class NuWroNuInteractionType : std::int64_t {
  UNKNOWN = -1,
  CCQE = 1,
  CCMEC = 2,
  CCRES = 11,
  CCCOH = 16,
  CCDIS = 26,
  NCRES = 31,
  NCCOH = 36,
  NCDIS = 46,
  NCQE = 51,
  CCHYPERON = 100,
};
extern const EnumDescriptor kNuWroNuInteractionTypeEnum;

/// Enumerates NEUT NuHepMC process identifiers. (SPINE `NeutNuInteractionType`)
enum class NeutNuInteractionType : std::int64_t {
  UNKNOWN = -1,
  CC_COH_NU = 100,
  CC_DIF_NU = 110,
  CC_COH_NUBAR = 125,
  CC_DIF_NUBAR = 135,
  NC_COH_NU = 150,
  NC_DIF_NU = 160,
  NC_COH_NUBAR = 175,
  NC_DIF_NUBAR = 185,
  CC_QE_NU = 200,
  CC_QE_PROTON_NUBAR = 225,
  NC_ELASTIC_PROTON_NU = 250,
  NC_ELASTIC_NEUTRON_NU = 251,
  NC_ELASTIC_PROTON_NUBAR = 275,
  NC_ELASTIC_NEUTRON_NUBAR = 276,
  CC_2P2H_NU = 300,
  CC_2P2H_NUBAR = 325,
  CC_RES_PROTON_PI_PLUS_NU = 400,
  CC_RES_PROTON_PI_ZERO_NU = 401,
  CC_RES_NEUTRON_PI_PLUS_NU = 402,
  CC_ETA_NU = 410,
  CC_KAON_NU = 411,
  CC_SINGLE_GAMMA_NU = 412,
  CC_RES_NEUTRON_PI_MINUS_NUBAR = 425,
  CC_RES_PROTON_PI_ZERO_NUBAR = 426,
  CC_RES_PROTON_PI_MINUS_NUBAR = 427,
  CC_ETA_NUBAR = 435,
  CC_KAON_NUBAR = 436,
  CC_SINGLE_GAMMA_NUBAR = 437,
  NC_RES_NEUTRON_PI_ZERO_NU = 450,
  NC_RES_PROTON_PI_ZERO_NU = 451,
  NC_RES_NEUTRON_PI_PLUS_NU = 452,
  NC_ETA_NEUTRON_NU = 460,
  NC_ETA_PROTON_NU = 461,
  NC_KAON_NEUTRON_NU = 462,
  NC_KAON_PROTON_NU = 463,
  NC_SINGLE_GAMMA_NEUTRON_NU = 464,
  NC_SINGLE_GAMMA_PROTON_NU = 465,
  NC_RES_NEUTRON_PI_ZERO_NUBAR = 475,
  NC_RES_PROTON_PI_ZERO_NUBAR = 476,
  NC_RES_PROTON_PI_MINUS_NUBAR = 478,
  NC_RES_NEUTRON_PI_PLUS_NUBAR = 479,
  NC_ETA_NEUTRON_NUBAR = 485,
  NC_ETA_PROTON_NUBAR = 486,
  NC_KAON_NEUTRON_NUBAR = 487,
  NC_KAON_PROTON_NUBAR = 488,
  NC_SINGLE_GAMMA_NEUTRON_NUBAR = 489,
  NC_SINGLE_GAMMA_PROTON_NUBAR = 490,
  CC_MULTI_PI_NU = 500,
  CC_MULTI_PI_NUBAR = 525,
  NC_MULTI_PI_NU = 550,
  NC_MULTI_PI_NUBAR = 575,
  CC_DIS_NU = 600,
  NC_DIS_NU = 601,
  CC_DIS_NUBAR = 625,
  NC_DIS_NUBAR = 675,
};
extern const EnumDescriptor kNeutNuInteractionTypeEnum;

/// The descriptor of an enum generated here, e.g. `enum_descriptor<ParticlePID>()`.
template <class E>
const EnumDescriptor& enum_descriptor();

template <>
inline const EnumDescriptor& enum_descriptor<ParticleShape>() { return kParticleShapeEnum; }
template <>
inline const EnumDescriptor& enum_descriptor<ParticlePID>() { return kParticlePIDEnum; }
template <>
inline const EnumDescriptor& enum_descriptor<NuCurrentType>() { return kNuCurrentTypeEnum; }
template <>
inline const EnumDescriptor& enum_descriptor<NuInteractionScheme>() {
  return kNuInteractionSchemeEnum;
}
template <>
inline const EnumDescriptor& enum_descriptor<LArSoftNuInteractionType>() {
  return kLArSoftNuInteractionTypeEnum;
}
template <>
inline const EnumDescriptor& enum_descriptor<GenieNuInteractionType>() {
  return kGenieNuInteractionTypeEnum;
}
template <>
inline const EnumDescriptor& enum_descriptor<GiBUUNuInteractionType>() {
  return kGiBUUNuInteractionTypeEnum;
}
template <>
inline const EnumDescriptor& enum_descriptor<NuWroNuInteractionType>() {
  return kNuWroNuInteractionTypeEnum;
}
template <>
inline const EnumDescriptor& enum_descriptor<NeutNuInteractionType>() {
  return kNeutNuInteractionTypeEnum;
}

/// SPINE's name for `value`, or an empty view when it is not a member.
inline std::string_view to_string(ParticleShape value) {
  return kParticleShapeEnum.name_of(static_cast<std::int64_t>(value));
}
inline std::string_view to_string(ParticlePID value) {
  return kParticlePIDEnum.name_of(static_cast<std::int64_t>(value));
}
inline std::string_view to_string(NuCurrentType value) {
  return kNuCurrentTypeEnum.name_of(static_cast<std::int64_t>(value));
}
inline std::string_view to_string(NuInteractionScheme value) {
  return kNuInteractionSchemeEnum.name_of(static_cast<std::int64_t>(value));
}
inline std::string_view to_string(LArSoftNuInteractionType value) {
  return kLArSoftNuInteractionTypeEnum.name_of(static_cast<std::int64_t>(value));
}
inline std::string_view to_string(GenieNuInteractionType value) {
  return kGenieNuInteractionTypeEnum.name_of(static_cast<std::int64_t>(value));
}
inline std::string_view to_string(GiBUUNuInteractionType value) {
  return kGiBUUNuInteractionTypeEnum.name_of(static_cast<std::int64_t>(value));
}
inline std::string_view to_string(NuWroNuInteractionType value) {
  return kNuWroNuInteractionTypeEnum.name_of(static_cast<std::int64_t>(value));
}
inline std::string_view to_string(NeutNuInteractionType value) {
  return kNeutNuInteractionTypeEnum.name_of(static_cast<std::int64_t>(value));
}

/// The enum SPINE uses to interpret `interaction_mode` / `interaction_type` for an
/// `interaction_scheme`, or nullptr when the scheme has none (UNKNOWN or out of range).
/// Those two fields are plain int64 in the model: the table depends on the scheme.
const EnumDescriptor* interaction_mode_enum(NuInteractionScheme scheme);
const EnumDescriptor* interaction_type_enum(NuInteractionScheme scheme);

}  // namespace spine
