// Copyright (c) 2026 rbfx-blueprint contributors.
// SPDX-License-Identifier: MIT

#include "TextureStreaming.h"

#include <algorithm>
#include <utility>

namespace Urho3D
{

bool TextureStreamingManager::RegisterTexture(const std::string& resourceName,
    const std::vector<unsigned>& mipBytes, unsigned residentMip)
{
    if (resourceName.empty() || mipBytes.empty())
        return false;
    if (std::any_of(mipBytes.begin(), mipBytes.end(), [](unsigned bytes) { return bytes == 0; }))
        return false;

    TextureEntry entry;
    entry.mipBytes = mipBytes;
    entry.residentMip = ClampMip(entry, residentMip);
    textures_[resourceName] = std::move(entry);
    return true;
}

bool TextureStreamingManager::UnregisterTexture(const std::string& resourceName)
{
    return textures_.erase(resourceName) != 0;
}

void TextureStreamingManager::Clear()
{
    textures_.clear();
}

unsigned TextureStreamingManager::ResidentBytes(const TextureEntry& entry, unsigned firstMip)
{
    firstMip = ClampMip(entry, firstMip);
    unsigned total = 0;
    for (unsigned mip = firstMip; mip < entry.mipBytes.size(); ++mip)
        total += entry.mipBytes[mip];
    return total;
}

unsigned TextureStreamingManager::ClampMip(const TextureEntry& entry, unsigned mip)
{
    return entry.mipBytes.empty() ? 0 : std::min(mip, static_cast<unsigned>(entry.mipBytes.size() - 1));
}

std::vector<TextureStreamingDecision> TextureStreamingManager::BuildPlan(
    const std::vector<TextureStreamingRequest>& requests) const
{
    std::vector<TextureStreamingRequest> sortedRequests;
    sortedRequests.reserve(requests.size());
    for (const TextureStreamingRequest& request : requests)
    {
        if (textures_.find(request.resourceName) != textures_.end())
            sortedRequests.push_back(request);
    }
    std::sort(sortedRequests.begin(), sortedRequests.end(), [](const auto& lhs, const auto& rhs)
    {
        if (lhs.priority != rhs.priority)
            return lhs.priority > rhs.priority;
        return lhs.resourceName < rhs.resourceName;
    });

    std::vector<TextureStreamingDecision> decisions;
    decisions.reserve(sortedRequests.size());
    unsigned usedBytes = 0;
    for (const TextureStreamingRequest& request : sortedRequests)
    {
        const TextureEntry& entry = textures_.at(request.resourceName);
        const unsigned desiredMip = ClampMip(entry, request.desiredMip);
        unsigned targetMip = desiredMip;
        while (targetMip < entry.mipBytes.size() && usedBytes + ResidentBytes(entry, targetMip) > budgetBytes_)
            ++targetMip;
        if (targetMip >= entry.mipBytes.size())
            targetMip = static_cast<unsigned>(entry.mipBytes.size() - 1);

        const unsigned bytesRequired = ResidentBytes(entry, targetMip);
        usedBytes += bytesRequired;
        TextureStreamingDecision decision;
        decision.resourceName = request.resourceName;
        decision.targetMip = targetMip;
        decision.bytesRequired = bytesRequired;
        decision.load = targetMip < entry.residentMip;
        decision.evict = targetMip > entry.residentMip;
        decisions.push_back(std::move(decision));
    }
    return decisions;
}

} // namespace Urho3D
