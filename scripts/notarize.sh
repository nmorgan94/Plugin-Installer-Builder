#!/bin/bash
# Notarize and staple a .pkg or .dmg:  ./scripts/notarize.sh dist/Plugin.dmg
# Env: APPLE_ID + APPLE_TEAM_ID + APPLE_APP_PASSWORD, or KEYCHAIN_PROFILE
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/common.sh"

ARTIFACT="${1:-}"
[ -n "$ARTIFACT" ] || die "Usage: $0 <artifact.pkg|artifact.dmg>"
[ -f "$ARTIFACT" ] || die "Artifact not found: $ARTIFACT"

if [ -n "${KEYCHAIN_PROFILE:-}" ]; then
    AUTH=(--keychain-profile "$KEYCHAIN_PROFILE")
else
    require_env APPLE_ID APPLE_TEAM_ID APPLE_APP_PASSWORD
    case "$APPLE_APP_PASSWORD" in
        @keychain:*)
            die "APPLE_APP_PASSWORD is altool syntax that notarytool does not understand.
Instead use: export KEYCHAIN_PROFILE='${APPLE_APP_PASSWORD#@keychain:}'"
            ;;
    esac
    AUTH=(--apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_PASSWORD")
fi

log "Submitting $(basename "$ARTIFACT") for notarization (may take several minutes)..."


OUTPUT="$(xcrun notarytool submit "$ARTIFACT" "${AUTH[@]}" --wait 2>&1)" || true
echo "$OUTPUT" >&2

STATUS="$(echo "$OUTPUT" | sed -n 's/^ *status: \(.*\)$/\1/p' | tail -1)"
if [ "$STATUS" != "Accepted" ]; then
    ID="$(echo "$OUTPUT" | sed -n 's/^ *id: \([0-9a-fA-F-]*\).*/\1/p' | head -1)"
    [ -n "$ID" ] && xcrun notarytool log "$ID" "${AUTH[@]}" >&2 || true
    die "notarization failed (status: ${STATUS:-unknown})"
fi

log "Stapling notarization ticket..."
xcrun stapler staple "$ARTIFACT" >&2
xcrun stapler validate "$ARTIFACT" >&2
log "Notarization complete."
