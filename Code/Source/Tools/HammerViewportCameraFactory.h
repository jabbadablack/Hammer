#pragma once

#include <AzFramework/Viewport/ViewportControllerInterface.h>
#include <AzFramework/Viewport/ViewportId.h>

namespace Hammer
{
    AzFramework::ViewportControllerPtr CreateViewportCameraController(AzFramework::ViewportId viewportId);
} // namespace Hammer
