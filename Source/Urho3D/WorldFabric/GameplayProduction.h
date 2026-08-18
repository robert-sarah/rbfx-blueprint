// Copyright (c) 2026 rbfx-blueprint contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <Urho3D/Animation/AnimationStateMachine.h>
#include <Urho3D/Math/Vector2.h>
#include <Urho3D/Math/Vector3.h>
#include <Urho3D/Urho3D.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Urho3D
{

enum class GameplayProjectMode
{
    TwoD,
    ThreeD,
    Hybrid,
};

enum class GameplayEquipmentSlot
{
    None,
    Head,
    Body,
    Weapon,
    Offhand,
    Accessory,
};

enum class GameplayAnimationMode
{
    Sprite2D,
    Skeletal3D,
    Hybrid,
};

enum class GameplayVFXMode
{
    Sprite2D,
    Particle3D,
    Hybrid,
};

enum class GameplayAIMode
{
    Idle,
    Investigate,
    Chase,
    Flee,
    Combat,
};

struct GameplayItemDefinition
{
    std::string id;
    std::string displayName;
    std::string category;
    unsigned maxStack{1};
    float weight{0.0f};
    std::int64_t value{0};
    bool equippable{false};
    GameplayEquipmentSlot equipmentSlot{GameplayEquipmentSlot::None};
    std::vector<std::string> tags;
};

struct GameplayInventoryEntry
{
    std::string itemId;
    unsigned quantity{0};
    float durability{1.0f};
};

/// Deterministic item definitions and stack-aware inventory storage.
class URHO3D_API GameplayInventory
{
public:
    bool RegisterDefinition(const GameplayItemDefinition& definition, std::string* error = nullptr);
    bool RemoveDefinition(const std::string& itemId);
    const GameplayItemDefinition* GetDefinition(const std::string& itemId) const;

    void SetMaxWeight(float maxWeight) { maxWeight_ = maxWeight; }
    float GetMaxWeight() const { return maxWeight_; }
    float GetCurrentWeight() const;
    bool CanAddItem(const std::string& itemId, unsigned quantity = 1) const;
    bool AddItem(const std::string& itemId, unsigned quantity = 1);
    bool RemoveItem(const std::string& itemId, unsigned quantity = 1);
    bool HasItem(const std::string& itemId, unsigned quantity = 1) const;
    unsigned GetQuantity(const std::string& itemId) const;
    std::vector<GameplayInventoryEntry> GetEntries() const;
    void ClearItems();

private:
    std::map<std::string, GameplayItemDefinition> definitions_;
    std::map<std::string, GameplayInventoryEntry> items_;
    float maxWeight_{100.0f};
};

/// Transactional equipment loadout backed by GameplayInventory.
class URHO3D_API GameplayEquipment
{
public:
    bool Equip(GameplayEquipmentSlot slot, const std::string& itemId, GameplayInventory& inventory);
    bool Unequip(GameplayEquipmentSlot slot, GameplayInventory& inventory);
    const std::string* GetEquipped(GameplayEquipmentSlot slot) const;
    std::map<GameplayEquipmentSlot, std::string> GetLoadout() const { return loadout_; }
    void Clear(GameplayInventory& inventory);

private:
    std::map<GameplayEquipmentSlot, std::string> loadout_;
};

struct GameplaySkillDefinition
{
    std::string id;
    std::string displayName;
    unsigned maxRank{1};
    unsigned costPerRank{1};
    std::vector<std::string> prerequisites;
};

/// Prerequisite-aware skill tree with deterministic point spending and reset.
class URHO3D_API GameplaySkillTree
{
public:
    bool Register(const GameplaySkillDefinition& definition, std::string* error = nullptr);
    bool Unlock(const std::string& skillId);
    bool ResetSkill(const std::string& skillId);
    bool CanUnlock(const std::string& skillId) const;
    unsigned GetRank(const std::string& skillId) const;
    unsigned GetAvailablePoints() const { return availablePoints_; }
    unsigned GetSpentPoints() const { return spentPoints_; }
    void SetAvailablePoints(unsigned points) { availablePoints_ = points; }
    const GameplaySkillDefinition* GetDefinition(const std::string& skillId) const;
    std::map<std::string, unsigned> GetRanks() const { return ranks_; }
    void Clear();

private:
    std::map<std::string, GameplaySkillDefinition> definitions_;
    std::map<std::string, unsigned> ranks_;
    unsigned availablePoints_{};
    unsigned spentPoints_{};
};

struct GameplayDialogueChoice
{
    std::string id;
    std::string text;
    std::string nextNode;
    std::vector<std::string> requiredFlags;
    std::vector<std::string> grantsFlags;
};

struct GameplayDialogueNode
{
    std::string id;
    std::string speaker;
    std::string line;
    std::vector<GameplayDialogueChoice> choices;
    bool terminal{false};
};

/// Branching dialogue runtime with deterministic flag-gated choices.
class URHO3D_API GameplayDialogueRuntime
{
public:
    bool RegisterNode(const GameplayDialogueNode& node, std::string* error = nullptr);
    bool RemoveNode(const std::string& nodeId);
    bool Start(const std::string& nodeId);
    bool Choose(const std::string& choiceId);
    bool SetFlag(const std::string& flag, bool value = true);
    bool HasFlag(const std::string& flag) const;
    bool CanChoose(const std::string& choiceId) const;
    const GameplayDialogueNode* GetCurrentNode() const;
    const GameplayDialogueNode* GetNode(const std::string& nodeId) const;
    const std::string& GetCurrentNodeId() const { return currentNodeId_; }
    bool IsCompleted() const { return completed_; }
    std::vector<std::string> GetFlags() const;
    void Reset();

private:
    const GameplayDialogueChoice* FindChoice(const std::string& choiceId) const;
    std::map<std::string, GameplayDialogueNode> nodes_;
    std::map<std::string, bool> flags_;
    std::string currentNodeId_;
    bool completed_{};
};

struct GameplayEconomyOffer
{
    std::string id;
    std::string itemId;
    std::string currency{"credits"};
    std::int64_t buyPrice{0};
    std::int64_t sellPrice{0};
    unsigned stock{0};
    bool infiniteStock{false};
};

/// Integer-unit economy service with atomic buy/sell transactions.
class URHO3D_API GameplayEconomySystem
{
public:
    bool RegisterOffer(const GameplayEconomyOffer& offer, std::string* error = nullptr);
    bool RemoveOffer(const std::string& offerId);
    const GameplayEconomyOffer* GetOffer(const std::string& offerId) const;
    bool SetBalance(const std::string& currency, std::int64_t amount);
    std::int64_t GetBalance(const std::string& currency) const;
    bool AddBalance(const std::string& currency, std::int64_t amount);
    bool Buy(const std::string& offerId, GameplayInventory& inventory, unsigned quantity = 1);
    bool Sell(const std::string& offerId, GameplayInventory& inventory, unsigned quantity = 1);
    std::map<std::string, std::int64_t> GetBalances() const { return balances_; }
    void Clear();

private:
    std::map<std::string, GameplayEconomyOffer> offers_;
    std::map<std::string, std::int64_t> balances_;
};

struct GameplayAIStimulus
{
    std::string id;
    std::string type;
    Vector3 position{Vector3::ZERO};
    float strength{1.0f};
    float ttlSeconds{1.0f};
};

struct GameplayAIGoal
{
    std::string id;
    std::string stimulusType;
    GameplayAIMode mode{GameplayAIMode::Idle};
    int priority{};
    float minimumStrength{0.0f};
    float radius{100.0f};
};

struct GameplayAIState
{
    GameplayAIMode mode{GameplayAIMode::Idle};
    std::string goalId;
    Vector3 targetPosition{Vector3::ZERO};
    float confidence{};
};

/// Deterministic utility layer composing perception-like stimuli into gameplay goals.
class URHO3D_API GameplayAdvancedAIController
{
public:
    bool AddGoal(const GameplayAIGoal& goal, std::string* error = nullptr);
    bool RemoveGoal(const std::string& goalId);
    bool SubmitStimulus(const GameplayAIStimulus& stimulus);
    bool RemoveStimulus(const std::string& stimulusId);
    void Update(const Vector3& observerPosition, float elapsedSeconds);
    void Reset();
    const GameplayAIState& GetState() const { return state_; }
    std::vector<GameplayAIGoal> GetGoals() const;
    std::vector<GameplayAIStimulus> GetStimuli() const;

private:
    std::map<std::string, GameplayAIGoal> goals_;
    std::map<std::string, GameplayAIStimulus> stimuli_;
    GameplayAIState state_;
};

struct GameplayAnimationClip
{
    std::string id;
    std::string resource;
    float duration{1.0f};
    float playRate{1.0f};
    bool looping{true};
};

struct GameplayCharacterAnimationProfile
{
    GameplayAnimationMode mode{GameplayAnimationMode::Skeletal3D};
    std::vector<GameplayAnimationClip> clips;
    std::string initialState;
};

struct GameplayAnimationBlend
{
    std::string state;
    float weight{1.0f};
};

/// Renderer-independent character animation facade backed by AnimationStateMachine.
class URHO3D_API GameplayCharacterAnimationController
{
public:
    bool Configure(const GameplayCharacterAnimationProfile& profile, std::string* error = nullptr);
    bool SetState(const std::string& state);
    bool Update(float elapsedSeconds);
    const GameplayAnimationClip* GetClip(const std::string& state) const;
    std::vector<GameplayAnimationBlend> GetBlendResults() const;
    GameplayAnimationMode GetMode() const { return mode_; }
    const std::string& GetCurrentState() const { return currentState_; }
    float GetStateTime() const { return machine_.GetStateTime(); }
    bool IsConfigured() const { return configured_; }

private:
    GameplayAnimationMode mode_{GameplayAnimationMode::Skeletal3D};
    std::map<std::string, GameplayAnimationClip> clips_;
    AnimationStateMachine machine_;
    std::string currentState_;
    bool configured_{};
};

struct GameplayVFXEffectDefinition
{
    std::string id;
    GameplayVFXMode mode{GameplayVFXMode::Particle3D};
    std::string resource;
    unsigned maxParticles{64};
    float emissionRate{10.0f};
    float particleLifetime{1.0f};
};

struct GameplayVFXInstance
{
    std::uint64_t id{};
    std::string effectId;
    Vector3 position{Vector3::ZERO};
    unsigned particleCount{};
    float age{};
    float emissionAccumulator{};
    bool active{true};
};

/// Bounded deterministic VFX instance manager for sprite and particle effects.
class URHO3D_API GameplayVFXSystem
{
public:
    bool RegisterEffect(const GameplayVFXEffectDefinition& definition, std::string* error = nullptr);
    bool RemoveEffect(const std::string& effectId);
    const GameplayVFXEffectDefinition* GetEffect(const std::string& effectId) const;
    std::uint64_t Spawn(const std::string& effectId, const Vector3& position = Vector3::ZERO);
    bool Stop(std::uint64_t instanceId);
    void Update(float elapsedSeconds);
    std::vector<GameplayVFXInstance> GetInstances() const;
    unsigned GetActiveParticleCount() const;
    void Clear();

private:
    std::map<std::string, GameplayVFXEffectDefinition> effects_;
    std::map<std::uint64_t, GameplayVFXInstance> instances_;
    std::uint64_t nextInstanceId_{1};
};

struct GameplayWidgetDefinition
{
    std::string id;
    std::string text;
    Vector2 anchorMin{Vector2::ZERO};
    Vector2 anchorMax{Vector2::ZERO};
    Vector2 size{Vector2::ZERO};
    int zOrder{};
    bool visible{true};
};

/// Data-oriented UI layout/runtime contract usable by UIElement, 2D HUDs and 3D overlays.
class URHO3D_API GameplayUIRuntime
{
public:
    bool RegisterWidget(const GameplayWidgetDefinition& widget, std::string* error = nullptr);
    bool RemoveWidget(const std::string& widgetId);
    bool SetVisible(const std::string& widgetId, bool visible);
    bool SetText(const std::string& widgetId, const std::string& text);
    const GameplayWidgetDefinition* GetWidget(const std::string& widgetId) const;
    std::string HitTest(const Vector2& point, const Vector2& canvasSize) const;
    std::vector<GameplayWidgetDefinition> GetVisibleWidgets() const;
    void Clear();

private:
    std::map<std::string, GameplayWidgetDefinition> widgets_;
};

struct GameplayContentAsset
{
    std::string id;
    std::string path;
    std::string type;
    GameplayProjectMode mode{GameplayProjectMode::Hybrid};
    unsigned revision{1};
};

struct GameplayProjectProfile
{
    GameplayProjectMode mode{GameplayProjectMode::Hybrid};
    bool enablePhysics2D{true};
    bool enablePhysics3D{true};
    bool enableNavigation{true};
    bool enableLighting3D{true};
    bool enableUI{true};
};

/// Deterministic registry and authoring validation for gameplay content assets.
class URHO3D_API GameplayContentRegistry
{
public:
    bool RegisterAsset(const GameplayContentAsset& asset, std::string* error = nullptr);
    bool RemoveAsset(const std::string& assetId);
    const GameplayContentAsset* GetAsset(const std::string& assetId) const;
    std::vector<GameplayContentAsset> Query(GameplayProjectMode mode, const std::string& type = {}) const;
    std::vector<GameplayContentAsset> GetAllAssets() const;
    bool Validate(const GameplayProjectProfile& profile, std::string* error = nullptr) const;
    void Clear();

private:
    std::map<std::string, GameplayContentAsset> assets_;
};

class URHO3D_API GameplayContentTooling
{
public:
    static GameplayProjectProfile MakeDefaultProfile(GameplayProjectMode mode);
    static bool ValidateProfile(const GameplayProjectProfile& profile, std::string* error = nullptr);
    static bool IsAssetCompatible(const GameplayProjectProfile& profile, const GameplayContentAsset& asset);
    static std::vector<std::string> GetSupportedExtensions();
    static std::string BuildDeterministicManifest(const GameplayContentRegistry& registry, const GameplayProjectProfile& profile);
};

} // namespace Urho3D
