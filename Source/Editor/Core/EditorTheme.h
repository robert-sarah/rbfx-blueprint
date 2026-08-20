// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <imgui.h>

namespace Urho3D::EditorThemeColors
{

// Semantic editor colors. Feature tabs must use these roles rather than inventing
// near-duplicate IM_COL32 literals locally.
constexpr ImU32 Background = IM_COL32(23, 26, 31, 255);
constexpr ImU32 Grid = IM_COL32(38, 43, 51, 255);
constexpr ImU32 Panel = IM_COL32(38, 43, 51, 255);
constexpr ImU32 PanelAlt = IM_COL32(43, 48, 57, 255);
constexpr ImU32 Border = IM_COL32(110, 120, 135, 255);
constexpr ImU32 BorderMuted = IM_COL32(55, 61, 72, 255);
constexpr ImU32 Accent = IM_COL32(90, 155, 235, 255);
constexpr ImU32 AccentMuted = IM_COL32(70, 110, 165, 255);
constexpr ImU32 AccentHighlight = IM_COL32(100, 210, 255, 255);
constexpr ImU32 TextPrimary = IM_COL32(225, 228, 235, 255);
constexpr ImU32 TextMuted = IM_COL32(150, 155, 165, 200);
constexpr ImU32 Error = IM_COL32(235, 70, 65, 255);
constexpr ImU32 Warning = IM_COL32(245, 175, 60, 255);
constexpr ImU32 Success = IM_COL32(90, 205, 120, 255);
constexpr ImU32 Execution = IM_COL32(245, 205, 80, 255);
constexpr ImU32 Boolean = IM_COL32(90, 205, 120, 255);
constexpr ImU32 Numeric = IM_COL32(90, 155, 235, 255);
constexpr ImU32 String = IM_COL32(230, 150, 90, 255);
constexpr ImU32 Vector = IM_COL32(180, 110, 230, 255);
constexpr ImU32 DefaultData = IM_COL32(185, 185, 195, 255);
constexpr ImU32 Debug = IM_COL32(115, 78, 35, 255);
constexpr ImU32 Search = IM_COL32(255, 205, 70, 255);
constexpr ImU32 CableShadow = IM_COL32(0, 0, 0, 125);
constexpr ImU32 LinkPreview = IM_COL32(245, 205, 80, 220);
constexpr ImU32 CommentFill = IM_COL32(52, 64, 82, 255);
constexpr ImU32 CommentBorder = IM_COL32(190, 190, 205, 180);
constexpr ImU32 MinimapBackground = IM_COL32(18, 20, 24, 230);
constexpr ImU32 MinimapBorder = IM_COL32(105, 115, 130, 220);
constexpr ImU32 MinimapNode = IM_COL32(100, 105, 120, 255);
constexpr ImU32 PinValue = IM_COL32(170, 220, 175, 255);

constexpr float Spacing = 4.0f;
constexpr float CornerRadius = 3.0f;
constexpr float BorderWidth = 1.0f;
constexpr float IconSize = 16.0f;
constexpr float InspectorRowHeight = 28.0f;

// Stable text hierarchy shared by all editor tabs. These values are deliberately
// expressed as relative editor metrics rather than per-tab magic numbers.
constexpr float FontSizeTitle = 18.0f;
constexpr float FontSizeSection = 15.0f;
constexpr float FontSizeNode = 14.0f;
constexpr float FontSizeLabel = 13.0f;
constexpr float FontSizeCaption = 11.0f;
constexpr float FontSizeMono = 13.0f;

inline ImVec4 ToColor(ImU32 color)
{
    return ImGui::ColorConvertU32ToFloat4(color);
}

} // namespace Urho3D::EditorThemeColors

namespace Urho3D::EditorTheme
{

inline void PushPanelColors()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorThemeColors::ToColor(EditorThemeColors::Panel));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, EditorThemeColors::ToColor(EditorThemeColors::PanelAlt));
}

inline void PopPanelColors()
{
    ImGui::PopStyleColor(2);
}

inline void PushToolbarColors(bool accent = false)
{
    ImGui::PushStyleColor(ImGuiCol_Button, EditorThemeColors::ToColor(
        accent ? EditorThemeColors::AccentMuted : EditorThemeColors::PanelAlt));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorThemeColors::ToColor(EditorThemeColors::Accent));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorThemeColors::ToColor(EditorThemeColors::AccentHighlight));
}

inline void PopToolbarColors()
{
    ImGui::PopStyleColor(3);
}

inline void PushDiagnosticText(bool error)
{
    ImGui::PushStyleColor(ImGuiCol_Text, EditorThemeColors::ToColor(
        error ? EditorThemeColors::Error : EditorThemeColors::Warning));
}

inline void PopDiagnosticText()
{
    ImGui::PopStyleColor();
}

} // namespace Urho3D::EditorTheme
