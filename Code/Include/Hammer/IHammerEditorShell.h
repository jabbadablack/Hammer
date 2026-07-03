#pragma once

#include <AzCore/RTTI/RTTIMacros.h>
#include <AzCore/RTTI/TypeInfoSimple.h>
#include <AzCore/std/function/function_template.h>
#include <AzCore/std/optional.h>

#include <Hammer/HammerTypeIds.h>

class QStatusBar;
class QWidget;

namespace Hammer
{
    class IHammerEditorShell
    {
    public:
        AZ_RTTI(IHammerEditorShell, IHammerEditorShellTypeId);
        virtual ~IHammerEditorShell() = default;

        virtual void PrepareEditorChrome() = 0;
        virtual void RestoreEditorChrome() = 0;

        virtual void RegisterViewportPane(const char* paneName, const AZStd::function<QWidget*(QWidget*)>& factory) = 0;
        virtual AZStd::optional<QWidget*> EmbedViewportPaneAsCentralWidget(
            const char* paneName, const AZStd::function<QWidget*()>& expectedContentAccessor) = 0;
        virtual void RestoreViewportPaneToDockWidget(QWidget* content) = 0;
        virtual void ClosePane(const char* paneName) = 0;

        virtual void RegisterHotkeyAction(
            const char* actionId, const char* name, const char* description, const char* category, const char* hotkey,
            AZStd::function<void()> callback) = 0;

        virtual QStatusBar* GetMainWindowStatusBar() const = 0;
    };
} // namespace Hammer
