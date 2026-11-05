"""SPINE tags and checkouts for the fixture tooling.

It imports neither h5py nor numpy, so a caller can resolve a tag before it has checked that
those packages are installed.

* ``resolve_tag(repo, tag)``: the commit a tag points to, from ``git ls-remote``. The commit of
  an annotated tag is its peeled ``^{}`` entry; a lightweight tag lists the commit itself.
* ``ensure_clone(tag, sha, repo, workdir)``: a checkout of that commit in
  ``<workdir>/spine_<sha>``, cloned once. The folder is named after the commit, so a tag that
  was moved to another commit never reuses the code of the old one.
"""

import os
import re
import shutil
import subprocess

SHA_RE = re.compile(r"^[0-9a-f]{40}$")


class CheckoutError(Exception):
    """A tag could not be resolved or checked out."""


class LsRemoteError(CheckoutError):
    """``git ls-remote`` could not read the repository (network, path, permissions)."""


class TagNotFound(CheckoutError):
    """The repository has no such tag."""


def resolve_tag(repo, tag):
    """Return the 40-character commit SHA that ``tag`` points to in ``repo`` (a URL or a path).

    Raises ``LsRemoteError`` when ``git ls-remote`` fails, and ``TagNotFound`` when it runs but
    does not list the tag.
    """
    ref = f"refs/tags/{tag}"
    cmd = ["git", "ls-remote", "--tags", repo, ref, ref + "^{}"]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise LsRemoteError(f"git ls-remote cannot read {repo}: {proc.stderr.strip()}")
    listed = {}
    for line in proc.stdout.splitlines():
        sha, _, name = line.partition("\t")
        listed[name.strip()] = sha.strip()
    sha = listed.get(ref + "^{}") or listed.get(ref)
    if sha is None:
        raise TagNotFound(f"{repo} has no tag {tag}")
    return sha


def ensure_clone(tag, sha, repo, workdir):
    """Clone ``tag`` of ``repo`` into ``<workdir>/spine_<sha>`` unless it is there already, check
    that its ``HEAD`` is ``sha``, and return the path of its ``src`` folder (the folder to put on
    ``PYTHONPATH``).

    Raises ``CheckoutError`` when ``sha`` is not a full commit SHA, when the clone fails, or when
    the checkout's ``HEAD`` is another commit (the tag was moved after ``sha`` was resolved).
    """
    if not SHA_RE.match(sha or ""):
        raise CheckoutError(f"not a full commit SHA: {sha!r}")
    path = os.path.join(workdir, f"spine_{sha}")
    if not os.path.isdir(os.path.join(path, "src", "spine")):
        os.makedirs(workdir, exist_ok=True)
        # clone next to the final folder, then rename: an interrupted clone leaves no half
        # checkout under the final name
        partial = f"{path}.partial-{os.getpid()}"
        shutil.rmtree(partial, ignore_errors=True)
        cmd = ["git", "clone", "-q", "--branch", tag]
        if "://" in repo:
            cmd += ["--depth", "1"]
        proc = subprocess.run(cmd + [repo, partial], capture_output=True, text=True)
        if proc.returncode != 0:
            shutil.rmtree(partial, ignore_errors=True)
            raise CheckoutError(f"git clone of {tag} from {repo} failed: {proc.stderr.strip()}")
        try:
            os.rename(partial, path)
        except OSError:  # another run cloned the same commit meanwhile
            shutil.rmtree(partial, ignore_errors=True)
    proc = subprocess.run(["git", "-C", path, "rev-parse", "HEAD"], capture_output=True, text=True)
    head = proc.stdout.strip()
    if proc.returncode != 0 or head != sha:
        raise CheckoutError(f"{path}: HEAD is {head or '?'}, expected {sha} (tag {tag})")
    return os.path.join(path, "src")
