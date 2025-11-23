#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
EXTERNAL_DIR="$ROOT_DIR/external"
mkdir -p "$EXTERNAL_DIR/httplib" "$EXTERNAL_DIR/json"

echo "Downloading cpp-httplib single-header..."
curl -fsSL -o "$EXTERNAL_DIR/httplib/httplib.h" \
  https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h

echo "Downloading nlohmann json single-header..."
curl -fsSL -o "$EXTERNAL_DIR/json/json.hpp" \
  https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp

echo "Done. Headers saved under $EXTERNAL_DIR"
