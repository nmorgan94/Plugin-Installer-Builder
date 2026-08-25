#!/bin/bash
# Build a distribution .dmg from the staged payload. Prints its path.
# Env: PRODUCT_NAME, SIGNING_IDENTITY_APP   Flags: --sign ID | --no-sign

set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/common.sh"

SIGN_IDENTITY="${SIGNING_IDENTITY_APP:-}"
case "${1:-}" in
    --sign)    SIGN_IDENTITY="${2:?--sign requires an identity}" ;;
    --no-sign) SIGN_IDENTITY="" ;;
    "")        ;;
    *)         die "Unknown option: $1 (expected --sign ID or --no-sign)" ;;
esac

require_env PRODUCT_NAME

[ -d "$PAYLOAD_DIR" ] || die "Payload not found. Run scripts/stage-payload.sh first."

OUT_DMG="$DIST_DIR/${PRODUCT_NAME}.dmg"
mkdir -p "$DIST_DIR"

log "Preparing disk image contents..."
rm -rf "$DMG_STAGING_DIR"
mkdir -p "$DMG_STAGING_DIR"

staged=0
for bundle in "$PAYLOAD_VST3_DIR"/*.vst3 "$PAYLOAD_COMPONENTS_DIR"/*.component; do
    [ -d "$bundle" ] || continue
    log "Adding $(basename "$bundle")"
    cp -R "$bundle" "$DMG_STAGING_DIR/"
    staged=$((staged + 1))
done
[ "$staged" -gt 0 ] || die "No plugin bundles found in $PAYLOAD_DIR"

# Aliases to the real install locations
[ -n "$(find "$DMG_STAGING_DIR" -maxdepth 1 -name '*.vst3' -print -quit)" ] &&
    ln -s "$VST3_INSTALL_DIR" "$DMG_STAGING_DIR/VST3 Folder"
[ -n "$(find "$DMG_STAGING_DIR" -maxdepth 1 -name '*.component' -print -quit)" ] &&
    ln -s "$COMPONENTS_INSTALL_DIR" "$DMG_STAGING_DIR/Components Folder"

log "Creating disk image..."
hdiutil create -volname "$PRODUCT_NAME" -srcfolder "$DMG_STAGING_DIR" \
    -ov -format UDZO "$OUT_DMG" >&2

rm -rf "$DMG_STAGING_DIR"

if [ -n "$SIGN_IDENTITY" ]; then
    log "Signing disk image..."
    codesign --force --sign "$SIGN_IDENTITY" --timestamp "$OUT_DMG" >&2
    codesign --verify --strict --verbose=4 "$OUT_DMG" >&2
fi

echo "$OUT_DMG"
