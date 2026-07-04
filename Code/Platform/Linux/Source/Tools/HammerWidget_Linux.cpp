#if !defined(Q_MOC_RUN)
#include <QRect>
#include <QWidget>
#endif

namespace Hammer::Platform
{
    // Same call as Windows, carried over as a best effort default
    void SyncAdoptedViewportGeometry(QWidget& adoptedRealViewport, const QRect& targetGeometry)
    {
        adoptedRealViewport.setGeometry(targetGeometry);
    }
} // namespace Hammer::Platform
