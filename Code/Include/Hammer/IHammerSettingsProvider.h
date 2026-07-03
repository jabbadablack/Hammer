#pragma once

#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/RTTIMacros.h>
#include <AzCore/RTTI/TypeInfoSimple.h>
#include <AzFramework/Input/Channels/InputChannelId.h>

#include <Hammer/HammerTypeIds.h>

namespace Hammer
{
    class IHammerSettingsProvider
    {
    public:
        AZ_RTTI(IHammerSettingsProvider, IHammerSettingsProviderTypeId);
        virtual ~IHammerSettingsProvider() = default;

        virtual bool GridSnappingEnabled() const = 0;
        virtual float GridSize() const = 0;
        virtual bool ShowGrid() const = 0;
        virtual bool AngleSnappingEnabled() const = 0;
        virtual float AngleStep() const = 0;
        virtual float ManipulatorLineBoundWidth() const = 0;
        virtual float ManipulatorCircleBoundWidth() const = 0;
        virtual bool StickySelectEnabled() const = 0;
        virtual AZ::Vector3 DefaultEditorCameraPosition() const = 0;
        virtual AZ::Vector2 DefaultEditorCameraOrientation() const = 0;
        virtual bool HelpersVisible() const = 0;
        virtual bool OnlyShowHelpersForSelectedEntities() const = 0;

        virtual AzFramework::InputChannelId CameraTranslateForwardChannelId() const = 0;
        virtual AzFramework::InputChannelId CameraTranslateBackwardChannelId() const = 0;
        virtual AzFramework::InputChannelId CameraTranslateLeftChannelId() const = 0;
        virtual AzFramework::InputChannelId CameraTranslateRightChannelId() const = 0;
        virtual AzFramework::InputChannelId CameraTranslateUpChannelId() const = 0;
        virtual AzFramework::InputChannelId CameraTranslateDownChannelId() const = 0;
        virtual AzFramework::InputChannelId CameraTranslateBoostChannelId() const = 0;
        virtual AzFramework::InputChannelId CameraFreeLookChannelId() const = 0;
        virtual AzFramework::InputChannelId CameraOrbitChannelId() const = 0;
        virtual AzFramework::InputChannelId CameraOrbitLookChannelId() const = 0;
        virtual float CameraRotateSpeed() const = 0;
        virtual bool CameraCaptureCursorForLook() const = 0;
        virtual float CameraTranslateSpeed() const = 0;
        virtual float CameraBoostMultiplier() const = 0;
        virtual float CameraDollyScrollSpeed() const = 0;
        virtual float CameraDefaultOrbitDistance() const = 0;
        virtual float CameraRotateSmoothness() const = 0;
        virtual float CameraTranslateSmoothness() const = 0;
        virtual bool CameraRotateSmoothingEnabled() const = 0;
        virtual bool CameraTranslateSmoothingEnabled() const = 0;
    };
} // namespace Hammer
