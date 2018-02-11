/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

  Version: <VERSION>  Build: <BUILDNUMBER>
  Copyright (c) <COPYRIGHTYEAR> Audiokinetic Inc.
*******************************************************************************/
#ifndef CONTINUATION_H
#define CONTINUATION_H

#include <future>


namespace detail {

    template<typename F, typename W, typename R>
    struct helper
    {
        F f;
        W w;

        helper(F f, W w)
            : f(std::move(f))
            , w(std::move(w))
        {
        }

        helper(const helper& other)
            : f(std::move(const_cast<helper&>(other).f))
            , w(other.w)
        {
        }

        helper(helper&& other)
            : f(std::move(other.f))
            , w(std::move(other.w))
        {
        }

        helper& operator=(helper other)
        {
            f = std::move(other.f);
            w = std::move(other.w);
            return *this;
        }

        R operator()()
        {
            f.wait();
            return w(std::move(f));
        }
    };

}

namespace util
{
    template<typename F, typename W>
    auto then(F f, W w) -> std::future<decltype(w(std::move(f)))>
    {
        return std::async(std::launch::async, detail::helper<F, W, decltype(w(std::move(f)))>(std::move(f), std::move(w)));
    }
}

#endif