
#pragma once

#include <AzCore/EBus/EBus.h>

#include <Hammer/HammerTypeIds.h>

namespace Hammer
{
    class HammerViewportRequests
    {
    public:
        AZ_RTTI(HammerViewportRequests, HammerViewportRequestBusTypeId);
        virtual ~HammerViewportRequests() = default;

        virtual void SetViewportCount(int count) = 0;
        virtual void ToggleMaximizeActiveViewport() = 0;
    };

    class HammerViewportRequestBusTraits
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
    };

    using HammerViewportRequestBus = AZ::EBus<HammerViewportRequests, HammerViewportRequestBusTraits>;
} // namespace Hammer
