
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/string/string_view.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>

#include <QDockWidget>
#include <QIcon>
#include <QMainWindow>
#include <QSettings>
#include <QStatusBar>
#include <QToolButton>
#include <QWidget>
#include <QCoreApplication>
#include <QEvent>
#include <QChildEvent>

#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>
#include <AzFramework/Windowing/WindowBus.h>
#include <AzToolsFramework/Viewport/ViewportSettings.h>

#include "HammerEditorSystemComponent.h"
#include "HammerQtWidgetUtils.h"
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

                    if (classView == "EditorViewportWidget" ||
                        classView == "HammerWidget" ||
                        classView == "HammerViewportLayoutWidget" ||
                        classView == "HammerRenderViewportWidget")
                    {
                        isViewport = true;
                    }
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

        QSettings settings("O3DE", "O3DE");
        constexpr const char* MigratedStaleLayoutKey = "HammerGem/migratedStaleLayoutOnce";
        if (!settings.value(MigratedStaleLayoutKey, false).toBool())
        {
            settings.remove("ViewportLayout");
            settings.remove("Editor/fancyWindowLayouts/last");
            settings.setValue(MigratedStaleLayoutKey, true);
        }

        m_originalIconsVisiblePreference = AzToolsFramework::IconsVisible();
        AzToolsFramework::SetIconsVisible(false);
    }

    void HammerEditorSystemComponent::Deactivate()
    {
        AzToolsFramework::SetIconsVisible(m_originalIconsVisiblePreference);

        DestroyViewportCountButtons();

        if (m_viewportFilter)
        {
            if (qApp)
            {
                qApp->removeEventFilter(m_viewportFilter);
            }
            delete m_viewportFilter;
            m_viewportFilter = nullptr;
        }

        if (m_paneDockWidget && m_viewportLayoutWidget)
        {
            m_viewportLayoutWidget->hide();
            m_paneDockWidget->setWidget(m_viewportLayoutWidget);
        }

        AzToolsFramework::CloseViewPane(ViewportPaneName);
        AzToolsFramework::UnregisterViewPane(ViewportPaneName);
        m_paneDockWidget = nullptr;
        m_viewportLayoutWidget = nullptr;

        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        HammerSystemComponent::Deactivate();
    }

    void HammerEditorSystemComponent::NotifyRegisterViews()
    {
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
        CreateViewportCountButtons();
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
                if (m_viewportLayoutWidget)
                {
                    AZ_Warning(
                        "HammerEditorSystemComponent", false,
                        "The '%s' view pane's widget was requested a second time; returning a placeholder instead "
                        "of constructing a duplicate HammerViewportLayoutWidget",
                        ViewportPaneName);
                    return new QWidget(parent);
                }

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

        QWidget* oldViewport = FindDescendantByClassName(mainWindowWidget, "EditorViewportWidget");
        AZ_Error("HammerEditorSystemComponent", oldViewport, "Could not find the main EditorViewportWidget");
        if (!oldViewport)
        {
            return;
        }

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

        QWidget* oldCentralWidget = viewPaneHost->centralWidget();

        AZ_Assert(oldViewport, "oldViewport must be non-null here - already checked above");
        if (auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get())
        {
            AZ::RPI::ViewportContextPtr defaultContext = viewportContextManager->GetDefaultViewportContext();
            AZ_Error(
                "HammerEditorSystemComponent", defaultContext,
                "Could not find the current default ViewportContext to free its name from the real viewport");
            if (defaultContext)
            {
                viewportContextManager->RenameViewportContext(defaultContext, AZ::Name("Hammer Hidden Perspective Viewport"));
            }
        }

        AzToolsFramework::OpenViewPane(ViewportPaneName);
        QWidget* content = AzToolsFramework::GetViewPaneWidget<QWidget>(ViewportPaneName);
        AZ_Error("HammerEditorSystemComponent", content, "Could not open the '%s' view pane", ViewportPaneName);
        if (!content)
        {
            return;
        }
        AZ_Assert(
            content == m_viewportLayoutWidget, "Opened '%s' view pane's content is not the HammerViewportLayoutWidget it was given",
            ViewportPaneName);

        m_paneDockWidget = qobject_cast<QDockWidget*>(content->parentWidget());
        AZ_Error("HammerEditorSystemComponent", m_paneDockWidget, "Could not find the dock widget hosting the '%s' view pane", ViewportPaneName);
        if (!m_paneDockWidget)
        {
            return;
        }

        viewPaneHost->removeDockWidget(m_paneDockWidget);
        m_paneDockWidget->hide();
        m_paneDockWidget->setWidget(nullptr);

        viewPaneHost->setCentralWidget(content);
        content->show();

        if (oldCentralWidget && oldCentralWidget != content)
        {
            oldCentralWidget->hide();
            oldCentralWidget->setParent(nullptr);
        }

        AZ_Assert(m_viewportLayoutWidget, "m_viewportLayoutWidget is null after being validated as 'content' above");
        m_viewportLayoutWidget->SetHiddenRealViewport(oldViewport);
    }

    void HammerEditorSystemComponent::CreateViewportCountButtons()
    {
        if (!m_viewportLayoutWidget)
        {
            return;
        }

        QWidget* mainWindowWidget = nullptr;
        AzToolsFramework::EditorRequestBus::BroadcastResult(mainWindowWidget, &AzToolsFramework::EditorRequests::GetMainWindow);
        auto* editorMainWindow = qobject_cast<QMainWindow*>(mainWindowWidget);
        AZ_Error(
            "HammerEditorSystemComponent", editorMainWindow, "Could not find the Editor's main QMainWindow to host viewport-count buttons");
        if (!editorMainWindow)
        {
            return;
        }

        QStatusBar* statusBar = editorMainWindow->statusBar();
        AZ_Error("HammerEditorSystemComponent", statusBar, "Editor main window has no status bar");
        if (!statusBar)
        {
            return;
        }

        for (int count : { 1, 2, 4 })
        {
            const QString iconPath = count == 1 ? QStringLiteral(":/Hammer/single-view.svg")
                : count == 2                    ? QStringLiteral(":/Hammer/duo-view.svg")
                                                 : QStringLiteral(":/Hammer/quad-view.svg");

            QToolButton* button = new QToolButton(statusBar);
            button->setIcon(QIcon(iconPath));
            button->setIconSize(QSize(16, 16));
            button->setCheckable(true);
            button->setAutoRaise(true);
            button->setChecked(count == 1);
            button->setToolTip(QObject::tr("%1 Viewport%2").arg(count).arg(count > 1 ? "s" : ""));

            QPointer<HammerViewportLayoutWidget> layoutWidget = m_viewportLayoutWidget;
            QObject::connect(
                button, &QToolButton::clicked, button,
                [layoutWidget, count]
                {
                    if (layoutWidget)
                    {
                        layoutWidget->SetViewportCount(count);
                    }
                });

            statusBar->addPermanentWidget(button);
            m_viewportCountButtons.push_back(button);
        }

        QObject::connect(
            m_viewportLayoutWidget, &HammerViewportLayoutWidget::ViewportCountChanged, m_viewportLayoutWidget,
            [this](int count)
            {
                int buttonIndex = 0;
                for (int buttonCount : { 1, 2, 4 })
                {
                    if (buttonIndex < static_cast<int>(m_viewportCountButtons.size()) && m_viewportCountButtons[buttonIndex])
                    {
                        m_viewportCountButtons[buttonIndex]->setChecked(buttonCount == count);
                    }
                    ++buttonIndex;
                }
            });
    }

    void HammerEditorSystemComponent::DestroyViewportCountButtons()
    {
        if (m_viewportLayoutWidget)
        {
            QObject::disconnect(m_viewportLayoutWidget, &HammerViewportLayoutWidget::ViewportCountChanged, m_viewportLayoutWidget, nullptr);
        }

        for (QPointer<QToolButton>& button : m_viewportCountButtons)
        {
            if (button)
            {
                delete button;
            }
        }
        m_viewportCountButtons.clear();
    }

} // namespace Hammer
