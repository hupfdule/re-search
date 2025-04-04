#!/bin/sh
set -e

# Version detection using POSIX-compliant sed
VERSION=$(sed -n 's/#define VERSION "\(.*\)"/\1/p' src/version.h)
if [ -z "$VERSION" ]; then
    VERSION=$(date +%Y%m%d)
fi

# Directories
BIN_DIR="bin"
DIST_DIR="dist"
FISH_FUNC_DIR="fish_functions"
TMP_DIR=$(mktemp -d)

# Cleanup trap
trap 'rm -rf "$TMP_DIR"' EXIT

# Package function for POSIX sh
package_binary() {
    binary_name=$1
    output_file="${binary_name}-${VERSION}.tar.gz"

    echo "Packaging $binary_name..."

    mkdir -p "${TMP_DIR}/bin"
    mkdir -p "${TMP_DIR}/share/fish/vendor_functions.d"
    mkdir -p "${TMP_DIR}/share/doc/re-search"

    cp "${BIN_DIR}/${binary_name}" "${TMP_DIR}/bin/re-search"
    cp "README.md"                 "${TMP_DIR}/share/doc/re-search/"
    cp "LICENSE"                   "${TMP_DIR}/share/doc/re-search/"

    if [ -d "$FISH_FUNC_DIR" ]; then
        cp -R "${FISH_FUNC_DIR}/"* "${TMP_DIR}/share/fish/vendor_functions.d/"
    fi

    mkdir -p "${DIST_DIR}"
    tar czf "${DIST_DIR}/$output_file" -C "$TMP_DIR" bin share
    rm -rf "${TMP_DIR}/bin" "${TMP_DIR}/share"

    echo "Created: $output_file"
}

# Package both versions
package_binary "re-search"
package_binary "re-search-static"
