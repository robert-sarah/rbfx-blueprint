// Copyright (c) 2026 rbfx-blueprint contributors
// SPDX-License-Identifier: MIT

#include "EditorProductionValidation.h"

#include <algorithm>

namespace Urho3D
{

namespace
{

void HashByte(std::uint64_t& hash, unsigned char value)
{
    hash ^= value;
    hash *= 1099511628211ull;
}

void HashString(std::uint64_t& hash, const std::string& value)
{
    for (unsigned char byte : value)
        HashByte(hash, byte);
    HashByte(hash, 0);
}

void HashBool(std::uint64_t& hash, bool value)
{
    HashByte(hash, value ? 1 : 0);
}

void AddIssue(EditorProductionValidationReport& report, EditorValidationSeverity severity,
    const char* code, const char* message)
{
    report.issues.push_back({severity, code, message});
}

} // namespace

unsigned EditorProductionValidationReport::Count(EditorValidationSeverity severity) const
{
    return static_cast<unsigned>(std::count_if(issues.begin(), issues.end(),
        [severity](const EditorValidationIssue& issue) { return issue.severity == severity; }));
}

bool EditorProductionValidationReport::HasError() const
{
    return Count(EditorValidationSeverity::Error) != 0;
}

EditorProductionValidationReport ValidateEditorProduction(const EditorProductionValidationInput& input)
{
    EditorProductionValidationReport report;
    report.passed = true;
    report.digest = 1469598103934665603ull;

    HashString(report.digest, input.workspaceId);
    HashBool(report.digest, input.activeScene);
    HashBool(report.digest, input.sceneView2D);
    HashBool(report.digest, input.sceneView3D);
    HashBool(report.digest, input.autosaveEnabled);
    HashBool(report.digest, input.autosaveDirectoryWritable);
    HashByte(report.digest, static_cast<unsigned char>(input.autosaveMaxSnapshots & 0xffu));
    HashBool(report.digest, input.recoveryManifestPresent);
    HashBool(report.digest, input.blueprintGraphReady);
    HashBool(report.digest, input.rbscriptWorkspaceReady);
    HashBool(report.digest, input.assetDatabaseReady);

    if (input.workspaceId.empty())
    {
        AddIssue(report, EditorValidationSeverity::Error, "workspace.empty", "Workspace identifier must not be empty.");
    }
    else if (input.workspaceId != "Layout" && input.workspaceId != "Scene2D" && input.workspaceId != "Scene3D"
        && input.workspaceId != "Blueprint" && input.workspaceId != "RbScript" && input.workspaceId != "ShaderGraph")
    {
        AddIssue(report, EditorValidationSeverity::Warning, "workspace.unknown",
            "Unknown workspace falls back to the default layout.");
    }

    if (!input.activeScene)
        AddIssue(report, EditorValidationSeverity::Error, "scene.missing", "A production editor session requires an active scene.");

    if (input.sceneView2D == input.sceneView3D)
    {
        AddIssue(report, EditorValidationSeverity::Error, "scene.view-mode",
            "Exactly one scene view profile must be active: 2D or 3D.");
    }

    if (input.autosaveEnabled)
    {
        if (!input.autosaveDirectoryWritable)
            AddIssue(report, EditorValidationSeverity::Error, "autosave.directory",
                "Autosave is enabled but its directory is not writable.");
        if (input.autosaveMaxSnapshots == 0 || input.autosaveMaxSnapshots > 100)
            AddIssue(report, EditorValidationSeverity::Error, "autosave.retention",
                "Autosave retention must be between 1 and 100 snapshots.");
    }
    else
    {
        AddIssue(report, EditorValidationSeverity::Warning, "autosave.disabled",
            "Autosave is disabled for this editor session.");
    }

    if (input.recoveryManifestPresent)
    {
        AddIssue(report, EditorValidationSeverity::Warning, "recovery.pending",
            "A recovery manifest is present and should be reviewed before editing.");
    }

    if ((input.workspaceId == "Blueprint" || input.workspaceId == "Layout") && !input.blueprintGraphReady)
    {
        AddIssue(report, EditorValidationSeverity::Error, "blueprint.unavailable",
            "The Blueprint graph service is not ready for this workspace.");
    }
    if ((input.workspaceId == "RbScript" || input.workspaceId == "Layout") && !input.rbscriptWorkspaceReady)
    {
        AddIssue(report, EditorValidationSeverity::Error, "rbscript.unavailable",
            "The RbScript workspace service is not ready for this workspace.");
    }
    if (!input.assetDatabaseReady)
    {
        AddIssue(report, EditorValidationSeverity::Error, "assets.unavailable",
            "The asset database is not ready for production editing.");
    }

    report.passed = !report.HasError();
    return report;
}

std::vector<EditorProductionValidationInput> GetEditorProductionReferenceScenarios()
{
    std::vector<EditorProductionValidationInput> scenarios;

    EditorProductionValidationInput scene2D;
    scene2D.workspaceId = "Scene2D";
    scene2D.activeScene = true;
    scene2D.sceneView2D = true;
    scene2D.assetDatabaseReady = true;
    scenarios.push_back(scene2D);

    EditorProductionValidationInput scene3D;
    scene3D.workspaceId = "Scene3D";
    scene3D.activeScene = true;
    scene3D.sceneView3D = true;
    scene3D.assetDatabaseReady = true;
    scenarios.push_back(scene3D);

    EditorProductionValidationInput blueprint;
    blueprint.workspaceId = "Blueprint";
    blueprint.activeScene = true;
    blueprint.sceneView3D = true;
    blueprint.blueprintGraphReady = true;
    blueprint.assetDatabaseReady = true;
    scenarios.push_back(blueprint);

    EditorProductionValidationInput rbscript;
    rbscript.workspaceId = "RbScript";
    rbscript.activeScene = true;
    rbscript.sceneView3D = true;
    rbscript.rbscriptWorkspaceReady = true;
    rbscript.assetDatabaseReady = true;
    scenarios.push_back(rbscript);

    EditorProductionValidationInput shaderGraph;
    shaderGraph.workspaceId = "ShaderGraph";
    shaderGraph.activeScene = true;
    shaderGraph.sceneView3D = true;
    shaderGraph.assetDatabaseReady = true;
    scenarios.push_back(shaderGraph);

    return scenarios;
}

} // namespace Urho3D
