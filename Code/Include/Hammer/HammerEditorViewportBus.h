
#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzFramework/Viewport/ViewportId.h>

#include <Hammer/HammerTypeIds.h>

namespace Hammer
{
    class HammerEditorActiveViewportRequests
    {
    public:
        AZ_RTTI(HammerEditorActiveViewportRequests, HammerEditorViewportBusTypeId);
        virtual ~HammerEditorActiveViewportRequests() = default;

        virtual void SetActiveViewportId(AzFramework::ViewportId viewportId) = 0;
        virtual AzFramework::ViewportId GetActiveViewportId() const = 0;
    };

    class HammerEditorActiveViewportRequestBusTraits
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
    };

    using HammerEditorActiveViewportRequestBus = AZ::EBus<HammerEditorActiveViewportRequests, HammerEditorActiveViewportRequestBusTraits>;
} // namespace Hammer
