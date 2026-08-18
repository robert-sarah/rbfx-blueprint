// Copyright (c) 2026 rbfx-blueprint contributors
// SPDX-License-Identifier: MIT

#include <Urho3D/WorldFabric/GameplayProduction.h>

#include <catch2/catch_amalgamated.hpp>

using namespace Urho3D;

namespace
{

GameplayItemDefinition MakeItem(const std::string& id, bool equippable = false, GameplayEquipmentSlot slot = GameplayEquipmentSlot::None)
{
    GameplayItemDefinition item;
    item.id = id;
    item.displayName = id;
    item.category = "gameplay";
    item.maxStack = 99;
    item.weight = 1.0f;
    item.value = 10;
    item.equippable = equippable;
    item.equipmentSlot = slot;
    return item;
}

} // namespace

TEST_CASE("Gameplay inventory stacks items and equipment swaps transactionally", "[gameplay][inventory][equipment]")
{
    GameplayInventory inventory;
    inventory.SetMaxWeight(4.0f);
    REQUIRE(inventory.RegisterDefinition(MakeItem("potion")));
    REQUIRE(inventory.RegisterDefinition(MakeItem("sword", true, GameplayEquipmentSlot::Weapon)));
    REQUIRE(inventory.RegisterDefinition(MakeItem("axe", true, GameplayEquipmentSlot::Weapon)));
    REQUIRE(inventory.AddItem("potion", 2));
    REQUIRE(inventory.AddItem("sword"));
    REQUIRE(inventory.AddItem("axe"));

    GameplayEquipment equipment;
    REQUIRE(equipment.Equip(GameplayEquipmentSlot::Weapon, "sword", inventory));
    CHECK_FALSE(inventory.HasItem("sword"));
    REQUIRE(equipment.Equip(GameplayEquipmentSlot::Weapon, "axe", inventory));
    CHECK(inventory.HasItem("sword"));
    CHECK(*equipment.GetEquipped(GameplayEquipmentSlot::Weapon) == "axe");
    CHECK_FALSE(inventory.AddItem("potion", 3));
    REQUIRE(equipment.Unequip(GameplayEquipmentSlot::Weapon, inventory));
    CHECK(inventory.HasItem("axe"));
}

TEST_CASE("Gameplay skill tree enforces prerequisites and refunds leaf skills", "[gameplay][skills]")
{
    GameplaySkillTree tree;
    REQUIRE(tree.Register({"root", "Root", 1, 1, {}}));
    REQUIRE(tree.Register({"advanced", "Advanced", 2, 2, {"root"}}));
    tree.SetAvailablePoints(5);
    CHECK_FALSE(tree.Unlock("advanced"));
    REQUIRE(tree.Unlock("root"));
    REQUIRE(tree.Unlock("advanced"));
    CHECK(tree.GetRank("root") == 1);
    CHECK(tree.GetRank("advanced") == 1);
    CHECK(tree.GetAvailablePoints() == 2);
    CHECK_FALSE(tree.ResetSkill("root"));
    REQUIRE(tree.ResetSkill("advanced"));
    CHECK(tree.GetRank("advanced") == 0);
    CHECK(tree.GetAvailablePoints() == 4);
    REQUIRE(tree.ResetSkill("root"));
    CHECK(tree.GetAvailablePoints() == 5);
}

TEST_CASE("Gameplay dialogue runtime gates choices with flags and reaches terminal nodes", "[gameplay][dialogue]")
{
    GameplayDialogueRuntime dialogue;
    REQUIRE(dialogue.RegisterNode({"start", "Guide", "The gate is closed.", {{"open", "Use the key", "end", {"has-key"}, {"gate-open"}}}, false}));
    REQUIRE(dialogue.RegisterNode({"end", "Guide", "The gate opens.", {}, true}));
    REQUIRE(dialogue.Start("start"));
    CHECK_FALSE(dialogue.CanChoose("open"));
    REQUIRE(dialogue.SetFlag("has-key"));
    REQUIRE(dialogue.Choose("open"));
    CHECK(dialogue.GetCurrentNodeId() == "end");
    CHECK(dialogue.IsCompleted());
    CHECK(dialogue.HasFlag("gate-open"));
}

TEST_CASE("Gameplay economy performs atomic buy and sell operations", "[gameplay][economy]")
{
    GameplayInventory inventory;
    REQUIRE(inventory.RegisterDefinition(MakeItem("ore")));
    GameplayEconomySystem economy;
    REQUIRE(economy.SetBalance("credits", 100));
    GameplayEconomyOffer offer;
    offer.id = "ore-offer";
    offer.itemId = "ore";
    offer.currency = "credits";
    offer.buyPrice = 25;
    offer.sellPrice = 10;
    offer.stock = 2;
    REQUIRE(economy.RegisterOffer(offer));
    REQUIRE(economy.Buy("ore-offer", inventory, 2));
    CHECK(inventory.GetQuantity("ore") == 2);
    CHECK(economy.GetBalance("credits") == 50);
    CHECK_FALSE(economy.Buy("ore-offer", inventory));
    REQUIRE(economy.Sell("ore-offer", inventory));
    CHECK(inventory.GetQuantity("ore") == 1);
    CHECK(economy.GetBalance("credits") == 60);
}

TEST_CASE("Advanced gameplay AI selects the highest-priority compatible stimulus deterministically", "[gameplay][ai]")
{
    GameplayAdvancedAIController ai;
    REQUIRE(ai.AddGoal({"combat", "enemy", GameplayAIMode::Combat, 10, 0.2f, 100.0f}));
    REQUIRE(ai.AddGoal({"investigate", "sound", GameplayAIMode::Investigate, 4, 0.1f, 100.0f}));
    REQUIRE(ai.SubmitStimulus({"sound-1", "sound", Vector3{2.0f, 0.0f, 0.0f}, 1.0f, 2.0f}));
    REQUIRE(ai.SubmitStimulus({"enemy-1", "enemy", Vector3{10.0f, 0.0f, 0.0f}, 0.8f, 2.0f}));
    ai.Update(Vector3::ZERO, 0.5f);
    CHECK(ai.GetState().mode == GameplayAIMode::Combat);
    CHECK(ai.GetState().goalId == "combat");
    CHECK(ai.GetState().targetPosition == Vector3{10.0f, 0.0f, 0.0f});
    ai.Update(Vector3::ZERO, 2.0f);
    CHECK(ai.GetState().mode == GameplayAIMode::Idle);
}

TEST_CASE("Character animation facade supports 2D and 3D profiles through one state machine", "[gameplay][animation][2d][3d]")
{
    GameplayCharacterAnimationProfile profile;
    profile.mode = GameplayAnimationMode::Sprite2D;
    profile.initialState = "idle";
    profile.clips = {{"idle", "idle.sheet", 1.0f, 1.0f, true}, {"run", "run.sheet", 0.5f, 1.0f, true}};
    GameplayCharacterAnimationController controller;
    REQUIRE(controller.Configure(profile));
    CHECK(controller.GetMode() == GameplayAnimationMode::Sprite2D);
    REQUIRE(controller.SetState("run"));
    CHECK(controller.GetCurrentState() == "run");
    controller.Update(0.1f);
    REQUIRE(controller.GetBlendResults().size() == 1);
    CHECK(controller.GetBlendResults().front().state == "run");
}

TEST_CASE("VFX system bounds emission and expires instances deterministically", "[gameplay][vfx]")
{
    GameplayVFXSystem vfx;
    REQUIRE(vfx.RegisterEffect({"spark", GameplayVFXMode::Hybrid, "spark.vfxgraph", 5, 10.0f, 1.0f}));
    const auto instance = vfx.Spawn("spark", Vector3{1.0f, 2.0f, 3.0f});
    REQUIRE(instance != 0);
    vfx.Update(0.8f);
    REQUIRE(vfx.GetInstances().size() == 1);
    CHECK(vfx.GetInstances().front().particleCount == 5);
    CHECK(vfx.GetActiveParticleCount() == 5);
    vfx.Update(0.2f);
    CHECK(vfx.GetInstances().empty());
}

TEST_CASE("Gameplay UI runtime resolves visible widgets by z-order and anchors", "[gameplay][ui][2d][3d]")
{
    GameplayUIRuntime ui;
    REQUIRE(ui.RegisterWidget({"panel", "Panel", Vector2{0.0f, 0.0f}, Vector2{}, Vector2{100.0f, 100.0f}, 1, true}));
    REQUIRE(ui.RegisterWidget({"button", "Play", Vector2{0.0f, 0.0f}, Vector2{}, Vector2{50.0f, 50.0f}, 2, true}));
    CHECK(ui.HitTest(Vector2{25.0f, 25.0f}, Vector2{800.0f, 600.0f}) == "button");
    REQUIRE(ui.SetVisible("button", false));
    CHECK(ui.HitTest(Vector2{25.0f, 25.0f}, Vector2{800.0f, 600.0f}) == "panel");
    REQUIRE(ui.SetText("panel", "Updated"));
    CHECK(ui.GetWidget("panel")->text == "Updated");
}

TEST_CASE("Gameplay content profiles keep 2D, 3D and hybrid projects separable", "[gameplay][content][2d][3d]")
{
    const auto profile2D = GameplayContentTooling::MakeDefaultProfile(GameplayProjectMode::TwoD);
    const auto profile3D = GameplayContentTooling::MakeDefaultProfile(GameplayProjectMode::ThreeD);
    const auto profileHybrid = GameplayContentTooling::MakeDefaultProfile(GameplayProjectMode::Hybrid);
    REQUIRE(GameplayContentTooling::ValidateProfile(profile2D));
    REQUIRE(GameplayContentTooling::ValidateProfile(profile3D));
    REQUIRE(GameplayContentTooling::ValidateProfile(profileHybrid));
    CHECK(profile2D.enablePhysics2D);
    CHECK_FALSE(profile2D.enablePhysics3D);
    CHECK(profile3D.enablePhysics3D);
    CHECK_FALSE(profile3D.enablePhysics2D);

    GameplayContentRegistry registry;
    REQUIRE(registry.RegisterAsset({"player-sprite", "Data/player.png", "sprite2d", GameplayProjectMode::TwoD, 1}));
    REQUIRE(registry.RegisterAsset({"player-model", "Data/player.mdl", "model3d", GameplayProjectMode::ThreeD, 1}));
    REQUIRE(registry.RegisterAsset({"dialogue", "Data/story.dialogue", "dialogue", GameplayProjectMode::Hybrid, 1}));
    CHECK(registry.Validate(profileHybrid));
    CHECK_FALSE(registry.Validate(profile2D));
    CHECK(registry.Query(GameplayProjectMode::TwoD).size() == 1);
    CHECK(registry.Query(GameplayProjectMode::ThreeD).size() == 1);
    const std::string firstManifest = GameplayContentTooling::BuildDeterministicManifest(registry, profileHybrid);
    const std::string secondManifest = GameplayContentTooling::BuildDeterministicManifest(registry, profileHybrid);
    CHECK(firstManifest == secondManifest);
    CHECK(GameplayContentTooling::GetSupportedExtensions().size() >= 10);
}
