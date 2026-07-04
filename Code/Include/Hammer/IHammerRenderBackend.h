#pragma once

#include <Atom/RPI.Public/Base.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/RTTIMacros.h>
#include <AzCore/RTTI/TypeInfoSimple.h>
#include <AzFramework/Viewport/ViewportControllerInterface.h>
#include <AzFramework/Viewport/ViewportId.h>

#include <Hammer/HammerTypeIds.h>

class QWidget;

namespace AtomToolsFramework
{
    class RenderViewportWidget;
}

namespace Hammer
{
    class IHammerRenderBackend
    {
    public:
        AZ_RTTI(IHammerRenderBackend, IHammerRenderBackendTypeId);
        virtual ~IHammerRenderBackend() = default;

        virtual void RenameDefaultViewportContext(const AZ::Name& newName) const = 0;
        virtual void SetOverlayPassEnabled(AZ::RPI::ViewportContextPtr viewportContext, bool enabled) const = 0;
        virtual void SetRenderTickEnabled(AZ::RPI::ViewportContextPtr viewportContext, bool enabled) const = 0;
        virtual void SyncViewportContextName(
            AZ::RPI::ViewportContextPtr viewportContext, AzFramework::ViewportId viewportId, bool active) const = 0;
        virtual AzFramework::ViewportControllerPtr BuildViewportCameraController(AzFramework::ViewportId viewportId) const = 0;
        virtual AtomToolsFramework::RenderViewportWidget* CreateRenderViewportWidget(QWidget* parent) const = 0;
    };
} // namespace Hammer
