#pragma once

#include <Hammer/IHammerRenderBackend.h>

namespace Hammer
{
    class HammerNullRenderBackend final : public IHammerRenderBackend
    {
    public:
        void RenameDefaultViewportContext(const AZ::Name&) const override {}
        AZ::RPI::ViewportContextPtr FindViewportContextByName(const AZ::Name&) const override { return nullptr; }
        void SyncActiveCamera(AZ::RPI::ViewportContextPtr, AZ::RPI::ViewportContextPtr) const override {}
        void SetOverlayPassEnabled(AZ::RPI::ViewportContextPtr, bool) const override {}
        void SetRenderTickEnabled(AZ::RPI::ViewportContextPtr, bool) const override {}
        void SyncViewportContextName(AZ::RPI::ViewportContextPtr, AzFramework::ViewportId, bool) const override {}
        AzFramework::ViewportControllerPtr BuildViewportCameraController(AzFramework::ViewportId) const override { return nullptr; }
        AtomToolsFramework::RenderViewportWidget* CreateRenderViewportWidget(QWidget*) const override { return nullptr; }
    };
} // namespace Hammer
