#!/bin/bash
# Build installers from packaging/input/, or from a plugin repo built in CI.
# Settings are in the README; ./build.sh --help lists the stages, which also run on their own.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/common.sh"

NOTARIZE=false
MAC_FORMAT=""  # pkg or dmg
BUILD_EXE=false
PLUGIN_REPO=""

usage() {
    echo "Usage: $0 [--plugin <repo>] (--pkg | --dmg) [--exe] [--notarize]"
    echo "       $0 [--plugin <repo>] --exe"
    echo ""
    echo "Options:"
    echo "  --pkg              Build a .pkg installer"
    echo "  --dmg              Build a .dmg disk image"
    echo "  --exe              Build a Windows .exe installer; can go with --pkg or --dmg"
    echo "  --notarize         Sign and notarize the Mac output (distribution)"
    echo "  --plugin <repo>    Build the plugin repo's main in CI (Build Plugin workflow) and"
    echo "                     package that, instead of packaging/input/"
    echo ""
    echo "The product name, version and bundle ID are read from the plugin bundles."
    echo "Output: packaging/output/<Product-Name>-<version>-Installer.<ext>; Mac output ends -unsigned without --notarize, -not-notarized until stapled."
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
        --pkg|--dmg)
            [ -z "$MAC_FORMAT" ] || [ "$MAC_FORMAT" = "${1#--}" ] ||
                die "--pkg and --dmg are mutually exclusive -- pick one."
            MAC_FORMAT="${1#--}"
            shift
            ;;
        --exe)
            BUILD_EXE=true
            shift
            ;;
        --plugin)
            [ -n "${2:-}" ] || die "--plugin needs a repo name, e.g. --plugin Acme-Reverb"
            PLUGIN_REPO="$2"
            shift 2
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

if [ -z "$MAC_FORMAT" ] && [ "$BUILD_EXE" = false ]; then
    echo "Error: no output format selected -- pass --pkg, --dmg or --exe." >&2
    echo "" >&2
    usage >&2
    exit 1
fi

if [ -z "$MAC_FORMAT" ] && [ "$NOTARIZE" = true ]; then
    echo "Error: --notarize is for Mac installers; Windows signing isn't supported yet." >&2
    exit 1
fi

for var in PRODUCT_NAME BUNDLE_ID VERSION; do
    [ -z "${!var:-}" ] || log "Note: ignoring $var from the environment; it's read from the bundles. Remove it from .env."
done

if [ "$NOTARIZE" = true ]; then
    REQUIRED=(SIGNING_IDENTITY_APP)

    if [ "$MAC_FORMAT" = pkg ]; then
        REQUIRED+=(SIGNING_IDENTITY_INSTALLER)
    fi

    if [ -z "${KEYCHAIN_PROFILE:-}" ]; then
        REQUIRED+=(APPLE_ID APPLE_TEAM_ID APPLE_APP_PASSWORD)
    fi

    require_env "${REQUIRED[@]}"
fi

# After the settings checks, so a missing one doesn't wait for a CI build first
if [ -n "$PLUGIN_REPO" ]; then
    # Set first, so a failed or cancelled download is cleaned up too
    trap 'rm -rf "${PLUGIN_DOWNLOAD_DIR:-}"' EXIT
    PLATFORMS=()
    [ -z "$MAC_FORMAT" ] || PLATFORMS+=(macOS)
    [ "$BUILD_EXE" = false ] || PLATFORMS+=(Windows)
    fetch_plugin "$PLUGIN_REPO" "${PLATFORMS[@]}"
fi

# Once, so every stage uses the same installer app
find_installer_tool

export MAC_PLUGINS_DIR WINDOWS_PLUGINS_DIR OUTPUT_DIR

RESULTS=()

if [ -n "$MAC_FORMAT" ]; then
    "$SCRIPT_DIR/scripts/stage-payload.sh" >/dev/null

    # Both installers are one release, so stop before signing if the builds differ
    if [ "$BUILD_EXE" = true ]; then
        read_payload_metadata
        MAC_BUILD="$PRODUCT_NAME $VERSION ($BUNDLE_ID)"
        read_info info "$WINDOWS_PLUGINS_DIR"/*.vst3
        WINDOWS_BUILD="$PRODUCT_NAME $VERSION ($BUNDLE_ID)"
        [ "$MAC_BUILD" = "$WINDOWS_BUILD" ] || die "the Mac build is $MAC_BUILD but the Windows build is $WINDOWS_BUILD"
    fi

    if [ "$NOTARIZE" = true ]; then
        "$SCRIPT_DIR/scripts/sign-plugins.sh"
    fi

    IDENTITY="${SIGNING_IDENTITY_APP:-}"
    [ "$MAC_FORMAT" = dmg ] || IDENTITY="${SIGNING_IDENTITY_INSTALLER:-}"

    if [ "$NOTARIZE" = true ]; then
        SIGN_ARGS=(--sign "$IDENTITY")
    else
        SIGN_ARGS=(--no-sign)
    fi

    FINAL_ARTIFACT="$("$SCRIPT_DIR/$MAC_FORMAT/build-$MAC_FORMAT.sh" "${SIGN_ARGS[@]}")"
    STATUS="Signed & Notarized: No (unsigned build)"

    if [ "$NOTARIZE" = true ]; then
        FINAL_ARTIFACT="$("$SCRIPT_DIR/scripts/notarize.sh" "$FINAL_ARTIFACT")"
        STATUS="Signed & Notarized: Yes"
    fi

    RESULTS+=("📦 Package: $FINAL_ARTIFACT" "🔐 $STATUS")
    rm -rf "$PAYLOAD_DIR"
fi

if [ "$BUILD_EXE" = true ]; then
    RESULTS+=("📦 Package: $("$SCRIPT_DIR/exe/build-exe.sh")" "🔐 Signed: No (Windows signing isn't supported yet)")
fi

echo ""
echo "✅ Build complete!"
printf '%s\n' "${RESULTS[@]}"
