# Shared helpers for the build scripts. Sourced, never executed directly.

log() { echo "$@" >&2; }

die() { echo "Error: $*" >&2; exit 1; }

require_env() {
    local missing=() var
    for var in "$@"; do
        [ -n "${!var:-}" ] || missing+=("$var")
    done
    if [ ${#missing[@]} -gt 0 ]; then
        die "missing environment variable(s): ${missing[*]}
Set them in .env (see .env.example), then: source .env"
    fi
}

notary_auth() {
    if [ -n "${KEYCHAIN_PROFILE:-}" ]; then
        AUTH=(--keychain-profile "$KEYCHAIN_PROFILE")
        return
    fi

    require_env APPLE_ID APPLE_TEAM_ID APPLE_APP_PASSWORD
    case "$APPLE_APP_PASSWORD" in
        @keychain:*)
            die "APPLE_APP_PASSWORD is altool syntax that notarytool does not understand.
Instead use: export KEYCHAIN_PROFILE='${APPLE_APP_PASSWORD#@keychain:}'"
            ;;
    esac
    AUTH=(--apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_PASSWORD")
}

notary_submit() {
    local file="$1" output status id
    log "Submitting $(basename "$file") for notarization (may take several minutes)..."
    output="$(xcrun notarytool submit "$file" "${AUTH[@]}" --wait 2>&1)" || true
    echo "$output" >&2

    status="$(echo "$output" | sed -n 's/^ *status: \(.*\)$/\1/p' | tail -1)"
    if [ "$status" != "Accepted" ]; then
        id="$(echo "$output" | sed -n 's/^ *id: \([0-9a-fA-F-]*\).*/\1/p' | head -1)"
        [ -n "$id" ] && xcrun notarytool log "$id" "${AUTH[@]}" >&2 || true
        die "notarization failed (status: ${status:-unknown})"
    fi
}

xml_escape() {
    local s="$1"
    s="${s//&/&amp;}"
    s="${s//</&lt;}"
    s="${s//>/&gt;}"
    printf '%s' "$s"
}

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DISTRIBUTION_TEMPLATE="$REPO_ROOT/distribution.xml"

BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/plugin-binaries}"
PAYLOAD_DIR="${PAYLOAD_DIR:-$REPO_ROOT/payload}"
PACKAGES_DIR="${PACKAGES_DIR:-$REPO_ROOT/packages}"
DIST_DIR="${DIST_DIR:-$REPO_ROOT/dist}"
DMG_STAGING_DIR="${DMG_STAGING_DIR:-$REPO_ROOT/dmg-staging}"
DMG_PLUGINS_DIR="${DMG_PLUGINS_DIR:-.plugins}"

# Plugins sit inside the payload at their real install paths.
VST3_INSTALL_DIR="/Library/Audio/Plug-Ins/VST3"
COMPONENTS_INSTALL_DIR="/Library/Audio/Plug-Ins/Components"
PAYLOAD_VST3_DIR="$PAYLOAD_DIR$VST3_INSTALL_DIR"
PAYLOAD_COMPONENTS_DIR="$PAYLOAD_DIR$COMPONENTS_INSTALL_DIR"
