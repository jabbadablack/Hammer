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
            return m_value = m_value ? m_value : finder();
        }

        T* Peek() const
        {
            return m_value;
        }

    private:
        T* m_value = nullptr;
    };
}
