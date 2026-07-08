
#pragma once

#include <AzCore/EBus/EBus.h>

#include <Hammer/HammerTypeIds.h>

namespace Hammer
{
    class ViewportBusTraits
        : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
    };

    class ViewportRequests
    {
    public:
        AZ_RTTI(ViewportRequests, ViewportRequestBusTypeId);
        virtual ~ViewportRequests() = default;

        virtual void SetCameraMirroringEnabled(bool enabled) = 0;
    };

    using ViewportRequestBus = AZ::EBus<ViewportRequests, ViewportBusTraits>;
} // namespace Hammer
