// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "EditorWorkspace.h"

namespace Urho3D
{

const ea::vector<EditorWorkspacePreset>& GetEditorWorkspacePresets()
{
    static const ea::vector<EditorWorkspacePreset> presets{
        {EditorWorkspace::Layout, "layout", "General project layout with scene, hierarchy, inspector and resources."},
        {EditorWorkspace::Scene2D, "scene2d", "2D scene authoring, hierarchy, inspector and tilemap-oriented layout."},
        {EditorWorkspace::Scene3D, "scene3d", "3D scene authoring, hierarchy, inspector, game preview and resources."},
        {EditorWorkspace::Blueprint, "blueprint", "Blueprint graph authoring, execution diagnostics and resource browser."},
        {EditorWorkspace::Scripting, "scripting", "rbscript editing, symbols, debugger, console and resources."},
        {EditorWorkspace::Animation, "animation", "Animation, scene preview, hierarchy, inspector and timeline tools."},
        {EditorWorkspace::Rendering, "rendering", "Materials, shaders, render paths, scene preview and profiling tools."},
        {EditorWorkspace::Audio, "audio", "Audio authoring, sound resources, mixer diagnostics and runtime preview."},
        {EditorWorkspace::Profiling, "profiling", "Runtime diagnostics, console, game preview and performance tools."},
        {EditorWorkspace::WorldFabric, "world_fabric", "Semantic dependency graph, impact analysis and production diagnostics."},
        {EditorWorkspace::Build, "build", "Asset cooking, packaging, launch configurations and build diagnostics."},
    };
    return presets;
}

const char* GetEditorWorkspaceId(EditorWorkspace workspace)
{
    for (const EditorWorkspacePreset& preset : GetEditorWorkspacePresets())
    {
        if (preset.id_ == workspace)
            return preset.name_;
    }
    return "layout";
}

const char* GetEditorWorkspaceName(EditorWorkspace workspace)
{
    switch (workspace)
    {
    case EditorWorkspace::Layout: return "Layout";
    case EditorWorkspace::Scene2D: return "2D Scene";
    case EditorWorkspace::Scene3D: return "3D Scene";
    case EditorWorkspace::Blueprint: return "Blueprint";
    case EditorWorkspace::Scripting: return "Scripting";
    case EditorWorkspace::Animation: return "Animation";
    case EditorWorkspace::Rendering: return "Rendering";
    case EditorWorkspace::Audio: return "Audio";
    case EditorWorkspace::Profiling: return "Profiling";
    case EditorWorkspace::WorldFabric: return "World Fabric";
    case EditorWorkspace::Build: return "Build";
    default: return "Layout";
    }
}

EditorWorkspace ParseEditorWorkspace(const ea::string& value)
{
    for (const EditorWorkspacePreset& preset : GetEditorWorkspacePresets())
    {
        if (value == preset.name_)
            return preset.id_;
    }
    return EditorWorkspace::Layout;
}

} // namespace Urho3D
