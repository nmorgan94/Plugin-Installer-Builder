#!/bin/bash
# Build the Windows .exe: CI's installer with the plugins zipped on. Prints its path.
# Env, optional: WINDOWS_BUILD_DIR (default plugin-binaries/Windows)
#                INSTALLER_EXE: a local installer exe instead of CI's
# No Mac-only tools; on Windows it needs tar.exe, as Git Bash's tar can't zip

set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/common.sh"

[ -d "$WINDOWS_BUILD_DIR" ] || die "Windows plugin builds not found: put them in $WINDOWS_BUILD_DIR"

bundles=()
for bundle in "$WINDOWS_BUILD_DIR"/*; do
    [ -e "$bundle" ] || continue
    case "$bundle" in
        *.vst3) [ -d "$bundle/Contents/x86_64-win" ] || die "$(basename "$bundle") isn't a 64-bit Windows build" ;;
        *)      die "only VST3 plugins can be installed on Windows, not $(basename "$bundle")" ;;
    esac
    bundles+=("$bundle")
done
[ ${#bundles[@]} -gt 0 ] || die "No .vst3 plugins in $WINDOWS_BUILD_DIR"

read_plugin_metadata "${bundles[@]}"
log "Windows build: $PRODUCT_NAME $VERSION ($BUNDLE_ID)"

# It names the Apps & Features entry, so must match the Mac bundle ID
for bundle in "${bundles[@]}"; do
    mac_id="$(plist_get "$BUILD_DIR/$(basename "$bundle")/Contents/Info.plist" CFBundleIdentifier)"
    [ -z "$mac_id" ] || [ "$mac_id" = "$BUNDLE_ID" ] ||
        die "the Mac build's bundle ID is $mac_id, but the Windows build gives $BUNDLE_ID (com.<vendor>.<name>)"
done

INSTALLER_EXE="${INSTALLER_EXE:-$(fetch_installer Windows)}"
[ -f "$INSTALLER_EXE" ] || die "Installer exe not found: $INSTALLER_EXE"

OUT_EXE="$(artifact_path exe "")"
mkdir -p "$DIST_DIR"

log "Preparing plugins..."
rm -rf "$EXE_STAGING_DIR"
mkdir -p "$EXE_STAGING_DIR/payload"

for bundle in "${bundles[@]}"; do
    log "Adding $(basename "$bundle")"
    cp -R "$bundle" "$EXE_STAGING_DIR/payload/"
done
write_manifest "$EXE_STAGING_DIR/payload"

# bsdtar (macOS, and Windows' tar.exe) writes zips
(cd "$EXE_STAGING_DIR/payload" && tar --format zip -cf ../payload.zip *)

log "Creating installer..."
size="$(wc -c < "$EXE_STAGING_DIR/payload.zip" | tr -d ' ')"
{
    cat "$INSTALLER_EXE" "$EXE_STAGING_DIR/payload.zip"
    printf 'PluginPayload1 %016x' "$size"
} > "$OUT_EXE"

rm -rf "$EXE_STAGING_DIR"
echo "$OUT_EXE"
