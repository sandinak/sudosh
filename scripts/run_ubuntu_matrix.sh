#!/usr/bin/env bash
set -euo pipefail

# Runs the Ubuntu 22.04 portion of the CI matrix locally in Docker.
# Requires: Docker daemon running.

IMAGE_TAG="sudosh-ubuntu-ci:local"
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

if ! docker info >/dev/null 2>&1; then
  echo "Docker daemon not running or not accessible." >&2
  exit 1
fi

echo "Building CI image (${IMAGE_TAG})..."
docker build -t "${IMAGE_TAG}" -f "$ROOT_DIR/docker/ubuntu-ci.Dockerfile" "$ROOT_DIR"

declare -a MATRIX=(
  "gcc|release|" \
  "gcc|release|address" \
  "gcc|release|undefined" \
  "clang|release|" \
  "clang|release|address" \
  "clang|release|undefined"
)

run_case() {
  local compiler="$1"; local mode="$2"; local sanitize="$3"
  echo "===== ubuntu-22.04 • ${compiler} • ${mode} • ${sanitize:-none} ====="
  local build_cmd="set -e; make clean; if [ -n '${sanitize}' ]; then make tests WERROR=1 SANITIZE=${sanitize}; else make tests WERROR=1; fi"
  if [ "${compiler}" = "clang" ]; then
    build_cmd="set -e; export CC=clang; make clean; if [ -n '${sanitize}' ]; then make tests WERROR=1 SANITIZE=${sanitize}; else make tests WERROR=1; fi"
  fi
  docker run --rm -t \
    -e CC="${compiler}" \
    "${IMAGE_TAG}" \
    bash -lc "${build_cmd}"
}

rc=0
for entry in "${MATRIX[@]}"; do
  IFS='|' read -r compiler mode sanitize <<<"$entry"
  if ! run_case "$compiler" "$mode" "$sanitize"; then
    rc=1
  fi
done

exit "$rc"

