#pragma once

#include <AzCore/RTTI/RTTIMacros.h>
#include <AzCore/RTTI/TypeInfoSimple.h>

#include <Hammer/HammerTypeIds.h>

class QWidget;

namespace AtomToolsFramework
{
    class RenderViewportWidget;
}

namespace Hammer
{
    class IHammerQtEnvironment
    {
    public:
        AZ_RTTI(IHammerQtEnvironment, IHammerQtEnvironmentTypeId);
        virtual ~IHammerQtEnvironment() = default;

        virtual void InstallMinimumSizeGuard() = 0;
        virtual void RemoveMinimumSizeGuard() = 0;
        virtual double DoubleClickIntervalMs() const = 0;
        virtual QWidget* FindMainEditorViewport(QWidget* root) const = 0;
        virtual AtomToolsFramework::RenderViewportWidget* FindRealRenderViewport(QWidget* root) const = 0;
        virtual QWidget* FindViewportUiOverlayWindow(QWidget* root) const = 0;
    };
} // namespace Hammer
