// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "OpenWorldGameplay.h"

#include <Urho3D/Scene/WorldPartition.h>
#include <Urho3D/Core/Context.h>
#include <Urho3D/Resource/JSONFile.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>

namespace Urho3D
{

namespace
{

float Clamp01(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

ea::string ToEA(const std::string& value)
{
    return ea::string{value.c_str()};
}

std::string ToStd(const ea::string& value)
{
    return std::string{value.c_str()};
}

void SetError(std::string* error, const char* message)
{
    if (error)
        *error = message;
}

JSONValue SerializeObjective(const OpenWorldQuestObjective& objective)
{
    JSONValue value(JSON_OBJECT);
    value.Set("id", ToEA(objective.id));
    value.Set("description", ToEA(objective.description));
    value.Set("required", objective.required);
    value.Set("progress", objective.progress);
    value.Set("optional", objective.optional);
    return value;
}

bool DeserializeObjective(const JSONValue& value, OpenWorldQuestObjective& objective)
{
    if (!value.IsObject() || !value.Contains("id") || !value["id"].IsString()
        || !value.Contains("description") || !value["description"].IsString()
        || !value.Contains("required") || !value["required"].IsNumber()
        || !value.Contains("progress") || !value["progress"].IsNumber()
        || !value.Contains("optional") || !value["optional"].IsBool())
        return false;

    objective.id = ToStd(value["id"].GetString());
    objective.description = ToStd(value["description"].GetString());
    objective.required = value["required"].GetUInt();
    objective.progress = value["progress"].GetUInt();
    objective.optional = value["optional"].GetBool();
    return !objective.id.empty() && objective.required > 0 && objective.progress <= objective.required;
}

JSONValue SerializeWeather(const OpenWorldWeatherState& weather)
{
    JSONValue value(JSON_OBJECT);
    value.Set("type", static_cast<unsigned>(weather.type));
    value.Set("intensity", weather.intensity);
    value.Set("windSpeed", weather.windSpeed);
    value.Set("humidity", weather.humidity);
    value.Set("transitionProgress", weather.transitionProgress);
    value.Set("seed", static_cast<unsigned>(weather.seed));
    return value;
}

bool DeserializeWeather(const JSONValue& value, OpenWorldWeatherState& weather)
{
    if (!value.IsObject() || !value.Contains("type") || !value["type"].IsNumber()
        || !value.Contains("intensity") || !value["intensity"].IsNumber()
        || !value.Contains("windSpeed") || !value["windSpeed"].IsNumber()
        || !value.Contains("humidity") || !value["humidity"].IsNumber()
        || !value.Contains("transitionProgress") || !value["transitionProgress"].IsNumber()
        || !value.Contains("seed") || !value["seed"].IsNumber())
        return false;

    const unsigned type = value["type"].GetUInt();
    if (type > static_cast<unsigned>(OpenWorldWeatherType::Snow))
        return false;
    weather.type = static_cast<OpenWorldWeatherType>(type);
    weather.intensity = value["intensity"].GetFloat();
    weather.windSpeed = value["windSpeed"].GetFloat();
    weather.humidity = value["humidity"].GetFloat();
    weather.transitionProgress = value["transitionProgress"].GetFloat();
    weather.seed = value["seed"].GetUInt();
    return std::isfinite(weather.intensity) && std::isfinite(weather.windSpeed) && std::isfinite(weather.humidity)
        && std::isfinite(weather.transitionProgress) && weather.intensity >= 0.0f && weather.intensity <= 1.0f
        && weather.windSpeed >= 0.0f && weather.humidity >= 0.0f && weather.humidity <= 1.0f
        && weather.transitionProgress >= 0.0f && weather.transitionProgress <= 1.0f;
}

bool DeserializeVector3(const JSONValue& value, Vector3& vector)
{
    if (!value.IsArray() || value.Size() != 3 || !value[0].IsNumber() || !value[1].IsNumber() || !value[2].IsNumber())
        return false;
    vector = Vector3{value[0].GetFloat(), value[1].GetFloat(), value[2].GetFloat()};
    return std::isfinite(vector.x_) && std::isfinite(vector.y_) && std::isfinite(vector.z_);
}

JSONValue SerializeVector3(const Vector3& vector)
{
    JSONValue value(JSON_ARRAY);
    value.Push(vector.x_);
    value.Push(vector.y_);
    value.Push(vector.z_);
    return value;
}

} // namespace

void OpenWorldWeatherSystem::SetState(OpenWorldWeatherType type, float intensity, float windSpeed, float humidity)
{
    state_.type = type;
    state_.intensity = Clamp01(intensity);
    state_.windSpeed = std::max(0.0f, windSpeed);
    state_.humidity = Clamp01(humidity);
    state_.transitionProgress = 1.0f;
    state_.seed = seed_;
    transitionElapsed_ = 0.0f;
    transitionDuration_ = 0.0f;
}

bool OpenWorldWeatherSystem::TransitionTo(OpenWorldWeatherType type, float durationSeconds)
{
    if (!std::isfinite(durationSeconds) || durationSeconds < 0.0f)
        return false;
    if (durationSeconds == 0.0f)
    {
        SetState(type, DefaultIntensity(type), DefaultWind(type), DefaultHumidity(type));
        return true;
    }

    transitionStart_ = state_;
    transitionTarget_ = state_;
    transitionTarget_.type = type;
    transitionTarget_.intensity = DefaultIntensity(type);
    transitionTarget_.windSpeed = DefaultWind(type);
    transitionTarget_.humidity = DefaultHumidity(type);
    transitionTarget_.transitionProgress = 1.0f;
    transitionTarget_.seed = seed_;
    transitionElapsed_ = 0.0f;
    transitionDuration_ = durationSeconds;
    state_.transitionProgress = 0.0f;
    return true;
}

void OpenWorldWeatherSystem::Update(float elapsedSeconds)
{
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0f || !IsTransitioning())
        return;

    transitionElapsed_ = std::min(transitionDuration_, transitionElapsed_ + elapsedSeconds);
    const float alpha = Clamp01(transitionElapsed_ / transitionDuration_);
    state_.intensity = transitionStart_.intensity + (transitionTarget_.intensity - transitionStart_.intensity) * alpha;
    state_.windSpeed = transitionStart_.windSpeed + (transitionTarget_.windSpeed - transitionStart_.windSpeed) * alpha;
    state_.humidity = transitionStart_.humidity + (transitionTarget_.humidity - transitionStart_.humidity) * alpha;
    state_.transitionProgress = alpha;
    state_.seed = seed_;
    if (alpha >= 1.0f)
    {
        state_.type = transitionTarget_.type;
        transitionDuration_ = 0.0f;
    }
}

float OpenWorldWeatherSystem::Clamp01(float value)
{
    return ::Urho3D::Clamp01(value);
}

float OpenWorldWeatherSystem::DefaultIntensity(OpenWorldWeatherType type)
{
    switch (type)
    {
    case OpenWorldWeatherType::Rain: return 0.65f;
    case OpenWorldWeatherType::Storm: return 1.0f;
    case OpenWorldWeatherType::Fog: return 0.55f;
    case OpenWorldWeatherType::Snow: return 0.7f;
    default: return 0.0f;
    }
}

float OpenWorldWeatherSystem::DefaultWind(OpenWorldWeatherType type)
{
    switch (type)
    {
    case OpenWorldWeatherType::Storm: return 22.0f;
    case OpenWorldWeatherType::Snow: return 7.0f;
    case OpenWorldWeatherType::Rain: return 5.0f;
    default: return 0.0f;
    }
}

float OpenWorldWeatherSystem::DefaultHumidity(OpenWorldWeatherType type)
{
    switch (type)
    {
    case OpenWorldWeatherType::Clear: return 0.35f;
    case OpenWorldWeatherType::Fog: return 1.0f;
    case OpenWorldWeatherType::Storm: return 0.95f;
    default: return 0.75f;
    }
}

void OpenWorldDayNightCycle::SetDayLength(float seconds)
{
    if (!std::isfinite(seconds))
        return;
    state_.dayLengthSeconds = std::max(1.0f, seconds);
    SetTimeOfDay(state_.timeOfDaySeconds);
}

void OpenWorldDayNightCycle::SetTimeOfDay(float seconds)
{
    if (!std::isfinite(seconds))
        return;
    const float day = state_.dayLengthSeconds;
    float wrapped = std::fmod(seconds, day);
    if (wrapped < 0.0f)
        wrapped += day;
    state_.timeOfDaySeconds = wrapped;
    Recalculate();
}

void OpenWorldDayNightCycle::Advance(float elapsedSeconds)
{
    if (std::isfinite(elapsedSeconds))
        SetTimeOfDay(state_.timeOfDaySeconds + elapsedSeconds);
}

void OpenWorldDayNightCycle::Recalculate()
{
    constexpr float pi = 3.14159265358979323846f;
    const float fraction = state_.timeOfDaySeconds / state_.dayLengthSeconds;
    const float solarPhase = (fraction - 0.25f) * 2.0f * pi;
    const float solarSin = std::sin(solarPhase);
    state_.sunElevationDegrees = solarSin * 90.0f;
    state_.sunAzimuthDegrees = fraction * 360.0f;
    state_.daylightFactor = Clamp01((solarSin + 0.12f) / 1.12f);
    state_.ambientFactor = 0.12f + state_.daylightFactor * 0.88f;
}

bool OpenWorldQuestSystem::Register(const OpenWorldQuestDefinition& definition)
{
    if (definition.id.empty() || definition.title.empty() || definition.objectives.empty())
        return false;
    std::unordered_map<std::string, bool> seen;
    for (const OpenWorldQuestObjective& objective : definition.objectives)
    {
        if (objective.id.empty() || objective.required == 0 || !seen.emplace(objective.id, true).second)
            return false;
    }
    return definitions_.emplace(definition.id, definition).second;
}

bool OpenWorldQuestSystem::Unregister(const std::string& questId)
{
    if (states_.find(questId) != states_.end())
        return false;
    return definitions_.erase(questId) != 0;
}

bool OpenWorldQuestSystem::Start(const std::string& questId)
{
    const auto definition = definitions_.find(questId);
    if (definition == definitions_.end())
        return false;
    const auto existing = states_.find(questId);
    if (existing != states_.end() && existing->second.active)
        return false;
    if (existing != states_.end() && existing->second.completed && !definition->second.repeatable)
        return false;

    OpenWorldQuestState state;
    state.questId = questId;
    state.objectives = definition->second.objectives;
    state.active = true;
    state.completed = false;
    state.revision = existing == states_.end() ? 1 : existing->second.revision + 1;
    for (OpenWorldQuestObjective& objective : state.objectives)
        objective.progress = 0;
    states_[questId] = state;
    return true;
}

bool OpenWorldQuestSystem::AdvanceObjective(const std::string& questId, const std::string& objectiveId, unsigned amount)
{
    if (amount == 0)
        return false;
    auto quest = states_.find(questId);
    if (quest == states_.end() || !quest->second.active || quest->second.completed)
        return false;
    for (OpenWorldQuestObjective& objective : quest->second.objectives)
    {
        if (objective.id != objectiveId)
            continue;
        objective.progress = std::min(objective.required, objective.progress + amount);
        ++quest->second.revision;
        if (IsComplete(quest->second))
        {
            quest->second.active = false;
            quest->second.completed = true;
        }
        return true;
    }
    return false;
}

bool OpenWorldQuestSystem::CompleteObjective(const std::string& questId, const std::string& objectiveId)
{
    const OpenWorldQuestState* state = GetState(questId);
    if (!state)
        return false;
    for (const OpenWorldQuestObjective& objective : state->objectives)
    {
        if (objective.id == objectiveId)
            return AdvanceObjective(questId, objectiveId, objective.required);
    }
    return false;
}

bool OpenWorldQuestSystem::Abandon(const std::string& questId)
{
    auto iter = states_.find(questId);
    if (iter == states_.end() || !iter->second.active)
        return false;
    iter->second.active = false;
    iter->second.completed = false;
    ++iter->second.revision;
    return true;
}

const OpenWorldQuestDefinition* OpenWorldQuestSystem::GetDefinition(const std::string& questId) const
{
    const auto iter = definitions_.find(questId);
    return iter != definitions_.end() ? &iter->second : nullptr;
}

const OpenWorldQuestState* OpenWorldQuestSystem::GetState(const std::string& questId) const
{
    const auto iter = states_.find(questId);
    return iter != states_.end() ? &iter->second : nullptr;
}

std::vector<OpenWorldQuestState> OpenWorldQuestSystem::GetActiveStates() const
{
    std::vector<OpenWorldQuestState> result;
    for (const auto& entry : states_)
    {
        if (entry.second.active)
            result.push_back(entry.second);
    }
    std::sort(result.begin(), result.end(), [](const OpenWorldQuestState& lhs, const OpenWorldQuestState& rhs)
    {
        return lhs.questId < rhs.questId;
    });
    return result;
}

std::vector<OpenWorldQuestState> OpenWorldQuestSystem::GetAllStates() const
{
    std::vector<OpenWorldQuestState> result;
    result.reserve(states_.size());
    for (const auto& entry : states_)
        result.push_back(entry.second);
    std::sort(result.begin(), result.end(), [](const OpenWorldQuestState& lhs, const OpenWorldQuestState& rhs)
    {
        return lhs.questId < rhs.questId;
    });
    return result;
}

std::vector<std::string> OpenWorldQuestSystem::GetCompletedQuestIds() const
{
    std::vector<std::string> result;
    for (const auto& entry : states_)
    {
        if (entry.second.completed)
            result.push_back(entry.first);
    }
    std::sort(result.begin(), result.end());
    return result;
}

bool OpenWorldQuestSystem::RestoreState(const OpenWorldQuestState& state)
{
    const OpenWorldQuestDefinition* definition = GetDefinition(state.questId);
    if (!definition || state.questId.empty() || state.objectives.size() != definition->objectives.size())
        return false;

    std::unordered_map<std::string, bool> expected;
    for (const OpenWorldQuestObjective& objective : definition->objectives)
        expected[objective.id] = true;
    for (const OpenWorldQuestObjective& objective : state.objectives)
    {
        if (!expected.erase(objective.id) || objective.required == 0 || objective.progress > objective.required)
            return false;
    }
    if (!expected.empty())
        return false;
    states_[state.questId] = state;
    return true;
}

void OpenWorldQuestSystem::Clear()
{
    definitions_.clear();
    states_.clear();
}

bool OpenWorldQuestSystem::IsComplete(const OpenWorldQuestState& state)
{
    for (const OpenWorldQuestObjective& objective : state.objectives)
    {
        if (!objective.optional && objective.progress < objective.required)
            return false;
    }
    return true;
}

std::string OpenWorldSaveService::Serialize(const OpenWorldSaveState& state)
{
    Context context;
    JSONFile jsonFile{&context};
    JSONValue root(JSON_OBJECT);
    root.Set("version", state.version);
    root.Set("playerPosition", SerializeVector3(state.playerPosition));
    root.Set("worldTimeSeconds", state.worldTimeSeconds);
    root.Set("weather", SerializeWeather(state.weather));
    root.Set("worldRevision", static_cast<unsigned>(state.worldRevision));

    JSONValue quests(JSON_ARRAY);
    for (const OpenWorldSaveQuest& quest : state.quests)
    {
        JSONValue questValue(JSON_OBJECT);
        questValue.Set("questId", ToEA(quest.questId));
        questValue.Set("active", quest.active);
        questValue.Set("completed", quest.completed);
        questValue.Set("revision", quest.revision);
        JSONValue objectives(JSON_ARRAY);
        for (const OpenWorldQuestObjective& objective : quest.objectives)
            objectives.Push(SerializeObjective(objective));
        questValue.Set("objectives", objectives);
        quests.Push(questValue);
    }
    root.Set("quests", quests);

    JSONValue changedCells(JSON_ARRAY);
    std::vector<std::string> sortedCells = state.changedCells;
    std::sort(sortedCells.begin(), sortedCells.end());
    sortedCells.erase(std::unique(sortedCells.begin(), sortedCells.end()), sortedCells.end());
    for (const std::string& cell : sortedCells)
        changedCells.Push(ToEA(cell));
    root.Set("changedCells", changedCells);

    jsonFile.GetRoot() = root;
    return ToStd(jsonFile.ToString("  "));
}

bool OpenWorldSaveService::Deserialize(const std::string& source, OpenWorldSaveState& state, std::string* error)
{
    Context context;
    JSONFile jsonFile{&context};
    if (!jsonFile.FromString(ToEA(source)) || !jsonFile.GetRoot().IsObject())
    {
        SetError(error, "World save is not valid JSON object data");
        return false;
    }
    const JSONValue& root = jsonFile.GetRoot();
    if (!root.Contains("version") || !root["version"].IsNumber() || root["version"].GetUInt() != 1)
    {
        SetError(error, "Unsupported world save version");
        return false;
    }

    OpenWorldSaveState parsed;
    parsed.version = root["version"].GetUInt();
    if (!root.Contains("playerPosition") || !DeserializeVector3(root["playerPosition"], parsed.playerPosition)
        || !root.Contains("worldTimeSeconds") || !root["worldTimeSeconds"].IsNumber()
        || !std::isfinite(root["worldTimeSeconds"].GetFloat())
        || !root.Contains("weather") || !DeserializeWeather(root["weather"], parsed.weather)
        || !root.Contains("worldRevision") || !root["worldRevision"].IsNumber()
        || !root.Contains("quests") || !root["quests"].IsArray()
        || !root.Contains("changedCells") || !root["changedCells"].IsArray())
    {
        SetError(error, "World save is missing required fields or contains invalid values");
        return false;
    }
    parsed.worldTimeSeconds = root["worldTimeSeconds"].GetFloat();
    parsed.worldRevision = root["worldRevision"].GetUInt();

    for (const JSONValue& questValue : root["quests"].GetArray())
    {
        if (!questValue.IsObject() || !questValue.Contains("questId") || !questValue["questId"].IsString()
            || !questValue.Contains("active") || !questValue["active"].IsBool()
            || !questValue.Contains("completed") || !questValue["completed"].IsBool()
            || !questValue.Contains("revision") || !questValue["revision"].IsNumber()
            || !questValue.Contains("objectives") || !questValue["objectives"].IsArray())
        {
            SetError(error, "World save contains an invalid quest state");
            return false;
        }
        OpenWorldSaveQuest quest;
        quest.questId = ToStd(questValue["questId"].GetString());
        quest.active = questValue["active"].GetBool();
        quest.completed = questValue["completed"].GetBool();
        quest.revision = questValue["revision"].GetUInt();
        if (quest.questId.empty())
        {
            SetError(error, "World save contains a quest without an id");
            return false;
        }
        for (const JSONValue& objectiveValue : questValue["objectives"].GetArray())
        {
            OpenWorldQuestObjective objective;
            if (!DeserializeObjective(objectiveValue, objective))
            {
                SetError(error, "World save contains an invalid quest objective");
                return false;
            }
            quest.objectives.push_back(objective);
        }
        parsed.quests.push_back(quest);
    }

    for (const JSONValue& cellValue : root["changedCells"].GetArray())
    {
        if (!cellValue.IsString() || cellValue.GetString().empty())
        {
            SetError(error, "World save contains an invalid changed-cell id");
            return false;
        }
        parsed.changedCells.push_back(ToStd(cellValue.GetString()));
    }
    std::sort(parsed.changedCells.begin(), parsed.changedCells.end());
    parsed.changedCells.erase(std::unique(parsed.changedCells.begin(), parsed.changedCells.end()), parsed.changedCells.end());
    state = parsed;
    if (error)
        error->clear();
    return true;
}

bool OpenWorldSaveService::SaveFile(const std::string& path, const OpenWorldSaveState& state, std::string* error)
{
    if (path.empty())
    {
        SetError(error, "Cannot save world state to an empty path");
        return false;
    }
    const std::string temporaryPath = path + ".tmp";
    std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        SetError(error, "Cannot open temporary world save file");
        return false;
    }
    const std::string serialized = Serialize(state);
    output.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
    output.close();
    if (!output)
    {
        std::remove(temporaryPath.c_str());
        SetError(error, "Cannot write temporary world save file");
        return false;
    }
    std::remove(path.c_str());
    if (std::rename(temporaryPath.c_str(), path.c_str()) != 0)
    {
        std::remove(temporaryPath.c_str());
        SetError(error, "Cannot atomically replace world save file");
        return false;
    }
    if (error)
        error->clear();
    return true;
}

bool OpenWorldSaveService::LoadFile(const std::string& path, OpenWorldSaveState& state, std::string* error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        SetError(error, "Cannot open world save file");
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof())
    {
        SetError(error, "Cannot read world save file");
        return false;
    }
    return Deserialize(buffer.str(), state, error);
}

bool OpenWorldSpatialAudioService::AddZone(const OpenWorldAudioZone& zone)
{
    if (zone.id.empty() || zone.halfExtents.x_ < 0.0f || zone.halfExtents.y_ < 0.0f || zone.halfExtents.z_ < 0.0f
        || !std::isfinite(zone.falloffDistance) || zone.falloffDistance < 0.0f
        || !std::isfinite(zone.reverbSend) || !std::isfinite(zone.ambientVolume))
        return false;
    OpenWorldAudioZone sanitized = zone;
    sanitized.reverbSend = Clamp01(sanitized.reverbSend);
    sanitized.ambientVolume = Clamp01(sanitized.ambientVolume);
    return zones_.emplace(zone.id, sanitized).second;
}

bool OpenWorldSpatialAudioService::RemoveZone(const std::string& zoneId)
{
    return zones_.erase(zoneId) != 0;
}

const OpenWorldAudioZone* OpenWorldSpatialAudioService::GetZone(const std::string& zoneId) const
{
    const auto iter = zones_.find(zoneId);
    return iter != zones_.end() ? &iter->second : nullptr;
}

OpenWorldAudioMix OpenWorldSpatialAudioService::Evaluate(const Vector3& listenerPosition) const
{
    OpenWorldAudioMix result;
    const OpenWorldAudioZone* selected = nullptr;
    float selectedInfluence = 0.0f;
    for (const auto& entry : zones_)
    {
        const OpenWorldAudioZone& zone = entry.second;
        const Vector3 delta = listenerPosition - zone.center;
        const Vector3 outside{std::max(std::abs(delta.x_) - zone.halfExtents.x_, 0.0f),
            std::max(std::abs(delta.y_) - zone.halfExtents.y_, 0.0f),
            std::max(std::abs(delta.z_) - zone.halfExtents.z_, 0.0f)};
        const float distance = outside.Length();
        const float influence = distance == 0.0f ? 1.0f
            : zone.falloffDistance > 0.0f ? Clamp01(1.0f - distance / zone.falloffDistance) : 0.0f;
        if (influence <= 0.0f)
            continue;
        if (!selected || zone.priority > selected->priority
            || (zone.priority == selected->priority && (influence > selectedInfluence
                || (influence == selectedInfluence && zone.id < selected->id))))
        {
            selected = &zone;
            selectedInfluence = influence;
        }
    }
    if (selected)
    {
        result.zoneId = selected->id;
        result.influence = selectedInfluence;
        result.reverbSend = selected->reverbSend * selectedInfluence;
        result.ambientVolume = selected->ambientVolume * selectedInfluence;
        result.ambientSound = selected->ambientSound;
    }
    return result;
}

void OpenWorldSpatialAudioService::Clear()
{
    zones_.clear();
}

unsigned OpenWorldRuntimeServices::Update(WorldPartition& partition, const Vector3& observerPosition, float elapsedSeconds)
{
    weather_.Update(elapsedSeconds);
    dayNight_.Advance(elapsedSeconds);
    return streaming_.Update(partition, observerPosition);
}

OpenWorldSaveState OpenWorldRuntimeServices::CaptureSaveState(const Vector3& playerPosition, uint64_t worldRevision,
    const std::vector<std::string>& changedCells) const
{
    OpenWorldSaveState state;
    state.playerPosition = playerPosition;
    state.worldTimeSeconds = dayNight_.GetTimeOfDay();
    state.weather = weather_.GetState();
    state.worldRevision = worldRevision;
    state.changedCells = changedCells;
    for (const OpenWorldQuestState& quest : quests_.GetAllStates())
        state.quests.push_back({quest.questId, quest.objectives, quest.active, quest.completed, quest.revision});
    return state;
}

bool OpenWorldRuntimeServices::RestoreSaveState(const OpenWorldSaveState& state)
{
    for (const OpenWorldSaveQuest& quest : state.quests)
    {
        if (!quests_.GetDefinition(quest.questId))
            return false;
    }
    weather_.SetSeed(state.weather.seed);
    weather_.SetState(state.weather.type, state.weather.intensity, state.weather.windSpeed, state.weather.humidity);
    dayNight_.SetTimeOfDay(state.worldTimeSeconds);
    for (const OpenWorldSaveQuest& quest : state.quests)
    {
        if (!quests_.RestoreState({quest.questId, quest.objectives, quest.active, quest.completed, quest.revision}))
            return false;
    }
    return true;
}

} // namespace Urho3D
