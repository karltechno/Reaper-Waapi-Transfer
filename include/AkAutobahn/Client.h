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

#include "autobahn.h"
#include "AkJson.h"

#include "FutureUtils.h"

#include <map>
#include <cstdint>
#include <functional>
#include <mutex>

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		class session;

		class Client
		{
		public:

			typedef std::function<void(uint64_t in_subscriptionId, const JsonProvider& in_jsonProvider)> WampEventCallback;

		public:

			Client(int in_timeout = 0);
			virtual ~Client();
			
			bool Connect(const char* in_uri, unsigned int in_port);

			bool Subscribe(const char* in_uri, const char* in_options, WampEventCallback in_callback, uint64_t& out_subscriptionId, std::string& out_result);
			bool Subscribe(const char* in_uri, const AkJson& in_options, WampEventCallback in_callback, uint64_t& out_subscriptionId, AkJson& out_result);
			
			bool Unsubscribe(const uint64_t& in_subscriptionId, std::string& out_result);
			bool Unsubscribe(const uint64_t& in_subscriptionId, AkJson& out_result);

			bool Call(const char* in_uri, const char* in_args, const char* in_options, std::string& out_result, int const in_timeOutOverride = -1);
			bool Call(const char* in_uri, const AkJson& in_args, const AkJson& in_options, AkJson& out_result, int const in_timeOutOverride = -1);

		protected:
			
			void Log(const char* log);

		private:
			
			bool SubscribeImpl(const char* in_uri, const AkJson& in_options, handler_t in_callback, uint64_t& out_subscriptionId);
			bool UnsubscribeImpl(const uint64_t& in_subscriptionId);

			template<typename T> bool GetFuture(std::future<T>& in_value, T& out_result, int const in_timeOut)
			{
				if (in_timeOut > 0)
				{
					return GetFutureWithTimeout(in_timeOut, in_value, out_result);
				}
				else
				{
					return GetFutureBlocking(in_value, out_result);
				}
			}
			
			void ErrorToAkJson(const std::exception& in_exception, AkJson& out_result);

		private:
			
			session* m_ws;
			int m_timeout;
		};
	}
}
