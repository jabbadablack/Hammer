#include "HammerEditorViewportSettings.h"

#include <Hammer/IHammerSettingsProvider.h>

#include <AzCore/Interface/Interface.h>
#include <Hammer/HammerEditorViewportBus.h>

namespace Hammer
{
    namespace
    {
        IHammerSettingsProvider& Settings()
        {
            auto* settings = AZ::Interface<IHammerSettingsProvider>::Get();
            AZ_Assert(settings, "IHammerSettingsProvider must be registered before HammerEditorViewportSettings is used");
            return *settings;
        }
    } // namespace

    HammerEditorViewportSettings::HammerEditorViewportSettings(AzFramework::ViewportId viewport)
        : m_viewportId(viewport)
    {
        AZ_Assert(viewport != AzFramework::InvalidViewportId, "HammerEditorViewportSettings constructed with an invalid ViewportId");
        AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler::BusConnect(viewport);
        AZ_Assert(
            AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler::BusIsConnectedId(viewport),
            "HammerEditorViewportSettings failed to connect to ViewportSettingsRequestBus for viewport %u",
            static_cast<unsigned>(viewport));
    }

    HammerEditorViewportSettings::~HammerEditorViewportSettings()
    {
        AZ_Assert(
            AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler::BusIsConnected(),
            "HammerEditorViewportSettings was not connected to ViewportSettingsRequestBus at destruction");
        AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler::BusDisconnect();
    }

    bool HammerEditorViewportSettings::GridSnappingEnabled() const
    {
        return Settings().GridSnappingEnabled();
    }

    float HammerEditorViewportSettings::GridSize() const
    {
        return Settings().GridSize();
    }

    bool HammerEditorViewportSettings::ShowGrid() const
    {
        return Settings().ShowGrid();
    }

    bool HammerEditorViewportSettings::AngleSnappingEnabled() const
    {
        return Settings().AngleSnappingEnabled();
    }

    float HammerEditorViewportSettings::AngleStep() const
    {
        return Settings().AngleStep();
    }

    float HammerEditorViewportSettings::ManipulatorLineBoundWidth() const
    {
        return Settings().ManipulatorLineBoundWidth();
    }

    float HammerEditorViewportSettings::ManipulatorCircleBoundWidth() const
    {
        return Settings().ManipulatorCircleBoundWidth();
    }

    bool HammerEditorViewportSettings::StickySelectEnabled() const
    {
        return Settings().StickySelectEnabled();
    }

    AZ::Vector3 HammerEditorViewportSettings::DefaultEditorCameraPosition() const
    {
        return Settings().DefaultEditorCameraPosition();
    }

    AZ::Vector2 HammerEditorViewportSettings::DefaultEditorCameraOrientation() const
    {
        return Settings().DefaultEditorCameraOrientation();
    }

    bool HammerEditorViewportSettings::IconsVisible() const
    {
        AzFramework::ViewportId activeViewportId = AzFramework::InvalidViewportId;
        HammerEditorActiveViewportRequestBus::BroadcastResult(activeViewportId, &HammerEditorActiveViewportRequests::GetActiveViewportId);
        return m_viewportId == activeViewportId;
    }

    bool HammerEditorViewportSettings::HelpersVisible() const
    {
        return Settings().HelpersVisible();
    }

    bool HammerEditorViewportSettings::OnlyShowHelpersForSelectedEntities() const
    {
        return Settings().OnlyShowHelpersForSelectedEntities();
    }
}
