#!/bin/bash
# Copy the Mac plugin bundles into the payload tree.
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/common.sh"

[ -d "$MAC_PLUGINS_DIR" ] || die "Mac plugin builds not found: put them in $MAC_PLUGINS_DIR"

rm -rf "$PAYLOAD_DIR"
mkdir -p "$PAYLOAD_VST3_DIR" "$PAYLOAD_COMPONENTS_DIR"

log "Preparing payload..."
staged=0

for plugin in "$MAC_PLUGINS_DIR"/*.vst3 "$MAC_PLUGINS_DIR"/*.component; do
    [ -d "$plugin" ] || continue
    case "$plugin" in
        *.vst3) dest="$PAYLOAD_VST3_DIR" ;;
        *)      dest="$PAYLOAD_COMPONENTS_DIR" ;;
    esac
    log "Adding $(basename "$plugin")"
    cp -R "$plugin" "$dest/"
    staged=$((staged + 1))
done

[ "$staged" -gt 0 ] || die "No .vst3 or .component bundles found in $MAC_PLUGINS_DIR"

# Here, before anything is signed, so mismatched bundles stop the build early
read_payload_metadata
log "Staged $staged plugin bundle(s): $PRODUCT_NAME $VERSION ($BUNDLE_ID)"
