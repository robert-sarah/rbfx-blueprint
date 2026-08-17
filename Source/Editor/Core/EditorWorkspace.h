// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <EASTL/string.h>
#include <EASTL/vector.h>

namespace Urho3D
{

enum class EditorWorkspace : unsigned char
{
    Layout,
    Scene2D,
    Scene3D,
    Blueprint,
    Scripting,
    Animation,
    Rendering,
    Audio,
    Profiling,
    WorldFabric,
    Build,
};

struct EditorWorkspacePreset
{
    EditorWorkspace id_{};
    const char* name_{};
    const char* description_{};
};

/// Return the built-in workspace presets in stable menu order.
const ea::vector<EditorWorkspacePreset>& GetEditorWorkspacePresets();

/// Return a stable identifier suitable for project settings and diagnostics.
const char* GetEditorWorkspaceId(EditorWorkspace workspace);

/// Return a human-readable workspace name.
const char* GetEditorWorkspaceName(EditorWorkspace workspace);

/// Parse a persisted workspace identifier. Unknown values fall back to Layout.
EditorWorkspace ParseEditorWorkspace(const ea::string& value);

} // namespace Urho3D
