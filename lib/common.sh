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

# Plugins sit inside the payload at their real install paths.
VST3_INSTALL_DIR="/Library/Audio/Plug-Ins/VST3"
COMPONENTS_INSTALL_DIR="/Library/Audio/Plug-Ins/Components"
PAYLOAD_VST3_DIR="$PAYLOAD_DIR$VST3_INSTALL_DIR"
PAYLOAD_COMPONENTS_DIR="$PAYLOAD_DIR$COMPONENTS_INSTALL_DIR"
