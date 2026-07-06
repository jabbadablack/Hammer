
#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzFramework/Viewport/ViewportId.h>

#include <Hammer/HammerTypeIds.h>

namespace Hammer
{
    class HammerViewportBusTraits
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
    };

    class HammerEditorActiveViewportRequests
    {
    public:
        AZ_RTTI(HammerEditorActiveViewportRequests, HammerEditorViewportBusTypeId);
        virtual ~HammerEditorActiveViewportRequests() = default;

        virtual void SetActiveViewportId(AzFramework::ViewportId viewportId) = 0;
        virtual AzFramework::ViewportId GetActiveViewportId() const = 0;
    };

    using HammerEditorActiveViewportRequestBus = AZ::EBus<HammerEditorActiveViewportRequests, HammerViewportBusTraits>;

    class HammerViewportRequests
    {
    public:
        AZ_RTTI(HammerViewportRequests, HammerViewportRequestBusTypeId);
        virtual ~HammerViewportRequests() = default;

        virtual void SetViewportCount(int count) = 0;
        virtual void ToggleMaximizeActiveViewport() = 0;
        virtual void SetActiveViewportViewModes(bool normal, bool wireframe, bool overdraw) = 0;
        virtual void SetCameraMirroringEnabled(bool enabled) = 0;
    };

    using HammerViewportRequestBus = AZ::EBus<HammerViewportRequests, HammerViewportBusTraits>;
} // namespace Hammer
