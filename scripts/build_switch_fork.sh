#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
git config --global --add safe.directory "$PWD"
cmake -B cmake-build-switch \
  -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DBUILTIN_NSP=OFF \
  -DBRLS_UNITY_BUILD=OFF -DANALYTICS=OFF -DSERVER_URL= -DSERVER_TOKEN=
cmake --build cmake-build-switch --target TsVitch.nro -j"${BUILD_JOBS:-4}"
