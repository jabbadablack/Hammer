#pragma once

#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

#include <Hammer/HammerTypeIds.h>

namespace Hammer
{
    class HammerViewportCameraComponent
        : public AzToolsFramework::Components::EditorComponentBase
    {
    public:
        AZ_EDITOR_COMPONENT(HammerViewportCameraComponent, HammerViewportCameraComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        void Activate() override;
        void Deactivate() override;
    };
} // namespace Hammer
