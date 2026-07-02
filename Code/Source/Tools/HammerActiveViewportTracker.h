#pragma once

#include <Hammer/HammerEditorViewportBus.h>

namespace Hammer
{
    class ActiveViewportTracker final
        : public HammerEditorActiveViewportRequestBus::Handler
    {
    public:
        ActiveViewportTracker();
        ~ActiveViewportTracker() override;

        void SetActiveViewportId(AzFramework::ViewportId viewportId) override;
        AzFramework::ViewportId GetActiveViewportId() const override;

    private:
        AzFramework::ViewportId m_id = AzFramework::InvalidViewportId;
    };
}
