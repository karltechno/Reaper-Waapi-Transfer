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

#include "Client.h"

#include <iostream>
#include <string>
#include <sstream>

#include "JSONHelpers.h"
#include "Logger.h"

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		Client::Client(int in_timeout)
			: m_timeout(in_timeout)
		{
			m_ws = new session();
		}

		Client::~Client()
		{
			delete m_ws;

			// IMPORTANT: In theory, deleting the websocket will hang the thread until the receiver thread has exited,
			// thus giving the garantee that the map "m_subscribers" can then be safely disposed of when the Client
			// object is fully destructed.
		}

		void Client::Log(const char* log)
		{
			Logger::Get()->LogMessage("WwiseAuthoringAPI client", log);
		}

		bool Client::Connect(const char* in_uri, unsigned int in_port)
		{
			try
			{
				m_ws->start(in_uri, in_port);
				auto future = m_ws->join("realm1");
				uint64_t result;
				return GetFuture<uint64_t>(future, result, m_timeout);
			}
			catch (const std::exception& e)
			{
				Log(e.what());
				return false;
			}
		}
		
		bool Client::Call(const char* in_uri, const char* in_args, const char* in_options, std::string& out_result, int const in_timeoutOverride /* = -1 */)
		{
			try
			{
				AkJson jsonArgs;
				rapidjson::Document docArgs;

				if (docArgs.Parse(in_args).HasParseError() || !JSONHelpers::FromRapidJson(docArgs, jsonArgs))
				{
					throw std::runtime_error("in_args should contain a valid JSON object string (empty object is allowed).");
				}

				AkJson jsonOptions;
				rapidjson::Document docOptions;

				if (docOptions.Parse(in_options).HasParseError() || !JSONHelpers::FromRapidJson(docOptions, jsonOptions))
				{
					throw std::runtime_error("in_options should contain a valid JSON object string (empty object is allowed).");
				}
				
				AkJson result;
				auto future = m_ws->call_options(in_uri, std::vector<AkVariant>{}, jsonArgs, jsonOptions);
				bool success = GetFuture<AkJson>(future, result, in_timeoutOverride == -1 ? m_timeout : in_timeoutOverride);

				if (!success)
				{
					return false;
				}

				out_result = JSONHelpers::GetAkJsonString(result);
				return true;
			}
			catch (std::exception& e)
			{
				Log(e.what());
				AkJson errorJson;
				ErrorToAkJson(e, errorJson);
				out_result = JSONHelpers::GetAkJsonString(errorJson);
				return false;
			}
		}
		
		void Client::ErrorToAkJson(const std::exception& in_exception, AkJson& out_result)
		{
			rapidjson::Document docError;

			if (!docError.Parse(in_exception.what()).HasParseError())
			{
				JSONHelpers::FromRapidJson(docError, out_result);
			}
			else
			{
				out_result = AkJson(AkJson::Map{
					{ "message", AkVariant(in_exception.what()) }
				});
			}
		}
		
		bool Client::Call(const char* in_uri, const AkJson& in_args, const AkJson& in_options, AkJson& out_result, int const in_timeoutOverride)
		{
			try
			{
				auto future = m_ws->call_options(in_uri, std::vector<AkVariant>{}, in_args, in_options);
				return GetFuture<AkJson>(future, out_result, in_timeoutOverride == -1 ? m_timeout : in_timeoutOverride);
			}
			catch (std::exception& e)
			{
				Log(e.what());
				ErrorToAkJson(e, out_result);
				return false;
			}
		}

		bool Client::SubscribeImpl(const char* in_uri, const AkJson& in_options, handler_t in_callback, uint64_t& out_subscriptionId)
		{
			Client* pThis = this;

			auto future = m_ws->subscribe(in_uri, in_callback, in_options);

			// wait for the answer
			subscription resultObject;
			if (!GetFuture<subscription>(future, resultObject, m_timeout))
			{
				return false;
			}

			out_subscriptionId = resultObject.id;
			return true;
		}

		bool Client::Subscribe(const char* in_uri, const char* in_options, WampEventCallback in_callback, uint64_t& out_subscriptionId, std::string& out_result)
		{
			try
			{
				rapidjson::Document doc;
				
				if (doc.Parse(in_options).HasParseError())
				{
					return false;
				}
				
				AkJson jsonOptions;
				JSONHelpers::FromRapidJson(doc, jsonOptions);

				return SubscribeImpl(in_uri, jsonOptions, in_callback, out_subscriptionId);
			}
			catch (const std::exception& e)
			{
				Log(e.what());
				AkJson errorJson;
				ErrorToAkJson(e, errorJson);
				out_result = JSONHelpers::GetAkJsonString(errorJson);
			}

			return false;
		}
		
		bool Client::Subscribe(const char* in_uri, const AkJson& in_options, WampEventCallback in_callback, uint64_t& out_subscriptionId, AkJson& out_result)
		{
			try
			{
				return SubscribeImpl(in_uri, in_options, in_callback, out_subscriptionId);
			}
			catch (const std::exception& e)
			{
				Log(e.what());
				ErrorToAkJson(e, out_result);
			}

			return false;
		}
		
		bool Client::UnsubscribeImpl(const uint64_t& in_subscriptionId)
		{
			auto result = m_ws->unsubscribe(in_subscriptionId);

			AkJson output;
			return GetFuture<AkJson>(result, output, m_timeout);
		}
		
		bool Client::Unsubscribe(const uint64_t& in_subscriptionId, std::string& out_result)
		{
			try
			{
				return UnsubscribeImpl(in_subscriptionId);
			}
			catch (const std::exception& e)
			{
				Log(e.what());
				AkJson errorJson;
				ErrorToAkJson(e, errorJson);
				out_result = JSONHelpers::GetAkJsonString(errorJson);
			}

			return false;
		}

		bool Client::Unsubscribe(const uint64_t& in_subscriptionId, AkJson& out_result)
		{
			try
			{
				return UnsubscribeImpl(in_subscriptionId);
			}
			catch (const std::exception& e)
			{
				Log(e.what());
				ErrorToAkJson(e, out_result);
			}

			return false;
		}
	}
}
