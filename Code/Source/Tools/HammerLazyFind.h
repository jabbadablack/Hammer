#pragma once

#include <AzCore/std/function/function_template.h>

namespace Hammer
{
    template<typename T>
    class LazyFind
    {
    public:
        T* Get(const AZStd::function<T*()>& finder)
        {
            m_value || (m_value = finder(), true);
            return m_value;
        }

        T* Peek() const
        {
            return m_value;
        }

    private:
        T* m_value = nullptr;
    };
}
