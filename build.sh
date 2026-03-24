#!/bin/bash
set -e

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

productbuild \
  --distribution distribution.xml \
  --package-path packages \
  dist/$INSTALLER_NAME

rm -f packages/$PKG_NAME
rm -rf payload/Library

echo "Built dist/$INSTALLER_NAME"