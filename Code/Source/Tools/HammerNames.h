#pragma once

#include <AzCore/Name/Name.h>
#include <AzCore/std/string/string.h>
#include <AzFramework/Viewport/ViewportId.h>

namespace Hammer::Names
{
    constexpr const char* ViewportPaneName = "Hammer Viewport";
    constexpr const char* HiddenPerspectiveViewportContextName = "Hammer Hidden Perspective Viewport";

    inline AZ::Name HiddenPerspectiveViewportContextAzName()
    {
        return AZ::Name(HiddenPerspectiveViewportContextName);
    }

    inline AZ::Name PerViewportContextName(AzFramework::ViewportId viewportId)
    {
        return AZ::Name(AZStd::string::format("Hammer Viewport %u", static_cast<unsigned>(viewportId)));
    }
} // namespace Hammer::Names
