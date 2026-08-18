// Copyright (c) 2026 rbfx-blueprint contributors
// SPDX-License-Identifier: MIT

#include "GameplayProduction.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace Urho3D
{

namespace
{

bool IsValidText(const std::string& value)
{
    return !value.empty() && value.find('\n') == std::string::npos && value.find('|') == std::string::npos;
}

void SetError(std::string* error, const char* message)
{
    if (error)
        *error = message;
}

bool MultiplyPositive(std::int64_t value, unsigned quantity, std::int64_t& result)
{
    if (value < 0 || quantity == 0)
        return false;
    if (value > std::numeric_limits<std::int64_t>::max() / quantity)
        return false;
    result = value * quantity;
    return true;
}

unsigned ModeValue(GameplayProjectMode mode)
{
    return static_cast<unsigned>(mode);
}

bool IsValidMode(GameplayProjectMode mode)
{
    return mode == GameplayProjectMode::TwoD || mode == GameplayProjectMode::ThreeD || mode == GameplayProjectMode::Hybrid;
}

} // namespace

bool GameplayInventory::RegisterDefinition(const GameplayItemDefinition& definition, std::string* error)
{
    if (!IsValidText(definition.id) || !IsValidText(definition.displayName) || definition.maxStack == 0
        || !std::isfinite(definition.weight) || definition.weight < 0.0f || definition.value < 0)
    {
        SetError(error, "Invalid item definition");
        return false;
    }
    if (definitions_.count(definition.id))
    {
        SetError(error, "Item definition already exists");
        return false;
    }
    if (definition.equippable && definition.equipmentSlot == GameplayEquipmentSlot::None)
    {
        SetError(error, "Equippable item requires an equipment slot");
        return false;
    }
    definitions_.emplace(definition.id, definition);
    return true;
}

bool GameplayInventory::RemoveDefinition(const std::string& itemId)
{
    if (items_.count(itemId))
        return false;
    return definitions_.erase(itemId) != 0;
}

const GameplayItemDefinition* GameplayInventory::GetDefinition(const std::string& itemId) const
{
    const auto it = definitions_.find(itemId);
    return it != definitions_.end() ? &it->second : nullptr;
}

float GameplayInventory::GetCurrentWeight() const
{
    float weight = 0.0f;
    for (const auto& pair : items_)
    {
        const auto definition = definitions_.find(pair.first);
        if (definition != definitions_.end())
            weight += definition->second.weight * pair.second.quantity;
    }
    return weight;
}

bool GameplayInventory::CanAddItem(const std::string& itemId, unsigned quantity) const
{
    const auto definition = definitions_.find(itemId);
    if (definition == definitions_.end() || quantity == 0)
        return false;
    const float addedWeight = definition->second.weight * quantity;
    return std::isfinite(addedWeight) && GetCurrentWeight() + addedWeight <= maxWeight_ + 0.0001f;
}

bool GameplayInventory::AddItem(const std::string& itemId, unsigned quantity)
{
    if (!CanAddItem(itemId, quantity))
        return false;
    auto& entry = items_[itemId];
    entry.itemId = itemId;
    entry.quantity += quantity;
    entry.durability = std::max(0.0f, std::min(1.0f, entry.durability));
    return true;
}

bool GameplayInventory::RemoveItem(const std::string& itemId, unsigned quantity)
{
    if (!HasItem(itemId, quantity))
        return false;
    auto it = items_.find(itemId);
    it->second.quantity -= quantity;
    if (it->second.quantity == 0)
        items_.erase(it);
    return true;
}

bool GameplayInventory::HasItem(const std::string& itemId, unsigned quantity) const
{
    const auto it = items_.find(itemId);
    return quantity > 0 && it != items_.end() && it->second.quantity >= quantity;
}

unsigned GameplayInventory::GetQuantity(const std::string& itemId) const
{
    const auto it = items_.find(itemId);
    return it != items_.end() ? it->second.quantity : 0;
}

std::vector<GameplayInventoryEntry> GameplayInventory::GetEntries() const
{
    std::vector<GameplayInventoryEntry> result;
    result.reserve(items_.size());
    for (const auto& pair : items_)
        result.push_back(pair.second);
    return result;
}

void GameplayInventory::ClearItems()
{
    items_.clear();
}

bool GameplayEquipment::Equip(GameplayEquipmentSlot slot, const std::string& itemId, GameplayInventory& inventory)
{
    if (slot == GameplayEquipmentSlot::None || !inventory.HasItem(itemId))
        return false;
    const GameplayItemDefinition* definition = inventory.GetDefinition(itemId);
    if (!definition || !definition->equippable || definition->equipmentSlot != slot)
        return false;

    const auto current = loadout_.find(slot);
    if (current != loadout_.end() && current->second == itemId)
        return true;
    if (current != loadout_.end() && !inventory.AddItem(current->second))
        return false;
    if (!inventory.RemoveItem(itemId))
    {
        if (current != loadout_.end())
            inventory.RemoveItem(current->second);
        return false;
    }
    loadout_[slot] = itemId;
    return true;
}

bool GameplayEquipment::Unequip(GameplayEquipmentSlot slot, GameplayInventory& inventory)
{
    const auto it = loadout_.find(slot);
    if (it == loadout_.end())
        return false;
    if (!inventory.AddItem(it->second))
        return false;
    loadout_.erase(it);
    return true;
}

const std::string* GameplayEquipment::GetEquipped(GameplayEquipmentSlot slot) const
{
    const auto it = loadout_.find(slot);
    return it != loadout_.end() ? &it->second : nullptr;
}

void GameplayEquipment::Clear(GameplayInventory& inventory)
{
    while (!loadout_.empty())
        Unequip(loadout_.begin()->first, inventory);
}

bool GameplaySkillTree::Register(const GameplaySkillDefinition& definition, std::string* error)
{
    if (!IsValidText(definition.id) || !IsValidText(definition.displayName) || definition.maxRank == 0 || definition.costPerRank == 0)
    {
        SetError(error, "Invalid skill definition");
        return false;
    }
    if (definitions_.count(definition.id))
    {
        SetError(error, "Skill definition already exists");
        return false;
    }
    for (const auto& prerequisite : definition.prerequisites)
    {
        if (prerequisite.empty() || prerequisite == definition.id)
        {
            SetError(error, "Invalid skill prerequisite");
            return false;
        }
    }
    definitions_.emplace(definition.id, definition);
    ranks_[definition.id] = 0;
    return true;
}

bool GameplaySkillTree::CanUnlock(const std::string& skillId) const
{
    const auto definition = definitions_.find(skillId);
    if (definition == definitions_.end())
        return false;
    const auto rank = ranks_.find(skillId);
    const unsigned currentRank = rank != ranks_.end() ? rank->second : 0;
    if (currentRank >= definition->second.maxRank || availablePoints_ < definition->second.costPerRank)
        return false;
    for (const auto& prerequisite : definition->second.prerequisites)
    {
        const auto prerequisiteRank = ranks_.find(prerequisite);
        if (prerequisiteRank == ranks_.end() || prerequisiteRank->second == 0)
            return false;
    }
    return true;
}

bool GameplaySkillTree::Unlock(const std::string& skillId)
{
    if (!CanUnlock(skillId))
        return false;
    const auto& definition = definitions_.at(skillId);
    ++ranks_[skillId];
    availablePoints_ -= definition.costPerRank;
    spentPoints_ += definition.costPerRank;
    return true;
}

bool GameplaySkillTree::ResetSkill(const std::string& skillId)
{
    const auto rankIt = ranks_.find(skillId);
    const auto definition = definitions_.find(skillId);
    if (rankIt == ranks_.end() || definition == definitions_.end() || rankIt->second == 0)
        return false;
    for (const auto& pair : definitions_)
    {
        if (pair.second.prerequisites.end() != std::find(pair.second.prerequisites.begin(), pair.second.prerequisites.end(), skillId)
            && ranks_.at(pair.first) > 0)
            return false;
    }
    const unsigned refund = rankIt->second * definition->second.costPerRank;
    rankIt->second = 0;
    availablePoints_ += refund;
    spentPoints_ -= refund;
    return true;
}

unsigned GameplaySkillTree::GetRank(const std::string& skillId) const
{
    const auto it = ranks_.find(skillId);
    return it != ranks_.end() ? it->second : 0;
}

const GameplaySkillDefinition* GameplaySkillTree::GetDefinition(const std::string& skillId) const
{
    const auto it = definitions_.find(skillId);
    return it != definitions_.end() ? &it->second : nullptr;
}

void GameplaySkillTree::Clear()
{
    definitions_.clear();
    ranks_.clear();
    availablePoints_ = 0;
    spentPoints_ = 0;
}

bool GameplayDialogueRuntime::RegisterNode(const GameplayDialogueNode& node, std::string* error)
{
    if (!IsValidText(node.id) || node.line.empty() || nodes_.count(node.id))
    {
        SetError(error, "Invalid or duplicate dialogue node");
        return false;
    }
    std::map<std::string, bool> choiceIds;
    for (const auto& choice : node.choices)
    {
        if (!IsValidText(choice.id) || choice.text.empty() || choiceIds.count(choice.id))
        {
            SetError(error, "Invalid or duplicate dialogue choice");
            return false;
        }
        choiceIds[choice.id] = true;
    }
    nodes_.emplace(node.id, node);
    return true;
}

bool GameplayDialogueRuntime::RemoveNode(const std::string& nodeId)
{
    if (currentNodeId_ == nodeId)
        Reset();
    return nodes_.erase(nodeId) != 0;
}

bool GameplayDialogueRuntime::Start(const std::string& nodeId)
{
    if (!nodes_.count(nodeId))
        return false;
    currentNodeId_ = nodeId;
    completed_ = nodes_.at(nodeId).terminal;
    return true;
}

const GameplayDialogueChoice* GameplayDialogueRuntime::FindChoice(const std::string& choiceId) const
{
    const GameplayDialogueNode* node = GetCurrentNode();
    if (!node)
        return nullptr;
    for (const auto& choice : node->choices)
    {
        if (choice.id == choiceId)
            return &choice;
    }
    return nullptr;
}

bool GameplayDialogueRuntime::CanChoose(const std::string& choiceId) const
{
    const GameplayDialogueChoice* choice = FindChoice(choiceId);
    if (!choice)
        return false;
    return std::all_of(choice->requiredFlags.begin(), choice->requiredFlags.end(), [this](const std::string& flag)
    {
        return HasFlag(flag);
    });
}

bool GameplayDialogueRuntime::Choose(const std::string& choiceId)
{
    const GameplayDialogueChoice* choice = FindChoice(choiceId);
    if (!choice || !CanChoose(choiceId))
        return false;
    for (const auto& flag : choice->grantsFlags)
        SetFlag(flag);
    if (choice->nextNode.empty())
    {
        completed_ = true;
        return true;
    }
    if (!nodes_.count(choice->nextNode))
        return false;
    currentNodeId_ = choice->nextNode;
    completed_ = nodes_.at(currentNodeId_).terminal;
    return true;
}

bool GameplayDialogueRuntime::SetFlag(const std::string& flag, bool value)
{
    if (!IsValidText(flag))
        return false;
    flags_[flag] = value;
    return true;
}

bool GameplayDialogueRuntime::HasFlag(const std::string& flag) const
{
    const auto it = flags_.find(flag);
    return it != flags_.end() && it->second;
}

const GameplayDialogueNode* GameplayDialogueRuntime::GetCurrentNode() const
{
    return GetNode(currentNodeId_);
}

const GameplayDialogueNode* GameplayDialogueRuntime::GetNode(const std::string& nodeId) const
{
    const auto it = nodes_.find(nodeId);
    return it != nodes_.end() ? &it->second : nullptr;
}

std::vector<std::string> GameplayDialogueRuntime::GetFlags() const
{
    std::vector<std::string> result;
    for (const auto& flag : flags_)
    {
        if (flag.second)
            result.push_back(flag.first);
    }
    return result;
}

void GameplayDialogueRuntime::Reset()
{
    currentNodeId_.clear();
    flags_.clear();
    completed_ = false;
}

bool GameplayEconomySystem::RegisterOffer(const GameplayEconomyOffer& offer, std::string* error)
{
    if (!IsValidText(offer.id) || !IsValidText(offer.itemId) || !IsValidText(offer.currency)
        || offer.buyPrice < 0 || offer.sellPrice < 0 || offers_.count(offer.id))
    {
        SetError(error, "Invalid or duplicate economy offer");
        return false;
    }
    offers_.emplace(offer.id, offer);
    return true;
}

bool GameplayEconomySystem::RemoveOffer(const std::string& offerId)
{
    return offers_.erase(offerId) != 0;
}

const GameplayEconomyOffer* GameplayEconomySystem::GetOffer(const std::string& offerId) const
{
    const auto it = offers_.find(offerId);
    return it != offers_.end() ? &it->second : nullptr;
}

bool GameplayEconomySystem::SetBalance(const std::string& currency, std::int64_t amount)
{
    if (!IsValidText(currency) || amount < 0)
        return false;
    balances_[currency] = amount;
    return true;
}

std::int64_t GameplayEconomySystem::GetBalance(const std::string& currency) const
{
    const auto it = balances_.find(currency);
    return it != balances_.end() ? it->second : 0;
}

bool GameplayEconomySystem::AddBalance(const std::string& currency, std::int64_t amount)
{
    if (!IsValidText(currency) || amount < 0 || GetBalance(currency) > std::numeric_limits<std::int64_t>::max() - amount)
        return false;
    balances_[currency] += amount;
    return true;
}

bool GameplayEconomySystem::Buy(const std::string& offerId, GameplayInventory& inventory, unsigned quantity)
{
    const GameplayEconomyOffer* offer = GetOffer(offerId);
    if (!offer || quantity == 0 || (!offer->infiniteStock && offer->stock < quantity))
        return false;
    std::int64_t total = 0;
    if (!MultiplyPositive(offer->buyPrice, quantity, total) || GetBalance(offer->currency) < total || !inventory.CanAddItem(offer->itemId, quantity))
        return false;
    if (!inventory.AddItem(offer->itemId, quantity))
        return false;
    balances_[offer->currency] -= total;
    if (!offer->infiniteStock)
        offers_[offerId].stock -= quantity;
    return true;
}

bool GameplayEconomySystem::Sell(const std::string& offerId, GameplayInventory& inventory, unsigned quantity)
{
    const GameplayEconomyOffer* offer = GetOffer(offerId);
    if (!offer || quantity == 0 || !inventory.HasItem(offer->itemId, quantity))
        return false;
    std::int64_t total = 0;
    if (!MultiplyPositive(offer->sellPrice, quantity, total) || !AddBalance(offer->currency, total))
        return false;
    if (!inventory.RemoveItem(offer->itemId, quantity))
    {
        balances_[offer->currency] -= total;
        return false;
    }
    if (!offer->infiniteStock)
    {
        const std::uint64_t newStock = static_cast<std::uint64_t>(offer->stock) + quantity;
        offers_[offerId].stock = static_cast<unsigned>(std::min<std::uint64_t>(newStock, std::numeric_limits<unsigned>::max()));
    }
    return true;
}

void GameplayEconomySystem::Clear()
{
    offers_.clear();
    balances_.clear();
}

bool GameplayAdvancedAIController::AddGoal(const GameplayAIGoal& goal, std::string* error)
{
    if (!IsValidText(goal.id) || !IsValidText(goal.stimulusType) || goal.priority < 0 || !std::isfinite(goal.minimumStrength)
        || !std::isfinite(goal.radius) || goal.minimumStrength < 0.0f || goal.radius <= 0.0f || goals_.count(goal.id))
    {
        SetError(error, "Invalid or duplicate AI goal");
        return false;
    }
    goals_.emplace(goal.id, goal);
    return true;
}

bool GameplayAdvancedAIController::RemoveGoal(const std::string& goalId)
{
    if (state_.goalId == goalId)
        state_ = {};
    return goals_.erase(goalId) != 0;
}

bool GameplayAdvancedAIController::SubmitStimulus(const GameplayAIStimulus& stimulus)
{
    if (!IsValidText(stimulus.id) || !IsValidText(stimulus.type) || !std::isfinite(stimulus.strength)
        || !std::isfinite(stimulus.ttlSeconds) || stimulus.strength < 0.0f || stimulus.ttlSeconds <= 0.0f)
        return false;
    stimuli_[stimulus.id] = stimulus;
    return true;
}

bool GameplayAdvancedAIController::RemoveStimulus(const std::string& stimulusId)
{
    return stimuli_.erase(stimulusId) != 0;
}

void GameplayAdvancedAIController::Update(const Vector3& observerPosition, float elapsedSeconds)
{
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0f)
        return;
    for (auto it = stimuli_.begin(); it != stimuli_.end();)
    {
        it->second.ttlSeconds -= elapsedSeconds;
        if (it->second.ttlSeconds <= 0.0f)
            it = stimuli_.erase(it);
        else
            ++it;
    }

    bool found = false;
    int bestPriority = -1;
    float bestScore = -1.0f;
    std::string bestGoal;
    std::string bestStimulus;
    for (const auto& goalPair : goals_)
    {
        const GameplayAIGoal& goal = goalPair.second;
        for (const auto& stimulusPair : stimuli_)
        {
            const GameplayAIStimulus& stimulus = stimulusPair.second;
            const Vector3 offset = stimulus.position - observerPosition;
            const float distance = offset.Length();
            if (stimulus.type != goal.stimulusType || distance > goal.radius || stimulus.strength < goal.minimumStrength)
                continue;
            const float score = stimulus.strength / (1.0f + distance);
            if (!found || goal.priority > bestPriority || (goal.priority == bestPriority && score > bestScore)
                || (goal.priority == bestPriority && score == bestScore && goal.id < bestGoal))
            {
                found = true;
                bestPriority = goal.priority;
                bestScore = score;
                bestGoal = goal.id;
                bestStimulus = stimulus.id;
            }
        }
    }

    if (!found)
    {
        state_ = {};
        return;
    }
    const GameplayAIStimulus& stimulus = stimuli_.at(bestStimulus);
    state_.mode = goals_.at(bestGoal).mode;
    state_.goalId = bestGoal;
    state_.targetPosition = stimulus.position;
    state_.confidence = std::max(0.0f, std::min(1.0f, bestScore));
}

void GameplayAdvancedAIController::Reset()
{
    stimuli_.clear();
    state_ = {};
}

std::vector<GameplayAIGoal> GameplayAdvancedAIController::GetGoals() const
{
    std::vector<GameplayAIGoal> result;
    for (const auto& pair : goals_)
        result.push_back(pair.second);
    return result;
}

std::vector<GameplayAIStimulus> GameplayAdvancedAIController::GetStimuli() const
{
    std::vector<GameplayAIStimulus> result;
    for (const auto& pair : stimuli_)
        result.push_back(pair.second);
    return result;
}

bool GameplayCharacterAnimationController::Configure(const GameplayCharacterAnimationProfile& profile, std::string* error)
{
    if (profile.clips.empty())
    {
        SetError(error, "Animation profile has no clips");
        return false;
    }
    std::string initial = profile.initialState.empty() ? profile.clips.front().id : profile.initialState;
    clips_.clear();
    machine_.Clear();
    for (const auto& clip : profile.clips)
    {
        if (!IsValidText(clip.id) || !std::isfinite(clip.duration) || !std::isfinite(clip.playRate) || clip.duration <= 0.0f || clip.playRate <= 0.0f
            || clips_.count(clip.id))
        {
            SetError(error, "Invalid or duplicate animation clip");
            return false;
        }
        AnimationStateMachineState state;
        state.name = ea::string{clip.id.c_str()};
        state.clip = ea::string{clip.resource.c_str()};
        state.duration = clip.duration;
        state.playRate = clip.playRate;
        state.looping = clip.looping;
        if (!machine_.AddState(state))
        {
            SetError(error, "Animation state machine rejected clip");
            return false;
        }
        clips_.emplace(clip.id, clip);
    }
    if (!clips_.count(initial) || !machine_.SetInitialState(ea::string{initial.c_str()}) || !machine_.Start(ea::string{initial.c_str()}))
    {
        SetError(error, "Animation profile has an invalid initial state");
        clips_.clear();
        machine_.Clear();
        return false;
    }
    mode_ = profile.mode;
    currentState_ = initial;
    configured_ = true;
    return true;
}

bool GameplayCharacterAnimationController::SetState(const std::string& state)
{
    if (!configured_ || !clips_.count(state))
        return false;
    if (!machine_.Start(ea::string{state.c_str()}))
        return false;
    currentState_ = state;
    return true;
}

bool GameplayCharacterAnimationController::Update(float elapsedSeconds)
{
    if (!configured_ || !std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0f)
        return false;
    const bool changed = machine_.Update(elapsedSeconds);
    currentState_ = machine_.GetCurrentState().c_str();
    return changed;
}

const GameplayAnimationClip* GameplayCharacterAnimationController::GetClip(const std::string& state) const
{
    const auto it = clips_.find(state);
    return it != clips_.end() ? &it->second : nullptr;
}

std::vector<GameplayAnimationBlend> GameplayCharacterAnimationController::GetBlendResults() const
{
    std::vector<GameplayAnimationBlend> result;
    for (const auto& blend : machine_.GetBlendResults())
        result.push_back({blend.state.c_str(), blend.weight});
    return result;
}

bool GameplayVFXSystem::RegisterEffect(const GameplayVFXEffectDefinition& definition, std::string* error)
{
    if (!IsValidText(definition.id) || !std::isfinite(definition.emissionRate) || !std::isfinite(definition.particleLifetime)
        || definition.maxParticles == 0 || definition.emissionRate < 0.0f || definition.particleLifetime <= 0.0f || effects_.count(definition.id))
    {
        SetError(error, "Invalid or duplicate VFX effect");
        return false;
    }
    effects_.emplace(definition.id, definition);
    return true;
}

bool GameplayVFXSystem::RemoveEffect(const std::string& effectId)
{
    for (const auto& pair : instances_)
    {
        if (pair.second.effectId == effectId)
            return false;
    }
    return effects_.erase(effectId) != 0;
}

const GameplayVFXEffectDefinition* GameplayVFXSystem::GetEffect(const std::string& effectId) const
{
    const auto it = effects_.find(effectId);
    return it != effects_.end() ? &it->second : nullptr;
}

std::uint64_t GameplayVFXSystem::Spawn(const std::string& effectId, const Vector3& position)
{
    if (!effects_.count(effectId))
        return 0;
    const std::uint64_t id = nextInstanceId_++;
    instances_[id] = {id, effectId, position, 0, 0.0f, 0.0f, true};
    return id;
}

bool GameplayVFXSystem::Stop(std::uint64_t instanceId)
{
    return instances_.erase(instanceId) != 0;
}

void GameplayVFXSystem::Update(float elapsedSeconds)
{
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0f)
        return;
    for (auto it = instances_.begin(); it != instances_.end();)
    {
        GameplayVFXInstance& instance = it->second;
        const GameplayVFXEffectDefinition& effect = effects_.at(instance.effectId);
        instance.age += elapsedSeconds;
        if (instance.age >= effect.particleLifetime)
        {
            it = instances_.erase(it);
            continue;
        }
        instance.emissionAccumulator += effect.emissionRate * elapsedSeconds;
        const unsigned emitted = static_cast<unsigned>(std::min<float>(std::floor(instance.emissionAccumulator), effect.maxParticles));
        if (emitted > 0)
        {
            instance.particleCount = std::min(effect.maxParticles, instance.particleCount + emitted);
            instance.emissionAccumulator -= emitted;
        }
        ++it;
    }
}

std::vector<GameplayVFXInstance> GameplayVFXSystem::GetInstances() const
{
    std::vector<GameplayVFXInstance> result;
    for (const auto& pair : instances_)
        result.push_back(pair.second);
    return result;
}

unsigned GameplayVFXSystem::GetActiveParticleCount() const
{
    std::uint64_t result = 0;
    for (const auto& pair : instances_)
        result = std::min<std::uint64_t>(std::numeric_limits<unsigned>::max(), result + pair.second.particleCount);
    return static_cast<unsigned>(result);
}

void GameplayVFXSystem::Clear()
{
    instances_.clear();
    effects_.clear();
    nextInstanceId_ = 1;
}

bool GameplayUIRuntime::RegisterWidget(const GameplayWidgetDefinition& widget, std::string* error)
{
    if (!IsValidText(widget.id) || widgets_.count(widget.id) || !std::isfinite(widget.anchorMin.x_) || !std::isfinite(widget.anchorMin.y_)
        || !std::isfinite(widget.anchorMax.x_) || !std::isfinite(widget.anchorMax.y_) || !std::isfinite(widget.size.x_) || !std::isfinite(widget.size.y_)
        || widget.anchorMin.x_ < 0.0f || widget.anchorMin.y_ < 0.0f || widget.anchorMax.x_ < widget.anchorMin.x_
        || widget.anchorMax.y_ < widget.anchorMin.y_ || widget.anchorMax.x_ > 1.0f || widget.anchorMax.y_ > 1.0f
        || widget.size.x_ < 0.0f || widget.size.y_ < 0.0f)
    {
        SetError(error, "Invalid or duplicate UI widget");
        return false;
    }
    widgets_.emplace(widget.id, widget);
    return true;
}

bool GameplayUIRuntime::RemoveWidget(const std::string& widgetId)
{
    return widgets_.erase(widgetId) != 0;
}

bool GameplayUIRuntime::SetVisible(const std::string& widgetId, bool visible)
{
    const auto it = widgets_.find(widgetId);
    if (it == widgets_.end())
        return false;
    it->second.visible = visible;
    return true;
}

bool GameplayUIRuntime::SetText(const std::string& widgetId, const std::string& text)
{
    const auto it = widgets_.find(widgetId);
    if (it == widgets_.end())
        return false;
    it->second.text = text;
    return true;
}

const GameplayWidgetDefinition* GameplayUIRuntime::GetWidget(const std::string& widgetId) const
{
    const auto it = widgets_.find(widgetId);
    return it != widgets_.end() ? &it->second : nullptr;
}

std::string GameplayUIRuntime::HitTest(const Vector2& point, const Vector2& canvasSize) const
{
    if (canvasSize.x_ <= 0.0f || canvasSize.y_ <= 0.0f)
        return {};
    std::string result;
    int bestZ = std::numeric_limits<int>::min();
    for (const auto& pair : widgets_)
    {
        const GameplayWidgetDefinition& widget = pair.second;
        if (!widget.visible)
            continue;
        const float minX = widget.anchorMin.x_ * canvasSize.x_;
        const float minY = widget.anchorMin.y_ * canvasSize.y_;
        const float maxX = widget.anchorMax.x_ > widget.anchorMin.x_ ? widget.anchorMax.x_ * canvasSize.x_ : minX + widget.size.x_;
        const float maxY = widget.anchorMax.y_ > widget.anchorMin.y_ ? widget.anchorMax.y_ * canvasSize.y_ : minY + widget.size.y_;
        if (point.x_ >= minX && point.x_ <= maxX && point.y_ >= minY && point.y_ <= maxY
            && (widget.zOrder > bestZ || (widget.zOrder == bestZ && widget.id > result)))
        {
            bestZ = widget.zOrder;
            result = widget.id;
        }
    }
    return result;
}

std::vector<GameplayWidgetDefinition> GameplayUIRuntime::GetVisibleWidgets() const
{
    std::vector<GameplayWidgetDefinition> result;
    for (const auto& pair : widgets_)
    {
        if (pair.second.visible)
            result.push_back(pair.second);
    }
    std::sort(result.begin(), result.end(), [](const GameplayWidgetDefinition& left, const GameplayWidgetDefinition& right)
    {
        if (left.zOrder != right.zOrder)
            return left.zOrder < right.zOrder;
        return left.id < right.id;
    });
    return result;
}

void GameplayUIRuntime::Clear()
{
    widgets_.clear();
}

bool GameplayContentRegistry::RegisterAsset(const GameplayContentAsset& asset, std::string* error)
{
    if (!IsValidText(asset.id) || !IsValidText(asset.path) || !IsValidText(asset.type) || !IsValidMode(asset.mode) || asset.revision == 0
        || assets_.count(asset.id))
    {
        SetError(error, "Invalid or duplicate gameplay content asset");
        return false;
    }
    assets_.emplace(asset.id, asset);
    return true;
}

bool GameplayContentRegistry::RemoveAsset(const std::string& assetId)
{
    return assets_.erase(assetId) != 0;
}

const GameplayContentAsset* GameplayContentRegistry::GetAsset(const std::string& assetId) const
{
    const auto it = assets_.find(assetId);
    return it != assets_.end() ? &it->second : nullptr;
}

std::vector<GameplayContentAsset> GameplayContentRegistry::Query(GameplayProjectMode mode, const std::string& type) const
{
    std::vector<GameplayContentAsset> result;
    for (const auto& pair : assets_)
    {
        if (pair.second.mode == mode && (type.empty() || pair.second.type == type))
            result.push_back(pair.second);
    }
    return result;
}

std::vector<GameplayContentAsset> GameplayContentRegistry::GetAllAssets() const
{
    std::vector<GameplayContentAsset> result;
    for (const auto& pair : assets_)
        result.push_back(pair.second);
    return result;
}

bool GameplayContentRegistry::Validate(const GameplayProjectProfile& profile, std::string* error) const
{
    if (!GameplayContentTooling::ValidateProfile(profile, error))
        return false;
    for (const auto& pair : assets_)
    {
        if (!GameplayContentTooling::IsAssetCompatible(profile, pair.second))
        {
            SetError(error, "Content asset is incompatible with project profile");
            return false;
        }
    }
    return true;
}

void GameplayContentRegistry::Clear()
{
    assets_.clear();
}

GameplayProjectProfile GameplayContentTooling::MakeDefaultProfile(GameplayProjectMode mode)
{
    GameplayProjectProfile profile;
    profile.mode = mode;
    profile.enablePhysics2D = mode != GameplayProjectMode::ThreeD;
    profile.enablePhysics3D = mode != GameplayProjectMode::TwoD;
    profile.enableNavigation = mode != GameplayProjectMode::TwoD;
    profile.enableLighting3D = mode != GameplayProjectMode::TwoD;
    profile.enableUI = true;
    return profile;
}

bool GameplayContentTooling::ValidateProfile(const GameplayProjectProfile& profile, std::string* error)
{
    if (!IsValidMode(profile.mode) || (profile.mode == GameplayProjectMode::TwoD && (profile.enablePhysics3D || profile.enableLighting3D))
        || (profile.mode == GameplayProjectMode::ThreeD && profile.enablePhysics2D))
    {
        SetError(error, "Invalid 2D, 3D or hybrid project profile");
        return false;
    }
    if (profile.mode == GameplayProjectMode::TwoD && profile.enableNavigation)
    {
        SetError(error, "2D profile cannot enable 3D navigation");
        return false;
    }
    return true;
}

bool GameplayContentTooling::IsAssetCompatible(const GameplayProjectProfile& profile, const GameplayContentAsset& asset)
{
    if (!IsValidMode(profile.mode) || !IsValidMode(asset.mode))
        return false;
    return profile.mode == GameplayProjectMode::Hybrid || profile.mode == asset.mode;
}

std::vector<std::string> GameplayContentTooling::GetSupportedExtensions()
{
    return {".inventory", ".equipment", ".dialogue", ".skilltree", ".economy", ".aiprofile", ".animationprofile", ".vfx", ".ui",
        ".gameplay2d", ".gameplay3d", ".gameplayhybrid"};
}

std::string GameplayContentTooling::BuildDeterministicManifest(const GameplayContentRegistry& registry, const GameplayProjectProfile& profile)
{
    if (!ValidateProfile(profile))
        return {};
    std::ostringstream stream;
    stream << "profile|" << ModeValue(profile.mode) << "|" << profile.enablePhysics2D << "|" << profile.enablePhysics3D << "|"
           << profile.enableNavigation << "|" << profile.enableLighting3D << "|" << profile.enableUI << '\n';
    for (const auto& asset : registry.GetAllAssets())
    {
        if (!IsAssetCompatible(profile, asset))
            continue;
        stream << asset.id << '|' << asset.path << '|' << asset.type << '|' << ModeValue(asset.mode) << '|' << asset.revision << '\n';
    }
    return stream.str();
}

} // namespace Urho3D
