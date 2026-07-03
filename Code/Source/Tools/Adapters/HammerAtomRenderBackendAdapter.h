#pragma once

#include <Hammer/IHammerRenderBackend.h>

namespace AZ::RPI
{
    class ViewportContextRequestsInterface;
}

namespace Hammer
{
    class HammerAtomRenderBackendAdapter final : public IHammerRenderBackend
    {
    public:
        explicit HammerAtomRenderBackendAdapter(AZ::RPI::ViewportContextRequestsInterface& viewportContextManager);

        void RenameDefaultViewportContext(const AZ::Name& newName) const override;
        AZ::RPI::ViewportContextPtr FindViewportContextByName(const AZ::Name& name) const override;
        void SyncActiveCamera(AZ::RPI::ViewportContextPtr hiddenContext, AZ::RPI::ViewportContextPtr activeContext) const override;
        void SetOverlayPassEnabled(AZ::RPI::ViewportContextPtr viewportContext, bool enabled) const override;
        void SetRenderTickEnabled(AZ::RPI::ViewportContextPtr viewportContext, bool enabled) const override;
        void SyncViewportContextName(
            AZ::RPI::ViewportContextPtr viewportContext, AzFramework::ViewportId viewportId, bool active) const override;
        AzFramework::ViewportControllerPtr BuildViewportCameraController(AzFramework::ViewportId viewportId) const override;
        AtomToolsFramework::RenderViewportWidget* CreateRenderViewportWidget(QWidget* parent) const override;

    private:
        AZ::RPI::ViewportContextRequestsInterface& m_viewportContextManager;
    };
} // namespace Hammer
