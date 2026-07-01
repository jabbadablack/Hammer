
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/string/string_view.h>

#include <Atom/RPI.Public/FeatureProcessorFactory.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <QLayout>
#include <QScreen>
#include <QSettings>
#include <QWidget>

#include "HammerEditorSystemComponent.h"
#include "HammerWidget.h"
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

        // Clears a stale "Layout" value an earlier version of this Gem left in the Windows
        // registry, which otherwise leaves CLayoutWnd in a broken state at startup.
        QSettings settings;
        settings.remove("ViewportLayout");
    }

    void HammerEditorSystemComponent::Deactivate()
    {
        if (m_hammerWidget)
        {
            delete m_hammerWidget;
        }

        m_loadTemplatesHandler.Disconnect();
        AZ::RPI::FeatureProcessorFactory::Get()->UnregisterFeatureProcessor<HammerWireframeFeatureProcessor>();

        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        HammerSystemComponent::Deactivate();
    }

    void HammerEditorSystemComponent::NotifyRegisterViews()
    {
        // Hammer builds its own top-level viewport window instead of registering a dockable pane.
    }

    void HammerEditorSystemComponent::NotifyEditorInitialized()
    {
        SplitIntoViewportWindows();
    }

    void HammerEditorSystemComponent::SplitIntoViewportWindows()
    {
        QWidget* mainWindowWidget = nullptr;
        AzToolsFramework::EditorRequestBus::BroadcastResult(mainWindowWidget, &AzToolsFramework::EditorRequests::GetMainWindow);
        AZ_Warning("HammerEditorSystemComponent", mainWindowWidget, "Could not get the Editor main window");
        if (!mainWindowWidget)
        {
            return;
        }

        // EditorViewportWidget is a private Editor class, so it's found generically by its Qt
        // class name rather than by including its header.
        for (QWidget* child : mainWindowWidget->findChildren<QWidget*>())
        {
            if (AZStd::string_view(child->metaObject()->className()) == "EditorViewportWidget")
            {
                m_mainViewportWidget = child;
                break;
            }
        }
        AZ_Warning("HammerEditorSystemComponent", m_mainViewportWidget, "Could not find the main EditorViewportWidget");
        if (!m_mainViewportWidget)
        {
            return;
        }

        QScreen* screen = mainWindowWidget->screen();
        AZ_Warning("HammerEditorSystemComponent", screen, "Could not get the screen the Editor main window is on");
        if (!screen)
        {
            return;
        }

        const QRect available = screen->availableGeometry();
        const QRect leftHalf(available.left(), available.top(), available.width() / 2, available.height());
        const QRect rightHalf(
            available.left() + available.width() / 2, available.top(), available.width() - available.width() / 2, available.height());

        if (QWidget* oldParent = m_mainViewportWidget->parentWidget())
        {
            if (QLayout* oldLayout = oldParent->layout())
            {
                oldLayout->removeWidget(m_mainViewportWidget);
            }
        }
        m_mainViewportWidget->setParent(nullptr);
        m_mainViewportWidget->setWindowFlags(Qt::Window);
        m_mainViewportWidget->show();
        m_mainViewportWidget->setGeometry(leftHalf);

        m_hammerWidget = new HammerWidget();
        m_hammerWidget->setWindowTitle("Hammer");
        m_hammerWidget->show();
        m_hammerWidget->setGeometry(rightHalf);
    }

} // namespace Hammer
