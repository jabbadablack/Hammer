#pragma once

#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzFramework/Viewport/MultiViewportController.h>
#include <AzFramework/Viewport/ViewportId.h>
#include <AzFramework/Visibility/EntityVisibilityQuery.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>

namespace Hammer
{
    class HammerViewportDisplayControllerInstance;
    class HammerEditorViewportSettings;

    class HammerViewportDisplayController final
        : public AzFramework::MultiViewportController<
              HammerViewportDisplayControllerInstance, AzFramework::ViewportControllerPriority::DispatchToAllPriorities>
    {
    };

    class HammerViewportDisplayControllerInstance final
        : public AzFramework::MultiViewportControllerInstanceInterface<HammerViewportDisplayController>
        , public AzToolsFramework::ViewportInteraction::EditorEntityViewportInteractionRequestBus::Handler
    {
    public:
        HammerViewportDisplayControllerInstance(AzFramework::ViewportId viewport, HammerViewportDisplayController* controller);
        ~HammerViewportDisplayControllerInstance() override;

        void UpdateViewport(const AzFramework::ViewportControllerUpdateEvent& event) override;

        void FindVisibleEntities(AZStd::vector<AZ::EntityId>& visibleEntities) override;

    private:
        void RefreshDisplay();

        AzFramework::EntityVisibilityQuery m_entityVisibilityQuery;
        AZStd::unique_ptr<HammerEditorViewportSettings> m_viewportSettings;
    };
}
