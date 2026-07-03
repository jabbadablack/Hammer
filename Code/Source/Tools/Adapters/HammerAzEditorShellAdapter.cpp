#include "HammerAzEditorShellAdapter.h"
#include <Hammer/IHammerQtEnvironment.h>

#include "../HammerOptionalUtils.h"

#include <AzCore/Interface/Interface.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <AzToolsFramework/ActionManager/Action/ActionManagerInterface.h>
#include <AzToolsFramework/ActionManager/HotKey/HotKeyManagerInterface.h>
#include <AzToolsFramework/Editor/ActionManagerIdentifiers/EditorContextIdentifiers.h>
#include <AzToolsFramework/Viewport/ViewportSettings.h>

#include <QDockWidget>
#include <QMainWindow>
#include <QSettings>
#include <QStatusBar>
#include <QWidget>

namespace Hammer
{
    void HammerAzEditorShellAdapter::PrepareEditorChrome()
    {
        QSettings settings("O3DE", "O3DE");
        constexpr const char* MigratedStaleLayoutKey = "HammerGem/migratedStaleLayoutOnce";
        const bool alreadyMigrated = settings.value(MigratedStaleLayoutKey, false).toBool();
        (!alreadyMigrated) &&
            (settings.remove("ViewportLayout"), settings.remove("Editor/fancyWindowLayouts/last"),
             settings.setValue(MigratedStaleLayoutKey, true), true);

        m_originalIconsVisiblePreference = AzToolsFramework::IconsVisible();
        AzToolsFramework::SetIconsVisible(false);
    }

    void HammerAzEditorShellAdapter::RestoreEditorChrome()
    {
        AzToolsFramework::SetIconsVisible(m_originalIconsVisiblePreference);
    }

    void HammerAzEditorShellAdapter::RegisterViewportPane(const char* paneName, const AZStd::function<QWidget*(QWidget*)>& factory)
    {
        AzToolsFramework::ViewPaneOptions viewOptions;
        viewOptions.isDeletable = true;
        viewOptions.showInMenu = true;
        viewOptions.isDisabledInComponentMode = false;

        AzToolsFramework::RegisterViewPane<QWidget>(
            paneName, "Viewport", viewOptions,
            [factory](QWidget* parent) -> QWidget*
            {
                return factory(parent);
            });
    }

    AZStd::optional<QWidget*> HammerAzEditorShellAdapter::EmbedViewportPaneAsCentralWidget(const char* paneName, QWidget* expectedContent)
    {
        QWidget* mainWindowWidget = nullptr;
        AzToolsFramework::EditorRequestBus::BroadcastResult(mainWindowWidget, &AzToolsFramework::EditorRequests::GetMainWindow);
        AZ_Error("HammerAzEditorShellAdapter", mainWindowWidget, "Could not get the Editor main window");

        auto* qtEnvironment = AZ::Interface<IHammerQtEnvironment>::Get();
        AZ_Assert(qtEnvironment, "IHammerQtEnvironment must be registered before EmbedViewportPaneAsCentralWidget is called");

        QWidget* oldViewport = nullptr;
        mainWindowWidget && (oldViewport = qtEnvironment->FindMainEditorViewport(mainWindowWidget), true);
        AZ_Error("HammerAzEditorShellAdapter", oldViewport, "Could not find the main EditorViewportWidget");

        QWidget* startAncestor = nullptr;
        oldViewport && (startAncestor = oldViewport->parentWidget(), true);

        QMainWindow* viewPaneHost = nullptr;
        for (QWidget* ancestor = startAncestor; ancestor && !viewPaneHost; ancestor = ancestor->parentWidget())
        {
            viewPaneHost = qobject_cast<QMainWindow*>(ancestor);
        }
        AZ_Error("HammerAzEditorShellAdapter", viewPaneHost, "Could not find the QMainWindow hosting the main viewport");

        QWidget* oldCentralWidget = nullptr;
        viewPaneHost && (oldCentralWidget = viewPaneHost->centralWidget(), true);

        viewPaneHost && (AzToolsFramework::OpenViewPane(paneName), true);

        QWidget* content = nullptr;
        viewPaneHost && (content = AzToolsFramework::GetViewPaneWidget<QWidget>(paneName), true);
        AZ_Error("HammerAzEditorShellAdapter", content, "Could not open the '%s' view pane", paneName);
        AZ_Assert(
            !content || content == expectedContent, "Opened '%s' view pane's content is not the expected widget", paneName);

        QDockWidget* paneDockWidget = nullptr;
        content && (paneDockWidget = qobject_cast<QDockWidget*>(content->parentWidget()), true);
        AZ_Error("HammerAzEditorShellAdapter", paneDockWidget, "Could not find the dock widget hosting the '%s' view pane", paneName);
        m_paneDockWidget = paneDockWidget;

        (viewPaneHost && paneDockWidget) &&
            (viewPaneHost->removeDockWidget(paneDockWidget), paneDockWidget->hide(), paneDockWidget->setWidget(nullptr), true);

        (viewPaneHost && content) && (viewPaneHost->setCentralWidget(content), content->show(), true);

        (oldCentralWidget && oldCentralWidget != content) &&
            (oldCentralWidget->hide(), oldCentralWidget->setParent(nullptr), true);

        const bool succeeded = viewPaneHost && oldViewport && content && paneDockWidget;
        return OptionalUtils::OptionalWhen(succeeded, oldViewport);
    }

    void HammerAzEditorShellAdapter::RestoreViewportPaneToDockWidget(QWidget* content)
    {
        (m_paneDockWidget && content) && (content->hide(), m_paneDockWidget->setWidget(content), true);
    }

    void HammerAzEditorShellAdapter::ClosePane(const char* paneName)
    {
        AzToolsFramework::CloseViewPane(paneName);
        AzToolsFramework::UnregisterViewPane(paneName);
    }

    void HammerAzEditorShellAdapter::RegisterHotkeyAction(
        const char* actionId, const char* name, const char* description, const char* category, const char* hotkey,
        AZStd::function<void()> callback)
    {
        auto* actionManagerInterface = AZ::Interface<AzToolsFramework::ActionManagerInterface>::Get();
        auto* hotKeyManagerInterface = AZ::Interface<AzToolsFramework::HotKeyManagerInterface>::Get();
        AZ_Error("HammerAzEditorShellAdapter", actionManagerInterface, "Could not find AzToolsFramework::ActionManagerInterface");
        AZ_Error("HammerAzEditorShellAdapter", hotKeyManagerInterface, "Could not find AzToolsFramework::HotKeyManagerInterface");

        AzToolsFramework::ActionProperties actionProperties;
        actionProperties.m_name = name;
        actionProperties.m_description = description;
        actionProperties.m_category = category;

        (actionManagerInterface && hotKeyManagerInterface) &&
            (actionManagerInterface->RegisterAction(
                 EditorIdentifiers::MainWindowActionContextIdentifier, actionId, actionProperties, AZStd::move(callback)),
             hotKeyManagerInterface->SetActionHotKey(actionId, hotkey), true);
    }

    QStatusBar* HammerAzEditorShellAdapter::GetMainWindowStatusBar() const
    {
        QWidget* mainWindowWidget = nullptr;
        AzToolsFramework::EditorRequestBus::BroadcastResult(mainWindowWidget, &AzToolsFramework::EditorRequests::GetMainWindow);
        auto* mainWindow = qobject_cast<QMainWindow*>(mainWindowWidget);
        AZ_Error("HammerAzEditorShellAdapter", mainWindow, "Could not find the Editor's main QMainWindow to host viewport-count buttons");

        QStatusBar* statusBar = nullptr;
        mainWindow && (statusBar = mainWindow->statusBar(), true);
        AZ_Error("HammerAzEditorShellAdapter", statusBar, "Editor main window has no status bar");
        return statusBar;
    }
} // namespace Hammer
