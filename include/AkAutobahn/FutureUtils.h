/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Version: v2017.1.0  Build: 6302
Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

#pragma once

#include <cstdint>
#include <future>

namespace AK
{
	namespace WwiseAuthoringAPI
	{
#ifdef VALIDATE_ECHO
		extern uint64_t __getCount;
		extern uint64_t __abortCount;
#endif

		template<typename T> bool GetFutureWithTimeout(int timeoutMs, std::future<T>& value, T& out_result)
		{
#ifdef VALIDATE_ECHO
			++__getCount;
#endif

			if (value.wait_for(std::chrono::milliseconds(timeoutMs)) != std::future_status::ready)
			{
#ifdef VALIDATE_ECHO
				++__abortCount;
				std::cout << "get aborted, ratio: " << (static_cast<float>(__abortCount) / __getCount) << std::endl;
				out_result = T();
#endif
				return false;
			}

			out_result = value.get();
			return true;
		}

		template<typename T> bool GetFutureBlocking(std::future<T>& value, T& out_result)
		{
			out_result = value.get();
			return true;
		}
	}
}
