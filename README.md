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

### 2. Build the Installer

Run the build script:

```bash
./build.sh
```

### 3. Find Your Installer

The installer will be created in the `dist/` folder:

```
dist/YourPlugin-Installer.pkg
```

## Requirements

- macOS with Xcode Command Line Tools installed

## Customization

### Change Plugin Version

Edit the version in `build.sh`:
```bash
--version 0.0.1 \
```

### Change Installer Title

Edit `distribution.xml`:
```xml
<title>Your Plugin Name</title>
```

### Change Package Identifier

Edit `build.sh`:
```bash
--identifier com.yourcompany.yourplugin \
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