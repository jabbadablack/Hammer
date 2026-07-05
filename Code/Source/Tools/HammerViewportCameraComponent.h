#pragma once

#include <AzCore/Component/Component.h>

namespace Hammer
{
    class HammerViewportCameraComponent
        : public AZ::Component
    {
    public:
        AZ_COMPONENT_DECL(HammerViewportCameraComponent);

        static void Reflect(AZ::ReflectContext* context);

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        void Activate() override;
        void Deactivate() override;
    };
} // namespace Hammer
