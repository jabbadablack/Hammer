
#pragma once

#if !defined(Q_MOC_RUN)
#include <QWidget>
#endif

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/function/function_template.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <Atom/RPI.Public/Base.h>

class QEvent;
class QGridLayout;
class QPushButton;
class QTimer;

namespace AtomToolsFramework
{
    class RenderViewportWidget;
}

namespace Hammer
{
    class HammerWidget;
    class ActiveViewportTracker;

    template<typename T>
    class LazyFind
    {
    public:
        T* Get(const AZStd::function<T*()>& finder)
        {
            return m_value = m_value ? m_value : finder();
        }

        T* Peek() const
        {
            return m_value;
        }

    private:
        T* m_value = nullptr;
    };

    class HammerViewportLayoutWidget
        : public QWidget
    {
        Q_OBJECT
    public:
        static constexpr int MinViewportCount = 1;
        static constexpr int MaxViewportCount = 4;

        explicit HammerViewportLayoutWidget(QWidget* parent = nullptr);
        ~HammerViewportLayoutWidget() override = default;

        void SetHiddenRealViewport(QWidget* realViewport);

    private:
        void SetViewportCount(int count);
        void SyncRealViewportToActive();
        bool eventFilter(QObject* watched, QEvent* event) override;

        QGridLayout* m_gridLayout = nullptr;
        QWidget* m_gridContainer = nullptr;
        AZStd::vector<QPushButton*> m_countButtons;
        AZStd::vector<HammerWidget*> m_viewports;
        AZStd::shared_ptr<ActiveViewportTracker> m_activeViewportTracker;
        QWidget* m_hiddenRealViewport = nullptr;
        HammerWidget* m_activeViewport = nullptr;
        QTimer* m_realViewportSyncTimer = nullptr;
        AZ::RPI::ViewportContextPtr m_hiddenRealViewportContext;
        LazyFind<QWidget> m_viewportUiOverlayWindow;
        LazyFind<AtomToolsFramework::RenderViewportWidget> m_realInternalRenderViewport;
    };
} // namespace Hammer
