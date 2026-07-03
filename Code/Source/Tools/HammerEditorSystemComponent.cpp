#include "HammerEditorSystemComponent.h"
#include "HammerViewportLayoutWidget.h"

#include "HammerNames.h"

#include "Adapters/HammerAtomRenderBackendAdapter.h"
#include "Adapters/HammerAzEditorShellAdapter.h"
#include "Adapters/HammerAzSettingsRegistryAdapter.h"
#include "Adapters/HammerNullEditorShell.h"
#include "Adapters/HammerNullQtEnvironment.h"
#include "Adapters/HammerNullRenderBackend.h"
#include "Adapters/HammerNullSettingsProvider.h"
#include "Adapters/HammerQtEnvironmentAdapter.h"

#include <AzCore/Interface/Interface.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/array.h>

#include <Atom/RPI.Public/ViewportContextBus.h>

#include <QIcon>
#include <QStatusBar>
#include <QToolButton>
#include <QWidget>

#include <Hammer/HammerTypeIds.h>

namespace Hammer
{
    class HammerAdapterRegistry
    {
    public:
        void RegisterNullAdapters()
        {
            m_registeredEditorShell = &m_nullEditorShell;
            m_registeredRenderBackend = &m_nullRenderBackend;
            m_registeredQtEnvironment = &m_nullQtEnvironment;
            m_registeredSettingsProvider = &m_nullSettingsProvider;

            AZ::Interface<IHammerEditorShell>::Register(m_registeredEditorShell);
            AZ::Interface<IHammerRenderBackend>::Register(m_registeredRenderBackend);
            AZ::Interface<IHammerQtEnvironment>::Register(m_registeredQtEnvironment);
            AZ::Interface<IHammerSettingsProvider>::Register(m_registeredSettingsProvider);
        }

        void UpgradeSettingsProvider()
        {
            m_realSettingsProvider = AZStd::make_unique<HammerAzSettingsRegistryAdapter>();
            AZ::Interface<IHammerSettingsProvider>::Unregister(m_registeredSettingsProvider);
            m_registeredSettingsProvider = m_realSettingsProvider.get();
            AZ::Interface<IHammerSettingsProvider>::Register(m_registeredSettingsProvider);
        }

        void UpgradeQtEnvironment()
        {
            m_realQtEnvironment = AZStd::make_unique<HammerQtEnvironmentAdapter>();
            AZ::Interface<IHammerQtEnvironment>::Unregister(m_registeredQtEnvironment);
            m_registeredQtEnvironment = m_realQtEnvironment.get();
            AZ::Interface<IHammerQtEnvironment>::Register(m_registeredQtEnvironment);
            m_realQtEnvironment->InstallMinimumSizeGuard();
        }

        void UpgradeRenderBackend()
        {
            auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
            AZ_Error("HammerEditorSystemComponent", viewportContextManager, "Could not find AZ::RPI::ViewportContextRequestsInterface");

            viewportContextManager &&
                (m_realRenderBackend = AZStd::make_unique<HammerAtomRenderBackendAdapter>(*viewportContextManager),
                 AZ::Interface<IHammerRenderBackend>::Unregister(m_registeredRenderBackend),
                 (m_registeredRenderBackend = m_realRenderBackend.get()),
                 AZ::Interface<IHammerRenderBackend>::Register(m_registeredRenderBackend), true);
        }

        void UpgradeEditorShell()
        {
            m_realEditorShell = AZStd::make_unique<HammerAzEditorShellAdapter>();
            AZ::Interface<IHammerEditorShell>::Unregister(m_registeredEditorShell);
            m_registeredEditorShell = m_realEditorShell.get();
            AZ::Interface<IHammerEditorShell>::Register(m_registeredEditorShell);
        }

        void Shutdown()
        {
            m_realQtEnvironment && (m_realQtEnvironment->RemoveMinimumSizeGuard(), true);

            AZ::Interface<IHammerEditorShell>::Unregister(m_registeredEditorShell);
            AZ::Interface<IHammerRenderBackend>::Unregister(m_registeredRenderBackend);
            AZ::Interface<IHammerQtEnvironment>::Unregister(m_registeredQtEnvironment);
            AZ::Interface<IHammerSettingsProvider>::Unregister(m_registeredSettingsProvider);
        }

    private:
        HammerNullEditorShell m_nullEditorShell;
        HammerNullRenderBackend m_nullRenderBackend;
        HammerNullQtEnvironment m_nullQtEnvironment;
        HammerNullSettingsProvider m_nullSettingsProvider;

        AZStd::unique_ptr<HammerAzEditorShellAdapter> m_realEditorShell;
        AZStd::unique_ptr<HammerAtomRenderBackendAdapter> m_realRenderBackend;
        AZStd::unique_ptr<HammerQtEnvironmentAdapter> m_realQtEnvironment;
        AZStd::unique_ptr<HammerAzSettingsRegistryAdapter> m_realSettingsProvider;

        IHammerEditorShell* m_registeredEditorShell = nullptr;
        IHammerRenderBackend* m_registeredRenderBackend = nullptr;
        IHammerQtEnvironment* m_registeredQtEnvironment = nullptr;
        IHammerSettingsProvider* m_registeredSettingsProvider = nullptr;
    };

    namespace
    {
        struct ViewportCountButtonSpec
        {
            int m_count;
            const char* m_iconPath;
            const char* m_tooltip;
        };

        constexpr AZStd::array<ViewportCountButtonSpec, 3> ButtonSpecs = { {
            { 1, ":/Hammer/single-view.svg", "1 Viewport" },
            { 2, ":/Hammer/duo-view.svg", "2 Viewports" },
            { 4, ":/Hammer/quad-view.svg", "4 Viewports" },
        } };
    } // namespace

    AZ_COMPONENT_IMPL(HammerEditorSystemComponent, "HammerEditorSystemComponent",
        HammerEditorSystemComponentTypeId, BaseSystemComponent);

    void HammerEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        serializeContext &&
            (serializeContext->Class<HammerEditorSystemComponent, HammerSystemComponent>()->Version(0), true);
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
        AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler::BusConnect();

        m_adapters = AZStd::make_unique<HammerAdapterRegistry>();
        m_adapters->RegisterNullAdapters();
        m_adapters->UpgradeSettingsProvider();
        m_adapters->UpgradeEditorShell();

        AZ::Interface<IHammerEditorShell>::Get()->PrepareEditorChrome();
    }

    void HammerEditorSystemComponent::Deactivate()
    {
        AZ::Interface<IHammerEditorShell>::Get()->RestoreEditorChrome();

        DestroyViewportCountButtons();

        m_viewportLayoutWidget &&
            (AZ::Interface<IHammerEditorShell>::Get()->RestoreViewportPaneToDockWidget(m_viewportLayoutWidget), true);

        AZ::Interface<IHammerEditorShell>::Get()->ClosePane(Names::ViewportPaneName);
        m_viewportLayoutWidget = nullptr;
        m_viewportLayoutHandle = AZStd::make_unique<NullViewportLayoutHandle>();

        m_adapters->Shutdown();
        m_adapters.reset();

        AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler::BusDisconnect();
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        HammerSystemComponent::Deactivate();
    }

    void HammerEditorSystemComponent::NotifyRegisterViews()
    {
        RegisterViewportPane();
    }

    void HammerEditorSystemComponent::NotifyEditorInitialized()
    {
        m_adapters->UpgradeQtEnvironment();
        m_adapters->UpgradeRenderBackend();

        EmbedViewportInCenter();
        CreateViewportCountButtons();
    }

    void HammerEditorSystemComponent::OnActionRegistrationHook()
    {
        auto* shell = AZ::Interface<IHammerEditorShell>::Get();

        struct HotkeyDef
        {
            const char* m_id;
            const char* m_name;
            const char* m_hotkey;
            int m_count;
        };
        constexpr HotkeyDef layoutHotkeys[] = {
            { "o3de.action.hammer.viewportLayoutSingle", "Single Viewport", "Ctrl+1", 1 },
            { "o3de.action.hammer.viewportLayoutDual", "Dual Viewport", "Ctrl+2", 2 },
            { "o3de.action.hammer.viewportLayoutQuad", "Quad Viewport", "Ctrl+3", 4 },
        };

        for (const HotkeyDef& def : layoutHotkeys)
        {
            const AZStd::string description = AZStd::string::format("Switch Hammer to the %s layout", def.m_name);
            shell->RegisterHotkeyAction(
                def.m_id, def.m_name, description.c_str(), "Viewport", def.m_hotkey,
                [this, count = def.m_count]
                {
                    m_viewportLayoutHandle->SetViewportCount(count);
                });
        }

        shell->RegisterHotkeyAction(
            "o3de.action.hammer.viewportToggleMaximize", "Toggle Maximize Viewport",
            "Maximize or restore the currently active Hammer viewport", "Viewport", "Ctrl+4",
            [this]
            {
                m_viewportLayoutHandle->ToggleMaximizeActiveViewport();
            });
    }

    void HammerEditorSystemComponent::RegisterViewportPane()
    {
        AZ::Interface<IHammerEditorShell>::Get()->RegisterViewportPane(
            Names::ViewportPaneName,
            [this](QWidget* parent) -> QWidget*
            {
                AZ_Assert(
                    !m_viewportLayoutWidget, "The '%s' view pane's widget was requested a second time", Names::ViewportPaneName);
                m_viewportLayoutWidget = new HammerViewportLayoutWidget(parent);
                AZ_Assert(m_viewportLayoutWidget, "Failed to allocate HammerViewportLayoutWidget");
                m_viewportLayoutHandle = AZStd::make_unique<LiveViewportLayoutHandle>(m_viewportLayoutWidget);
                return m_viewportLayoutWidget;
            });
    }

    void HammerEditorSystemComponent::EmbedViewportInCenter()
    {
        AZ::Interface<IHammerRenderBackend>::Get()->RenameDefaultViewportContext(Names::HiddenPerspectiveViewportContextAzName());

        const AZStd::optional<QWidget*> oldViewport =
            AZ::Interface<IHammerEditorShell>::Get()->EmbedViewportPaneAsCentralWidget(Names::ViewportPaneName, m_viewportLayoutWidget);

        QWidget* oldViewportPtr = nullptr;
        oldViewport.has_value() && (oldViewportPtr = *oldViewport, true);

        (oldViewportPtr && m_viewportLayoutWidget) && (m_viewportLayoutWidget->SetHiddenRealViewport(*oldViewportPtr), true);
    }

    void HammerEditorSystemComponent::CreateViewportCountButtons()
    {
        QStatusBar* statusBar = AZ::Interface<IHammerEditorShell>::Get()->GetMainWindowStatusBar();
        AZ_Error("HammerEditorSystemComponent", statusBar, "Could not find the Editor's status bar to host viewport-count buttons");

        (statusBar && m_viewportLayoutWidget) &&
            ([this, statusBar]
             {
                 for (const ViewportCountButtonSpec& spec : ButtonSpecs)
                 {
                     QToolButton* button = new QToolButton(statusBar);
                     button->setIcon(QIcon(QString::fromUtf8(spec.m_iconPath)));
                     button->setIconSize(QSize(16, 16));
                     button->setCheckable(true);
                     button->setAutoRaise(true);
                     button->setChecked(spec.m_count == 1);
                     button->setToolTip(QObject::tr(spec.m_tooltip));

                     QPointer<HammerViewportLayoutWidget> layoutWidget = m_viewportLayoutWidget;
                     const int count = spec.m_count;
                     QObject::connect(
                         button, &QToolButton::clicked, button,
                         [layoutWidget, count]
                         {
                             layoutWidget && (layoutWidget->SetViewportCount(count), true);
                         });

                     statusBar->addPermanentWidget(button);
                     m_viewportCountButtons.push_back(button);
                 }

                 QObject::connect(
                     m_viewportLayoutWidget, &HammerViewportLayoutWidget::ViewportCountChanged, m_viewportLayoutWidget,
                     [this](int count)
                     {
                         for (size_t i = 0; i < ButtonSpecs.size() && i < m_viewportCountButtons.size(); ++i)
                         {
                             QPointer<QToolButton>& button = m_viewportCountButtons[i];
                             button && (button->setChecked(ButtonSpecs[i].m_count == count), true);
                         }
                     });
             }(),
             true);
    }

    void HammerEditorSystemComponent::DestroyViewportCountButtons()
    {
        m_viewportLayoutWidget &&
            (QObject::disconnect(
                 m_viewportLayoutWidget, &HammerViewportLayoutWidget::ViewportCountChanged, m_viewportLayoutWidget, nullptr),
             true);

        for (QPointer<QToolButton>& button : m_viewportCountButtons)
        {
            button && (delete button, true);
        }
        m_viewportCountButtons.clear();
    }

} // namespace Hammer
