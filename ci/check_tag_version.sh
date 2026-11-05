#!/bin/bash
# Refuse a release tag that differs from project(VERSION) in CMakeLists.txt.
# Run it before pushing a tag; CI runs it again on every tag push, as a backstop.
#
#   ci/check_tag_version.sh vX.Y.Z
#
# Exit codes: 0 the tag matches; 1 it does not (or no version was found); 2 usage.
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 vX.Y.Z" >&2
  exit 2
fi
tag=$1
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

version=$(sed -n -E 's/^project\(spine-reader VERSION ([0-9]+\.[0-9]+\.[0-9]+)[ )].*/\1/p' \
  "$repo/CMakeLists.txt")
if [[ -z "$version" ]]; then
  echo "check_tag_version: no 'project(spine-reader VERSION X.Y.Z' line in $repo/CMakeLists.txt" >&2
  exit 1
fi
if [[ "$tag" != "v$version" ]]; then
  echo "check_tag_version: tag $tag does not match project(VERSION) $version (expected v$version)" >&2
  exit 1
fi
echo "check_tag_version: tag $tag matches project(VERSION) $version"
