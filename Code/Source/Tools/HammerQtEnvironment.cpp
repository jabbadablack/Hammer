#include "HammerQtEnvironment.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/string/string_view.h>

#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>

#include <QApplication>
#include <QEvent>
#include <QWidget>

namespace Hammer
{
    namespace
    {
        constexpr AZStd::array<AZStd::string_view, 4> DirectViewportClassNames = {
            "EditorViewportWidget", "HammerWidget", "HammerViewportLayoutWidget", "HammerRenderViewportWidget"
        };
        constexpr AZStd::array<AZStd::string_view, 3> ParentViewportClassNames = {
            "EditorViewportWidget", "HammerWidget", "HammerViewportLayoutWidget"
        };

        template<size_t N>
        bool ClassNameMatches(AZStd::string_view className, const AZStd::array<AZStd::string_view, N>& names)
        {
            return AZStd::find(names.begin(), names.end(), className) != names.end();
        }

        bool IsViewportLikeWidget(QWidget& widget)
        {
            AZStd::string_view className;
            const char* rawClassName = widget.metaObject()->className();
            rawClassName && (className = rawClassName, true);

            const bool isDirectMatch = ClassNameMatches(className, DirectViewportClassNames);

            QWidget* parent = widget.parentWidget();
            AZStd::string_view parentClassName;
            parent && (parentClassName = parent->metaObject()->className(), true);

            const bool isParentMatch = (className == "QWidget") && ClassNameMatches(parentClassName, ParentViewportClassNames);

            return isDirectMatch || isParentMatch;
        }

        void ClampMinimumViewportSize(QWidget& widget)
        {
            widget.setMinimumSize(AZStd::GetMax(widget.minimumWidth(), 32), AZStd::GetMax(widget.minimumHeight(), 32));
            widget.resize(AZStd::GetMax(widget.width(), 32), AZStd::GetMax(widget.height(), 32));
        }

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

        // dynamic_cast, not azdynamic_cast: T here is a plain Qt/AtomToolsFramework widget type
        // with no AZ_RTTI and no Q_OBJECT of its own, so neither azdynamic_cast nor
        // QObject::findChild<T*>() can identify it.
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

        class MinimumSizeGuardFilter : public QObject
        {
        public:
            using QObject::QObject;

        protected:
            bool eventFilter(QObject* obj, QEvent* event) override
            {
                QWidget* widget = nullptr;
                (obj && obj->isWidgetType()) && (widget = static_cast<QWidget*>(obj), true);
                (widget && IsViewportLikeWidget(*widget)) && (ClampMinimumViewportSize(*widget), true);
                return QObject::eventFilter(obj, event);
            }
        };

        MinimumSizeGuardFilter* g_filter = nullptr;
    } // namespace

    void InstallMinimumSizeGuard()
    {
        AZ_Assert(!g_filter, "InstallMinimumSizeGuard called while a size-guard filter is already installed");
        AZ_Assert(qApp, "InstallMinimumSizeGuard called without a QApplication");

        g_filter = new MinimumSizeGuardFilter(qApp);
        AZ_Assert(g_filter, "Failed to allocate MinimumSizeGuardFilter");
        qApp->installEventFilter(g_filter);
    }

    void RemoveMinimumSizeGuard()
    {
        g_filter && (qApp->removeEventFilter(g_filter), true);
        delete g_filter;
        g_filter = nullptr;
    }

    double DoubleClickIntervalMs()
    {
        return qApp->doubleClickInterval();
    }

    QWidget* FindMainEditorViewport(QWidget* root)
    {
        QWidget* found = nullptr;
        root && (found = FindDescendantByClassName(*root, "EditorViewportWidget"), true);
        return found;
    }

    AtomToolsFramework::RenderViewportWidget* FindRealRenderViewport(QWidget* root)
    {
        AtomToolsFramework::RenderViewportWidget* renderViewport = nullptr;
        root && (renderViewport = FindDescendantByDynamicCast<AtomToolsFramework::RenderViewportWidget>(*root), true);
        renderViewport && (renderViewport->SetInputProcessingEnabled(false), true);
        return renderViewport;
    }

    QWidget* FindViewportUiOverlayWindow(QWidget* root)
    {
        QWidget* found = nullptr;
        root && (found = root->findChild<QWidget*>(QStringLiteral("ViewportUiWindow")), true);
        return found;
    }
} // namespace Hammer
