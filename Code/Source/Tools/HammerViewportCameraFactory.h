#pragma once

#include <AzFramework/Viewport/ViewportControllerInterface.h>
#include <AzFramework/Viewport/ViewportId.h>

namespace Hammer
{
    // Builds a free-look + Alt/LMB orbit camera controller, matching the default viewport's feel.
    AzFramework::ViewportControllerPtr CreateViewportCameraController(AzFramework::ViewportId viewportId);
}
