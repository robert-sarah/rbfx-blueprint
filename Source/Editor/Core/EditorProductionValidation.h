// Copyright (c) 2026 rbfx-blueprint contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Urho3D
{

enum class EditorValidationSeverity : unsigned char
{
    Warning,
    Error
};

struct EditorValidationIssue
{
    EditorValidationSeverity severity{EditorValidationSeverity::Error};
    std::string code;
    std::string message;
};

/// Input contract shared by editor UI, CI smoke tests and project diagnostics.
struct EditorProductionValidationInput
{
    std::string workspaceId{"Layout"};
    bool activeScene{};
    bool sceneView2D{};
    bool sceneView3D{};
    bool autosaveEnabled{true};
    bool autosaveDirectoryWritable{true};
    unsigned autosaveMaxSnapshots{10};
    bool recoveryManifestPresent{};
    bool blueprintGraphReady{};
    bool rbscriptWorkspaceReady{};
    bool assetDatabaseReady{};
};

struct EditorProductionValidationReport
{
    bool passed{};
    std::vector<EditorValidationIssue> issues;
    std::uint64_t digest{};

    unsigned Count(EditorValidationSeverity severity) const;
    bool HasError() const;
};

/// Validate editor state before opening a production workspace or running a smoke test.
EditorProductionValidationReport ValidateEditorProduction(
    const EditorProductionValidationInput& input);

/// Return stable reference scenarios used by editor smoke tests and documentation.
std::vector<EditorProductionValidationInput> GetEditorProductionReferenceScenarios();

} // namespace Urho3D
