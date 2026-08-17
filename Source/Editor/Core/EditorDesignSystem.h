// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <EASTL/string.h>

namespace Urho3D
{

enum class EditorTheme : unsigned char
{
    Dark,
    Light,
    HighContrast,
};

/// Apply the editor-wide visual language to the ImGui style template.
void ApplyEditorStyle(EditorTheme theme);

const char* GetEditorThemeId(EditorTheme theme);
const char* GetEditorThemeName(EditorTheme theme);
EditorTheme ParseEditorTheme(const ea::string& value);

} // namespace Urho3D
