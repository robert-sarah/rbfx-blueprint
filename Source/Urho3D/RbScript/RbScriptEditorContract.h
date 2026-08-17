// SPDX-License-Identifier: MIT

#pragma once

#include <EASTL/string.h>

namespace Urho3D
{

/// Small editor-independent contract shared by resource routing and automated tests.
namespace RbScriptEditorContract
{

inline bool IsRbScriptResourcePath(const ea::string& resourceName)
{
    constexpr const char* extension = ".rbscript";
    constexpr unsigned extensionSize = 9;
    return resourceName.size() >= extensionSize
        && resourceName.substr(resourceName.size() - extensionSize) == extension;
}

inline ea::string GetTemplateSource(unsigned templateIndex)
{
    switch (templateIndex)
    {
    case 1:
        return
            "// rbscript component template\n"
            "module Generated.Health;\n"
            "script HealthComponent : Component {\n"
            "    var health: int = 100;\n"
            "}\n";
    case 2:
        return
            "// rbscript gameplay template\n"
            "module Generated.Gameplay;\n"
            "script PlayerController : Component {\n"
            "    fn on_start() {\n"
            "    }\n"
            "    fn tick(delta: float) {\n"
            "    }\n"
            "}\n";
    case 3:
        return
            "// rbscript network template\n"
            "module Generated.Network;\n"
            "script NetworkActor : Component {\n"
            "    on Network.Replicated(Variant value) {\n"
            "    }\n"
            "    fn on_start() {\n"
            "    }\n"
            "}\n";
    default:
        return
            "// rbscript empty template\n"
            "module Generated.Empty;\n"
            "script EmptyScript : Component {\n"
            "}\n";
    }
}

} // namespace RbScriptEditorContract

} // namespace Urho3D
