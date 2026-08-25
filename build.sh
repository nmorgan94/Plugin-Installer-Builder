#!/bin/bash
# Build a distributable installer for the plugins in plugin-binaries/.
#
# Each stage lives in scripts/ and can be run on its own for debugging --
# see ./build.sh --help.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

NOTARIZE=false
BUILD_PKG=false
BUILD_DMG=false

usage() {
    echo "Usage: $0 (--pkg | --dmg) [--notarize]"
    echo ""
    echo "Exactly one of --pkg or --dmg is required."
    echo ""
    echo "Options:"
    echo "  --pkg        Build a .pkg installer"
    echo "  --dmg        Build a .dmg disk image"
    echo "  --notarize   Sign and notarize the output (distribution)"
    echo ""
    echo "Configured by environment (see .env.example):"
    echo "  PRODUCT_NAME  Installer title, and the artifact filename"
    echo "  BUNDLE_ID     Package identifier"
    echo "  VERSION       Package version"
    echo ""
    echo "A notarized .pkg needs a Developer ID Installer certificate."
    echo "A notarized .dmg needs only a Developer ID Application certificate,"
    echo "so use --dmg if the Installer certificate is unavailable."
    echo ""
    echo "Individual stages, runnable on their own after 'source .env':"
    echo "  scripts/stage-payload.sh"
    echo "  scripts/sign-plugins.sh"
    echo "  scripts/build-pkg.sh"
    echo "  scripts/build-dmg.sh"
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

if [ "$BUILD_PKG" = true ] && [ "$BUILD_DMG" = true ]; then
    echo "Error: --pkg and --dmg are mutually exclusive -- pick one." >&2
    exit 1
fi

if [ "$BUILD_PKG" = false ] && [ "$BUILD_DMG" = false ]; then
    echo "Error: no output format selected -- pass --pkg or --dmg." >&2
    echo "" >&2
    usage >&2
    exit 1
fi

export VERSION PRODUCT_NAME BUNDLE_ID BUILD_DIR PAYLOAD_DIR PACKAGES_DIR DIST_DIR DMG_STAGING_DIR

REQUIRED=(PRODUCT_NAME)

if [ "$BUILD_PKG" = true ]; then
    REQUIRED+=(BUNDLE_ID VERSION)
fi

if [ "$NOTARIZE" = true ]; then
    REQUIRED+=(SIGNING_IDENTITY_APP)

    if [ "$BUILD_PKG" = true ]; then
        REQUIRED+=(SIGNING_IDENTITY_INSTALLER)
    fi

    if [ -z "${KEYCHAIN_PROFILE:-}" ]; then
        REQUIRED+=(APPLE_ID APPLE_TEAM_ID APPLE_APP_PASSWORD)
    fi
fi

require_env "${REQUIRED[@]}"

"$SCRIPT_DIR/scripts/stage-payload.sh" >/dev/null

if [ "$NOTARIZE" = true ]; then
    "$SCRIPT_DIR/scripts/sign-plugins.sh"
fi

if [ "$BUILD_DMG" = true ]; then
    STAGE="build-dmg.sh"
    IDENTITY="${SIGNING_IDENTITY_APP:-}"
else
    STAGE="build-pkg.sh"
    IDENTITY="${SIGNING_IDENTITY_INSTALLER:-}"
fi

if [ "$NOTARIZE" = true ]; then
    SIGN_ARGS=(--sign "$IDENTITY")
else
    SIGN_ARGS=(--no-sign)
fi

FINAL_ARTIFACT="$("$SCRIPT_DIR/scripts/$STAGE" "${SIGN_ARGS[@]}")"

if [ "$NOTARIZE" = true ]; then
    "$SCRIPT_DIR/scripts/notarize.sh" "$FINAL_ARTIFACT"
fi

rm -rf "$PAYLOAD_DIR"

echo ""
echo "✅ Build complete!"
echo "📦 Package: $FINAL_ARTIFACT"

if [ "$NOTARIZE" = true ]; then
    echo "🔐 Signed & Notarized: Yes"
else
    echo "🔐 Signed & Notarized: No (unsigned build)"
fi
