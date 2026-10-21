// GENERATED from SPINE 1.4.0 by tools/gen/generate.py. Do not edit.
#pragma once

// All generated SPINE classes. SPINE_READER_FOR_EACH_CLASS(X) expands to X(Class) for
// every class of the model, in the order of the schema dump.

#include <spine/model/enums.hpp>
#include <spine/model/reco_particle.hpp>
#include <spine/model/truth_particle.hpp>
#include <spine/model/reco_interaction.hpp>
#include <spine/model/truth_interaction.hpp>
#include <spine/model/run_info.hpp>
#include <spine/model/trigger.hpp>
#include <spine/model/image_meta3d.hpp>
#include <spine/model/flash.hpp>
#include <spine/model/crt_hit.hpp>
#include <spine/model/flash_hypothesis.hpp>

#define SPINE_READER_MODEL_SPINE_VERSION "1.4.0"

#define SPINE_READER_FOR_EACH_CLASS(X) X(RecoParticle) X(TruthParticle) X(RecoInteraction) \
    X(TruthInteraction) X(RunInfo) X(Trigger) X(ImageMeta3D) X(Flash) X(CRTHit) X(FlashHypothesis)
