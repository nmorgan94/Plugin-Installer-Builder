# Plugin Installer Builder

Turns VST3 and Audio Unit plugin builds into a signed, notarised macOS installer. The `.dmg` holds
the plugins and a JUCE installer app (`installer/`) that installs or removes them with one admin
password. A `.pkg` is also supported.

## Use

```bash
cp -R YourPlugin.vst3 YourPlugin.component plugin-binaries/   # empty it first between plugins
./build.sh --dmg                                              # unsigned, for testing
source .env && ./build.sh --dmg --notarize                    # signed and notarised, to ship
```

Output goes to `dist/<Product-Name>-<version>-Installer.dmg`. Only a notarised, stapled build gets
that plain name; otherwise it ends in `-unsigned` or `-not-notarized`. The name, version and bundle
ID are read from the plugin bundles, so change them in the plugin's CMake.

```bash
./build.sh --pkg [--notarize]   # .pkg; notarising needs a Developer ID Installer certificate
./build.sh --help               # all options, and how to run each stage on its own
```

## Setup

```bash
cp .env.example .env   # then fill it in; the comments explain each value
```

## Verify a release

```bash
xcrun stapler validate dist/<file>.dmg
spctl -a -vv -t open --context context:primary-signature dist/<file>.dmg
```

Expect `accepted` and `source=Notarized Developer ID`.

## Requirements

- Xcode Command Line Tools, CMake 3.25+ and `jq` (included with macOS 15+)
- Universal (`x86_64` + `arm64`) plugin binaries; the build refuses anything else
- A Developer ID Application certificate, for `--notarize`
