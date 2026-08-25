#!/bin/bash
# Notarize and staple a .pkg or .dmg:  ./scripts/notarize.sh dist/Plugin.dmg
# Env: APPLE_ID + APPLE_TEAM_ID + APPLE_APP_PASSWORD, or KEYCHAIN_PROFILE
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
