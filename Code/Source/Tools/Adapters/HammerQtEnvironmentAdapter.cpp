#include "HammerQtEnvironmentAdapter.h"

#include "../HammerOptionalUtils.h"

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
            return OptionalUtils::ToOptional(it, children.end()).value_or(nullptr);
        }
    } // namespace

    class HammerQtEnvironmentAdapter::MinimumSizeGuardFilter : public QObject
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

    HammerQtEnvironmentAdapter::HammerQtEnvironmentAdapter()
    {
        AZ_Assert(qApp, "HammerQtEnvironmentAdapter constructed before a QApplication exists");
    }

    HammerQtEnvironmentAdapter::~HammerQtEnvironmentAdapter()
    {
        RemoveMinimumSizeGuard();
    }

    void HammerQtEnvironmentAdapter::InstallMinimumSizeGuard()
    {
        m_filter = new MinimumSizeGuardFilter(qApp);
        qApp->installEventFilter(m_filter);
    }

    void HammerQtEnvironmentAdapter::RemoveMinimumSizeGuard()
    {
        m_filter && (qApp->removeEventFilter(m_filter), true);
        delete m_filter;
        m_filter = nullptr;
    }

    double HammerQtEnvironmentAdapter::DoubleClickIntervalMs() const
    {
        return qApp->doubleClickInterval();
    }

    QWidget* HammerQtEnvironmentAdapter::FindMainEditorViewport(QWidget* root) const
    {
        QWidget* found = nullptr;
        root && (found = FindDescendantByClassName(*root, "EditorViewportWidget"), true);
        return found;
    }

    AtomToolsFramework::RenderViewportWidget* HammerQtEnvironmentAdapter::FindRealRenderViewport(QWidget* root) const
    {
        QWidget* found = nullptr;
        root && (found = FindDescendantByClassName(*root, "RenderViewportWidget"), true);
        auto* renderViewport = static_cast<AtomToolsFramework::RenderViewportWidget*>(found);
        renderViewport && (renderViewport->SetInputProcessingEnabled(false), true);
        return renderViewport;
    }

    QWidget* HammerQtEnvironmentAdapter::FindViewportUiOverlayWindow(QWidget* root) const
    {
        QWidget* found = nullptr;
        root && (found = root->findChild<QWidget*>(QStringLiteral("ViewportUiWindow")), true);
        return found;
    }
} // namespace Hammer
