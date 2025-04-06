#!/bin/sh
set -e

# Version detection using POSIX-compliant sed
VERSION=$(sed -n 's/#define VERSION "\(.*\)"/\1/p' src/version.h)
if [ -z "$VERSION" ]; then
    VERSION=$(date +%Y%m%d)
fi

ARCH=$(dpkg --print-architecture)

# Directories
BIN_DIR="bin"
DIST_DIR="dist"
FISH_FUNC_DIR="fish_functions"
FISH_CONF_DIR="fish_config"
TMP_DIR=$(mktemp -d)
DEB_DIR=$(mktemp -d)

# Cleanup trap
trap 'rm -rf "$TMP_DIR"' EXIT
trap 'rm -rf "$DEB_DIR"' EXIT

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

# Package function for POSIX sh
package_deb() {
    binary_name=$1
    output_file="${binary_name}_${VERSION}_${ARCH}.deb"

    echo "Creating debian package…"

    mkdir -p "${DEB_DIR}/DEBIAN"
    mkdir -p "${DEB_DIR}/usr/bin"
    mkdir -p "${DEB_DIR}/usr/share/fish/vendor_functions.d"
    mkdir -p "${DEB_DIR}/usr/share/fish/vendor_conf.d"
    mkdir -p "${DEB_DIR}/usr/share/doc/re-search"

    cp "debian/control"            "${DEB_DIR}/DEBIAN"
    cp "${BIN_DIR}/${binary_name}" "${DEB_DIR}/usr/bin/re-search"
    cp "README.md"                 "${DEB_DIR}/usr/share/doc/re-search/"
    cp "LICENSE"                   "${DEB_DIR}/usr/share/doc/re-search/"

    cp -R "${FISH_FUNC_DIR}/"* "${DEB_DIR}/usr/share/fish/vendor_functions.d/"
    cp -R "${FISH_CONF_DIR}/"* "${DEB_DIR}/usr/share/fish/vendor_conf.d/"

    sed -i "s/^Version: .*$/Version: ${VERSION}/" "${DEB_DIR}/DEBIAN/control"
    sed -i "s/^Architecture: .*$/Architecture: ${ARCH}/" "${DEB_DIR}/DEBIAN/control"

    mkdir -p "${DIST_DIR}"
    dpkg-deb --build ${DEB_DIR} "${DIST_DIR}/$output_file"

    echo "Created: $output_file"
}

# Package both versions
package_binary "re-search"
package_binary "re-search-static"
# Package only the dynamically linked one into a deb package
package_deb    "re-search"
