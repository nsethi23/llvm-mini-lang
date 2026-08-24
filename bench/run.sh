#!/usr/bin/env bash
# Regenerates bench/results.md (and README.md's benchmark section) from
# scratch. See PRD.md M8's demo: this is the whole demo command.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

# A dedicated Release build, separate from the Debug build under build/ that
# development/tests normally use -- benchmarking a Debug binary (no
# optimization, extra checks) would badly misrepresent the interpreter's
# real speed, since it's the arm most sensitive to how mlang's own C++ was
# compiled.
build_dir="build-release"
if [[ ! -x "$build_dir/mlang" ]]; then
  echo "bench/run.sh: $build_dir/mlang not found, building it first..." >&2

  # See README.md's Prerequisites: llvm@18 is keg-only on macOS, and Linux
  # needs llvm-config-18's cmake dir explicitly too.
  prefix_path=""
  if [[ "$(uname -s)" == "Darwin" ]] && command -v brew >/dev/null; then
    prefix_path="$(brew --prefix llvm@18)"
  elif command -v llvm-config-18 >/dev/null; then
    prefix_path="$(llvm-config-18 --cmakedir)"
  fi

  cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    ${prefix_path:+-DCMAKE_PREFIX_PATH="$prefix_path"} >/dev/null
  cmake --build "$build_dir" -j --target mlang >/dev/null
fi

python3 bench/run.py --mlang "$build_dir/mlang"
