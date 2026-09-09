#!/bin/bash
################################################################################
# package-deb.sh - Build and package Tux Manager for Debian/Ubuntu
################################################################################

set -e

# Get script directory and source shared config
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config"

# Default values
QT_BIN_PATH=""
NCPUS=$(nproc)

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --qt)
            QT_BIN_PATH="$2"
            shift 2
            ;;
        --version)
            APP_VERSION="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--qt /path/to/qt/bin] [--version x.y.z]"
            exit 1
            ;;
    esac
done

# Setup Qt environment
if [ -n "$QT_BIN_PATH" ]; then
    export PATH="$QT_BIN_PATH:$PATH"
fi

# Build header (qmake resolved after dependency preflight)
echo "================================"
echo "Building Tux Manager for Debian"
echo "================================"
echo "CPUs: $NCPUS"
echo "Version: $APP_VERSION"
echo ""

# Preflight: check build dependencies and print install command if missing
qt6_ready=false
qt5_ready=false
if command -v dpkg-query >/dev/null 2>&1; then
    missing=()

    require_pkg() {
        local pkg="$1"
        if ! dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q "install ok installed"; then
            missing+=("$pkg")
        fi
    }

    require_pkg build-essential
    require_pkg debhelper
    require_pkg dpkg-dev
    require_pkg pkg-config

    if dpkg-query -W -f='${Status}\n' qt6-base-dev qt6-l10n-tools 2>/dev/null | grep -c "install ok installed" | grep -q '^2$'; then
        qt6_ready=true
    fi
    if dpkg-query -W -f='${Status}\n' qtbase5-dev qttools5-dev-tools 2>/dev/null | grep -c "install ok installed" | grep -q '^2$'; then
        qt5_ready=true
    fi

    if [ ${#missing[@]} -gt 0 ] || { [ "$qt6_ready" = false ] && [ "$qt5_ready" = false ]; }; then
        echo "Missing build dependencies detected."
        echo ""
        echo "Install the following packages (or equivalent) and re-run:"
        if command -v apt-get >/dev/null 2>&1; then
            if [ ${#missing[@]} -gt 0 ]; then
                echo "  sudo apt-get install ${missing[*]}"
            fi
            if [ "$qt6_ready" = false ] && [ "$qt5_ready" = false ]; then
                echo "  sudo apt-get install qt6-base-dev qt6-l10n-tools"
                echo "  or: sudo apt-get install qtbase5-dev qttools5-dev-tools"
            fi
        else
            for pkg in "${missing[@]}"; do
                echo "  - $pkg"
            done
            if [ "$qt6_ready" = false ] && [ "$qt5_ready" = false ]; then
                echo "  - Qt6: qt6-base-dev and qt6-l10n-tools"
                echo "  - Qt5: qtbase5-dev and qttools5-dev-tools"
            fi
        fi
        echo ""
        exit 1
    fi

fi

# Resolve qmake for a complete Qt base + Linguist toolchain.
if [ -z "$QT_BIN_PATH" ]; then
    if [ "$qt6_ready" = true ] && command -v qmake6 >/dev/null 2>&1; then
        QMAKE_CMD="qmake6"
    elif [ "$qt5_ready" = true ] && command -v qmake-qt5 >/dev/null 2>&1; then
        QMAKE_CMD="qmake-qt5"
    elif [ "$qt5_ready" = true ] && command -v qmake >/dev/null 2>&1; then
        QMAKE_CMD="qmake"
    elif command -v qmake6 >/dev/null 2>&1; then
        QMAKE_CMD="qmake6"
    elif command -v qmake >/dev/null 2>&1; then
        QMAKE_CMD="qmake"
    else
        echo "Error: qmake not found in PATH."
        echo "Install the Qt development packages noted above or use --qt to specify Qt bin path."
        exit 1
    fi
else
    if command -v qmake >/dev/null 2>&1; then
        QMAKE_CMD="qmake"
    else
        echo "Error: qmake not found in specified Qt path: $QT_BIN_PATH"
        exit 1
    fi
fi

echo "Qt command: $QMAKE_CMD ($(which $QMAKE_CMD | xargs dirname))"
echo ""

# Get project root directory (parent of packaging/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DEBIAN_DIR="$PROJECT_ROOT/debian"
CHANGELOG_PATH="$DEBIAN_DIR/changelog"

if [ ! -d "$DEBIAN_DIR" ]; then
    echo "Error: debian packaging metadata not found at $DEBIAN_DIR"
    exit 1
fi

DISTRO_TAG=""
if [ -r /etc/os-release ]; then
    . /etc/os-release
    if [ "${ID:-}" = "debian" ] && [ -n "${VERSION_ID:-}" ]; then
        DISTRO_TAG="deb${VERSION_ID}"
    elif [ "${ID:-}" = "ubuntu" ] && [ -n "${VERSION_ID:-}" ]; then
        DISTRO_TAG="ubuntu${VERSION_ID}"
    fi
fi

if [ -n "$DISTRO_TAG" ]; then
    DEB_VERSION="${APP_VERSION}-1~${DISTRO_TAG}"
else
    DEB_VERSION="${APP_VERSION}"
fi

BACKUP_CHANGELOG=""
if [ -f "$CHANGELOG_PATH" ]; then
    BACKUP_CHANGELOG=$(mktemp)
    cp "$CHANGELOG_PATH" "$BACKUP_CHANGELOG"
fi

trap 'if [ -n "$BACKUP_CHANGELOG" ] && [ -f "$BACKUP_CHANGELOG" ]; then cp "$BACKUP_CHANGELOG" "$CHANGELOG_PATH"; rm -f "$BACKUP_CHANGELOG"; fi' EXIT

cat > "$CHANGELOG_PATH" <<EOF
${APP_NAME} (${DEB_VERSION}) unstable; urgency=medium

  * Automated build.

 -- ${MAINTAINER}  $(date -R)
EOF

echo ""
echo "Step 1: Building package with debhelper..."
cd "$PROJECT_ROOT"
export QMAKE="$QMAKE_CMD"
dpkg-buildpackage -b -us -uc

echo ""
echo "Step 2: Collecting .deb output..."

OUTPUT_DIR="$PROJECT_ROOT/packaging/output"
OUTPUT_PARENT="$(cd "$PROJECT_ROOT/.." && pwd)"
mkdir -p "$OUTPUT_DIR"

DEB_FILES=()
collect_debs() {
    local base_dir="$1"
    local arch
    for arch in amd64 all; do
        [ -f "$base_dir/${APP_NAME}_${DEB_VERSION}_${arch}.deb" ] && DEB_FILES+=("$base_dir/${APP_NAME}_${DEB_VERSION}_${arch}.deb")
        [ -f "$base_dir/${APP_NAME}-dbgsym_${DEB_VERSION}_${arch}.deb" ] && DEB_FILES+=("$base_dir/${APP_NAME}-dbgsym_${DEB_VERSION}_${arch}.deb")
    done
    return 0
}

collect_debs "$OUTPUT_PARENT"
collect_debs "$PROJECT_ROOT"

if [ ${#DEB_FILES[@]} -eq 0 ]; then
    mapfile -t DEB_FILES < <(find "$OUTPUT_PARENT" -maxdepth 1 -type f -name "${APP_NAME}_${DEB_VERSION}_*.deb") || true
fi
if [ ${#DEB_FILES[@]} -eq 0 ]; then
    mapfile -t DEB_FILES < <(find "$PROJECT_ROOT" -maxdepth 1 -type f -name "${APP_NAME}_${DEB_VERSION}_*.deb") || true
fi

if [ ${#DEB_FILES[@]} -eq 0 ]; then
    echo "Error: No .deb artifacts found in $OUTPUT_PARENT or $PROJECT_ROOT"
    echo "Expected pattern: ${APP_NAME}_${DEB_VERSION}_*.deb"
    echo "Available .deb files in $OUTPUT_PARENT:"
    find "$OUTPUT_PARENT" -maxdepth 1 -type f -name "*.deb" -print
    exit 1
fi

for DEB_FILE in "${DEB_FILES[@]}"; do
    mv "$DEB_FILE" "$OUTPUT_DIR/"
done

echo ""
echo "================================"
echo "Build complete!"
echo "================================"
shopt -s nullglob
OUTPUT_DEBS=("$OUTPUT_DIR"/${APP_NAME}_${DEB_VERSION}_*.deb)
shopt -u nullglob

echo "Package(s):"
for DEB_FILE in "${OUTPUT_DEBS[@]}"; do
    echo "  $DEB_FILE"
done
echo ""
echo "To install:"
if [ ${#OUTPUT_DEBS[@]} -gt 0 ]; then
    echo "  sudo dpkg -i ${OUTPUT_DEBS[0]}"
fi
echo "  sudo apt-get install -f  # Install dependencies if needed"
echo ""
