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

# MinGW builds import the C++ runtime dynamically unless the toolchain is configured
# for static linkage. A package that omits these DLLs may start far enough to create a
# native window and then fail with a missing _ZSt21ios_base_library_initv entry point
# when Windows resolves an older system copy. Prefer the exact runtime next to the
# build, then the explicitly supplied toolchain root, then the host MinGW sysroot.
find_mingw_runtime() {
    local name="$1"
    local candidate
    local roots=(
        "$BUILD_BIN_DIR"
        "${MINGW_RUNTIME_ROOT:-}"
        "/usr/lib/gcc/x86_64-w64-mingw32/13-posix"
        "/usr/lib/gcc/x86_64-w64-mingw32/13-win32"
        "/usr/x86_64-w64-mingw32/lib"
    )
    for candidate in "${roots[@]}"; do
        if [[ -n "$candidate" && -f "$candidate/$name" ]]; then
            printf '%s\n' "$candidate/$name"
            return 0
        fi
    done
    return 1
}

# Only require MinGW runtime DLLs when the PE binaries actually import them.
# This keeps the script valid for static-linked Windows packages and non-Windows builds.
if [[ -f "$BUILD_BIN_DIR/Editor.exe" || -f "$BUILD_BIN_DIR/libUrho3D.dll" ]]; then
    for runtime in libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll; do
        if grep -aFq "$runtime" "$BUILD_BIN_DIR/Editor.exe" "$BUILD_BIN_DIR/libUrho3D.dll" 2>/dev/null; then
            runtime_path=$(find_mingw_runtime "$runtime" || true)
            if [[ -z "$runtime_path" ]]; then
                echo "package_runtime: imported MinGW runtime is missing: $runtime" >&2
                exit 5
            fi
            cp -f "$runtime_path" "$OUTPUT_DIR/bin/$runtime"
        fi
    done
fi

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

Windows MinGW packages also include every imported runtime DLL (`libstdc++-6.dll`, `libgcc_s_seh-1.dll`, and `libwinpthread-1.dll` when required by the PE binaries). Do not replace these files with older copies from another MinGW installation.

Do not distribute `Editor.exe` or `Player.exe` without these resource directories and imported runtime DLLs. A package containing only executables and DLLs can create a native window while rendering no usable editor interface or can fail before process startup with a missing C++ runtime entry point.
EOF

# Fail closed: a package without these directories is not runnable for the editor.
for required in Editor CoreData EditorData; do
    case "$required" in
        Editor) [[ -f "$OUTPUT_DIR/bin/Editor" || -f "$OUTPUT_DIR/bin/Editor.exe" ]] ;;
        *) [[ -d "$OUTPUT_DIR/bin/$required" ]] ;;
    esac
 done

# Fail closed for PE packages: every imported MinGW runtime must be beside the binary.
if [[ -f "$OUTPUT_DIR/bin/Editor.exe" || -f "$OUTPUT_DIR/bin/libUrho3D.dll" ]]; then
    for runtime in libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll; do
        if grep -aFq "$runtime" "$OUTPUT_DIR/bin/Editor.exe" "$OUTPUT_DIR/bin/libUrho3D.dll" 2>/dev/null \
            && [[ ! -f "$OUTPUT_DIR/bin/$runtime" ]]; then
            echo "package_runtime: package is incomplete; imported runtime is absent: $runtime" >&2
            exit 6
        fi
    done
fi

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
