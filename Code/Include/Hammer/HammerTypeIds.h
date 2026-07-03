
#pragma once

namespace Hammer
{
    // System Component TypeIds
    inline constexpr const char* HammerSystemComponentTypeId = "{7AD1C518-DF9D-4712-80AB-C7BBDB5A6366}";
    inline constexpr const char* HammerEditorSystemComponentTypeId = "{FCED3F35-EF1F-47AA-BC60-80A89661D94B}";

    // Module derived classes TypeIds
    inline constexpr const char* HammerModuleInterfaceTypeId = "{6772D9A3-F6B2-44FD-B1F4-773DA576C5C8}";
    inline constexpr const char* HammerModuleTypeId = "{45933550-7A7E-4711-901D-E04531D5566D}";
    // The Editor Module by default is mutually exclusive with the Client Module
    // so they use the Same TypeId
    inline constexpr const char* HammerEditorModuleTypeId = HammerModuleTypeId;

    // Interface TypeIds
    inline constexpr const char* HammerRequestsTypeId = "{355DDE5E-34C4-4C71-B7D4-DAE8CE63B41A}";
    inline constexpr const char* IHammerEditorShellTypeId = "{C3B645D1-9A07-4E45-AD72-E0B240BAF469}";
    inline constexpr const char* IHammerRenderBackendTypeId = "{3BE1BE23-420C-4C12-A9BB-31D8AF0C2D73}";
    inline constexpr const char* IHammerQtEnvironmentTypeId = "{B321B218-A0A0-44F3-B6F5-397328653A45}";
    inline constexpr const char* IHammerSettingsProviderTypeId = "{E79ADFB9-AFCE-400B-B047-FC538F734993}";
} // namespace Hammer
