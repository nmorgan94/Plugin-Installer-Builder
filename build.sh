#!/bin/bash
set -e

rm -f dist/MyPlugin-Installer.pkg
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

pkgbuild \
  --root payload \
  --identifier com.example.pluginbundle \
  --version 0.0.1 \
  --install-location / \
  packages/PluginBundle.pkg

echo "Creating final installer..."

productbuild \
  --distribution distribution.xml \
  --package-path packages \
  dist/MyPlugin-Installer.pkg

rm -f packages/PluginBundle.pkg
rm -rf payload/Library

echo "Built dist/MyPlugin-Installer.pkg"