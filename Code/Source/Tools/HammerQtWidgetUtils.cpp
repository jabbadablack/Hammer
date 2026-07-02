#include "HammerQtWidgetUtils.h"

#include <QWidget>

namespace Hammer
{
    QWidget* FindDescendantByClassName(QWidget* root, AZStd::string_view className)
    {
        AZ_Assert(root, "FindDescendantByClassName called with a null root widget");
        if (!root)
        {
            return nullptr;
        }

        for (QWidget* child : root->findChildren<QWidget*>())
        {
            if (AZStd::string_view(child->metaObject()->className()) == className)
            {
                return child;
            }
        }
        return nullptr;
    }
}
