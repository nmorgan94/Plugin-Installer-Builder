#!/bin/bash
# Build a distribution .dmg from the staged payload, containing the plugins
# and an installer app that copies them into place. Prints the .dmg path.
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

OUT_DMG="$DIST_DIR/${PRODUCT_NAME}-Installer.dmg"
mkdir -p "$DIST_DIR"

log "Preparing disk image contents..."
rm -rf "$DMG_STAGING_DIR"

PLUGINS_DIR="$DMG_STAGING_DIR/$DMG_PLUGINS_DIR"
mkdir -p "$PLUGINS_DIR"

staged=0
for bundle in "$PAYLOAD_VST3_DIR"/*.vst3 "$PAYLOAD_COMPONENTS_DIR"/*.component; do
    [ -d "$bundle" ] || continue
    log "Adding $(basename "$bundle")"
    cp -R "$bundle" "$PLUGINS_DIR/"
    staged=$((staged + 1))
done
[ "$staged" -gt 0 ] || die "No plugin bundles found in $PAYLOAD_DIR"


APP="$DMG_STAGING_DIR/Install $PRODUCT_NAME.app"

log "Building installer app..."
osacompile -o "$APP" "$REPO_ROOT/resources/installer.applescript" >&2
INSTALL_SH="$(cat "$REPO_ROOT/resources/install.sh")"
printf '%s\n' "${INSTALL_SH//@PLUGINS_DIR@/$DMG_PLUGINS_DIR}" > "$APP/Contents/Resources/install.sh"
chmod +x "$APP/Contents/Resources/install.sh"

if [ -n "$SIGN_IDENTITY" ]; then
    log "Signing installer app..."
    codesign --force --sign "$SIGN_IDENTITY" --options runtime --timestamp "$APP" >&2
fi

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
