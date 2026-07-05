
#pragma once

#include <AzCore/Component/Component.h>

namespace Hammer
{
    class HammerSystemComponent
        : public AZ::Component
    {
    public:
        AZ_COMPONENT_DECL(HammerSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

    protected:
        void Activate() override;
        void Deactivate() override;
    };
} // namespace Hammer
