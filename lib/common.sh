# Shared helpers for the build scripts. Sourced, never executed directly.

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PACKAGING_DIR="$REPO_ROOT/packaging"

# Apart, since Mac and Windows builds share bundle names; --plugin downloads to a temp folder instead
MAC_PLUGINS_DIR="${MAC_PLUGINS_DIR:-$PACKAGING_DIR/input/macOS}"
WINDOWS_PLUGINS_DIR="${WINDOWS_PLUGINS_DIR:-$PACKAGING_DIR/input/Windows}"

OUTPUT_DIR="${OUTPUT_DIR:-$PACKAGING_DIR/output}"

# Working folders, safe to delete
WORK_DIR="$PACKAGING_DIR/work"
PAYLOAD_DIR="$WORK_DIR/payload"
PACKAGES_DIR="$WORK_DIR/packages"
DMG_STAGING_DIR="$WORK_DIR/dmg-staging"
EXE_STAGING_DIR="$WORK_DIR/exe-staging"
INSTALLER_CACHE_DIR="$WORK_DIR/installer-cache"

# Plugins sit inside the payload at their real install paths
VST3_INSTALL_DIR="/Library/Audio/Plug-Ins/VST3"
COMPONENTS_INSTALL_DIR="/Library/Audio/Plug-Ins/Components"
PAYLOAD_VST3_DIR="$PAYLOAD_DIR$VST3_INSTALL_DIR"
PAYLOAD_COMPONENTS_DIR="$PAYLOAD_DIR$COMPONENTS_INSTALL_DIR"

DISTRIBUTION_TEMPLATE="$REPO_ROOT/pkg/distribution.xml"
DMG_PLUGINS_DIR=".plugins"  # the installer app looks for exactly this name beside itself

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

# Sets INSTALLER_TOOL, which runs the C++ packaging commands: the Mac app, or on Windows the
# installer exe. Exported, so later stages reuse it
find_installer_tool() {
    [ -z "${INSTALLER_TOOL:-}" ] || return 0

    case "$(uname -s)" in
        Darwin)
            find_installer_app
            INSTALLER_TOOL="$INSTALLER_APP/Contents/MacOS/$(plist_get "$INSTALLER_APP/Contents/Info.plist" CFBundleExecutable)"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            find_installer_exe
            INSTALLER_TOOL="$INSTALLER_EXE"
            ;;
        *)
            die "packaging runs on macOS, or Windows in Git Bash, not $(uname -s)"
            ;;
    esac

    export INSTALLER_TOOL
}

# Set INSTALLER_APP / INSTALLER_EXE: CI's installer, unless already set. The run is chosen out here,
# since fetch_installer runs in a subshell
find_installer_app() {
    [ -n "${INSTALLER_APP:-}" ] || { find_installer_run; INSTALLER_APP="$(fetch_installer macOS)" || exit 1; }
    [ -d "$INSTALLER_APP" ] || die "Installer app not found: $INSTALLER_APP"
    export INSTALLER_APP
}

find_installer_exe() {
    [ -n "${INSTALLER_EXE:-}" ] || { find_installer_run; INSTALLER_EXE="$(fetch_installer Windows)" || exit 1; }
    [ -f "$INSTALLER_EXE" ] || die "Installer exe not found: $INSTALLER_EXE"
    export INSTALLER_EXE
}

# Passes the command's stdout through; its stderr is the error message if it fails
installer_command() {
    local errors
    find_installer_tool
    { errors="$("$INSTALLER_TOOL" "$@" 2>&1 1>&3 3>&-)"; } 3>&1 || die "$errors"
}

# Sets PRODUCT_NAME, PLUGIN_NAME, VERSION and BUNDLE_ID from: info <bundle>..., or manifest <dir>,
# which also writes installer.json
read_info() {
    local output key value
    find_installer_tool  # here, as installer_command's subshell would lose it
    output="$(installer_command "$@")" || exit 1

    while IFS='=' read -r key value; do
        case "$key" in
            PRODUCT_NAME|PLUGIN_NAME|VERSION|BUNDLE_ID) printf -v "$key" '%s' "$value"; export "$key" ;;
        esac
    done <<< "$output"
}

read_payload_metadata() {
    read_info info "$PAYLOAD_VST3_DIR"/*.vst3 "$PAYLOAD_COMPONENTS_DIR"/*.component
}

# <output>/<Product-Name>-<version>-Installer-<unsigned|not-notarized>.<ext>; pass the signing
# identity, empty if unsigned. notarize.sh gives the plain name once stapled
artifact_path() {
    local suffix="-not-notarized"
    [ -n "$2" ] || suffix="-unsigned"
    [ "$1" != exe ] || suffix=""  # Never signed, so nothing to tell apart, and users see the name
    printf '%s/%s-%s-Installer%s.%s' "$OUTPUT_DIR" "${PRODUCT_NAME// /-}" "$VERSION" "$suffix" "$1"
}

require_gh() {
    command -v gh >/dev/null || die "gh is required for CI downloads: brew install gh, then gh auth login"
}

# gh for this repo, so it works from any folder
gh_repo() { (cd "$REPO_ROOT" && gh "$@"); }
gh_api() { gh_repo api "$@"; }

# CI uploads are bare .tar.gz files, not zipped
download_artifact() {
    local artifact="$1" dir="$2"
    rm -rf "$dir.partial"
    mkdir -p "$dir.partial"
    gh_api "repos/{owner}/{repo}/actions/artifacts/$artifact/zip" | tar -xzf - -C "$dir.partial" || return 1
    mv "$dir.partial" "$dir"
}

# Starts <workflow> on main and waits for it to pass; sets WORKFLOW_RUN. <title> picks out the new
# run, any if empty
run_workflow() {
    local workflow="$1" what="$2" title="$3" before run="" conclusion=""
    shift 3
    before="$(gh_api "repos/{owner}/{repo}/actions/workflows/$workflow/runs?per_page=1" --jq '.workflow_runs[0].id // 0')" ||
        die "couldn't list the $workflow runs (try: gh auth login)"
    gh_repo workflow run "$workflow" --ref main "$@" >&2 || die "couldn't start a CI build of $what"

    # GitHub takes a few seconds to list a new run; a failed check just tries again
    for _ in $(seq 20); do
        sleep 3
        run="$(gh_api "repos/{owner}/{repo}/actions/workflows/$workflow/runs?per_page=10" \
            --jq "[.workflow_runs[] | select(.id > $before and (\"$title\" == \"\" or .display_title == \"$title\"))][0].id // empty")" ||
            run=""
        [ -z "$run" ] || break
    done
    [ -n "$run" ] || die "started a CI build of $what but can't find it: gh run list --workflow $workflow"

    log "Building $what in CI run $run (takes a while)..."
    # The watch also stops on GitHub API errors, so it's retried and the run's own result decides
    for _ in $(seq 10); do
        gh_repo run watch "$run" --interval 15 >&2 || sleep 15
        conclusion="$(gh_api "repos/{owner}/{repo}/actions/runs/$run" --jq 'select(.status == "completed") | .conclusion')" ||
            conclusion=""
        [ -z "$conclusion" ] || break
    done
    [ "$conclusion" = success ] ||
        die "CI run $run ${conclusion:-never finished}: $(gh_api "repos/{owner}/{repo}/actions/runs/$run" --jq .html_url)"
    WORKFLOW_RUN="$run"
}

# Sets INSTALLER_RUN: the latest Installer CI run on main, or a new one if that predates the
# installer's last commit or its files have expired. Exported, so the whole build uses one run
find_installer_run() {
    [ -z "${INSTALLER_RUN:-}" ] || return 0
    local needed latest status run=""
    require_gh

    [ -z "$(git -C "$REPO_ROOT" status --porcelain -- installer/)" ] ||
        log "Note: CI's installer won't have your uncommitted changes to installer/. To try them, see INSTALLER_APP in the README."

    needed="$(git -C "$REPO_ROOT" log -1 --format=%H -- installer/ .github/workflows/installer.yml)"
    case "$(gh_api "repos/{owner}/{repo}/compare/$needed...main" --jq .status 2>/dev/null)" in
        identical|ahead) ;;
        *) die "CI builds the installer from main on GitHub, which doesn't have ${needed:0:12} yet. Push it, or see INSTALLER_APP in the README." ;;
    esac

    # GitHub's branch/status filters sometimes return nothing
    latest="$(gh_api "repos/{owner}/{repo}/actions/workflows/installer.yml/runs?per_page=100" \
        --jq '[.workflow_runs[] | select(.head_branch == "main" and .conclusion == "success")][0] // empty | "\(.id) \(.head_sha)"')" ||
        die "couldn't list the Installer CI runs (try: gh auth login)"

    if [ -n "$latest" ]; then
        status="$(gh_api "repos/{owner}/{repo}/compare/$needed...${latest#* }" --jq .status)" ||
            die "couldn't check Installer CI run ${latest% *} against ${needed:0:12} (try again)"
        case "$status" in
            identical|ahead) run="${latest% *}" ;;
            *) log "The last Installer CI run predates the installer's latest commit" ;;
        esac
    fi

    if [ -n "$run" ] && [ "$(gh_api "repos/{owner}/{repo}/actions/runs/$run/artifacts" --jq '[.artifacts[] | select(.expired | not)] | length')" = 0 ]; then
        log "Installer CI run $run's files have expired"
        run=""
    fi

    if [ -z "$run" ]; then
        run_workflow installer.yml "the installer" ""
        run="$WORKFLOW_RUN"
    fi

    # Keep only this run's downloads
    mkdir -p "$INSTALLER_CACHE_DIR"
    find "$INSTALLER_CACHE_DIR" -mindepth 1 -maxdepth 1 ! -name "$run" -exec rm -rf {} +

    INSTALLER_RUN="$run"
    export INSTALLER_RUN
}

# Prints the <platform> installer from INSTALLER_RUN, downloading it the first time
fetch_installer() {
    local platform="$1" ext=app dir artifact found
    [ "$platform" = macOS ] || ext=exe
    find_installer_run

    dir="$INSTALLER_CACHE_DIR/$INSTALLER_RUN/$platform"
    if [ ! -d "$dir" ]; then
        artifact="$(gh_api "repos/{owner}/{repo}/actions/runs/$INSTALLER_RUN/artifacts" \
            --jq ".artifacts[] | select(.name == \"Installer-$platform.tar.gz\" and (.expired | not)) | .id")"
        [ -n "$artifact" ] || die "Installer CI run $INSTALLER_RUN has no $platform installer"

        log "Downloading the $platform installer from Installer CI run $INSTALLER_RUN..."
        download_artifact "$artifact" "$dir" || die "couldn't download the $platform installer"
    fi

    # By type, so the name lives only in installer/CMakeLists.txt
    found=("$dir"/*."$ext")
    [ ${#found[@]} -eq 1 ] && [ -e "${found[0]}" ] || die "Installer CI run $INSTALLER_RUN's download has no single .$ext"
    echo "${found[0]}"
}

# Prints "<artifact> <run>": the newest CI build of <repo> <platform> <commit>
plugin_artifact() {
    gh_api "repos/{owner}/{repo}/actions/artifacts?name=$1-$2-${3:0:12}.tar.gz" \
        --jq '[.artifacts[] | select(.expired | not)][0] // empty | "\(.id) \(.workflow_run.id)"'
}

# Downloads <repo>'s main from CI for each platform, building it if needed
# Sets PLUGIN_COMMIT, MAC_PLUGINS_DIR, WINDOWS_PLUGINS_DIR and PLUGIN_DOWNLOAD_DIR
fetch_plugin() {
    local repo="$1" platform missing=() platforms found
    shift
    require_gh
    [[ "$repo" =~ ^[A-Za-z0-9._-]+$ ]] || die "not a repo name: $repo"

    PLUGIN_COMMIT="$(gh_api "repos/{owner}/$repo/commits/main" --jq .sha)" ||
        die "couldn't find $repo's main branch on GitHub"
    log "$repo main is at ${PLUGIN_COMMIT:0:12}"

    for platform in "$@"; do
        [ -n "$(plugin_artifact "$repo" "$platform" "$PLUGIN_COMMIT")" ] || missing+=("$platform")
    done

    if [ ${#missing[@]} -gt 0 ]; then
        platforms="${missing[0]}"
        [ ${#missing[@]} -eq 1 ] || platforms=both
        run_workflow plugin.yml "$repo (${missing[*]})" "Build $repo $PLUGIN_COMMIT" \
            -f repo="$repo" -f ref="$PLUGIN_COMMIT" -f platforms="$platforms"
    fi

    PLUGIN_DOWNLOAD_DIR="$(mktemp -d)"
    for platform in "$@"; do
        found="$(plugin_artifact "$repo" "$platform" "$PLUGIN_COMMIT")"
        [ -n "$found" ] || die "no $platform build of $repo ${PLUGIN_COMMIT:0:12} in CI"
        log "Downloading the $platform build from CI run ${found#* }..."
        download_artifact "${found% *}" "$PLUGIN_DOWNLOAD_DIR/$platform" ||
            die "couldn't download $repo's $platform build"
    done
    # Both, even if one wasn't fetched, so it never mixes with packaging/input/
    MAC_PLUGINS_DIR="$PLUGIN_DOWNLOAD_DIR/macOS"
    WINDOWS_PLUGINS_DIR="$PLUGIN_DOWNLOAD_DIR/Windows"
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
