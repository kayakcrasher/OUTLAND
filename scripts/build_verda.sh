#!/usr/bin/env bash
set -euo pipefail

project_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_directory"

python3 tools/generate_verda_kit.py
python3 tools/generate_verda_audio.py
python3 tools/validate_verda_kit.py
cmake -S . -B build -DOUTLAND_BUILD_TESTS=ON
cmake --build build -j2
# Refresh generated audio even if only an asset generator changed and no relink occurred.
cmake -E copy_directory assets build/assets
ctest --test-dir build --output-on-failure
