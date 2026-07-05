#include "HammerQtEnvironment.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/string/string_view.h>

#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>

#include <QWidget>

namespace Hammer
{
    namespace
    {
        QWidget* FindDescendantByClassName(QWidget& root, AZStd::string_view className)
        {
            const QList<QWidget*> children = root.findChildren<QWidget*>();
            const auto it = AZStd::find_if(
                children.begin(), children.end(),
                [className](QWidget* child)
                {
                    return AZStd::string_view(child->metaObject()->className()) == className;
                });
            return it != children.end() ? *it : nullptr;
        }

        template<typename T>
        T* FindDescendantByDynamicCast(QWidget& root)
        {
            const QList<QWidget*> children = root.findChildren<QWidget*>();
            const auto it = AZStd::find_if(
                children.begin(), children.end(),
                [](QWidget* child)
                {
                    return dynamic_cast<T*>(child) != nullptr;
                });
            return it != children.end() ? dynamic_cast<T*>(*it) : nullptr;
        }
    } // namespace

    QWidget* FindMainEditorViewport(QWidget* root)
    {
        AZ_Assert(root, "FindMainEditorViewport called with a null root widget");
        QWidget* found = nullptr;
        root && (found = FindDescendantByClassName(*root, "EditorViewportWidget"), true);
        return found;
    }

    AtomToolsFramework::RenderViewportWidget* FindRealRenderViewport(QWidget* root)
    {
        AZ_Assert(root, "FindRealRenderViewport called with a null root widget");
        AtomToolsFramework::RenderViewportWidget* renderViewport = nullptr;
        root && (renderViewport = FindDescendantByDynamicCast<AtomToolsFramework::RenderViewportWidget>(*root), true);
        renderViewport && (renderViewport->SetInputProcessingEnabled(false), true);
        return renderViewport;
    }

    QWidget* FindViewportUiOverlayWindow(QWidget* root)
    {
        AZ_Assert(root, "FindViewportUiOverlayWindow called with a null root widget");
        QWidget* found = nullptr;
        root && (found = root->findChild<QWidget*>(QStringLiteral("ViewportUiWindow")), true);
        return found;
    }
} // namespace Hammer
