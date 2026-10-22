#!/bin/bash
# Build a distribution .dmg from the staged payload, containing the plugins
# and an installer app that copies them into place. Prints the .dmg path.
# Env: SIGNING_IDENTITY_APP (name and version come from the bundles)   Flags: --sign ID | --no-sign
#      INSTALLER_APP: a prebuilt installer app to use instead of building installer/

set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/common.sh"

SIGN_IDENTITY="${SIGNING_IDENTITY_APP:-}"
case "${1:-}" in
    --sign)    SIGN_IDENTITY="${2:?--sign requires an identity}" ;;
    --no-sign) SIGN_IDENTITY="" ;;
    "")        ;;
    *)         die "Unknown option: $1 (expected --sign ID or --no-sign)" ;;
esac

[ -d "$PAYLOAD_DIR" ] || die "Payload not found. Run scripts/stage-payload.sh first."
read_payload_metadata

OUT_DMG="$(artifact_path dmg "$SIGN_IDENTITY")"
mkdir -p "$DIST_DIR"

log "Preparing disk image contents..."
rm -rf "$DMG_STAGING_DIR"

PLUGINS_DIR="$DMG_STAGING_DIR/$DMG_PLUGINS_DIR"
mkdir -p "$PLUGINS_DIR"

for bundle in "$PAYLOAD_VST3_DIR"/*.vst3 "$PAYLOAD_COMPONENTS_DIR"/*.component; do
    [ -d "$bundle" ] || continue
    log "Adding $(basename "$bundle")"
    cp -R "$bundle" "$PLUGINS_DIR/"
done

write_manifest "$PLUGINS_DIR"
chflags hidden "$PLUGINS_DIR"

if [ -z "${INSTALLER_APP:-}" ]; then
    # Incremental, so quick when installer/ hasn't changed
    log "Building installer app..."
    (cd "$INSTALLER_DIR" && cmake --workflow --preset release) >&2
    INSTALLER_APP="$INSTALLER_DIR/build-release/SampleRealmInstaller_artefacts/Release/SampleRealm Installer.app"
fi
[ -d "$INSTALLER_APP" ] || die "Installer app not found: $INSTALLER_APP"

APP="$DMG_STAGING_DIR/Install $PRODUCT_NAME.app"
ditto "$INSTALLER_APP" "$APP"

# Each product's installer gets its own identity, so macOS and the password prompt name the product
plutil -replace CFBundleIdentifier -string "$BUNDLE_ID.installer" "$APP/Contents/Info.plist"
plutil -replace CFBundleName -string "Install $PRODUCT_NAME" "$APP/Contents/Info.plist"
plutil -replace CFBundleDisplayName -string "Install $PRODUCT_NAME" "$APP/Contents/Info.plist"

if [ -n "$SIGN_IDENTITY" ]; then
    # Nested code first: the app's signature covers the signed helper inside it
    log "Signing installer app..."
    sign_code "$SIGN_IDENTITY" "$APP/Contents/Helpers/InstallerHelper"
    sign_code "$SIGN_IDENTITY" "$APP"
    codesign --verify --strict --deep "$APP" >&2
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
