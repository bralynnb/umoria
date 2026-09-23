#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build-web/data
make -f web/engine.mk -j "${BUILD_JOBS:-4}"
cp data/*.txt build-web/data/
sed 's/${umoria_version}/5.7.15-coop/g;s/${current_date}/2026-09-23/g' data/splash.txt.in > build-web/data/splash.txt
sed 's/${umoria_version}/5.7.15-coop/g;s/${current_date}/2026-09-23/g' data/versions.txt.in > build-web/data/versions.txt
cp LICENSE AUTHORS build-web/
