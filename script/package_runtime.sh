#!/usr/bin/env bash
# Copyright (c) 2026 the rbfx-blueprint project.
# SPDX-License-Identifier: MIT
#
# Assemble a runnable runtime package. Unlike PackageBuilder, this script
# packages executables, runtime libraries, and engine/editor resources.

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_BIN_DIR=${1:-"$ROOT_DIR/bin"}
OUTPUT_DIR=${2:-"$ROOT_DIR/dist/rbfx-blueprint-runtime"}
RESOURCE_ROOT=${RBFX_RESOURCE_ROOT:-"$ROOT_DIR/bin"}

if [[ ! -d "$BUILD_BIN_DIR" ]]; then
    echo "package_runtime: build binary directory not found: $BUILD_BIN_DIR" >&2
    exit 2
fi

for required in CoreData EditorData; do
    if [[ ! -d "$RESOURCE_ROOT/$required" ]]; then
        echo "package_runtime: required resource directory is missing: $RESOURCE_ROOT/$required" >&2
        exit 3
    fi
done

rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR/bin" "$OUTPUT_DIR/build-info"

copy_first_existing() {
    local destination=$1
    shift
    local candidate
    for candidate in "$@"; do
        if [[ -f "$candidate" ]]; then
            cp -f "$candidate" "$destination/"
            return 0
        fi
    done
    return 1
}

copy_first_existing "$OUTPUT_DIR/bin" \
    "$BUILD_BIN_DIR/Editor" "$BUILD_BIN_DIR/Editor.exe" || {
    echo "package_runtime: Editor executable was not found in $BUILD_BIN_DIR" >&2
    exit 4
}

copy_first_existing "$OUTPUT_DIR/bin" \
    "$BUILD_BIN_DIR/Player" "$BUILD_BIN_DIR/Player.exe" || true

# Copy native runtime libraries produced beside the binaries.
find "$BUILD_BIN_DIR" -maxdepth 1 -type f \
    \( -iname '*.dll' -o -iname '*.so' -o -iname '*.so.*' -o -iname '*.dylib' \) \
    -exec cp -f {} "$OUTPUT_DIR/bin/" \;

# These directories are runtime contracts of EditorApplication and Player.
for resource in CoreData EditorData Data Autoload; do
    if [[ -d "$RESOURCE_ROOT/$resource" ]]; then
        cp -a "$RESOURCE_ROOT/$resource" "$OUTPUT_DIR/bin/"
    fi
done

cat > "$OUTPUT_DIR/run-editor.bat" <<'EOF'
@echo off
setlocal
cd /d "%~dp0bin"
if not exist "Editor.exe" (
  echo Editor.exe not found.
  exit /b 1
)
Editor.exe %*
EOF

cat > "$OUTPUT_DIR/run-player.bat" <<'EOF'
@echo off
setlocal
cd /d "%~dp0bin"
if not exist "Player.exe" (
  echo Player.exe not found.
  exit /b 1
)
Player.exe %*
EOF

cat > "$OUTPUT_DIR/README-DISTRIBUTION.md" <<'EOF'
# rbfx-blueprint runtime package

Run `run-editor.bat` on Windows to start the editor. The `bin` directory deliberately contains `CoreData` and `EditorData` beside the executable. They are required for resource mounting, editor fonts, UI definitions, and renderer shaders.

Do not distribute `Editor.exe` or `Player.exe` without these resource directories. A package containing only executables and DLLs can create a native window while rendering no usable editor interface.
EOF

# Fail closed: a package without these directories is not runnable for the editor.
for required in Editor CoreData EditorData; do
    case "$required" in
        Editor) [[ -f "$OUTPUT_DIR/bin/Editor" || -f "$OUTPUT_DIR/bin/Editor.exe" ]] ;;
        *) [[ -d "$OUTPUT_DIR/bin/$required" ]] ;;
    esac
 done

find "$OUTPUT_DIR" -type f -printf '%P\n' | sort > "$OUTPUT_DIR/build-info/FILE-LIST.txt"
(
    cd "$OUTPUT_DIR"
    find . -type f ! -path './build-info/SHA256SUMS.txt' -print0 \
        | sort -z \
        | xargs -0 sha256sum > build-info/SHA256SUMS.txt
)

printf 'Runtime package created at %s\n' "$OUTPUT_DIR"
printf 'Editor resources: CoreData=%s files, EditorData=%s files\n' \
    "$(find "$OUTPUT_DIR/bin/CoreData" -type f | wc -l)" \
    "$(find "$OUTPUT_DIR/bin/EditorData" -type f | wc -l)"
