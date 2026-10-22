#!/bin/bash
# Code sign the plugin bundles staged in the payload tree.
# Env, required: SIGNING_IDENTITY_APP
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/common.sh"

require_env SIGNING_IDENTITY_APP
[ -d "$PAYLOAD_DIR" ] || die "Payload not found. Run scripts/stage-payload.sh first."

signed=0

for bundle in "$PAYLOAD_VST3_DIR"/*.vst3 "$PAYLOAD_COMPONENTS_DIR"/*.component; do
    [ -d "$bundle" ] || continue
    log "Signing $(basename "$bundle")..."

    while IFS= read -r nested; do
        [ -n "$nested" ] || continue
        log "  nested: ${nested#"$bundle"/}"
        sign_code "$SIGNING_IDENTITY_APP" "$nested"
    done < <(find "$bundle" \( -name '*.framework' -o -name '*.dylib' -o -name '*.bundle' \) -depth)

    sign_code "$SIGNING_IDENTITY_APP" "$bundle"
    codesign --verify --strict --verbose=4 "$bundle"
    signed=$((signed + 1))
done

[ "$signed" -gt 0 ] || die "No plugin bundles found in $PAYLOAD_DIR"

log "Signed $signed plugin bundle(s)."
