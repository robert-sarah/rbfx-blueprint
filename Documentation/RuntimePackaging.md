# Runtime packaging

A runnable rbfx-blueprint desktop distribution is more than the native executable and its shared libraries. The editor and player require the engine resource directories to be mounted beside the runtime binaries.

## Required layout

The minimum editor layout is:

```text
package/
├── run-editor.bat
└── bin/
    ├── Editor.exe
    ├── libUrho3D.dll
    ├── CoreData/
    └── EditorData/
```

`CoreData` contains renderer resources and engine assets. `EditorData` contains editor fonts and UI definitions. `EditorApplication` configures `EP_RESOURCE_PATHS` as `CoreData;EditorData`, and the runtime resolves these directories from the program directory or a detected parent prefix.

For a package that also launches game projects, include `Player.exe`, `Data/`, and `Autoload/` when those resources are used. Native runtime DLLs must be copied beside the executable on Windows.

## Canonical assembly command

The repository helper assembles a runnable package and fails closed when `CoreData` or `EditorData` is absent:

```bash
./script/package_runtime.sh <build-bin-dir> <output-dir>
```

For example:

```bash
./script/package_runtime.sh build/bin/Release dist/rbfx-blueprint-windows-x64
```

Set `RBFX_RESOURCE_ROOT` when the resources are stored outside the repository root:

```bash
RBFX_RESOURCE_ROOT=/path/to/bin \
  ./script/package_runtime.sh /path/to/build/bin dist/rbfx-blueprint-runtime
```

The helper copies the runtime binaries and libraries, adds `CoreData`, `EditorData`, `Data`, and `Autoload` when available, creates Windows launch scripts, writes a file inventory, and records SHA-256 hashes. It deliberately refuses to create an editor package when either of the two mandatory editor resource directories is missing.

## Diagnosing a black window

A window titled `Editor | OpenGL` with a completely black client area usually means that the native window was created but the editor resource contract was not satisfied. Run the executable from a terminal to retain the startup log:

```bat
Editor.exe > editor-output.log 2>&1
```

The log should report mounted `CoreData` and `EditorData`, loaded Noto and Font Awesome fonts, and the ImGui shader. The message `No resource directories or packages were mounted`, or missing-font and missing-shader errors, indicates an incomplete distribution rather than a gameplay-scene problem.

If the required directories are present and the log shows a valid OpenGL context, investigate the native GPU driver, OpenGL version, shader compilation and the graphics backend next. A real Windows graphical smoke test remains required because Linux cross-compilation cannot certify the Windows display driver.
