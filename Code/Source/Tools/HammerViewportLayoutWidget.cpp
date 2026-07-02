#include "HammerViewportLayoutWidget.h"
#include "HammerViewportManipulatorController.h"
#include "HammerWidget.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/string/string_view.h>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>

#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace Hammer
{
    HammerViewportLayoutWidget::HammerViewportLayoutWidget(QWidget* parent)
        : QWidget(parent)
        , m_activeViewportTracker(AZStd::make_shared<ActiveViewportTracker>())
    {
        QVBoxLayout* outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(0, 0, 0, 0);
        outerLayout->setSpacing(0);

        QHBoxLayout* toolbarLayout = new QHBoxLayout();
        toolbarLayout->addWidget(new QLabel(tr("Viewports:"), this));
        for (int count : { 1, 2, 4 })
        {
            QPushButton* button = new QPushButton(QString::number(count), this);
            button->setCheckable(true);
            button->setFixedWidth(24);
            connect(button, &QPushButton::clicked, this, [this, count] { SetViewportCount(count); });
            toolbarLayout->addWidget(button);
            m_countButtons.push_back(button);
        }
        toolbarLayout->addStretch();
        outerLayout->addLayout(toolbarLayout);

        m_gridContainer = new QWidget(this);
        m_gridLayout = new QGridLayout(m_gridContainer);
        m_gridLayout->setContentsMargins(0, 0, 0, 0);
        m_gridLayout->setSpacing(0);
        outerLayout->addWidget(m_gridContainer, /*stretch*/ 1);

        SetViewportCount(4);

        m_realViewportSyncTimer = new QTimer(this);
        connect(m_realViewportSyncTimer, &QTimer::timeout, this, &HammerViewportLayoutWidget::SyncRealViewportToActive);
        m_realViewportSyncTimer->start(16);
    }

    void HammerViewportLayoutWidget::SetViewportCount(int count)
    {
        count = AZStd::clamp(count, MinViewportCount, MaxViewportCount);

        if (m_viewports.empty())
        {
            for (int i = 0; i < MaxViewportCount; ++i)
            {
                HammerWidget* viewport = new HammerWidget(m_gridContainer, m_activeViewportTracker);
                AZ_Assert(viewport, "Failed to allocate HammerWidget #%d", i);
                m_viewports.push_back(viewport);
            }

            for (HammerWidget* viewport : m_viewports)
            {
                connect(
                    viewport, &HammerWidget::ViewportFocusRequested, this,
                    [this, viewport]
                    {
                        for (HammerWidget* sibling : m_viewports)
                        {
                            if (sibling != viewport)
                            {
                                sibling->SetActive(false);
                            }
                        }
                        viewport->SetActive(true);
                        m_activeViewport = viewport;
                    });
            }

            m_viewports[0]->SetActive(true);
            m_activeViewport = m_viewports[0];
        }

        for (HammerWidget* viewport : m_viewports)
        {
            m_gridLayout->removeWidget(viewport);
            viewport->hide();
        }

        const int columns = count <= 1 ? 1 : 2;
        const int shownCount = AZStd::GetMin(count, static_cast<int>(m_viewports.size()));
        for (int i = 0; i < shownCount; ++i)
        {
            m_gridLayout->addWidget(m_viewports[i], i / columns, i % columns);
            m_viewports[i]->show();
        }

        int buttonIndex = 0;
        for (int buttonCount : { 1, 2, 4 })
        {
            m_countButtons[buttonIndex]->setChecked(buttonCount == count);
            ++buttonIndex;
        }
    }

    void HammerViewportLayoutWidget::SetHiddenRealViewport(QWidget* realViewport)
    {
        AZ_Assert(realViewport, "SetHiddenRealViewport called with a null widget");
        if (!realViewport)
        {
            return;
        }

        AZ_Assert(m_gridContainer, "m_gridContainer must be constructed before hosting the real viewport");
        realViewport->setParent(m_gridContainer);

        realViewport->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        realViewport->setFocusPolicy(Qt::NoFocus);
        realViewport->show();
        m_hiddenRealViewport = realViewport;

        if (auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get())
        {
            m_hiddenRealViewportContext = viewportContextManager->GetViewportContextByName(AZ::Name("Hammer Hidden Perspective Viewport"));
            AZ_Error(
                "HammerViewportLayoutWidget", m_hiddenRealViewportContext,
                "Could not resolve the real viewport's own ViewportContext by name - camera syncing will not work");
        }
    }

    void HammerViewportLayoutWidget::SyncRealViewportToActive()
    {
        if (!m_hiddenRealViewport || !m_activeViewport)
        {
            return;
        }
        AZ_Assert(m_gridContainer, "m_gridContainer must exist by the time viewports are being tracked");
        AZ_Assert(
            m_hiddenRealViewport->parentWidget() == m_gridContainer,
            "m_hiddenRealViewport is expected to remain parented to m_gridContainer - geometry below is computed relative to it");

        static constexpr int OffScreenCoordinate = -10000;
        m_hiddenRealViewport->setGeometry(
            OffScreenCoordinate, OffScreenCoordinate, m_activeViewport->width(), m_activeViewport->height());

        AtomToolsFramework::RenderViewportWidget* activeViewportWidget = m_activeViewport->GetViewportWidget();
        AZ_Assert(activeViewportWidget, "The active HammerWidget must have a valid RenderViewportWidget by now");
        if (m_hiddenRealViewportContext && activeViewportWidget)
        {
            AZ::RPI::ViewportContextPtr activeContext = activeViewportWidget->GetViewportContext();
            if (activeContext)
            {
                m_hiddenRealViewportContext->SetCameraTransform(activeContext->GetCameraTransform());
            }
        }

        m_viewportUiOverlayWindow.Get(
            [this]() -> QWidget*
            {
                QWidget* found = m_hiddenRealViewport->findChild<QWidget*>(QStringLiteral("ViewportUiWindow"));
                if (found)
                {
                    found->installEventFilter(this);
                }
                return found;
            });

        m_realInternalRenderViewport.Get(
            [this]() -> AtomToolsFramework::RenderViewportWidget*
            {
                const QList<QWidget*> children = m_hiddenRealViewport->findChildren<QWidget*>();
                const auto it = AZStd::find_if(
                    children.begin(), children.end(),
                    [](QWidget* child) { return AZStd::string_view(child->metaObject()->className()) == "RenderViewportWidget"; });
                if (it == children.end())
                {
                    return nullptr;
                }
                auto* renderViewport = static_cast<AtomToolsFramework::RenderViewportWidget*>(*it);
                renderViewport->SetInputProcessingEnabled(false);
                return renderViewport;
            });
    }

    bool HammerViewportLayoutWidget::eventFilter(QObject* watched, QEvent* event)
    {
        if (watched == m_viewportUiOverlayWindow.Peek() && m_activeViewport &&
            (event->type() == QEvent::Move || event->type() == QEvent::Resize))
        {
            const QPoint desiredTopLeft = m_activeViewport->mapToGlobal(QPoint(0, 0));
            if (m_viewportUiOverlayWindow.Peek()->pos() != desiredTopLeft)
            {
                m_viewportUiOverlayWindow.Peek()->move(desiredTopLeft);
            }
        }
        return QWidget::eventFilter(watched, event);
    }
} // namespace Hammer
