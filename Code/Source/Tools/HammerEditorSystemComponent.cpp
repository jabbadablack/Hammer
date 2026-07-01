
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/string/string_view.h>

#include <Atom/RPI.Public/FeatureProcessorFactory.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <QMainWindow>
#include <QSettings>
#include <QSplitter>
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

        // Clears a stale "Layout" value an earlier version of this Gem left in the Windows
        // registry, which otherwise leaves CLayoutWnd in a broken state at startup.
        QSettings settings;
        settings.remove("ViewportLayout");
    }

    void HammerEditorSystemComponent::Deactivate()
    {
        // Not explicitly deleted: both widgets are owned by the Qt splitter/window hierarchy and
        // are destroyed by Qt itself during Editor shutdown; deleting them again here raced with
        // that teardown and crashed on exit.
        m_hammerWidget = nullptr;
        m_perspectiveWidget = nullptr;

        AZ::RPI::FeatureProcessorFactory::Get()->UnregisterFeatureProcessor<HammerWireframeFeatureProcessor>();

        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        HammerSystemComponent::Deactivate();
    }

    void HammerEditorSystemComponent::NotifyRegisterViews()
    {
        // Hammer embeds its own viewport in the center area instead of registering a dockable pane.
    }

    void HammerEditorSystemComponent::NotifyEditorInitialized()
    {
        EmbedViewportsInCenter();
    }

    void HammerEditorSystemComponent::EmbedViewportsInCenter()
    {
        QWidget* mainWindowWidget = nullptr;
        AzToolsFramework::EditorRequestBus::BroadcastResult(mainWindowWidget, &AzToolsFramework::EditorRequests::GetMainWindow);
        AZ_Warning("HammerEditorSystemComponent", mainWindowWidget, "Could not get the Editor main window");
        if (!mainWindowWidget)
        {
            return;
        }

        // EditorViewportWidget is a private Editor class, so it's found generically by its Qt
        // class name rather than by including its header. It's only used here to locate the
        // QMainWindow that hosts it - reparenting the widget itself broke its render pipeline
        // (its swapchain wasn't recreated cleanly), so it's replaced outright instead.
        QWidget* oldViewport = nullptr;
        for (QWidget* child : mainWindowWidget->findChildren<QWidget*>())
        {
            if (AZStd::string_view(child->metaObject()->className()) == "EditorViewportWidget")
            {
                oldViewport = child;
                break;
            }
        }
        AZ_Warning("HammerEditorSystemComponent", oldViewport, "Could not find the main EditorViewportWidget");
        if (!oldViewport)
        {
            return;
        }

        // Walk up to the QMainWindow whose central widget hosts the viewport (Code/Editor/MainWindow.cpp: m_viewPaneHost).
        QMainWindow* viewPaneHost = nullptr;
        for (QWidget* ancestor = oldViewport->parentWidget(); ancestor; ancestor = ancestor->parentWidget())
        {
            if (QMainWindow* mainWindow = qobject_cast<QMainWindow*>(ancestor))
            {
                viewPaneHost = mainWindow;
                break;
            }
        }
        AZ_Warning("HammerEditorSystemComponent", viewPaneHost, "Could not find the QMainWindow hosting the main viewport");
        if (!viewPaneHost)
        {
            return;
        }

        QSplitter* splitter = new QSplitter(Qt::Horizontal, viewPaneHost);
        m_perspectiveWidget = new HammerWidget(/*wireframe*/ false);
        splitter->addWidget(m_perspectiveWidget);
        m_hammerWidget = new HammerWidget(/*wireframe*/ true);
        splitter->addWidget(m_hammerWidget);

        // Deletes the old CLayoutWnd (and the EditorViewportWidget inside it) - safe since
        // nothing was extracted from it this time.
        viewPaneHost->setCentralWidget(splitter);
    }

} // namespace Hammer
