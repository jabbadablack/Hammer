#if !defined(Q_MOC_RUN)
#include <QRect>
#include <QWidget>
#endif

namespace Hammer::Platform
{
    void SyncAdoptedViewportGeometry(QWidget& adoptedRealViewport, const QRect& targetGeometry)
    {
        adoptedRealViewport.setGeometry(targetGeometry);
    }
} // namespace Hammer::Platform
