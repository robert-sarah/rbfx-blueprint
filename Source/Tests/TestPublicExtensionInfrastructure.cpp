// SPDX-License-Identifier: MIT

#include <Urho3D/IO/VectorBuffer.h>
#include <Urho3D/Plugins/PluginSDK.h>
#include <Urho3D/Resource/DistributedPackageRegistry.h>
#include <Urho3D/WorldFabric/WorldFabricRealtimeSession.h>
#include <Urho3D/RbScript/InteractiveDocumentation.h>
#include <Urho3D/RbScript/RbScriptLspService.h>

#include "CommonUtils.h"

#include <catch2/catch_amalgamated.hpp>

using namespace Urho3D;

TEST_CASE("Public plugin SDK validates ABI and activates dependencies", "[plugins][sdk]")
{
    const auto context = Tests::GetOrCreateContext(Tests::CreateCompleteContext);
    PluginSdk sdk(context.Get());
    ea::vector<ea::string> events;

    PluginSdkModule foundation;
    foundation.descriptor.name = "SDK.Foundation";
    foundation.descriptor.version = "1.0.0";
    foundation.minimumEngineVersion = {1, 0, 0};
    foundation.registration.registerReflection = [&events](Context*, ea::string*)
    {
        events.push_back("foundation");
        return true;
    };

    PluginSdkModule gameplay;
    gameplay.descriptor.name = "SDK.Gameplay";
    gameplay.descriptor.version = "1.2.0";
    gameplay.descriptor.dependencies = {foundation.descriptor.name};
    gameplay.minimumEngineVersion = {1, 0, 0};
    gameplay.registration.registerReflection = [&events](Context*, ea::string*)
    {
        events.push_back("gameplay");
        return true;
    };

    REQUIRE(sdk.RegisterModule(gameplay));
    REQUIRE(sdk.RegisterModule(foundation));
    ea::string error;
    REQUIRE(sdk.Validate({1, 3, 0}, 1, &error));
    REQUIRE(sdk.ActivateAll({1, 3, 0}, 1, &error));
    CHECK(events == ea::vector<ea::string>{"foundation", "gameplay"});
    CHECK(sdk.GetActiveModules().size() == 2);

    CHECK_FALSE(sdk.ActivateAll({1, 3, 0}, 2, &error));
    CHECK_FALSE(error.empty());
    sdk.DeactivateAll();
    REQUIRE(sdk.UnregisterModule(gameplay.descriptor.name));
    REQUIRE(sdk.UnregisterModule(foundation.descriptor.name));
}

TEST_CASE("Distributed package registry resolves and round-trips reproducibly", "[packages][registry]")
{
    const auto context = Tests::GetOrCreateContext(Tests::CreateCompleteContext);
    DistributedPackageRegistry source(context.Get());
    REQUIRE(source.AddRepository({"central", "https://packages.example/central", 10, false}));
    REQUIRE(source.AddRepository({"mirror", "https://packages.example/mirror", 5, true}));

    DistributedPackage package;
    package.name = "Studio.Runtime";
    package.version = "1.4.2";
    package.platform = PackagePlatform::Windows;
    package.architecture = "x86_64";
    package.digest = "sha256:runtime-142";
    package.size = 4096;
    package.artifact = "studio-runtime-1.4.2-win64.zip";
    package.dependencies = {"Studio.Core >=1.0.0"};
    REQUIRE(source.Publish(package));

    DistributedPackage newer = package;
    newer.version = "1.5.0";
    newer.digest = "sha256:runtime-150";
    newer.artifact = "studio-runtime-1.5.0-win64.zip";
    REQUIRE(source.Publish(newer));

    ea::string error;
    const DistributedPackage* resolved = source.Resolve({"Studio.Runtime", "^1.0.0", PackagePlatform::Windows, "x86_64"}, &error);
    REQUIRE(resolved);
    CHECK(resolved->version == "1.5.0");

    ea::vector<PackageReplicationTarget> plan;
    REQUIRE(source.BuildReplicationPlan(newer.digest, plan, &error));
    REQUIRE(plan.size() == 1);
    CHECK(plan.front().repositoryId == "mirror");

    const unsigned long long digest = source.ComputeDigest(&error);
    REQUIRE(digest != 0);
    const JSONValue json = source.ToJSON();
    DistributedPackageRegistry restored(context.Get());
    REQUIRE(restored.FromJSON(json, &error));
    CHECK(restored.ComputeDigest(&error) == digest);

    VectorBuffer buffer;
    buffer.SetName("Runtime.packageregistry");
    REQUIRE(source.Save(buffer));
    buffer.Seek(0);
    DistributedPackageRegistry loaded(context.Get());
    REQUIRE(loaded.BeginLoad(buffer));
    CHECK(loaded.Resolve({"Studio.Runtime", "1.4.2", PackagePlatform::Windows, "x86_64"}));

    CHECK(DistributedPackageRegistry::CheckExtension("Runtime.packageregistry"));
    CHECK(DistributedPackageRegistry::CheckExtension("Runtime.packages"));
    CHECK(DistributedPackageRegistry::CheckExtension("Runtime.packageindex"));
    CHECK_FALSE(DistributedPackageRegistry::CheckExtension("Runtime.json"));

    JSONValue invalid = json;
    invalid["packages"][0]["version"] = "release";
    REQUIRE_FALSE(restored.FromJSON(invalid, &error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("WorldFabric realtime sessions converge through ordered envelopes", "[worldfabric][collaboration][realtime]")
{
    const auto context = Tests::GetOrCreateContext(Tests::CreateCompleteContext);
    WorldFabricGraph graphA;
    WorldFabricGraph graphB;
    WorldFabricCollaboration collaborationA(&graphA);
    WorldFabricCollaboration collaborationB(&graphB);
    WorldFabricRealtimeSession sessionA(&collaborationA, "session-1");
    WorldFabricRealtimeSession sessionB(&collaborationB, "session-1");
    REQUIRE(sessionA.Join("alice", "Alice"));
    REQUIRE(sessionA.Join("bob", "Bob"));
    REQUIRE(sessionB.Join("alice", "Alice"));
    REQUIRE(sessionB.Join("bob", "Bob"));

    WorldFabricOperation operation;
    operation.clientId = "alice";
    operation.kind = WorldFabricOperationKind::AddNode;
    operation.key = "Runtime.Player";
    operation.type = "Player";
    operation.nodeKind = WorldFabricNodeKind::Entity;
    REQUIRE(sessionA.SubmitLocal(operation));
    REQUIRE(sessionA.GetPendingOutgoing().size() == 1);
    REQUIRE(sessionB.Receive(sessionA.GetPendingOutgoing().front()));
    REQUIRE(sessionB.ApplyIncoming());
    CHECK(sessionB.GetHistory().size() == 1);
    CHECK(sessionB.IsAcknowledged("alice", 1));
    CHECK(graphA.GetNodes().size() == 1);
    CHECK(graphB.GetNodes().size() == 1);
    CHECK(sessionA.GetLamportClock() > 0);
    (void)context;
}

TEST_CASE("rbscript LSP indexes documents and serves JSON-RPC tooling", "[rbscript][lsp]")
{
    RbScriptTypeRegistry registry;
    registry.RegisterType({RbScriptTypeKind::Int, "int"});
    registry.RegisterFunction({"spawn", {RbScriptTypeKind::Node, "Node"}, {}, false, true});
    RbScriptLspService lsp;
    lsp.SetTypeRegistry(&registry);
    const ea::string uri = "file:///project/Player.rbscript";
    const ea::string source = "module Game;\nscript Player : Component {\n    var health: int = 100;\n    fn tick(delta: float) { }\n}\n";
    REQUIRE(lsp.OpenDocument(uri, source));
    REQUIRE(lsp.GetDocument(uri));
    CHECK(lsp.GetDocument(uri)->version == 1);
    const RbScriptLspPosition playerPosition{1, 8};
    CHECK(lsp.Complete(uri, playerPosition).size() > 0);
    RbScriptLspLocation definition;
    CHECK(lsp.GoToDefinition(uri, playerPosition, definition));
    CHECK(definition.uri == uri);
    CHECK_FALSE(lsp.Rename(uri, playerPosition, "1Invalid").size());
    CHECK_FALSE(lsp.Hover(uri, playerPosition).empty());

    JSONValue open(JSON_OBJECT);
    open.Set("jsonrpc", "2.0");
    open.Set("id", 1u);
    open.Set("method", "textDocument/didOpen");
    JSONValue openParams(JSON_OBJECT);
    JSONValue textDocument(JSON_OBJECT);
    textDocument.Set("uri", uri + "-rpc");
    textDocument.Set("version", 1u);
    textDocument.Set("text", source);
    openParams.Set("textDocument", ea::move(textDocument));
    open.Set("params", ea::move(openParams));
    JSONValue response;
    REQUIRE(lsp.HandleJsonRpc(open, response));
    CHECK(response.Get("result").IsNull());

    JSONValue initialize(JSON_OBJECT);
    initialize.Set("jsonrpc", "2.0");
    initialize.Set("id", 2u);
    initialize.Set("method", "initialize");
    REQUIRE(lsp.HandleJsonRpc(initialize, response));
    CHECK(response.Get("result").Get("capabilities").Get("renameProvider").GetBool());
}

TEST_CASE("rbscript LSP resolves workspace definitions references and symbols", "[rbscript][lsp][workspace]")
{
    RbScriptLspService lsp;
    const ea::string declarationsUri = "file:///project/Combat.rbscript";
    const ea::string usageUri = "file:///project/Player.rbscript";
    const ea::string declarations = "module Combat;\nscript CombatMath : Component {\n    fn apply_damage(amount: int) { }\n}\n";
    const ea::string usage = "module Combat;\nscript Player : Component {\n    fn tick() { apply_damage(1); }\n}\n";
    REQUIRE(lsp.OpenDocument(declarationsUri, declarations));
    REQUIRE(lsp.OpenDocument(usageUri, usage));

    const RbScriptLspPosition usagePosition{2, 16};
    RbScriptLspLocation definition;
    REQUIRE(lsp.GoToDefinition(usageUri, usagePosition, definition));
    CHECK(definition.uri == declarationsUri);
    CHECK(definition.range.start.line == 2);
    CHECK(definition.range.start.character == 7);

    const ea::vector<RbScriptLspLocation> allReferences = lsp.FindReferences(usageUri, usagePosition);
    REQUIRE(allReferences.size() == 2);
    CHECK(allReferences.front().uri == declarationsUri);
    CHECK(allReferences.back().uri == usageUri);

    const ea::vector<RbScriptLspLocation> usagesOnly = lsp.FindReferences(usageUri, usagePosition, false);
    REQUIRE(usagesOnly.size() == 1);
    CHECK(usagesOnly.front().uri == usageUri);

    const ea::vector<RbScriptLspWorkspaceSymbol> symbols = lsp.WorkspaceSymbols("apply_damage");
    REQUIRE(symbols.size() == 1);
    CHECK(symbols.front().name == "apply_damage");
    CHECK(symbols.front().location.uri == declarationsUri);

    JSONValue initialize(JSON_OBJECT);
    initialize.Set("jsonrpc", "2.0");
    initialize.Set("id", 1u);
    initialize.Set("method", "initialize");
    JSONValue response;
    REQUIRE(lsp.HandleJsonRpc(initialize, response));
    CHECK(response.Get("result").Get("capabilities").Get("referencesProvider").GetBool());
    CHECK(response.Get("result").Get("capabilities").Get("workspaceSymbolProvider").GetBool());

    JSONValue references(JSON_OBJECT);
    references.Set("jsonrpc", "2.0");
    references.Set("id", 2u);
    references.Set("method", "textDocument/references");
    JSONValue referencesParams(JSON_OBJECT);
    JSONValue referenceDocument(JSON_OBJECT);
    referenceDocument.Set("uri", usageUri);
    referencesParams.Set("textDocument", ea::move(referenceDocument));
    JSONValue referencePosition(JSON_OBJECT);
    referencePosition.Set("line", usagePosition.line);
    referencePosition.Set("character", usagePosition.character);
    referencesParams.Set("position", ea::move(referencePosition));
    JSONValue referenceContext(JSON_OBJECT);
    referenceContext.Set("includeDeclaration", false);
    referencesParams.Set("context", ea::move(referenceContext));
    references.Set("params", ea::move(referencesParams));
    REQUIRE(lsp.HandleJsonRpc(references, response));
    REQUIRE(response.Get("result").IsArray());
    CHECK(response.Get("result").Size() == 1);

    JSONValue workspace(JSON_OBJECT);
    workspace.Set("jsonrpc", "2.0");
    workspace.Set("id", 3u);
    workspace.Set("method", "workspace/symbol");
    JSONValue workspaceParams(JSON_OBJECT);
    workspaceParams.Set("query", "apply_damage");
    workspace.Set("params", ea::move(workspaceParams));
    REQUIRE(lsp.HandleJsonRpc(workspace, response));
    REQUIRE(response.Get("result").IsArray());
    CHECK(response.Get("result").Size() == 1);
    CHECK(response.Get("result")[0].Get("location").Get("uri").GetString() == declarationsUri);
}

TEST_CASE("interactive documentation searches and round-trips", "[documentation][rbscript]")
{
    InteractiveDocumentation documentation;
    REQUIRE(documentation.AddPage({"world-fabric", "World Fabric", "Architecture",
        "Semantic dependency graph for production systems.", {"graph", "pipeline"}}));
    REQUIRE(documentation.AddSymbol({"AddNode", "worldfabric.AddNode", "method", "WorldFabricId AddNode(...)" ,
        "Adds a semantic node.", "cpp://WorldFabric", "1.0.0"}));
    REQUIRE(documentation.Search("semantic").size() == 2);
    CHECK(documentation.RenderMarkdown("world-fabric").find("World Fabric") != ea::string::npos);
    CHECK(documentation.RenderHtml("world-fabric").find("<!doctype html>") != ea::string::npos);
    const unsigned long long digest = documentation.ComputeDigest();
    REQUIRE(digest != 0);
    InteractiveDocumentation restored;
    ea::string error;
    REQUIRE(restored.FromJSON(documentation.ToJSON(), &error));
    CHECK(restored.ComputeDigest(&error) == digest);
    CHECK(restored.FindSymbol("worldfabric.AddNode"));
    CHECK_FALSE(restored.AddPage({"world-fabric", "Duplicate", "Architecture", "body", {}}, &error));
}
