#!/bin/bash
# Notarize and staple a .pkg or .dmg, then print its path:  ./scripts/notarize.sh packaging/output/Plugin.dmg
# Env, required: APPLE_ID + APPLE_TEAM_ID + APPLE_APP_PASSWORD, or KEYCHAIN_PROFILE
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/common.sh"

ARTIFACT="${1:-}"
[ -n "$ARTIFACT" ] || die "Usage: $0 <artifact.pkg|artifact.dmg>"
[ -f "$ARTIFACT" ] || die "Artifact not found: $ARTIFACT"

notary_auth
notary_submit "$ARTIFACT"

log "Stapling notarization ticket..."
xcrun stapler staple "$ARTIFACT" >&2
xcrun stapler validate "$ARTIFACT" >&2
log "Notarization complete."

# Only now does it get the plain release name
RELEASE="${ARTIFACT/-not-notarized./.}"
[ "$RELEASE" = "$ARTIFACT" ] || mv "$ARTIFACT" "$RELEASE"
echo "$RELEASE"
