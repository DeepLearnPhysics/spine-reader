// spine-dump: print what spine-reader sees in SPINE HDF5 files.
//
//   spine-dump [--events N] file.h5 [more.h5 ...]
//
// Prints each file's schema report, then per-event object counts and the first reco
// particle of each event.

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <spine/error.hpp>
#include <spine/reader.hpp>

int main(int argc, char** argv) {
  std::vector<std::string> paths;
  std::uint64_t max_events = 5;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--events" && i + 1 < argc) {
      max_events = std::strtoull(argv[++i], nullptr, 10);
    } else {
      paths.push_back(arg);
    }
  }
  if (paths.empty()) {
    std::cerr << "usage: spine-dump [--events N] file.h5 [more.h5 ...]\n";
    return 2;
  }

  try {
    spine::Reader reader(paths);
    for (std::size_t f = 0; f < reader.num_files(); ++f) {
      std::cout << reader.schema_report(f).summary() << "\n";
    }
    std::cout << reader.num_events() << " events\n";
    for (std::uint64_t i = 0; i < reader.num_events() && i < max_events; ++i) {
      const spine::Event event = reader.event(i);
      std::cout << "event " << i;
      if (const spine::RunInfo* run = event.run_info()) {
        std::cout << " (run " << run->run << ", subrun " << run->subrun << ", event " << run->event
                  << ")";
      }
      std::cout << ": " << event.reco_interactions().size() << " reco interactions, "
                << event.reco_particles().size() << " reco particles, "
                << event.truth_interactions().size() << " truth interactions, "
                << event.truth_particles().size() << " truth particles\n";
      if (!event.reco_particles().empty()) {
        const spine::RecoParticle& p = event.reco_particles().front();
        std::cout << "  first reco particle: id " << p.id << ", pdg " << p.pdg_code << ", ke "
                  << p.ke << " MeV, start (" << p.start_point[0] << ", " << p.start_point[1]
                  << ", " << p.start_point[2] << ") " << p.units;
        if (const spine::RecoInteraction* interaction = event.interaction_of(p)) {
          std::cout << ", " << event.particles_of(*interaction).size()
                    << " particles in its interaction";
        }
        std::cout << "\n";
      }
    }
  } catch (const spine::Error& e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
  return 0;
}
