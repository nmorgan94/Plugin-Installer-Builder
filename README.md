# Plugin Installer Builder

Turns VST3 and Audio Unit plugin builds into installers for macOS and Windows, all from a Mac. The
`.dmg` and `.exe` hold the plugins and a JUCE installer app (`installer/`) that installs or removes
them, asking for admin rights when needed. A `.pkg` is also supported.

## Use

```bash
cp -R YourPlugin.vst3 YourPlugin.component plugin-binaries/macOS/   # empty these first between plugins
cp -R YourPlugin.vst3 plugin-binaries/Windows/                      # the Windows build
./build.sh --dmg                                                    # unsigned, for testing
source .env && ./build.sh --dmg --notarize                          # signed and notarised, to ship
./build.sh --exe                                                    # Windows installer (unsigned)
```

Output goes to `dist/<Product-Name>-<version>-Installer.<dmg|exe>`. Only a notarised, stapled build
gets that plain name; otherwise it ends in `-unsigned` or `-not-notarized`. The name, version and
bundle ID are read from the plugin bundles, so change them in the plugin's CMake.

The installer app comes from this repo's latest successful Installer CI run on `main`
(`gh workflow run installer.yml`). To use your own build instead, set `INSTALLER_APP` (Mac) or
`INSTALLER_EXE` (Windows) to its path.

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

- Xcode Command Line Tools, `jq` (included with macOS 15+) and `gh`, signed in (`gh auth login`)
- CMake 3.25+, only to build `installer/` yourself
- Universal (`x86_64` + `arm64`) plugin binaries; the build refuses anything else
- A Developer ID Application certificate, for `--notarize`
