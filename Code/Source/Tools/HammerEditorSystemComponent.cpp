
#include <AzCore/Serialization/SerializeContext.h>

#include <Atom/RPI.Public/FeatureProcessorFactory.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>

#include "HammerWidget.h"
#include "HammerEditorSystemComponent.h"
#include "HammerWireframeFeatureProcessor.h"

#include <Hammer/HammerTypeIds.h>

namespace Hammer
{
    AZ_COMPONENT_IMPL(HammerEditorSystemComponent, "HammerEditorSystemComponent",
        HammerEditorSystemComponentTypeId, BaseSystemComponent);

    void HammerEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        HammerWireframeFeatureProcessor::Reflect(context);

        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<HammerEditorSystemComponent, HammerSystemComponent>()
                ->Version(0);
        }
    }

    HammerEditorSystemComponent::HammerEditorSystemComponent() = default;

    HammerEditorSystemComponent::~HammerEditorSystemComponent() = default;

    void HammerEditorSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        BaseSystemComponent::GetProvidedServices(provided);
        provided.push_back(AZ_CRC_CE("HammerEditorService"));
    }

    void HammerEditorSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        BaseSystemComponent::GetIncompatibleServices(incompatible);
        incompatible.push_back(AZ_CRC_CE("HammerEditorService"));
    }

    void HammerEditorSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        BaseSystemComponent::GetRequiredServices(required);
    }

    void HammerEditorSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        BaseSystemComponent::GetDependentServices(dependent);
    }

    void HammerEditorSystemComponent::Activate()
    {
        HammerSystemComponent::Activate();
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();

        AZ::RPI::FeatureProcessorFactory::Get()->RegisterFeatureProcessor<HammerWireframeFeatureProcessor>();

        m_loadTemplatesHandler = AZ::RPI::PassSystemInterface::OnReadyLoadTemplatesEvent::Handler(
            []()
            {
                AZ::RPI::PassSystemInterface::Get()->LoadPassTemplateMappings("Passes/HammerWireframePasses.azasset");
            });
        AZ::RPI::PassSystemInterface::Get()->ConnectEvent(m_loadTemplatesHandler);
    }

    void HammerEditorSystemComponent::Deactivate()
    {
        m_loadTemplatesHandler.Disconnect();
        AZ::RPI::FeatureProcessorFactory::Get()->UnregisterFeatureProcessor<HammerWireframeFeatureProcessor>();

        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        HammerSystemComponent::Deactivate();
    }

    void HammerEditorSystemComponent::NotifyRegisterViews()
    {
        AzToolsFramework::ViewPaneOptions options;
        options.paneRect = QRect(100, 100, 500, 400);
        options.showOnToolsToolbar = true;
        options.toolbarIcon = ":/Hammer/toolbar_icon.svg";

        // Register our custom widget as a dockable tool with the Editor under an Examples sub-menu
        AzToolsFramework::RegisterViewPane<HammerWidget>("Hammer", "Examples", options);
    }

} // namespace Hammer
