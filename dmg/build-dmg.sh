#!/bin/bash
# Build a distribution .dmg from the staged payload, containing the plugins
# and an installer app that copies them into place. Prints the .dmg path.
# Env, optional: SIGNING_IDENTITY_APP signs the app and .dmg (or --sign ID; --no-sign skips)
#                INSTALLER_APP: a local installer app instead of CI's

set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/common.sh"

LAYOUT="$REPO_ROOT/dmg/layout.json"

# Lays out the DMG window without Finder; pinned, installed on first use
DMGBUILD_PACKAGES="dmgbuild==1.6.7 ds_store==1.3.3 mac_alias==2.2.3"
DMGBUILD_VENV="$REPO_ROOT/dmg/.venv"

dmgbuild() {
    if [ "$(cat "$DMGBUILD_VENV/packages" 2>/dev/null)" != "$DMGBUILD_PACKAGES" ]; then
        python3 -c 'import sys; sys.exit(sys.version_info < (3, 10))' 2>/dev/null \
            || die "dmgbuild needs Python 3.10 or newer (brew install python)"
        log "Installing $DMGBUILD_PACKAGES..."
        rm -rf "$DMGBUILD_VENV"
        python3 -m venv "$DMGBUILD_VENV"
        "$DMGBUILD_VENV/bin/pip" install --quiet --disable-pip-version-check $DMGBUILD_PACKAGES >&2
        echo "$DMGBUILD_PACKAGES" > "$DMGBUILD_VENV/packages"
    fi

    "$DMGBUILD_VENV/bin/dmgbuild" "$@"
}

# Asks the installer app to draw background.png and @2x, labelled with its own name
draw_background() {
    local app="$1" out="$2"
    "$app/Contents/MacOS/$(plist_get "$app/Contents/Info.plist" CFBundleExecutable)" \
        --dmg-background "$out" "$(basename "$app" .app)" "$LAYOUT" >/dev/null 2>&1
}

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

INSTALLER_APP="${INSTALLER_APP:-$(fetch_installer macOS)}"
[ -d "$INSTALLER_APP" ] || die "Installer app not found: $INSTALLER_APP"

APP="$DMG_STAGING_DIR/Install $PRODUCT_NAME.app"
ditto "$INSTALLER_APP" "$APP"

# Each product's installer gets its own identity, so macOS and the password prompt name the product
plutil -replace CFBundleIdentifier -string "$BUNDLE_ID.installer" "$APP/Contents/Info.plist"
plutil -replace CFBundleName -string "Install $PRODUCT_NAME" "$APP/Contents/Info.plist"
plutil -replace CFBundleDisplayName -string "Install $PRODUCT_NAME" "$APP/Contents/Info.plist"

log "Drawing the window background..."
BACKGROUND_DIR="$(mktemp -d)"
draw_background "$APP" "$BACKGROUND_DIR" || die "The installer app couldn't draw the DMG background"

if [ -n "$SIGN_IDENTITY" ]; then
    # Nested code first: the app's signature covers the signed helper inside it
    log "Signing installer app..."
    sign_code "$SIGN_IDENTITY" "$APP/Contents/Helpers/InstallerHelper"
    sign_code "$SIGN_IDENTITY" "$APP"
    codesign --verify --strict --deep "$APP" >&2
fi

log "Creating disk image..."
dmgbuild -s "$REPO_ROOT/dmg/settings.py" -D staging="$DMG_STAGING_DIR" -D layout="$LAYOUT" \
    -D background="$BACKGROUND_DIR/background.png" -D app="$(basename "$APP")" -D hidden="$DMG_PLUGINS_DIR" \
    "$PRODUCT_NAME" "$OUT_DMG" >&2

rm -rf "$DMG_STAGING_DIR" "$BACKGROUND_DIR"

if [ -n "$SIGN_IDENTITY" ]; then
    log "Signing disk image..."
    codesign --force --sign "$SIGN_IDENTITY" --timestamp "$OUT_DMG" >&2
    codesign --verify --strict --verbose=4 "$OUT_DMG" >&2
fi

echo "$OUT_DMG"
