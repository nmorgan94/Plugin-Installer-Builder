#!/bin/bash
# Build the Windows .exe: CI's installer with the plugins zipped on. Prints its path.
# Env, optional: INSTALLER_EXE, a local installer exe instead of CI's
# Runs on a Mac, or in Git Bash on Windows

set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/common.sh"

[ -d "$WINDOWS_PLUGINS_DIR" ] || die "Windows plugin builds not found: put them in $WINDOWS_PLUGINS_DIR"

bundles=()
for bundle in "$WINDOWS_PLUGINS_DIR"/*; do
    [ -e "$bundle" ] || continue
    case "$bundle" in
        *.vst3) [ -d "$bundle/Contents/x86_64-win" ] || die "$(basename "$bundle") isn't a 64-bit Windows build" ;;
        *)      die "only VST3 plugins can be installed on Windows, not $(basename "$bundle")" ;;
    esac
    bundles+=("$bundle")
done
[ ${#bundles[@]} -gt 0 ] || die "No .vst3 plugins in $WINDOWS_PLUGINS_DIR"

read_info info "${bundles[@]}"
log "Windows build: $PRODUCT_NAME $VERSION ($BUNDLE_ID)"

# It names the Apps & Features entry, so must match the Mac bundle ID
for bundle in "${bundles[@]}"; do
    mac_id="$(plist_get "$MAC_PLUGINS_DIR/$(basename "$bundle")/Contents/Info.plist" CFBundleIdentifier)"
    [ -z "$mac_id" ] || [ "$mac_id" = "$BUNDLE_ID" ] ||
        die "the Mac build's bundle ID is $mac_id, but the Windows build gives $BUNDLE_ID (com.<vendor>.<name>)"
done

find_installer_exe

OUT_EXE="$(artifact_path exe "")"
mkdir -p "$OUTPUT_DIR"

log "Preparing plugins..."
rm -rf "$EXE_STAGING_DIR"
mkdir -p "$EXE_STAGING_DIR/payload"

for bundle in "${bundles[@]}"; do
    log "Adding $(basename "$bundle")"
    cp -R "$bundle" "$EXE_STAGING_DIR/payload/"
done
read_info manifest "$EXE_STAGING_DIR/payload"

log "Creating installer..."
installer_command pack-exe "$INSTALLER_EXE" "$EXE_STAGING_DIR/payload" "$OUT_EXE"

rm -rf "$EXE_STAGING_DIR"
echo "$OUT_EXE"
