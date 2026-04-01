# macOS Audio Plugin Installer Builder

This project creates a macOS installer package (`.pkg`) for audio plugins (VST3 and Audio Unit formats).

## What It Does

The installer builder:
- ✅ Packages your VST3 and AU plugins into a single installer
- ✅ Automatically detects plugin names from your binaries
- ✅ Installs plugins to the correct system directories
- ✅ Creates a user-friendly installer with no customization needed

## Installation Locations

When users run the installer, plugins are installed to:
- **VST3**: `/Library/Audio/Plug-Ins/VST3/`
- **Audio Unit**: `/Library/Audio/Plug-Ins/Components/`

## How to Use

### 1. Prepare Your Plugins

Copy your compiled plugin binaries into the `plugin-binaries/` folder:

```
plugin-binaries/
├── YourPlugin.vst3/
└── YourPlugin.component/
```

**Supported formats:**
- `.vst3` (VST3 plugins)
- `.component` (Audio Unit plugins)

You can include one or both formats.

### Change Plugin Version

Edit the version in `build.sh`:
```bash
--version 0.0.1 \
```

### Change Installer Title

Edit `distribution.xml`

### 2. Build the Installer

#### Unsigned Build (Development)

For testing and development:

```bash
./build.sh
```

#### Signed and Notarized Build (Distribution)

For public distribution:

```bash
./build.sh --notarize
```

This will automatically sign AND notarize your installer for a professional, warning-free installation experience.

### 3. Find Your Installer

The installer will be created in the `dist/` folder:

```
dist/YourPlugin-Installer.pkg
```

## Requirements

- macOS with Xcode Command Line Tools installed
- For signing and notarization: Apple Developer Program Account

## Code Signing & Notarization

### Why Notarize?

| Build Type | Command | Use Case | User Experience |
|------------|---------|----------|-----------------|
| **Unsigned** | `./build.sh` | Development/testing | ⚠️ Gatekeeper blocks, requires manual override |
| **Signed + Notarized** | `./build.sh --notarize` | Public distribution | ✅ No warnings, installs smoothly |

### Setup

#### 1. Get Apple Developer Certificates

From [Apple Developer Portal](https://developer.apple.com/account):
- Download **Developer ID Application** certificate (for plugins)
- Download **Developer ID Installer** certificate (for packages)
- Install both by double-clicking them

Folow this [Documentation](https://developer.apple.com/help/account/certificates/create-developer-id-certificates) for help

#### 2. Set Environment Variables

```bash
# Required for --notarize
export SIGNING_IDENTITY_APP="Developer ID Application: Your Name (TEAMID)"
export SIGNING_IDENTITY_INSTALLER="Developer ID Installer: Your Name (TEAMID)"
export APPLE_ID="your@email.com"
export APPLE_TEAM_ID="TEAMID"
export APPLE_APP_PASSWORD="app-specific-password"
```

#### 3. Find Your Team ID

```bash
security find-identity
```

Look for your Developer ID certificates - the Team ID is in parentheses.

#### 4. Create App-Specific Password (for notarization)

1. Visit [appleid.apple.com](https://appleid.apple.com)
2. Go to Security → App-Specific Passwords
3. Generate a new password (name it "Notarization")
4. Store securely in Keychain:

```bash
xcrun notarytool store-credentials "AC_PASSWORD" \
  --apple-id "your@email.com" \
  --team-id "TEAMID" \
  --password "app-specific-password"

# Then reference it:
export APPLE_APP_PASSWORD="@keychain:AC_PASSWORD"
```

### Build Commands

```bash
# Development (unsigned)
./build.sh

# Distribution (signed and notarized)
./build.sh --notarize
```

### Verify Your Build

```bash
# Check plugin signature
codesign --verify --deep --verbose plugin-binaries/YourPlugin.vst3

# Check package signature
pkgutil --check-signature dist/YourPlugin-Installer-signed.pkg

# Check notarization status
spctl --assess --verbose --type install dist/YourPlugin-Installer-signed.pkg
```

## Project Structure

```
.
├── build.sh              # Main build script
├── distribution.xml      # Installer configuration
├── plugin-binaries/      # Place your plugins here
├── payload/              # Temporary staging (auto-generated)
├── packages/             # Temporary packages (auto-generated)
└── dist/                 # Final installer output (auto-generated)
```