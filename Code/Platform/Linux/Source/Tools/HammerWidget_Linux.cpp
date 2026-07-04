#if !defined(Q_MOC_RUN)
#include <QRect>
#include <QWidget>
#endif

namespace Hammer::Platform
{
    // Same call as Windows, carried over as a best-effort default - unverified on Linux.
    // X11/Wayland child-window semantics differ enough from Win32 HWNDs that this forced
    // resize may turn out to be unnecessary or insufficient once actually tested.
    void SyncAdoptedViewportGeometry(QWidget& adoptedRealViewport, const QRect& targetGeometry)
    {
        adoptedRealViewport.setGeometry(targetGeometry);
    }
} // namespace Hammer::Platform
