// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "EditorDesignSystem.h"

#include <Urho3D/SystemUI/SystemUI.h>

namespace Urho3D
{

namespace
{

void SetCommonStyle(ImGuiStyle& style)
{
    style.WindowRounding = 4.0f;
    style.ChildRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 5.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.ItemSpacing = {6.0f, 5.0f};
    style.ItemInnerSpacing = {5.0f, 4.0f};
    style.WindowPadding = {8.0f, 8.0f};
    style.FramePadding = {7.0f, 4.0f};
    style.CellPadding = {5.0f, 4.0f};
    style.IndentSpacing = 16.0f;
    style.ScrollbarSize = 13.0f;
    style.GrabMinSize = 10.0f;
}

void SetDarkPalette(ImVec4* colors)
{
    colors[ImGuiCol_Text] = ImVec4(0.93f, 0.95f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.56f, 0.62f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.090f, 0.115f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.085f, 0.100f, 0.130f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.105f, 0.120f, 0.155f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.22f, 0.27f, 0.34f, 0.85f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.25f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.135f, 0.160f, 0.205f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.185f, 0.225f, 0.290f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.230f, 0.285f, 0.365f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.065f, 0.078f, 0.100f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.090f, 0.115f, 0.155f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.065f, 0.078f, 0.100f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.065f, 0.080f, 0.105f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.035f, 0.045f, 0.060f, 0.80f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.23f, 0.29f, 0.38f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.31f, 0.40f, 0.52f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.38f, 0.49f, 0.64f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.34f, 0.70f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.31f, 0.58f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.42f, 0.72f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.145f, 0.190f, 0.255f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.225f, 0.330f, 0.455f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.285f, 0.420f, 0.580f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.155f, 0.235f, 0.340f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.225f, 0.345f, 0.490f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.290f, 0.435f, 0.610f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.22f, 0.27f, 0.34f, 0.85f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.30f, 0.46f, 0.68f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.39f, 0.60f, 0.90f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.25f, 0.34f, 0.46f, 0.45f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.34f, 0.53f, 0.78f, 0.78f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.43f, 0.68f, 0.98f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.105f, 0.145f, 0.195f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.255f, 0.390f, 0.560f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.180f, 0.285f, 0.415f, 1.00f);
    colors[ImGuiCol_TabDimmed] = ImVec4(0.080f, 0.105f, 0.140f, 1.00f);
    colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.145f, 0.220f, 0.330f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.30f, 0.64f, 0.98f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.105f, 0.130f, 0.175f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.48f, 0.80f, 0.45f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.98f, 0.72f, 0.18f, 0.95f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.40f, 0.72f, 1.00f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.78f, 0.88f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.10f, 0.14f, 0.20f, 0.55f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.02f, 0.03f, 0.05f, 0.70f);
}

void SetLightPalette(ImVec4* colors)
{
    SetDarkPalette(colors);
    colors[ImGuiCol_Text] = ImVec4(0.10f, 0.13f, 0.18f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.42f, 0.47f, 0.55f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.93f, 0.95f, 0.98f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.89f, 0.92f, 0.96f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.98f, 0.99f, 1.00f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.65f, 0.70f, 0.78f, 0.85f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.83f, 0.87f, 0.93f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.75f, 0.82f, 0.91f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.68f, 0.77f, 0.88f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.78f, 0.83f, 0.91f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.70f, 0.78f, 0.89f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.82f, 0.86f, 0.93f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.80f, 0.85f, 0.92f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.78f, 0.82f, 0.88f, 0.80f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.53f, 0.62f, 0.74f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.43f, 0.54f, 0.68f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.35f, 0.47f, 0.62f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.72f, 0.80f, 0.90f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.57f, 0.70f, 0.86f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.45f, 0.61f, 0.80f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.68f, 0.79f, 0.91f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.57f, 0.71f, 0.87f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.47f, 0.64f, 0.82f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.76f, 0.82f, 0.90f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.52f, 0.67f, 0.85f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.64f, 0.75f, 0.88f, 1.00f);
    colors[ImGuiCol_TabDimmed] = ImVec4(0.82f, 0.86f, 0.92f, 1.00f);
    colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.72f, 0.80f, 0.90f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.20f, 0.47f, 0.82f, 0.55f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.84f, 0.88f, 0.94f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.30f, 0.55f, 0.85f, 0.35f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.25f, 0.35f, 0.35f);
}

void SetHighContrastPalette(ImVec4* colors)
{
    SetDarkPalette(colors);
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.72f, 0.76f, 0.82f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.025f, 0.030f, 0.040f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.035f, 0.045f, 0.060f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.62f, 0.72f, 0.88f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.15f, 0.21f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.32f, 0.48f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.34f, 0.50f, 0.72f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.14f, 0.24f, 0.38f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.45f, 0.70f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.36f, 0.62f, 0.92f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.34f, 0.54f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.52f, 0.80f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 0.86f, 0.20f, 1.00f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 0.36f, 0.36f, 1.00f);
}

} // namespace

void ApplyEditorStyle(EditorTheme theme)
{
    ImGuiStyle& style = ui::GetStyleTemplate();
    SetCommonStyle(style);

    switch (theme)
    {
    case EditorTheme::Light:
        SetLightPalette(style.Colors);
        break;
    case EditorTheme::HighContrast:
        SetHighContrastPalette(style.Colors);
        break;
    case EditorTheme::Dark:
    default:
        SetDarkPalette(style.Colors);
        break;
    }
}

const char* GetEditorThemeId(EditorTheme theme)
{
    switch (theme)
    {
    case EditorTheme::Light: return "light";
    case EditorTheme::HighContrast: return "high_contrast";
    case EditorTheme::Dark:
    default: return "dark";
    }
}

const char* GetEditorThemeName(EditorTheme theme)
{
    switch (theme)
    {
    case EditorTheme::Light: return "Light";
    case EditorTheme::HighContrast: return "High Contrast";
    case EditorTheme::Dark:
    default: return "Dark";
    }
}

EditorTheme ParseEditorTheme(const ea::string& value)
{
    if (value == "light")
        return EditorTheme::Light;
    if (value == "high_contrast")
        return EditorTheme::HighContrast;
    return EditorTheme::Dark;
}

} // namespace Urho3D
