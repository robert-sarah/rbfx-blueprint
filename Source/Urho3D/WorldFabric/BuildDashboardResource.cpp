// SPDX-License-Identifier: MIT

#include "../Precompiled.h"

#include "BuildDashboardResource.h"

#include "../IO/Deserializer.h"
#include "../IO/Serializer.h"
#include "../Resource/JSONFile.h"

namespace Urho3D
{

namespace
{

const BuildTaskExecutor validationExecutor = [](const BuildTask&, const ea::vector<BuildTaskResult>&,
    BuildTaskResult&, ea::string&) { return true; };

} // namespace

BuildDashboardResource::BuildDashboardResource(Context* context)
    : Resource(context)
{
}

bool BuildDashboardResource::CheckExtension(const ea::string& fileName)
{
    return fileName.ends_with(".builddashboard", false) || fileName.ends_with(".buildgraph", false);
}

const char* BuildDashboardResource::GetBuildTaskKindName(BuildTaskKind kind)
{
    switch (kind)
    {
    case BuildTaskKind::ImportAsset: return "ImportAsset";
    case BuildTaskKind::CompileShader: return "CompileShader";
    case BuildTaskKind::CompileScript: return "CompileScript";
    case BuildTaskKind::CookVFX: return "CookVFX";
    case BuildTaskKind::BuildPackage: return "BuildPackage";
    case BuildTaskKind::Custom: return "Custom";
    default: return "Custom";
    }
}

bool BuildDashboardResource::ParseBuildTaskKind(const ea::string& name, BuildTaskKind& kind)
{
    if (name == "ImportAsset") kind = BuildTaskKind::ImportAsset;
    else if (name == "CompileShader") kind = BuildTaskKind::CompileShader;
    else if (name == "CompileScript") kind = BuildTaskKind::CompileScript;
    else if (name == "CookVFX") kind = BuildTaskKind::CookVFX;
    else if (name == "BuildPackage") kind = BuildTaskKind::BuildPackage;
    else if (name == "Custom") kind = BuildTaskKind::Custom;
    else return false;
    return true;
}

JSONValue BuildDashboardResource::ToJSON() const
{
    JSONValue root(JSON_OBJECT);
    root.Set("version", 1u);
    root.Set("platform", platform_);
    root.Set("configuration", configuration_);

    JSONValue tasks(JSON_ARRAY);
    for (const BuildTask& task : tasks_)
    {
        JSONValue taskJson(JSON_OBJECT);
        taskJson.Set("key", task.key);
        taskJson.Set("kind", GetBuildTaskKindName(task.kind));

        JSONValue metadata(JSON_OBJECT);
        metadata.SetStringVariantMap(task.metadata, context_);
        taskJson.Set("metadata", ea::move(metadata));

        JSONValue dependencies(JSON_ARRAY);
        for (const ea::string& dependency : task.dependencies)
            dependencies.Push(dependency);
        taskJson.Set("dependencies", ea::move(dependencies));
        tasks.Push(ea::move(taskJson));
    }
    root.Set("tasks", ea::move(tasks));
    return root;
}

bool BuildDashboardResource::FromJSON(const JSONValue& root, ea::string* error)
{
    if (!root.IsObject())
    {
        if (error)
            *error = "Build Dashboard root must be a JSON object";
        return false;
    }

    const JSONValue& version = root.Get("version");
    const JSONValue& platform = root.Get("platform");
    const JSONValue& configuration = root.Get("configuration");
    const JSONValue& tasks = root.Get("tasks");
    if (!version.IsNumber() || version.GetUInt() != 1u || !platform.IsString()
        || !configuration.IsString() || !tasks.IsArray())
    {
        if (error)
            *error = "Build Dashboard requires version 1, platform, configuration and tasks";
        return false;
    }
    if (platform.GetString().empty() || configuration.GetString().empty())
    {
        if (error)
            *error = "Build Dashboard platform and configuration must not be empty";
        return false;
    }

    ea::vector<BuildTask> candidateTasks;
    candidateTasks.reserve(tasks.Size());
    BuildGraph graph;
    for (const JSONValue& taskJson : tasks.GetArray())
    {
        if (!taskJson.IsObject() || !taskJson.Get("key").IsString() || !taskJson.Get("kind").IsString()
            || !taskJson.Get("metadata").IsObject() || !taskJson.Get("dependencies").IsArray())
        {
            if (error)
                *error = "Build Dashboard contains an invalid task";
            return false;
        }

        BuildTask task;
        task.key = taskJson.Get("key").GetString();
        if (task.key.empty() || !ParseBuildTaskKind(taskJson.Get("kind").GetString(), task.kind))
        {
            if (error)
                *error = "Build Dashboard task key or kind is invalid";
            return false;
        }
        task.metadata = taskJson.Get("metadata").GetStringVariantMap();
        for (const JSONValue& dependency : taskJson.Get("dependencies").GetArray())
        {
            if (!dependency.IsString() || dependency.GetString().empty())
            {
                if (error)
                    *error = Format("Build task '{}' contains an invalid dependency", task.key);
                return false;
            }
            task.dependencies.push_back(dependency.GetString());
        }

        if (!graph.AddTask(task, validationExecutor))
        {
            if (error)
                *error = Format("Invalid Build task '{}': {}", task.key, graph.GetLastError());
            return false;
        }
        candidateTasks.push_back(ea::move(task));
    }

    for (const BuildTask& task : candidateTasks)
    {
        for (const ea::string& dependency : task.dependencies)
        {
            if (!graph.AddDependency(task.key, dependency))
            {
                if (error)
                    *error = Format("Invalid dependency for '{}': {}", task.key, graph.GetLastError());
                return false;
            }
        }
    }

    ea::string graphError;
    if (!graph.Validate(&graphError))
    {
        if (error)
            *error = graphError;
        return false;
    }

    platform_ = platform.GetString();
    configuration_ = configuration.GetString();
    tasks_ = ea::move(candidateTasks);
    return true;
}

BuildGraph BuildDashboardResource::CreateValidationGraph() const
{
    BuildGraph graph;
    for (const BuildTask& task : tasks_)
        graph.AddTask(task, validationExecutor);
    for (const BuildTask& task : tasks_)
    {
        for (const ea::string& dependency : task.dependencies)
            graph.AddDependency(task.key, dependency);
    }
    return graph;
}

bool BuildDashboardResource::Validate(ea::string* error) const
{
    BuildGraph graph = CreateValidationGraph();
    return graph.Validate(error);
}

ea::vector<ea::string> BuildDashboardResource::GetBuildOrder(ea::string* error) const
{
    BuildGraph graph = CreateValidationGraph();
    return graph.GetBuildOrder(error);
}

unsigned long long BuildDashboardResource::ComputeDigest(ea::string* error) const
{
    BuildGraph graph = CreateValidationGraph();
    if (!graph.Validate(error))
        return 0;
    return graph.ComputeDigest();
}

void BuildDashboardResource::SerializeInBlock(Archive& archive)
{
    (void)archive;
}

bool BuildDashboardResource::BeginLoad(Deserializer& source)
{
    JSONFile jsonFile(context_);
    if (!jsonFile.Load(source))
        return false;
    return FromJSON(jsonFile.GetRoot());
}

bool BuildDashboardResource::Save(Serializer& dest) const
{
    JSONFile jsonFile(context_);
    jsonFile.GetRoot() = ToJSON();
    return jsonFile.Save(dest);
}

} // namespace Urho3D
