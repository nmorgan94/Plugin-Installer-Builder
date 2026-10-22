# Shared helpers for the build scripts. Sourced, never executed directly.

log() { echo "$@" >&2; }

die() { echo "Error: $*" >&2; exit 1; }

require_env() {
    local missing=() var
    for var in "$@"; do
        [ -n "${!var:-}" ] || missing+=("$var")
    done
    if [ ${#missing[@]} -gt 0 ]; then
        die "missing environment variable(s): ${missing[*]}
Set them in .env (see .env.example), then: source .env"
    fi
}

notary_auth() {
    if [ -n "${KEYCHAIN_PROFILE:-}" ]; then
        AUTH=(--keychain-profile "$KEYCHAIN_PROFILE")
        return
    fi

    require_env APPLE_ID APPLE_TEAM_ID APPLE_APP_PASSWORD
    case "$APPLE_APP_PASSWORD" in
        @keychain:*)
            die "APPLE_APP_PASSWORD is altool syntax that notarytool does not understand.
Instead use: export KEYCHAIN_PROFILE='${APPLE_APP_PASSWORD#@keychain:}'"
            ;;
    esac
    AUTH=(--apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_PASSWORD")
}

notary_submit() {
    local file="$1" output status id
    log "Submitting $(basename "$file") for notarization (may take several minutes)..."
    output="$(xcrun notarytool submit "$file" "${AUTH[@]}" --wait 2>&1)" || true
    echo "$output" >&2

    status="$(echo "$output" | sed -n 's/^ *status: \(.*\)$/\1/p' | tail -1)"
    if [ "$status" != "Accepted" ]; then
        id="$(echo "$output" | sed -n 's/^ *id: \([0-9a-fA-F-]*\).*/\1/p' | head -1)"
        [ -n "$id" ] && xcrun notarytool log "$id" "${AUTH[@]}" >&2 || true
        die "notarization failed (status: ${status:-unknown})"
    fi
}

plist_get() { plutil -extract "$2" raw -o - "$1" 2>/dev/null || true; }

require_jq() {
    command -v jq >/dev/null || die "jq is required: it comes with macOS 15+, or run: brew install jq"
}

# An empty value matches anything
check_same() {
    local field="$1" first="$2" first_value="$3" bundle="$4" value="$5"
    [ -z "$first_value" ] || [ -z "$value" ] || [ "$first_value" = "$value" ] ||
        die "$field mismatch: $bundle $value, $first $first_value"
}

# Sets PRODUCT_NAME, VERSION, BUNDLE_ID, CATEGORY, COPYRIGHT and PLUGIN_* from the bundles; stops if they disagree
read_plugin_metadata() {
    require_jq
    local bundle base file first="" sep=$'\x1f'
    local name vendor version id category url email copyright plist_version au_name exe archs
    PLUGIN_NAME="" PLUGIN_VENDOR="" VERSION="" BUNDLE_ID="" CATEGORY=""
    PLUGIN_URL="" PLUGIN_EMAIL="" COPYRIGHT=""

    for bundle in "$@"; do
        [ -d "$bundle" ] || continue
        base="$(basename "$bundle")"
        name="" vendor="" version="" id="" category="" url="" email="" copyright=""

        file="$bundle/Contents/Resources/moduleinfo.json"
        if [ -f "$file" ]; then
            # Steinberg's format allows trailing commas, which jq rejects
            IFS="$sep" read -r name version vendor url email category < <(
                perl -0pe 's/,(\s*[}\]])/$1/g' "$file" | jq -r --arg sep "$sep" '[
                    .Name, .Version, ."Factory Info".Vendor, ."Factory Info".URL, ."Factory Info"."E-Mail",
                    first(.Classes[] | select(.Category == "Audio Module Class") | ."Sub Categories"[0])
                ] | map(. // "") | join($sep)'
            ) || die "can't read $file"
        fi

        file="$bundle/Contents/Info.plist"
        if [ -f "$file" ]; then
            plist_version="$(plist_get "$file" CFBundleShortVersionString)"
            [ -z "$version" ] || [ "$version" = "$plist_version" ] ||
                die "$base: Info.plist says $plist_version but moduleinfo.json says $version"
            version="$plist_version"
            id="$(plist_get "$file" CFBundleIdentifier)"
            [ -n "$id" ] || die "$base: Info.plist has no CFBundleIdentifier"
            copyright="$(plist_get "$file" NSHumanReadableCopyright)"

            # Audio Units are named "Vendor: Name"
            au_name="$(plist_get "$file" AudioComponents.0.name)"
            if [ -n "$au_name" ]; then
                vendor="${vendor:-${au_name%%: *}}"
                name="${name:-${au_name#*: }}"
                case "$(plist_get "$file" AudioComponents.0.type)" in
                    aumu) category="${category:-Instrument}" ;;
                    *)    category="${category:-Fx}" ;;
                esac
            fi

            exe="$(plist_get "$file" CFBundleExecutable)"
            archs="$(lipo -archs "$bundle/Contents/MacOS/$exe" 2>/dev/null || true)"
            [[ " $archs " == *" x86_64 "* && " $archs " == *" arm64 "* ]] ||
                die "$base is not universal (built for: ${archs:-unknown})"
        fi

        [ -n "$version" ] || die "$base: no version found (no moduleinfo.json or Info.plist)"

        first="${first:-$base}"
        check_same version "$first" "$VERSION" "$base" "$version"
        check_same name "$first" "$PLUGIN_NAME" "$base" "$name"
        check_same vendor "$first" "$PLUGIN_VENDOR" "$base" "$vendor"
        check_same "bundle ID" "$first" "$BUNDLE_ID" "$base" "$id"
        VERSION="${VERSION:-$version}" BUNDLE_ID="${BUNDLE_ID:-$id}"
        PLUGIN_NAME="${PLUGIN_NAME:-$name}" PLUGIN_VENDOR="${PLUGIN_VENDOR:-$vendor}"
        CATEGORY="${CATEGORY:-$category}" PLUGIN_URL="${PLUGIN_URL:-$url}"
        PLUGIN_EMAIL="${PLUGIN_EMAIL:-$email}" COPYRIGHT="${COPYRIGHT:-$copyright}"
    done

    [ -n "$first" ] || die "No plugin bundles found"
    [ -n "$PLUGIN_NAME" ] && [ -n "$PLUGIN_VENDOR" ] || die "couldn't find the plugin's name and vendor"

    case "$PLUGIN_NAME" in
        "$PLUGIN_VENDOR"*) PRODUCT_NAME="$PLUGIN_NAME" ;;
        *)                 PRODUCT_NAME="$PLUGIN_VENDOR $PLUGIN_NAME" ;;
    esac
    # Characters Finder or Windows can't show in a file name
    case "$PRODUCT_NAME" in
        *[\\/:*?\"\<\>\|]*) die "product name '$PRODUCT_NAME' can't be used in a file name" ;;
    esac

    export PLUGIN_NAME PLUGIN_VENDOR PRODUCT_NAME VERSION BUNDLE_ID CATEGORY \
        PLUGIN_URL PLUGIN_EMAIL COPYRIGHT
}

read_payload_metadata() {
    read_plugin_metadata "$PAYLOAD_VST3_DIR"/*.vst3 "$PAYLOAD_COMPONENTS_DIR"/*.component
}

# installer.json tells the installer app what it's installing; uses read_plugin_metadata's values
write_manifest() {
    require_jq
    local dir="$1" bundle names=()

    for bundle in "$dir"/*.vst3 "$dir"/*.component; do
        [ -d "$bundle" ] || continue
        names+=("$(basename "$bundle")")
    done

    [ ${#names[@]} -gt 0 ] || die "No plugin bundles in $dir"

    jq -n --arg product "$PRODUCT_NAME" --arg vendor "$PLUGIN_VENDOR" \
        --arg version "$VERSION" --arg bundleId "$BUNDLE_ID" --arg category "$CATEGORY" \
        --arg url "$PLUGIN_URL" --arg email "$PLUGIN_EMAIL" --arg copyright "$COPYRIGHT" \
        '{schema: 1, product: $product, vendor: $vendor, version: $version, bundleId: $bundleId,
          category: $category, url: $url, email: $email, copyright: $copyright,
          bundles: [$ARGS.positional[] | {name: ., format: (if endswith(".vst3") then "VST3" else "AU" end)}]}' \
        --args "${names[@]}" > "$dir/installer.json"
}

# dist/<Product-Name>-<version>-Installer-<unsigned|not-notarized>.<ext>; pass the signing identity,
# empty if unsigned. Only notarize.sh gives a build the plain release name, once it's stapled
artifact_path() {
    local suffix="-not-notarized"
    [ -n "$2" ] || suffix="-unsigned"
    printf '%s/%s-%s-Installer%s.%s' "$DIST_DIR" "${PRODUCT_NAME// /-}" "$VERSION" "$suffix" "$1"
}

# The flags notarisation requires
sign_code() {
    codesign --force --sign "$1" --options runtime --timestamp "$2" >&2
}

xml_escape() {
    local s="$1"
    s="${s//&/&amp;}"
    s="${s//</&lt;}"
    s="${s//>/&gt;}"
    printf '%s' "$s"
}

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DISTRIBUTION_TEMPLATE="$REPO_ROOT/pkg/distribution.xml"

BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/plugin-binaries}"
PAYLOAD_DIR="${PAYLOAD_DIR:-$REPO_ROOT/payload}"
PACKAGES_DIR="${PACKAGES_DIR:-$REPO_ROOT/packages}"
DIST_DIR="${DIST_DIR:-$REPO_ROOT/dist}"
DMG_STAGING_DIR="${DMG_STAGING_DIR:-$REPO_ROOT/dmg-staging}"
DMG_PLUGINS_DIR=".plugins"  # the installer app looks for exactly this name beside itself
INSTALLER_DIR="$REPO_ROOT/installer"

# Plugins sit inside the payload at their real install paths.
VST3_INSTALL_DIR="/Library/Audio/Plug-Ins/VST3"
COMPONENTS_INSTALL_DIR="/Library/Audio/Plug-Ins/Components"
PAYLOAD_VST3_DIR="$PAYLOAD_DIR$VST3_INSTALL_DIR"
PAYLOAD_COMPONENTS_DIR="$PAYLOAD_DIR$COMPONENTS_INSTALL_DIR"
