// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <Urho3D/Math/Vector3.h>
#include <Urho3D/WorldFabric/OpenWorldRuntime.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Urho3D
{

class WorldPartition;

enum class OpenWorldWeatherType
{
    Clear,
    Rain,
    Storm,
    Fog,
    Snow,
};

struct OpenWorldWeatherState
{
    OpenWorldWeatherType type{OpenWorldWeatherType::Clear};
    float intensity{0.0f};
    float windSpeed{0.0f};
    float humidity{0.5f};
    float transitionProgress{1.0f};
    uint64_t seed{1};
};

/// Deterministic weather state machine. Rendering, particles and audio consume its state.
class OpenWorldWeatherSystem
{
public:
    void SetSeed(uint64_t seed) { seed_ = seed ? seed : 1; state_.seed = seed_; }
    uint64_t GetSeed() const { return seed_; }
    void SetState(OpenWorldWeatherType type, float intensity, float windSpeed, float humidity);
    bool TransitionTo(OpenWorldWeatherType type, float durationSeconds);
    void Update(float elapsedSeconds);
    const OpenWorldWeatherState& GetState() const { return state_; }
    bool IsTransitioning() const { return transitionDuration_ > 0.0f && state_.transitionProgress < 1.0f; }

private:
    static float Clamp01(float value);
    static float DefaultIntensity(OpenWorldWeatherType type);
    static float DefaultWind(OpenWorldWeatherType type);
    static float DefaultHumidity(OpenWorldWeatherType type);

    uint64_t seed_{1};
    OpenWorldWeatherState state_;
    OpenWorldWeatherState transitionStart_;
    OpenWorldWeatherState transitionTarget_;
    float transitionElapsed_{0.0f};
    float transitionDuration_{0.0f};
};

struct OpenWorldDayNightState
{
    float timeOfDaySeconds{43200.0f};
    float dayLengthSeconds{86400.0f};
    float sunElevationDegrees{0.0f};
    float sunAzimuthDegrees{0.0f};
    float daylightFactor{1.0f};
    float ambientFactor{1.0f};
};

/// Fixed-rate day/night clock with stable sun and ambient-light projections.
class OpenWorldDayNightCycle
{
public:
    void SetDayLength(float seconds);
    float GetDayLength() const { return state_.dayLengthSeconds; }
    void SetTimeOfDay(float seconds);
    void Advance(float elapsedSeconds);
    const OpenWorldDayNightState& GetState() const { return state_; }
    float GetTimeOfDay() const { return state_.timeOfDaySeconds; }

private:
    void Recalculate();
    OpenWorldDayNightState state_;
};

struct OpenWorldQuestObjective
{
    std::string id;
    std::string description;
    unsigned required{1};
    unsigned progress{0};
    bool optional{false};
};

struct OpenWorldQuestDefinition
{
    std::string id;
    std::string title;
    std::string description;
    std::vector<OpenWorldQuestObjective> objectives;
    std::string reward;
    bool repeatable{false};
};

struct OpenWorldQuestState
{
    std::string questId;
    std::vector<OpenWorldQuestObjective> objectives;
    bool active{true};
    bool completed{false};
    unsigned revision{0};
};

/// Data-oriented quest service with deterministic objective progression and persistence-friendly state.
class OpenWorldQuestSystem
{
public:
    bool Register(const OpenWorldQuestDefinition& definition);
    bool Unregister(const std::string& questId);
    bool Start(const std::string& questId);
    bool AdvanceObjective(const std::string& questId, const std::string& objectiveId, unsigned amount = 1);
    bool CompleteObjective(const std::string& questId, const std::string& objectiveId);
    bool Abandon(const std::string& questId);
    const OpenWorldQuestDefinition* GetDefinition(const std::string& questId) const;
    const OpenWorldQuestState* GetState(const std::string& questId) const;
    std::vector<OpenWorldQuestState> GetActiveStates() const;
    std::vector<OpenWorldQuestState> GetAllStates() const;
    std::vector<std::string> GetCompletedQuestIds() const;
    bool RestoreState(const OpenWorldQuestState& state);
    void Clear();

private:
    static bool IsComplete(const OpenWorldQuestState& state);
    std::unordered_map<std::string, OpenWorldQuestDefinition> definitions_;
    std::unordered_map<std::string, OpenWorldQuestState> states_;
};

struct OpenWorldSaveQuest
{
    std::string questId;
    std::vector<OpenWorldQuestObjective> objectives;
    bool active{true};
    bool completed{false};
    unsigned revision{0};
};

struct OpenWorldSaveState
{
    unsigned version{1};
    Vector3 playerPosition{Vector3::ZERO};
    float worldTimeSeconds{43200.0f};
    OpenWorldWeatherState weather;
    uint64_t worldRevision{0};
    std::vector<OpenWorldSaveQuest> quests;
    std::vector<std::string> changedCells;
};

/// Versioned JSON persistence for runtime world state. It performs atomic replacement for file saves.
class OpenWorldSaveService
{
public:
    static std::string Serialize(const OpenWorldSaveState& state);
    static bool Deserialize(const std::string& source, OpenWorldSaveState& state, std::string* error = nullptr);
    static bool SaveFile(const std::string& path, const OpenWorldSaveState& state, std::string* error = nullptr);
    static bool LoadFile(const std::string& path, OpenWorldSaveState& state, std::string* error = nullptr);
};

struct OpenWorldAudioZone
{
    std::string id;
    Vector3 center{Vector3::ZERO};
    Vector3 halfExtents{Vector3::ONE};
    unsigned priority{0};
    float reverbSend{0.0f};
    float ambientVolume{1.0f};
    float falloffDistance{10.0f};
    std::string ambientSound;
};

struct OpenWorldAudioMix
{
    std::string zoneId;
    float influence{0.0f};
    float reverbSend{0.0f};
    float ambientVolume{0.0f};
    std::string ambientSound;
};

/// Deterministic spatial audio zone resolver complementing SoundSource3D attenuation.
class OpenWorldSpatialAudioService
{
public:
    bool AddZone(const OpenWorldAudioZone& zone);
    bool RemoveZone(const std::string& zoneId);
    const OpenWorldAudioZone* GetZone(const std::string& zoneId) const;
    OpenWorldAudioMix Evaluate(const Vector3& listenerPosition) const;
    void Clear();

private:
    std::unordered_map<std::string, OpenWorldAudioZone> zones_;
};

/// Runtime composition root for the deterministic open-world services.
class OpenWorldRuntimeServices
{
public:
    unsigned Update(WorldPartition& partition, const Vector3& observerPosition, float elapsedSeconds);
    OpenWorldSaveState CaptureSaveState(const Vector3& playerPosition, uint64_t worldRevision,
        const std::vector<std::string>& changedCells) const;
    bool RestoreSaveState(const OpenWorldSaveState& state);

    OpenWorldStreamingScheduler& GetStreaming() { return streaming_; }
    OpenWorldWeatherSystem& GetWeather() { return weather_; }
    OpenWorldDayNightCycle& GetDayNight() { return dayNight_; }
    OpenWorldQuestSystem& GetQuests() { return quests_; }
    OpenWorldSpatialAudioService& GetSpatialAudio() { return spatialAudio_; }
    const OpenWorldStreamingScheduler& GetStreaming() const { return streaming_; }
    const OpenWorldWeatherSystem& GetWeather() const { return weather_; }
    const OpenWorldDayNightCycle& GetDayNight() const { return dayNight_; }
    const OpenWorldQuestSystem& GetQuests() const { return quests_; }
    const OpenWorldSpatialAudioService& GetSpatialAudio() const { return spatialAudio_; }

private:
    OpenWorldStreamingScheduler streaming_;
    OpenWorldWeatherSystem weather_;
    OpenWorldDayNightCycle dayNight_;
    OpenWorldQuestSystem quests_;
    OpenWorldSpatialAudioService spatialAudio_;
};

} // namespace Urho3D
