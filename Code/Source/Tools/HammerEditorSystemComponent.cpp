
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/string/string_view.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>

#include <QDockWidget>
#include <QMainWindow>
#include <QSettings>
#include <QWidget>
#include <QCoreApplication>
#include <QEvent>
#include <QChildEvent>

#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>
#include <AzFramework/Windowing/WindowBus.h>

#include "HammerEditorSystemComponent.h"
#include "HammerViewportLayoutWidget.h"

#include <Hammer/HammerTypeIds.h>

namespace Hammer
{
    namespace
    {
        constexpr const char* ViewportPaneName = "Hammer Viewport";
    }

    class ViewportSizeFilter : public QObject
    {
    public:
        using QObject::QObject;

    protected:
        bool eventFilter(QObject* obj, QEvent* event) override
        {
            if (obj && obj->isWidgetType())
            {
                QWidget* widget = static_cast<QWidget*>(obj);
                const char* className = widget->metaObject()->className();
                if (className)
                {
                    AZStd::string_view classView(className);
                    bool isViewport = false;

                    // 1. Check if the widget is a viewport container
                    if (classView == "EditorViewportWidget" || 
                        classView == "HammerWidget" || 
                        classView == "HammerViewportLayoutWidget" ||
                        classView == "HammerRenderViewportWidget")
                    {
                        isViewport = true;
                    }
                    // 2. Check if the widget is a RenderViewportWidget (class QWidget under a viewport container)
                    else if (classView == "QWidget")
                    {
                        if (QWidget* parent = widget->parentWidget())
                        {
                            const char* parentClassName = parent->metaObject()->className();
                            if (parentClassName)
                            {
                                AZStd::string_view parentClassView(parentClassName);
                                if (parentClassView == "EditorViewportWidget" || 
                                    parentClassView == "HammerWidget" || 
                                    parentClassView == "HammerViewportLayoutWidget")
                                {
                                    isViewport = true;
                                }
                            }
                        }
                    }

                    if (isViewport)
                    {
                        // Unconditionally enforce at least 32x32 size limits to prevent 0-sized render targets and buffers
                        if (widget->minimumWidth() < 32 || widget->minimumHeight() < 32)
                        {
                            widget->setMinimumSize(32, 32);
                        }
                        if (widget->width() < 32 || widget->height() < 32)
                        {
                            widget->resize(32, 32);
                        }
                    }
                }
            }

            return QObject::eventFilter(obj, event);
        }
    };

    AZ_COMPONENT_IMPL(HammerEditorSystemComponent, "HammerEditorSystemComponent",
        HammerEditorSystemComponentTypeId, BaseSystemComponent);

    void HammerEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
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

        // Clears a stale "Layout" value an earlier version of this Gem left in the Windows
        // registry, which otherwise leaves CLayoutWnd in a broken state at startup. Must use the
        // same explicit ("O3DE", "O3DE") scope MainWindow::m_settings uses (Code/Editor/
        // MainWindow.cpp:291) - a bare QSettings() reads QCoreApplication's organization/app name
        // instead, which is a different registry location and silently no-ops here.
        QSettings settings("O3DE", "O3DE");
        settings.remove("ViewportLayout");

        // QtViewPaneManager persists its dock layout (including which "Perspective" instances
        // were open) under Editor/fancyWindowLayouts/last (Code/Editor/QtViewPaneManager.cpp:
        // GetFancyViewPaneStateGroupName/SaveStateToLayout) and restores it during Editor startup,
        // before Hammer's own code runs. A session where multiple extra "Perspective" instances
        // were left open (as HammerViewportLayoutWidget used to leave them, hidden but not
        // destroyed) got persisted there, and restoring it on the next launch crashed inside
        // EditorViewportWidget::CheckRespondToInput. HammerViewportLayoutWidget now properly
        // deletes its extra "Perspective" instances instead of just hiding them, so this shouldn't
        // recur, but this clears out any already-corrupted state from before that fix.
        settings.remove("Editor/fancyWindowLayouts/last");
    }

    void HammerEditorSystemComponent::Deactivate()
    {
        if (m_viewportFilter)
        {
            if (qApp)
            {
                qApp->removeEventFilter(m_viewportFilter);
            }
            delete m_viewportFilter;
            m_viewportFilter = nullptr;
        }

        // EmbedViewportInCenter() pulled m_viewportLayoutWidget out of m_paneDockWidget to use as
        // the Editor's central widget directly; give it back before closing the pane through
        // QtViewPaneManager, since QtViewPane::CloseInstance() sends a QCloseEvent to
        // dockWidget->widget() - if that were still null (or pointing at nothing), the close path
        // would dispatch to a null receiver.
        if (m_paneDockWidget && m_viewportLayoutWidget)
        {
            m_viewportLayoutWidget->hide();
            m_paneDockWidget->setWidget(m_viewportLayoutWidget);
        }

        // Closing/unregistering the pane lets QtViewPaneManager drive teardown order, the same
        // way it does for every other Editor pane, instead of us manually nulling widget pointers.
        AzToolsFramework::CloseViewPane(ViewportPaneName);
        AzToolsFramework::UnregisterViewPane(ViewportPaneName);
        m_paneDockWidget = nullptr;
        m_viewportLayoutWidget = nullptr;

        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        HammerSystemComponent::Deactivate();
    }

    void HammerEditorSystemComponent::NotifyRegisterViews()
    {
        // This is the Editor-startup point where the EditorRequests bus handler that actually
        // implements pane registration (SandboxIntegrationManager) is guaranteed to be connected;
        // registering from Activate() is too early and silently no-ops.
        RegisterViewportPane();
    }

    void HammerEditorSystemComponent::NotifyEditorInitialized()
    {
        if (qApp)
        {
            m_viewportFilter = new ViewportSizeFilter(qApp);
            qApp->installEventFilter(m_viewportFilter);
        }

        EmbedViewportInCenter();
    }

    void HammerEditorSystemComponent::RegisterViewportPane()
    {
        AzToolsFramework::ViewPaneOptions viewOptions;
        viewOptions.isDeletable = true;
        viewOptions.showInMenu = true;
        viewOptions.isDisabledInComponentMode = false;

        AzToolsFramework::RegisterViewPane<QWidget>(
            ViewportPaneName,
            "Viewport",
            viewOptions,
            [this](QWidget* parent) -> QWidget*
            {
                m_viewportLayoutWidget = new HammerViewportLayoutWidget(parent);
                AZ_Assert(m_viewportLayoutWidget, "Failed to allocate HammerViewportLayoutWidget");
                return m_viewportLayoutWidget;
            });
    }

    void HammerEditorSystemComponent::EmbedViewportInCenter()
    {
        QWidget* mainWindowWidget = nullptr;
        AzToolsFramework::EditorRequestBus::BroadcastResult(mainWindowWidget, &AzToolsFramework::EditorRequests::GetMainWindow);
        AZ_Error("HammerEditorSystemComponent", mainWindowWidget, "Could not get the Editor main window");
        if (!mainWindowWidget)
        {
            return;
        }

        // EditorViewportWidget is a private Editor class, so it's found generically by its Qt
        // class name rather than by including its header. It's not a registered view pane (the
        // real Editor never registers it as one - it's created directly as part of CLayoutWnd),
        // so this walk is the only way to locate the QMainWindow that hosts it.
        QWidget* oldViewport = nullptr;
        for (QWidget* child : mainWindowWidget->findChildren<QWidget*>())
        {
            if (AZStd::string_view(child->metaObject()->className()) == "EditorViewportWidget")
            {
                oldViewport = child;
                break;
            }
        }
        AZ_Error("HammerEditorSystemComponent", oldViewport, "Could not find the main EditorViewportWidget");
        if (!oldViewport)
        {
            return;
        }

        // Walk up to the QMainWindow whose central widget hosts the viewport (Code/Editor/MainWindow.cpp: m_viewPaneHost).
        // This is the same QMainWindow that QtViewPaneManager docks every other pane into.
        QMainWindow* viewPaneHost = nullptr;
        for (QWidget* ancestor = oldViewport->parentWidget(); ancestor; ancestor = ancestor->parentWidget())
        {
            if (QMainWindow* mainWindow = qobject_cast<QMainWindow*>(ancestor))
            {
                viewPaneHost = mainWindow;
                break;
            }
        }
        AZ_Error("HammerEditorSystemComponent", viewPaneHost, "Could not find the QMainWindow hosting the main viewport");
        if (!viewPaneHost)
        {
            return;
        }

        // Captured before the central-widget swap below: this is the actual widget
        // viewPaneHost->setCentralWidget() will detach (CLayoutWnd, hosting oldViewport), not
        // oldViewport itself. Qt's setCentralWidget() does not delete the outgoing central widget -
        // it only hides it and drops it from the QMainWindowLayout, leaving it alive as an orphaned
        // child of viewPaneHost. Left in that state, it kept receiving queued Qt metacalls (visible
        // in Editor.log as a callstack through EditorViewportWidget's private slot into
        // ReflectionScreenSpaceTracePass::BuildInternal) that rebuilt its Atom render pipeline
        // against invalid/stale geometry and crashed on a null AttachmentImage. Fully unparenting it
        // (not just hiding it) removes it from viewPaneHost's widget tree entirely so it stops
        // receiving those events.
        QWidget* oldCentralWidget = viewPaneHost->centralWidget();

        m_paneDockWidget = AzToolsFramework::InstanceViewPane(ViewportPaneName);
        AZ_Error("HammerEditorSystemComponent", m_paneDockWidget, "Could not instance the '%s' view pane", ViewportPaneName);
        if (!m_paneDockWidget)
        {
            return;
        }

        QWidget* content = m_paneDockWidget->widget();
        AZ_Error("HammerEditorSystemComponent", content, "Instanced '%s' view pane has no content widget", ViewportPaneName);
        if (!content)
        {
            return;
        }
        AZ_Assert(
            content == m_viewportLayoutWidget, "Instanced '%s' view pane's content is not the HammerViewportLayoutWidget it was given",
            ViewportPaneName);

        // The QDockWidget itself can never be shown as - or inside - the central widget: it was
        // briefly tried directly (viewPaneHost->setCentralWidget(m_paneDockWidget)) and crashed as
        // soon as it received a show event, because AzQtComponents::FancyDocking installs a global
        // event filter (FancyDocking::UpdateTitleBars) that assumes every QDockWidget is either
        // normally docked or hosted inside a genuine floating window, and null-derefs otherwise.
        // So instead: pull the pane's plain content widget out and use *that* as the central
        // widget - FancyDocking only reacts to QDockWidgets, not plain QWidgets, so this sidesteps
        // it entirely. The now-empty m_paneDockWidget is detached from the normal dock-area layout
        // and hidden (not deleted) purely so QtViewPaneManager's bookkeeping stays valid for the
        // later CloseViewPane/UnregisterViewPane calls in Deactivate(), which restores content
        // back into it first.
        viewPaneHost->removeDockWidget(m_paneDockWidget);
        m_paneDockWidget->hide();
        m_paneDockWidget->setWidget(nullptr);

        viewPaneHost->setCentralWidget(content);
        content->show();

        // See the comment above where oldCentralWidget was captured: setCentralWidget() only hides
        // and unparents-from-the-layout the previous central widget, it does not delete or fully
        // detach it. Explicitly unparenting it here (rather than leaving it as a hidden child of
        // viewPaneHost) stops it from continuing to receive Qt events/queued metacalls that were
        // driving it to rebuild its render pipeline in a bad state. Not deleted outright: destroying
        // its Atom render pipeline here risks the already-diagnosed ~ViewportContext ->
        // Scene::RemoveRenderPipeline pass-tree-rebuild crash from tearing one pipeline down while
        // Hammer's own are still being brought up.
        if (oldCentralWidget && oldCentralWidget != content)
        {
            oldCentralWidget->hide();
            oldCentralWidget->setParent(nullptr);
        }
    }

} // namespace Hammer
