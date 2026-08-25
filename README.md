# macOS Audio Plugin Installer Builder

Builds a `.pkg` installer or `.dmg` disk image for VST3 and Audio Unit plugins.

## Quick Start

```bash
cp -R YourPlugin.vst3 YourPlugin.component plugin-binaries/
./build.sh --pkg
```

Output lands in `dist/`.

## Build

```bash
./build.sh --pkg                # unsigned .pkg   (development)
./build.sh --dmg                # unsigned .dmg   (development)
./build.sh --pkg --notarize     # signed + notarized .pkg
./build.sh --dmg --notarize     # signed + notarized .dmg
./build.sh --help
```

`--pkg` or `--dmg` is required.

**No Developer ID Installer certificate? Use `--dmg`.** A notarized `.pkg` requires it and there is
no workaround. A `.dmg` is signed with the Developer ID *Application* certificate instead, and is
still notarized, stapled and warning-free.

Plugins install to `/Library/Audio/Plug-Ins/VST3/` and `/Library/Audio/Plug-Ins/Components/`.

The `.dmg` contains the plugins plus an `Install <name>.app` that copies them into place after asking
for an admin password.

## Signing Setup

```bash
cp .env.example .env      # fill in your details
source .env
./build.sh --dmg --notarize
```

```bash
# .env
export SIGNING_IDENTITY_APP="Developer ID Application: Your Name (TEAMID)"
export SIGNING_IDENTITY_INSTALLER="Developer ID Installer: Your Name (TEAMID)"   # --pkg only
export APPLE_ID="your@email.com"
export APPLE_TEAM_ID="TEAMID"
export APPLE_APP_PASSWORD="xxxx-xxxx-xxxx-xxxx"
```

Find your Team ID:

```bash
security find-identity -v -p codesigning
```

Get an app-specific password at [appleid.apple.com](https://appleid.apple.com) → Security. To keep it
out of your shell history and `ps` output, store it in the keychain instead:

```bash
xcrun notarytool store-credentials "AC_PASSWORD" \
  --apple-id "your@email.com" --team-id "TEAMID" --password "xxxx-xxxx-xxxx-xxxx"

export KEYCHAIN_PROFILE="AC_PASSWORD"
```

## Configuration

Set in `.env` — **required**, there are no defaults:

```bash
export PRODUCT_NAME="My Plugin"                # title, dmg volume, filenames
export BUNDLE_ID="com.yourcompany.myplugin"    # package identifier
export VERSION="0.0.1"                         # bump for each release
```

`--dmg` needs `PRODUCT_NAME` only; `--pkg` needs all three. The build stops and lists anything
missing, so a release can't go out under a placeholder identifier or a stale version.

These are substituted into `distribution.xml`, which is a template. Edit it directly to add a license
agreement, welcome screen, background image or extra install choices — just leave the
`${PRODUCT_NAME}` and `${BUNDLE_ID}` placeholders in place.


They read the same environment variables as `build.sh`. The header comment in each script lists which
ones it uses.

## Verify

```bash
codesign --verify --strict --verbose=4 dist/"My Plugin-Installer.dmg"
xcrun stapler validate dist/"My Plugin-Installer.dmg"
spctl --assess -v --type open --context context:primary-signature dist/"My Plugin-Installer.dmg"
spctl --assess -v --type install dist/"My Plugin-Installer-signed.pkg"
```

Expect `accepted` and `source=Notarized Developer ID`.

## Requirements

- Xcode Command Line Tools
- Apple Developer Program account (for signing and notarization)
