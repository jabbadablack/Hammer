
#pragma once

#include <AzCore/EBus/EBus.h>

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

    class HammerViewportRequests
    {
    public:
        AZ_RTTI(HammerViewportRequests, HammerViewportRequestBusTypeId);
        virtual ~HammerViewportRequests() = default;

        virtual void SetActiveViewportViewModes(bool normal, bool wireframe, bool overdraw) = 0;
        virtual void SetCameraMirroringEnabled(bool enabled) = 0;
    };

    using HammerViewportRequestBus = AZ::EBus<HammerViewportRequests, HammerViewportBusTraits>;
} // namespace Hammer
