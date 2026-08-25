#!/bin/bash
# Copy plugin bundles from plugin-binaries/ into the payload tree.
# Env: BUILD_DIR, PAYLOAD_DIR
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/common.sh"

[ -d "$BUILD_DIR" ] || die "Build directory not found: $BUILD_DIR"

rm -rf "$PAYLOAD_DIR"
mkdir -p "$PAYLOAD_VST3_DIR" "$PAYLOAD_COMPONENTS_DIR"

log "Preparing payload..."
staged=0

for plugin in "$BUILD_DIR"/*.vst3 "$BUILD_DIR"/*.component; do
    [ -d "$plugin" ] || continue
    case "$plugin" in
        *.vst3) dest="$PAYLOAD_VST3_DIR" ;;
        *)      dest="$PAYLOAD_COMPONENTS_DIR" ;;
    esac
    log "Adding $(basename "$plugin")"
    cp -R "$plugin" "$dest/"
    staged=$((staged + 1))
done

[ "$staged" -gt 0 ] || die "No .vst3 or .component bundles found in $BUILD_DIR"

log "Staged $staged plugin bundle(s)."
