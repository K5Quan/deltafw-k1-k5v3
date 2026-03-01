#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------
# Colors & Formatting
# ---------------------------------------------
BOLD="\e[1m"
RESET="\e[0m"
RED="\e[31m"
GREEN="\e[32m"
YELLOW="\e[33m"
BLUE="\e[34m"
CYAN="\e[36m"
WHITE="\e[37m"

BG_RED="\e[41m"
BG_GREEN="\e[42m"
BG_YELLOW="\e[43m"
BG_BLUE="\e[44m"
BG_CYAN="\e[46m"

# Inverted Badges
BADGE_INFO="${BOLD}${WHITE}${BG_BLUE} INFO ${RESET}"
BADGE_OK="${BOLD}${WHITE}${BG_GREEN}  OK  ${RESET}"
BADGE_WARN="${BOLD}${WHITE}${BG_YELLOW} WARN ${RESET}"
BADGE_ERR="${BOLD}${WHITE}${BG_RED} ERR  ${RESET}"
BADGE_BUILD="${BOLD}${WHITE}${BG_CYAN} BLD  ${RESET}"

# ---------------------------------------------
# Logging Functions
# ---------------------------------------------
log_header() {
    echo -e "\n  ⚡ ${BOLD}${CYAN}$1${RESET}"
    echo -e "  ══════════════════════════════════════════════════════════"
}

log_info() {
    echo -e "${BADGE_INFO} $1"
}

log_success() {
    echo -e "${BADGE_OK} $1"
}

log_warn() {
    echo -e "${BADGE_WARN} $1"
}

log_error() {
    echo -e "${BADGE_ERR} $1"
}

log_build() {
    echo -e "${BADGE_BUILD} $1"
}

print_logo() {
    echo -e "${CYAN}${BOLD}"
    echo "      _      _ _        __"
    echo "   __| | ___| | |_ __ _/ _|_      __"
    echo "  / _\` |/ _ \\ | __/ _\` | |_\\ \\ /\\ / /"
    echo " | (_| |  __/ | || (_| |  _|\\ V  V /"
    echo "  \\__,_|\\___|_|\\__\\__,_|_|   \\_/\\_/"
    echo "           meson tool suite"
    echo -e "${RESET}"
}

# ---------------------------------------------
# Usage:
#   ./compile-with-meson.sh [Preset] [Meson options...]
# ---------------------------------------------

IMAGE=uvk1-uvk5v3
PRESET=${1:-default}
shift || true

# Any remaining args will be treated as Meson options
EXTRA_ARGS=("$@")

# ---------------------------------------------
# Build the Docker image (ensure we have Meson)
# ---------------------------------------------
# ---------------------------------------------
# Build the Docker image (ensure we have Meson)
# ---------------------------------------------
if ! docker image inspect "$IMAGE" &> /dev/null; then
    log_build "Building Docker image..."
    docker build -t "$IMAGE" -f toolchain/Dockerfile . > /dev/null
fi

# ---------------------------------------------
# Configuration Helper
# ---------------------------------------------
get_config() {
    # Run config parser inside docker to ensure tomli is available
    docker run --rm -v "$PWD":/src -w /src "$IMAGE" python3 toolchain/config_parser.py config.toml "$@"
}

# ---------------------------------------------
# Main Build Function
# ---------------------------------------------
build_preset() {
    local preset="$1"
    
    # Check if preset exists in config
    presets_list=$(get_config list_presets)
    if [[ ! " $presets_list " =~ " $preset " ]]; then
        log_error "Unknown preset: '$preset'"
        return 1
    fi
    
    # Fetch Meson Args
    MESON_ARGS_STR=$(get_config get_preset_args "$preset")
    read -r -a MESON_ARGS <<< "$MESON_ARGS_STR"

    # Fetch Project Info
    eval $(get_config get_project_info)
    
    log_header "Building ${preset}..."
    start_time=$(date +%s)
    
    # Single build directory as requested
    BUILD_DIR="build"

    # Logic: Meson needs a clean directory or explicit reconfigure for different options.
    # We use a preset-specific subdirectory for the build system itself,
    # but we will copy the artifacts to the root build/ dir.
    local PRESET_BUILD_DIR="${BUILD_DIR}/${preset}"
    
    log_build "Configuring ${preset}..."
    docker run --rm -u $(id -u):$(id -g) -v "$PWD":/src -w /src "$IMAGE" \
        sh -c "rm -rf '${PRESET_BUILD_DIR}' && mkdir -p '${PRESET_BUILD_DIR}'"

    docker run --rm -u $(id -u):$(id -g) -v "$PWD":/src -w /src "$IMAGE" \
        meson setup "${PRESET_BUILD_DIR}" toolchain \
        --cross-file toolchain/cross_arm.ini \
        "${MESON_ARGS[@]}" \
        "${EXTRA_ARGS[@]}" 2>&1 | python3 toolchain/build_formatter.py

    # Calculate version info for filename
    # VERSION comes from config.toml (get_project_info)
    if command -v git &> /dev/null; then
        COMMIT=$(git rev-parse --short HEAD)
    else
        COMMIT="unknown"
    fi
    DATE=$(date +%d%m%Y)
    LOWER_PRESET=$(echo "$preset" | tr '[:upper:]' '[:lower:]')
    BASENAME="deltafw.${LOWER_PRESET}.v${VERSION}.${COMMIT}.${DATE}"
    FILENAME_BIN="${BASENAME}.bin"
    FILENAME_HEX="${BASENAME}.hex"
    FILENAME_PACKED="${BASENAME}.packed.bin"
    FILENAME_QSH="${BASENAME}.qsh"

    # Compile
    log_build "Compiling ${preset}..."
    
    if docker run --rm -u $(id -u):$(id -g) -v "$PWD":/src -w /src "$IMAGE" \
        meson compile -C "${PRESET_BUILD_DIR}" 2>&1 | python3 toolchain/build_formatter.py; then
        
        # Copy artifacts to build folder
        log_build "Finalizing artifacts..."
        if [ -f "${PRESET_BUILD_DIR}/deltafw.bin" ]; then
            cp "${PRESET_BUILD_DIR}/deltafw.bin" "$BUILD_DIR/$FILENAME_BIN"
            [ -f "${PRESET_BUILD_DIR}/deltafw.hex" ] && cp "${PRESET_BUILD_DIR}/deltafw.hex" "$BUILD_DIR/$FILENAME_HEX"
            [ -f "${PRESET_BUILD_DIR}/deltafw_packed.bin" ] && cp "${PRESET_BUILD_DIR}/deltafw_packed.bin" "$BUILD_DIR/$FILENAME_PACKED"
            
            # Pack into QSH Container
            FULL_GIT_MSG=$(git log -1 --pretty="%B" 2>/dev/null | tr -d '"' | tr '\n' ' ' || echo "")
            python3 toolchain/qsh_packer.py pack \
                "$BUILD_DIR/$FILENAME_QSH" \
                --type firmware \
                --fw-bin "${PRESET_BUILD_DIR}/deltafw.bin" \
                --title "deltafw" \
                --author "qshosfw" \
                --desc "Preset: $preset | $FULL_GIT_MSG" \
                --fw-ver "$VERSION" \
                --fw-target "uvk1,uv-k5v3" \
                --fw-git "$COMMIT" \
                --fw-arch "arm" \
                --fw-license "GPL-3.0" \
                --fw-page-size 256 \
                --fw-base-addr 0x08000000 \
                --aux-file "${PRESET_BUILD_DIR}/deltafw.elf" \
                --aux-type "elf"

            end_time=$(date +%s)
            duration=$((end_time - start_time))
            log_success "Built ${preset} in ${duration}s"
            echo -e "${BADGE_OK} Firmware: ${BOLD}${CYAN}${BUILD_DIR}/${FILENAME_BIN}${RESET}"
            echo -e "${BADGE_OK} Container: ${BOLD}${CYAN}${BUILD_DIR}/${FILENAME_QSH}${RESET}"
        else
            log_error "Artifact not found!"
            return 1
        fi
    else
        log_error "Compilation failed for ${preset}!"
        return 1
    fi
    return 0
}

print_logo
log_info "Target Preset: ${BOLD}${PRESET}${RESET}"

BUILD_DIR="build"
mkdir -p "$BUILD_DIR"
chmod 777 "$BUILD_DIR"

if [[ "$PRESET" == "All" ]]; then
    # Dynamic list from config
    PRESETS_STR=$(get_config list_presets)
    read -r -a PRESETS <<< "$PRESETS_STR"
    SUCCESS_COUNT=0
    TOTAL_COUNT=0
    for p in "${PRESETS[@]}"; do
        ((TOTAL_COUNT++))
        if build_preset "$p"; then
            ((SUCCESS_COUNT++))
        fi
    done
    log_info "Build Summary: ${SUCCESS_COUNT}/${TOTAL_COUNT} presets succeeded."
    if [[ $SUCCESS_COUNT -eq 0 ]]; then
        log_error "All presets failed to build!"
        exit 1
    fi
else
    if ! build_preset "$PRESET"; then
        exit 1
    fi
fi
