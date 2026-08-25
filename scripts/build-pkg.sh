#!/bin/bash
# Build the installer .pkg from the staged payload. Prints its path.
# Env: PRODUCT_NAME, BUNDLE_ID, VERSION, SIGNING_IDENTITY_INSTALLER
# Flags: --sign ID | --no-sign
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../lib/common.sh"

SIGN_IDENTITY="${SIGNING_IDENTITY_INSTALLER:-}"
case "${1:-}" in
    --sign)    SIGN_IDENTITY="${2:?--sign requires an identity}" ;;
    --no-sign) SIGN_IDENTITY="" ;;
    "")        ;;
    *)         die "Unknown option: $1 (expected --sign ID or --no-sign)" ;;
esac

require_env PRODUCT_NAME BUNDLE_ID VERSION

[ -d "$PAYLOAD_DIR" ] || die "Payload not found. Run scripts/stage-payload.sh first."
[ -f "$DISTRIBUTION_TEMPLATE" ] || die "Template not found: $DISTRIBUTION_TEMPLATE"

mkdir -p "$PACKAGES_DIR" "$DIST_DIR"

# Render the template.
RENDERED="$PACKAGES_DIR/distribution.xml"
XML="$(cat "$DISTRIBUTION_TEMPLATE")"
XML="${XML//\$\{PRODUCT_NAME\}/$(xml_escape "$PRODUCT_NAME")}"
XML="${XML//\$\{BUNDLE_ID\}/$(xml_escape "$BUNDLE_ID")}"
printf '%s\n' "$XML" > "$RENDERED"

OUT_PKG="$DIST_DIR/${PRODUCT_NAME}-Installer.pkg"

log "Building installer package ($BUNDLE_ID, version $VERSION)..."
pkgbuild --root "$PAYLOAD_DIR" --identifier "$BUNDLE_ID" --version "$VERSION" \
    --install-location / "$PACKAGES_DIR/component.pkg" >&2

log "Creating final installer..."
productbuild --distribution "$RENDERED" --package-path "$PACKAGES_DIR" "$OUT_PKG" >&2
rm -f "$PACKAGES_DIR/component.pkg" "$RENDERED"

if [ -n "$SIGN_IDENTITY" ]; then
    log "Signing installer package..."
    productsign --sign "$SIGN_IDENTITY" "$OUT_PKG" "${OUT_PKG%.pkg}-signed.pkg" >&2
    rm -f "$OUT_PKG"
    OUT_PKG="${OUT_PKG%.pkg}-signed.pkg"
    pkgutil --check-signature "$OUT_PKG" >&2
fi

echo "$OUT_PKG"
