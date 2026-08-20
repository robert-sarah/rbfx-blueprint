// Copyright (c) 2026 rbfx-blueprint contributors
// SPDX-License-Identifier: MIT

#include <catch2/catch_amalgamated.hpp>

#include <algorithm>

#include <Editor/Core/EditorProductionValidation.h>

using namespace Urho3D;

TEST_CASE("Editor production reference scenarios validate independently", "[editor][production]")
{
    const std::vector<EditorProductionValidationInput> scenarios = GetEditorProductionReferenceScenarios();
    REQUIRE(scenarios.size() == 5);

    std::vector<std::uint64_t> digests;
    for (const EditorProductionValidationInput& input : scenarios)
    {
        const EditorProductionValidationReport report = ValidateEditorProduction(input);
        REQUIRE(report.passed);
        REQUIRE_FALSE(report.HasError());
        REQUIRE(report.Count(EditorValidationSeverity::Warning) == 0);
        digests.push_back(report.digest);
    }

    std::sort(digests.begin(), digests.end());
    REQUIRE(std::adjacent_find(digests.begin(), digests.end()) == digests.end());
}

TEST_CASE("Editor production validation rejects unsafe state", "[editor][production]")
{
    EditorProductionValidationInput input;
    input.workspaceId = "Scene2D";
    input.activeScene = true;
    input.sceneView2D = true;
    input.autosaveMaxSnapshots = 0;
    input.autosaveDirectoryWritable = false;
    input.assetDatabaseReady = false;

    const EditorProductionValidationReport report = ValidateEditorProduction(input);
    REQUIRE_FALSE(report.passed);
    REQUIRE(report.HasError());
    REQUIRE(report.Count(EditorValidationSeverity::Error) == 3);
    REQUIRE(report.digest != 0);
}

TEST_CASE("Editor production validation preserves warnings without hiding errors", "[editor][production]")
{
    EditorProductionValidationInput input;
    input.workspaceId = "CustomWorkspace";
    input.activeScene = true;
    input.sceneView3D = true;
    input.autosaveEnabled = false;
    input.recoveryManifestPresent = true;
    input.assetDatabaseReady = true;

    const EditorProductionValidationReport warningReport = ValidateEditorProduction(input);
    REQUIRE(warningReport.passed);
    REQUIRE_FALSE(warningReport.HasError());
    REQUIRE(warningReport.Count(EditorValidationSeverity::Warning) == 3);

    input.sceneView2D = true;
    const EditorProductionValidationReport errorReport = ValidateEditorProduction(input);
    REQUIRE_FALSE(errorReport.passed);
    REQUIRE(errorReport.Count(EditorValidationSeverity::Error) == 1);
    REQUIRE(errorReport.Count(EditorValidationSeverity::Warning) == 3);
}
