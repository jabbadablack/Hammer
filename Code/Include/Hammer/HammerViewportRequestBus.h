
#pragma once

#include <AzCore/EBus/EBus.h>

namespace Hammer
{
    class HammerViewportRequests
    {
    public:
        AZ_RTTI(HammerViewportRequests, "{6C6EE216-D4D5-4A2F-9E64-6B1C6E1F62B4}");
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
