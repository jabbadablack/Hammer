#pragma once

#include <Hammer/IHammerEditorShell.h>

namespace Hammer
{
    class HammerNullEditorShell final : public IHammerEditorShell
    {
    public:
        void PrepareEditorChrome() override {}
        void RestoreEditorChrome() override {}

        void RegisterViewportPane(const char*, const AZStd::function<QWidget*(QWidget*)>&) override {}
        AZStd::optional<QWidget*> EmbedViewportPaneAsCentralWidget(const char*, const AZStd::function<QWidget*()>&) override
        {
            return AZStd::nullopt;
        }
        void RestoreViewportPaneToDockWidget(QWidget*) override {}
        void ClosePane(const char*) override {}

        void RegisterHotkeyAction(const char*, const char*, const char*, const char*, const char*, AZStd::function<void()>) override {}

        QStatusBar* GetMainWindowStatusBar() const override { return nullptr; }
    };
} // namespace Hammer
