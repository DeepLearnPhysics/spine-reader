#!/usr/bin/env python3
"""Writes the small SPINE files of the Reader and Event tests (tests/test_reader.cpp).

    source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc13-opt/setup.sh
    PYTHONPATH=<spine v1.4.0 checkout>/src:$PYTHONPATH python3 make_reader_cases.py [--outdir DIR]

The files are written by SPINE's own HDF5Writer (no h5py writing, except one attribute, see
reader_c). What is special about them is the content, which the golden fixtures lack:

* SPINE numbers objects by list position, but nothing in a file says so, and the reader must
  not rely on it. Here ids are NOT list positions: some are permuted among positions, some
  exceed the list length, one particle belongs to no interaction, interactions list particles
  that are not in the event (one id equals the list length), one match points at a truth object
  that does not exist, one match has no overlap score;
* the files are small and cut differently: A (V1, 3 events), B (V2, 2 events), C (V1, 1 event),
  so a Reader over [A, B, C] has 6 events and boundaries at global events 3 and 5;
* (run, subrun, event) triples are all different, except that B's first event repeats A's last;
* C records its ImageMeta3D under the old name "Meta".

The expectations live in tests/test_reader.cpp, in plain C++, and nowhere else.
"""
import argparse
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools", "fixtures"))
import spine_worker as w  # noqa: E402  (imports SPINE through spine_api(), lazily)


def small_chunks(rows=8):
    """SPINE leaves the chunk shape to h5py, which picks 32 to 1024 rows for the empty datasets the
    writer creates, so even tiny files take 100 kB or more. Ask for `rows` rows per chunk instead.
    Content, dtypes, attributes and offsets are untouched."""
    import h5py

    original = h5py.Group.create_dataset

    def create_dataset(self, name, shape=None, dtype=None, data=None, **kwargs):
        if (kwargs.get("chunks") is None and kwargs.get("maxshape") is not None
                and shape is not None):
            shape = tuple(shape)
            kwargs["chunks"] = (rows,) + tuple(max(d, 1) for d in shape[1:])
        return original(self, name, shape, dtype, data, **kwargs)

    h5py.Group.create_dataset = create_dataset


def particles(sd, side, specs):
    cls = sd.RecoParticle if side == "reco" else sd.TruthParticle
    return sd.ObjectList([w.make(cls, s) for s in specs], cls())


def interactions(sd, side, specs):
    cls = sd.RecoInteraction if side == "reco" else sd.TruthInteraction
    return sd.ObjectList([w.make(cls, s) for s in specs], cls())


# Each event: run_info, then (id, interaction_id, match_ids, match_overlaps) for particles and
# (id, particle_ids, match_ids, match_overlaps) for interactions, reco and truth.
def event_a0():
    return dict(
        run_info=dict(run=7, subrun=2, event=101),
        reco_particles=[
            dict(id=11, interaction_id=1, match_ids=[1, 2, 7], match_overlaps=[0.75, 0.5, 0.25]),
            dict(id=4, interaction_id=2, match_ids=[], match_overlaps=[]),
            dict(id=8, interaction_id=0, match_ids=[0], match_overlaps=[0.5]),
            # two matches, one score
            dict(id=3, interaction_id=1, match_ids=[2, 0], match_overlaps=[0.5]),
            dict(id=20, interaction_id=-1, match_ids=[], match_overlaps=[]),
        ],
        reco_interactions=[
            dict(id=1, particle_ids=[11, 3], match_ids=[0, 1], match_overlaps=[0.875, 0.5]),
            dict(id=2, particle_ids=[4, 99], match_ids=[], match_overlaps=[]),
            dict(id=0, particle_ids=[8], match_ids=[1], match_overlaps=[0.25]),
        ],
        truth_particles=[
            dict(id=2, interaction_id=1, match_ids=[4, 11], match_overlaps=[0.625, 0.125]),
            dict(id=0, interaction_id=0, match_ids=[8], match_overlaps=[0.5]),
            dict(id=1, interaction_id=1, match_ids=[11], match_overlaps=[0.75]),
        ],
        truth_interactions=[
            dict(id=1, particle_ids=[2, 1], match_ids=[1], match_overlaps=[0.5]),
            dict(id=0, particle_ids=[0], match_ids=[2, 1], match_overlaps=[0.75, 0.25]),
        ],
    )


def event_a1():  # ids equal positions, one id equal to the list length; matches point at nothing
    return dict(
        run_info=dict(run=7, subrun=2, event=102),
        reco_particles=[
            dict(id=0, interaction_id=0, match_ids=[0], match_overlaps=[0.5]),
            dict(id=1, interaction_id=0, match_ids=[], match_overlaps=[]),
        ],
        reco_interactions=[dict(id=0, particle_ids=[0, 1, 2], match_ids=[0], match_overlaps=[0.5])],
        truth_particles=[],
        truth_interactions=[],
    )


def event_a2():  # empty
    return dict(run_info=dict(run=8, subrun=1, event=5), reco_particles=[], reco_interactions=[],
                truth_particles=[], truth_interactions=[])


def event_b0():  # same triple as A's last event
    return dict(run_info=dict(run=8, subrun=1, event=5),
                reco_particles=[dict(id=0, interaction_id=0)],
                reco_interactions=[dict(id=0, particle_ids=[0])],
                truth_particles=[], truth_interactions=[])


def event_b1():  # ids reversed among two positions
    return dict(
        run_info=dict(run=9, subrun=0, event=1),
        reco_particles=[dict(id=1, interaction_id=1), dict(id=0, interaction_id=0)],
        reco_interactions=[dict(id=1, particle_ids=[1]), dict(id=0, particle_ids=[0])],
        truth_particles=[dict(id=1, match_ids=[0], match_overlaps=[0.25]),
                         dict(id=0, match_ids=[1], match_overlaps=[0.5])],
        truth_interactions=[],
    )


def event_c0():
    return dict(
        run_info=dict(run=10, subrun=3, event=77),
        meta=dict(lower=[-10.0, -20.0, -30.0], upper=[10.0, 20.0, 30.0], size=[1.0, 2.0, 3.0],
                  count=[20, 20, 20]),
        reco_particles=[dict(id=2, interaction_id=0), dict(id=0, interaction_id=0)],
        reco_interactions=[dict(id=0, particle_ids=[2, 0])],
        truth_particles=[], truth_interactions=[],
    )


def build(sd, ev):
    out = {
        "run_info": w.make(sd.RunInfo, ev["run_info"]),
        "reco_particles": particles(sd, "reco", ev["reco_particles"]),
        "reco_interactions": interactions(sd, "reco", ev["reco_interactions"]),
        "truth_particles": particles(sd, "truth", ev["truth_particles"]),
        "truth_interactions": interactions(sd, "truth", ev["truth_interactions"]),
    }
    if "meta" in ev:
        out["meta"] = w.make(sd.ImageMeta3D, ev["meta"])
    return out


def write(path, events, format_version, keys):
    sd, _version, HDF5Writer, _ = w.spine_api()
    if os.path.exists(path):
        os.remove(path)
    writer = HDF5Writer(file_name=path, overwrite=True, keys=keys, format_version=format_version)
    batch = {"index": np.arange(len(events))}
    for key in keys:
        batch[key] = [e[key] for e in map(lambda ev: build(sd, ev), events)]
    writer(batch, cfg={"note": "spine-reader reader test data"})
    writer.finalize()
    writer.close()
    del writer


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--outdir", default=HERE)
    args = parser.parse_args()
    w.apply_libver("v110")
    small_chunks()
    keys = ["run_info", "reco_particles", "reco_interactions", "truth_particles",
            "truth_interactions"]
    a = os.path.join(args.outdir, "reader_a_v1.h5")
    b = os.path.join(args.outdir, "reader_b_v2.h5")
    c = os.path.join(args.outdir, "reader_c_v1_meta.h5")
    write(a, [event_a0(), event_a1(), event_a2()], 1, keys)
    write(b, [event_b0(), event_b1()], 2, keys)
    write(c, [event_c0()], 1, keys + ["meta"])

    # SPINE v0.x recorded ImageMeta3D under its alias Meta; v1.4.0 does not, so say so by hand.
    import h5py
    with h5py.File(c, "r+") as f:
        assert f["meta"].attrs["class_name"] in ("ImageMeta3D", b"ImageMeta3D")
        f["meta"].attrs["class_name"] = "Meta"
    for path in (a, b, c):
        print(f"{os.path.basename(path)}: {os.path.getsize(path)} bytes")


if __name__ == "__main__":
    main()
