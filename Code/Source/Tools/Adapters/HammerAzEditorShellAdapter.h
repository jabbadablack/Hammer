#pragma once

#include <Hammer/IHammerEditorShell.h>

#if !defined(Q_MOC_RUN)
#include <QPointer>
#endif

class QDockWidget;

namespace Hammer
{
    class HammerAzEditorShellAdapter final : public IHammerEditorShell
    {
    public:
        void PrepareEditorChrome() override;
        void RestoreEditorChrome() override;

        void RegisterViewportPane(const char* paneName, const AZStd::function<QWidget*(QWidget*)>& factory) override;
        AZStd::optional<QWidget*> EmbedViewportPaneAsCentralWidget(
            const char* paneName, const AZStd::function<QWidget*()>& expectedContentAccessor) override;
        void RestoreViewportPaneToDockWidget(QWidget* content) override;
        void ClosePane(const char* paneName) override;

        void RegisterHotkeyAction(
            const char* actionId, const char* name, const char* description, const char* category, const char* hotkey,
            AZStd::function<void()> callback) override;

        QStatusBar* GetMainWindowStatusBar() const override;

    private:
        QPointer<QDockWidget> m_paneDockWidget;
    };
} // namespace Hammer
