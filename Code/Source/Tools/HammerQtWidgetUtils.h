#pragma once

#include <AzCore/std/string/string_view.h>

class QWidget;

namespace Hammer
{
    QWidget* FindDescendantByClassName(QWidget* root, AZStd::string_view className);
}
