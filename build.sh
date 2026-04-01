#!/bin/bash
set -e

# Parse command line arguments
NOTARIZE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --notarize)
            NOTARIZE=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--notarize]"
            echo ""
            echo "Options:"
            echo "  (no flags)   Build unsigned installer (development)"
            echo "  --notarize   Build signed and notarized installer (distribution)"
            exit 1
            ;;
    esac
done

# Check for required environment variables if notarizing
if [ "$NOTARIZE" = true ]; then
    if [ -z "$SIGNING_IDENTITY_APP" ]; then
        echo "Error: SIGNING_IDENTITY_APP environment variable not set"
        echo "Example: export SIGNING_IDENTITY_APP='Developer ID Application: Your Name'"
        exit 1
    fi
    if [ -z "$SIGNING_IDENTITY_INSTALLER" ]; then
        echo "Error: SIGNING_IDENTITY_INSTALLER environment variable not set"
        echo "Example: export SIGNING_IDENTITY_INSTALLER='Developer ID Installer: Your Name'"
        exit 1
    fi
    if [ -z "$APPLE_ID" ] || [ -z "$APPLE_TEAM_ID" ] || [ -z "$APPLE_APP_PASSWORD" ]; then
        echo "Error: Notarization requires APPLE_ID, APPLE_TEAM_ID, and APPLE_APP_PASSWORD"
        echo "Example:"
        echo "  export APPLE_ID='your@email.com'"
        echo "  export APPLE_TEAM_ID='TEAMID'"
        echo "  export APPLE_APP_PASSWORD='@keychain:AC_PASSWORD'"
        exit 1
    fi
fi

rm -f dist/*.pkg
mkdir -p dist

echo "Preparing payload..."

mkdir -p payload/Library/Audio/Plug-Ins/VST3/
mkdir -p payload/Library/Audio/Plug-Ins/Components/

BUILD_DIR="plugin-binaries"

echo "Searching for VST3 plugins..."

for plugin in $BUILD_DIR/*.vst3; do
    if [ -d "$plugin" ]; then
        echo "Adding $(basename "$plugin")"
        cp -R "$plugin" payload/Library/Audio/Plug-Ins/VST3/
    fi
done

echo "Searching for AU plugins..."

for plugin in $BUILD_DIR/*.component; do
    if [ -d "$plugin" ]; then
        echo "Adding $(basename "$plugin")"
        cp -R "$plugin" payload/Library/Audio/Plug-Ins/Components/
    fi
done

# Sign plugin binaries if notarizing
if [ "$NOTARIZE" = true ]; then
    echo "Signing plugin binaries..."
    
    # Sign VST3 plugins
    for plugin in payload/Library/Audio/Plug-Ins/VST3/*.vst3; do
        if [ -d "$plugin" ]; then
            echo "Signing $(basename "$plugin")..."
            codesign --force --sign "$SIGNING_IDENTITY_APP" \
                --options runtime \
                --timestamp \
                --deep \
                "$plugin"
            
            # Verify signature
            codesign --verify --verbose "$plugin"
        fi
    done
    
    # Sign AU plugins
    for plugin in payload/Library/Audio/Plug-Ins/Components/*.component; do
        if [ -d "$plugin" ]; then
            echo "Signing $(basename "$plugin")..."
            codesign --force --sign "$SIGNING_IDENTITY_APP" \
                --options runtime \
                --timestamp \
                --deep \
                "$plugin"
            
            # Verify signature
            codesign --verify --verbose "$plugin"
        fi
    done
    
    echo "Plugin signing complete."
fi

echo "Building installer package..."

IDENTIFIER=$(grep -o 'pkg-ref id="[^"]*"' distribution.xml | head -1 | sed 's/pkg-ref id="\(.*\)"/\1/')
PKG_NAME=$(grep -A1 'pkg-ref id=' distribution.xml | tail -1 | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
INSTALLER_NAME="${PKG_NAME%.pkg}-Installer.pkg"

pkgbuild \
  --root payload \
  --identifier $IDENTIFIER \
  --version 0.0.1 \
  --install-location / \
  packages/$PKG_NAME

echo "Creating final installer..."

UNSIGNED_PKG="dist/$INSTALLER_NAME"
SIGNED_PKG="dist/${INSTALLER_NAME%.pkg}-signed.pkg"
FINAL_PKG="$UNSIGNED_PKG"

productbuild \
  --distribution distribution.xml \
  --package-path packages \
  "$UNSIGNED_PKG"

# Sign and notarize if requested
if [ "$NOTARIZE" = true ]; then
    echo "Signing installer package..."
    productsign --sign "$SIGNING_IDENTITY_INSTALLER" \
        "$UNSIGNED_PKG" \
        "$SIGNED_PKG"
    
    # Verify package signature
    pkgutil --check-signature "$SIGNED_PKG"
    
    # Remove unsigned package and use signed one
    rm -f "$UNSIGNED_PKG"
    FINAL_PKG="$SIGNED_PKG"
    
    echo "Package signing complete."
    
    echo "Submitting package for notarization..."
    echo "This may take several minutes..."
    
    # Submit for notarization
    xcrun notarytool submit "$FINAL_PKG" \
        --apple-id "$APPLE_ID" \
        --team-id "$APPLE_TEAM_ID" \
        --password "$APPLE_APP_PASSWORD" \
        --wait
    
    if [ $? -eq 0 ]; then
        echo "Notarization successful!"
        
        # Staple the notarization ticket
        echo "Stapling notarization ticket..."
        xcrun stapler staple "$FINAL_PKG"
        
        # Verify stapling
        xcrun stapler validate "$FINAL_PKG"
        
        echo "Notarization complete."
    else
        echo "Error: Notarization failed!"
        echo "Check your credentials and try again."
        exit 1
    fi
fi

rm -f packages/$PKG_NAME
rm -rf payload/Library

echo ""
echo "✅ Build complete!"
echo "📦 Package: $FINAL_PKG"

if [ "$NOTARIZE" = true ]; then
    echo "🔐 Signed & Notarized: Yes"
else
    echo "🔐 Signed & Notarized: No (unsigned build)"
fi