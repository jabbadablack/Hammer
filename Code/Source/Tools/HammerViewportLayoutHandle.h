#pragma once

#if !defined(Q_MOC_RUN)
#include <QPointer>
#endif

namespace Hammer
{
    class HammerViewportLayoutWidget;

    class IHammerViewportLayoutHandle
    {
    public:
        virtual ~IHammerViewportLayoutHandle() = default;

        virtual void SetViewportCount(int count) = 0;
        virtual void ToggleMaximizeActiveViewport() = 0;
    };

    class NullViewportLayoutHandle final : public IHammerViewportLayoutHandle
    {
    public:
        void SetViewportCount(int count) override;
        void ToggleMaximizeActiveViewport() override;
    };

    class LiveViewportLayoutHandle final : public IHammerViewportLayoutHandle
    {
    public:
        explicit LiveViewportLayoutHandle(HammerViewportLayoutWidget* widget);

        void SetViewportCount(int count) override;
        void ToggleMaximizeActiveViewport() override;

    private:
        QPointer<HammerViewportLayoutWidget> m_widget;
    };
} // namespace Hammer
