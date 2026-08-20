// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "../Core/StringUtils.h"

#include <map>
#include <string>
#include <vector>

namespace Urho3D
{

struct URHO3D_API TextureStreamingRequest
{
    std::string resourceName;
    unsigned desiredMip{};
    float priority{};
};

struct URHO3D_API TextureStreamingDecision
{
    std::string resourceName;
    unsigned targetMip{};
    unsigned bytesRequired{};
    bool load{};
    bool evict{};
};

/// Deterministic mip-residency planner independent of the graphics backend.
/// Mip index zero is the highest-quality level; all levels from targetMip to
/// the coarsest level are considered resident.
class URHO3D_API TextureStreamingManager
{
public:
    bool RegisterTexture(const std::string& resourceName, const std::vector<unsigned>& mipBytes,
        unsigned residentMip = 0);
    bool UnregisterTexture(const std::string& resourceName);
    void Clear();

    void SetBudgetBytes(unsigned budgetBytes) { budgetBytes_ = budgetBytes; }
    unsigned GetBudgetBytes() const { return budgetBytes_; }
    unsigned GetRegisteredTextureCount() const { return static_cast<unsigned>(textures_.size()); }

    std::vector<TextureStreamingDecision> BuildPlan(const std::vector<TextureStreamingRequest>& requests) const;

private:
    struct TextureEntry
    {
        std::vector<unsigned> mipBytes;
        unsigned residentMip{};
    };

    static unsigned ResidentBytes(const TextureEntry& entry, unsigned firstMip);
    static unsigned ClampMip(const TextureEntry& entry, unsigned mip);

    std::map<std::string, TextureEntry> textures_;
    unsigned budgetBytes_{256u * 1024u * 1024u};
};

} // namespace Urho3D
