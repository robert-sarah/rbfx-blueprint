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

} // namespace Urho3D::EditorThemeColors
