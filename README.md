# Plugin Installer Builder

Turns VST3 and Audio Unit plugin builds into installers for macOS and Windows, from a Mac; the
Windows `.exe` can also be built on Windows, in Git Bash. The `.dmg` and `.exe` hold the plugins and
a JUCE installer app (`installer/`) that installs or removes them, asking for admin rights when
needed. A `.pkg` is also supported.

## Use

```bash
cp -R YourPlugin.vst3 YourPlugin.component packaging/input/macOS/   # empty these first between plugins
cp -R YourPlugin.vst3 packaging/input/Windows/                      # the Windows build
./build.sh --dmg                                                    # unsigned, for testing
source .env && ./build.sh --dmg --notarize                          # signed and notarised, to ship
./build.sh --exe                                                    # Windows installer (unsigned)
```

Output goes to `packaging/output/<Product-Name>-<version>-Installer.<dmg|exe>`. A Mac build gets that plain
name only once notarised and stapled; otherwise it ends in `-unsigned` or `-not-notarized`. The .exe
always has the plain name. The name, version and
bundle ID are read from the plugin bundles, so change them in the plugin's CMake.

The installer app comes from this repo's latest successful Installer CI run on `main`, unless you
point the build at your own (see Settings). If `installer/` has changed since that run, or its files
have expired (after 30 days), `build.sh` starts a new run and waits for it. Push installer changes
first: CI builds what's on GitHub. Every build needs the app, `--pkg` included: its commands read the
plugin bundles and write the `.exe`. For debugging, `<app>/Contents/MacOS/<name> --help` lists them.

```bash
./build.sh --pkg [--notarize]   # .pkg; notarising needs a Developer ID Installer certificate
./build.sh --help               # all options, and how to run each stage on its own
```

## Settings

Unsigned `--dmg`, `--pkg` and `--exe` builds need no settings. Everything else is below.

**For `--notarize`**, in `.env` (copy `.env.example`, uncomment what you use, then `source .env`):

| Setting | Needed for | Example |
|---|---|---|
| `SIGNING_IDENTITY_APP` | `--dmg --notarize`, `--pkg --notarize` | `Developer ID Application: Acme Audio Ltd (AB12CD34EF)` |
| `SIGNING_IDENTITY_INSTALLER` | `--pkg --notarize` only | `Developer ID Installer: Acme Audio Ltd (AB12CD34EF)` |
| `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD` | notarisation login, option 1 | `dev@acme-audio.com`, `AB12CD34EF`, `abcd-efgh-ijkl-mnop` |
| `KEYCHAIN_PROFILE` | notarisation login, option 2 (instead of option 1) | `acme-notary` |

**Optional overrides**, set in front of a single command rather than in `.env`, so they don't stick:

| Setting | Use with | What it does |
|---|---|---|
| `INSTALLER_APP` | any build on a Mac | Uses this installer app instead of CI's, e.g. your own build of `installer/` |
| `INSTALLER_EXE` | `--exe` | The same for Windows |

```bash
INSTALLER_APP=installer/build-release/PluginInstaller_artefacts/Release/Acme-Installer.app ./build.sh --dmg
```

`INSTALLER_BRAND_DIR` isn't one of these: it's a CMake option for building the installer app, see
Rebranding.

## Verify a release

```bash
xcrun stapler validate packaging/output/<file>.dmg
spctl -a -vv -t open --context context:primary-signature packaging/output/<file>.dmg
```

Expect `accepted` and `source=Notarized Developer ID`.

## Rebranding

The installer app's look comes from a brand folder built into it, `installer/brand/` by default.
Everything is optional; anything missing falls back to a neutral dark look.

| File | Used for | Without it |
|---|---|---|
| `brand.json` | company name, colours, fonts | neutral defaults |
| `logo.svg` | window logo, and the app icon if there's no `icon.svg`; give it `width="1024" height="1024"` | vendor name only |
| `icon.svg` | app icon: a 1024 canvas with an 824 rounded square in the middle, as macOS expects | `logo.svg`, else the OS default |
| `fonts/` | the font files `brand.json` names (variable fonts work) | system font |

`brand.json` keys:

- `company`: names the app `<company>-Installer`, ID `com.<company>.Installer` (default `Plugin`)
- `colours`, each `#rrggbb`: `background`, `panel` (left panel), `line`, `text`, `mutedText`,
  `accent` (buttons, tick boxes), `onAccent` (text and ticks on the accent, default white),
  `accentText` (category, links), `highlight` ("Update from")
- `fonts`: `body` and `heading` file names in `fonts/`, plus `bodyBold` if `body` isn't a variable font

To use your own brand, put it in another folder and point the build at it, so `installer/brand/`
stays untouched. For CI, add the same variable to `cacheVariables` in `installer/CMakePresets.json`.

```bash
cd installer
cmake --preset release -DINSTALLER_BRAND_DIR=/path/to/your-brand
cmake --build --preset release
```


## Requirements

- Xcode Command Line Tools and `gh`, signed in (`gh auth login`)
- To build the `.exe` on Windows instead: Git Bash and `gh`, signed in
- CMake 3.25+, only to build `installer/` yourself
- Universal (`x86_64` + `arm64`) plugin binaries; the build refuses anything else
- A Developer ID Application certificate, for `--notarize`
- For `--dmg`, Python 3.10+ (`brew install python`; Apple's is too old). The first build installs
  `dmgbuild`, which lays out the DMG window, into `dmg/.venv/`
