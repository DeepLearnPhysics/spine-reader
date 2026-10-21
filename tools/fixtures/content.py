"""Synthetic event content for the spine-reader golden fixtures (no SPINE import).

The content is a plain-Python description (dicts, lists, numbers).  The SPINE
worker (``spine_worker.py``) turns it into SPINE objects with whatever SPINE
version is on ``PYTHONPATH``, silently dropping the fields that version does not
know, and hands them to that version's own ``HDF5Writer``.

Conventions that follow SPINE's own:

* ``id`` of a particle / interaction is its position in the event's list.
  They are assigned by the worker; this module only fixes the grouping
  (which particles belong to which interaction).
* ``interaction_id`` of a particle is the position of its interaction.
* Reco ``match_ids`` point to truth ids (positions) of the same event and vice
  versa; ``match_overlaps`` are IoU values, best match first.
* Variable-length index arrays are short (a few voxels) to keep files small.

Every enum label of ``shape``, ``pid``, ``chi2_pid``, ``current_type`` and
``interaction_scheme`` occurs in the data, both bool values occur, there are
NaN / +inf / -inf values, empty variable arrays, a particle-less event, an
interaction without particles and non-ASCII strings (2- and 3-byte UTF-8).
"""

import math
import random

NAN = float("nan")
INF = float("inf")

# Enum values (spine.constants.enums)
UNKNOWN = -1
SHOWER, TRACK, MICHEL, DELTA, LOWE, GHOST, LARCV_UNKNOWN = 0, 1, 2, 3, 4, 5, 6
PHOTON, ELECTRON, MUON, PION, PROTON, KAON = 0, 1, 2, 3, 4, 5
CC, NC = 0, 1
LARSOFT, GENIE, GIBUU, NUWRO, NEUT = 0, 1, 2, 3, 4

PDG = {UNKNOWN: -1, PHOTON: 22, ELECTRON: 11, MUON: 13, PION: 211, PROTON: 2212, KAON: 321}
MASS = {PHOTON: 0.0, ELECTRON: 0.511, MUON: 105.66, PION: 139.57, PROTON: 938.27, KAON: 493.68}

NUM_EVENTS = 5
# The two writer calls: events [0, 2) then [2, 5).  The second batch starts with
# the particle-less event, which exercises appending with absolute offsets.
BATCHES = ((0, 2), (2, 5))


def _r(rng, lo, hi, nd=3):
    """Random value on a dyadic grid (1/16, or 1/64 for nd > 2).

    Such values are exact in float32 *and* float64 and print short, which keeps
    the expected JSON small.  The "ugly" values (0.91, derived sums, ...) that
    do not survive a float32 round trip come from the explicit literals in
    ``build_events`` and from SPINE's own derived properties.
    """
    step = 16 if nd <= 2 else 64
    return round(rng.uniform(lo, hi) * step) / step


def _unit(rng):
    v = [rng.gauss(0, 1) for _ in range(3)]
    n = math.sqrt(sum(x * x for x in v))
    return [round(x / n * 128) / 128 for x in v]


def _vec(rng, lo, hi):
    return [_r(rng, lo, hi, 2) for _ in range(3)]


def _voxels(rng, n):
    """Sorted unique voxel indices (variable-length int32 array)."""
    return sorted(rng.sample(range(100, 9000), n))


def _scores(rng, n, best):
    s = [rng.random() + (1.5 if i == best else 0.05) for i in range(n)]
    t = sum(s)
    return [round(x / t, 4) for x in s]


def reco_particle(rng, shape, pid, primary, nvox, match, chi2_pid=UNKNOWN, units="cm", **kw):
    """One reco particle; ``match`` is a list of (truth_id, overlap) pairs."""
    track = shape == TRACK
    index = _voxels(rng, nvox) if nvox else []
    d = _unit(rng)
    start = _vec(rng, -150, 150)
    length = _r(rng, 4, 140) if track else NAN
    end = [round(s + length * c, 3) for s, c in zip(start, d)] if track else [NAN] * 3
    calo = _r(rng, 20, 900, 2) if nvox else NAN
    p = dict(
        units=units,
        shape=shape,
        pid=pid,
        chi2_pid=chi2_pid,
        is_primary=primary,
        is_valid=True,
        is_contained=rng.random() > 0.3,
        is_time_contained=rng.random() > 0.15,
        is_cathode_crosser=rng.random() > 0.8,
        is_matched=bool(match),
        cathode_offset=_r(rng, -2, 2) if rng.random() > 0.5 else 0.0,
        index=index,
        orig_index=[i + 7 for i in index],
        depositions=[_r(rng, 0.1, 3, 3) for _ in index],
        depositions_q=[_r(rng, 0.1, 5, 3) for _ in index],
        match_ids=[m for m, _ in match],
        match_overlaps=[o for _, o in match],
        length=length,
        calo_ke=calo,
        csda_ke=round(0.93 * calo, 2) if track else NAN,
        mcs_ke=_r(rng, 50, 800, 2) if (track and rng.random() > 0.5) else NAN,
        start_point=start,
        end_point=end,
        start_dir=d,
        end_dir=[-c for c in d] if track else [NAN] * 3,
        chi2_per_pid=[_r(rng, 0.5, 40, 2) for _ in range(6)] if track else [NAN] * 6,
        pid_scores=_scores(rng, 6, pid if pid >= 0 else 0),
        primary_scores=[round(1 - 0.8 * primary, 3), round(0.8 * primary + 0.1, 3)],
        fragment_ids=[],  # filled below (one fragment per particle)
        ppn_ids=[],
        start_dedx=_r(rng, 1.5, 4.5) if track else NAN,
        vertex_distance=_r(rng, 0, 6),
    )
    p.update(kw)
    return p


def truth_particle(rng, shape, pid, primary, nvox, match, units="cm", **kw):
    index = _voxels(rng, nvox) if nvox else []
    d = _unit(rng)
    mass = MASS.get(pid, NAN)
    ke = _r(rng, 20, 900, 2)
    pmag = math.sqrt(ke * ke + 2 * ke * mass) if not math.isnan(mass) else ke
    vtx = _vec(rng, -150, 150)
    p = dict(
        units=units,
        shape=shape,
        pid=pid,
        chi2_pid=UNKNOWN,
        pdg_code=PDG[pid],
        is_primary=primary,
        is_valid=True,
        is_contained=rng.random() > 0.3,
        is_time_contained=True,
        is_cathode_crosser=rng.random() > 0.85,
        is_matched=bool(match),
        index=index,
        orig_index=[i + 7 for i in index],
        index_adapt=index[: max(len(index) - 1, 0)],
        index_g4=index + ([9001, 9002] if index else []),
        depositions=[_r(rng, 0.1, 3, 3) for _ in index],
        depositions_q=[_r(rng, 0.1, 5, 3) for _ in index],
        depositions_adapt=[_r(rng, 0.1, 3, 3) for _ in index[: max(len(index) - 1, 0)]],
        depositions_g4=[_r(rng, 0.1, 3, 3) for _ in index] + ([0.5, 0.25] if index else []),
        match_ids=[m for m, _ in match],
        match_overlaps=[o for _, o in match],
        length=_r(rng, 4, 140) if shape == TRACK else NAN,
        calo_ke=_r(rng, 20, 900, 2) if nvox else NAN,
        energy_init=round(ke + (0.0 if math.isnan(mass) else mass), 3),
        energy_deposit=_r(rng, 10, ke, 2),
        distance_travel=_r(rng, 2, 120),
        num_voxels=nvox,
        start_point=vtx,
        end_point=[round(v + 30 * c, 3) for v, c in zip(vtx, d)],
        position=vtx,
        end_position=[round(v + 30 * c, 3) for v, c in zip(vtx, d)],
        momentum=[round(pmag * c, 3) for c in d],
        end_momentum=[round(0.1 * pmag * c, 3) for c in d],
        t=_r(rng, 0, 5),
        end_t=_r(rng, 5, 9),
        first_step_t=_r(rng, 0, 5),
        last_step_t=_r(rng, 5, 9),
        creation_process="primary" if primary else "muIoni",
        parent_creation_process="primary",
        ancestor_creation_process="primary",
    )
    p.update(kw)
    return p


def reco_interaction(rng, vertex, match, units="cm", flash=None, **kw):
    it = dict(
        units=units,
        vertex=vertex,
        dir=_unit(rng),
        is_fiducial=rng.random() > 0.25,
        is_contained=rng.random() > 0.3,
        is_time_contained=True,
        match_ids=[m for m, _ in match],
        match_overlaps=[o for _, o in match],
        is_matched=bool(match),
    )
    if flash is not None:
        it.update(
            is_flash_matched=True,
            flash_ids=[flash],
            flash_volume_ids=[flash % 2],
            flash_times=[_r(rng, -0.4, 1.6)],
            flash_scores=[_r(rng, 0.5, 5)],
            flash_hypothesis_ids=[flash + 10],
            flash_total_pe=_r(rng, 2e3, 3e4, 1),
            flash_hypo_pe=_r(rng, 2e3, 3e4, 1),
        )
    it.update(kw)
    return it


def truth_interaction(rng, vertex, match, scheme, current, units="cm", **kw):
    e = _r(rng, 0.3, 3.0)
    it = dict(
        units=units,
        vertex=vertex,
        position=vertex,
        reco_vertex=[round(v + 0.5, 3) for v in vertex],
        reco_dir=_unit(rng),
        is_fiducial=rng.random() > 0.25,
        is_contained=rng.random() > 0.3,
        is_time_contained=True,
        match_ids=[m for m, _ in match],
        match_overlaps=[o for _, o in match],
        is_matched=bool(match),
        current_type=current,
        interaction_scheme=scheme,
        interaction_mode=rng.choice([0, 1, 2, 3, 10]),
        interaction_type=rng.choice([1000, 1001, 1005, 1091]),
        pdg_code=14,
        lepton_pdg_code=13 if current == CC else 14,
        target=1000180400,
        nucleon=2112,
        quark=2,
        energy_init=e,
        hadronic_invariant_mass=_r(rng, 1.0, 2.5),
        momentum_transfer=_r(rng, 0.1, 1.5),
        momentum_transfer_mag=_r(rng, 0.1, 1.5),
        energy_transfer=_r(rng, 0.1, 1.5),
        lepton_p=_r(rng, 0.1, 2.5),
        distance_travel=0.0,
        t=_r(rng, 0, 5),
        theta=_r(rng, 0, 1.5),
        bjorken_x=_r(rng, 0.05, 0.9),
        inelasticity=_r(rng, 0.05, 0.9),
        creation_process="primary",
        momentum=[round(e * c, 3) for c in _unit(rng)],
    )
    it.update(kw)
    return it


def build_events():
    """The 5 synthetic events.  Deterministic (pure-Python Mersenne Twister).

    ====  ==========================  ==========================  =======  =======
    ev    reco (interactions: parts)  truth (interactions: parts) flashes  crthits
    ====  ==========================  ==========================  =======  =======
    0     2: 3 + 2                    2: 3 + 2                    2        1
    1     1: 2                        2: 2 + 1                    1        0
    2     none                        none                        0        2
    3     2: 1 + 2                    1: 3                        3        0
    4     2: 1 + 0 (!)                1: 1                        0        1
    ====  ==========================  ==========================  =======  =======
    """
    rng = random.Random(20261021)
    events = []

    def common(e):
        return {
            "run_info": dict(run=9001, subrun=3 + (e >= 3), event=1000 + 17 * e),
            # upper == lower + size * count is enforced by SPINE (all values exact in float32)
            "meta": dict(lower=[-360.0, -180.0, 0.0], upper=[360.0, 180.0, 1400.0],
                         size=[0.5, 0.5, 0.5], count=[1440, 720, 2800])
            if e < 3
            else dict(lower=[-358.5, -181.0, 0.0], upper=[358.5, 181.0, 1400.0],
                      size=[0.25, 0.25, 0.25], count=[2868, 1448, 5600]),
            "trigger": dict(id=e, type=2 if e % 2 else 1, time_s=1_700_000_000 + 60 * e,
                            time_ns=123_456_789 - 1000 * e, beam_time_s=1_700_000_000 + 60 * e,
                            beam_time_ns=2_000 + e),
        }

    def tp(*a, **k):  # truth particle shorthand with the bookkeeping every one needs
        k.setdefault("children_counts", [0] * 6)
        k.setdefault("children_id", [])
        k.setdefault("orig_children_id", [])
        return truth_particle(rng, *a, **k)

    # ---- event 0: rich ----------------------------------------------------
    ev = common(0)
    ev["reco"] = [
        reco_interaction(rng, _vec(rng, -150, 150), [(0, 0.88), (1, 0.05)], flash=0),
        reco_interaction(rng, _vec(rng, -150, 150), [(1, 0.7)], flash=1),
    ]
    ev["reco"][0]["particles"] = [
        reco_particle(rng, TRACK, MUON, True, 8, [(1, 0.91), (4, 0.06)], chi2_pid=MUON),
        reco_particle(rng, TRACK, PROTON, True, 5, [(0, 0.77)], chi2_pid=PROTON),
        reco_particle(rng, SHOWER, PHOTON, False, 6, [(2, 0.52), (3, 0.2)], chi2_pid=PHOTON),
    ]
    ev["reco"][1]["particles"] = [
        reco_particle(rng, SHOWER, ELECTRON, True, 7, [(3, 0.83)], chi2_pid=ELECTRON),
        reco_particle(rng, MICHEL, ELECTRON, False, 3, [], chi2_pid=UNKNOWN),
    ]
    ev["truth"] = [
        truth_interaction(rng, _vec(rng, -150, 150), [(0, 0.86)], GENIE, CC, orig_id=100, nu_id=0, track_id=1,
                          lepton_track_id=2),
        truth_interaction(rng, _vec(rng, -150, 150), [(1, 0.7), (0, 0.1)], LARSOFT, NC, orig_id=101, nu_id=1,
                          track_id=6, lepton_track_id=-1),
    ]
    ev["truth"][0]["particles"] = [
        tp(TRACK, MUON, True, 8, [(0, 0.9)], orig_id=11, track_id=2, ancestor_track_id=1, parent_track_id=1,
           parent_id=-1, group_id=0, parent_pdg_code=14, ancestor_pdg_code=14, children_id=[2],
           orig_children_id=[13], children_counts=[1, 0, 0, 0, 0, 0]),
        tp(TRACK, PROTON, True, 5, [(1, 0.8)], orig_id=12, track_id=3, ancestor_track_id=1, parent_track_id=1,
           parent_id=-1, group_id=1, parent_pdg_code=14, ancestor_pdg_code=14),
        tp(SHOWER, PHOTON, False, 6, [(2, 0.6)], orig_id=13, track_id=5, ancestor_track_id=1, parent_track_id=2,
           parent_id=0, group_id=2, parent_pdg_code=13, ancestor_pdg_code=14,
           creation_process="µIoni", ancestor_creation_process="π→µν"),
    ]
    ev["truth"][1]["particles"] = [
        tp(SHOWER, ELECTRON, True, 7, [(3, 0.85)], orig_id=21, track_id=7, ancestor_track_id=7, parent_track_id=0,
           parent_id=-1, group_id=3, parent_pdg_code=0, ancestor_pdg_code=14, children_id=[4],
           orig_children_id=[22], children_counts=[0, 1, 0, 0, 0, 0]),
        tp(MICHEL, ELECTRON, False, 3, [], orig_id=22, track_id=8, ancestor_track_id=7, parent_track_id=7,
           parent_id=3, group_id=4, parent_pdg_code=11, ancestor_pdg_code=14),
    ]
    ev["flashes"] = [
        dict(volume_id=0, frame=0, on_beam_time=1, in_beam_frame=True, time=0.12, time_width=0.34,
             time_abs=1500.12, total_pe=9873.5, fast_to_total=0.31, pe_per_ch=[12.5, 0.0, 301.25, NAN, 7.0, 2.5],
             center=[-210.5, 12.0, 340.25], width=[5.0, 30.0, 45.5]),
        dict(volume_id=1, frame=0, on_beam_time=0, in_beam_frame=False, time=3.5, time_width=0.1,
             time_abs=1503.5, total_pe=812.0, fast_to_total=NAN, pe_per_ch=[],
             center=[200.0, -20.0, 100.0], width=[2.0, 2.0, 2.0]),
    ]
    ev["crthits"] = [
        dict(plane=3, ts0_s=1_700_000_000, ts0_ns=1500.5, ts0_s_corr=1_700_000_000.0, ts0_ns_corr=1498.25,
             ts1_ns=1200.0, total_pe=55.5, tagger="top", feb_id=[17, 18, 255],
             center=[10.0, 400.0, 20.0], width=[40.0, 2.0, 40.0]),
    ]
    events.append(ev)

    # ---- event 1: shape LARCV_UNKNOWN / UNKNOWN, pid UNKNOWN, "px" units ----
    ev = common(1)
    ev["reco"] = [reco_interaction(rng, _vec(rng, -150, 150), [(0, 0.45)], units="px")]
    ev["reco"][0]["particles"] = [
        reco_particle(rng, LARCV_UNKNOWN, UNKNOWN, False, 3, [(1, 0.2)], units="px"),
        reco_particle(rng, UNKNOWN, PION, True, 4, [(0, 0.5)], chi2_pid=PION, units="px"),
    ]
    ev["truth"] = [
        truth_interaction(rng, _vec(rng, -150, 150), [(0, 0.4)], GIBUU, UNKNOWN, orig_id=200, nu_id=0, track_id=1),
        truth_interaction(rng, _vec(rng, -150, 150), [], NUWRO, CC, orig_id=201, nu_id=1, track_id=4),
    ]
    ev["truth"][0]["particles"] = [
        tp(LARCV_UNKNOWN, UNKNOWN, True, 3, [(0, 0.25)], orig_id=40, track_id=2, ancestor_track_id=1,
           parent_track_id=1, parent_id=-1, group_id=0),
        tp(UNKNOWN, MUON, False, 4, [(1, 0.55)], orig_id=41, track_id=3, ancestor_track_id=1, parent_track_id=1,
           parent_id=0, group_id=1),
    ]
    ev["truth"][1]["particles"] = [
        tp(TRACK, PROTON, True, 6, [], orig_id=50, track_id=5, ancestor_track_id=4, parent_track_id=4,
           parent_id=-1, group_id=2),
    ]
    ev["flashes"] = [
        dict(volume_id=0, frame=1, on_beam_time=0, in_beam_frame=False, time=-1.5, time_width=0.2,
             time_abs=998.5, total_pe=44.25, fast_to_total=0.5, pe_per_ch=[1.0, 2.0, 3.0],
             center=[0.0, 0.0, 0.0], width=[1.0, 1.0, 1.0]),
    ]
    ev["crthits"] = []
    events.append(ev)

    # ---- event 2: no particles, no interactions, no flashes; two CRT hits ----
    ev = common(2)
    ev["reco"], ev["truth"], ev["flashes"] = [], [], []
    ev["crthits"] = [
        dict(plane=1, ts0_s=1_700_000_120, ts0_ns=10.0, ts0_s_corr=1_700_000_120.0, ts0_ns_corr=9.5,
             ts1_ns=-5.0, total_pe=1.0, tagger="µ-bottom", feb_id=[], center=[1.0, 2.0, 3.0],
             width=[0.5, 0.5, 0.5]),
        dict(plane=2, ts0_s=1_700_000_120, ts0_ns=20.0, ts0_s_corr=1_700_000_120.0, ts0_ns_corr=19.5,
             ts1_ns=INF, total_pe=-INF, tagger="side", feb_id=[3, 4], center=[-1.0, -2.0, -3.0],
             width=[1.0, 1.0, 1.0]),
    ]
    events.append(ev)

    # ---- event 3: 2 reco interactions, inf values, DELTA / LOWE / GHOST, KAON ----
    ev = common(3)
    ev["reco"] = [
        reco_interaction(rng, _vec(rng, -150, 150), [(0, 0.9)], flash=0),
        reco_interaction(rng, _vec(rng, -150, 150), [(0, 0.02)]),
    ]
    ev["reco"][0]["particles"] = [
        reco_particle(rng, DELTA, MUON, True, 9, [(0, 0.95)], chi2_pid=KAON, length=INF, start_dedx=-INF),
    ]
    ev["reco"][1]["particles"] = [
        reco_particle(rng, LOWE, PROTON, True, 5, [(1, 0.6), (2, 0.1)], chi2_pid=UNKNOWN),
        reco_particle(rng, GHOST, KAON, False, 0, [], chi2_pid=UNKNOWN, is_valid=False),
    ]
    ev["truth"] = [
        truth_interaction(rng, _vec(rng, -150, 150), [(0, 0.88), (1, 0.03)], NEUT, NC, orig_id=300, nu_id=0,
                          track_id=1, lepton_track_id=2),
    ]
    ev["truth"][0]["particles"] = [
        tp(DELTA, PION, True, 9, [(0, 0.93)], orig_id=60, track_id=2, ancestor_track_id=1, parent_track_id=1,
           parent_id=-1, group_id=0),
        tp(LOWE, KAON, True, 5, [(1, 0.6), (2, 0.1)], orig_id=61, track_id=3, ancestor_track_id=1,
           parent_track_id=1, parent_id=-1, group_id=1),
        tp(GHOST, MUON, False, 0, [], orig_id=62, track_id=4, ancestor_track_id=1, parent_track_id=3,
           parent_id=1, group_id=2),
    ]
    ev["flashes"] = [
        dict(volume_id=0, frame=0, on_beam_time=1, in_beam_frame=True, time=0.5, time_width=0.25,
             time_abs=2000.5, total_pe=5000.0, fast_to_total=0.25, pe_per_ch=[50.0] * 4,
             center=[-100.0, 0.0, 200.0], width=[3.0, 3.0, 3.0]),
        dict(volume_id=1, frame=0, on_beam_time=0, in_beam_frame=False, time=-2.0, time_width=0.5,
             time_abs=1997.0, total_pe=100.0, fast_to_total=0.9, pe_per_ch=[1.5],
             center=[100.0, 0.0, 200.0], width=[3.0, 3.0, 3.0]),
        dict(volume_id=1, frame=1, on_beam_time=0, in_beam_frame=False, time=8.0, time_width=0.5,
             time_abs=2008.0, total_pe=0.0, fast_to_total=0.0, pe_per_ch=[0.0, 0.0],
             center=[100.0, 0.0, 200.0], width=[0.0, 0.0, 0.0]),
    ]
    ev["crthits"] = []
    events.append(ev)

    # ---- event 4: an interaction with a particle and one without any ----
    ev = common(4)
    ev["reco"] = [
        reco_interaction(rng, _vec(rng, -150, 150), [(0, 0.5)]),
        reco_interaction(rng, [NAN, NAN, NAN], []),
    ]
    ev["reco"][0]["particles"] = [reco_particle(rng, SHOWER, ELECTRON, True, 5, [(0, 0.5)], chi2_pid=ELECTRON)]
    ev["reco"][1]["particles"] = []
    ev["truth"] = [
        truth_interaction(rng, _vec(rng, -150, 150), [(0, 0.5)], UNKNOWN, CC, orig_id=400, nu_id=0, track_id=1),
    ]
    ev["truth"][0]["particles"] = [
        tp(SHOWER, ELECTRON, True, 5, [(0, 0.5)], orig_id=70, track_id=2, ancestor_track_id=1, parent_track_id=1,
           parent_id=-1, group_id=0),
    ]
    ev["flashes"] = []
    ev["crthits"] = [
        dict(plane=0, ts0_s=1_700_000_240, ts0_ns=1.0, ts0_s_corr=1_700_000_240.0, ts0_ns_corr=1.0,
             ts1_ns=0.0, total_pe=7.5, tagger="top", feb_id=[1], center=[0.0, 0.0, 0.0], width=[0.0, 0.0, 0.0]),
    ]
    events.append(ev)

    assert len(events) == NUM_EVENTS
    return events


def exotic_events():
    """Non-object products: strings, scalars, 2-D arrays, lists, multi-lists.

    Exercises the ``string``, ``list`` and ``multi_list`` product kinds and the
    non-object ``array`` products, including empty events.
    """
    rng = random.Random(7)
    rows_per_event = (4, 0, 3)  # rows of the 2-D array / multi-list per event
    lists_per_event = (2, 0, 3)  # elements of the same-width list per event
    out = []
    for e in range(3):
        k = rows_per_event[e]
        out.append(
            {
                "label": ["event-0-π", "", "µ-event-2"][e],
                "arr_f32_n3": [[round(rng.gauss(0, 1), 3) for _ in range(3)] for _ in range(k)],
                "lists_same_width": [
                    [10 * i + j for j in range(1 + (i + e) % 3)] for i in range(lists_per_event[e])
                ],
                # fixed number (3) of arrays of different widths per event
                "lists_mixed_width": [
                    [[round(rng.gauss(0, 1), 3) for _ in range(3)] for _ in range(k)],
                    [round(rng.gauss(0, 1), 3) for _ in range(k)],
                    [[round(rng.gauss(0, 1), 3) for _ in range(2)] for _ in range(k)],
                ],
            }
        )
    return out
