#pragma once

#include <AzCore/std/optional.h>
#include <AzCore/std/typetraits/decay.h>

namespace Hammer::OptionalUtils
{
    template<typename Iterator>
    auto ToOptional(Iterator it, Iterator end) -> AZStd::optional<AZStd::decay_t<decltype(*it)>>
    {
        AZStd::optional<AZStd::decay_t<decltype(*it)>> result;
        (it != end) && (result = *it, true);
        return result;
    }

    template<typename T>
    AZStd::optional<AZStd::decay_t<T>> OptionalWhen(bool condition, T&& value)
    {
        AZStd::optional<AZStd::decay_t<T>> result;
        condition && (result = AZStd::forward<T>(value), true);
        return result;
    }

    template<typename T, typename F>
    auto Transform(const AZStd::optional<T>& opt, F&& f) -> AZStd::optional<AZStd::decay_t<decltype(f(*opt))>>
    {
        AZStd::optional<AZStd::decay_t<decltype(f(*opt))>> result;
        opt.has_value() && (result = f(*opt), true);
        return result;
    }

    template<typename T, typename F>
    auto AndThen(const AZStd::optional<T>& opt, F&& f) -> AZStd::decay_t<decltype(f(*opt))>
    {
        AZStd::decay_t<decltype(f(*opt))> result{};
        opt.has_value() && (result = f(*opt), true);
        return result;
    }
} // namespace Hammer::OptionalUtils
