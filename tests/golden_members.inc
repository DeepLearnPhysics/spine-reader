// GENERATED from schema/spine-1.4.0.json by tools/fixtures/make_member_list.py. Do not edit.
// The members of each model class, in SPINE's order, for tests/test_golden.cpp.

#define GOLDEN_MEMBERS_RecoParticle(X) \
    X(is_truth) X(units) X(id) X(is_contained) X(is_time_contained) X(is_cathode_crosser) \
    X(is_matched) X(cathode_offset) X(index) X(orig_index) X(match_ids) X(match_overlaps) \
    X(interaction_id) X(chi2_pid) X(is_primary) X(is_crt_matched) X(is_valid) X(length) \
    X(calo_ke) X(csda_ke) X(mcs_ke) X(shape) X(pid) X(fragment_ids) X(start_point) \
    X(end_point) X(chi2_per_pid) X(csda_ke_per_pid) X(mcs_ke_per_pid) X(crt_ids) X(crt_times) \
    X(crt_scores) X(start_dedx) X(end_dedx) X(vertex_distance) X(start_straightness) \
    X(directional_spread) X(axial_spread) X(start_dir) X(end_dir) X(pid_scores) \
    X(primary_scores) X(ppn_ids) X(pdg_code) X(mass) X(ke) X(momentum) X(p) X(num_fragments) \
    X(size) X(depositions_sum) X(depositions_q_sum) X(best_match_id) X(best_match_overlap) \
    X(module_ids)

#define GOLDEN_MEMBERS_TruthParticle(X) \
    X(orig_id) X(is_truth) X(index_adapt) X(index_g4) X(units) X(id) X(is_contained) \
    X(is_time_contained) X(is_cathode_crosser) X(is_matched) X(cathode_offset) X(index) \
    X(orig_index) X(match_ids) X(match_overlaps) X(interaction_id) X(chi2_pid) X(is_primary) \
    X(is_crt_matched) X(is_valid) X(length) X(calo_ke) X(csda_ke) X(mcs_ke) X(shape) X(pid) \
    X(fragment_ids) X(start_point) X(end_point) X(chi2_per_pid) X(csda_ke_per_pid) \
    X(mcs_ke_per_pid) X(crt_ids) X(crt_times) X(crt_scores) X(parent_id) X(group_id) X(nu_id) \
    X(children_id) X(mct_index) X(mcst_index) X(group_primary) X(interaction_primary) \
    X(track_id) X(parent_track_id) X(ancestor_track_id) X(pdg_code) X(parent_pdg_code) \
    X(ancestor_pdg_code) X(num_voxels) X(t) X(end_t) X(first_step_t) X(last_step_t) \
    X(parent_t) X(ancestor_t) X(energy_init) X(energy_deposit) X(distance_travel) \
    X(creation_process) X(parent_creation_process) X(ancestor_creation_process) X(position) \
    X(end_position) X(parent_position) X(ancestor_position) X(first_step) X(last_step) \
    X(momentum) X(end_momentum) X(orig_interaction_id) X(orig_parent_id) X(orig_group_id) \
    X(reco_length) X(orig_children_id) X(children_counts) X(reco_start_dir) X(reco_end_dir) \
    X(start_dir) X(end_dir) X(ke) X(reco_ke) X(reco_momentum) X(p) X(end_p) X(mass) \
    X(num_fragments) X(size) X(depositions_sum) X(depositions_q_sum) X(best_match_id) \
    X(best_match_overlap) X(module_ids) X(size_adapt) X(size_g4) X(depositions_adapt_sum) \
    X(depositions_adapt_q_sum) X(depositions_g4_sum)

#define GOLDEN_MEMBERS_RecoInteraction(X) \
    X(is_truth) X(units) X(id) X(is_contained) X(is_time_contained) X(is_cathode_crosser) \
    X(is_matched) X(cathode_offset) X(index) X(orig_index) X(match_ids) X(match_overlaps) \
    X(is_fiducial) X(is_flash_matched) X(flash_total_pe) X(flash_hypo_pe) X(particle_ids) \
    X(vertex) X(flash_ids) X(flash_volume_ids) X(flash_times) X(flash_scores) \
    X(flash_hypothesis_ids) X(dir) X(primary_particle_ids) X(num_particles) \
    X(num_primary_particles) X(particle_counts) X(primary_particle_counts) X(is_crt_matched) \
    X(crt_ids) X(crt_times) X(crt_scores) X(topology) X(size) X(depositions_sum) \
    X(depositions_q_sum) X(best_match_id) X(best_match_overlap) X(module_ids)

#define GOLDEN_MEMBERS_TruthInteraction(X) \
    X(orig_id) X(is_truth) X(index_adapt) X(index_g4) X(units) X(id) X(is_contained) \
    X(is_time_contained) X(is_cathode_crosser) X(is_matched) X(cathode_offset) X(index) \
    X(orig_index) X(match_ids) X(match_overlaps) X(is_fiducial) X(is_flash_matched) \
    X(flash_total_pe) X(flash_hypo_pe) X(particle_ids) X(vertex) X(flash_ids) \
    X(flash_volume_ids) X(flash_times) X(flash_scores) X(flash_hypothesis_ids) \
    X(interaction_id) X(current_type) X(interaction_scheme) X(interaction_mode) \
    X(interaction_type) X(mct_index) X(track_id) X(lepton_track_id) X(pdg_code) \
    X(lepton_pdg_code) X(target) X(nucleon) X(quark) X(energy_init) X(hadronic_invariant_mass) \
    X(momentum_transfer) X(momentum_transfer_mag) X(energy_transfer) X(lepton_p) \
    X(distance_travel) X(t) X(theta) X(bjorken_x) X(inelasticity) X(creation_process) \
    X(position) X(momentum) X(nu_id) X(reco_vertex) X(reco_dir) X(time) X(dir) \
    X(primary_particle_ids) X(num_particles) X(num_primary_particles) X(particle_counts) \
    X(primary_particle_counts) X(is_crt_matched) X(crt_ids) X(crt_times) X(crt_scores) \
    X(topology) X(size) X(depositions_sum) X(depositions_q_sum) X(best_match_id) \
    X(best_match_overlap) X(module_ids) X(size_adapt) X(size_g4) X(depositions_adapt_sum) \
    X(depositions_adapt_q_sum) X(depositions_g4_sum)

#define GOLDEN_MEMBERS_RunInfo(X) \
    X(run) X(subrun) X(event)

#define GOLDEN_MEMBERS_Trigger(X) \
    X(id) X(type) X(time_s) X(time_ns) X(beam_time_s) X(beam_time_ns)

#define GOLDEN_MEMBERS_ImageMeta3D(X) \
    X(lower) X(upper) X(size) X(count)

#define GOLDEN_MEMBERS_Flash(X) \
    X(units) X(id) X(volume_id) X(frame) X(on_beam_time) X(in_beam_frame) X(time) \
    X(time_width) X(time_abs) X(total_pe) X(fast_to_total) X(pe_per_ch) X(center) X(width)

#define GOLDEN_MEMBERS_CRTHit(X) \
    X(units) X(id) X(plane) X(ts0_s) X(ts0_ns) X(ts0_s_corr) X(ts0_ns_corr) X(ts1_ns) \
    X(total_pe) X(tagger) X(feb_id) X(center) X(width) X(time)

#define GOLDEN_MEMBERS_FlashHypothesis(X) \
    X(id) X(interaction_id) X(volume_id) X(score) X(is_truth) X(flash_ids) X(pe_per_ch) \
    X(total_pe)
