
#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>

namespace Hammer
{
    class HammerRequests
    {
    public:
        AZ_RTTI(HammerRequests, "{D56F8FEC-3F62-46F4-B698-9A72260F1334}");
        virtual ~HammerRequests() = default;
        // Put your public methods here
    };
    
    class HammerBusTraits
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////
    };

    using HammerRequestBus = AZ::EBus<HammerRequests, HammerBusTraits>;
    using HammerInterface = AZ::Interface<HammerRequests>;

} // namespace Hammer
