#!/bin/bash

set -euo pipefail

SRC="$(cd "$(dirname "$0")/../../.." && pwd)/@PLUGINS_DIR@"

# Overridable so the script can be exercised against a temp dir when testing.
VST3_DIR="${VST3_DIR:-/Library/Audio/Plug-Ins/VST3}"
COMPONENTS_DIR="${COMPONENTS_DIR:-/Library/Audio/Plug-Ins/Components}"

install_bundle() {
    local src="$1" dest_dir="$2" name
    name="$(basename "$src")"

    mkdir -p "$dest_dir"

    rm -rf "${dest_dir:?}/$name"
    cp -R "$src" "$dest_dir/"

    xattr -dr com.apple.quarantine "$dest_dir/$name" 2>/dev/null || true
}

installed=()
for p in "$SRC"/*.vst3; do
    [ -d "$p" ] || continue
    install_bundle "$p" "$VST3_DIR"
    installed+=("$(basename "$p")  ->  $VST3_DIR")
done
for p in "$SRC"/*.component; do
    [ -d "$p" ] || continue
    install_bundle "$p" "$COMPONENTS_DIR"
    installed+=("$(basename "$p")  ->  $COMPONENTS_DIR")
done

[ ${#installed[@]} -gt 0 ] || { echo "No plugin bundles found beside the installer." >&2; exit 1; }

# stdout becomes the text of the confirmation dialog.
printf 'Installed %d plugin bundle(s):\n\n' "${#installed[@]}"
printf '%s\n' "${installed[@]}"
printf '\nRestart your DAW to load them.\n'
