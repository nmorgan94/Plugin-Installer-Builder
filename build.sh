#!/bin/bash
# Build a distributable installer for the plugins in plugin-binaries/.
# Settings: none for unsigned builds; --notarize and the overrides are in README > Settings.
#
# Each stage lives in scripts/ and can be run on its own for debugging --
# see ./build.sh --help.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

NOTARIZE=false
BUILD_PKG=false
BUILD_DMG=false
BUILD_EXE=false

usage() {
    echo "Usage: $0 (--pkg | --dmg | --exe) [--notarize]"
    echo ""
    echo "Exactly one of --pkg, --dmg or --exe is required."
    echo ""
    echo "Options:"
    echo "  --pkg        Build a .pkg installer"
    echo "  --dmg        Build a .dmg disk image"
    echo "  --exe        Build a Windows .exe installer from plugin-binaries/Windows/"
    echo "  --notarize   Sign and notarize the output (distribution; Mac only)"
    echo ""
    echo "The product name, version and bundle ID are read from the plugin bundles."
    echo "Output: dist/<Product-Name>-<version>-Installer.<ext>; -unsigned without --notarize, -not-notarized until stapled."
    echo ""
    echo "A notarized .pkg needs a Developer ID Installer certificate."
    echo "A notarized .dmg needs only a Developer ID Application certificate,"
    echo "so use --dmg if the Installer certificate is unavailable."
    echo ""
    echo "Individual stages, runnable on their own after 'source .env':"
    echo "  scripts/stage-payload.sh"
    echo "  scripts/sign-plugins.sh"
    echo "  pkg/build-pkg.sh"
    echo "  dmg/build-dmg.sh"
    echo "  exe/build-exe.sh"
    echo "  scripts/notarize.sh <artifact>"
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --notarize)
            NOTARIZE=true
            shift
            ;;
        --pkg)
            BUILD_PKG=true
            shift
            ;;
        --dmg)
            BUILD_DMG=true
            shift
            ;;
        --exe)
            BUILD_EXE=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

FORMATS=0
for format in "$BUILD_PKG" "$BUILD_DMG" "$BUILD_EXE"; do
    [ "$format" = false ] || FORMATS=$((FORMATS + 1))
done

if [ "$FORMATS" -gt 1 ]; then
    echo "Error: --pkg, --dmg and --exe are mutually exclusive -- pick one." >&2
    exit 1
fi

if [ "$FORMATS" -eq 0 ]; then
    echo "Error: no output format selected -- pass --pkg, --dmg or --exe." >&2
    echo "" >&2
    usage >&2
    exit 1
fi

if [ "$BUILD_EXE" = true ] && [ "$NOTARIZE" = true ]; then
    echo "Error: --notarize is for Mac installers; Windows signing isn't supported yet." >&2
    exit 1
fi

export BUILD_DIR WINDOWS_BUILD_DIR PAYLOAD_DIR PACKAGES_DIR DIST_DIR DMG_STAGING_DIR EXE_STAGING_DIR

for var in PRODUCT_NAME BUNDLE_ID VERSION; do
    [ -z "${!var:-}" ] || log "Note: ignoring $var from the environment; it's read from the bundles. Remove it from .env."
done

if [ "$NOTARIZE" = true ]; then
    REQUIRED=(SIGNING_IDENTITY_APP)

    if [ "$BUILD_PKG" = true ]; then
        REQUIRED+=(SIGNING_IDENTITY_INSTALLER)
    fi

    if [ -z "${KEYCHAIN_PROFILE:-}" ]; then
        REQUIRED+=(APPLE_ID APPLE_TEAM_ID APPLE_APP_PASSWORD)
    fi

    require_env "${REQUIRED[@]}"
fi

summary() {
    echo ""
    echo "✅ Build complete!"
    echo "📦 Package: $1"
    echo "🔐 $2"
}

if [ "$BUILD_EXE" = true ]; then
    summary "$("$SCRIPT_DIR/exe/build-exe.sh")" "Signed: No (Windows signing isn't supported yet)"
    exit 0
fi

"$SCRIPT_DIR/scripts/stage-payload.sh" >/dev/null

if [ "$NOTARIZE" = true ]; then
    "$SCRIPT_DIR/scripts/sign-plugins.sh"
fi

if [ "$BUILD_DMG" = true ]; then
    STAGE="dmg/build-dmg.sh"
    IDENTITY="${SIGNING_IDENTITY_APP:-}"
else
    STAGE="pkg/build-pkg.sh"
    IDENTITY="${SIGNING_IDENTITY_INSTALLER:-}"
fi

if [ "$NOTARIZE" = true ]; then
    SIGN_ARGS=(--sign "$IDENTITY")
else
    SIGN_ARGS=(--no-sign)
fi

FINAL_ARTIFACT="$("$SCRIPT_DIR/$STAGE" "${SIGN_ARGS[@]}")"

if [ "$NOTARIZE" = true ]; then
    FINAL_ARTIFACT="$("$SCRIPT_DIR/scripts/notarize.sh" "$FINAL_ARTIFACT")"
fi

rm -rf "$PAYLOAD_DIR"

if [ "$NOTARIZE" = true ]; then
    summary "$FINAL_ARTIFACT" "Signed & Notarized: Yes"
else
    summary "$FINAL_ARTIFACT" "Signed & Notarized: No (unsigned build)"
fi
